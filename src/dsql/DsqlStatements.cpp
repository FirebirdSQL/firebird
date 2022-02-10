/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2022.02.07 Adriano dos Santos Fernandes: Refactored from dsql.cpp
 */

#include "firebird.h"
#include "../dsql/DsqlStatements.h"
#include "../dsql/dsql.h"
#include "../dsql/Nodes.h"
#include "../dsql/DsqlCompilerScratch.h"
#include "../jrd/Statement.h"
#include "../dsql/errd_proto.h"
#include "../dsql/gen_proto.h"
#include "../jrd/cmp_proto.h"

using namespace Firebird;
using namespace Jrd;


// Class DsqlStatement

// Rethrow an exception with isc_no_meta_update and prefix codes.
void DsqlStatement::rethrowDdlException(status_exception& ex, bool metadataUpdate, DdlNode* node)
{
	Arg::StatusVector newVector;

	if (metadataUpdate)
		newVector << Arg::Gds(isc_no_meta_update);

	node->putErrorPrefix(newVector);

	const ISC_STATUS* status = ex.value();

	if (status[1] == isc_no_meta_update)
		status += 2;

	newVector.append(Arg::StatusVector(status));

	status_exception::raise(newVector);
}

int DsqlStatement::release()
{
	fb_assert(refCounter.value() > 0);
	const int refCnt = --refCounter;

	if (!refCnt)
	{
		doRelease();
		dsqlAttachment->deletePool(&getPool());
	}

	return refCnt;
}

void DsqlStatement::doRelease()
{
	setSqlText(nullptr);
	setOrgText(nullptr, 0);
}

void DsqlStatement::setOrgText(const char* ptr, ULONG len)
{
	if (!ptr || !len)
	{
		orgText = NULL;
		return;
	}

	const string text(ptr, len);

	if (text == *sqlText)
		orgText = sqlText;
	else
		orgText = FB_NEW_POOL(getPool()) RefString(getPool(), text);
}


// DsqlDmlStatement

void DsqlDmlStatement::doRelease()
{
	if (auto parent = getParentRequest())
	{
		FB_SIZE_T pos;
		if (parent->cursors.find(this, pos))
			parent->cursors.remove(pos);
	}

	if (statement)
	{
		thread_db* tdbb = JRD_get_thread_data();
		ThreadStatusGuard status_vector(tdbb);

		try
		{
			statement->release(tdbb);
		}
		catch (Exception&)
		{} // no-op
	}

	DsqlStatement::doRelease();
}

void DsqlDmlStatement::dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch, ntrace_result_t* traceResult)
{
	{	// scope
		ContextPoolHolder scratchContext(tdbb, &scratch->getPool());
		node = Node::doDsqlPass(scratch, node);
	}

	if (scratch->clientDialect > SQL_DIALECT_V5)
		scratch->getDsqlStatement()->setBlrVersion(5);
	else
		scratch->getDsqlStatement()->setBlrVersion(4);

	GEN_statement(scratch, node);

	unsigned messageNumber = 0;

	for (auto message : ports)
		message->msg_buffer_number = messageNumber++;

	// have the access method compile the statement

#ifdef DSQL_DEBUG
	if (DSQL_debug & 64)
	{
		dsql_trace("Resulting BLR code for DSQL:");
		gds__trace_raw("Statement:\n");
		gds__trace_raw(getSqlText()->c_str(), getSqlText()->length());
		gds__trace_raw("\nBLR:\n");
		fb_print_blr(scratch->getBlrData().begin(),
			(ULONG) scratch->getBlrData().getCount(),
			gds__trace_printer, 0, 0);
	}
#endif

	FbLocalStatus localStatus;

	// check for warnings
	if (tdbb->tdbb_status_vector->getState() & IStatus::STATE_WARNINGS)
	{
		// save a status vector
		fb_utils::copyStatus(&localStatus, tdbb->tdbb_status_vector);
		fb_utils::init_status(tdbb->tdbb_status_vector);
	}

	ISC_STATUS status = FB_SUCCESS;

	try
	{
		const auto attachment = scratch->getAttachment()->dbb_attachment;
		const auto& blr = scratch->getBlrData();
		const auto& debugData = scratch->getDebugData();

		statement = CMP_compile(tdbb, blr.begin(), blr.getCount(),
			(scratch->flags & DsqlCompilerScratch::FLAG_INTERNAL_REQUEST),
			debugData.getCount(), debugData.begin());

		if (getSqlText())
			statement->sqlText = getSqlText();

		fb_assert(statement->blr.isEmpty());

		if (attachment->getDebugOptions().getDsqlKeepBlr())
			statement->blr.insert(0, blr.begin(), blr.getCount());
	}
	catch (const Exception&)
	{
		status = tdbb->tdbb_status_vector->getErrors()[1];
		*traceResult = status == isc_no_priv ?
			ITracePlugin::RESULT_UNAUTHORIZED : ITracePlugin::RESULT_FAILED;
	}

	// restore warnings (if there are any)
	if (localStatus->getState() & IStatus::STATE_WARNINGS)
	{
		Arg::StatusVector cur(tdbb->tdbb_status_vector->getWarnings());
		Arg::StatusVector saved(localStatus->getWarnings());
		saved << cur;

		tdbb->tdbb_status_vector->setWarnings2(saved.length(), saved.value());
	}

	// free blr memory
	scratch->getBlrData().free();

	if (status)
		status_exception::raise(tdbb->tdbb_status_vector);

	node = NULL;
}

DsqlDmlRequest* DsqlDmlStatement::createRequest(thread_db* tdbb, dsql_dbb* dbb)
{
	return FB_NEW_POOL(getPool()) DsqlDmlRequest(tdbb, getPool(), dbb, this);
}


// DsqlDdlStatement

DsqlDdlStatement::~DsqlDdlStatement()
{
	dsqlAttachment->deletePool(&scratch->getPool());
}

bool DsqlDdlStatement::mustBeReplicated() const
{
	return node->mustBeReplicated();
}

void DsqlDdlStatement::dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch, ntrace_result_t* traceResult)
{
	Database* const dbb = tdbb->getDatabase();

	scratch->flags |= DsqlCompilerScratch::FLAG_DDL;

	try
	{
		node = Node::doDsqlPass(scratch, node);
	}
	catch (status_exception& ex)
	{
		rethrowDdlException(ex, false, node);
	}

	if (dbb->readOnly())
		ERRD_post(Arg::Gds(isc_read_only_database));

	// In read-only replica, only replicator is allowed to execute DDL.
	// As an exception, not replicated DDL statements are also allowed.
	if (dbb->isReplica(REPLICA_READ_ONLY) &&
		!(tdbb->tdbb_flags & TDBB_replicator) &&
		node->mustBeReplicated())
	{
		ERRD_post(Arg::Gds(isc_read_only_trans));
	}

	const auto dbDialect = (dbb->dbb_flags & DBB_DB_SQL_dialect_3) ? SQL_DIALECT_V6 : SQL_DIALECT_V5;

	if ((scratch->flags & DsqlCompilerScratch::FLAG_AMBIGUOUS_STMT) &&
		dbDialect != scratch->clientDialect)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-817) <<
				  Arg::Gds(isc_ddl_not_allowed_by_db_sql_dial) << Arg::Num(dbDialect));
	}

	if (scratch->clientDialect > SQL_DIALECT_V5)
		scratch->getDsqlStatement()->setBlrVersion(5);
	else
		scratch->getDsqlStatement()->setBlrVersion(4);

	this->scratch = scratch;
}

DsqlDdlRequest* DsqlDdlStatement::createRequest(thread_db* tdbb, dsql_dbb* dbb)
{
	return FB_NEW_POOL(getPool()) DsqlDdlRequest(getPool(), dbb, scratch, node);
}


// DsqlTransactionStatement

DsqlTransactionStatement::~DsqlTransactionStatement()
{
	dsqlAttachment->deletePool(&scratch->getPool());
}

void DsqlTransactionStatement::dsqlPass(thread_db* tdbb, DsqlCompilerScratch* scratch,
	ntrace_result_t* /*traceResult*/)
{
	node = Node::doDsqlPass(scratch, node);

	this->scratch = scratch;
}

DsqlTransactionRequest* DsqlTransactionStatement::createRequest(thread_db* tdbb, dsql_dbb* dbb)
{
	return FB_NEW_POOL(getPool()) DsqlTransactionRequest(getPool(), dbb, this, node);
}
