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

namespace Firebird::Jrd {

// Engine objects used by interface objects
class blb;
class jrd_tra;
class DsqlCursor;
class DsqlBatch;
class DsqlRequest;
class Statement;
class StableAttachmentPart;
class Attachment;
class Service;
class UserId;

namespace Replication
{
	class Applier;
}

// forward declarations
class JStatement;
class JAttachment;
class JProvider;

class JBlob final :
	public RefCntIface<IBlobImpl<JBlob, CheckStatusWrapper>>
{
public:
	// IBlob implementation
	int release() override;
	void getInfo(CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	int getSegment(CheckStatusWrapper* status, unsigned int length, void* buffer,
		unsigned int* segmentLength) override;
	void putSegment(CheckStatusWrapper* status, unsigned int length, const void* buffer) override;
	void cancel(CheckStatusWrapper* status) override;
	void close(CheckStatusWrapper* status) override;
	int seek(CheckStatusWrapper* status, int mode, int offset) override;			// returns position
	void deprecatedCancel(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;

public:
	JBlob(blb* handle, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	blb* getHandle() noexcept
	{
		return blob;
	}

	void clearHandle()
	{
		blob = NULL;
	}

private:
	blb* blob;
	RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(CheckStatusWrapper* status);
	void internalClose(CheckStatusWrapper* status);
};

class JTransaction final :
	public RefCntIface<ITransactionImpl<JTransaction, CheckStatusWrapper> >
{
public:
	// ITransaction implementation
	int release() override;
	void getInfo(CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void prepare(CheckStatusWrapper* status,
		unsigned int msg_length = 0, const unsigned char* message = 0) override;
	void commit(CheckStatusWrapper* status) override;
	void commitRetaining(CheckStatusWrapper* status) override;
	void rollback(CheckStatusWrapper* status) override;
	void rollbackRetaining(CheckStatusWrapper* status) override;
	void disconnect(CheckStatusWrapper* status) override;
	ITransaction* join(CheckStatusWrapper* status, ITransaction* transaction) override;
	JTransaction* validate(CheckStatusWrapper* status, IAttachment* testAtt) override;
	JTransaction* enterDtc(CheckStatusWrapper* status) override;
	void deprecatedCommit(CheckStatusWrapper* status) override;
	void deprecatedRollback(CheckStatusWrapper* status) override;
	void deprecatedDisconnect(CheckStatusWrapper* status) override;

public:
	JTransaction(jrd_tra* handle, StableAttachmentPart* sa);

	jrd_tra* getHandle() noexcept
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
	RefPtr<StableAttachmentPart> sAtt;

	JTransaction(JTransaction* from);

	void freeEngineData(CheckStatusWrapper* status);
	void internalCommit(CheckStatusWrapper* status);
	void internalRollback(CheckStatusWrapper* status);
	void internalDisconnect(CheckStatusWrapper* status);
};

class JResultSet final :
	public RefCntIface<IResultSetImpl<JResultSet, CheckStatusWrapper> >
{
public:
	// IResultSet implementation
	int release() override;
	int fetchNext(CheckStatusWrapper* status, void* message) override;
	int fetchPrior(CheckStatusWrapper* status, void* message) override;
	int fetchFirst(CheckStatusWrapper* status, void* message) override;
	int fetchLast(CheckStatusWrapper* status, void* message) override;
	int fetchAbsolute(CheckStatusWrapper* status, int position, void* message) override;
	int fetchRelative(CheckStatusWrapper* status, int offset, void* message) override;
	FB_BOOLEAN isEof(CheckStatusWrapper* status) override;
	FB_BOOLEAN isBof(CheckStatusWrapper* status) override;
	IMessageMetadata* getMetadata(CheckStatusWrapper* status) override;
	void close(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;
	void setDelayedOutputFormat(CheckStatusWrapper* status, IMessageMetadata* format) override;
	void getInfo(CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	JResultSet(DsqlCursor* handle, JStatement* aStatement);

	StableAttachmentPart* getAttachment();

	DsqlCursor* getHandle() noexcept
	{
		return cursor;
	}

	void resetHandle()
	{
		cursor = NULL;
	}

private:
	DsqlCursor* cursor;
	RefPtr<JStatement> statement;
	int state;

	void freeEngineData(CheckStatusWrapper* status);
};

class JBatch final :
	public RefCntIface<IBatchImpl<JBatch, CheckStatusWrapper> >
{
public:
	// IBatch implementation
	int release() override;
	void add(CheckStatusWrapper* status, unsigned count, const void* inBuffer) override;
	void addBlob(CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
		unsigned parLength, const unsigned char* par) override;
	void appendBlobData(CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void addBlobStream(CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void registerBlob(CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) override;
	IBatchCompletionState* execute(CheckStatusWrapper* status,
		ITransaction* transaction) override;
	void cancel(CheckStatusWrapper* status) override;
	unsigned getBlobAlignment(CheckStatusWrapper* status) override;
	IMessageMetadata* getMetadata(CheckStatusWrapper* status) override;
	void setDefaultBpb(CheckStatusWrapper* status, unsigned parLength, const unsigned char* par) override;
	void close(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;
	void getInfo(CheckStatusWrapper* status, unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	JBatch(DsqlBatch* handle, JStatement* aStatement, IMessageMetadata* aMetadata);

	StableAttachmentPart* getAttachment();

	DsqlBatch* getHandle() noexcept
	{
		return batch;
	}

	void resetHandle()
	{
		batch = NULL;
	}

private:
	DsqlBatch* batch;
	RefPtr<JStatement> statement;
	RefPtr<IMessageMetadata> m_meta;

	void freeEngineData(CheckStatusWrapper* status);
};

class JReplicator final :
	public RefCntIface<IReplicatorImpl<JReplicator, CheckStatusWrapper> >
{
public:
	// IReplicator implementation
	int release() override;
	void process(CheckStatusWrapper* status, unsigned length, const unsigned char* data) override;
	void close(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;

public:
	JReplicator(Replication::Applier* appl, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	Replication::Applier* getHandle() noexcept
	{
		return applier;
	}

	void resetHandle()
	{
		applier = NULL;
	}

private:
	Replication::Applier* applier;
	RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(CheckStatusWrapper* status);
};

class JStatement final :
	public RefCntIface<IStatementImpl<JStatement, CheckStatusWrapper> >
{
public:
	// IStatement implementation
	int release() override;
	void getInfo(CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void free(CheckStatusWrapper* status) override;
	void deprecatedFree(CheckStatusWrapper* status) override;
	ISC_UINT64 getAffectedRecords(CheckStatusWrapper* userStatus) override;
	IMessageMetadata* getOutputMetadata(CheckStatusWrapper* userStatus) override;
	IMessageMetadata* getInputMetadata(CheckStatusWrapper* userStatus) override;
	unsigned getType(CheckStatusWrapper* status) override;
    const char* getPlan(CheckStatusWrapper* status, FB_BOOLEAN detailed) override;
	ITransaction* execute(CheckStatusWrapper* status,
		ITransaction* transaction, IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, void* outBuffer) override;
	JResultSet* openCursor(CheckStatusWrapper* status,
		ITransaction* transaction, IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, unsigned int flags) override;
	void setCursorName(CheckStatusWrapper* status, const char* name) override;
	unsigned getFlags(CheckStatusWrapper* status) override;

	unsigned int getTimeout(CheckStatusWrapper* status) override;
	void setTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;
	JBatch* createBatch(CheckStatusWrapper* status, IMessageMetadata* inMetadata,
		unsigned parLength, const unsigned char* par) override;

	unsigned getMaxInlineBlobSize(CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(CheckStatusWrapper* status, unsigned size) override;

public:
	JStatement(DsqlRequest* handle, StableAttachmentPart* sa, Array<UCHAR>& meta);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	DsqlRequest* getHandle() noexcept
	{
		return statement;
	}

private:
	DsqlRequest* statement;
	RefPtr<StableAttachmentPart> sAtt;
	StatementMetadata metadata;

	void freeEngineData(CheckStatusWrapper* status);
};

class JRequest final :
	public RefCntIface<IRequestImpl<JRequest, CheckStatusWrapper> >
{
public:
	// IRequest implementation
	int release() override;
	void receive(CheckStatusWrapper* status, int level, unsigned int msg_type,
		unsigned int length, void* message) override;
	void send(CheckStatusWrapper* status, int level, unsigned int msg_type,
		unsigned int length, const void* message) override;
	void getInfo(CheckStatusWrapper* status, int level,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void start(CheckStatusWrapper* status, ITransaction* tra, int level) override;
	void startAndSend(CheckStatusWrapper* status, ITransaction* tra, int level,
		unsigned int msg_type, unsigned int length, const void* message) override;
	void unwind(CheckStatusWrapper* status, int level) override;
	void free(CheckStatusWrapper* status) override;
	void deprecatedFree(CheckStatusWrapper* status) override;

public:
	JRequest(Statement* handle, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	Statement* getHandle() noexcept
	{
		return rq;
	}

private:
	Statement* rq;
	RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(CheckStatusWrapper* status);
};

class JEvents final : public RefCntIface<IEventsImpl<JEvents, CheckStatusWrapper> >
{
public:
	// IEvents implementation
	int release() override;
	void cancel(CheckStatusWrapper* status) override;
	void deprecatedCancel(CheckStatusWrapper* status) override;

public:
	JEvents(int aId, StableAttachmentPart* sa, IEventCallback* aCallback);

	JEvents* getHandle() noexcept
	{
		return this;
	}

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

private:
	int id;
	RefPtr<StableAttachmentPart> sAtt;
	RefPtr<IEventCallback> callback;

	void freeEngineData(CheckStatusWrapper* status);
};

class JAttachment final :
	public RefCntIface<IAttachmentImpl<JAttachment, CheckStatusWrapper> >
{
public:
	// IAttachment implementation
	int release() override;
	void addRef() override;

	void getInfo(CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	JTransaction* startTransaction(CheckStatusWrapper* status,
		unsigned int tpbLength, const unsigned char* tpb) override;
	JTransaction* reconnectTransaction(CheckStatusWrapper* status,
		unsigned int length, const unsigned char* id) override;
	JRequest* compileRequest(CheckStatusWrapper* status,
		unsigned int blr_length, const unsigned char* blr) override;
	void transactRequest(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int blr_length, const unsigned char* blr,
		unsigned int in_msg_length, const unsigned char* in_msg,
		unsigned int out_msg_length, unsigned char* out_msg) override;
	JBlob* createBlob(CheckStatusWrapper* status, ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	JBlob* openBlob(CheckStatusWrapper* status, ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	int getSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdl_length, const unsigned char* sdl,
		unsigned int param_length, const unsigned char* param,
		int sliceLength, unsigned char* slice) override;
	void putSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdl_length, const unsigned char* sdl,
		unsigned int param_length, const unsigned char* param,
		int sliceLength, unsigned char* slice) override;
	void executeDyn(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int length, const unsigned char* dyn) override;
	JStatement* prepare(CheckStatusWrapper* status, ITransaction* tra,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags) override;
	ITransaction* execute(CheckStatusWrapper* status,
		ITransaction* transaction, unsigned int stmtLength, const char* sqlStmt,
		unsigned int dialect, IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, void* outBuffer) override;
	IResultSet* openCursor(CheckStatusWrapper* status,
		ITransaction* transaction, unsigned int stmtLength, const char* sqlStmt,
		unsigned int dialect, IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, const char* cursorName, unsigned int cursorFlags) override;
	JEvents* queEvents(CheckStatusWrapper* status, IEventCallback* callback,
		unsigned int length, const unsigned char* events) override;
	void cancelOperation(CheckStatusWrapper* status, int option) override;
	void ping(CheckStatusWrapper* status) override;
	void detach(CheckStatusWrapper* status) override;
	void dropDatabase(CheckStatusWrapper* status) override;
	void deprecatedDetach(CheckStatusWrapper* status) override;
	void deprecatedDropDatabase(CheckStatusWrapper* status) override;

	unsigned int getIdleTimeout(CheckStatusWrapper* status) override;
	void setIdleTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;
	unsigned int getStatementTimeout(CheckStatusWrapper* status) override;
	void setStatementTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;
	IBatch* createBatch(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned stmtLength, const char* sqlStmt, unsigned dialect,
		IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) override;
	IReplicator* createReplicator(CheckStatusWrapper* status) override;

	unsigned getMaxBlobCacheSize(CheckStatusWrapper* status) override;
	void setMaxBlobCacheSize(CheckStatusWrapper* status, unsigned size) override;
	unsigned getMaxInlineBlobSize(CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(CheckStatusWrapper* status, unsigned size) override;
public:
	explicit JAttachment(StableAttachmentPart* js);

	StableAttachmentPart* getStable() noexcept
	{
		return att;
	}

	Jrd::Attachment* getHandle() noexcept;
	const Jrd::Attachment* getHandle() const noexcept;

	StableAttachmentPart* getAttachment() noexcept
	{
		return att;
	}

	JTransaction* getTransactionInterface(CheckStatusWrapper* status, ITransaction* tra);
	jrd_tra* getEngineTransaction(CheckStatusWrapper* status, ITransaction* tra);

private:
	friend class StableAttachmentPart;

	StableAttachmentPart* att;

	void freeEngineData(CheckStatusWrapper* status, bool forceFree);

	void detachEngine()
	{
		att = NULL;
	}

	void internalDetach(CheckStatusWrapper* status);
	void internalDropDatabase(CheckStatusWrapper* status);
};

class JService final :
	public RefCntIface<IServiceImpl<JService, CheckStatusWrapper> >
{
public:
	// IService implementation
	int release() override;
	void detach(CheckStatusWrapper* status) override;
	void deprecatedDetach(CheckStatusWrapper* status) override;
	void query(CheckStatusWrapper* status,
		unsigned int sendLength, const unsigned char* sendItems,
		unsigned int receiveLength, const unsigned char* receiveItems,
		unsigned int bufferLength, unsigned char* buffer) override;
	void start(CheckStatusWrapper* status,
		unsigned int spbLength, const unsigned char* spb) override;
	void cancel(CheckStatusWrapper* status) override;

public:
	explicit JService(Jrd::Service* handle);
	Jrd::Service* svc;

private:
	void freeEngineData(CheckStatusWrapper* status);
};

class JProvider final :
	public StdPlugin<IProviderImpl<JProvider, CheckStatusWrapper> >
{
public:
	explicit JProvider(IPluginConfig* pConf)
		: cryptCallback(NULL), pluginConfig(pConf)
	{ }

	static JProvider* getInstance()
	{
		JProvider* p = FB_NEW JProvider(NULL);
		p->addRef();
		return p;
	}

	ICryptKeyCallback* getCryptCallback()
	{
		return cryptCallback;
	}

	// IProvider implementation
	JAttachment* attachDatabase(CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	JAttachment* createDatabase(CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	JService* attachServiceManager(CheckStatusWrapper* status, const char* service,
		unsigned int spbLength, const unsigned char* spb);
	void shutdown(CheckStatusWrapper* status, unsigned int timeout, const int reason);
	void setDbCryptCallback(CheckStatusWrapper* status,
		ICryptKeyCallback* cryptCb);

private:
	JAttachment* internalAttach(CheckStatusWrapper* status, const char* const fileName,
		unsigned int dpbLength, const unsigned char* dpb, const UserId* existingId);
	ICryptKeyCallback* cryptCallback;
	IPluginConfig* pluginConfig;
};

} // namespace Firebird::Jrd

#endif // JRD_ENGINE_INTERFACE_H
