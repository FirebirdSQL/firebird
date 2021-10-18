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
 *  The Original Code was created by Vlad Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2007 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "fb_types.h"
#include "../../include/fb_blk.h"
#include "fb_exception.h"
#include "iberror.h"

#include "../align.h"
#include "../common/dsc.h"
#include "../exe.h"
#include "IscDS.h"
#include "ValidatePassword.h"
#include "../tra.h"

#include "../blb_proto.h"
#include "../evl_proto.h"
#include "../exe_proto.h"
#include "../intl_proto.h"
#include "../mov_proto.h"
#include "../common/utils_proto.h"


using namespace Jrd;
using namespace Firebird;

namespace EDS
{

const char* FIREBIRD_PROVIDER_NAME = "Firebird";

class RegisterFBProvider
{
public:
	RegisterFBProvider()
	{
		Provider* provider = FB_NEW FBProvider(FIREBIRD_PROVIDER_NAME);
		Manager::addProvider(provider);
	}
};

static RegisterFBProvider reg;

static bool isConnectionBrokenError(FbStatusVector* status);
static void parseSQLDA(XSQLDA* xsqlda, UCharBuffer& buff, Firebird::Array<dsc>& descs);

// 	IscProvider

void IscProvider::getRemoteError(const FbStatusVector* status, string& err) const
{
	err = "";

	char buff[1024];
	const ISC_STATUS* p = status->getErrors();
	const ISC_STATUS* const end = p + fb_utils::statusLength(p);

	while (p < end - 1)
	{
		const ISC_STATUS code = *p ? p[1] : 0;
		if (!m_api.interpret(buff, sizeof(buff), &p))
			break;

		string rem_err;
		rem_err.printf("%lu : %s\n", code, buff);
		err += rem_err;
	}
}

Connection* IscProvider::doCreateConnection()
{
	return FB_NEW IscConnection(*this);
}

// IscConnection

IscConnection::IscConnection(IscProvider& prov) :
	Connection(prov),
	m_iscProvider(prov),
	m_handle(0)
{
}

IscConnection::~IscConnection()
{
}

void IscConnection::attach(thread_db* tdbb)
{
	Attachment* attachment = tdbb->getAttachment();

	// Avoid change of m_dpb by validatePassword() below
	ClumpletWriter newDpb(ClumpletReader::dpbList, MAX_DPB_SIZE, m_dpb.begin(), m_dpb.getCount());
	validatePassword(tdbb, m_dbName, newDpb);
	newDpb.insertInt(isc_dpb_ext_call_depth, attachment->att_ext_call_depth + 1);

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		ICryptKeyCallback* cb = tdbb->getAttachment()->att_crypt_callback;
		try
		{
			m_iscProvider.fb_database_crypt_callback(&status, cb);
			if (status->getState() & IStatus::STATE_ERRORS) {
				raise(&status, tdbb, "crypt_callback");
			}

			m_iscProvider.isc_attach_database(&status, m_dbName.length(), m_dbName.c_str(),
				&m_handle, newDpb.getBufferLength(),
				reinterpret_cast<const char*>(newDpb.getBuffer()));
			if (status->getState() & IStatus::STATE_ERRORS) {
				raise(&status, tdbb, "attach");
			}
		}
		catch (const Exception&)
		{
			m_iscProvider.fb_database_crypt_callback(&status, NULL);
			throw;
		}

		m_iscProvider.fb_database_crypt_callback(&status, NULL);
		if (status->getState() & IStatus::STATE_ERRORS) {
			raise(&status, tdbb, "crypt_callback");
		}
	}

	unsigned char buff[BUFFER_TINY];
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		const unsigned char info[] = {isc_info_db_sql_dialect, fb_info_features, isc_info_end};
		m_iscProvider.isc_database_info(&status, &m_handle, sizeof(info), info, sizeof(buff), buff);
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		raise(&status, tdbb, "isc_database_info");
	}

	memset(m_features, false, sizeof(m_features));
	m_sqlDialect = 1;

	const unsigned char* p = buff, *end = buff + sizeof(buff);
	while (p < end)
	{
		const UCHAR item = *p++;
		const USHORT len = m_iscProvider.isc_vax_integer(p, sizeof(USHORT));
		p += sizeof(USHORT);

		switch (item)
		{
			case isc_info_db_sql_dialect:
				m_sqlDialect = m_iscProvider.isc_vax_integer(p, len);
				break;

			case fb_info_features:
				for (int i = 0; i < len; i++)
				{
					if (p[i] == 0)
						ERR_post(Arg::Gds(isc_random) << Arg::Str("Bad provider feature value"));

					if (p[i] < fb_feature_max)
						setFeature(static_cast<info_features>(p[i]));
					// else this provider supports unknown feature, ignore it.
				}
				break;

			case isc_info_error:
				{
					const ULONG err = m_iscProvider.isc_vax_integer(p + 1, len - 1);
					if (err == isc_infunk)
					{
						if (*p == fb_info_features)
						{
							// Used provider follow Firebird error reporting conventions but is not aware of
							// this info item. Assume Firebird 3 or earlier.
							m_features[fb_feature_multi_statements] = true;
							m_features[fb_feature_multi_transactions] = true;
							m_features[fb_feature_statement_long_life] = true;
						}
						break;
					}
					ERR_post(Arg::Gds(isc_random) << Arg::Str("Unexpected error in isc_database_info"));
				}

			case isc_info_truncated:
				ERR_post(Arg::Gds(isc_random) << Arg::Str("Result truncation in isc_database_info"));

			case isc_info_end:
				p = end;
				break;
		}
		p += len;
	}

}

void IscConnection::doDetach(thread_db* tdbb)
{
	FbLocalStatus status;
	if (m_handle)
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		FB_API_HANDLE h = m_handle;
		m_handle = 0;
		m_iscProvider.isc_detach_database(&status, &h);
		m_handle = h;
	}

	if ((status->getState() & IStatus::STATE_ERRORS) &&
		!isConnectionBrokenError(&status))
	{
		raise(&status, tdbb, "detach");
	}
}

bool IscConnection::cancelExecution(bool forced)
{
	FbLocalStatus status;

	if (m_handle)
	{
		m_iscProvider.fb_cancel_operation(&status, &m_handle,
			forced ? fb_cancel_abort : fb_cancel_raise);

		if (!forced && m_handle &&
			(status->getState() & IStatus::STATE_ERRORS) &&
			(status->getErrors()[1] != isc_bad_db_handle))
		{
			status->init();
			m_iscProvider.fb_cancel_operation(&status, &m_handle, fb_cancel_abort);
		}
	}
	return !(status->getState() & IStatus::STATE_ERRORS);
}

bool IscConnection::resetSession(thread_db* tdbb)
{
	if (!m_handle)
		return false;

	if (!testFeature(fb_feature_session_reset))
		return true;

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		m_iscProvider.isc_dsql_execute_immediate(&status, &m_handle,
			NULL, 0, "ALTER SESSION RESET", m_sqlDialect, NULL);
	}

	if (!(status->getState() & IStatus::STATE_ERRORS))
		return true;

	if (status->getErrors()[1] == isc_dsql_error)
	{
		clearFeature(fb_feature_session_reset);
		return true;
	}

	return false;
}

// this ISC connection instance is available for the current execution context if it
// a) has no active statements or supports many active statements
//    and
// b) has no active transactions or has active transaction of given
//	  TraScope bound to current jrd transaction or supports many active
//    transactions
bool IscConnection::isAvailable(thread_db* tdbb, TraScope traScope) const
{
	if (m_used_stmts && !testFeature(fb_feature_multi_statements))
		return false;

	if (m_transactions.getCount() && !testFeature(fb_feature_multi_transactions) && !findTransaction(tdbb, traScope))
	{
		return false;
	}

	return true;
}

bool IscConnection::validate(Jrd::thread_db* tdbb)
{
	if (!m_handle)
		return false;

	FbLocalStatus status;

	EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

	const unsigned char info[] = {isc_info_attachment_id, isc_info_end};
	unsigned char buff[32];

	return m_iscProvider.isc_database_info(&status, &m_handle,
		sizeof(info), info, sizeof(buff), buff) == 0;
}

Blob* IscConnection::createBlob()
{
	return FB_NEW IscBlob(*this);
}

Transaction* IscConnection::doCreateTransaction()
{
	return FB_NEW IscTransaction(*this);
}

Statement* IscConnection::doCreateStatement()
{
	return FB_NEW IscStatement(*this);
}

// IscTransaction

void IscTransaction::generateTPB(thread_db* tdbb, ClumpletWriter& tpb,
	TraModes traMode, bool readOnly, bool wait, int lockTimeout) const
{
	if (traMode == traReadCommitedReadConsistency && !m_connection.testFeature(fb_feature_read_consistency))
		traMode = traConcurrency;

	Transaction::generateTPB(tdbb, tpb, traMode, readOnly, wait, lockTimeout);
}

void IscTransaction::doStart(FbStatusVector* status, thread_db* tdbb, Firebird::ClumpletWriter& tpb)
{
	fb_assert(!m_handle);
	FB_API_HANDLE& db_handle = m_iscConnection.getAPIHandle();

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		m_iscProvider.isc_start_transaction(status, &m_handle, 1, &db_handle,
			tpb.getBufferLength(), tpb.getBuffer());
	}

	if ((status->getState() & IStatus::STATE_ERRORS) &&
		(status->getErrors()[1] == isc_bad_tpb_form) &&
		tpb.find(isc_tpb_read_consistency) &&
		m_connection.testFeature(fb_feature_read_consistency))
	{
		tpb.deleteWithTag(isc_tpb_read_committed);
		tpb.deleteWithTag(isc_tpb_read_consistency);
		tpb.insertTag(isc_tpb_concurrency);

		{
			EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
			m_iscProvider.isc_start_transaction(status, &m_handle, 1, &db_handle,
				tpb.getBufferLength(), tpb.getBuffer());
		}

		if (!(status->getState() & IStatus::STATE_ERRORS))
			m_connection.clearFeature(fb_feature_read_consistency);
	}
}

void IscTransaction::doPrepare(FbStatusVector* /*status*/, thread_db* /*tdbb*/, int /*info_len*/, const char* /*info*/)
{
}

void IscTransaction::doCommit(FbStatusVector* status, thread_db* tdbb, bool retain)
{
	EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
	if (retain)
		m_iscProvider.isc_commit_retaining(status, &m_handle);
	else
		m_iscProvider.isc_commit_transaction(status, &m_handle);

	fb_assert(retain && m_handle || !retain && !m_handle ||
		(status->getState() & IStatus::STATE_ERRORS) && m_handle);
}

void IscTransaction::doRollback(FbStatusVector* status, thread_db* tdbb, bool retain)
{
	EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
	if (retain)
		m_iscProvider.isc_rollback_retaining(status, &m_handle);
	else
		m_iscProvider.isc_rollback_transaction(status, &m_handle);

	if ((status->getState() & IStatus::STATE_ERRORS) &&
		isConnectionBrokenError(status) && !retain)
	{
		m_handle = 0;
		fb_utils::init_status(status);
	}

	fb_assert(retain && m_handle || !retain && !m_handle ||
		(status->getState() & IStatus::STATE_ERRORS) && m_handle);
}


// IscStatement

IscStatement::IscStatement(IscConnection& conn) :
	Statement(conn),
	m_iscProvider(*(IscProvider*) conn.getProvider()),
	m_iscConnection(conn),
	m_handle(0),
	m_in_xsqlda(NULL),
	m_out_xsqlda(NULL)
{
}

IscStatement::~IscStatement()
{
	delete[] (char*) m_in_xsqlda;
	delete[] (char*) m_out_xsqlda;
}

void IscStatement::doPrepare(thread_db* tdbb, const string& sql)
{
	FB_API_HANDLE& h_conn = m_iscConnection.getAPIHandle();
	FB_API_HANDLE& h_tran = getIscTransaction()->getAPIHandle();

	FbLocalStatus status;

	// prepare and get output parameters
	if (!m_out_xsqlda)
	{
		m_out_xsqlda = (XSQLDA*) FB_NEW_POOL (getPool()) char [XSQLDA_LENGTH(1)];
		m_out_xsqlda->sqln = 1;
		m_out_xsqlda->version = 1;
	}

	const char* sWhereError = NULL;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		if (!m_handle)
		{
			fb_assert(!m_allocated);
			if (m_iscProvider.isc_dsql_allocate_statement(&status, &h_conn, &m_handle)) {
				sWhereError = "isc_dsql_allocate_statement";
			}
			m_allocated = (m_handle != 0);
		}

		if (!sWhereError) {
			if (m_iscProvider.isc_dsql_prepare(&status, &h_tran, &m_handle, sql.length(),
				sql.c_str(), m_connection.getSqlDialect(), m_out_xsqlda))
			{
				sWhereError = "isc_dsql_prepare";
			}
		}
	}
	if (sWhereError) {
		raise(&status, tdbb, sWhereError, &sql);
	}

	// adjust output parameters
	if (m_out_xsqlda->sqld > m_out_xsqlda->sqln)
	{
		const int n = m_out_xsqlda->sqld;
		delete[] (char*) m_out_xsqlda;

		m_out_xsqlda = (XSQLDA*) FB_NEW_POOL (getPool()) char [XSQLDA_LENGTH(n)];
		m_out_xsqlda->sqln = n;
		m_out_xsqlda->version = 1;

		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		if (m_iscProvider.isc_dsql_describe(&status, &m_handle, 1, m_out_xsqlda))
		{
			sWhereError = "isc_dsql_describe";
		}
	}
	if (sWhereError) {
		raise(&status, tdbb, sWhereError, &sql);
	}

	for (int i = 0; i != m_out_xsqlda->sqld; ++i)
	{
		if (m_out_xsqlda->sqlvar[i].sqltype == SQL_TEXT)
			m_out_xsqlda->sqlvar[i].sqltype = SQL_VARYING;
	}

	parseSQLDA(m_out_xsqlda, m_out_buffer, m_outDescs);
	m_outputs = m_out_xsqlda ? m_out_xsqlda->sqld : 0;

	// get input parameters
	if (!m_in_xsqlda)
	{
		m_in_xsqlda = (XSQLDA*) FB_NEW_POOL (getPool()) char [XSQLDA_LENGTH(1)];
		m_in_xsqlda->sqln = 1;
		m_in_xsqlda->version = 1;
	}

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		if (m_iscProvider.isc_dsql_describe_bind(&status, &m_handle, 1, m_in_xsqlda))
		{
			sWhereError = "isc_dsql_describe_bind";
		}
	}
	if (sWhereError) {
		raise(&status, tdbb, sWhereError, &sql);
	}

	// adjust input parameters
	if (m_in_xsqlda->sqld > m_in_xsqlda->sqln)
	{
		const int n = m_in_xsqlda->sqld;
		delete[] (char*) m_in_xsqlda;

		m_in_xsqlda = (XSQLDA*) FB_NEW_POOL (getPool()) char [XSQLDA_LENGTH(n)];
		m_in_xsqlda->sqln = n;
		m_in_xsqlda->version = 1;

		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		if (m_iscProvider.isc_dsql_describe_bind(&status, &m_handle, 1, m_in_xsqlda))
		{
			sWhereError = "isc_dsql_describe_bind";
		}
	}
	if (sWhereError) {
		raise(&status, tdbb, sWhereError, &sql);
	}

	parseSQLDA(m_in_xsqlda, m_in_buffer, m_inDescs);
	m_inputs = m_in_xsqlda ? m_in_xsqlda->sqld : 0;

	// get statement type
	const unsigned char stmt_info[] = {isc_info_sql_stmt_type};
	unsigned char info_buff[16];
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		if (m_iscProvider.isc_dsql_sql_info(&status, &m_handle, sizeof(stmt_info), stmt_info,
			sizeof(info_buff), info_buff))
		{
			sWhereError = "isc_dsql_sql_info";
		}
	}
	if (sWhereError) {
		raise(&status, tdbb, sWhereError, &sql);
	}

	if (info_buff[0] != stmt_info[0])
	{
		ERR_build_status(&status, Arg::Gds(isc_random) << "Unknown statement type");

		sWhereError = "isc_dsql_sql_info";
		raise(&status, tdbb, sWhereError, &sql);
	}

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		const int len = m_iscProvider.isc_vax_integer(&info_buff[1], 2);
		const int stmt_type = m_iscProvider.isc_vax_integer(&info_buff[3], len);

		m_stmt_selectable = (stmt_type == isc_info_sql_stmt_select ||
			stmt_type == isc_info_sql_stmt_select_for_upd);

		if (stmt_type == isc_info_sql_stmt_start_trans ||
			stmt_type == isc_info_sql_stmt_commit ||
			stmt_type == isc_info_sql_stmt_rollback)
		{
			ERR_build_status(&status, Arg::Gds(isc_eds_expl_tran_ctrl));

			sWhereError = "isc_dsql_prepare";
			raise(&status, tdbb, sWhereError, &sql);
		}
	}
}

void IscStatement::doSetTimeout(thread_db* tdbb, unsigned int timeout)
{
	if (!m_connection.testFeature(fb_feature_statement_timeout))
		return;

	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		m_iscProvider.fb_dsql_set_timeout(&status, &m_handle, timeout);
	}

	if (status->getState() & IStatus::STATE_ERRORS)
	{
		// silently ignore error if timeouts is not supported by remote server
		// or loaded client library
		if (status[0] == isc_arg_gds && (status[1] == isc_wish_list || status[1] == isc_unavailable))
		{
			m_connection.clearFeature(fb_feature_statement_timeout);
			return;
		}

		raise(&status, tdbb, "fb_dsql_set_timeout");
	}
}

void IscStatement::doExecute(thread_db* tdbb)
{
	FB_API_HANDLE& h_tran = getIscTransaction()->getAPIHandle();

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		m_iscProvider.isc_dsql_execute2(&status, &h_tran, &m_handle, 1, m_in_xsqlda, m_out_xsqlda);
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		raise(&status, tdbb, "isc_dsql_execute2");
	}
}

void IscStatement::doOpen(thread_db* tdbb)
{
	FB_API_HANDLE& h_tran = getIscTransaction()->getAPIHandle();
	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		m_iscProvider.isc_dsql_execute(&status, &h_tran, &m_handle, 1, m_in_xsqlda);
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		raise(&status, tdbb, "isc_dsql_execute");
	}
}

bool IscStatement::doFetch(thread_db* tdbb)
{
	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		const ISC_STATUS res = m_iscProvider.isc_dsql_fetch(&status, &m_handle, 1, m_out_xsqlda);
		if (res == 100) {
			return false;
		}
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		raise(&status, tdbb, "isc_dsql_fetch");
	}
	return true;
}

void IscStatement::doClose(thread_db* tdbb, bool drop)
{
	fb_assert(m_handle);
	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		m_iscProvider.isc_dsql_free_statement(&status, &m_handle, drop ? DSQL_drop : DSQL_close);
		m_allocated = (m_handle != 0);
	}
	if (status->getState() & IStatus::STATE_ERRORS)
	{
		// we can do nothing else with this statement after this point
		m_allocated = m_handle = 0;
		raise(&status, tdbb, "isc_dsql_free_statement");
	}
}

void IscStatement::doSetInParams(thread_db* tdbb, unsigned int count, const MetaString* const* names,
	const NestConst<Jrd::ValueExprNode>* params)
{
	Statement::doSetInParams(tdbb, count, names, params);

	if (names)
	{
		XSQLVAR* xVar = m_in_xsqlda->sqlvar;
		for (unsigned int i = 0; i < count; i++, xVar++)
		{
			const int max_len = sizeof(xVar->sqlname);
			const int len = MIN(names[i]->length(), max_len - 1);
			xVar->sqlname_length = len;
			strncpy(xVar->sqlname, names[i]->c_str(), len);
			xVar->sqlname[max_len-1] = 0;
		}
	}
}

//  IscBlob

IscBlob::IscBlob(IscConnection& conn) :
	Blob(conn),
	m_iscProvider(*(IscProvider*) conn.getProvider()),
	m_iscConnection(conn),
	m_handle(0)
{
	memset(&m_blob_id, 0, sizeof(m_blob_id));
}

IscBlob::~IscBlob()
{
	fb_assert(!m_handle);
}

void IscBlob::open(thread_db* tdbb, Transaction& tran, const dsc& desc, const UCharBuffer* bpb)
{
	fb_assert(!m_handle);
	fb_assert(sizeof(m_blob_id) == desc.dsc_length);

	FB_API_HANDLE& h_db = m_iscConnection.getAPIHandle();
	FB_API_HANDLE& h_tran = ((IscTransaction&) tran).getAPIHandle();

	memcpy(&m_blob_id, desc.dsc_address, sizeof(m_blob_id));

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, m_iscConnection, FB_FUNCTION);

		ISC_USHORT bpb_len = bpb ? bpb->getCount() : 0;
		const ISC_UCHAR* bpb_buff = bpb ? bpb->begin() : NULL;

		m_iscProvider.isc_open_blob2(&status, &h_db, &h_tran, &m_handle, &m_blob_id,
			bpb_len, bpb_buff);
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		m_iscConnection.raise(&status, tdbb, "isc_open_blob2");
	}
	fb_assert(m_handle);
}

void IscBlob::create(thread_db* tdbb, Transaction& tran, dsc& desc, const UCharBuffer* bpb)
{
	fb_assert(!m_handle);
	fb_assert(sizeof(m_blob_id) == desc.dsc_length);

	FB_API_HANDLE& h_db = m_iscConnection.getAPIHandle();
	FB_API_HANDLE& h_tran = ((IscTransaction&) tran).getAPIHandle();

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, m_iscConnection, FB_FUNCTION);

		ISC_USHORT bpb_len = bpb ? bpb->getCount() : 0;
		const char* bpb_buff = bpb ? reinterpret_cast<const char*>(bpb->begin()) : NULL;

		m_iscProvider.isc_create_blob2(&status, &h_db, &h_tran, &m_handle, &m_blob_id,
			bpb_len, bpb_buff);
		memcpy(desc.dsc_address, &m_blob_id, sizeof(m_blob_id));
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		m_iscConnection.raise(&status, tdbb, "isc_create_blob2");
	}
	fb_assert(m_handle);
}

USHORT IscBlob::read(thread_db* tdbb, UCHAR* buff, USHORT len)
{
	fb_assert(m_handle);

	USHORT result = 0;
	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, m_iscConnection, FB_FUNCTION);
		m_iscProvider.isc_get_segment(&status, &m_handle, &result, len, reinterpret_cast<SCHAR*>(buff));
	}
	switch (status->getErrors()[1])
	{
	case isc_segstr_eof:
		fb_assert(result == 0);
		break;
	case isc_segment:
	case 0:
		break;
	default:
		m_iscConnection.raise(&status, tdbb, "isc_get_segment");
	}

	return result;
}

void IscBlob::write(thread_db* tdbb, const UCHAR* buff, USHORT len)
{
	fb_assert(m_handle);

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, m_iscConnection, FB_FUNCTION);
		m_iscProvider.isc_put_segment(&status, &m_handle, len, reinterpret_cast<const SCHAR*>(buff));
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		m_iscConnection.raise(&status, tdbb, "isc_put_segment");
	}
}

void IscBlob::close(thread_db* tdbb)
{
	fb_assert(m_handle);
	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, m_iscConnection, FB_FUNCTION);
		m_iscProvider.isc_close_blob(&status, &m_handle);
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		m_iscConnection.raise(&status, tdbb, "isc_close_blob");
	}
	fb_assert(!m_handle);
}


void IscBlob::cancel(thread_db* tdbb)
{
	if (!m_handle)
		return;

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, m_iscConnection, FB_FUNCTION);
		m_iscProvider.isc_cancel_blob(&status, &m_handle);
	}
	if (status->getState() & IStatus::STATE_ERRORS) {
		m_iscConnection.raise(&status, tdbb, "isc_close_blob");
	}
	fb_assert(!m_handle);
}


// IscProvider
// isc api

class IscStatus
{
public:
	explicit IscStatus(FbStatusVector* pStatus)
		: iStatus(pStatus)
	{
		fb_utils::init_status(aStatus);
	}

	~IscStatus()
	{
		Arg::StatusVector tmp(aStatus);
		tmp.copyTo(iStatus);
	}

	operator ISC_STATUS*()
	{
		return aStatus;
	}

private:
	FbStatusVector* iStatus;
	ISC_STATUS_ARRAY aStatus;
};


ISC_STATUS IscProvider::notImplemented(FbStatusVector* status) const
{
	Arg::Gds(isc_unavailable).copyTo(status);

	return status->getErrors()[1];
}

ISC_STATUS ISC_EXPORT IscProvider::isc_attach_database(FbStatusVector* user_status,
	short file_length, const char* file_name, isc_db_handle* public_handle,
	short dpb_length, const char* dpb)
{
	if (!m_api.attach_database)
		return notImplemented(user_status);

	return (*m_api.attach_database) (IscStatus(user_status), file_length, file_name,
			public_handle, dpb_length, dpb);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_array_gen_sdl(FbStatusVector* user_status,
	const ISC_ARRAY_DESC*, short*, char*, short*)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_array_get_slice(FbStatusVector* user_status,
									  isc_db_handle *,
									  isc_tr_handle *,
									  ISC_QUAD *,
									  const ISC_ARRAY_DESC*,
									  void *,
									  ISC_LONG *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_array_lookup_bounds(FbStatusVector* user_status,
										  isc_db_handle *,
										  isc_tr_handle *,
										  const char*,
										  const char*,
										  ISC_ARRAY_DESC *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_array_lookup_desc(FbStatusVector* user_status,
										isc_db_handle *,
										isc_tr_handle *,
										const char*,
										const char*,
										ISC_ARRAY_DESC *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_array_set_desc(FbStatusVector* user_status,
									 const char*,
									 const char*,
									 const short*,
									 const short*,
									 const short*,
									 ISC_ARRAY_DESC *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_array_put_slice(FbStatusVector* user_status,
									  isc_db_handle *,
									  isc_tr_handle *,
									  ISC_QUAD *,
									  const ISC_ARRAY_DESC*,
									  void *,
									  ISC_LONG *)
{
	return notImplemented(user_status);
}

void ISC_EXPORT IscProvider::isc_blob_default_desc(ISC_BLOB_DESC *,
								  const unsigned char*,
								  const unsigned char*)
{
	return;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_blob_gen_bpb(FbStatusVector* user_status,
								   const ISC_BLOB_DESC*,
								   const ISC_BLOB_DESC*,
								   unsigned short,
								   unsigned char *,
								   unsigned short *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_blob_info(FbStatusVector* user_status,
								isc_blob_handle* blob_handle,
								short item_length,
								const char* items,
								short buffer_length,
								char* buffer)
{
	if (!m_api.blob_info)
		return notImplemented(user_status);

	return (*m_api.blob_info) (IscStatus(user_status), blob_handle,
			item_length, items, buffer_length, buffer);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_blob_lookup_desc(FbStatusVector* user_status,
									   isc_db_handle *,
									   isc_tr_handle *,
									   const unsigned char*,
									   const unsigned char*,
									   ISC_BLOB_DESC *,
									   unsigned char *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_blob_set_desc(FbStatusVector* user_status,
									const unsigned char*,
									const unsigned char*,
									short,
									short,
									short,
									ISC_BLOB_DESC *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_cancel_blob(FbStatusVector* user_status,
								  isc_blob_handle* blob_handle)
{
	if (!m_api.cancel_blob)
		return notImplemented(user_status);

	return (*m_api.cancel_blob) (IscStatus(user_status), blob_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_cancel_events(FbStatusVector* user_status,
									isc_db_handle *,
									ISC_LONG *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_close_blob(FbStatusVector* user_status,
								 isc_blob_handle* blob_handle)
{
	if (!m_api.close_blob)
		return notImplemented(user_status);

	return (*m_api.close_blob) (IscStatus(user_status), blob_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_commit_retaining(FbStatusVector* user_status,
	isc_tr_handle* tra_handle)
{
	if (!m_api.commit_retaining)
		return notImplemented(user_status);

	return (*m_api.commit_retaining) (IscStatus(user_status), tra_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_commit_transaction(FbStatusVector* user_status,
	isc_tr_handle* tra_handle)
{
	if (!m_api.commit_transaction)
		return notImplemented(user_status);

	return (*m_api.commit_transaction) (IscStatus(user_status), tra_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_create_blob(FbStatusVector* user_status,
								  isc_db_handle* db_handle,
								  isc_tr_handle* tr_handle,
								  isc_blob_handle* blob_handle,
								  ISC_QUAD* blob_id)
{
	if (!m_api.create_blob)
		return notImplemented(user_status);

	return (*m_api.create_blob) (IscStatus(user_status), db_handle, tr_handle,
			blob_handle, blob_id);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_create_blob2(FbStatusVector* user_status,
								  isc_db_handle* db_handle,
								  isc_tr_handle* tr_handle,
								  isc_blob_handle* blob_handle,
								  ISC_QUAD* blob_id,
								  short bpb_length,
								  const char* bpb)
{
	if (!m_api.create_blob2)
		return notImplemented(user_status);

	return (*m_api.create_blob2) (IscStatus(user_status), db_handle, tr_handle,
			blob_handle, blob_id, bpb_length, bpb);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_create_database(FbStatusVector* user_status,
									  short,
									  const char*,
									  isc_db_handle *,
									  short,
									  const char*,
									  short)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_database_info(FbStatusVector* user_status,
									isc_db_handle* db_handle,
									short info_len,
									const unsigned char* info,
									short res_len,
									unsigned char* res)
{
	if (!m_api.database_info)
		return notImplemented(user_status);

	return (*m_api.database_info) (IscStatus(user_status), db_handle,
			info_len, reinterpret_cast<const ISC_SCHAR*>(info), res_len, reinterpret_cast<ISC_SCHAR*>(res));
}

void ISC_EXPORT IscProvider::isc_decode_date(const ISC_QUAD*,
							void *)
{
	return;
}

void ISC_EXPORT IscProvider::isc_decode_sql_date(const ISC_DATE*,
								void *)
{
	return;
}

void ISC_EXPORT IscProvider::isc_decode_sql_time(const ISC_TIME*,
								void *)
{
	return;
}

void ISC_EXPORT IscProvider::isc_decode_timestamp(const ISC_TIMESTAMP*,
								 void *)
{
	return;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_detach_database(FbStatusVector* user_status,
									  isc_db_handle* public_handle)
{
	if (!m_api.detach_database)
		return notImplemented(user_status);

	return (*m_api.detach_database) (IscStatus(user_status), public_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_drop_database(FbStatusVector* user_status,
									isc_db_handle *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_allocate_statement(FbStatusVector* user_status,
	isc_db_handle* db_handle, isc_stmt_handle* stmt_handle)
{
	if (!m_api.dsql_allocate_statement)
		return notImplemented(user_status);

	return (*m_api.dsql_allocate_statement) (IscStatus(user_status), db_handle, stmt_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_alloc_statement2(FbStatusVector* user_status,
	isc_db_handle* db_handle, isc_stmt_handle* stmt_handle)
{
	if (!m_api.dsql_alloc_statement2)
		return notImplemented(user_status);

	return (*m_api.dsql_alloc_statement2) (IscStatus(user_status), db_handle, stmt_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_describe(FbStatusVector* user_status,
	isc_stmt_handle* stmt_handle, unsigned short dialect, XSQLDA* sqlda)
{
	if (!m_api.dsql_describe)
		return notImplemented(user_status);

	return (*m_api.dsql_describe) (IscStatus(user_status), stmt_handle, dialect, sqlda);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_describe_bind(FbStatusVector* user_status,
	isc_stmt_handle* stmt_handle, unsigned short dialect, XSQLDA* sqlda)
{
	if (!m_api.dsql_describe_bind)
		return notImplemented(user_status);

	return (*m_api.dsql_describe_bind) (IscStatus(user_status), stmt_handle, dialect, sqlda);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_exec_immed2(FbStatusVector* user_status,
									   isc_db_handle *,
									   isc_tr_handle *,
									   unsigned short,
									   const char*,
									   unsigned short,
									   const XSQLDA *,
									   const XSQLDA *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_execute(FbStatusVector* user_status,
	isc_tr_handle* tra_handle, isc_stmt_handle* stmt_handle, unsigned short dialect,
	const XSQLDA* sqlda)
{
	if (!m_api.dsql_execute)
		return notImplemented(user_status);

	return (*m_api.dsql_execute) (IscStatus(user_status), tra_handle, stmt_handle, dialect, sqlda);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_execute2(FbStatusVector* user_status,
	isc_tr_handle* tra_handle, isc_stmt_handle* stmt_handle, unsigned short dialect,
	const XSQLDA* in_sqlda, const XSQLDA* out_sqlda)
{
	if (!m_api.dsql_execute2)
		return notImplemented(user_status);

	return (*m_api.dsql_execute2) (IscStatus(user_status), tra_handle, stmt_handle, dialect,
			in_sqlda, out_sqlda);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_execute_immediate(FbStatusVector* user_status,
	isc_db_handle* db_handle, isc_tr_handle* tra_handle, unsigned short length,
	const char* str, unsigned short dialect, const XSQLDA* sqlda)
{
	if (!m_api.dsql_execute_immediate)
		return notImplemented(user_status);

	return (*m_api.dsql_execute_immediate) (IscStatus(user_status),
		db_handle, tra_handle, length, str, dialect, sqlda);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_fetch(FbStatusVector* user_status,
	isc_stmt_handle* stmt_handle, unsigned short da_version, const XSQLDA* sqlda)
{
	if (!m_api.dsql_fetch)
		return notImplemented(user_status);

	return (*m_api.dsql_fetch) (IscStatus(user_status), stmt_handle, da_version, sqlda);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_finish(isc_db_handle *)
{
	return isc_unavailable;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_free_statement(FbStatusVector* user_status,
	isc_stmt_handle* stmt_handle, unsigned short option)
{
	if (!m_api.dsql_free_statement)
		return notImplemented(user_status);

	return (*m_api.dsql_free_statement) (IscStatus(user_status), stmt_handle, option);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_insert(FbStatusVector* user_status,
								  isc_stmt_handle *,
								  unsigned short,
								  XSQLDA *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_prepare(FbStatusVector* user_status,
	isc_tr_handle* tra_handle, isc_stmt_handle* stmt_handle, unsigned short length,
	const char* str, unsigned short dialect, XSQLDA* sqlda)
{
	if (!m_api.dsql_prepare)
		return notImplemented(user_status);

	return (*m_api.dsql_prepare) (IscStatus(user_status), tra_handle, stmt_handle,
				length, str, dialect, sqlda);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_set_cursor_name(FbStatusVector* user_status,
										   isc_stmt_handle *,
										   const char*,
										   unsigned short)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_dsql_sql_info(FbStatusVector* user_status,
	isc_stmt_handle* stmt_handle, short items_len, const unsigned char* items,
	short buffer_len, unsigned char* buffer)
{
	if (!m_api.dsql_sql_info)
		return notImplemented(user_status);

	return (*m_api.dsql_sql_info) (IscStatus(user_status), stmt_handle, items_len, reinterpret_cast<const ISC_SCHAR*>(items),
				buffer_len, reinterpret_cast<ISC_SCHAR*>(buffer));
}

void ISC_EXPORT IscProvider::isc_encode_date(const void*,
							ISC_QUAD *)
{
	return;
}

void ISC_EXPORT IscProvider::isc_encode_sql_date(const void*,
								ISC_DATE *)
{
	return;
}

void ISC_EXPORT IscProvider::isc_encode_sql_time(const void*,
								ISC_TIME *)
{
	return;
}

void ISC_EXPORT IscProvider::isc_encode_timestamp(const void*,
								 ISC_TIMESTAMP *)
{
	return;
}

ISC_LONG ISC_EXPORT_VARARG IscProvider::isc_event_block(char * *,
									   char * *,
									   unsigned short, ...)
{
	return 0;
}

void ISC_EXPORT IscProvider::isc_event_counts(ISC_ULONG *,
							 short,
							 char *,
							 const char*)
{
	return;
}

// 17 May 2001 - IscProvider::isc_expand_dpb is DEPRECATED
void ISC_EXPORT_VARARG IscProvider::isc_expand_dpb(char * *,
								  short *, ...)
{
	return;
}

int ISC_EXPORT IscProvider::isc_modify_dpb(char * *,
						  short *,
						  unsigned short,
						  const char*,
						  short)
{
	return 0;
}

ISC_LONG ISC_EXPORT IscProvider::isc_free(char *)
{
	return 0;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_get_segment(FbStatusVector* user_status,
								  isc_blob_handle* blob_handle,
								  unsigned short* length,
								  unsigned short buffer_length,
								  char* buffer)
{
	if (!m_api.get_segment)
		return notImplemented(user_status);

	return (*m_api.get_segment) (IscStatus(user_status), blob_handle, length,
			buffer_length, buffer);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_get_slice(FbStatusVector* user_status,
								isc_db_handle *,
								isc_tr_handle *,
								ISC_QUAD *,
								short,
								const char*,
								short,
								const ISC_LONG*,
								ISC_LONG,
								void *,
								ISC_LONG *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_interprete(char *,
								 const FbStatusVector * *)
{
	return isc_unavailable;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_open_blob(FbStatusVector* user_status,
								isc_db_handle* db_handle,
								isc_tr_handle* tr_handle,
								isc_blob_handle* blob_handle,
								ISC_QUAD* blob_id)
{
	if (!m_api.open_blob)
		return notImplemented(user_status);

	return (*m_api.open_blob) (IscStatus(user_status), db_handle, tr_handle,
			blob_handle, blob_id);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_open_blob2(FbStatusVector* user_status,
								isc_db_handle* db_handle,
								isc_tr_handle* tr_handle,
								isc_blob_handle* blob_handle,
								ISC_QUAD* blob_id,
								ISC_USHORT bpb_length,
								const ISC_UCHAR* bpb)
{
	if (!m_api.open_blob2)
		return notImplemented(user_status);

	return (*m_api.open_blob2) (IscStatus(user_status), db_handle, tr_handle,
			blob_handle, blob_id, bpb_length, bpb);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_prepare_transaction2(FbStatusVector* user_status,
										   isc_tr_handle *,
										   ISC_USHORT,
										   const ISC_UCHAR*)
{
	return notImplemented(user_status);
}

void ISC_EXPORT IscProvider::isc_print_sqlerror(ISC_SHORT,
							   const FbStatusVector*)
{
	return;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_print_status(const FbStatusVector*)
{
	return isc_unavailable;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_put_segment(FbStatusVector* user_status,
								  isc_blob_handle* blob_handle,
								  unsigned short buffer_length,
								  const char* buffer)
{
	if (!m_api.put_segment)
		return notImplemented(user_status);

	return (*m_api.put_segment) (IscStatus(user_status), blob_handle,
			buffer_length, buffer);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_put_slice(FbStatusVector* user_status,
								isc_db_handle *,
								isc_tr_handle *,
								ISC_QUAD *,
								short,
								const char*,
								short,
								const ISC_LONG*,
								ISC_LONG,
								void *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_que_events(FbStatusVector* user_status,
								 isc_db_handle *,
								 ISC_LONG *,
								 ISC_USHORT,
								 const ISC_UCHAR*,
								 isc_callback,
								 void *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_rollback_retaining(FbStatusVector* user_status,
	isc_tr_handle* tra_handle)
{
	if (!m_api.rollback_retaining)
		return notImplemented(user_status);

	return (*m_api.rollback_retaining) (IscStatus(user_status), tra_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_rollback_transaction(FbStatusVector* user_status,
	isc_tr_handle* tra_handle)
{
	if (!m_api.rollback_transaction)
		return notImplemented(user_status);

	return (*m_api.rollback_transaction) (IscStatus(user_status), tra_handle);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_start_multiple(FbStatusVector* user_status,
	isc_tr_handle* tra_handle, short count, void* vec)
{
	if (!m_api.start_multiple)
		return notImplemented(user_status);

	return (*m_api.start_multiple) (IscStatus(user_status), tra_handle, count, vec);
}


struct why_teb
{
	FB_API_HANDLE* teb_database;
	int teb_tpb_length;
	const UCHAR* teb_tpb;
};

ISC_STATUS ISC_EXPORT_VARARG IscProvider::isc_start_transaction(FbStatusVector* user_status,
											   isc_tr_handle* tra_handle,
											   short count, ...)
{
	if (!m_api.start_multiple)				// !!!
		return notImplemented(user_status);

	Firebird::HalfStaticArray<why_teb, 16> tebs;
	why_teb* teb = tebs.getBuffer(count);

	const why_teb* const end = teb + count;
	va_list ptr;
	va_start(ptr, count);

	for (why_teb* teb_iter = teb; teb_iter < end; teb_iter++)
	{
		teb_iter->teb_database = va_arg(ptr, FB_API_HANDLE*);
		teb_iter->teb_tpb_length = va_arg(ptr, int);
		teb_iter->teb_tpb = va_arg(ptr, UCHAR *);
	}
	va_end(ptr);

	return (*m_api.start_multiple) (IscStatus(user_status), tra_handle, count, teb);
}

ISC_STATUS ISC_EXPORT_VARARG IscProvider::isc_reconnect_transaction(FbStatusVector* user_status,
											isc_db_handle *,
											isc_tr_handle *,
											short,
											const char*)
{
	return notImplemented(user_status);
}

ISC_LONG ISC_EXPORT IscProvider::isc_sqlcode(const FbStatusVector* user_status)
{
	return isc_unavailable;
}

void ISC_EXPORT IscProvider::isc_sql_interprete(short,
							   char *,
							   short)
{
	return;
}

ISC_STATUS ISC_EXPORT IscProvider::isc_transaction_info(FbStatusVector* user_status,
									   isc_tr_handle *,
									   short,
									   const char*,
									   short,
									   char *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_transact_request(FbStatusVector* user_status,
									   isc_db_handle *,
									   isc_tr_handle *,
									   unsigned short,
									   char *,
									   unsigned short,
									   char *,
									   unsigned short,
									   char *)
{
	return notImplemented(user_status);
}

ISC_LONG ISC_EXPORT IscProvider::isc_vax_integer(const unsigned char* p, short len)
{
	return ::isc_vax_integer((ISC_SCHAR*) p, len);
}

ISC_INT64 ISC_EXPORT IscProvider::isc_portable_integer(const unsigned char* p, short len)
{
	return ::isc_portable_integer(p, len);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_seek_blob(FbStatusVector* user_status,
								isc_blob_handle *,
								short,
								ISC_LONG,
								ISC_LONG *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_service_attach(FbStatusVector* user_status,
									 unsigned short,
									 const char*,
									 isc_svc_handle *,
									 unsigned short,
									 const char*)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_service_detach(FbStatusVector* user_status,
									 isc_svc_handle *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_service_query(FbStatusVector* user_status,
									isc_svc_handle *,
									isc_resv_handle *,
									unsigned short,
									const char*,
									unsigned short,
									const char*,
									unsigned short,
									char *)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::isc_service_start(FbStatusVector* user_status,
									isc_svc_handle *,
									isc_resv_handle *,
									unsigned short,
									const char*)
{
	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::fb_cancel_operation(FbStatusVector* user_status,
										isc_db_handle* db_handle,
										USHORT option)
{
	if (m_api.cancel_operation)
		return m_api.cancel_operation(IscStatus(user_status), db_handle, option);

	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::fb_database_crypt_callback(FbStatusVector* user_status,
										void* cb)
{
	if (m_api.database_crypt_callback)
		return m_api.database_crypt_callback(IscStatus(user_status), cb);

	return notImplemented(user_status);
}

ISC_STATUS ISC_EXPORT IscProvider::fb_dsql_set_timeout(FbStatusVector* user_status,
	isc_stmt_handle* stmt_handle,
	ULONG timeout)
{
	if (m_api.dsql_set_timeout)
		return m_api.dsql_set_timeout(IscStatus(user_status), stmt_handle, timeout);

	return notImplemented(user_status);
}

void IscProvider::loadAPI()
{
	FbLocalStatus status;
	notImplemented(&status);
	status_exception::raise(&status);
}


// FBProvider

#define PROTO(PREFIX, NAME) (decltype(&::PREFIX##_##NAME)) &PREFIX##_##NAME

#if defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
static FirebirdApiPointers isc_callbacks =
{
	PROTO(isc, attach_database),
	PROTO(isc, array_gen_sdl),
	PROTO(isc, array_get_slice),
	PROTO(isc, array_lookup_bounds),
	PROTO(isc, array_lookup_desc),
	PROTO(isc, array_set_desc),
	PROTO(isc, array_put_slice),
	PROTO(isc, blob_default_desc),
	PROTO(isc, blob_gen_bpb),
	PROTO(isc, blob_info),
	PROTO(isc, blob_lookup_desc),
	PROTO(isc, blob_set_desc),
	PROTO(isc, cancel_blob),
	PROTO(isc, cancel_events),
	PROTO(isc, close_blob),
	PROTO(isc, commit_retaining),
	PROTO(isc, commit_transaction),
	PROTO(isc, create_blob),
	PROTO(isc, create_blob2),
	PROTO(isc, create_database),
	PROTO(isc, database_info),
	PROTO(isc, decode_date),
	PROTO(isc, decode_sql_date),
	PROTO(isc, decode_sql_time),
	PROTO(isc, decode_timestamp),
	PROTO(isc, detach_database),
	PROTO(isc, drop_database),
	PROTO(isc, dsql_allocate_statement),
	PROTO(isc, dsql_alloc_statement2),
	PROTO(isc, dsql_describe),
	PROTO(isc, dsql_describe_bind),
	PROTO(isc, dsql_exec_immed2),
	PROTO(isc, dsql_execute),
	PROTO(isc, dsql_execute2),
	PROTO(isc, dsql_execute_immediate),
	PROTO(isc, dsql_fetch),
	PROTO(isc, dsql_finish),
	PROTO(isc, dsql_free_statement),
	PROTO(isc, dsql_insert),
	PROTO(isc, dsql_prepare),
	PROTO(isc, dsql_set_cursor_name),
	PROTO(isc, dsql_sql_info),
	PROTO(isc, encode_date),
	PROTO(isc, encode_sql_date),
	PROTO(isc, encode_sql_time),
	PROTO(isc, encode_timestamp),
	PROTO(isc, event_block),
	PROTO(isc, event_counts),
	PROTO(isc, expand_dpb),
	PROTO(isc, modify_dpb),
	PROTO(isc, free),
	PROTO(isc, get_segment),
	PROTO(isc, get_slice),
	PROTO(isc, open_blob),
	PROTO(isc, open_blob2),
	PROTO(isc, prepare_transaction2),
	PROTO(isc, print_sqlerror),
	PROTO(isc, print_status),
	PROTO(isc, put_segment),
	PROTO(isc, put_slice),
	PROTO(isc, que_events),
	PROTO(isc, rollback_retaining),
	PROTO(isc, rollback_transaction),
	PROTO(isc, start_multiple),
	PROTO(isc, start_transaction),
	PROTO(isc, reconnect_transaction),
	PROTO(isc, sqlcode),
	PROTO(isc, sql_interprete),
	PROTO(isc, transaction_info),
	PROTO(isc, transact_request),
	PROTO(isc, vax_integer),
	PROTO(isc, seek_blob),
	PROTO(isc, service_attach),
	PROTO(isc, service_detach),
	PROTO(isc, service_query),
	PROTO(isc, service_start),
	PROTO(fb, interpret),
	PROTO(fb, cancel_operation),
	PROTO(fb, database_crypt_callback),
	PROTO(fb, dsql_set_timeout)
};
#if defined __GNUC__
#pragma GCC diagnostic pop
#endif


void FBProvider::loadAPI()
{
	m_api = isc_callbacks;
	m_api_loaded = true;
}


static bool isConnectionBrokenError(FbStatusVector* status)
{
	const ISC_STATUS code = status->getErrors()[1];
	switch (code)
	{
	case isc_shutdown:
	case isc_att_shutdown:
	case isc_bad_db_handle:
		return true;

	default:
		return fb_utils::isNetworkError(code);
	}
}


static void parseSQLDA(XSQLDA* xsqlda, UCharBuffer& buff, Firebird::Array<dsc> &descs)
{
	FB_SIZE_T offset = 0;
	XSQLVAR* xVar = xsqlda->sqlvar;

    for (int i = 0; i < xsqlda->sqld; xVar++, i++)
	{
		const UCHAR dtype = fb_utils::sqlTypeToDscType(xVar->sqltype & ~1);
		xVar->sqltype |= 1;

		if (type_alignments[dtype])
			offset = FB_ALIGN(offset, type_alignments[dtype]);

		offset += xVar->sqllen;
		const int type = xVar->sqltype & (~1);
		if (type == SQL_VARYING)
			offset += sizeof(SSHORT);

		// null indicator
		offset = FB_ALIGN(offset, type_alignments[dtype_short]);
		offset += sizeof(SSHORT);
    }

	descs.resize(xsqlda->sqld * 2);
	UCHAR* p = buff.getBuffer(offset);

	offset = 0;
	xVar = xsqlda->sqlvar;

    for (int i = 0; i < xsqlda->sqld; xVar++, i++)
	{
		const UCHAR dtype = fb_utils::sqlTypeToDscType(xVar->sqltype & ~1);
		if (type_alignments[dtype])
			offset = FB_ALIGN(offset, type_alignments[dtype]);

		xVar->sqldata = (ISC_SCHAR*) (p + offset);

		// build the src descriptor
		dsc& src = descs[i * 2];
		src.dsc_dtype = dtype;
		src.dsc_length = xVar->sqllen;
		src.dsc_scale = xVar->sqlscale;
		src.dsc_sub_type = xVar->sqlsubtype;
		src.dsc_address = (UCHAR*) xVar->sqldata;

		offset += xVar->sqllen;
		const int type = xVar->sqltype & (~1);
		if (type == SQL_VARYING)
		{
			offset += sizeof(SSHORT);
			src.dsc_length += sizeof(SSHORT);
		}
		else if (type == SQL_NULL) {
			src.dsc_flags |= DSC_null;
		}

		// null indicator
		offset = FB_ALIGN(offset, type_alignments[dtype_short]);
		xVar->sqlind = (SSHORT*) (p + offset);

		dsc& null = descs[i * 2 + 1];
		null.makeShort(0, xVar->sqlind);

		offset += sizeof(SSHORT);
	}
}



} // namespace EDS
