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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Alex Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_ENGINE_INTERFACE_H
#define JRD_ENGINE_INTERFACE_H

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/StatementMetadata.h"
#include "../common/classes/RefCounted.h"

namespace Jrd {

// Engine objects used by interface objects
class blb;
class jrd_tra;
class DsqlCursor;
class DsqlBatch;
class dsql_req;
class JrdStatement;
class StableAttachmentPart;
class Attachment;
class Service;
class UserId;
class Applier;

// forward declarations
class JStatement;
class JAttachment;
class JProvider;

class JBlob FB_FINAL :
	public Firebird::RefCntIface<Firebird::IBlobImpl<JBlob, Firebird::CheckStatusWrapper> >
{
public:
	// IBlob implementation
	int release() override;
	void getInfo(Firebird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	int getSegment(Firebird::CheckStatusWrapper* status, unsigned int length, void* buffer,
		unsigned int* segmentLength) override;
	void putSegment(Firebird::CheckStatusWrapper* status, unsigned int length, const void* buffer) override;
	void cancel(Firebird::CheckStatusWrapper* status) override;
	void close(Firebird::CheckStatusWrapper* status) override;
	int seek(Firebird::CheckStatusWrapper* status, int mode, int offset) override;			// returns position

public:
	JBlob(blb* handle, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	blb* getHandle() throw()
	{
		return blob;
	}

	void clearHandle()
	{
		blob = NULL;
	}

private:
	blb* blob;
	Firebird::RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JTransaction FB_FINAL :
	public Firebird::RefCntIface<Firebird::ITransactionImpl<JTransaction, Firebird::CheckStatusWrapper> >
{
public:
	// ITransaction implementation
	int release() override;
	void getInfo(Firebird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void prepare(Firebird::CheckStatusWrapper* status,
		unsigned int msg_length = 0, const unsigned char* message = 0) override;
	void commit(Firebird::CheckStatusWrapper* status) override;
	void commitRetaining(Firebird::CheckStatusWrapper* status) override;
	void rollback(Firebird::CheckStatusWrapper* status) override;
	void rollbackRetaining(Firebird::CheckStatusWrapper* status) override;
	void disconnect(Firebird::CheckStatusWrapper* status) override;
	Firebird::ITransaction* join(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction) override;
	JTransaction* validate(Firebird::CheckStatusWrapper* status, Firebird::IAttachment* testAtt) override;
	JTransaction* enterDtc(Firebird::CheckStatusWrapper* status) override;

public:
	JTransaction(jrd_tra* handle, StableAttachmentPart* sa);

	jrd_tra* getHandle() throw()
	{
		return transaction;
	}

	void setHandle(jrd_tra* handle)
	{
		transaction = handle;
	}

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	void clear()
	{
		transaction = NULL;
		release();
	}

private:
	jrd_tra* transaction;
	Firebird::RefPtr<StableAttachmentPart> sAtt;

	JTransaction(JTransaction* from);

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JResultSet FB_FINAL :
	public Firebird::RefCntIface<Firebird::IResultSetImpl<JResultSet, Firebird::CheckStatusWrapper> >
{
public:
	// IResultSet implementation
	int release() override;
	int fetchNext(Firebird::CheckStatusWrapper* status, void* message) override;
	int fetchPrior(Firebird::CheckStatusWrapper* status, void* message) override;
	int fetchFirst(Firebird::CheckStatusWrapper* status, void* message) override;
	int fetchLast(Firebird::CheckStatusWrapper* status, void* message) override;
	int fetchAbsolute(Firebird::CheckStatusWrapper* status, int position, void* message) override;
	int fetchRelative(Firebird::CheckStatusWrapper* status, int offset, void* message) override;
	FB_BOOLEAN isEof(Firebird::CheckStatusWrapper* status) override;
	FB_BOOLEAN isBof(Firebird::CheckStatusWrapper* status) override;
	Firebird::IMessageMetadata* getMetadata(Firebird::CheckStatusWrapper* status) override;
	void close(Firebird::CheckStatusWrapper* status) override;
	void setDelayedOutputFormat(Firebird::CheckStatusWrapper* status, Firebird::IMessageMetadata* format) override;

public:
	JResultSet(DsqlCursor* handle, JStatement* aStatement);

	StableAttachmentPart* getAttachment();

	DsqlCursor* getHandle() throw()
	{
		return cursor;
	}

	void resetHandle()
	{
		cursor = NULL;
	}

private:
	DsqlCursor* cursor;
	Firebird::RefPtr<JStatement> statement;
	int state;

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JBatch FB_FINAL :
	public Firebird::RefCntIface<Firebird::IBatchImpl<JBatch, Firebird::CheckStatusWrapper> >
{
public:
	// IBatch implementation
	int release() override;
	void add(Firebird::CheckStatusWrapper* status, unsigned count, const void* inBuffer) override;
	void addBlob(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
		unsigned parLength, const unsigned char* par) override;
	void appendBlobData(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void addBlobStream(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void registerBlob(Firebird::CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) override;
	Firebird::IBatchCompletionState* execute(Firebird::CheckStatusWrapper* status,
		Firebird::ITransaction* transaction) override;
	void cancel(Firebird::CheckStatusWrapper* status) override;
	unsigned getBlobAlignment(Firebird::CheckStatusWrapper* status) override;
	Firebird::IMessageMetadata* getMetadata(Firebird::CheckStatusWrapper* status) override;
	void setDefaultBpb(Firebird::CheckStatusWrapper* status, unsigned parLength, const unsigned char* par) override;
	void close(Firebird::CheckStatusWrapper* status) override;

public:
	JBatch(DsqlBatch* handle, JStatement* aStatement, Firebird::IMessageMetadata* aMetadata);

	StableAttachmentPart* getAttachment();

	DsqlBatch* getHandle() throw()
	{
		return batch;
	}

	void resetHandle()
	{
		batch = NULL;
	}

private:
	DsqlBatch* batch;
	Firebird::RefPtr<JStatement> statement;
	Firebird::RefPtr<Firebird::IMessageMetadata> m_meta;

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JReplicator FB_FINAL :
	public Firebird::RefCntIface<Firebird::IReplicatorImpl<JReplicator, Firebird::CheckStatusWrapper> >
{
public:
	// IReplicator implementation
	int release() override;
	void process(Firebird::CheckStatusWrapper* status, unsigned length, const unsigned char* data) override;
	void close(Firebird::CheckStatusWrapper* status) override;

public:
	JReplicator(Applier* appl, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	Applier* getHandle() throw()
	{
		return applier;
	}

	void resetHandle()
	{
		applier = NULL;
	}

private:
	Applier* applier;
	Firebird::RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JStatement FB_FINAL :
	public Firebird::RefCntIface<Firebird::IStatementImpl<JStatement, Firebird::CheckStatusWrapper> >
{
public:
	// IStatement implementation
	int release() override;
	void getInfo(Firebird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void free(Firebird::CheckStatusWrapper* status) override;
	ISC_UINT64 getAffectedRecords(Firebird::CheckStatusWrapper* userStatus) override;
	Firebird::IMessageMetadata* getOutputMetadata(Firebird::CheckStatusWrapper* userStatus) override;
	Firebird::IMessageMetadata* getInputMetadata(Firebird::CheckStatusWrapper* userStatus) override;
	unsigned getType(Firebird::CheckStatusWrapper* status) override;
    const char* getPlan(Firebird::CheckStatusWrapper* status, FB_BOOLEAN detailed) override;
	Firebird::ITransaction* execute(Firebird::CheckStatusWrapper* status,
		Firebird::ITransaction* transaction, Firebird::IMessageMetadata* inMetadata, void* inBuffer,
		Firebird::IMessageMetadata* outMetadata, void* outBuffer) override;
	JResultSet* openCursor(Firebird::CheckStatusWrapper* status,
		Firebird::ITransaction* transaction, Firebird::IMessageMetadata* inMetadata, void* inBuffer,
		Firebird::IMessageMetadata* outMetadata, unsigned int flags) override;
	void setCursorName(Firebird::CheckStatusWrapper* status, const char* name) override;
	unsigned getFlags(Firebird::CheckStatusWrapper* status) override;

	unsigned int getTimeout(Firebird::CheckStatusWrapper* status) override;
	void setTimeout(Firebird::CheckStatusWrapper* status, unsigned int timeOut) override;
	JBatch* createBatch(Firebird::CheckStatusWrapper* status, Firebird::IMessageMetadata* inMetadata,
		unsigned parLength, const unsigned char* par) override;

public:
	JStatement(dsql_req* handle, StableAttachmentPart* sa, Firebird::Array<UCHAR>& meta);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	dsql_req* getHandle() throw()
	{
		return statement;
	}

private:
	dsql_req* statement;
	Firebird::RefPtr<StableAttachmentPart> sAtt;
	Firebird::StatementMetadata metadata;

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JRequest FB_FINAL :
	public Firebird::RefCntIface<Firebird::IRequestImpl<JRequest, Firebird::CheckStatusWrapper> >
{
public:
	// IRequest implementation
	int release() override;
	void receive(Firebird::CheckStatusWrapper* status, int level, unsigned int msg_type,
		unsigned int length, void* message) override;
	void send(Firebird::CheckStatusWrapper* status, int level, unsigned int msg_type,
		unsigned int length, const void* message) override;
	void getInfo(Firebird::CheckStatusWrapper* status, int level,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void start(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tra, int level) override;
	void startAndSend(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tra, int level,
		unsigned int msg_type, unsigned int length, const void* message) override;
	void unwind(Firebird::CheckStatusWrapper* status, int level) override;
	void free(Firebird::CheckStatusWrapper* status) override;

public:
	JRequest(JrdStatement* handle, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	JrdStatement* getHandle() throw()
	{
		return rq;
	}

private:
	JrdStatement* rq;
	Firebird::RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JEvents FB_FINAL : public Firebird::RefCntIface<Firebird::IEventsImpl<JEvents, Firebird::CheckStatusWrapper> >
{
public:
	// IEvents implementation
	int release() override;
	void cancel(Firebird::CheckStatusWrapper* status) override;

public:
	JEvents(int aId, StableAttachmentPart* sa, Firebird::IEventCallback* aCallback);

	JEvents* getHandle() throw()
	{
		return this;
	}

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

private:
	int id;
	Firebird::RefPtr<StableAttachmentPart> sAtt;
	Firebird::RefPtr<Firebird::IEventCallback> callback;

	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JAttachment FB_FINAL :
	public Firebird::RefCntIface<Firebird::IAttachmentImpl<JAttachment, Firebird::CheckStatusWrapper> >
{
public:
	// IAttachment implementation
	int release() override;
	void addRef() override;

	void getInfo(Firebird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	JTransaction* startTransaction(Firebird::CheckStatusWrapper* status,
		unsigned int tpbLength, const unsigned char* tpb) override;
	JTransaction* reconnectTransaction(Firebird::CheckStatusWrapper* status,
		unsigned int length, const unsigned char* id) override;
	JRequest* compileRequest(Firebird::CheckStatusWrapper* status,
		unsigned int blr_length, const unsigned char* blr) override;
	void transactRequest(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		unsigned int blr_length, const unsigned char* blr,
		unsigned int in_msg_length, const unsigned char* in_msg,
		unsigned int out_msg_length, unsigned char* out_msg) override;
	JBlob* createBlob(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	JBlob* openBlob(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	int getSlice(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdl_length, const unsigned char* sdl,
		unsigned int param_length, const unsigned char* param,
		int sliceLength, unsigned char* slice) override;
	void putSlice(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdl_length, const unsigned char* sdl,
		unsigned int param_length, const unsigned char* param,
		int sliceLength, unsigned char* slice) override;
	void executeDyn(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		unsigned int length, const unsigned char* dyn) override;
	JStatement* prepare(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tra,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags) override;
	Firebird::ITransaction* execute(Firebird::CheckStatusWrapper* status,
		Firebird::ITransaction* transaction, unsigned int stmtLength, const char* sqlStmt,
		unsigned int dialect, Firebird::IMessageMetadata* inMetadata, void* inBuffer,
		Firebird::IMessageMetadata* outMetadata, void* outBuffer) override;
	Firebird::IResultSet* openCursor(Firebird::CheckStatusWrapper* status,
		Firebird::ITransaction* transaction, unsigned int stmtLength, const char* sqlStmt,
		unsigned int dialect, Firebird::IMessageMetadata* inMetadata, void* inBuffer,
		Firebird::IMessageMetadata* outMetadata, const char* cursorName, unsigned int cursorFlags) override;
	JEvents* queEvents(Firebird::CheckStatusWrapper* status, Firebird::IEventCallback* callback,
		unsigned int length, const unsigned char* events) override;
	void cancelOperation(Firebird::CheckStatusWrapper* status, int option) override;
	void ping(Firebird::CheckStatusWrapper* status) override;
	void detach(Firebird::CheckStatusWrapper* status) override;
	void dropDatabase(Firebird::CheckStatusWrapper* status) override;

	unsigned int getIdleTimeout(Firebird::CheckStatusWrapper* status) override;
	void setIdleTimeout(Firebird::CheckStatusWrapper* status, unsigned int timeOut) override;
	unsigned int getStatementTimeout(Firebird::CheckStatusWrapper* status) override;
	void setStatementTimeout(Firebird::CheckStatusWrapper* status, unsigned int timeOut) override;
	Firebird::IBatch* createBatch(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		unsigned stmtLength, const char* sqlStmt, unsigned dialect,
		Firebird::IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) override;
	Firebird::IReplicator* createReplicator(Firebird::CheckStatusWrapper* status) override;

public:
	explicit JAttachment(StableAttachmentPart* js);

	StableAttachmentPart* getStable() throw()
	{
		return att;
	}

	Jrd::Attachment* getHandle() throw();
	const Jrd::Attachment* getHandle() const throw();

	StableAttachmentPart* getAttachment() throw()
	{
		return att;
	}

	JTransaction* getTransactionInterface(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tra);
	jrd_tra* getEngineTransaction(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tra);

private:
	friend class StableAttachmentPart;

	StableAttachmentPart* att;

	void freeEngineData(Firebird::CheckStatusWrapper* status, bool forceFree);

	void detachEngine()
	{
		att = NULL;
	}
};

class JService FB_FINAL :
	public Firebird::RefCntIface<Firebird::IServiceImpl<JService, Firebird::CheckStatusWrapper> >
{
public:
	// IService implementation
	int release() override;
	void detach(Firebird::CheckStatusWrapper* status) override;
	void query(Firebird::CheckStatusWrapper* status,
		unsigned int sendLength, const unsigned char* sendItems,
		unsigned int receiveLength, const unsigned char* receiveItems,
		unsigned int bufferLength, unsigned char* buffer) override;
	void start(Firebird::CheckStatusWrapper* status,
		unsigned int spbLength, const unsigned char* spb) override;

public:
	explicit JService(Jrd::Service* handle);
	Jrd::Service* svc;

private:
	void freeEngineData(Firebird::CheckStatusWrapper* status);
};

class JProvider FB_FINAL :
	public Firebird::StdPlugin<Firebird::IProviderImpl<JProvider, Firebird::CheckStatusWrapper> >
{
public:
	explicit JProvider(Firebird::IPluginConfig* pConf)
		: cryptCallback(NULL), pluginConfig(pConf)
	{ }

	static JProvider* getInstance()
	{
		JProvider* p = FB_NEW JProvider(NULL);
		p->addRef();
		return p;
	}

	Firebird::ICryptKeyCallback* getCryptCallback()
	{
		return cryptCallback;
	}

	// IProvider implementation
	JAttachment* attachDatabase(Firebird::CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	JAttachment* createDatabase(Firebird::CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	JService* attachServiceManager(Firebird::CheckStatusWrapper* status, const char* service,
		unsigned int spbLength, const unsigned char* spb);
	void shutdown(Firebird::CheckStatusWrapper* status, unsigned int timeout, const int reason);
	void setDbCryptCallback(Firebird::CheckStatusWrapper* status,
		Firebird::ICryptKeyCallback* cryptCb);

private:
	JAttachment* internalAttach(Firebird::CheckStatusWrapper* status, const char* const fileName,
		unsigned int dpbLength, const unsigned char* dpb, const UserId* existingId);
	Firebird::ICryptKeyCallback* cryptCallback;
	Firebird::IPluginConfig* pluginConfig;
};

} // namespace Jrd

#endif // JRD_ENGINE_INTERFACE_H
