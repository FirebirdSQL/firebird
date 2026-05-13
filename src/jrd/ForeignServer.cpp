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
 *  The Original Code was created by Vasiliy Yashkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 Vasiliy Yashkov <vasiliy.yashkov13@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"

#include "../auth/SecureRemotePassword/Message.h"
#include "../common/os/path_utils.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/Mapping.h"
#include "../dsql/make_proto.h"

using namespace Firebird;
using namespace Jrd;

namespace Jrd
{

	[[noreturn]] void throwError(const Firebird::string& message, const Firebird::string& value)
	{
		Firebird::string msg;
		msg.printf("%s specifies: %s", message.c_str(), value.c_str());
		(Firebird::Arg::Gds(isc_random) << Firebird::Arg::Str(msg)).raise();
	}

	bool getExternalValue(const Firebird::string& value, ExternalValueType type, Firebird::string& output)
	{
		Firebird::string temp;

		switch (type)
		{
		case ExternalValueType::TYPE_ENV:
			fb_utils::readenv(value.c_str(), temp);
			if (temp.isEmpty())
				throwError("missing environment variable", value);
			break;

		case ExternalValueType::TYPE_FILE:
			{
				Firebird::PathName filename = value.c_str();
				PathUtils::fixupSeparators(filename);
				if (PathUtils::isRelative(filename))
					filename = fb_utils::getPrefix(Firebird::IConfigManager::DIR_CONF, filename.c_str());

				Firebird::AutoPtr<FILE> file(os_utils::fopen(filename.c_str(), "rt"));
				if (!file)
					throwError("missing or inaccessible file", value);

				if (temp.LoadFromFile(file))
					temp.alltrim("\r");

				if (temp.isEmpty())
					throwError("first empty line of file", value);
				break;
			}
		default:
			return false; // just string value
		}

		output = temp;

		return true;
	}

	bool getBooleanValue(const string& value)
	{
		string low(value);
		low.lower();

		if (low == "true" || low == "yes" || low == "1")
			return true;

		return false;
	}

}

void ForeignServer::addOption(const MetaName& name, const string& value, ExternalValueType type)
{
	ForeignOption* option = options.getOrPut(name);
	option->m_name = name;
	option->m_value = value;
	option->m_type = type;
}

const GenericMap<MetaStringOptionPair>& ForeignServer::getOptions() const
{
	return options;
}

ForeignTableConnection* ForeignTableProvider::createForeignConnection(thread_db* tdbb, ForeignServer* server)
{
	Jrd::Attachment* attachment = tdbb->getAttachment();
	const string& currentUser = attachment->getEffectiveUserName().c_str();
	const string& fServer = server->getServerName().c_str();

	// If there is mapping for user and foreign server,
	// get a map of the connection parameters.
	Mapping mapping(Mapping::MAP_NO_FLAGS, NULL);
	GenericMap<MetaStringOptionPair>* foreignMap;
	mapping.getForeignUserMap(attachment, currentUser, fServer, foreignMap);

	// If there was no mapping for user and server,
	// or any properties were not specified in it, use server options.
	const auto& options = server->getOptions();

	auto getOption = [&](const char* optionName) -> string
	{
		ForeignOption option(*tdbb->getAttachment()->att_pool);
		if (foreignMap && foreignMap->get(optionName, option))
			return option.getActualValue();
		if (options.get(optionName, option))
			return option.getActualValue();
		return "";
	};

	string connectionString = getOption(FOREIGN_SERVER_CONNECTION_STRING);
	string user = getOption(FOREIGN_SERVER_USER);
	string password = getOption(FOREIGN_SERVER_PASSWORD);
	string role = getOption(FOREIGN_SERVER_ROLE);

	ClumpletWriter dpb(ClumpletReader::dpbList, MAX_DPB_SIZE);
	generateDPB(tdbb, dpb, user, password, role);

	string optionsString;
	makeOptionsString(options, optionsString);

	// Add server options to existing database parameter buffer,
	// but exclude adding options that should be added with separate isc_ tags
	if (optionsString.hasData())
		dpb.insertString(isc_dpb_foreign_options, optionsString.c_str(), optionsString.length());

	if (server->getPluginName().hasData())
	{
		PathName providers(server->getPluginName());
		providers.insert(0, "Providers=");
		dpb.insertString(isc_dpb_config, providers);
	}

	if (connectionString.isEmpty())
		connectionString = tdbb->getDatabase()->dbb_database_name.c_str();

	ForeignTableConnection* connection = static_cast<ForeignTableConnection*>(
		EDS::Manager::getProviderConnection(tdbb, this, dpb, connectionString, user, password, role,
			EDS::TraScope::traCommon));

	fb_assert(connection != NULL);

	connection->getProviderInfo(tdbb);

	return connection;
}

// Make additional server options string, excluding generic ones that have specific isc tags
void ForeignTableProvider::makeOptionsString(const GenericMap<MetaStringOptionPair>& optionsMap, string& options)
{
	for (const auto& option : optionsMap)
	{
		// Skip general options
		if (isGenericOption(option.first))
			continue;

		options.append(option.first.c_str());
		options.append(";");
		options.append(option.second.getActualValue());
		options.append(";");
	}
}

// Check if the option is generic, i.e. it has a separate isc tag
bool ForeignTableProvider::isGenericOption(const MetaName& option)
{
	return std::any_of(std::begin(FOREIGN_GENERAL_OPTIONS), std::end(FOREIGN_GENERAL_OPTIONS),
		[&](const char* value) { return strcmp(value, option.c_str()) == 0; });
}

void ForeignTableConnection::getProviderInfo(thread_db* tdbb)
{
	info_db_provider dbProvider = info_db_provider::isc_info_db_code_last_value;
	memset(m_sqlFeatures, false, sizeof(m_sqlFeatures));

	FbLocalStatus status;
	unsigned char buff[BUFFER_TINY];
	{
		EDS::EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		const unsigned char info[] = {isc_info_db_provider, isc_info_end};
		m_iscProvider.isc_database_info(&status, &m_handle, sizeof(info), info, sizeof(buff), buff);
	}
	if (status->getState() & IStatus::STATE_ERRORS)
		raise(&status, tdbb, "isc_info_db_provider");

	for (ClumpletReader p(ClumpletReader::InfoResponse, buff, sizeof(buff)); !p.isEof(); p.moveNext())
	{
		switch (p.getClumpTag())
		{
			case isc_info_db_provider:
				dbProvider = static_cast<info_db_provider>(p.getInt());
				break;
		}
	}

	if (dbProvider == isc_info_db_code_firebird)
	{
		memset(m_sqlFeatures, true, sizeof(m_sqlFeatures));
	}
	else
	{
		{
			EDS::EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

			const unsigned char info[] = {fb_info_sql_features, isc_info_end};
			m_iscProvider.isc_database_info(&status, &m_handle, sizeof(info), info, sizeof(buff), buff);
		}
		if (status->getState() & IStatus::STATE_ERRORS)
			raise(&status, tdbb, "fb_info_sql_features");

		for (ClumpletReader p(ClumpletReader::InfoResponse, buff, sizeof(buff)); !p.isEof(); p.moveNext())
		{
			const UCHAR* b = p.getBytes();
			switch (p.getClumpTag())
			{
				case fb_info_sql_features:
					for (unsigned i = 0; i < p.getClumpLength(); i++)
					{
						if (b[i] == 0)
							ERR_post(Arg::Gds(isc_random) << Arg::Str("Bad provider SQL feature value"));

						if (b[i] < fb_feature_sql_max)
							setSqlFeature(static_cast<info_sql_features>(b[i]));
					}
					break;
			}
		}
	}
}

void ForeignTableStatement::openInternal(thread_db* tdbb, EDS::IscTransaction* transaction)
{
	fb_assert(isAllocated() && m_stmt_selectable);
	fb_assert(!m_error);
	fb_assert(!m_active);

	m_transaction = transaction;

	doOpen(tdbb);

	m_active = true;
	m_fetched = false;
}

void ForeignTableStatement::executeInternal(thread_db* tdbb, EDS::IscTransaction* transaction, record_param* org_rpb,
	record_param* new_rpb, bool usePrimaryKeys, const SortedArray<int>* skippedOrgRpb,
	const SortedArray<int>* skippedNewRpb)
{
	fb_assert(isAllocated() && !m_stmt_selectable);
	fb_assert(!m_active);

	m_transaction = transaction;

	// If the foreign provider didn't return the input parameters metadata,
	// let's create it from the record descriptors
	const ForeignTableConnection* foreignConnection = static_cast<ForeignTableConnection*>(getConnection());
	const bool useEquiv = foreignConnection->testSqlFeature(fb_feature_sql_op_equiv);
	const USHORT multiplier = (usePrimaryKeys || useEquiv) ? 1 : 2;
	if (foreignConnection->testFeature(fb_feature_internal_input_types))
	{
		USHORT count = 0;

		if (new_rpb)
		{
			count = new_rpb->rpb_record->getFormat()->fmt_count;
			if (skippedNewRpb && skippedNewRpb->getCount() > 0)
				count -= skippedNewRpb->getCount();
		}
		if (org_rpb)
		{
			count += org_rpb->rpb_record->getFormat()->fmt_count;
			if (skippedOrgRpb && skippedOrgRpb->getCount() > 0)
				count -= skippedOrgRpb->getCount() * multiplier;
		}

		DescList sqldaDescs(count);

		if (new_rpb)
			addRecordDescs(sqldaDescs, new_rpb->rpb_record, skippedNewRpb);

		if (org_rpb)
			addRecordDescs(sqldaDescs, org_rpb->rpb_record, skippedOrgRpb, multiplier);

		remakeInputSQLDA(tdbb, sqldaDescs.getCount(), sqldaDescs.begin());
	}

	if (new_rpb)
	{
		// New values should be set with 0 offset
		setInParamsInternal(tdbb, new_rpb, 0, skippedNewRpb);
	}

	if (org_rpb)
	{
		USHORT offset = 0;
		if (new_rpb)
		{
			offset = new_rpb->rpb_record->getFormat()->fmt_count;
			if (skippedNewRpb && skippedNewRpb->getCount() > 0)
				offset -= skippedNewRpb->getCount();
		}
		// The old value should be set after new ones (if they exist).
		setInParamsInternal(tdbb, org_rpb, offset, skippedOrgRpb, multiplier);
	}

	doExecute(tdbb);
}

void ForeignTableStatement::addRecordDescs(DescList& outDescs, Record* record,
	const SortedArray<int>* skippedParameters, const USHORT multiplier)
{
	const Jrd::Format* format = record->getFormat();
	USHORT count = format->fmt_count;

	for (FB_SIZE_T i = 0; i < count; ++i)
	{
		if (skippedParameters && skippedParameters->exist(i))
			continue;

		for (USHORT j = 0; j < multiplier; ++j)
		{
			dsc src = format->fmt_desc[i];
			src.dsc_address += (IPTR)record->getData();

			outDescs.add(src);
		}
	}
}

void ForeignTableStatement::setInParamsInternal(thread_db* tdbb,
	record_param* rpb,
	const USHORT offset,
	const SortedArray<int>* skippedParameters,
	const USHORT multiplier)
{
	Record* const record = rpb->rpb_record;
	const Jrd::Format* format = record->getFormat();

	USHORT skipped = 0;
	for (USHORT i = 0; i < format->fmt_count; ++i)
	{
		if (skippedParameters && skippedParameters->exist(i))
		{
			++skipped;
			continue;
		}

		dsc src = format->fmt_desc[i];
		src.dsc_address += (IPTR)record->getData();
		const USHORT pos = offset + multiplier * (i - skipped);
		dsc& validDestDesc = m_inDescs[pos * 2];

		for (USHORT j = pos; j < pos + multiplier; ++j)
		{
			dsc& dst = m_inDescs[j * 2];
			dsc& null = m_inDescs[j * 2 + 1];

			const bool srcNull = record->isNull(i);
			*((SSHORT*) null.dsc_address) = (srcNull ? -1 : 0);

			if (srcNull)
			{
				memset(dst.dsc_address, 0, dst.dsc_length);
			}
			else
			{
				// Parameter can be set multiple times,
				// so it's necessary to copy properties from valid destination descriptor
				if (j > pos)
				{
					dst.dsc_dtype = validDestDesc.dsc_dtype;
					dst.dsc_length = validDestDesc.dsc_length;
					dst.dsc_flags = validDestDesc.dsc_flags;
					dst.dsc_scale = validDestDesc.dsc_scale;
					dst.dsc_sub_type = validDestDesc.dsc_sub_type;
					dst.dsc_address = validDestDesc.dsc_address;
				}

				if (dst.isBlob())
				{
					dsc srcBlob;
					srcBlob.clear();
					ISC_QUAD srcBlobID;

					if (src.isBlob())
					{
						srcBlob = src;
					}
					else
					{
						srcBlob.makeBlob(dst.getBlobSubType(), dst.getTextType(), &srcBlobID);
						MOV_move(tdbb, &src, &srcBlob);
					}

					putExtBlob(tdbb, srcBlob, dst);
				}
				else
					MOV_move(tdbb, &src, &dst);
			}
		}
	}
}

bool ForeignTableStatement::fetchInternal(thread_db* tdbb, Record* record)
{
	fb_assert(isAllocated() && m_stmt_selectable);
	fb_assert(!m_error);
	fb_assert(m_active);

	if (!doFetch(tdbb))
		return false;

	m_fetched = true;

	const Jrd::Format* format = record->getFormat();
	record->nullify();

	USHORT recordIdx = 0;
	for (unsigned int outIdx = 0; outIdx < m_outDescs.getCount() / 2; ++outIdx, ++recordIdx)
	{
		dsc toDesc = format->fmt_desc[recordIdx];
		if (toDesc.isUnknown())
		{
			--outIdx;
			continue;
		}

		toDesc.dsc_address += (IPTR)record->getData();

		dsc& fromDesc = m_outDescs[outIdx * 2];
		const dsc& null = m_outDescs[outIdx * 2 + 1];
		dsc* local = &fromDesc;
		dsc localDsc;
		bid localBlobID;

		const bool srcNull = (*(SSHORT*) null.dsc_address) == -1;
		if (fromDesc.isBlob() && !srcNull)
		{
			localDsc = fromDesc;
			localDsc.dsc_address = (UCHAR*) &localBlobID;
			getExtBlob(tdbb, fromDesc, localDsc);
			local = &localDsc;
		}

		if (!srcNull)
		{
			MOV_move(tdbb, local, &toDesc);
			record->clearNull(recordIdx);
		}
	}

	if (m_singleton)
	{
		if (doFetch(tdbb))
		{
			FbLocalStatus status;
			Arg::Gds(isc_sing_select_err).copyTo(&status);
			raise(&status, tdbb, "isc_dsql_fetch");
		}
		return false;
	}

	return true;
}

void ForeignTableAdapter::ensureConnect(thread_db* tdbb)
{
	if (!m_provider)
		m_provider = static_cast<ForeignTableProvider*>(EDS::Manager::getProvider("Firebird"));

	if (!m_connection || !m_connection->getAPIHandle())
		m_connection = static_cast<ForeignTableConnection*>(m_provider->createForeignConnection(tdbb, m_server));
}

EDS::Statement* ForeignTableAdapter::createStatement(thread_db* tdbb, record_param* org_rpb, record_param* new_rpb,
	const string& filter, const string& order)
{
	ensureConnect(tdbb);

	// Prepare index array for a new record that contains read-only field indexes
	// to exclude from the insert, update, and delete statements
	if (new_rpb)
		processRecord(m_skippedNewRpbIdx, new_rpb, true);

	// Prepare index array for the original record
	// whose fields shouldn't be used in the update or delete statements
	if (org_rpb)
		processRecord(m_skippedOrgRpbIdx, org_rpb);

	Firebird::string sql;
	getSql(sql, filter, order, org_rpb, new_rpb);

	// Create a statement and return it for later use
	ForeignTableStatement* statement = static_cast<ForeignTableStatement*>(m_connection->createStatement(sql));
	statement->setRawSql(sql);
	return statement;
}

void ForeignTableAdapter::execute(thread_db* tdbb, EDS::Statement* statement, record_param* org_rpb,
	record_param* new_rpb)
{
	EDS::IscTransaction* transaction = static_cast<EDS::IscTransaction*>(
		EDS::Transaction::getTransaction(tdbb, m_connection, EDS::TraScope::traCommon));

	ForeignTableStatement* ftStatement = (ForeignTableStatement*) statement;
	const string rawSql = ftStatement->getRawSql();

	ftStatement->prepare(tdbb, transaction, rawSql, false);

	const TimeoutTimer* timer = tdbb->getTimeoutTimer();
	if (timer)
		ftStatement->setTimeout(tdbb, timer->timeToExpire());

	if (ftStatement->isSelectable())
		ftStatement->openInternal(tdbb, transaction);
	else
	{
		const bool usePrimaryKeys = m_foreignPKNames.hasData();
		ftStatement->executeInternal(tdbb, transaction, org_rpb, new_rpb, usePrimaryKeys, &m_skippedOrgRpbIdx,
			&m_skippedNewRpbIdx);
	}
}

bool ForeignTableAdapter::fetch(thread_db* tdbb, EDS::Statement* statement, Record* record)
{
	ForeignTableStatement* ftStatement = (ForeignTableStatement*) statement;

	return ftStatement->fetchInternal(tdbb, record);
}

bool ForeignTableAdapter::testSqlFeature(thread_db* tdbb, info_sql_features value)
{
	ensureConnect(tdbb);
	return m_connection->testSqlFeature(value);
}

void ForeignTableAdapter::getSql(string& sql, const string& filter, const string& order, record_param* org_rpb,
	record_param* new_rpb) const
{
	vec<jrd_fld*>* vector = m_relation->rel_fields;
	vec<jrd_fld*>::iterator fieldIter = vector->begin();
	SSHORT id;

	const string schemaQualifiedName = getOriginalTableName();

	if (!org_rpb && !new_rpb)
	{
		sql += "select ";
		sql += getRelationFieldsSql(true);
		sql += " from ";
		sql += schemaQualifiedName;

		if (filter.hasData())
		{
			sql += " where 1 = 1 ";
			sql += filter;
		}

		if (order.hasData())
		{
			sql += " ";
			sql += order;
		}
		else if (m_foreignPKNames.hasData())
		{
			string orderPK(" order by ");
			for (const MetaName& pkName : m_foreignPKNames)
			{
				ForeignField* field = nullptr;
				m_foreignFields.get(pkName, field);
				if (field)
				{
					orderPK.append(getOriginalFieldName(field->name));
					orderPK.append(", ");
				}
			}
			orderPK.rtrim(", ");
			sql += orderPK;
		}
	}
	else if (!org_rpb && new_rpb)
	{
		sql += "insert into ";
		sql += schemaQualifiedName;
		sql += "(";
		sql += getRelationFieldsSql(false);
		sql += ") ";
		sql += "values";
		sql += "(";

		fieldIter = vector->begin();
		for (const vec<jrd_fld*>::const_iterator end = vector->end(); fieldIter < end; ++fieldIter)
		{
			if (*fieldIter)
			{
				const jrd_fld* field = *fieldIter;
				if (!isFieldReadOnlyOrComputed(field))
				{
					sql += "?, ";
				}
			}
		}

		sql.rtrim(", ");
		sql += ") ";
	}
	else if (org_rpb && new_rpb)
	{
		sql += "update ";
		sql += schemaQualifiedName;
		sql += " set ";

		for (const vec<jrd_fld*>::const_iterator end = vector->end(); fieldIter < end; ++fieldIter)
		{
			if (*fieldIter)
			{
				const jrd_fld* field = *fieldIter;
				if (!isFieldReadOnlyOrComputed(field))
				{
					sql += getOriginalFieldName(field->fld_name);
					sql += " = ?, ";
				}
			}
		}

		sql.rtrim(", ");

		sql += getWhereClauseSql();
	}
	else
	{
		fb_assert(org_rpb && !new_rpb);
		sql += "delete from ";
		sql += schemaQualifiedName;

		sql += getWhereClauseSql();
	}

	sql.trim();
}

// Get a list of fields to access the foreign table.
// In case of insertion into an external table, exclude read-only fields.
string ForeignTableAdapter::getRelationFieldsSql(bool select) const
{
	vec<jrd_fld*>* vector = m_relation->rel_fields;
	vec<jrd_fld*>::iterator fieldIter = vector->begin();

	string fields = "";

	for (const vec<jrd_fld*>::const_iterator end = vector->end(); fieldIter < end; ++fieldIter)
	{
		if (*fieldIter)
		{
			jrd_fld* field = *fieldIter;
			if (!isFieldReadOnlyOrComputed(field) || select)
			{
				fields += getOriginalFieldName(field->fld_name);
				fields += ", ";
			}
		}
	}

	fields.rtrim(", ");

	return fields;
}

const string ForeignTableAdapter::getWhereClauseSql() const
{
	vec<jrd_fld*>* vector = m_relation->rel_fields;
	vec<jrd_fld*>::iterator fieldIter = vector->begin();
	SSHORT id = 0;

	string where = " where 1 = 1 ";

	const FB_BOOLEAN usePrimaryKeys = m_foreignPKNames.hasData();

	for (const vec<jrd_fld*>::const_iterator end = vector->end();
		fieldIter < end;
		++fieldIter, ++id)
	{
		if (*fieldIter)
		{
			jrd_fld* field = *fieldIter;
			const MetaName name = field->fld_name;
			const bool isPrimaryKey = m_foreignPKNames.exist(name);
			if (!usePrimaryKeys || isPrimaryKey)
			{
				if (!m_skippedOrgRpbIdx.exist(id))
				{
					const string origName = getOriginalFieldName(field->fld_name);
					if (isPrimaryKey)
					{
						where += " and (";
						where += origName;
						where += " = ?) ";
					}
					else if(m_connection->testSqlFeature(fb_feature_sql_op_equiv))
					{
						where += " and (";
						where += origName;
						where += " is not distinct from ?) ";
					}
					else
					{
						where += " and (";
						where += origName;
						where += " = ? or (";
						where += origName;
						where += " is null and ? is null)) ";
					}
				}
			}
		}
	}

	return where;
}

void ForeignTableAdapter::addTableOption(const MetaName& name, const string& value,
	const ExternalValueType type)
{
	ForeignOption* option = m_tableOptions.getOrPut(name);
	option->m_name = name;
	option->m_value = value;
	option->m_type = type;

}

void ForeignTableAdapter::addTableField(const MetaName& fieldName, const MetaName& optionName, const string& value,
	const ExternalValueType type)
{
	ForeignField* field;
	if (!m_foreignFields.get(fieldName, field))
	{
		field = FB_NEW_POOL(*m_relation->rel_pool) ForeignField(*m_relation->rel_pool);
		field->name = fieldName;
		m_foreignFields.put(fieldName, field);
	}

	ForeignOption* option = field->options.getOrPut(optionName);
	option->m_name = optionName;
	option->m_value = value;
	option->m_type = type;

	if (optionName == FOREIGN_TABLE_COLUMN_PK && getBooleanValue(value))
		m_foreignPKNames.add(fieldName);
	else if (optionName == FOREIGN_TABLE_COLUMN_READONLY && getBooleanValue(value))
		m_readOnlyNames.add(fieldName);
}

const string ForeignTableAdapter::getOriginalTableName() const
{
	string schemaQualifiedName = "";

	if (m_tableOptions.exist(MetaName(FOREIGN_TABLE_SCHEMA_NAME)))
	{
		schemaQualifiedName += m_tableOptions.get(MetaName(FOREIGN_TABLE_SCHEMA_NAME))->m_value;
		schemaQualifiedName += ".";
	}

	if (m_tableOptions.exist(MetaName(FOREIGN_TABLE_NAME)))
		schemaQualifiedName += m_tableOptions.get(MetaName(FOREIGN_TABLE_NAME))->m_value;
	else
		schemaQualifiedName += m_relation->getName().object.toQuotedString();

	return schemaQualifiedName;
}

const string ForeignTableAdapter::getOriginalFieldName(const MetaName& name) const
{
	string origName = name.c_str();

	ForeignField* field = nullptr;
	m_foreignFields.get(name, field);
	if (field)
	{
		GenericMap<MetaStringOptionPair>::Accessor accessor(&field->options);
		for (bool found = accessor.getFirst(); found; found = accessor.getNext())
		{
			if (accessor.current()->second.m_name == FOREIGN_TABLE_COLUMN_NAME)
		 		origName = accessor.current()->second.m_value;
		}
	}

	return origName;
}

// In the case of a new record, read-only field indexes are saved
// to exclude them from data change statements.
// In the case of an original record, if at least one primary key is specified
// in the field parameters, the condition will use the primary key(s).
// If no primary keys are specified, the condition will contain all fields except blobs.
void ForeignTableAdapter::processRecord(SortedArray<int>& outSkippedRpbIdx, record_param* rpb, bool isNewRecord)
{
	vec<jrd_fld*>* vector = m_relation->rel_fields;
	vec<jrd_fld*>::iterator fieldIter = vector->begin();
	SSHORT id = 0;
	const FB_BOOLEAN usePrimaryKeys = m_foreignPKNames.hasData();

	for (const vec<jrd_fld*>::const_iterator end = vector->end();
		fieldIter < end;
		++fieldIter, ++id)
	{
		if (outSkippedRpbIdx.exist(id))
			continue;

		if (*fieldIter)
		{
			jrd_fld* field = *fieldIter;
			if (isNewRecord)
			{
				if (isFieldReadOnlyOrComputed(field))
					outSkippedRpbIdx.add(id);
			}
			else
			{
				if (!usePrimaryKeys || m_foreignPKNames.exist(field->fld_name))
				{
					const Jrd::Format* format = nullptr;
					if (rpb)
						format = rpb->rpb_record->getFormat();

					if ((format && format->fmt_desc[id].isBlob()))
						outSkippedRpbIdx.add(id);
				}
				else
					outSkippedRpbIdx.add(id);
			}
		}
	}
}

bool ForeignTableAdapter::isFieldReadOnlyOrComputed(const jrd_fld* field) const
{
	return m_readOnlyNames.exist(field->fld_name) || field->fld_computation;
}
