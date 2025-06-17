/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "firebird/Message.h"
#include <optional>
#include "../common/Int128.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_pair.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/MetaString.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/stack.h"
#include "../common/status.h"
#include "../intl/charsets.h"
#include "../jrd/intl.h"

using namespace Firebird;


namespace
{

class ProfilerPlugin;


static constexpr UCHAR STREAM_BLOB_BPB[] = {
	isc_bpb_version1,
	isc_bpb_type, 1, isc_bpb_type_stream,
};

static const CInt128 ONE_SECOND_IN_NS{1'000'000'000};
static CInt128 ticksFrequency{0};

SINT64 ticksToNanoseconds(FB_UINT64 ticks)
{
	return CInt128((SINT64) ticks).mul(ONE_SECOND_IN_NS).div(ticksFrequency, 0).toInt64(0);
}

auto& defaultPool()
{
	return *getDefaultMemoryPool();
}

void quote(string& name)
{
	const char QUOTE = '"';

	for (unsigned p = 0; p < name.length(); ++p)
	{
		if (name[p] == QUOTE)
		{
			name.insert(p, 1, QUOTE);
			++p;
		}
	}

	name.insert(0u, 1, QUOTE);
	name += QUOTE;
}

struct Stats
{
	void hit(FB_UINT64 elapsedTicks)
	{
		if (counter == 0 || elapsedTicks < minElapsedTicks)
			minElapsedTicks = elapsedTicks;

		if (counter == 0 || elapsedTicks > maxElapsedTicks)
			maxElapsedTicks = elapsedTicks;

		totalElapsedTicks += elapsedTicks;
		++counter;
	}

	FB_UINT64 counter = 0;
	FB_UINT64 minElapsedTicks = 0;
	FB_UINT64 maxElapsedTicks = 0;
	FB_UINT64 totalElapsedTicks = 0;
};

struct Cursor
{
	MetaString name{defaultPool()};
	unsigned line;
	unsigned column;
};

struct RecordSource
{
	std::optional<ULONG> parentId;
	unsigned level;
	string accessPath{defaultPool()};
};

struct RecordSourceStats
{
	Stats openStats;
	Stats fetchStats;
};

struct Statement
{
	unsigned level = 0;
	string type{defaultPool()};
	MetaString schemaName{defaultPool()};
	MetaString packageName{defaultPool()};
	MetaString routineName{defaultPool()};
	SINT64 parentStatementId;
	string sqlText{defaultPool()};
};

using LineColumnKey = NonPooledPair<unsigned, unsigned>;
using CursorRecSourceKey = NonPooledPair<unsigned, unsigned>;

struct Request
{
	bool dirty = true;
	unsigned level = 0;
	SINT64 statementId;
	SINT64 callerStatementId = 0;
	SINT64 callerRequestId = 0;
	ISC_TIMESTAMP_TZ startTimestamp;
	std::optional<ISC_TIMESTAMP_TZ> finishTimestamp;
	std::optional<FB_UINT64> totalElapsedTicks;
	NonPooledMap<CursorRecSourceKey, RecordSourceStats> recordSourcesStats{defaultPool()};
	NonPooledMap<LineColumnKey, Stats> psqlStats{defaultPool()};
};

using StatementCursorKey = NonPooledPair<SINT64, unsigned>;
using StatementCursorRecSourceKey = NonPooledPair<NonPooledPair<SINT64, unsigned>, unsigned>;

class Session final :
	public IProfilerSessionImpl<Session, ThrowStatusExceptionWrapper>,
	public RefCounted
{
public:
	Session(ThrowStatusExceptionWrapper* status, ProfilerPlugin* aPlugin,
			const char* aDescription, ISC_TIMESTAMP_TZ aStartTimestamp);

public:
	void dispose() override
	{
		plugin = nullptr;	// avoid circular reference
		release();
	}

public:
	SINT64 getId() override
	{
		return id;
	}

	unsigned getFlags() override
	{
		return FLAG_AFTER_EVENTS;
	}

	void cancel(ThrowStatusExceptionWrapper* status) override;

	void finish(ThrowStatusExceptionWrapper* status, ISC_TIMESTAMP_TZ timestamp) override;

	void deprecatedDefineStatement(ThrowStatusExceptionWrapper* status, SINT64 statementId, SINT64 parentStatementId,
		const char* type, const char* packageName, const char* routineName, const char* sqlText) override
	{
		defineStatement2(status, statementId, parentStatementId, type, nullptr, packageName, routineName, sqlText);
	}

	void defineCursor(SINT64 statementId, unsigned cursorId, const char* name, unsigned line, unsigned column) override;

	void defineRecordSource(SINT64 statementId, unsigned cursorId, unsigned recSourceId,
		unsigned level, const char* accessPath, unsigned parentRecordSourceId) override;

	void onRequestStart(ThrowStatusExceptionWrapper* status, SINT64 statementId, SINT64 requestId,
		SINT64 callerStatementId, SINT64 callerRequestId, ISC_TIMESTAMP_TZ timestamp) override;

	void onRequestFinish(ThrowStatusExceptionWrapper* status, SINT64 statementId, SINT64 requestId,
		ISC_TIMESTAMP_TZ timestamp, IProfilerStats* stats) override;

	void beforePsqlLineColumn(SINT64 statementId, SINT64 requestId, unsigned line, unsigned column) override
	{
	}

	void afterPsqlLineColumn(SINT64 statementId, SINT64 requestId,
		unsigned line, unsigned column, IProfilerStats* stats) override;

	void beforeRecordSourceOpen(SINT64 statementId, SINT64 requestId, unsigned cursorId, unsigned recSourceId) override
	{
	}

	void afterRecordSourceOpen(SINT64 statementId, SINT64 requestId, unsigned cursorId, unsigned recSourceId,
		IProfilerStats* stats) override;

	void beforeRecordSourceGetRecord(SINT64 statementId, SINT64 requestId,
		unsigned cursorId, unsigned recSourceId) override
	{
	}

	void afterRecordSourceGetRecord(SINT64 statementId, SINT64 requestId, unsigned cursorId, unsigned recSourceId,
		IProfilerStats* stats) override;

	void defineStatement2(ThrowStatusExceptionWrapper* status, SINT64 statementId, SINT64 parentStatementId,
		const char* type, const char* schemaName, const char* packageName, const char* routineName,
		const char* sqlText) override;

private:
	Request* getRequest(SINT64 statementId, SINT64 requestId)
	{
		return requests.get(detailedRequests ? requestId : -statementId);
	}

public:
	RefPtr<ProfilerPlugin> plugin;
	NonPooledMap<SINT64, Statement> statements{defaultPool()};
	NonPooledMap<StatementCursorKey, Cursor> cursors{defaultPool()};
	NonPooledMap<StatementCursorRecSourceKey, RecordSource> recordSources{defaultPool()};
	NonPooledMap<SINT64, Request> requests{defaultPool()};
	SINT64 id;
	ISC_TIMESTAMP_TZ startTimestamp;
	std::optional<ISC_TIMESTAMP_TZ> finishTimestamp;
	string description{defaultPool()};
	bool detailedRequests = false;
	bool dirty = true;
};

class ProfilerPlugin final : public StdPlugin<IProfilerPluginImpl<ProfilerPlugin, ThrowStatusExceptionWrapper>>
{
public:
	explicit ProfilerPlugin(IPluginConfig*)
	{
	}

	void init(ThrowStatusExceptionWrapper* status, IAttachment* attachment, FB_UINT64 aTicksFrequency) override;

	IProfilerSession* startSession(ThrowStatusExceptionWrapper* status,
		const char* description, const char* options, ISC_TIMESTAMP_TZ timestamp) override;

	void flush(ThrowStatusExceptionWrapper* status) override;

private:
	void createInitialMetadata(ThrowStatusExceptionWrapper* status, RefPtr<IAttachment> attachment,
		RefPtr<ITransaction> transaction);
	void recreateViews(ThrowStatusExceptionWrapper* status, RefPtr<IAttachment> attachment,
		RefPtr<ITransaction> transaction);

	void loadMetadata(ThrowStatusExceptionWrapper* status);

public:
	RefPtr<IAttachment> userAttachment;
	ObjectsArray<RefPtr<Session>> sessions{getPool()};
};

//--------------------------------------

void ProfilerPlugin::init(ThrowStatusExceptionWrapper* status, IAttachment* attachment, FB_UINT64 aTicksFrequency)
{
	userAttachment = attachment;
	ticksFrequency = (SINT64) aTicksFrequency;

	constexpr auto sql = R"""(
		select exists(
		           select true
		               from system.rdb$relation_fields
		               where rdb$schema_name = 'PLG$PROFILER' and
		                     rdb$relation_name = 'PLG$PROF_STATEMENTS' and
		                     rdb$field_name = 'SCHEMA_NAME'
		       ) metadata_created,
		       rdb$get_context('SYSTEM', 'DB_NAME') db_name,
		       (select rdb$owner_name
		            from system.rdb$relations
		            where rdb$schema_name = 'SYSTEM' and
		                  rdb$relation_name = 'RDB$DATABASE'
		       ) owner_name,
		       current_role,
		       rdb$role_in_use('PLG$PROFILER') role_in_use
		    from system.rdb$database
	)""";

	FB_MESSAGE(message, ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, metadataCreated)
		(FB_INTL_VARCHAR(MAXPATHLEN * 4, CS_METADATA), dbName)
		(FB_INTL_VARCHAR(MAX_SQL_IDENTIFIER_LEN, CS_METADATA), ownerName)
		(FB_INTL_VARCHAR(MAX_SQL_IDENTIFIER_LEN, CS_METADATA), currentRole)
		(FB_BOOLEAN, roleInUse)
	) message(status, MasterInterfacePtr());
	message.clear();

	RefPtr<IAttachment> refAttachment(attachment);
	RefPtr<ITransaction> refTransaction;
	string currentRole;
	bool roleInUse;

	for (unsigned i = 0; i < 2; ++i)
	{
		refTransaction = makeNoIncRef(refAttachment->startTransaction(status, 0, nullptr));

		auto resultSet = makeNoIncRef(refAttachment->openCursor(status, refTransaction, 0, sql, SQL_DIALECT_CURRENT,
			nullptr, nullptr, message.getMetadata(), nullptr, 0));

		if (resultSet->fetchNext(status, message.getData()) == IStatus::RESULT_NO_DATA)
		{
			fb_assert(false);
			return;
		}

		if (i == 0)
		{
			currentRole = string(message->currentRole.str, message->currentRole.length);
			quote(currentRole);

			roleInUse = message->roleInUse;

			if (message->metadataCreated)
				break;

			auto dispatcher = makeNoIncRef(MasterInterfacePtr()->getDispatcher());
			const auto util = MasterInterfacePtr()->getUtilInterface();

			const auto dbName = string(message->dbName.str, message->dbName.length);

			auto ownerName = string(message->ownerName.str, message->ownerName.length);
			quote(ownerName);

			AutoDispose<IXpbBuilder> dpb(util->getXpbBuilder(status, IXpbBuilder::DPB, nullptr, 0));
			dpb->insertString(status, isc_dpb_user_name, ownerName.c_str());
			dpb->insertTag(status, isc_dpb_utf8_filename);
			dpb->insertInt(status, isc_dpb_no_db_triggers, 1);

			refAttachment = makeNoIncRef(dispatcher->attachDatabase(status, dbName.c_str(),
				dpb->getBufferLength(status), dpb->getBuffer(status)));
		}
	}

	if (!message->metadataCreated)
		createInitialMetadata(status, refAttachment, refTransaction);

	if (!roleInUse)
	{
		// Refresh roles.

		attachment->execute(status, nullptr, 0, "set role plg$profiler",
			SQL_DIALECT_CURRENT, nullptr, nullptr, nullptr, nullptr);

		attachment->execute(status, nullptr, 0, ("set role " + currentRole).c_str(),
			SQL_DIALECT_CURRENT, nullptr, nullptr, nullptr, nullptr);
	}

	loadMetadata(status);
}

IProfilerSession* ProfilerPlugin::startSession(ThrowStatusExceptionWrapper* status,
	const char* description, const char* options, ISC_TIMESTAMP_TZ timestamp)
{
	AutoDispose<Session> session(FB_NEW Session(status, this, description, timestamp));

	if (options && options[0])
	{
		string optionsStr = options;
		optionsStr.alltrim();

		if (optionsStr.hasData())
		{
			optionsStr.upper();

			if (optionsStr == "DETAILED_REQUESTS")
				session->detailedRequests = true;
			else
			{
				static const ISC_STATUS statusVector[] = {
					isc_arg_gds,
					isc_random,
					isc_arg_string,
					(ISC_STATUS) "Invalid OPTIONS for Default_Profiler.",
					isc_arg_end
				};

				status->setErrors(statusVector);
				return nullptr;
			}
		}
	}

	return session.release();
}

void ProfilerPlugin::flush(ThrowStatusExceptionWrapper* status)
{
	constexpr auto sessionSql = R"""(
		update or insert into plg$profiler.plg$prof_sessions
		    (profile_id, attachment_id, user_name, description, start_timestamp, finish_timestamp)
		    values (?, current_connection, current_user, ?, ?, ?)
		    matching (profile_id)
	)""";

	FB_MESSAGE(SessionMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, profileId)
		(FB_INTL_VARCHAR(255 * 4, CS_UTF8), description)
		(FB_TIMESTAMP_TZ, startTimestamp)
		(FB_TIMESTAMP_TZ, finishTimestamp)
	) sessionMessage(status, MasterInterfacePtr());
	sessionMessage.clear();

	constexpr auto statementSql = R"""(
		update or insert into plg$profiler.plg$prof_statements
		    (profile_id, statement_id, parent_statement_id, statement_type, schema_name, package_name, routine_name,
		     sql_text)
		    values (?, ?, ?, ?, ?, ?, ?, ?)
		    matching (profile_id, statement_id)
	)""";

	FB_MESSAGE(StatementMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, profileId)
		(FB_BIGINT, statementId)
		(FB_BIGINT, parentStatementId)
		(FB_INTL_VARCHAR(20 * 4, CS_UTF8), statementType)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * 4, CS_UTF8), schemaName)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * 4, CS_UTF8), packageName)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * 4, CS_UTF8), routineName)
		(FB_BLOB, sqlText)
	) statementMessage(status, MasterInterfacePtr());
	statementMessage.clear();

	constexpr auto cursorSql = R"""(
		update or insert into plg$profiler.plg$prof_cursors
		    (profile_id, statement_id, cursor_id, name, line_num, column_num)
		    values (?, ?, ?, ?, ?, ?)
		    matching (profile_id, statement_id, cursor_id)
	)""";

	FB_MESSAGE(CursorMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, profileId)
		(FB_BIGINT, statementId)
		(FB_INTEGER, cursorId)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * 4, CS_UTF8), name)
		(FB_BIGINT, lineNum)
		(FB_BIGINT, columnNum)
	) cursorMessage(status, MasterInterfacePtr());
	cursorMessage.clear();

	constexpr auto recSrcSql = R"""(
		update or insert into plg$profiler.plg$prof_record_sources
		    (profile_id, statement_id, cursor_id, record_source_id,
		     parent_record_source_id, level, access_path)
		    values (?, ?, ?, ?, ?, ?, ?)
		    matching (profile_id, statement_id, cursor_id, record_source_id)
	)""";

	FB_MESSAGE(RecSrcMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, profileId)
		(FB_BIGINT, statementId)
		(FB_INTEGER, cursorId)
		(FB_INTEGER, recordSourceId)
		(FB_INTEGER, parentRecordSourceId)
		(FB_INTEGER, level)
		(FB_BLOB, accessPath)
	) recSrcMessage(status, MasterInterfacePtr());
	recSrcMessage.clear();

	constexpr auto requestSql = R"""(
		update or insert into plg$profiler.plg$prof_requests
		    (profile_id, statement_id, request_id, caller_statement_id, caller_request_id, start_timestamp,
		     finish_timestamp, total_elapsed_time)
		    values (?, ?, ?, ?, ?, ?, ?, ?)
		    matching (profile_id, statement_id, request_id)
	)""";

	FB_MESSAGE(RequestMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, profileId)
		(FB_BIGINT, statementId)
		(FB_BIGINT, requestId)
		(FB_BIGINT, callerStatementId)
		(FB_BIGINT, callerRequestId)
		(FB_TIMESTAMP_TZ, startTimestamp)
		(FB_TIMESTAMP_TZ, finishTimestamp)
		(FB_BIGINT, totalElapsedTime)
	) requestMessage(status, MasterInterfacePtr());
	requestMessage.clear();

	constexpr auto recSrcStatSql = R"""(
		execute block (
		    profile_id type of column plg$profiler.plg$prof_record_source_stats.profile_id = ?,
		    statement_id type of column plg$profiler.plg$prof_record_source_stats.statement_id = ?,
		    request_id type of column plg$profiler.plg$prof_record_source_stats.request_id = ?,
		    cursor_id type of column plg$profiler.plg$prof_record_source_stats.cursor_id = ?,
		    record_source_id type of column plg$profiler.plg$prof_record_source_stats.record_source_id = ?,
		    open_counter type of column plg$profiler.plg$prof_record_source_stats.open_counter = ?,
		    open_min_elapsed_time type of column plg$profiler.plg$prof_record_source_stats.open_min_elapsed_time = ?,
		    open_max_elapsed_time type of column plg$profiler.plg$prof_record_source_stats.open_max_elapsed_time = ?,
		    open_total_elapsed_time type of column plg$profiler.plg$prof_record_source_stats.open_total_elapsed_time = ?,
		    fetch_counter type of column plg$profiler.plg$prof_record_source_stats.fetch_counter = ?,
		    fetch_min_elapsed_time type of column plg$profiler.plg$prof_record_source_stats.fetch_min_elapsed_time = ?,
		    fetch_max_elapsed_time type of column plg$profiler.plg$prof_record_source_stats.fetch_max_elapsed_time = ?,
		    fetch_total_elapsed_time type of column plg$profiler.plg$prof_record_source_stats.fetch_total_elapsed_time = ?
		)
		as
		begin
		    merge into plg$profiler.plg$prof_record_source_stats
		        using system.rdb$database on
		            profile_id = :profile_id and
		            statement_id = :statement_id and
		            request_id = :request_id and
		            cursor_id = :cursor_id and
		            record_source_id = :record_source_id
		        when not matched then
		            insert (profile_id, statement_id, request_id, cursor_id, record_source_id,
		                    open_counter, open_min_elapsed_time, open_max_elapsed_time, open_total_elapsed_time,
		                    fetch_counter, fetch_min_elapsed_time, fetch_max_elapsed_time, fetch_total_elapsed_time)
		                values (:profile_id, :statement_id, :request_id, :cursor_id, :record_source_id,
		                        :open_counter, :open_min_elapsed_time, :open_max_elapsed_time, :open_total_elapsed_time,
		                        :fetch_counter, :fetch_min_elapsed_time, :fetch_max_elapsed_time, :fetch_total_elapsed_time)
		        when matched then
		            update set
		                open_counter = open_counter + :open_counter,
		                open_min_elapsed_time = minvalue(open_min_elapsed_time, :open_min_elapsed_time),
		                open_max_elapsed_time = maxvalue(open_max_elapsed_time, :open_max_elapsed_time),
		                open_total_elapsed_time = open_total_elapsed_time + :open_total_elapsed_time,
		                fetch_counter = fetch_counter + :fetch_counter,
		                fetch_min_elapsed_time = minvalue(fetch_min_elapsed_time, :fetch_min_elapsed_time),
		                fetch_max_elapsed_time = maxvalue(fetch_max_elapsed_time, :fetch_max_elapsed_time),
		                fetch_total_elapsed_time = fetch_total_elapsed_time + :fetch_total_elapsed_time;
		end
	)""";

	FB_MESSAGE(RecSrcStatMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, profileId)
		(FB_BIGINT, statementId)
		(FB_BIGINT, requestId)
		(FB_INTEGER, cursorId)
		(FB_INTEGER, recordSourceId)
		(FB_BIGINT, openCounter)
		(FB_BIGINT, openMinElapsedTime)
		(FB_BIGINT, openMaxElapsedTime)
		(FB_BIGINT, openTotalElapsedTime)
		(FB_BIGINT, fetchCounter)
		(FB_BIGINT, fetchMinElapsedTime)
		(FB_BIGINT, fetchMaxElapsedTime)
		(FB_BIGINT, fetchTotalElapsedTime)
	) recSrcStatMessage(status, MasterInterfacePtr());
	recSrcStatMessage.clear();

	constexpr auto psqlStatSql = R"""(
		execute block (
		    profile_id type of column plg$profiler.plg$prof_psql_stats.profile_id = ?,
		    statement_id type of column plg$profiler.plg$prof_psql_stats.statement_id = ?,
		    request_id type of column plg$profiler.plg$prof_psql_stats.request_id = ?,
		    line_num type of column plg$profiler.plg$prof_psql_stats.line_num = ?,
		    column_num type of column plg$profiler.plg$prof_psql_stats.column_num = ?,
		    counter type of column plg$profiler.plg$prof_psql_stats.counter = ?,
		    min_elapsed_time type of column plg$profiler.plg$prof_psql_stats.min_elapsed_time = ?,
		    max_elapsed_time type of column plg$profiler.plg$prof_psql_stats.max_elapsed_time = ?,
		    total_elapsed_time type of column plg$profiler.plg$prof_psql_stats.total_elapsed_time = ?
		)
		as
		begin
		    merge into plg$profiler.plg$prof_psql_stats
		        using system.rdb$database on
		            profile_id = :profile_id and
		            statement_id = :statement_id and
		            request_id = :request_id and
		            line_num = :line_num and
		            column_num = :column_num
		        when not matched then
		            insert (profile_id, statement_id, request_id, line_num, column_num,
		                    counter, min_elapsed_time, max_elapsed_time, total_elapsed_time)
		                values (:profile_id, :statement_id, :request_id, :line_num, :column_num,
		                        :counter, :min_elapsed_time, :max_elapsed_time, :total_elapsed_time)
		        when matched then
		            update set
		                counter = counter + :counter,
		                min_elapsed_time = minvalue(min_elapsed_time, :min_elapsed_time),
		                max_elapsed_time = maxvalue(max_elapsed_time, :max_elapsed_time),
		                total_elapsed_time = total_elapsed_time + :total_elapsed_time;
		end
	)""";

	FB_MESSAGE(PsqlStatMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, profileId)
		(FB_BIGINT, statementId)
		(FB_BIGINT, requestId)
		(FB_INTEGER, lineNum)
		(FB_INTEGER, columnNum)
		(FB_BIGINT, counter)
		(FB_BIGINT, minElapsedTime)
		(FB_BIGINT, maxElapsedTime)
		(FB_BIGINT, totalElapsedTime)
	) psqlStatMessage(status, MasterInterfacePtr());
	psqlStatMessage.clear();

	auto transaction = makeNoIncRef(userAttachment->startTransaction(status, 0, nullptr));

	auto sessionStmt = makeNoIncRef(userAttachment->prepare(status, transaction, 0, sessionSql, SQL_DIALECT_CURRENT, 0));
	auto statementStmt = makeNoIncRef(userAttachment->prepare(
		status, transaction, 0, statementSql, SQL_DIALECT_CURRENT, 0));
	auto cursorStmt = makeNoIncRef(userAttachment->prepare(
		status, transaction, 0, cursorSql, SQL_DIALECT_CURRENT, 0));
	auto recSrcStmt = makeNoIncRef(userAttachment->prepare(
		status, transaction, 0, recSrcSql, SQL_DIALECT_CURRENT, 0));
	auto requestBatch = makeNoIncRef(userAttachment->createBatch(status, transaction, 0, requestSql, SQL_DIALECT_CURRENT,
		requestMessage.getMetadata(), 0, nullptr));
	auto recSrcStatBatch = makeNoIncRef(userAttachment->createBatch(
		status, transaction, 0, recSrcStatSql, SQL_DIALECT_CURRENT, recSrcStatMessage.getMetadata(), 0, nullptr));
	auto psqlStatBatch = makeNoIncRef(userAttachment->createBatch(
		status, transaction, 0, psqlStatSql, SQL_DIALECT_CURRENT, psqlStatMessage.getMetadata(), 0, nullptr));

	unsigned requestBatchSize = 0;
	unsigned recSrcStatBatchSize = 0;
	unsigned psqlStatBatchSize = 0;

	const auto executeBatch = [&](IBatch* batch, unsigned& batchSize)
	{
		if (batchSize)
		{
			batchSize = 0;

			if (const auto batchCs = batch->execute(status, transaction))
				batchCs->dispose();
		}
	};

	const auto executeBatches = [&]()
	{
		executeBatch(requestBatch, requestBatchSize);
		executeBatch(recSrcStatBatch, recSrcStatBatchSize);
		executeBatch(psqlStatBatch, psqlStatBatchSize);
	};

	const auto addBatch = [&](IBatch* batch, unsigned& batchSize, const auto& message)
	{
		batch->add(status, 1, message.getData());

		if (++batchSize == 1000)
			executeBatches();
	};

	for (unsigned sessionIdx = 0; sessionIdx < sessions.getCount(); )
	{
		auto& session = sessions[sessionIdx];

		if (session->dirty)
		{
			sessionMessage->profileIdNull = FB_FALSE;
			sessionMessage->profileId = session->getId();

			sessionMessage->descriptionNull = session->description.isEmpty();
			sessionMessage->description.set(session->description.c_str());

			sessionMessage->startTimestampNull = FB_FALSE;
			sessionMessage->startTimestamp = session->startTimestamp;

			sessionMessage->finishTimestampNull = !session->finishTimestamp.has_value();
			if (session->finishTimestamp.has_value())
				sessionMessage->finishTimestamp = session->finishTimestamp.value();

			sessionStmt->execute(status, transaction, sessionMessage.getMetadata(),
				sessionMessage.getData(), nullptr, nullptr);

			session->dirty = false;
		}

		RightPooledMap<unsigned, Array<const NonPooledPair<SINT64, Statement>*>> statementsByLevel;

		for (auto& statementIt : session->statements)
		{
			auto& profileStatement = statementIt.second;

			if (auto currentStatement = &profileStatement; currentStatement->level == 0)
			{
				Stack<Statement*> stack;

				while (currentStatement && currentStatement->level == 0)
				{
					stack.push(currentStatement);
					currentStatement = session->statements.get(currentStatement->parentStatementId);
				}

				unsigned level = currentStatement ? currentStatement->level : 0;

				while (stack.hasData())
					stack.pop()->level = ++level;
			}

			const auto levelArray = statementsByLevel.getOrPut(profileStatement.level);
			levelArray->add(&statementIt);
		}

		for (auto& levelIt : statementsByLevel)
		{
			for (const auto statementIt : levelIt.second)
			{
				const auto profileStatementId = statementIt->first;
				auto& profileStatement = statementIt->second;

				statementMessage->profileIdNull = FB_FALSE;
				statementMessage->profileId = session->getId();

				statementMessage->statementIdNull = FB_FALSE;
				statementMessage->statementId = profileStatementId;

				statementMessage->parentStatementIdNull = profileStatement.parentStatementId == 0 ? FB_TRUE : FB_FALSE;
				statementMessage->parentStatementId = profileStatement.parentStatementId;

				statementMessage->statementTypeNull = FB_FALSE;
				statementMessage->statementType.set(profileStatement.type.c_str());

				statementMessage->schemaNameNull = profileStatement.schemaName.isEmpty();
				statementMessage->schemaName.set(profileStatement.schemaName.c_str());

				statementMessage->packageNameNull = profileStatement.packageName.isEmpty();
				statementMessage->packageName.set(profileStatement.packageName.c_str());

				statementMessage->routineNameNull = profileStatement.routineName.isEmpty();
				statementMessage->routineName.set(profileStatement.routineName.c_str());

				statementMessage->sqlTextNull = profileStatement.sqlText.isEmpty();

				if (profileStatement.sqlText.hasData())
				{
					auto blob = makeNoIncRef(userAttachment->createBlob(
						status, transaction, &statementMessage->sqlText, sizeof(STREAM_BLOB_BPB), STREAM_BLOB_BPB));
					blob->putSegment(status, profileStatement.sqlText.length(), profileStatement.sqlText.c_str());
					blob->close(status);
					blob.clear();
				}

				statementStmt->execute(status, transaction, statementMessage.getMetadata(),
					statementMessage.getData(), nullptr, nullptr);
			}
		}

		for (const auto& cursorIt : session->cursors)
		{
			const auto statementId = cursorIt.first.first;
			const auto cursorId = cursorIt.first.second;
			const auto& cursor = cursorIt.second;

			cursorMessage->profileIdNull = FB_FALSE;
			cursorMessage->profileId = session->getId();

			cursorMessage->statementIdNull = FB_FALSE;
			cursorMessage->statementId = statementId;

			cursorMessage->cursorIdNull = FB_FALSE;
			cursorMessage->cursorId = cursorId;

			cursorMessage->nameNull = cursor.name.isEmpty();
			cursorMessage->name.set(cursor.name.c_str());

			cursorMessage->lineNumNull = cursor.line == 0 ? FB_TRUE : FB_FALSE;
			cursorMessage->lineNum = cursor.line;

			cursorMessage->columnNumNull = cursor.column == 0 ? FB_TRUE : FB_FALSE;
			cursorMessage->columnNum = cursor.column;

			cursorStmt->execute(status, transaction, cursorMessage.getMetadata(),
				cursorMessage.getData(), nullptr, nullptr);
		}

		for (const auto& recSourceIt : session->recordSources)
		{
			const auto statementId = recSourceIt.first.first.first;
			const auto cursorId = recSourceIt.first.first.second;
			const auto recSourceId = recSourceIt.first.second;
			const auto& recSrc = recSourceIt.second;

			recSrcMessage->profileIdNull = FB_FALSE;
			recSrcMessage->profileId = session->getId();

			recSrcMessage->statementIdNull = FB_FALSE;
			recSrcMessage->statementId = statementId;

			recSrcMessage->cursorIdNull = FB_FALSE;
			recSrcMessage->cursorId = cursorId;

			recSrcMessage->recordSourceIdNull = FB_FALSE;
			recSrcMessage->recordSourceId = recSourceId;

			recSrcMessage->parentRecordSourceIdNull = !recSrc.parentId.has_value();
			recSrcMessage->parentRecordSourceId = recSrc.parentId.value_or(0);

			recSrcMessage->levelNull = FB_FALSE;
			recSrcMessage->level = recSrc.level;

			recSrcMessage->accessPathNull = FB_FALSE;
			{	// scope
				auto blob = makeNoIncRef(userAttachment->createBlob(status, transaction, &recSrcMessage->accessPath,
					sizeof(STREAM_BLOB_BPB), STREAM_BLOB_BPB));
				blob->putSegment(status, recSrc.accessPath.length(), recSrc.accessPath.c_str());
				blob->close(status);
				blob.clear();
			}

			recSrcStmt->execute(status, transaction, recSrcMessage.getMetadata(),
				recSrcMessage.getData(), nullptr, nullptr);
		}

		RightPooledMap<unsigned, Array<NonPooledPair<SINT64, Request>*>> requestsByLevel;
		Array<SINT64> finishedRequests;

		for (auto& requestIt : session->requests)
		{
			auto& profileRequest = requestIt.second;

			if (auto currentRequest = &profileRequest; currentRequest->level == 0)
			{
				Stack<Request*> stack;

				while (currentRequest && currentRequest->level == 0)
				{
					stack.push(currentRequest);
					currentRequest = session->requests.get(currentRequest->callerRequestId);
				}

				unsigned level = currentRequest ? currentRequest->level : 0;

				while (stack.hasData())
					stack.pop()->level = ++level;
			}

			const auto levelArray = requestsByLevel.getOrPut(profileRequest.level);
			levelArray->add(&requestIt);
		}

		for (auto& levelIt : requestsByLevel)
		{
			for (const auto requestIt : levelIt.second)
			{
				const auto profileRequestId = MAX(requestIt->first, 0);
				auto& profileRequest = requestIt->second;

				if (profileRequest.dirty)
				{
					requestMessage->profileIdNull = FB_FALSE;
					requestMessage->profileId = session->getId();

					requestMessage->requestIdNull = FB_FALSE;
					requestMessage->requestId = profileRequestId;

					requestMessage->statementIdNull = FB_FALSE;
					requestMessage->statementId = profileRequest.statementId;

					requestMessage->callerStatementIdNull = profileRequest.callerStatementId == 0 ? FB_TRUE : FB_FALSE;
					requestMessage->callerStatementId = profileRequest.callerStatementId;

					requestMessage->callerRequestIdNull = profileRequest.callerRequestId == 0 ? FB_TRUE : FB_FALSE;
					requestMessage->callerRequestId = profileRequest.callerRequestId;

					requestMessage->startTimestampNull = FB_FALSE;
					requestMessage->startTimestamp = profileRequest.startTimestamp;

					requestMessage->finishTimestampNull = !profileRequest.finishTimestamp.has_value();
					if (profileRequest.finishTimestamp.has_value())
						requestMessage->finishTimestamp = profileRequest.finishTimestamp.value();

					requestMessage->totalElapsedTimeNull = !profileRequest.totalElapsedTicks.has_value();
					requestMessage->totalElapsedTime = ticksToNanoseconds(profileRequest.totalElapsedTicks.value_or(0));

					addBatch(requestBatch, requestBatchSize, requestMessage);

					if (profileRequest.finishTimestamp.has_value())
						finishedRequests.add(requestIt->first);

					profileRequest.dirty = false;
				}

				for (const auto& [cursorRecSource, stats] : profileRequest.recordSourcesStats)
				{
					recSrcStatMessage->profileIdNull = FB_FALSE;
					recSrcStatMessage->profileId = session->getId();

					recSrcStatMessage->statementIdNull = FB_FALSE;
					recSrcStatMessage->statementId = profileRequest.statementId;

					recSrcStatMessage->requestIdNull = FB_FALSE;
					recSrcStatMessage->requestId = profileRequestId;

					recSrcStatMessage->cursorIdNull = FB_FALSE;
					recSrcStatMessage->cursorId = cursorRecSource.first;

					recSrcStatMessage->recordSourceIdNull = FB_FALSE;
					recSrcStatMessage->recordSourceId = cursorRecSource.second;

					recSrcStatMessage->openCounterNull = FB_FALSE;
					recSrcStatMessage->openCounter = stats.openStats.counter;

					recSrcStatMessage->openMinElapsedTimeNull = FB_FALSE;
					recSrcStatMessage->openMinElapsedTime = ticksToNanoseconds(stats.openStats.minElapsedTicks);

					recSrcStatMessage->openMaxElapsedTimeNull = FB_FALSE;
					recSrcStatMessage->openMaxElapsedTime = ticksToNanoseconds(stats.openStats.maxElapsedTicks);

					recSrcStatMessage->openTotalElapsedTimeNull = FB_FALSE;
					recSrcStatMessage->openTotalElapsedTime = ticksToNanoseconds(stats.openStats.totalElapsedTicks);

					recSrcStatMessage->fetchCounterNull = FB_FALSE;
					recSrcStatMessage->fetchCounter = stats.fetchStats.counter;

					recSrcStatMessage->fetchMinElapsedTimeNull = FB_FALSE;
					recSrcStatMessage->fetchMinElapsedTime = ticksToNanoseconds(stats.fetchStats.minElapsedTicks);

					recSrcStatMessage->fetchMaxElapsedTimeNull = FB_FALSE;
					recSrcStatMessage->fetchMaxElapsedTime = ticksToNanoseconds(stats.fetchStats.maxElapsedTicks);

					recSrcStatMessage->fetchTotalElapsedTimeNull = FB_FALSE;
					recSrcStatMessage->fetchTotalElapsedTime = ticksToNanoseconds(stats.fetchStats.totalElapsedTicks);

					addBatch(recSrcStatBatch, recSrcStatBatchSize, recSrcStatMessage);
				}

				profileRequest.recordSourcesStats.clear();

				for (const auto& statsIt : profileRequest.psqlStats)
				{
					const auto& lineColumn = statsIt.first;

					psqlStatMessage->profileIdNull = FB_FALSE;
					psqlStatMessage->profileId = session->getId();

					psqlStatMessage->statementIdNull = FB_FALSE;
					psqlStatMessage->statementId = profileRequest.statementId;

					psqlStatMessage->requestIdNull = FB_FALSE;
					psqlStatMessage->requestId = profileRequestId;

					psqlStatMessage->lineNumNull = FB_FALSE;
					psqlStatMessage->lineNum = lineColumn.first;

					psqlStatMessage->columnNumNull = FB_FALSE;
					psqlStatMessage->columnNum = lineColumn.second;

					psqlStatMessage->counterNull = FB_FALSE;
					psqlStatMessage->counter = statsIt.second.counter;

					psqlStatMessage->minElapsedTimeNull = FB_FALSE;
					psqlStatMessage->minElapsedTime = ticksToNanoseconds(statsIt.second.minElapsedTicks);

					psqlStatMessage->maxElapsedTimeNull = FB_FALSE;
					psqlStatMessage->maxElapsedTime = ticksToNanoseconds(statsIt.second.maxElapsedTicks);

					psqlStatMessage->totalElapsedTimeNull = FB_FALSE;
					psqlStatMessage->totalElapsedTime = ticksToNanoseconds(statsIt.second.totalElapsedTicks);

					addBatch(psqlStatBatch, psqlStatBatchSize, psqlStatMessage);
				}

				profileRequest.psqlStats.clear();
			}
		}

		if (!session->finishTimestamp.has_value())
		{
			session->statements.clear();
			session->recordSources.clear();
			session->cursors.clear();

			for (const auto requestId : finishedRequests)
				session->requests.remove(requestId);

			++sessionIdx;
		}
		else
			sessions.remove(sessionIdx);
	}

	executeBatches();

	transaction->commit(status);
	transaction.clear();
}

void ProfilerPlugin::createInitialMetadata(ThrowStatusExceptionWrapper* status, RefPtr<IAttachment> attachment,
	RefPtr<ITransaction> transaction)
{
	constexpr const char* createSqlStaments[] = {
		"create role plg$profiler",

		"grant default plg$profiler to public",

		"create schema plg$profiler default character set utf8",

		"grant usage on schema plg$profiler to plg$profiler",

		"create sequence plg$profiler.plg$prof_profile_id",

		"grant usage on sequence plg$profiler.plg$prof_profile_id to plg$profiler",

		R"""(
		create table plg$profiler.plg$prof_sessions (
		    profile_id bigint not null
		        constraint plg$prof_sessions_pk
		            primary key
		            using index plg$prof_sessions_profile,
		    attachment_id bigint not null,
		    user_name char(63) character set utf8 not null,
		    description varchar(255) character set utf8,
		    start_timestamp timestamp with time zone not null,
		    finish_timestamp timestamp with time zone
		))""",

		"grant select, update, insert, delete on table plg$profiler.plg$prof_sessions to plg$profiler",

		R"""(
		create table plg$profiler.plg$prof_statements (
		    profile_id bigint not null
		        constraint plg$prof_statements_session_fk
		            references plg$profiler.plg$prof_sessions
		            on delete cascade
		            using index plg$prof_statements_profile,
		    statement_id bigint not null,
		    parent_statement_id bigint,
		    statement_type varchar(20) character set utf8 not null,
		    schema_name char(63) character set utf8,
		    package_name char(63) character set utf8,
		    routine_name char(63) character set utf8,
		    sql_text blob sub_type text character set utf8,
		    constraint plg$prof_statements_pk
		        primary key (profile_id, statement_id)
		        using index plg$prof_statements_profile_statement,
		    constraint plg$prof_statements_parent_statement_fk
		        foreign key (profile_id, parent_statement_id)
		        references plg$profiler.plg$prof_statements (profile_id, statement_id)
		        on delete cascade
		        using index plg$prof_statements_parent_statement
		))""",

		"grant select, update, insert, delete on table plg$profiler.plg$prof_statements to plg$profiler",

		R"""(
		create table plg$profiler.plg$prof_cursors (
		    profile_id bigint not null
		        constraint plg$prof_cursors_session_fk
		            references plg$profiler.plg$prof_sessions
		            on delete cascade
		            using index plg$prof_cursors_profile,
		    statement_id bigint not null,
		    cursor_id integer not null,
		    name char(63) character set utf8,
		    line_num integer,
		    column_num integer,
		    constraint plg$prof_cursors_pk
		        primary key (profile_id, statement_id, cursor_id)
		        using index plg$prof_cursors_profile_statement_cursor,
		    constraint plg$prof_cursors_statement_fk
		        foreign key (profile_id, statement_id) references plg$profiler.plg$prof_statements
		        on delete cascade
		        using index plg$prof_cursors_profile_statement
		))""",

		"grant select, update, insert, delete on table plg$profiler.plg$prof_cursors to plg$profiler",

		R"""(
		create table plg$profiler.plg$prof_record_sources (
		    profile_id bigint not null
		        constraint plg$prof_record_sources_session_fk
		            references plg$profiler.plg$prof_sessions
		            on delete cascade
		            using index plg$prof_record_sources_profile,
		    statement_id bigint not null,
		    cursor_id integer not null,
		    record_source_id integer not null,
		    parent_record_source_id integer,
		    level integer not null,
		    access_path blob sub_type text character set utf8 not null,
		    constraint plg$prof_record_sources_pk
		        primary key (profile_id, statement_id, cursor_id, record_source_id)
		        using index plg$prof_record_sources_profile_statement_cursor_recsource,
		    constraint plg$prof_record_sources_statement_fk
		        foreign key (profile_id, statement_id) references plg$profiler.plg$prof_statements
		        on delete cascade
		        using index plg$prof_record_sources_profile_statement,
		    constraint plg$prof_record_sources_cursor_fk
		        foreign key (profile_id, statement_id, cursor_id) references plg$profiler.plg$prof_cursors
		        on delete cascade
		        using index plg$prof_record_sources_profile_statement_cursor,
		    constraint plg$prof_record_sources_parent_record_source_fk
		        foreign key (profile_id, statement_id, cursor_id, parent_record_source_id)
		        references plg$profiler.plg$prof_record_sources (profile_id, statement_id, cursor_id, record_source_id)
		        on delete cascade
		        using index plg$prof_record_sources_profile_statement_cursor_parent_rec_src
		))""",

		"grant select, update, insert, delete on table plg$profiler.plg$prof_record_sources to plg$profiler",

		R"""(
		create table plg$profiler.plg$prof_requests (
		    profile_id bigint not null
		        constraint plg$prof_requests_session_fk
		            references plg$profiler.plg$prof_sessions
		            on delete cascade
		            using index plg$prof_requests_profile,
		    statement_id bigint not null,
		    request_id bigint not null,
		    caller_statement_id bigint,
		    caller_request_id bigint,
		    start_timestamp timestamp with time zone not null,
		    finish_timestamp timestamp with time zone,
		    total_elapsed_time bigint,
		    constraint plg$prof_requests_pk
		        primary key (profile_id, statement_id, request_id)
		        using index plg$prof_requests_profile_request_statement,
		    constraint plg$prof_requests_statement_fk
		        foreign key (profile_id, statement_id) references plg$profiler.plg$prof_statements
		        on delete cascade
		        using index plg$prof_requests_profile_statement,
		    constraint plg$prof_requests_caller_statement_fk
		        foreign key (profile_id, caller_statement_id) references plg$profiler.plg$prof_statements
		        on delete cascade
		        using index plg$prof_requests_profile_caller_statement,
		    constraint plg$prof_requests_caller_request_fk
		        foreign key (profile_id, caller_statement_id, caller_request_id)
		        references plg$profiler.plg$prof_requests (profile_id, statement_id, request_id)
		        on delete cascade
		        using index plg$prof_requests_profile_caller_statement_caller_request
		))""",

		"grant select, update, insert, delete on table plg$profiler.plg$prof_requests to plg$profiler",

		R"""(
		create table plg$profiler.plg$prof_psql_stats (
		    profile_id bigint not null
		        constraint plg$prof_psql_stats_session_fk
		            references plg$profiler.plg$prof_sessions
		            on delete cascade
		            using index plg$prof_psql_stats_profile,
		    statement_id bigint not null,
		    request_id bigint not null,
		    line_num integer not null,
		    column_num integer not null,
		    counter bigint not null,
		    min_elapsed_time bigint not null,
		    max_elapsed_time bigint not null,
		    total_elapsed_time bigint not null,
		    constraint plg$prof_psql_stats_pk
		        primary key (profile_id, statement_id, request_id, line_num, column_num)
		        using index plg$prof_psql_stats_profile_statement_request_line_column,
		    constraint plg$prof_psql_stats_request_fk
		        foreign key (profile_id, statement_id, request_id) references plg$profiler.plg$prof_requests
		        on delete cascade
		        using index plg$prof_psql_stats_profile_request,
		    constraint plg$prof_psql_stats_statement_fk
		        foreign key (profile_id, statement_id) references plg$profiler.plg$prof_statements
		        on delete cascade
		        using index plg$prof_psql_stats_profile_statement
		))""",

		"grant select, update, insert, delete on table plg$profiler.plg$prof_psql_stats to plg$profiler",

		R"""(
		create table plg$profiler.plg$prof_record_source_stats (
		    profile_id bigint not null
		        constraint plg$prof_record_source_stats_session_fk
		            references plg$profiler.plg$prof_sessions
		            on delete cascade
		            using index plg$prof_record_source_stats_profile_id,
		    statement_id bigint not null,
		    request_id bigint not null,
		    cursor_id integer not null,
		    record_source_id integer not null,
		    open_counter bigint not null,
		    open_min_elapsed_time bigint not null,
		    open_max_elapsed_time bigint not null,
		    open_total_elapsed_time bigint not null,
		    fetch_counter bigint not null,
		    fetch_min_elapsed_time bigint not null,
		    fetch_max_elapsed_time bigint not null,
		    fetch_total_elapsed_time bigint not null,
		    constraint plg$prof_record_source_stats_pk
		        primary key (profile_id, statement_id, request_id, cursor_id, record_source_id)
		        using index plg$prof_record_source_stats_profile_stat_req_cur_recsource,
		    constraint plg$prof_record_source_stats_request_fk
		        foreign key (profile_id, statement_id, request_id) references plg$profiler.plg$prof_requests
		        on delete cascade
		        using index plg$prof_record_source_stats_profile_request,
		    constraint plg$prof_record_source_stats_statement_fk
		        foreign key (profile_id, statement_id) references plg$profiler.plg$prof_statements
		        on delete cascade
		        using index plg$prof_record_source_stats_profile_statement,
		    constraint plg$prof_record_source_stats_cursor_fk
		        foreign key (profile_id, statement_id, cursor_id) references plg$profiler.plg$prof_cursors
		        on delete cascade
		        using index plg$prof_record_source_stats_statement_cursor,
		    constraint plg$prof_record_source_stats_record_source_fk
		        foreign key (profile_id, statement_id, cursor_id, record_source_id)
		        references plg$profiler.plg$prof_record_sources
		        on delete cascade
		        using index plg$prof_record_source_stats_statement_cursor_record_source
		))""",

		"grant select, update, insert, delete on table plg$profiler.plg$prof_record_source_stats to plg$profiler"
	};

	for (const auto createSql : createSqlStaments)
	{
		attachment->execute(status, transaction, 0, createSql, SQL_DIALECT_CURRENT,
			nullptr, nullptr, nullptr, nullptr);
	}

	recreateViews(status, attachment, transaction);

	transaction->commit(status);
	transaction.clear();
}

void ProfilerPlugin::recreateViews(ThrowStatusExceptionWrapper* status, RefPtr<IAttachment> attachment,
	RefPtr<ITransaction> transaction)
{
	constexpr const char* sqlStaments[] = {
		R"""(
		recreate view plg$profiler.plg$prof_statement_stats_view
		as
		select req.profile_id,
		       req.statement_id,
		       sta.statement_type,
		       sta.schema_name,
		       sta.package_name,
		       sta.routine_name,
		       sta.parent_statement_id,
		       sta_parent.statement_type parent_statement_type,
		       sta_parent.routine_name parent_routine_name,
		       (select sql_text
		          from plg$profiler.plg$prof_statements
		          where profile_id = req.profile_id and
		                statement_id = coalesce(sta.parent_statement_id, req.statement_id)
		       ) sql_text,
		       count(*) counter,
		       min(req.total_elapsed_time) min_elapsed_time,
		       max(req.total_elapsed_time) max_elapsed_time,
		       cast(sum(req.total_elapsed_time) as bigint) total_elapsed_time,
		       cast(sum(req.total_elapsed_time) / count(*) as bigint) avg_elapsed_time
		  from plg$profiler.plg$prof_requests req
		  join plg$profiler.plg$prof_statements sta
		    on sta.profile_id = req.profile_id and
		       sta.statement_id = req.statement_id
		  left join plg$profiler.plg$prof_statements sta_parent
		    on sta_parent.profile_id = sta.profile_id and
		       sta_parent.statement_id = sta.parent_statement_id
		  group by req.profile_id,
		           req.statement_id,
		           sta.statement_type,
		           sta.schema_name,
		           sta.package_name,
		           sta.routine_name,
		           sta.parent_statement_id,
		           sta_parent.statement_type,
		           sta_parent.routine_name
		  order by sum(req.total_elapsed_time) desc
		)""",

		"grant select on table plg$profiler.plg$prof_statement_stats_view to plg$profiler",

		R"""(
		recreate view plg$profiler.plg$prof_psql_stats_view
		as
		select pstat.profile_id,
		       pstat.statement_id,
		       sta.statement_type,
		       sta.schema_name,
		       sta.package_name,
		       sta.routine_name,
		       sta.parent_statement_id,
		       sta_parent.statement_type parent_statement_type,
		       sta_parent.routine_name parent_routine_name,
		       (select sql_text
		          from plg$profiler.plg$prof_statements
		          where profile_id = pstat.profile_id and
		                statement_id = coalesce(sta.parent_statement_id, pstat.statement_id)
		       ) sql_text,
		       pstat.line_num,
		       pstat.column_num,
		       cast(sum(pstat.counter) as bigint) counter,
		       min(pstat.min_elapsed_time) min_elapsed_time,
		       max(pstat.max_elapsed_time) max_elapsed_time,
		       cast(sum(pstat.total_elapsed_time) as bigint) total_elapsed_time,
		       cast(sum(pstat.total_elapsed_time) / nullif(sum(pstat.counter), 0) as bigint) avg_elapsed_time
		  from plg$profiler.plg$prof_psql_stats pstat
		  join plg$profiler.plg$prof_statements sta
		    on sta.profile_id = pstat.profile_id and
		       sta.statement_id = pstat.statement_id
		  left join plg$profiler.plg$prof_statements sta_parent
		    on sta_parent.profile_id = sta.profile_id and
		       sta_parent.statement_id = sta.parent_statement_id
		  group by pstat.profile_id,
		           pstat.statement_id,
		           sta.statement_type,
		           sta.schema_name,
		           sta.package_name,
		           sta.routine_name,
		           sta.parent_statement_id,
		           sta_parent.statement_type,
		           sta_parent.routine_name,
		           pstat.line_num,
		           pstat.column_num
		  order by sum(pstat.total_elapsed_time) desc
		)""",

		"grant select on table plg$profiler.plg$prof_psql_stats_view to plg$profiler",

		R"""(
		recreate view plg$profiler.plg$prof_record_source_stats_view
		as
		select rstat.profile_id,
		       rstat.statement_id,
		       sta.statement_type,
		       sta.schema_name,
		       sta.package_name,
		       sta.routine_name,
		       sta.parent_statement_id,
		       sta_parent.statement_type parent_statement_type,
		       sta_parent.routine_name parent_routine_name,
		       (select sql_text
		          from plg$profiler.plg$prof_statements
		          where profile_id = rstat.profile_id and
		                statement_id = coalesce(sta.parent_statement_id, rstat.statement_id)
		       ) sql_text,
		       rstat.cursor_id,
		       cur.name cursor_name,
		       cur.line_num cursor_line_num,
		       cur.column_num cursor_column_num,
		       rstat.record_source_id,
		       recsrc.parent_record_source_id,
		       recsrc.level,
		       recsrc.access_path,
		       cast(sum(rstat.open_counter) as bigint) open_counter,
		       min(rstat.open_min_elapsed_time) open_min_elapsed_time,
		       max(rstat.open_max_elapsed_time) open_max_elapsed_time,
		       cast(sum(rstat.open_total_elapsed_time) as bigint) open_total_elapsed_time,
		       cast(sum(rstat.open_total_elapsed_time) / nullif(sum(rstat.open_counter), 0) as bigint) open_avg_elapsed_time,
		       cast(sum(rstat.fetch_counter) as bigint) fetch_counter,
		       min(rstat.fetch_min_elapsed_time) fetch_min_elapsed_time,
		       max(rstat.fetch_max_elapsed_time) fetch_max_elapsed_time,
		       cast(sum(rstat.fetch_total_elapsed_time) as bigint) fetch_total_elapsed_time,
		       cast(sum(rstat.fetch_total_elapsed_time) / nullif(sum(rstat.fetch_counter), 0) as bigint) fetch_avg_elapsed_time,
		       cast(coalesce(sum(rstat.open_total_elapsed_time), 0) + coalesce(sum(rstat.fetch_total_elapsed_time), 0) as bigint) open_fetch_total_elapsed_time
		  from plg$profiler.plg$prof_record_source_stats rstat
		  join plg$profiler.plg$prof_cursors cur
		    on cur.profile_id = rstat.profile_id and
		       cur.statement_id = rstat.statement_id and
		       cur.cursor_id = rstat.cursor_id
		  join plg$profiler.plg$prof_record_sources recsrc
		    on recsrc.profile_id = rstat.profile_id and
		       recsrc.statement_id = rstat.statement_id and
		       recsrc.cursor_id = rstat.cursor_id and
		       recsrc.record_source_id = rstat.record_source_id
		  join plg$profiler.plg$prof_statements sta
		    on sta.profile_id = rstat.profile_id and
		       sta.statement_id = rstat.statement_id
		  left join plg$profiler.plg$prof_statements sta_parent
		    on sta_parent.profile_id = sta.profile_id and
		       sta_parent.statement_id = sta.parent_statement_id
		  group by rstat.profile_id,
		           rstat.statement_id,
		           sta.statement_type,
		           sta.schema_name,
		           sta.package_name,
		           sta.routine_name,
		           sta.parent_statement_id,
		           sta_parent.statement_type,
		           sta_parent.routine_name,
		           rstat.cursor_id,
		           cur.name,
		           cur.line_num,
		           cur.column_num,
		           rstat.record_source_id,
		           recsrc.parent_record_source_id,
		           recsrc.level,
		           recsrc.access_path
		  order by coalesce(sum(rstat.open_total_elapsed_time), 0) + coalesce(sum(rstat.fetch_total_elapsed_time), 0) desc
		)""",

		"grant select on table plg$profiler.plg$prof_record_source_stats_view to plg$profiler"
	};

	for (const auto sqlStatement : sqlStaments)
	{
		attachment->execute(status, transaction, 0, sqlStatement, SQL_DIALECT_CURRENT,
			nullptr, nullptr, nullptr, nullptr);
	}
}

// Load objects in engine caches so they can be used in the user's transaction.
void ProfilerPlugin::loadMetadata(ThrowStatusExceptionWrapper* status)
{
	constexpr auto loadObjectsSql =
		R"""(
		select *
		    from plg$profiler%schema.plg$prof_sessions
		    cross join plg$profiler%schema.plg$prof_statements
		    cross join plg$profiler%schema.plg$prof_record_sources
		    cross join plg$profiler%schema.plg$prof_requests
		    cross join plg$profiler%schema.plg$prof_psql_stats
		    cross join plg$profiler%schema.plg$prof_record_source_stats
		    where next value for plg$profiler.plg$prof_profile_id = 0
		)""";

	auto transaction = makeNoIncRef(userAttachment->startTransaction(status, 0, nullptr));

	makeNoIncRef(userAttachment->prepare(status, transaction, 0, loadObjectsSql, SQL_DIALECT_CURRENT, 0));

	transaction->commit(status);
	transaction.clear();
}

//--------------------------------------

Session::Session(ThrowStatusExceptionWrapper* status, ProfilerPlugin* aPlugin,
		const char* aDescription, ISC_TIMESTAMP_TZ aStartTimestamp)
	: plugin(aPlugin),
	  startTimestamp(aStartTimestamp),
	  description(defaultPool(), aDescription)
{
	FB_MESSAGE(SequenceMessage, ThrowStatusExceptionWrapper,
		(FB_BIGINT, value)
	) sequenceMessage(status, MasterInterfacePtr());
	sequenceMessage.clear();

	constexpr auto sequenceSql = "select next value for plg$profiler.plg$prof_profile_id from system.rdb$database";

	auto transaction = makeNoIncRef(plugin->userAttachment->startTransaction(status, 0, nullptr));

	auto resultSet = makeNoIncRef(plugin->userAttachment->openCursor(status, transaction, 0, sequenceSql,
		SQL_DIALECT_CURRENT,
		nullptr, nullptr, sequenceMessage.getMetadata(), nullptr, 0));

	resultSet->fetchNext(status, sequenceMessage.getData());
	id = sequenceMessage->value;

	transaction->commit(status);
	transaction.clear();

	plugin->sessions.add(makeRef(this));

	addRef();
}

void Session::cancel(ThrowStatusExceptionWrapper* status)
{
	for (unsigned sessionIdx = 0; sessionIdx < plugin->sessions.getCount(); ++sessionIdx)
	{
		const auto& session = plugin->sessions[sessionIdx];

		if (session.getPtr() == this)
		{
			plugin->sessions.remove(sessionIdx);
			break;
		}
	}
}

void Session::finish(ThrowStatusExceptionWrapper* status, ISC_TIMESTAMP_TZ timestamp)
{
	dirty = true;
	finishTimestamp = timestamp;
}

void Session::defineStatement2(ThrowStatusExceptionWrapper* status, SINT64 statementId, SINT64 parentStatementId,
	const char* type, const char* schemaName, const char* packageName, const char* routineName, const char* sqlText)
{
	const auto statement = statements.put(statementId);
	fb_assert(statement);

	if (!statement)
		return;

	statement->type = type;
	statement->schemaName = schemaName;
	statement->packageName = packageName;
	statement->routineName = routineName;
	statement->parentStatementId = parentStatementId;
	statement->sqlText = sqlText;
}

void Session::defineCursor(SINT64 statementId, unsigned cursorId, const char* name, unsigned line, unsigned column)
{
	const auto cursor = cursors.put({statementId, cursorId});
	fb_assert(cursor);

	if (!cursor)
		return;

	cursor->name = name;
	cursor->line = line;
	cursor->column = column;
}

void Session::defineRecordSource(SINT64 statementId, unsigned cursorId, unsigned recSourceId,
	unsigned level, const char* accessPath, unsigned parentRecordSourceId)
{
	const auto recSource = recordSources.put({{statementId, cursorId}, recSourceId});
	fb_assert(recSource);

	if (!recSource)
		return;

	recSource->level = level;
	recSource->accessPath = accessPath;

	if (parentRecordSourceId)
		recSource->parentId = parentRecordSourceId;
}

void Session::onRequestStart(ThrowStatusExceptionWrapper* status, SINT64 statementId, SINT64 requestId,
	SINT64 callerStatementId, SINT64 callerRequestId, ISC_TIMESTAMP_TZ timestamp)
{
	const auto request = requests.put(detailedRequests ? requestId : -statementId);

	if (!request)
		return;

	request->statementId = statementId;
	request->startTimestamp = timestamp;

	if (detailedRequests)
	{
		request->callerStatementId = callerStatementId;
		request->callerRequestId = callerRequestId;
	}
}

void Session::onRequestFinish(ThrowStatusExceptionWrapper* status, SINT64 statementId, SINT64 requestId,
	ISC_TIMESTAMP_TZ timestamp, IProfilerStats* stats)
{
	if (const auto request = requests.get(requestId))
	{
		request->dirty = true;
		request->finishTimestamp = timestamp;
		request->totalElapsedTicks = stats->getElapsedTicks();
	}
}

void Session::afterPsqlLineColumn(SINT64 statementId, SINT64 requestId,
	unsigned line, unsigned column, IProfilerStats* stats)
{
	if (const auto request = getRequest(statementId, requestId))
	{
		const auto profileStats = request->psqlStats.getOrPut({line, column});
		profileStats->hit(stats->getElapsedTicks());
	}
}

void Session::afterRecordSourceOpen(SINT64 statementId, SINT64 requestId, unsigned cursorId, unsigned recSourceId,
	IProfilerStats* stats)
{
	if (const auto request = getRequest(statementId, requestId))
	{
		const auto profileStats = request->recordSourcesStats.getOrPut({cursorId, recSourceId});
		profileStats->openStats.hit(stats->getElapsedTicks());
	}
}

void Session::afterRecordSourceGetRecord(SINT64 statementId, SINT64 requestId, unsigned cursorId, unsigned recSourceId,
	IProfilerStats* stats)
{
	if (const auto request = getRequest(statementId, requestId))
	{
		const auto profileStats = request->recordSourcesStats.getOrPut({cursorId, recSourceId});
		profileStats->fetchStats.hit(stats->getElapsedTicks());
	}
}

//--------------------------------------

SimpleFactory<ProfilerPlugin> factory;

} // anonymous namespace

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(IMaster* master)
{
	CachedMasterInterface::set(master);
	PluginManagerInterfacePtr()->registerPluginFactory(IPluginManager::TYPE_PROFILER, "Default_Profiler", &factory);
	getUnloadDetector()->registerMe();
}
