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

namespace Firebird::Why
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
	explicit HandleArray(MemoryPool& pool)
		: array(pool)
	{
	}

	void add(T* obj)
	{
		MutexLockGuard guard(mtx, FB_FUNCTION);

		array.add(obj);
	}

	void remove(T* obj)
	{
		MutexLockGuard guard(mtx, FB_FUNCTION);
		FB_SIZE_T pos;

		if (array.find(obj, pos))
			array.remove(pos);
	}

	void destroy(unsigned dstrFlags)
	{
		MutexLockGuard guard(mtx, FB_FUNCTION);

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
	Mutex mtx;
	SortedArray<T*> array;
};

template <typename Impl, typename Intf>
class YHelper : public RefCntIface<Intf>, public YObject
{
public:
	typedef typename Intf::Declaration NextInterface;
	typedef YAttachment YRef;

	static constexpr unsigned DF_RELEASE =		0x1;
	static constexpr unsigned DF_KEEP_NEXT =	0x2;

	explicit YHelper(NextInterface* aNext, const char* m = NULL)
		:
#ifdef DEV_BUILD
		  RefCntIface<Intf>(m),
#endif
		  next(REF_NO_INCR, aNext)
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

	RefPtr<NextInterface> next;
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
	public YHelper<YEvents, IEventsImpl<YEvents, CheckStatusWrapper>>
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_events_handle;

	YEvents(YAttachment* aAttachment, IEvents* aNext, IEventCallback* aCallback);

	void destroy(unsigned dstrFlags) override;
	FB_API_HANDLE& getHandle();

	// IEvents implementation
	void cancel(CheckStatusWrapper* status) override;
	void deprecatedCancel(CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	RefPtr<IEventCallback> callback;

private:
	AtomicCounter destroyed;
};

class YRequest final :
	public YHelper<YRequest, IRequestImpl<YRequest, CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_req_handle;

	YRequest(YAttachment* aAttachment, IRequest* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_req_handle& getHandle();

	// IRequest implementation
	void receive(CheckStatusWrapper* status, int level, unsigned int msgType,
		unsigned int length, void* message) override;
	void send(CheckStatusWrapper* status, int level, unsigned int msgType,
		unsigned int length, const void* message) override;
	void getInfo(CheckStatusWrapper* status, int level, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	void start(CheckStatusWrapper* status, ITransaction* transaction, int level) override;
	void startAndSend(CheckStatusWrapper* status, ITransaction* transaction, int level,
		unsigned int msgType, unsigned int length, const void* message) override;
	void unwind(CheckStatusWrapper* status, int level) override;
	void free(CheckStatusWrapper* status) override;
	void deprecatedFree(CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	isc_req_handle* userHandle;
};

class YTransaction final :
	public YHelper<YTransaction, ITransactionImpl<YTransaction, CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_trans_handle;

	YTransaction(YAttachment* aAttachment, ITransaction* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_tr_handle& getHandle();

	// ITransaction implementation
	void getInfo(CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	void prepare(CheckStatusWrapper* status, unsigned int msgLength,
		const unsigned char* message) override;
	void commit(CheckStatusWrapper* status) override;
	void commitRetaining(CheckStatusWrapper* status) override;
	void rollback(CheckStatusWrapper* status) override;
	void rollbackRetaining(CheckStatusWrapper* status) override;
	void disconnect(CheckStatusWrapper* status) override;
	ITransaction* join(CheckStatusWrapper* status, ITransaction* transaction) override;
	ITransaction* validate(CheckStatusWrapper* status, IAttachment* testAtt) override;
	YTransaction* enterDtc(CheckStatusWrapper* status) override;
	void deprecatedCommit(CheckStatusWrapper* status) override;
	void deprecatedRollback(CheckStatusWrapper* status) override;
	void deprecatedDisconnect(CheckStatusWrapper* status) override;

	void addCleanupHandler(CheckStatusWrapper* status, CleanupCallback* callback);
	void selfCheck();

public:
	AtomicAttPtr attachment;
	HandleArray<YBlob> childBlobs;
	HandleArray<YResultSet> childCursors;
	Array<CleanupCallback*> cleanupHandlers;

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

typedef RefPtr<ITransaction> NextTransaction;

class YBlob final :
	public YHelper<YBlob, IBlobImpl<YBlob, CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_segstr_handle;

	YBlob(YAttachment* aAttachment, YTransaction* aTransaction, IBlob* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_blob_handle& getHandle();

	// IBlob implementation
	void getInfo(CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	int getSegment(CheckStatusWrapper* status, unsigned int length, void* buffer,
								   unsigned int* segmentLength) override;
	void putSegment(CheckStatusWrapper* status, unsigned int length, const void* buffer) override;
	void cancel(CheckStatusWrapper* status) override;
	void close(CheckStatusWrapper* status) override;
	int seek(CheckStatusWrapper* status, int mode, int offset) override;
	void deprecatedCancel(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	AtomicTraPtr transaction;
};

class YResultSet final :
	public YHelper<YResultSet, IResultSetImpl<YResultSet, CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_result_set;

	YResultSet(YAttachment* anAttachment, YTransaction* aTransaction, IResultSet* aNext);
	YResultSet(YAttachment* anAttachment, YTransaction* aTransaction, YStatement* aStatement,
		IResultSet* aNext);

	void destroy(unsigned dstrFlags) override;

	// IResultSet implementation
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
	AtomicAttPtr attachment;
	AtomicTraPtr transaction;
	YStatement* statement;
};

class YBatch final :
	public YHelper<YBatch, IBatchImpl<YBatch, CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_result_set;	// isc_bad_batch

	YBatch(YAttachment* anAttachment, IBatch* aNext);

	void destroy(unsigned dstrFlags) override;

	// IBatch implementation
	void add(CheckStatusWrapper* status, unsigned count, const void* inBuffer) override;
	void addBlob(CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
		unsigned parLength, const unsigned char* par) override;
	void appendBlobData(CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void addBlobStream(CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	unsigned getBlobAlignment(CheckStatusWrapper* status) override;
	IMessageMetadata* getMetadata(CheckStatusWrapper* status) override;
	void registerBlob(CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) override;
	IBatchCompletionState* execute(CheckStatusWrapper* status, ITransaction* transaction) override;
	void cancel(CheckStatusWrapper* status) override;
	void setDefaultBpb(CheckStatusWrapper* status, unsigned parLength, const unsigned char* par) override;
	void close(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;
	void getInfo(CheckStatusWrapper* status, unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	AtomicAttPtr attachment;
};


class YReplicator final :
	public YHelper<YReplicator, IReplicatorImpl<YReplicator, CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_repl_handle;

	YReplicator(YAttachment* anAttachment, IReplicator* aNext);

	void destroy(unsigned dstrFlags) override;

	// IReplicator implementation
	void process(CheckStatusWrapper* status, unsigned length, const unsigned char* data) override;
	void close(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
};


class YMetadata
{
public:
	explicit YMetadata(bool in) noexcept
		: flag(false), input(in)
	{ }

	IMessageMetadata* get(IStatement* next, YStatement* statement);

private:
	RefPtr<MsgMetadata> metadata;
	volatile bool flag;
	bool input;
};

class YStatement final :
	public YHelper<YStatement, IStatementImpl<YStatement, CheckStatusWrapper> >
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_stmt_handle;

	YStatement(YAttachment* aAttachment, IStatement* aNext);

	void destroy(unsigned dstrFlags) override;

	// IStatement implementation
	void getInfo(CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	unsigned getType(CheckStatusWrapper* status) override;
	const char* getPlan(CheckStatusWrapper* status, FB_BOOLEAN detailed) override;
	ISC_UINT64 getAffectedRecords(CheckStatusWrapper* status) override;
	IMessageMetadata* getInputMetadata(CheckStatusWrapper* status) override;
	IMessageMetadata* getOutputMetadata(CheckStatusWrapper* status) override;
	ITransaction* execute(CheckStatusWrapper* status, ITransaction* transaction,
		IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, void* outBuffer) override;
	IResultSet* openCursor(CheckStatusWrapper* status, ITransaction* transaction,
		IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata,
		unsigned int flags) override;
	void setCursorName(CheckStatusWrapper* status, const char* name) override;
	void free(CheckStatusWrapper* status) override;
	void deprecatedFree(CheckStatusWrapper* status) override;
	unsigned getFlags(CheckStatusWrapper* status) override;

	unsigned int getTimeout(CheckStatusWrapper* status) override;
	void setTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;
	YBatch* createBatch(CheckStatusWrapper* status, IMessageMetadata* inMetadata,
		unsigned parLength, const unsigned char* par) override;

	unsigned getMaxInlineBlobSize(CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(CheckStatusWrapper* status, unsigned size) override;

public:
	AtomicAttPtr attachment;
	Mutex statementMutex;
	YResultSet* cursor;

	IMessageMetadata* getMetadata(bool in, IStatement* next);

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
	Mutex enterMutex;
};

class YAttachment final :
	public YHelper<YAttachment, IAttachmentImpl<YAttachment, CheckStatusWrapper> >,
	public EnterCount
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_db_handle;

	YAttachment(IProvider* aProvider, IAttachment* aNext,
		const PathName& aDbPath);
	~YAttachment();

	void destroy(unsigned dstrFlags) override;
	void shutdown();
	isc_db_handle& getHandle() noexcept;
	void getOdsVersion(USHORT* majorVersion, USHORT* minorVersion);

	// IAttachment implementation
	void getInfo(CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	YTransaction* startTransaction(CheckStatusWrapper* status, unsigned int tpbLength,
		const unsigned char* tpb) override;
	YTransaction* reconnectTransaction(CheckStatusWrapper* status, unsigned int length,
		const unsigned char* id) override;
	YRequest* compileRequest(CheckStatusWrapper* status, unsigned int blrLength,
		const unsigned char* blr) override;
	void transactRequest(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int blrLength, const unsigned char* blr, unsigned int inMsgLength,
		const unsigned char* inMsg, unsigned int outMsgLength, unsigned char* outMsg) override;
	YBlob* createBlob(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
		unsigned int bpbLength, const unsigned char* bpb) override;
	YBlob* openBlob(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
		unsigned int bpbLength, const unsigned char* bpb) override;
	int getSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
		const unsigned char* param, int sliceLength, unsigned char* slice) override;
	void putSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
		const unsigned char* param, int sliceLength, unsigned char* slice) override;
	void executeDyn(CheckStatusWrapper* status, ITransaction* transaction, unsigned int length,
		const unsigned char* dyn) override;
	YStatement* prepare(CheckStatusWrapper* status, ITransaction* tra,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags) override;
	ITransaction* execute(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, void* outBuffer) override;
	IResultSet* openCursor(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata,
		const char* cursorName, unsigned int cursorFlags) override;
	YEvents* queEvents(CheckStatusWrapper* status, IEventCallback* callback,
		unsigned int length, const unsigned char* eventsData) override;
	void cancelOperation(CheckStatusWrapper* status, int option) override;
	void ping(CheckStatusWrapper* status) override;
	void detach(CheckStatusWrapper* status) override;
	void dropDatabase(CheckStatusWrapper* status) override;
	void deprecatedDetach(CheckStatusWrapper* status) override;
	void deprecatedDropDatabase(CheckStatusWrapper* status) override;

	void addCleanupHandler(CheckStatusWrapper* status, CleanupCallback* callback);
	YTransaction* getTransaction(ITransaction* tra);
	void getNextTransaction(CheckStatusWrapper* status, ITransaction* tra, NextTransaction& next);
	void execute(CheckStatusWrapper* status, isc_tr_handle* traHandle,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, void* outBuffer);

	unsigned int getIdleTimeout(CheckStatusWrapper* status) override;
	void setIdleTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;
	unsigned int getStatementTimeout(CheckStatusWrapper* status) override;
	void setStatementTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;
	YBatch* createBatch(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned stmtLength, const char* sqlStmt, unsigned dialect,
		IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) override;
	YReplicator* createReplicator(CheckStatusWrapper* status) override;

	unsigned getMaxBlobCacheSize(CheckStatusWrapper* status) override;
	void setMaxBlobCacheSize(CheckStatusWrapper* status, unsigned size) override;
	unsigned getMaxInlineBlobSize(CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(CheckStatusWrapper* status, unsigned size) override;

public:
	IProvider* provider;
	PathName dbPath;
	HandleArray<YBlob> childBlobs;
	HandleArray<YEvents> childEvents;
	HandleArray<YRequest> childRequests;
	HandleArray<YStatement> childStatements;
	HandleArray<IscStatement> childIscStatements;
	HandleArray<YTransaction> childTransactions;
	Array<CleanupCallback*> cleanupHandlers;
	StatusHolder savedStatus;	// Do not use raise() method of this class in yValve.

private:
	USHORT cachedOdsMajorVersion = 0;
	USHORT cachedOdsMinorVersion = 0;
};

class YService final :
	public YHelper<YService, IServiceImpl<YService, CheckStatusWrapper> >,
	public EnterCount
{
public:
	static constexpr ISC_STATUS ERROR_CODE = isc_bad_svc_handle;

	YService(IProvider* aProvider, IService* aNext, bool utf8, Dispatcher* yProvider);
	~YService();

	void shutdown();
	void destroy(unsigned dstrFlags) override;
	isc_svc_handle& getHandle() noexcept;

	// IService implementation
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
	typedef IService NextInterface;
	typedef YService YRef;

private:
	IProvider* provider;
	bool utf8Connection;		// Client talks to us using UTF8, else - system default charset

public:
	RefPtr<IService> alternativeHandle;
	ClumpletWriter attachSpb;
	RefPtr<Dispatcher> ownProvider;
};

class Dispatcher final :
	public StdPlugin<IProviderImpl<Dispatcher, CheckStatusWrapper> >
{
public:
	Dispatcher() noexcept
		: cryptCallback(NULL)
	{ }

	// IProvider implementation
	YAttachment* attachDatabase(CheckStatusWrapper* status, const char* filename,
		unsigned int dpbLength, const unsigned char* dpb) override;
	YAttachment* createDatabase(CheckStatusWrapper* status, const char* filename,
		unsigned int dpbLength, const unsigned char* dpb) override;
	YService* attachServiceManager(CheckStatusWrapper* status, const char* serviceName,
		unsigned int spbLength, const unsigned char* spb) override;
	void shutdown(CheckStatusWrapper* status, unsigned int timeout, const int reason) override;
	void setDbCryptCallback(CheckStatusWrapper* status,
		ICryptKeyCallback* cryptCallback) override;

	void destroy(unsigned) noexcept
	{ }

public:
	IService* internalServiceAttach(CheckStatusWrapper* status,
		const PathName& svcName, ClumpletReader& spb,
		std::function<void(CheckStatusWrapper*, IService*)> start,
		IProvider** retProvider);

private:
	YAttachment* attachOrCreateDatabase(CheckStatusWrapper* status, bool createFlag,
		const char* filename, unsigned int dpbLength, const unsigned char* dpb);

	ICryptKeyCallback* cryptCallback;
};

class UtilInterface final :
	public AutoIface<IUtilImpl<UtilInterface, CheckStatusWrapper> >
{
	// IUtil implementation
public:
	void getFbVersion(CheckStatusWrapper* status, IAttachment* att,
		IVersionCallback* callback) override;
	void loadBlob(CheckStatusWrapper* status, ISC_QUAD* blobId, IAttachment* att,
		ITransaction* tra, const char* file, FB_BOOLEAN txt) override;
	void dumpBlob(CheckStatusWrapper* status, ISC_QUAD* blobId, IAttachment* att,
		ITransaction* tra, const char* file, FB_BOOLEAN txt) override;
	void getPerfCounters(CheckStatusWrapper* status, IAttachment* att,
		const char* countersSet, ISC_INT64* counters) override;			// in perf.cpp

	YAttachment* executeCreateDatabase(CheckStatusWrapper* status,
		unsigned stmtLength, const char* creatDBstatement, unsigned dialect,
		FB_BOOLEAN* stmtIsCreateDb = nullptr) override
	{
		return executeCreateDatabase2(status, stmtLength, creatDBstatement, dialect,
			0, nullptr, stmtIsCreateDb);
	}

	YAttachment* executeCreateDatabase2(CheckStatusWrapper* status,
		unsigned stmtLength, const char* creatDBstatement, unsigned dialect,
		unsigned dpbLength, const unsigned char* dpb,
		FB_BOOLEAN* stmtIsCreateDb = nullptr) override;

	void decodeDate(ISC_DATE date, unsigned* year, unsigned* month, unsigned* day) override;
	void decodeTime(ISC_TIME time,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions) override;
	ISC_DATE encodeDate(unsigned year, unsigned month, unsigned day) override;
	ISC_TIME encodeTime(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) override;
	unsigned formatStatus(char* buffer, unsigned bufferSize, IStatus* status) override;
	unsigned getClientVersion() override;
	IXpbBuilder* getXpbBuilder(CheckStatusWrapper* status,
		unsigned kind, const unsigned char* buf, unsigned len) override;
	unsigned setOffsets(CheckStatusWrapper* status, IMessageMetadata* metadata,
		IOffsetsCallback* callback) override;
	IDecFloat16* getDecFloat16(CheckStatusWrapper* status) override;
	IDecFloat34* getDecFloat34(CheckStatusWrapper* status) override;
	void decodeTimeTz(CheckStatusWrapper* status, const ISC_TIME_TZ* timeTz,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
		unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void decodeTimeStampTz(CheckStatusWrapper* status, const ISC_TIMESTAMP_TZ* timeStampTz,
		unsigned* year, unsigned* month, unsigned* day, unsigned* hours, unsigned* minutes, unsigned* seconds,
		unsigned* fractions, unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void encodeTimeTz(CheckStatusWrapper* status, ISC_TIME_TZ* timeTz,
		unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions, const char* timeZone) override;
	void encodeTimeStampTz(CheckStatusWrapper* status, ISC_TIMESTAMP_TZ* timeStampTz,
		unsigned year, unsigned month, unsigned day,
		unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions, const char* timeZone) override;
	IInt128* getInt128(CheckStatusWrapper* status) override;
	void decodeTimeTzEx(CheckStatusWrapper* status, const ISC_TIME_TZ_EX* timeEx,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
		unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void decodeTimeStampTzEx(CheckStatusWrapper* status, const ISC_TIMESTAMP_TZ_EX* timeStampEx,
		unsigned* year, unsigned* month, unsigned* day, unsigned* hours, unsigned* minutes, unsigned* seconds,
		unsigned* fractions, unsigned timeZoneBufferLength, char* timeZoneBuffer) override;

	void convert(CheckStatusWrapper* status,
		unsigned sourceType, unsigned sourceScale, unsigned sourceLength, const void* source,
		unsigned targetType, unsigned targetScale, unsigned targetLength, void* target) override;

};


}	// namespace Firebird::Why

#endif	// YVALVE_Y_OBJECTS_H
