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
 * Dmitry Yemanov
 * Sean Leyne
 * Alex Peshkoff
 * Adriano dos Santos Fernandes
 *
 */

#ifndef YVALVE_Y_OBJECTS_H
#define YVALVE_Y_OBJECTS_H

#include "firebird.h"
#include "firebird/Interface.h"
#include "iberror.h"
#include "../common/StatusHolder.h"
#include "../common/classes/fb_atomic.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/MsgMetadata.h"
#include "../common/classes/ClumpletWriter.h"

#include <functional>

namespace Why
{


class YAttachment;
class YBlob;
class YRequest;
class YResultSet;
class YService;
class YStatement;
class IscStatement;
class YTransaction;
class Dispatcher;

class YObject
{
public:
	YObject() noexcept
		: handle(0)
	{
	}

protected:
	FB_API_HANDLE handle;
};

class CleanupCallback
{
public:
	virtual void cleanupCallbackFunction() = 0;
	virtual ~CleanupCallback() { }
};

template <typename T>
class HandleArray
{
public:
	explicit HandleArray(Firebird::MemoryPool& pool)
		: array(pool)
	{
	}

	void add(T* obj)
	{
		Firebird::MutexLockGuard guard(mtx, FB_FUNCTION);

		array.add(obj);
	}

	void remove(T* obj)
	{
		Firebird::MutexLockGuard guard(mtx, FB_FUNCTION);
		FB_SIZE_T pos;

		if (array.find(obj, pos))
			array.remove(pos);
	}

	void destroy(unsigned dstrFlags)
	{
		Firebird::MutexLockGuard guard(mtx, FB_FUNCTION);

		// Call destroy() only once even if handle is not removed from array
		// by this call for any reason
		for (int i = array.getCount() - 1; i >= 0; i--)
			array[i]->destroy(dstrFlags);

		clear();
	}

	void assign(HandleArray& from)
	{
		clear();
		array.assign(from.array);
	}

	void clear()
	{
		array.clear();
	}

private:
	Firebird::Mutex mtx;
	Firebird::SortedArray<T*> array;
};

template <typename Impl, typename Intf>
class YHelper : public Firebird::RefCntIface<Intf>, public YObject
{
public:
	typedef typename Intf::Declaration NextInterface;
	typedef YAttachment YRef;

	static constexpr unsigned DF_RELEASE =		0x1;
	static constexpr unsigned DF_KEEP_NEXT =	0x2;

	explicit YHelper(NextInterface* aNext, const char* m = NULL)
		:
#ifdef DEV_BUILD
		  Firebird::RefCntIface<Intf>(m),
#endif
		  next(Firebird::REF_NO_INCR, aNext)
	{ }

	int release() override
	{
		int rc = --this->refCounter;
//		this->refCntDPrt('-');
		if (rc == 0)
		{
			if (next)
				destroy(0);
			delete this;
		}

		return rc;
	}

	virtual void destroy(unsigned dstrFlags) = 0;

	void destroy2(unsigned dstrFlags)
	{
		if (dstrFlags & DF_KEEP_NEXT)
			next.clear();
		else
			next = NULL;

		if (dstrFlags & DF_RELEASE)
		{
			this->release();
		}
	}

	Firebird::RefPtr<NextInterface> next;
};

template <class YT>
class AtomicYPtr
{
public:
	AtomicYPtr(YT* v) noexcept
	{
		atmPtr.store(v, std::memory_order_relaxed);
	}

	YT* get() noexcept
	{
		return atmPtr.load(std::memory_order_relaxed);
	}

	YT* release()
	{
		YT* v = atmPtr;
		if (v && atmPtr.compare_exchange_strong(v, nullptr))
			return v;
		return nullptr;
	}

private:
	std::atomic<YT*> atmPtr;
};

typedef AtomicYPtr<YAttachment> AtomicAttPtr;
typedef AtomicYPtr<YTransaction> AtomicTraPtr;

class YEvents final :
	public YHelper<YEvents, Firebird::IEventsImpl<YEvents, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_events_handle;

	YEvents(YAttachment* aAttachment, Firebird::IEvents* aNext, Firebird::IEventCallback* aCallback);

	void destroy(unsigned dstrFlags) override;
	FB_API_HANDLE& getHandle();

	// IEvents implementation
	void cancel(Firebird::CheckStatusWrapper* status) override;
	void deprecatedCancel(Firebird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	Firebird::RefPtr<Firebird::IEventCallback> callback;

private:
	Firebird::AtomicCounter destroyed;
};

class YRequest final :
	public YHelper<YRequest, Firebird::IRequestImpl<YRequest, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_req_handle;

	YRequest(YAttachment* aAttachment, Firebird::IRequest* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_req_handle& getHandle();

	// IRequest implementation
	void receive(Firebird::CheckStatusWrapper* status, int level, unsigned int msgType,
		unsigned int length, void* message) override;
	void send(Firebird::CheckStatusWrapper* status, int level, unsigned int msgType,
		unsigned int length, const void* message) override;
	void getInfo(Firebird::CheckStatusWrapper* status, int level, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	void start(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, int level) override;
	void startAndSend(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, int level,
		unsigned int msgType, unsigned int length, const void* message) override;
	void unwind(Firebird::CheckStatusWrapper* status, int level) override;
	void free(Firebird::CheckStatusWrapper* status) override;
	void deprecatedFree(Firebird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	isc_req_handle* userHandle;
};

class YTransaction final :
	public YHelper<YTransaction, Firebird::ITransactionImpl<YTransaction, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_trans_handle;

	YTransaction(YAttachment* aAttachment, Firebird::ITransaction* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_tr_handle& getHandle();

	// ITransaction implementation
	void getInfo(Firebird::CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	void prepare(Firebird::CheckStatusWrapper* status, unsigned int msgLength,
		const unsigned char* message) override;
	void commit(Firebird::CheckStatusWrapper* status) override;
	void commitRetaining(Firebird::CheckStatusWrapper* status) override;
	void rollback(Firebird::CheckStatusWrapper* status) override;
	void rollbackRetaining(Firebird::CheckStatusWrapper* status) override;
	void disconnect(Firebird::CheckStatusWrapper* status) override;
	Firebird::ITransaction* join(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction) override;
	Firebird::ITransaction* validate(Firebird::CheckStatusWrapper* status, Firebird::IAttachment* testAtt) override;
	YTransaction* enterDtc(Firebird::CheckStatusWrapper* status) override;
	void deprecatedCommit(Firebird::CheckStatusWrapper* status) override;
	void deprecatedRollback(Firebird::CheckStatusWrapper* status) override;
	void deprecatedDisconnect(Firebird::CheckStatusWrapper* status) override;

	void addCleanupHandler(Firebird::CheckStatusWrapper* status, CleanupCallback* callback);
	void selfCheck();

public:
	AtomicAttPtr attachment;
	HandleArray<YBlob> childBlobs;
	HandleArray<YResultSet> childCursors;
	Firebird::Array<CleanupCallback*> cleanupHandlers;

private:
	YTransaction(YTransaction* from)
		: YHelper(from->next),
		  attachment(from->attachment.get()),
		  childBlobs(getPool()),
		  childCursors(getPool()),
		  cleanupHandlers(getPool())
	{
		childBlobs.assign(from->childBlobs);
		from->childBlobs.clear();
		childCursors.assign(from->childCursors);
		from->childCursors.clear();
		cleanupHandlers.assign(from->cleanupHandlers);
		from->cleanupHandlers.clear();
	}
};

typedef Firebird::RefPtr<Firebird::ITransaction> NextTransaction;

class YBlob final :
	public YHelper<YBlob, Firebird::IBlobImpl<YBlob, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_segstr_handle;

	YBlob(YAttachment* aAttachment, YTransaction* aTransaction, Firebird::IBlob* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_blob_handle& getHandle();

	// IBlob implementation
	void getInfo(Firebird::CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	int getSegment(Firebird::CheckStatusWrapper* status, unsigned int length, void* buffer,
								   unsigned int* segmentLength) override;
	void putSegment(Firebird::CheckStatusWrapper* status, unsigned int length, const void* buffer) override;
	void cancel(Firebird::CheckStatusWrapper* status) override;
	void close(Firebird::CheckStatusWrapper* status) override;
	int seek(Firebird::CheckStatusWrapper* status, int mode, int offset) override;
	void deprecatedCancel(Firebird::CheckStatusWrapper* status) override;
	void deprecatedClose(Firebird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	AtomicTraPtr transaction;
};

class YResultSet final :
	public YHelper<YResultSet, Firebird::IResultSetImpl<YResultSet, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_result_set;

	YResultSet(YAttachment* anAttachment, YTransaction* aTransaction, Firebird::IResultSet* aNext);
	YResultSet(YAttachment* anAttachment, YTransaction* aTransaction, YStatement* aStatement,
		Firebird::IResultSet* aNext);

	void destroy(unsigned dstrFlags) override;

	// IResultSet implementation
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
	void deprecatedClose(Firebird::CheckStatusWrapper* status) override;
	void setDelayedOutputFormat(Firebird::CheckStatusWrapper* status, Firebird::IMessageMetadata* format) override;
	void getInfo(Firebird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	AtomicAttPtr attachment;
	AtomicTraPtr transaction;
	YStatement* statement;
};

class YBatch final :
	public YHelper<YBatch, Firebird::IBatchImpl<YBatch, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_result_set;	// isc_bad_batch

	YBatch(YAttachment* anAttachment, Firebird::IBatch* aNext);

	void destroy(unsigned dstrFlags) override;

	// IBatch implementation
	void add(Firebird::CheckStatusWrapper* status, unsigned count, const void* inBuffer) override;
	void addBlob(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
		unsigned parLength, const unsigned char* par) override;
	void appendBlobData(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void addBlobStream(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	unsigned getBlobAlignment(Firebird::CheckStatusWrapper* status) override;
	Firebird::IMessageMetadata* getMetadata(Firebird::CheckStatusWrapper* status) override;
	void registerBlob(Firebird::CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) override;
	Firebird::IBatchCompletionState* execute(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction) override;
	void cancel(Firebird::CheckStatusWrapper* status) override;
	void setDefaultBpb(Firebird::CheckStatusWrapper* status, unsigned parLength, const unsigned char* par) override;
	void close(Firebird::CheckStatusWrapper* status) override;
	void deprecatedClose(Firebird::CheckStatusWrapper* status) override;
	void getInfo(Firebird::CheckStatusWrapper* status, unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	AtomicAttPtr attachment;
};


class YReplicator final :
	public YHelper<YReplicator, Firebird::IReplicatorImpl<YReplicator, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_repl_handle;

	YReplicator(YAttachment* anAttachment, Firebird::IReplicator* aNext);

	void destroy(unsigned dstrFlags) override;

	// IReplicator implementation
	void process(Firebird::CheckStatusWrapper* status, unsigned length, const unsigned char* data) override;
	void close(Firebird::CheckStatusWrapper* status) override;
	void deprecatedClose(Firebird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
};


class YMetadata
{
public:
	explicit YMetadata(bool in) noexcept
		: flag(false), input(in)
	{ }

	Firebird::IMessageMetadata* get(Firebird::IStatement* next, YStatement* statement);

private:
	Firebird::RefPtr<Firebird::MsgMetadata> metadata;
	volatile bool flag;
	bool input;
};

class YStatement final :
	public YHelper<YStatement, Firebird::IStatementImpl<YStatement, Firebird::CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_stmt_handle;

	YStatement(YAttachment* aAttachment, Firebird::IStatement* aNext);

	void destroy(unsigned dstrFlags) override;

	// IStatement implementation
	void getInfo(Firebird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	unsigned getType(Firebird::CheckStatusWrapper* status) override;
	const char* getPlan(Firebird::CheckStatusWrapper* status, FB_BOOLEAN detailed) override;
	ISC_UINT64 getAffectedRecords(Firebird::CheckStatusWrapper* status) override;
	Firebird::IMessageMetadata* getInputMetadata(Firebird::CheckStatusWrapper* status) override;
	Firebird::IMessageMetadata* getOutputMetadata(Firebird::CheckStatusWrapper* status) override;
	Firebird::ITransaction* execute(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		Firebird::IMessageMetadata* inMetadata, void* inBuffer,
		Firebird::IMessageMetadata* outMetadata, void* outBuffer) override;
	Firebird::IResultSet* openCursor(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		Firebird::IMessageMetadata* inMetadata, void* inBuffer, Firebird::IMessageMetadata* outMetadata,
		unsigned int flags) override;
	void setCursorName(Firebird::CheckStatusWrapper* status, const char* name) override;
	void free(Firebird::CheckStatusWrapper* status) override;
	void deprecatedFree(Firebird::CheckStatusWrapper* status) override;
	unsigned getFlags(Firebird::CheckStatusWrapper* status) override;

	unsigned int getTimeout(Firebird::CheckStatusWrapper* status) override;
	void setTimeout(Firebird::CheckStatusWrapper* status, unsigned int timeOut) override;
	YBatch* createBatch(Firebird::CheckStatusWrapper* status, Firebird::IMessageMetadata* inMetadata,
		unsigned parLength, const unsigned char* par) override;

	unsigned getMaxInlineBlobSize(Firebird::CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(Firebird::CheckStatusWrapper* status, unsigned size) override;

public:
	AtomicAttPtr attachment;
	Firebird::Mutex statementMutex;
	YResultSet* cursor;

	Firebird::IMessageMetadata* getMetadata(bool in, Firebird::IStatement* next);

private:
	YMetadata input, output;
};

class EnterCount
{
public:
	EnterCount() noexcept
		: enterCount(0)
	{}

	~EnterCount()
	{
		fb_assert(enterCount == 0);
	}

	int enterCount;
	Firebird::Mutex enterMutex;
};

class YAttachment final :
	public YHelper<YAttachment, Firebird::IAttachmentImpl<YAttachment, Firebird::CheckStatusWrapper> >,
	public EnterCount
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_db_handle;

	YAttachment(Firebird::IProvider* aProvider, Firebird::IAttachment* aNext,
		const Firebird::PathName& aDbPath);
	~YAttachment();

	void destroy(unsigned dstrFlags) override;
	void shutdown();
	isc_db_handle& getHandle() noexcept;
	void getOdsVersion(USHORT* majorVersion, USHORT* minorVersion);

	// IAttachment implementation
	void getInfo(Firebird::CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	YTransaction* startTransaction(Firebird::CheckStatusWrapper* status, unsigned int tpbLength,
		const unsigned char* tpb) override;
	YTransaction* reconnectTransaction(Firebird::CheckStatusWrapper* status, unsigned int length,
		const unsigned char* id) override;
	YRequest* compileRequest(Firebird::CheckStatusWrapper* status, unsigned int blrLength,
		const unsigned char* blr) override;
	void transactRequest(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		unsigned int blrLength, const unsigned char* blr, unsigned int inMsgLength,
		const unsigned char* inMsg, unsigned int outMsgLength, unsigned char* outMsg) override;
	YBlob* createBlob(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int bpbLength, const unsigned char* bpb) override;
	YBlob* openBlob(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int bpbLength, const unsigned char* bpb) override;
	int getSlice(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
		const unsigned char* param, int sliceLength, unsigned char* slice) override;
	void putSlice(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
		const unsigned char* param, int sliceLength, unsigned char* slice) override;
	void executeDyn(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction, unsigned int length,
		const unsigned char* dyn) override;
	YStatement* prepare(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tra,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags) override;
	Firebird::ITransaction* execute(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		Firebird::IMessageMetadata* inMetadata, void* inBuffer,
		Firebird::IMessageMetadata* outMetadata, void* outBuffer) override;
	Firebird::IResultSet* openCursor(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		Firebird::IMessageMetadata* inMetadata, void* inBuffer, Firebird::IMessageMetadata* outMetadata,
		const char* cursorName, unsigned int cursorFlags) override;
	YEvents* queEvents(Firebird::CheckStatusWrapper* status, Firebird::IEventCallback* callback,
		unsigned int length, const unsigned char* eventsData) override;
	void cancelOperation(Firebird::CheckStatusWrapper* status, int option) override;
	void ping(Firebird::CheckStatusWrapper* status) override;
	void detach(Firebird::CheckStatusWrapper* status) override;
	void dropDatabase(Firebird::CheckStatusWrapper* status) override;
	void deprecatedDetach(Firebird::CheckStatusWrapper* status) override;
	void deprecatedDropDatabase(Firebird::CheckStatusWrapper* status) override;

	void addCleanupHandler(Firebird::CheckStatusWrapper* status, CleanupCallback* callback);
	YTransaction* getTransaction(Firebird::ITransaction* tra);
	void getNextTransaction(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tra, NextTransaction& next);
	void execute(Firebird::CheckStatusWrapper* status, isc_tr_handle* traHandle,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		Firebird::IMessageMetadata* inMetadata, void* inBuffer,
		Firebird::IMessageMetadata* outMetadata, void* outBuffer);

	unsigned int getIdleTimeout(Firebird::CheckStatusWrapper* status) override;
	void setIdleTimeout(Firebird::CheckStatusWrapper* status, unsigned int timeOut) override;
	unsigned int getStatementTimeout(Firebird::CheckStatusWrapper* status) override;
	void setStatementTimeout(Firebird::CheckStatusWrapper* status, unsigned int timeOut) override;
	YBatch* createBatch(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction,
		unsigned stmtLength, const char* sqlStmt, unsigned dialect,
		Firebird::IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) override;
	YReplicator* createReplicator(Firebird::CheckStatusWrapper* status) override;

	unsigned getMaxBlobCacheSize(Firebird::CheckStatusWrapper* status) override;
	void setMaxBlobCacheSize(Firebird::CheckStatusWrapper* status, unsigned size) override;
	unsigned getMaxInlineBlobSize(Firebird::CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(Firebird::CheckStatusWrapper* status, unsigned size) override;

public:
	Firebird::IProvider* provider;
	Firebird::PathName dbPath;
	HandleArray<YBlob> childBlobs;
	HandleArray<YEvents> childEvents;
	HandleArray<YRequest> childRequests;
	HandleArray<YStatement> childStatements;
	HandleArray<IscStatement> childIscStatements;
	HandleArray<YTransaction> childTransactions;
	Firebird::Array<CleanupCallback*> cleanupHandlers;
	Firebird::StatusHolder savedStatus;	// Do not use raise() method of this class in yValve.

private:
	USHORT cachedOdsMajorVersion = 0;
	USHORT cachedOdsMinorVersion = 0;
};

class YService final :
	public YHelper<YService, Firebird::IServiceImpl<YService, Firebird::CheckStatusWrapper> >,
	public EnterCount
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_svc_handle;

	YService(Firebird::IProvider* aProvider, Firebird::IService* aNext, bool utf8, Dispatcher* yProvider);
	~YService();

	void shutdown();
	void destroy(unsigned dstrFlags) override;
	isc_svc_handle& getHandle() noexcept;

	// IService implementation
	void detach(Firebird::CheckStatusWrapper* status) override;
	void deprecatedDetach(Firebird::CheckStatusWrapper* status) override;
	void query(Firebird::CheckStatusWrapper* status,
		unsigned int sendLength, const unsigned char* sendItems,
		unsigned int receiveLength, const unsigned char* receiveItems,
		unsigned int bufferLength, unsigned char* buffer) override;
	void start(Firebird::CheckStatusWrapper* status,
		unsigned int spbLength, const unsigned char* spb) override;
	void cancel(Firebird::CheckStatusWrapper* status) override;

public:
	typedef Firebird::IService NextInterface;
	typedef YService YRef;

private:
	Firebird::IProvider* provider;
	bool utf8Connection;		// Client talks to us using UTF8, else - system default charset

public:
	Firebird::RefPtr<IService> alternativeHandle;
	Firebird::ClumpletWriter attachSpb;
	Firebird::RefPtr<Dispatcher> ownProvider;
};

class Dispatcher final :
	public Firebird::StdPlugin<Firebird::IProviderImpl<Dispatcher, Firebird::CheckStatusWrapper> >
{
public:
	Dispatcher() noexcept
		: cryptCallback(NULL)
	{ }

	// IProvider implementation
	YAttachment* attachDatabase(Firebird::CheckStatusWrapper* status, const char* filename,
		unsigned int dpbLength, const unsigned char* dpb) override;
	YAttachment* createDatabase(Firebird::CheckStatusWrapper* status, const char* filename,
		unsigned int dpbLength, const unsigned char* dpb) override;
	YService* attachServiceManager(Firebird::CheckStatusWrapper* status, const char* serviceName,
		unsigned int spbLength, const unsigned char* spb) override;
	void shutdown(Firebird::CheckStatusWrapper* status, unsigned int timeout, const int reason) override;
	void setDbCryptCallback(Firebird::CheckStatusWrapper* status,
		Firebird::ICryptKeyCallback* cryptCallback) override;

	void destroy(unsigned) noexcept
	{ }

public:
	Firebird::IService* internalServiceAttach(Firebird::CheckStatusWrapper* status,
		const Firebird::PathName& svcName, Firebird::ClumpletReader& spb,
		std::function<void(Firebird::CheckStatusWrapper*, Firebird::IService*)> start,
		Firebird::IProvider** retProvider);

private:
	YAttachment* attachOrCreateDatabase(Firebird::CheckStatusWrapper* status, bool createFlag,
		const char* filename, unsigned int dpbLength, const unsigned char* dpb);

	Firebird::ICryptKeyCallback* cryptCallback;
};

class UtilInterface final :
	public Firebird::AutoIface<Firebird::IUtilImpl<UtilInterface, Firebird::CheckStatusWrapper> >
{
	// IUtil implementation
public:
	void getFbVersion(Firebird::CheckStatusWrapper* status, Firebird::IAttachment* att,
		Firebird::IVersionCallback* callback) override;
	void loadBlob(Firebird::CheckStatusWrapper* status, ISC_QUAD* blobId, Firebird::IAttachment* att,
		Firebird::ITransaction* tra, const char* file, FB_BOOLEAN txt) override;
	void dumpBlob(Firebird::CheckStatusWrapper* status, ISC_QUAD* blobId, Firebird::IAttachment* att,
		Firebird::ITransaction* tra, const char* file, FB_BOOLEAN txt) override;
	void getPerfCounters(Firebird::CheckStatusWrapper* status, Firebird::IAttachment* att,
		const char* countersSet, ISC_INT64* counters) override;			// in perf.cpp

	YAttachment* executeCreateDatabase(Firebird::CheckStatusWrapper* status,
		unsigned stmtLength, const char* creatDBstatement, unsigned dialect,
		FB_BOOLEAN* stmtIsCreateDb = nullptr) override
	{
		return executeCreateDatabase2(status, stmtLength, creatDBstatement, dialect,
			0, nullptr, stmtIsCreateDb);
	}

	YAttachment* executeCreateDatabase2(Firebird::CheckStatusWrapper* status,
		unsigned stmtLength, const char* creatDBstatement, unsigned dialect,
		unsigned dpbLength, const unsigned char* dpb,
		FB_BOOLEAN* stmtIsCreateDb = nullptr) override;

	void decodeDate(ISC_DATE date, unsigned* year, unsigned* month, unsigned* day) override;
	void decodeTime(ISC_TIME time,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions) override;
	ISC_DATE encodeDate(unsigned year, unsigned month, unsigned day) override;
	ISC_TIME encodeTime(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) override;
	unsigned formatStatus(char* buffer, unsigned bufferSize, Firebird::IStatus* status) override;
	unsigned getClientVersion() override;
	Firebird::IXpbBuilder* getXpbBuilder(Firebird::CheckStatusWrapper* status,
		unsigned kind, const unsigned char* buf, unsigned len) override;
	unsigned setOffsets(Firebird::CheckStatusWrapper* status, Firebird::IMessageMetadata* metadata,
		Firebird::IOffsetsCallback* callback) override;
	Firebird::IDecFloat16* getDecFloat16(Firebird::CheckStatusWrapper* status) override;
	Firebird::IDecFloat34* getDecFloat34(Firebird::CheckStatusWrapper* status) override;
	void decodeTimeTz(Firebird::CheckStatusWrapper* status, const ISC_TIME_TZ* timeTz,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
		unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void decodeTimeStampTz(Firebird::CheckStatusWrapper* status, const ISC_TIMESTAMP_TZ* timeStampTz,
		unsigned* year, unsigned* month, unsigned* day, unsigned* hours, unsigned* minutes, unsigned* seconds,
		unsigned* fractions, unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void encodeTimeTz(Firebird::CheckStatusWrapper* status, ISC_TIME_TZ* timeTz,
		unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions, const char* timeZone) override;
	void encodeTimeStampTz(Firebird::CheckStatusWrapper* status, ISC_TIMESTAMP_TZ* timeStampTz,
		unsigned year, unsigned month, unsigned day,
		unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions, const char* timeZone) override;
	Firebird::IInt128* getInt128(Firebird::CheckStatusWrapper* status) override;
	void decodeTimeTzEx(Firebird::CheckStatusWrapper* status, const ISC_TIME_TZ_EX* timeEx,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
		unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void decodeTimeStampTzEx(Firebird::CheckStatusWrapper* status, const ISC_TIMESTAMP_TZ_EX* timeStampEx,
		unsigned* year, unsigned* month, unsigned* day, unsigned* hours, unsigned* minutes, unsigned* seconds,
		unsigned* fractions, unsigned timeZoneBufferLength, char* timeZoneBuffer) override;

	void convert(Firebird::CheckStatusWrapper* status,
		unsigned sourceType, unsigned sourceScale, unsigned sourceLength, const void* source,
		unsigned targetType, unsigned targetScale, unsigned targetLength, void* target) override;

};

}	// namespace Why

#endif	// YVALVE_Y_OBJECTS_H
