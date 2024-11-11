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
 *  Copyright (c) 2008 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "fb_types.h"
#include "../../include/fb_blk.h"

#include "../align.h"
#include "../exe.h"
#include "../jrd.h"
#include "../tra.h"
#include "../common/dsc.h"
#include "../../dsql/dsql.h"
#include "../firebird/impl/sqlda_pub.h"

#include "../blb_proto.h"
#include "../evl_proto.h"
#include "../exe_proto.h"
#include "../mov_proto.h"
#include "../mov_proto.h"
#include "../PreparedStatement.h"
#include "../Function.h"

#include "InternalDS.h"
#include "ValidatePassword.h"

using namespace Jrd;
using namespace Firebird;

namespace EDS {

const char* INTERNAL_PROVIDER_NAME = "Internal";

class RegisterInternalProvider
{
public:
	RegisterInternalProvider(MemoryPool&)
	{
		InternalProvider* provider = FB_NEW InternalProvider(INTERNAL_PROVIDER_NAME);
		Manager::addProvider(provider);
	}
};

static GlobalPtr<RegisterInternalProvider> reg;

// InternalProvider

void InternalProvider::jrdAttachmentEnd(thread_db* tdbb, Attachment* att, bool forced)
{
	Provider::jrdAttachmentEnd(tdbb, att, forced);

	Connection* conn = att->att_ext_parent;
	if (!conn)
		return;

	{	// scope
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		AttToConnMap::Accessor acc(&m_connections);
		if (acc.locate(AttToConn(conn->getBoundAtt(), conn)))
		{
			InternalConnection* intConn = (InternalConnection*) acc.current().m_conn;
			if (!intConn->getJrdAtt() || intConn->getJrdAtt()->getHandle() != att)
			{
				fb_assert(intConn->getJrdAtt() == NULL);
				return;
			}

			fb_assert(intConn == conn);
		}
		else
			return;
	}

	if (conn)
		releaseConnection(tdbb, *conn, false);
}

void InternalProvider::getRemoteError(const FbStatusVector* status, string& err) const
{
	err = "";

	char buff[1024];
	const ISC_STATUS* p = status->getErrors();

	for (;;)
	{
		const ISC_STATUS* code = p + 1;
		if (!fb_interpret(buff, sizeof(buff), &p))
			break;

		string rem_err;
		rem_err.printf("%lu : %s\n", *code, buff);
		err += rem_err;
	}
}

Connection* InternalProvider::doCreateConnection()
{
	return FB_NEW InternalConnection(*this);
}


// InternalConnection

InternalConnection::~InternalConnection()
{
}

void InternalConnection::attach(thread_db* tdbb)
{
	fb_assert(!m_attachment);
	Database* dbb = tdbb->getDatabase();
	Attachment* attachment = tdbb->getAttachment();
	fb_assert(m_dbName.isEmpty() || m_dbName == dbb->dbb_database_name.c_str());

	// Don't wrap raised errors. This is needed for backward compatibility.
	setWrapErrors(false);

	if (m_dpb.isEmpty())
	{
		m_isCurrent = true;
		m_attachment = attachment->getInterface();
	}
	else
	{
		m_isCurrent = false;
		m_dbName = dbb->dbb_database_name.c_str();

		// Avoid change of m_dpb by validatePassword() below
		ClumpletWriter newDpb(ClumpletReader::dpbList, MAX_DPB_SIZE, m_dpb.begin(), m_dpb.getCount());
		validatePassword(tdbb, m_dbName, newDpb);
		newDpb.insertInt(isc_dpb_ext_call_depth, attachment->att_ext_call_depth + 1);

		FbLocalStatus status;
		{
			EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
			m_provider.reset(attachment->getProvider());
			m_provider->addRef();
			m_attachment.assignRefNoIncr(m_provider->attachDatabase(&status, m_dbName.c_str(),
				newDpb.getBufferLength(), newDpb.getBuffer()));
		}

		if (status->getState() & IStatus::STATE_ERRORS)
			raise(&status, tdbb, "JProvider::attach");

		m_attachment->getHandle()->att_ext_parent = this;
	}

	m_sqlDialect = (attachment->att_database->dbb_flags & DBB_DB_SQL_dialect_3) ?
					SQL_DIALECT_V6 : SQL_DIALECT_V5;

	memset(m_features, false, sizeof(m_features));
	static const info_features features[] = ENGINE_FEATURES;
	for (int i = 0; i < FB_NELEM(features); i++)
		setFeature(features[i]);
}

void InternalConnection::doDetach(thread_db* tdbb)
{
	fb_assert(m_attachment);
	if (!m_attachment->getHandle())
		return;

	if (m_isCurrent)
	{
		m_attachment = NULL;
	}
	else
	{
		FbLocalStatus status;

		RefPtr<JAttachment> att = m_attachment;
		m_attachment = NULL;	// release and nullify

		{	// scope
			EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
			att->detach(&status);
		}

		if (!(status->getState() & IStatus::STATE_ERRORS))
			att.clear();

		if (status->getErrors()[1] == isc_att_shutdown || status->getErrors()[1] == isc_shutdown)
		{
			status->init();
		}

		if (status->getState() & IStatus::STATE_ERRORS)
		{
			m_attachment = att;
			raise(&status, tdbb, "JAttachment::detach");
		}
	}

	fb_assert(!m_attachment);
}

bool InternalConnection::cancelExecution(bool /*forced*/)
{
	if (!m_attachment->getHandle())
		return false;

	if (m_isCurrent)
		return true;

	FbLocalStatus status;

	m_attachment->cancelOperation(&status, fb_cancel_raise);
	return !(status->getState() & IStatus::STATE_ERRORS);
}

bool InternalConnection::resetSession(thread_db* tdbb)
{
	fb_assert(!m_isCurrent);

	if (m_isCurrent)
		return true;

	FbLocalStatus status;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		m_attachment->execute(&status, NULL, 0, "ALTER SESSION RESET",
			m_sqlDialect, NULL, NULL, NULL, NULL);
	}
	return !(status->getState() & IStatus::STATE_ERRORS);
}

// this internal connection instance is available for the current execution context if it
// a) is current connection and current thread's attachment is equal to
//	  this attachment, or
// b) is not current connection
bool InternalConnection::isAvailable(thread_db* tdbb, TraScope /*traScope*/) const
{
	return !m_isCurrent ||
		(m_isCurrent && (tdbb->getAttachment() == m_attachment->getHandle()));
}

bool InternalConnection::validate(thread_db* tdbb)
{
	if (m_isCurrent)
		return true;

	if (!m_attachment)
		return false;

	EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
	FbLocalStatus status;
	m_attachment->ping(&status);
	return status.isSuccess();
}

bool InternalConnection::isSameDatabase(const PathName& dbName, ClumpletReader& dpb) const
{
	if (m_isCurrent)
	{
		const Attachment* att = m_attachment->getHandle();
		const MetaString& attUser = att->getUserName();
		const MetaString& attRole = att->getSqlRole();

		MetaString str;

		if (dpb.find(isc_dpb_user_name))
		{
			dpb.getString(str);
			if (str != attUser)
				return false;
		}

		if (dpb.find(isc_dpb_sql_role_name))
		{
			dpb.getString(str);
			if (str != attRole)
				return false;
		}
	}

	return Connection::isSameDatabase(dbName, dpb);
}

Transaction* InternalConnection::doCreateTransaction()
{
	return FB_NEW InternalTransaction(*this);
}

Statement* InternalConnection::doCreateStatement()
{
	return FB_NEW InternalStatement(*this);
}

Blob* InternalConnection::createBlob()
{
	return FB_NEW InternalBlob(*this);
}


// InternalTransaction()

void InternalTransaction::doStart(FbStatusVector* status, thread_db* tdbb, ClumpletWriter& tpb)
{
	fb_assert(!m_transaction);

	jrd_tra* localTran = tdbb->getTransaction();
	fb_assert(localTran);

	if (m_scope == traCommon && m_IntConnection.isCurrent())
		m_transaction = localTran->getInterface(true);
	else
	{
		JAttachment* att = m_IntConnection.getJrdAtt();

		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		m_transaction.assignRefNoIncr(
			att->startTransaction(status, tpb.getBufferLength(), tpb.getBuffer()));

		if (m_transaction)
			m_transaction->getHandle()->tra_callback_count = localTran->tra_callback_count;
	}
}

void InternalTransaction::doPrepare(FbStatusVector* /*status*/, thread_db* /*tdbb*/,
		int /*info_len*/, const char* /*info*/)
{
	fb_assert(m_transaction);
	fb_assert(false);
}

void InternalTransaction::doCommit(FbStatusVector* status, thread_db* tdbb, bool retain)
{
	fb_assert(m_transaction);

	if (m_scope == traCommon && m_IntConnection.isCurrent())
	{
		if (!retain) {
			m_transaction = NULL;	// release and nullify
		}
	}
	else
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		if (retain)
			m_transaction->commitRetaining(status);
		else
		{
			m_transaction->commit(status);
			if (!(status->getState() & IStatus::STATE_ERRORS))
				m_transaction.clear();
		}
	}
}

void InternalTransaction::doRollback(FbStatusVector* status, thread_db* tdbb, bool retain)
{
	fb_assert(m_transaction);

	if (m_connection.isBroken())
	{
		m_transaction.clear();
		return;
	}

	if (m_scope == traCommon && m_IntConnection.isCurrent())
	{
		if (!retain) {
			m_transaction = NULL;	// release and nullify
		}
		return;
	}

	ISC_STATUS err = 0;
	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);
		if (retain)
			m_transaction->rollbackRetaining(status);
		else
			m_transaction->rollback(status);

		if (status->getState() & IStatus::STATE_ERRORS)
			err = status->getErrors()[1];

		if (err == isc_cancelled)
		{
			FbLocalStatus temp;
			JAttachment* jAtt = m_IntConnection.getJrdAtt();
			jAtt->cancelOperation(&temp, fb_cancel_disable);

			status->init();
			if (retain)
				m_transaction->rollbackRetaining(status);
			else
				m_transaction->rollback(status);

			err = (status->getState() & IStatus::STATE_ERRORS) ?
				status->getErrors()[1] : 0;

			jAtt->cancelOperation(&temp, fb_cancel_enable);
		}
	}

	if ((!err || err == isc_att_shutdown || err == isc_shutdown) && !retain)
	{
		m_transaction.clear();
		status->init();
	}
}


// InternalStatement

InternalStatement::InternalStatement(InternalConnection& conn) :
	Statement(conn),
	m_intConnection(conn),
	m_intTransaction(0),
	m_request(0),
	m_cursor(0),
	m_inMetadata(FB_NEW MsgMetadata),
	m_outMetadata(FB_NEW MsgMetadata)
{
}

InternalStatement::~InternalStatement()
{
}

void InternalStatement::doPrepare(thread_db* tdbb, const string& sql)
{
	m_inMetadata->reset();
	m_outMetadata->reset();

	JAttachment* att = m_intConnection.getJrdAtt();
	JTransaction* tran = getIntTransaction()->getJrdTran();

	FbLocalStatus status;

	if (m_request)
	{
		doClose(tdbb, true);
		fb_assert(!m_allocated);
	}

	CallerName save_caller_name(tran->getHandle()->tra_caller_name);

	if (m_callerPrivileges)
	{
		Request* request = tdbb->getRequest();
		auto statement = request ? request->getStatement() : NULL;
		CallerName callerName;
		const Routine* routine;

		if (statement && statement->parentStatement)
			statement = statement->parentStatement;

		if (statement)
		{
			if (statement->triggerInvoker)
			{
				tran->getHandle()->tra_caller_name =
					CallerName(obj_trigger, statement->triggerName, statement->triggerInvoker->getUserName());
			}
			else if (statement->triggerName.object.hasData())
			{
				tran->getHandle()->tra_caller_name =
					CallerName(obj_trigger, statement->triggerName, "");
			}
			else if ((routine = statement->getRoutine()) && routine->getName().object.hasData())
			{
				const MetaString& userName = routine->invoker ? routine->invoker->getUserName() : "";
				tran->getHandle()->tra_caller_name = CallerName(routine->getObjectType(), routine->getName(), userName);
			}
		}
		else
			tran->getHandle()->tra_caller_name = CallerName();
	}

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		m_request.assignRefNoIncr(att->prepare(&status, tran, sql.length(), sql.c_str(),
			m_connection.getSqlDialect(), 0));
	}
	m_allocated = (m_request != NULL);

	if (tran->getHandle())
		tran->getHandle()->tra_caller_name = save_caller_name;

	if (status->getState() & IStatus::STATE_ERRORS)
		raise(&status, tdbb, "JAttachment::prepare", &sql);

	const auto dsqlStatement = m_request->getHandle()->getDsqlStatement();

	if (dsqlStatement->getSendMsg())
	{
		try
		{
			PreparedStatement::parseDsqlMessage(dsqlStatement->getSendMsg(), m_inDescs,
				m_inMetadata, m_in_buffer);
			m_inputs = m_inMetadata->getCount();
		}
		catch (const Exception&)
		{
			raise(tdbb->tdbb_status_vector, tdbb, "parse input message", &sql);
		}
	}
	else
		m_inputs = 0;

	if (dsqlStatement->getReceiveMsg())
	{
		try
		{
			PreparedStatement::parseDsqlMessage(dsqlStatement->getReceiveMsg(), m_outDescs,
				m_outMetadata, m_out_buffer);
			m_outputs = m_outMetadata->getCount();
		}
		catch (const Exception&)
		{
			raise(tdbb->tdbb_status_vector, tdbb, "parse output message", &sql);
		}
	}
	else
		m_outputs = 0;

	m_stmt_selectable = false;

	switch (dsqlStatement->getType())
	{
	case DsqlStatement::TYPE_SELECT:
	case DsqlStatement::TYPE_RETURNING_CURSOR:
	case DsqlStatement::TYPE_SELECT_UPD:
	case DsqlStatement::TYPE_SELECT_BLOCK:
		m_stmt_selectable = true;
		break;

	case DsqlStatement::TYPE_START_TRANS:
	case DsqlStatement::TYPE_COMMIT:
	case DsqlStatement::TYPE_ROLLBACK:
	case DsqlStatement::TYPE_COMMIT_RETAIN:
	case DsqlStatement::TYPE_ROLLBACK_RETAIN:
	case DsqlStatement::TYPE_CREATE_DB:
		Arg::Gds(isc_eds_expl_tran_ctrl).copyTo(&status);
		raise(&status, tdbb, "JAttachment::prepare", &sql);
		break;

	case DsqlStatement::TYPE_INSERT:
	case DsqlStatement::TYPE_DELETE:
	case DsqlStatement::TYPE_UPDATE:
	case DsqlStatement::TYPE_UPDATE_CURSOR:
	case DsqlStatement::TYPE_DELETE_CURSOR:
	case DsqlStatement::TYPE_DDL:
	case DsqlStatement::TYPE_EXEC_PROCEDURE:
	case DsqlStatement::TYPE_SET_GENERATOR:
	case DsqlStatement::TYPE_SAVEPOINT:
	case DsqlStatement::TYPE_EXEC_BLOCK:
		break;
	}
}


void InternalStatement::doSetTimeout(thread_db* tdbb, unsigned int timeout)
{
	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		m_request->setTimeout(&status, timeout);
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		raise(&status, tdbb, "JStatement::setTimeout");
}


void InternalStatement::doExecute(thread_db* tdbb)
{
	JTransaction* transaction = getIntTransaction()->getJrdTran();

	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		fb_assert(m_inMetadata->getMessageLength() == m_in_buffer.getCount());
		fb_assert(m_outMetadata->getMessageLength() == m_out_buffer.getCount());

		m_request->execute(&status, transaction,
			m_inMetadata, m_in_buffer.begin(), m_outMetadata, m_out_buffer.begin());
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		raise(&status, tdbb, "JStatement::execute");
}


void InternalStatement::doOpen(thread_db* tdbb)
{
	JTransaction* transaction = getIntTransaction()->getJrdTran();

	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		if (m_cursor)
		{
			m_cursor->close(&status);
			m_cursor.clear();
		}

		fb_assert(m_inMetadata->getMessageLength() == m_in_buffer.getCount());

		m_cursor.assignRefNoIncr(m_request->openCursor(&status, transaction,
			m_inMetadata, m_in_buffer.begin(), m_outMetadata, 0));
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		raise(&status, tdbb, "JStatement::open");
}


bool InternalStatement::doFetch(thread_db* tdbb)
{
	FbLocalStatus status;

	bool res = true;

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		fb_assert(m_outMetadata->getMessageLength() == m_out_buffer.getCount());
		fb_assert(m_cursor);
		res = m_cursor->fetchNext(&status, m_out_buffer.begin()) == IStatus::RESULT_OK;
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		raise(&status, tdbb, "JResultSet::fetchNext");

	return res;
}


void InternalStatement::doClose(thread_db* tdbb, bool drop)
{
	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, *this, FB_FUNCTION);

		if (m_cursor)
		{
			m_cursor->close(&status);
			m_cursor.clear();
		}

		if (status->getState() & IStatus::STATE_ERRORS)
		{
			raise(&status, tdbb, "JResultSet::close");
		}

		if (drop)
		{
			if (m_request)
			{
				m_request->free(&status);
				m_request.clear();
			}

			m_allocated = false;

			if (status->getState() & IStatus::STATE_ERRORS)
			{
				raise(&status, tdbb, "JStatement::free");
			}
		}
	}
}

// We need to copy external blob from\to dynamic statement.
// If external blob is permanent one - we could use its blob_id and avoid
// copying blob contents into new temporary blob.
// If external blob is temporary - we could access it by blob_id only if
// dynamic statement is executed in the same transaction as local (caller)
// statement.

static bool isPermanentBlob(const dsc& src)
{
	if (src.isBlob())
	{
		const bid* srcBlobID = reinterpret_cast<bid*>(src.dsc_address);
		return (srcBlobID->bid_internal.bid_relation_id != 0);
	}

	return false;
}

void InternalStatement::putExtBlob(thread_db* tdbb, dsc& src, dsc& dst)
{
	if (isPermanentBlob(src) || (m_transaction->getScope() == traCommon && m_intConnection.isCurrent()))
		MOV_move(tdbb, &src, &dst);
	else
		Statement::putExtBlob(tdbb, src, dst);
}

void InternalStatement::getExtBlob(thread_db* tdbb, const dsc& src, dsc& dst)
{
	fb_assert(dst.dsc_length == src.dsc_length);
	fb_assert(dst.dsc_length == sizeof(bid));

	if (isPermanentBlob(src) || (m_transaction->getScope() == traCommon && m_intConnection.isCurrent()))
		memcpy(dst.dsc_address, src.dsc_address, sizeof(bid));
	else
		Statement::getExtBlob(tdbb, src, dst);
}



// InternalBlob

InternalBlob::InternalBlob(InternalConnection& conn) :
	Blob(conn),
	m_connection(conn),
	m_blob(NULL)
{
	memset(&m_blob_id, 0, sizeof(m_blob_id));
}

InternalBlob::~InternalBlob()
{
	fb_assert(!m_blob);
}

void InternalBlob::open(thread_db* tdbb, Transaction& tran, const dsc& desc, const UCharBuffer* bpb)
{
	fb_assert(!m_blob);
	fb_assert(sizeof(m_blob_id) == desc.dsc_length);

	JAttachment* att = m_connection.getJrdAtt();
	JTransaction* transaction = static_cast<InternalTransaction&>(tran).getJrdTran();
	memcpy(&m_blob_id, desc.dsc_address, sizeof(m_blob_id));

	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, m_connection, FB_FUNCTION);

		USHORT bpb_len = bpb ? bpb->getCount() : 0;
		const UCHAR* bpb_buff = bpb ? bpb->begin() : NULL;

		m_blob.assignRefNoIncr(
			att->openBlob(&status, transaction, &m_blob_id, bpb_len, bpb_buff));
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		m_connection.raise(&status, tdbb, "JAttachment::openBlob");

	fb_assert(m_blob);
}

void InternalBlob::create(thread_db* tdbb, Transaction& tran, dsc& desc, const UCharBuffer* bpb)
{
	fb_assert(!m_blob);
	fb_assert(sizeof(m_blob_id) == desc.dsc_length);

	JAttachment* att = m_connection.getJrdAtt();
	JTransaction* transaction = ((InternalTransaction&) tran).getJrdTran();
	memset(&m_blob_id, 0, sizeof(m_blob_id));

	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, m_connection, FB_FUNCTION);

		const USHORT bpb_len = bpb ? bpb->getCount() : 0;
		const UCHAR* bpb_buff = bpb ? bpb->begin() : NULL;

		m_blob.assignRefNoIncr(
			att->createBlob(&status, transaction, &m_blob_id, bpb_len, bpb_buff));
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		m_connection.raise(&status, tdbb, "JAttachment::createBlob");

	fb_assert(m_blob);
	memcpy(desc.dsc_address, &m_blob_id, sizeof(m_blob_id));
}

USHORT InternalBlob::read(thread_db* tdbb, UCHAR* buff, USHORT len)
{
	fb_assert(m_blob);

	unsigned result = 0;
	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, m_connection, FB_FUNCTION);
		m_blob->getSegment(&status, len, buff, &result);
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		m_connection.raise(&status, tdbb, "JBlob::getSegment");

	return result;
}

void InternalBlob::write(thread_db* tdbb, const UCHAR* buff, USHORT len)
{
	fb_assert(m_blob);

	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, m_connection, FB_FUNCTION);
		m_blob->putSegment(&status, len, buff);
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		m_connection.raise(&status, tdbb, "JBlob::putSegment");
}

void InternalBlob::close(thread_db* tdbb)
{
	fb_assert(m_blob);
	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, m_connection, FB_FUNCTION);
		m_blob->close(&status);
		m_blob.clear();
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		m_connection.raise(&status, tdbb, "JBlob::close");

	fb_assert(!m_blob);
}

void InternalBlob::cancel(thread_db* tdbb)
{
	if (!m_blob) {
		return;
	}

	FbLocalStatus status;

	{
		EngineCallbackGuard guard(tdbb, m_connection, FB_FUNCTION);
		m_blob->cancel(&status);
		m_blob.clear();
	}

	if (status->getState() & IStatus::STATE_ERRORS)
		m_connection.raise(&status, tdbb, "JBlob::cancel");

	fb_assert(!m_blob);
}


} // namespace EDS
