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

#ifndef EXTDS_INTERNAL_H
#define EXTDS_INTERNAL_H


#include "ExtDS.h"

namespace EDS {

class InternalProvider : public Provider
{
public:
	explicit InternalProvider(const char* prvName) :
		Provider(prvName)
	{
	}

	~InternalProvider()
	{}

	virtual void jrdAttachmentEnd(Jrd::thread_db* tdbb, Jrd::Attachment* att, bool forced);

	virtual void initialize() {}
	virtual void getRemoteError(const Jrd::FbStatusVector* status, Firebird::string& err) const;

protected:
	virtual Connection* doCreateConnection();
};


class InternalConnection : public Connection
{
protected:
	friend class InternalProvider;

	explicit InternalConnection(InternalProvider& prov) :
	  Connection(prov),
	  m_attachment(0),
	  m_isCurrent(false)
	{}

	virtual ~InternalConnection();

public:
	virtual void attach(Jrd::thread_db* tdbb);

	virtual bool cancelExecution(bool forced);
	virtual bool resetSession();

	virtual bool isAvailable(Jrd::thread_db* tdbb, TraScope traScope) const;

	virtual bool isConnected() const { return (m_attachment != 0); }
	virtual bool validate(Jrd::thread_db* tdbb);

	virtual bool isSameDatabase(const Firebird::PathName& dbName,
		Firebird::ClumpletReader& dpb) const;

	virtual bool isCurrent() const { return m_isCurrent; }

	Jrd::JAttachment* getJrdAtt() { return m_attachment; }

	virtual Blob* createBlob();

protected:
	virtual Transaction* doCreateTransaction();
	virtual Statement* doCreateStatement();
	virtual void doDetach(Jrd::thread_db* tdbb);

	Firebird::AutoPlugin<Jrd::JProvider> m_provider;
	Firebird::RefPtr<Jrd::JAttachment> m_attachment;
	bool m_isCurrent;
};


class InternalTransaction : public Transaction
{
protected:
	friend class InternalConnection;

	explicit InternalTransaction(InternalConnection& conn) :
	  Transaction(conn),
	  m_IntConnection(conn),
	  m_transaction(0)
	{}

	virtual ~InternalTransaction() {}

public:
	Jrd::JTransaction* getJrdTran() { return m_transaction; }

protected:
	virtual void doStart(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, Firebird::ClumpletWriter& tpb);
	virtual void doPrepare(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, int info_len, const char* info);
	virtual void doCommit(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain);
	virtual void doRollback(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain);

	InternalConnection& m_IntConnection;
	Firebird::RefPtr<Jrd::JTransaction> m_transaction;
};


class InternalStatement : public Statement
{
protected:
	friend class InternalConnection;

	explicit InternalStatement(InternalConnection& conn);
	~InternalStatement();

protected:
	virtual void doPrepare(Jrd::thread_db* tdbb, const Firebird::string& sql);
	virtual void doSetTimeout(Jrd::thread_db* tdbb, unsigned int timeout);
	virtual void doExecute(Jrd::thread_db* tdbb);
	virtual void doOpen(Jrd::thread_db* tdbb);
	virtual bool doFetch(Jrd::thread_db* tdbb);
	virtual void doClose(Jrd::thread_db* tdbb, bool drop);

	virtual void putExtBlob(Jrd::thread_db* tdbb, dsc& src, dsc& dst);
	virtual void getExtBlob(Jrd::thread_db* tdbb, const dsc& src, dsc& dst);

	InternalTransaction* getIntTransaction()
	{
		return (InternalTransaction*) m_transaction;
	}

	InternalConnection& m_intConnection;
	InternalTransaction* m_intTransaction;

	Firebird::RefPtr<Jrd::JStatement> m_request;
	Firebird::RefPtr<Jrd::JResultSet> m_cursor;
	Firebird::RefPtr<Firebird::MsgMetadata> m_inMetadata, m_outMetadata;
};


class InternalBlob : public Blob
{
	friend class InternalConnection;
protected:
	explicit InternalBlob(InternalConnection& conn);

public:
	~InternalBlob();

public:
	virtual void open(Jrd::thread_db* tdbb, Transaction& tran, const dsc& desc, const Firebird::UCharBuffer* bpb);
	virtual void create(Jrd::thread_db* tdbb, Transaction& tran, dsc& desc, const Firebird::UCharBuffer* bpb);
	virtual USHORT read(Jrd::thread_db* tdbb, UCHAR* buff, USHORT len);
	virtual void write(Jrd::thread_db* tdbb, const UCHAR* buff, USHORT len);
	virtual void close(Jrd::thread_db* tdbb);
	virtual void cancel(Jrd::thread_db* tdbb);

private:
	InternalConnection& m_connection;
	Firebird::RefPtr<Jrd::JBlob> m_blob;
	ISC_QUAD m_blob_id;
};

} // namespace EDS

#endif // EXTDS_INTERNAL_H
