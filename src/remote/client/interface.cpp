/*
 *	PROGRAM:	JRD Remote Interface
 *	MODULE:		interface.cpp
 *	DESCRIPTION:	User visible entrypoints remote interface
 *
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
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix" port
 *
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
 *
 * 2002.10.29 Sean Leyne - Removed support for obsolete IPX/SPX Protocol
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../remote/remote.h"
#include "../common/gdsassert.h"
#include "../common/isc_proto.h"
#include <stdarg.h>

#ifndef NO_NFS
#include <sys/param.h>
#endif

#include "ibase.h"
#include "../common/ThreadStart.h"
#include "../jrd/license.h"
#include "../remote/inet_proto.h"
#include "../remote/merge_proto.h"
#include "../remote/parse_proto.h"
#include "../remote/remot_proto.h"
#include "../remote/proto_proto.h"
#include "../common/cvt.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_f_proto.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/BatchCompletionState.h"
#include "../common/config/config.h"
#include "../common/utils_proto.h"
#include "../common/classes/DbImplementation.h"
#include "../common/Auth.h"
#include "../common/classes/GetPlugins.h"
#include "firebird/Interface.h"
#include "../common/StatementMetadata.h"
#include "../common/IntlParametersBlock.h"
#include "../common/status.h"

#include "../auth/SecurityDatabase/LegacyClient.h"
#include "../auth/SecureRemotePassword/client/SrpClient.h"
#include "../auth/trusted/AuthSspi.h"
#include "../plugins/crypt/arc4/Arc4.h"
#include "BlrFromMessage.h"
#include "../dsql/DsqlBatch.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef WIN_NT
#include <process.h>
#endif

#if defined(WIN_NT)
#include "../common/isc_proto.h"
#include "../remote/os/win32/wnet_proto.h"
#include "../remote/os/win32/xnet_proto.h"
#endif

#ifdef WIN_NT
#define sleep(seconds)		Sleep ((seconds) * 1000)
#endif // WIN_NT


const char* const PROTOCOL_INET = "inet";
const char* const PROTOCOL_INET4 = "inet4";
const char* const PROTOCOL_INET6 = "inet6";

#ifdef WIN_NT
const char* const PROTOCOL_WNET = "wnet";
const char* const PROTOCOL_XNET = "xnet";

const char* const WNET_SEPARATOR = "@";
const char* const WNET_LOCALHOST = "\\\\.";
#endif

const char* const INET_SEPARATOR = "/";

const char* const INET_LOCALHOST = "localhost";


using namespace Firebird;

namespace {
	void handle_error(ISC_STATUS code)
	{
		Arg::Gds(code).raise();
	}

	template <typename T>
	inline void CHECK_HANDLE(T* blk, ISC_STATUS error)
	{
		if (!blk || !blk->checkHandle())
		{
			handle_error(error);
		}
	}

	inline void CHECK_LENGTH(rem_port* port, size_t length)
	{
		if (length > MAX_USHORT && port->port_protocol < PROTOCOL_VERSION13)
			status_exception::raise(Arg::Gds(isc_imp_exc) << Arg::Gds(isc_blktoobig));
	}

	class SaveString
	{
	public:
		SaveString(cstring& toSave, ULONG newLength, UCHAR* newBuffer)
			: ptr(&toSave),
			  oldValue(*ptr)
		{
			ptr->cstr_address = newBuffer;
			ptr->cstr_allocated = newLength;
		}

		~SaveString()
		{
			*ptr = oldValue;
		}

	private:
		cstring* ptr;
		cstring oldValue;
	};
}

namespace Remote {

// Provider stuff
class Attachment;
class Statement;

class Blob FB_FINAL : public RefCntIface<IBlobImpl<Blob, CheckStatusWrapper> >
{
public:
	// IBlob implementation
	int release() override;
	void getInfo(CheckStatusWrapper* status,
						 unsigned int itemsLength, const unsigned char* items,
						 unsigned int bufferLength, unsigned char* buffer) override;
	int getSegment(CheckStatusWrapper* status, unsigned int bufferLength,
								   void* buffer, unsigned int* segmentLength) override;
	void putSegment(CheckStatusWrapper* status, unsigned int length, const void* buffer) override;
	void cancel(CheckStatusWrapper* status) override;
	void close(CheckStatusWrapper* status) override;
	int seek(CheckStatusWrapper* status, int mode, int offset) override;			// returns position
	void deprecatedCancel(Firebird::CheckStatusWrapper* status) override;
	void deprecatedClose(Firebird::CheckStatusWrapper* status) override;

public:
	explicit Blob(Rbl* handle)
		: blob(handle)
	{
		blob->rbl_self = &blob;
	}

private:
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalCancel(Firebird::CheckStatusWrapper* status);
	void internalClose(Firebird::CheckStatusWrapper* status);

	Rbl* blob;
};

int Blob::release()
{
	if (--refCounter != 0)
	{
		return 1;
	}

	if (blob)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Transaction FB_FINAL : public RefCntIface<ITransactionImpl<Transaction, CheckStatusWrapper> >
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
	ITransaction* join(CheckStatusWrapper* status, ITransaction* tra) override;
	Transaction* validate(CheckStatusWrapper* status, IAttachment* attachment) override;
	Transaction* enterDtc(CheckStatusWrapper* status) override;
	void deprecatedCommit(Firebird::CheckStatusWrapper* status) override;
	void deprecatedRollback(Firebird::CheckStatusWrapper* status) override;
	void deprecatedDisconnect(Firebird::CheckStatusWrapper* status) override;

public:
	Transaction(Rtr* handle, Attachment* a)
		: remAtt(a),
		  transaction(handle)
	{
		transaction->rtr_self = &transaction;
	}

	Rtr* getTransaction()
	{
		return transaction;
	}

	void clear()
	{
		transaction = NULL;
	}

private:
	Transaction(Transaction* from)
		: remAtt(from->remAtt),
		  transaction(from->transaction)
	{ }

	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalCommit(Firebird::CheckStatusWrapper* status);
	void internalRollback(Firebird::CheckStatusWrapper* status);
	void internalDisconnect(Firebird::CheckStatusWrapper* status);

	Attachment* remAtt;
	Rtr* transaction;
};

int Transaction::release()
{
	if (--refCounter != 0)
		return 1;

	if (transaction)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);	// ASF: Rollback - is this correct for reconnected transactions?
	}
	delete this;

	return 0;
}

class ResultSet FB_FINAL : public RefCntIface<IResultSetImpl<ResultSet, CheckStatusWrapper> >
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

	ResultSet(Statement* s, IMessageMetadata* outFmt, unsigned f)
		: stmt(s), flags(f), tmpStatement(false), delayedFormat(outFmt == DELAYED_OUT_FORMAT)
	{
		if (!delayedFormat)
			outputFormat = outFmt;
	}

private:
	bool fetch(CheckStatusWrapper* status, void* message, P_FETCH operation, int position = 0);
	void releaseStatement();
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalClose(CheckStatusWrapper* status);

	Statement* stmt;
	const unsigned flags;
	RefPtr<IMessageMetadata> outputFormat;

public:
	bool tmpStatement, delayedFormat;
};

int ResultSet::release()
{
	if (--refCounter != 0)
		return 1;

	if (stmt)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Batch FB_FINAL : public RefCntIface<IBatchImpl<Batch, CheckStatusWrapper> >
{
public:
	static const ULONG DEFER_BATCH_LIMIT = 64;

	Batch(Statement* s, IMessageMetadata* inFmt, unsigned parLength, const unsigned char* par);

	// IBatch implementation
	int release() override;
	void add(Firebird::CheckStatusWrapper* status, unsigned count, const void* inBuffer) override;
	void addBlob(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
		unsigned parLength, const unsigned char* par) override;
	void appendBlobData(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void addBlobStream(Firebird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void registerBlob(Firebird::CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) override;
	Firebird::IBatchCompletionState* execute(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* transaction) override;
	void cancel(Firebird::CheckStatusWrapper* status) override;
	unsigned getBlobAlignment(Firebird::CheckStatusWrapper* status) override;
	void setDefaultBpb(Firebird::CheckStatusWrapper* status, unsigned parLength, const unsigned char* par) override;
	Firebird::IMessageMetadata* getMetadata(Firebird::CheckStatusWrapper* status) override;
	void close(Firebird::CheckStatusWrapper* status) override;
	void deprecatedClose(Firebird::CheckStatusWrapper* status) override;
	void getInfo(CheckStatusWrapper* status,
				 unsigned int itemsLength, const unsigned char* items,
				 unsigned int bufferLength, unsigned char* buffer) override;

private:
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalClose(Firebird::CheckStatusWrapper* status);
	void releaseStatement();
	void setServerInfo();

	void cleanup()
	{
		if (blobPolicy != BLOB_NONE)
			blobStream = blobStreamBuffer;
		sizePointer = nullptr;
		messageStream = 0;
	}

	void genBlobId(ISC_QUAD* blobId)
	{
		if (++genId.gds_quad_low == 0)
			++genId.gds_quad_high;
		memcpy(blobId, &genId, sizeof(genId));
	}

	bool batchHasData()
	{
		return batchActive;
	}

	// working with message stream buffer
	void putMessageData(ULONG count, const void* p)
	{
		fb_assert(messageStreamBuffer);

		const UCHAR* ptr = reinterpret_cast<const UCHAR*>(p);

		while(count)
		{
			ULONG remainSpace = messageBufferSize - messageStream;
			ULONG step = MIN(count, remainSpace);
			if (step == messageBufferSize)
			{
				// direct packet sent
				sendMessagePacket(step, ptr, false);
			}
			else
			{
				// use buffer
				memcpy(&messageStreamBuffer[messageStream * alignedSize], ptr, step * alignedSize);
				messageStream += step;
				if (messageStream == messageBufferSize)
				{
					sendMessagePacket(messageBufferSize, messageStreamBuffer, false);
					messageStream = 0;
				}
			}

			count -= step;
			ptr += step * alignedSize;
		}
	}

	// working with blob stream buffer
	void newBlob()
	{
		setServerInfo();
		alignBlobBuffer(blobAlign);

		fb_assert(blobStream - blobStreamBuffer <= blobBufferSize);
		ULONG space = blobBufferSize - (blobStream - blobStreamBuffer);
		if (space < Rsr::BatchStream::SIZEOF_BLOB_HEAD)
		{
			sendBlobPacket(blobStream - blobStreamBuffer, blobStreamBuffer, false);
			blobStream = blobStreamBuffer;
		}
	}

	void alignBlobBuffer(unsigned alignment, ULONG* bs = NULL)
	{
		fb_assert(alignment);

		ULONG align = FB_ALIGN(blobStream, alignment) - blobStream;
		if (bs)
			*bs += align;

		FB_UINT64 zeroFill = 0;
		putBlobData(align, &zeroFill);
	}

	void putBlobData(ULONG size, const void* p)
	{
		fb_assert(blobStreamBuffer);

		const UCHAR* ptr = reinterpret_cast<const UCHAR*>(p);

		while(size)
		{
			ULONG space = blobBufferSize - (blobStream - blobStreamBuffer);
			ULONG step = MIN(size, space);
			if (step == blobBufferSize)
			{
				// direct packet sent
				sendBlobPacket(blobBufferSize, ptr, false);
			}
			else
			{
				// use buffer
				memcpy(blobStream, ptr, step);
				blobStream += step;
				if (blobStream - blobStreamBuffer == blobBufferSize)
				{
					sendBlobPacket(blobBufferSize, blobStreamBuffer, false);
					blobStream = blobStreamBuffer;
					sizePointer = nullptr;
				}
			}

			size -= step;
			ptr += step;
		}
	}

	void setSizePointer()
	{
		fb_assert(FB_ALIGN(blobStream, sizeof(*sizePointer)) == blobStream);
		sizePointer = reinterpret_cast<ULONG*>(blobStream);
	}

	void putSegment(ULONG size, const void* ptr)
	{
		if (!sizePointer)
		{
			newBlob();

			ISC_QUAD quadZero = {0, 0};
			putBlobData(sizeof quadZero, &quadZero);
			setSizePointer();
			ULONG longZero = 0;
			putBlobData(sizeof longZero, &longZero);
			putBlobData(sizeof longZero, &longZero);
		}

		*sizePointer += size;

		if (segmented)
		{
			if (size > MAX_USHORT)
			{
				(Arg::Gds(isc_imp_exc) << Arg::Gds(isc_blobtoobig)
					<< Arg::Gds(isc_big_segment) << Arg::Num(size)).raise();
			}

			*sizePointer += sizeof(USHORT);

			alignBlobBuffer(BLOB_SEGHDR_ALIGN, sizePointer);

			USHORT segSize = size;
			putBlobData(sizeof segSize, &segSize);
		}

		putBlobData(size, ptr);
	}

	void flashBatch()
	{
		if (blobPolicy != BLOB_NONE)
		{
			setServerInfo();
			alignBlobBuffer(blobAlign);
			ULONG size = blobStream - blobStreamBuffer;
			if (size)
			{
				sendBlobPacket(size, blobStreamBuffer, messageStream == 0);
				blobStream = blobStreamBuffer;
			}
		}

		if (messageStream)
		{
			sendMessagePacket(messageStream, messageStreamBuffer, true);
			messageStream = 0;
		}

		batchActive = false;
		blobCount = messageCount = 0;
	}

	void sendBlobPacket(unsigned size, const UCHAR* ptr, bool flash);
	void sendMessagePacket(unsigned size, const UCHAR* ptr, bool flash);
	void sendDeferredPacket(IStatus* status, rem_port* port, PACKET* packet, bool flash);

	Firebird::AutoPtr<UCHAR, Firebird::ArrayDelete> messageStreamBuffer, blobStreamBuffer;
	ULONG messageStream;
	UCHAR* blobStream;
	ULONG* sizePointer;

	ULONG messageSize, alignedSize, blobBufferSize, messageBufferSize, flags;
	Statement* stmt;
	RefPtr<IMessageMetadata> format;
	ISC_QUAD genId;
	int blobAlign;
	UCHAR blobPolicy;
	bool segmented, defSegmented, batchActive;

	ULONG messageCount, blobCount, serverSize, blobHeadSize;

public:
	bool tmpStatement;
};

int Batch::release()
{
	if (--refCounter != 0)
		return 1;

	if (stmt)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Replicator FB_FINAL : public RefCntIface<IReplicatorImpl<Replicator, CheckStatusWrapper> >
{
public:
	// IReplicator implementation
	int release() override;
	void process(CheckStatusWrapper* status, unsigned length, const unsigned char* data) override;
	void close(CheckStatusWrapper* status) override;
	void deprecatedClose(CheckStatusWrapper* status) override;

	explicit Replicator(Attachment* att) : attachment(att)
	{}

private:
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalClose(CheckStatusWrapper* status);

	Attachment* attachment;
};

int Replicator::release()
{
	if (--refCounter != 0)
		return 1;

	if (attachment)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Statement FB_FINAL : public RefCntIface<IStatementImpl<Statement, CheckStatusWrapper> >
{
public:
	// IStatement implementation
	int release() override;
	void getInfo(CheckStatusWrapper* status,
						 unsigned int itemsLength, const unsigned char* items,
						 unsigned int bufferLength, unsigned char* buffer) override;
	unsigned getType(CheckStatusWrapper* status) override;
	const char* getPlan(CheckStatusWrapper* status, FB_BOOLEAN detailed) override;
	Firebird::IMessageMetadata* getInputMetadata(CheckStatusWrapper* status) override;
	Firebird::IMessageMetadata* getOutputMetadata(CheckStatusWrapper* status) override;
	ISC_UINT64 getAffectedRecords(CheckStatusWrapper* status) override;
	ITransaction* execute(CheckStatusWrapper* status, ITransaction* tra,
		IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, void* outBuffer) override;
	ResultSet* openCursor(CheckStatusWrapper* status, ITransaction* tra,
		IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outFormat,
		unsigned int flags) override;
	void setCursorName(CheckStatusWrapper* status, const char* name) override;
	void free(CheckStatusWrapper* status) override;
	void deprecatedFree(CheckStatusWrapper* status) override;
	unsigned getFlags(CheckStatusWrapper* status) override;

	unsigned int getTimeout(CheckStatusWrapper* status) override
	{
		if (statement->rsr_rdb->rdb_port->port_protocol < PROTOCOL_STMT_TOUT)
		{
			status->setErrors(Arg::Gds(isc_wish_list).value());
			return 0;
		}

		return statement->rsr_timeout;
	}

	void setTimeout(CheckStatusWrapper* status, unsigned int timeOut) override
	{
		if (timeOut && statement->rsr_rdb->rdb_port->port_protocol < PROTOCOL_STMT_TOUT)
		{
			status->setErrors(Arg::Gds(isc_wish_list).value());
			return;
		}

		statement->rsr_timeout = timeOut;
	}

	Batch* createBatch(CheckStatusWrapper* status, IMessageMetadata* inMetadata,
		unsigned parLength, const unsigned char* par) override;

public:
	Statement(Rsr* handle, Attachment* a, unsigned aDialect)
		: metadata(getPool(), this, NULL),
		  remAtt(a),
		  statement(handle),
		  dialect(aDialect)
	{
		statement->rsr_self = &statement;
	}

	Rsr* getStatement()
	{
		return statement;
	}

	Attachment* getAttachment()
	{
		return remAtt;
	}

	void parseMetadata(const Array<UCHAR>& buffer)
	{
		metadata.clear();
		metadata.parse((ULONG) buffer.getCount(), buffer.begin());
	}

	unsigned getDialect() const
	{
		return dialect;
	}

private:
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalFree(CheckStatusWrapper* status);

	StatementMetadata metadata;
	Attachment* remAtt;
	Rsr* statement;
	unsigned dialect;
};

int Statement::release()
{
	if (--refCounter != 0)
		return 1;

	if (statement)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Request FB_FINAL : public RefCntIface<IRequestImpl<Request, CheckStatusWrapper> >
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
	void start(CheckStatusWrapper* status, Firebird::ITransaction* tra, int level) override;
	void startAndSend(CheckStatusWrapper* status, Firebird::ITransaction* tra, int level, unsigned int msg_type,
							  unsigned int length, const void* message) override;
	void unwind(CheckStatusWrapper* status, int level) override;
	void free(CheckStatusWrapper* status) override;
	void deprecatedFree(CheckStatusWrapper* status) override;

public:
	Request(Rrq* handle, Attachment* a)
		: remAtt(a), rq(handle)
	{
		rq->rrq_self = &rq;
	}

private:
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalFree(CheckStatusWrapper* status);

	Attachment* remAtt;
	Rrq* rq;
};

int Request::release()
{
	if (--refCounter != 0)
		return 1;

	if (rq)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Events FB_FINAL : public RefCntIface<IEventsImpl<Events, CheckStatusWrapper> >
{
public:
	// IEvents implementation
	int release() override;
	void cancel(CheckStatusWrapper* status) override;
	void deprecatedCancel(CheckStatusWrapper* status) override;

public:
	Events(Rvnt* handle)
		: rvnt(handle), rdb(rvnt->rvnt_rdb)
	{
		rvnt->rvnt_self = &rvnt;
	}

private:
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalCancel(CheckStatusWrapper* status);

	Rvnt* rvnt;
	Rdb* rdb;
};


int Events::release()
{
	int rc = --refCounter;
	if (rc != 0)
	{
		fb_assert(rc > 0);
		return 1;
	}

	if (rvnt)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Attachment FB_FINAL : public RefCntIface<IAttachmentImpl<Attachment, CheckStatusWrapper> >
{
public:
	// IAttachment implementation
	int release() override;
	void getInfo(CheckStatusWrapper* status,
						 unsigned int itemsLength, const unsigned char* items,
						 unsigned int bufferLength, unsigned char* buffer) override;
	Firebird::ITransaction* startTransaction(CheckStatusWrapper* status,
		unsigned int tpbLength, const unsigned char* tpb) override;
	Firebird::ITransaction* reconnectTransaction(CheckStatusWrapper* status, unsigned int length, const unsigned char* id) override;
	Firebird::IRequest* compileRequest(CheckStatusWrapper* status, unsigned int blr_length, const unsigned char* blr) override;
	void transactRequest(CheckStatusWrapper* status, ITransaction* transaction,
								 unsigned int blr_length, const unsigned char* blr,
								 unsigned int in_msg_length, const unsigned char* in_msg,
								 unsigned int out_msg_length, unsigned char* out_msg) override;
	Firebird::IBlob* createBlob(CheckStatusWrapper* status, ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	Firebird::IBlob* openBlob(CheckStatusWrapper* status, ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	int getSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
						 unsigned int sdl_length, const unsigned char* sdl,
						 unsigned int param_length, const unsigned char* param,
						 int sliceLength, unsigned char* slice) override;
	void putSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
						  unsigned int sdl_length, const unsigned char* sdl,
						  unsigned int param_length, const unsigned char* param,
						  int sliceLength, unsigned char* slice) override;
	void executeDyn(CheckStatusWrapper* status, ITransaction* transaction, unsigned int length,
		const unsigned char* dyn) override;
	Statement* prepare(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned dialect, unsigned int flags) override;
	Firebird::ITransaction* execute(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned dialect,
		IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, void* outBuffer) override;
	Firebird::IResultSet* openCursor(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned dialect,
		IMessageMetadata* inMetadata, void* inBuffer, Firebird::IMessageMetadata* outMetadata,
		const char* cursorName, unsigned int cursorFlags) override;
	Firebird::IEvents* queEvents(CheckStatusWrapper* status, Firebird::IEventCallback* callback,
									 unsigned int length, const unsigned char* events) override;
	void cancelOperation(CheckStatusWrapper* status, int option) override;
	void ping(CheckStatusWrapper* status) override;
	void detach(CheckStatusWrapper* status) override;
	void dropDatabase(CheckStatusWrapper* status) override;
	void deprecatedDetach(Firebird::CheckStatusWrapper* status) override;
	void deprecatedDropDatabase(Firebird::CheckStatusWrapper* status) override;

	unsigned int getIdleTimeout(CheckStatusWrapper* status) override;
	void setIdleTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;
	unsigned int getStatementTimeout(CheckStatusWrapper* status) override;
	void setStatementTimeout(CheckStatusWrapper* status, unsigned int timeOut) override;

	Batch* createBatch(Firebird::CheckStatusWrapper* status, ITransaction* transaction,
		unsigned stmtLength, const char* sqlStmt, unsigned dialect,
		IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) override;

	Replicator* createReplicator(Firebird::CheckStatusWrapper* status) override;

public:
	Attachment(Rdb* handle, const PathName& path)
		: replicator(nullptr), rdb(handle), dbPath(getPool(), path)
	{ }

	Rdb* getRdb()
	{
		return rdb;
	}

	const PathName& getDbPath()
	{
		return dbPath;
	}

	Rtr* remoteTransaction(ITransaction* apiTra);
	Transaction* remoteTransactionInterface(ITransaction* apiTra);
	Statement* createStatement(CheckStatusWrapper* status, unsigned dialect);

	Replicator* replicator;

private:
	void execWithCheck(CheckStatusWrapper* status, const string& stmt);
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalDetach(Firebird::CheckStatusWrapper* status);
	void internalDropDatabase(Firebird::CheckStatusWrapper* status);
	SLONG getSingleInfo(CheckStatusWrapper* status, UCHAR infoItem);

	Rdb* rdb;
	const PathName dbPath;
};

int Attachment::release()
{
	if (--refCounter != 0)
		return 1;

	if (rdb)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class Service FB_FINAL : public RefCntIface<IServiceImpl<Service, CheckStatusWrapper> >
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
	void start(CheckStatusWrapper* status, unsigned int spbLength, const unsigned char* spb) override;

public:
	Service(Rdb* handle) : rdb(handle) { }

private:
	void freeClientData(CheckStatusWrapper* status, bool force = false);
	void internalDetach(CheckStatusWrapper* status);

	Rdb* rdb;
};

int Service::release()
{
	if (--refCounter != 0)
		return 1;

	if (rdb)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);
		freeClientData(&status, true);
	}
	delete this;

	return 0;
}

class RProvider : public StdPlugin<IProviderImpl<RProvider, CheckStatusWrapper> >
{
public:
	explicit RProvider(IPluginConfig*)
		: cryptCallback(NULL)
	{ }
	RProvider()
		: cryptCallback(NULL)
	{ }

	// IProvider implementation
	IAttachment* attachDatabase(CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	IAttachment* createDatabase(CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	IService* attachServiceManager(CheckStatusWrapper* status, const char* service,
		unsigned int spbLength, const unsigned char* spb);
	void shutdown(CheckStatusWrapper* status, unsigned int timeout, const int reason);
	void setDbCryptCallback(CheckStatusWrapper* status, ICryptKeyCallback* cryptCallback);

protected:
	IAttachment* attach(CheckStatusWrapper* status, const char* filename, unsigned int dpb_length,
		const unsigned char* dpb, bool loopback);
	IAttachment* create(CheckStatusWrapper* status, const char* filename, unsigned int dpb_length,
		const unsigned char* dpb, bool loopback);
	IService* attachSvc(CheckStatusWrapper* status, const char* service, unsigned int spbLength,
		const unsigned char* spb, bool loopback);

private:
	Firebird::ICryptKeyCallback* cryptCallback;
};

void RProvider::shutdown(CheckStatusWrapper* status, unsigned int /*timeout*/, const int /*reason*/)
{
	status->init();
}

void RProvider::setDbCryptCallback(CheckStatusWrapper* status, ICryptKeyCallback* callback)
{
	status->init();
	cryptCallback = callback;
}

class Loopback : public IProviderBaseImpl<Loopback, CheckStatusWrapper, RProvider>
{
public:
	explicit Loopback(IPluginConfig*)
	{ }

	// IProvider implementation
	IAttachment* attachDatabase(CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	IAttachment* createDatabase(CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	IService* attachServiceManager(CheckStatusWrapper* status, const char* service,
		unsigned int spbLength, const unsigned char* spb);
};

namespace {
	SimpleFactory<RProvider> remoteFactory;
	SimpleFactory<Loopback> loopbackFactory;
}

void registerRedirector(Firebird::IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(IPluginManager::TYPE_PROVIDER, "Remote", &remoteFactory);
	iPlugin->registerPluginFactory(IPluginManager::TYPE_PROVIDER, "Loopback", &loopbackFactory);

	Auth::registerLegacyClient(iPlugin);
	Auth::registerSrpClient(iPlugin);
#ifdef TRUSTED_AUTH
	Auth::registerTrustedClient(iPlugin);
#endif

	Crypt::registerArc4(iPlugin);
}

} // namespace Remote

/*
extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(IMaster* master)
{
	IPluginManager* pi = master->getPluginManager();
	registerRedirector(pi);
	pi->release();
}
*/

namespace Remote {

static Rvnt* add_event(rem_port*);
static void add_other_params(rem_port*, ClumpletWriter&, const ParametersSet&);
static void add_working_directory(ClumpletWriter&, const PathName&);
static rem_port* analyze(ClntAuthBlock& cBlock, PathName& attach_name, unsigned flags,
	ClumpletWriter& pb, const ParametersSet& parSet, PathName& node_name, PathName* ref_db_name,
	Firebird::ICryptKeyCallback* cryptCb);
static void batch_gds_receive(rem_port*, struct rmtque *, USHORT);
static void batch_dsql_fetch(rem_port*, struct rmtque *, USHORT);
static void clear_queue(rem_port*);
static void clear_stmt_que(rem_port*, Rsr*);
static void disconnect(rem_port*);
static void enqueue_receive(rem_port*, t_rmtque_fn, Rdb*, void*, Rrq::rrq_repeat*);
static void dequeue_receive(rem_port*);
static THREAD_ENTRY_DECLARE event_thread(THREAD_ENTRY_PARAM);
static Rvnt* find_event(rem_port*, SLONG);
static bool get_new_dpb(ClumpletWriter&, const ParametersSet&);
static void info(CheckStatusWrapper*, Rdb*, P_OP, USHORT, USHORT, USHORT,
	const UCHAR*, USHORT, const UCHAR*, ULONG, UCHAR*);
static void init(CheckStatusWrapper*, ClntAuthBlock&, rem_port*, P_OP, PathName&,
	ClumpletWriter&, IntlParametersBlock&, ICryptKeyCallback* cryptCallback);
static Rtr* make_transaction(Rdb*, USHORT);
static void mov_dsql_message(const UCHAR*, const rem_fmt*, UCHAR*, const rem_fmt*);
static void move_error(const Arg::StatusVector& v);
static void receive_after_start(Rrq*, USHORT);
static void receive_packet(rem_port*, PACKET *);
static void receive_packet_noqueue(rem_port*, PACKET *);
static void receive_queued_packet(rem_port*, USHORT);
static void receive_response(IStatus*, Rdb*, PACKET *);
static void release_blob(Rbl*);
static void release_event(Rvnt*);
static void release_object(IStatus*, Rdb*, P_OP, USHORT);
static void release_request(Rrq*);
static void release_statement(Rsr**);
static void release_sql_request(Rsr*);
static void release_transaction(Rtr*);
static void send_and_receive(IStatus*, Rdb*, PACKET *);
static void send_blob(CheckStatusWrapper*, Rbl*, USHORT, const UCHAR*);
static void send_packet(rem_port*, PACKET *);
static void send_partial_packet(rem_port*, PACKET *);
static void server_death(rem_port*);
static void svcstart(CheckStatusWrapper*, Rdb*, P_OP, USHORT, USHORT, USHORT, const UCHAR*);
static void unsupported();
static void zap_packet(PACKET *);
static void cleanDpb(Firebird::ClumpletWriter&, const ParametersSet*);
static void authFillParametersBlock(ClntAuthBlock& authItr, ClumpletWriter& dpb,
	const ParametersSet* tags, rem_port* port);
static void authReceiveResponse(bool havePacket, ClntAuthBlock& authItr, rem_port* port,
	Rdb* rdb, IStatus* status, PACKET* packet, bool checkKeys);

static AtomicCounter remote_event_id;

static const unsigned ANALYZE_USER_VFY =	0x01;
static const unsigned ANALYZE_LOOPBACK =	0x02;
static const unsigned ANALYZE_MOUNTS =		0x04;
static const unsigned ANALYZE_EMP_NAME =	0x08;

inline static void reset(IStatus* status) throw()
{
	status->init();
}

#define SET_OBJECT(rdb, object, id) rdb->rdb_port->setHandle(object, id)

inline static void defer_packet(rem_port* port, PACKET* packet, bool sent = false)
{
	// hvlad: passed packet often is rdb->rdb_packet and therefore can be
	// changed inside clear_queue. To not confuse caller we must preserve
	// packet content

	rem_que_packet p;
	p.packet = *packet;
	p.sent = sent;

	clear_queue(port);
	*packet = p.packet;

	// don't use string references in P_RESP structure copied from another packet
	memset(&p.packet.p_resp, 0, sizeof(p.packet.p_resp));
	port->port_deferred_packets->add(p);
}

IAttachment* RProvider::attach(CheckStatusWrapper* status, const char* filename, unsigned int dpb_length,
	const unsigned char* dpb, bool loopback)
{
/**************************************
 *
 *	g d s _ a t t a c h _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Connect to an old, grungy database, corrupted by user data.
 *
 **************************************/
	try
	{
		reset(status);

		ClumpletWriter newDpb(ClumpletReader::dpbList, MAX_DPB_SIZE, dpb, dpb_length);
		unsigned flags = ANALYZE_MOUNTS;

		if (get_new_dpb(newDpb, dpbParam))
			flags |= ANALYZE_USER_VFY;

		if (loopback)
			flags |= ANALYZE_LOOPBACK;

		PathName expanded_name(filename);
		PathName node_name;

		ClntAuthBlock cBlock(&expanded_name, &newDpb, &dpbParam);
		rem_port* port = analyze(cBlock, expanded_name, flags, newDpb, dpbParam, node_name, NULL, cryptCallback);

		if (!port)
		{
			Arg::Gds(isc_unavailable).copyTo(status);
			return NULL;
		}

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// The client may have set a parameter for dummy_packet_interval.  Add that to the
		// the DPB so the server can pay attention to it.

		add_other_params(port, newDpb, dpbParam);
		add_working_directory(newDpb, node_name);

		IntlDpb intl;
		HANDSHAKE_DEBUG(fprintf(stderr, "Cli: call init for DB='%s'\n", expanded_name.c_str()));
		init(status, cBlock, port, op_attach, expanded_name, newDpb, intl, cryptCallback);

		Attachment* a = FB_NEW Attachment(port->port_context, filename);
		a->addRef();
		return a;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return NULL;
	}
}


IAttachment* RProvider::attachDatabase(CheckStatusWrapper* status, const char* filename,
	unsigned int dpb_length, const unsigned char* dpb)
{
/**************************************
 *
 *	g d s _ a t t a c h _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Connect to an old, grungy database, corrupted by user data.
 *
 **************************************/

	return attach(status, filename, dpb_length, dpb, false);
}


IAttachment* Loopback::attachDatabase(CheckStatusWrapper* status, const char* filename,
	unsigned int dpb_length, const unsigned char* dpb)
{
/**************************************
 *
 *	g d s _ a t t a c h _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Connect to an old, grungy database, corrupted by user data.
 *
 **************************************/

	return attach(status, filename, dpb_length, dpb, true);
}


void Blob::getInfo(CheckStatusWrapper* status,
				   unsigned int itemsLength, const unsigned char* items,
				   unsigned int bufferLength, unsigned char* buffer)
{
/**************************************
 *
 *	g d s _ b l o b _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on blob object.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(blob, isc_bad_segstr_handle);

		Rdb* rdb = blob->rbl_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		info(status, rdb, op_info_blob, blob->rbl_id, 0,
			 itemsLength, items, 0, 0, bufferLength, buffer);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Blob::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	g d s _ c a n c e l _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Abort a partially completed blob.
 *
 **************************************/
	try
	{
		if (!blob)
		{
			return;
		}

		CHECK_HANDLE(blob, isc_bad_segstr_handle);

		Rdb* rdb = blob->rbl_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		try
		{
			release_object(status, rdb, op_cancel_blob, blob->rbl_id);
		}
		catch (const Exception&)
		{
			if (!force)
				throw;
		}
		release_blob(blob);
		blob = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Blob::internalCancel(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ c a n c e l _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Abort a partially completed blob.
 *
 **************************************/
	reset(status);
	freeClientData(status);
}


void Blob::cancel(CheckStatusWrapper* status)
{
	internalCancel(status);
	if (status->isEmpty())
		release();
}


void Blob::deprecatedCancel(CheckStatusWrapper* status)
{
	internalCancel(status);
}


void Blob::internalClose(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ c l o s e _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Close a completed blob.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(blob, isc_bad_segstr_handle);

		Rdb* rdb = blob->rbl_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		if ((blob->rbl_flags & Rbl::CREATE) && blob->rbl_ptr != blob->rbl_buffer)
		{
			send_blob(status, blob, 0, NULL);
		}

		release_object(status, rdb, op_close_blob, blob->rbl_id);
		release_blob(blob);
		blob = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Blob::close(CheckStatusWrapper* status)
{
	internalClose(status);
	if (status->isEmpty())
		release();
}


void Blob::deprecatedClose(CheckStatusWrapper* status)
{
	internalClose(status);
}


void Events::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	g d s _ $ c a n c e l _ e v e n t s
 *
 **************************************
 *
 * Functional description
 *	Cancel an outstanding event.
 *
 **************************************/
	RefPtr<IEventCallback> callback;

	try
	{
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		if (!rvnt)
		{
			return;
		}
		CHECK_HANDLE(rvnt, isc_bad_events_handle);

		try
		{
			// Tell the remote server to cancel it and delete it from the list

			PACKET*	packet = &rdb->rdb_packet;

			// Set the various parameters for the packet:
			// remote operation to perform, which database,
			// and which event.

			packet->p_operation = op_cancel_events;
			packet->p_event.p_event_database = rdb->rdb_id;
			const SLONG save_id = packet->p_event.p_event_rid = rvnt->rvnt_id;

			// Send the packet, and if that worked, get a response

			try
			{
				LocalStatus ls;
				CheckStatusWrapper dummy(&ls);
				send_packet(rdb->rdb_port, packet);
				receive_response(&dummy, rdb, packet);
			}
			catch (const Exception&) { }

			// Get ready to fire the event.

			if (rvnt->rvnt_id == save_id)
			{
				callback = rvnt->rvnt_callback;
				rvnt->rvnt_id = 0;
			}
		}
		catch (const Exception&)
		{
			if (!force)
				throw;
		}

		rvnt = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	// If the event has never been fired, fire it off with a length of 0.
	// Note: it is job of person being notified to check that counts
	// actually changed and that they were not woken up because of
	// server death.

	if (callback)
		callback->eventCallbackFunction(0, NULL);
}


void Events::internalCancel(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ $ c a n c e l _ e v e n t s
 *
 **************************************
 *
 * Functional description
 *	Cancel an outstanding event.
 *
 **************************************/
	reset(status);
	freeClientData(status);
}


void Events::cancel(CheckStatusWrapper* status)
{
	internalCancel(status);
	if (status->isEmpty())
		release();
}


void Events::deprecatedCancel(CheckStatusWrapper* status)
{
	internalCancel(status);
}


void Transaction::internalCommit(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ c o m m i t
 *
 **************************************
 *
 * Functional description
 *	Commit a transaction.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = transaction->rtr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		release_object(status, rdb, op_commit, transaction->rtr_id);
		REMOTE_cleanup_transaction(transaction);
		release_transaction(transaction);
		transaction = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Transaction::commit(CheckStatusWrapper* status)
{
	internalCommit(status);
	if (status->isEmpty())
		release();
}


void Transaction::deprecatedCommit(CheckStatusWrapper* status)
{
	internalCommit(status);
}


void Transaction::commitRetaining(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ c o m m i t _ r e t a i n i n g
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	try
	{
		reset(status);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = transaction->rtr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		release_object(status, rdb, op_commit_retaining, transaction->rtr_id);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


ITransaction* Transaction::join(CheckStatusWrapper* status, ITransaction* tra)
{
/**************************************
 *
 *	I T r a n s a c t i o n :: j o i n
 *
 **************************************
 *
 * Functional description
 *	Join this and passed transactions
 *	into single distributed transaction
 *
 **************************************/
	try
	{
		reset(status);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		return DtcInterfacePtr()->join(status, this, tra);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


Transaction* Transaction::validate(CheckStatusWrapper* /*status*/, IAttachment* testAtt)
{
	return (transaction && remAtt == testAtt) ? this : NULL;
}


Transaction* Transaction::enterDtc(CheckStatusWrapper* status)
{
	try
	{
		Transaction* copy = FB_NEW Transaction(this);
		copy->addRef();

		transaction = NULL;

		return copy;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


Firebird::IRequest* Attachment::compileRequest(CheckStatusWrapper* status,
										   unsigned int blr_length, const unsigned char* blr)
{
/**************************************
 *
 *	g d s _ c o m p i l e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Validate data length

		CHECK_LENGTH(port, blr_length);

		// Parse the request in case blr_d_float must be converted to blr_double

		const UCHAR* new_blr = blr;

		// Make up a packet for the remote guy

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_compile;
		P_CMPL* compile = &packet->p_cmpl;
		compile->p_cmpl_database = rdb->rdb_id;
		compile->p_cmpl_blr.cstr_length = blr_length;
		compile->p_cmpl_blr.cstr_address = new_blr;

		send_and_receive(status, rdb, packet);

		// Parse the request to find the messages

		RMessage* next;

		RMessage* message = PARSE_messages(blr, blr_length);
		USHORT max_msg = 0;
		for (next = message; next; next = next->msg_next)
			max_msg = MAX(max_msg, next->msg_number);

		// Allocate request block
		Rrq* request = FB_NEW Rrq(max_msg + 1);
		request->rrq_rdb = rdb;
		request->rrq_id = packet->p_resp.p_resp_object;
		request->rrq_max_msg = max_msg;
		SET_OBJECT(rdb, request, request->rrq_id);
		request->rrq_next = rdb->rdb_requests;
		rdb->rdb_requests = request;

		// when the messages are parsed, they are linked together; we need
		// to place the messages in the tail of the request block and create
		// a queue of length 1 for each message number

		for (; message; message = next)
		{
			next = message->msg_next;

			message->msg_next = message;

			Rrq::rrq_repeat * tail = &request->rrq_rpt[message->msg_number];
			tail->rrq_message = message;
			tail->rrq_xdr = message;
			tail->rrq_format = (rem_fmt*) message->msg_address;

			message->msg_address = NULL;
		}

		Firebird::IRequest* r = FB_NEW Request(request, this);
		r->addRef();
		return r;
	}
	catch (const Exception& ex)
	{
	    // deallocate new_blr here???
		ex.stuffException(status);
	}
	return NULL;
}


IBlob* Attachment::createBlob(CheckStatusWrapper* status, ITransaction* apiTra, ISC_QUAD* blob_id,
									 unsigned int bpb_length, const unsigned char* bpb)
{
/**************************************
 *
 *	g d s _ c r e a t e _ b l o b 2
 *
 **************************************
 *
 * Functional description
 *	Open an existing blob.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		// Validate data length

		CHECK_LENGTH(port, bpb_length);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_create_blob2;
		P_BLOB* p_blob = &packet->p_blob;
		p_blob->p_blob_transaction = transaction->rtr_id;
		p_blob->p_blob_bpb.cstr_length = bpb_length;
		fb_assert(!p_blob->p_blob_bpb.cstr_allocated ||
			p_blob->p_blob_bpb.cstr_allocated < p_blob->p_blob_bpb.cstr_length);
		// CVC: Should we ensure here that cstr_allocated < bpb_length???
		// Otherwise, xdr_cstring() calling alloc_string() to decode would
		// cause memory problems on the client side for SS, as the client
		// would try to write to the application's provided R/O buffer.
		p_blob->p_blob_bpb.cstr_address = bpb;

		try
		{
			send_and_receive(status, rdb, packet);
		}
		catch (const Exception&)
		{
			p_blob->p_blob_bpb.cstr_length = 0;
			p_blob->p_blob_bpb.cstr_address = NULL;

			throw;
		}

		p_blob->p_blob_bpb.cstr_length = 0;
		p_blob->p_blob_bpb.cstr_address = NULL;

		Rbl* blob = FB_NEW Rbl();
		*blob_id = packet->p_resp.p_resp_blob_id;
		blob->rbl_rdb = rdb;
		blob->rbl_rtr = transaction;
		blob->rbl_id = packet->p_resp.p_resp_object;
		blob->rbl_flags |= Rbl::CREATE;
		SET_OBJECT(rdb, blob, blob->rbl_id);
		blob->rbl_next = transaction->rtr_blobs;
		transaction->rtr_blobs = blob;

		Firebird::IBlob* b = FB_NEW Blob(blob);
		b->addRef();
		return b;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


Firebird::IAttachment* RProvider::create(CheckStatusWrapper* status, const char* filename,
	unsigned int dpb_length, const unsigned char* dpb, bool loopback)
{
/**************************************
 *
 *	g d s _ c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Create a nice, squeeky clean database, uncorrupted by user data.
 *
 **************************************/

	try
	{
		reset(status);

		ClumpletWriter newDpb(ClumpletReader::dpbList, MAX_DPB_SIZE,
			reinterpret_cast<const UCHAR*>(dpb), dpb_length);
		unsigned flags = ANALYZE_MOUNTS;

		if (get_new_dpb(newDpb, dpbParam))
			flags |= ANALYZE_USER_VFY;

		if (loopback)
			flags |= ANALYZE_LOOPBACK;

		PathName expanded_name(filename);
		PathName node_name;

		ClntAuthBlock cBlock(&expanded_name, &newDpb, &dpbParam);
		rem_port* port = analyze(cBlock, expanded_name, flags, newDpb, dpbParam, node_name, NULL, cryptCallback);

		if (!port)
		{
			Arg::Gds(isc_unavailable).copyTo(status);
			return NULL;
		}

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);
		Rdb* rdb = port->port_context;

		// The client may have set a parameter for dummy_packet_interval.  Add that to the
		// the DPB so the server can pay attention to it.  Note: allocation code must
		// ensure sufficient space has been added.

		add_other_params(port, newDpb, dpbParam);
		add_working_directory(newDpb, node_name);

		IntlDpb intl;
		init(status, cBlock, port, op_create, expanded_name, newDpb, intl, cryptCallback);

		Firebird::IAttachment* a = FB_NEW Attachment(rdb, filename);
		a->addRef();
		return a;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


IAttachment* RProvider::createDatabase(CheckStatusWrapper* status, const char* fileName,
	unsigned int dpbLength, const unsigned char* dpb)
{
/**************************************
 *
 *	g d s _ c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Create a nice, squeeky clean database, uncorrupted by user data.
 *
 **************************************/

	return create(status, fileName, dpbLength, dpb, false);
}


IAttachment* Loopback::createDatabase(CheckStatusWrapper* status, const char* fileName,
	unsigned int dpbLength, const unsigned char* dpb)
{
/**************************************
 *
 *	g d s _ c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Create a nice, squeeky clean database, uncorrupted by user data.
 *
 **************************************/

	return create(status, fileName, dpbLength, dpb, true);
}


void Attachment::getInfo(CheckStatusWrapper* status,
						 unsigned int item_length, const unsigned char* items,
						 unsigned int buffer_length, unsigned char* buffer)
{
/**************************************
 *
 *	g d s _ d a t a b a s e _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on database object.
 *
 **************************************/
	try
	{
		reset(status);

		HalfStaticArray<UCHAR, 1024> temp;

		CHECK_HANDLE(rdb, isc_bad_db_handle);

		rem_port* port = rdb->rdb_port;
		USHORT protocol = memchr(items, fb_info_protocol_version, item_length) ? port->port_protocol : 0;
		protocol &= FB_PROTOCOL_MASK;

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		UCHAR* temp_buffer = temp.getBuffer(buffer_length);

		info(status, rdb, op_info_database, rdb->rdb_id, 0,
			 item_length, items, 0, 0, buffer_length, temp_buffer);

		string version;
		port->versionInfo(version);

		MERGE_database_info(temp_buffer, buffer, buffer_length,
							DbImplementation::current.backwardCompatibleImplementation(), 3, 1,
							reinterpret_cast<const UCHAR*>(version.c_str()),
							reinterpret_cast<const UCHAR*>(port->port_host->str_data),
							protocol);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Attachment::executeDyn(CheckStatusWrapper* status, ITransaction* apiTra, unsigned int length,
	const unsigned char* dyn)
{
/**************************************
 *
 *	g d s _ d d l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		// Validate data length

		CHECK_LENGTH(port, length);

		// Make up a packet for the remote guy

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_ddl;
		P_DDL* ddl = &packet->p_ddl;
		ddl->p_ddl_database = rdb->rdb_id;
		ddl->p_ddl_transaction = transaction->rtr_id;
		ddl->p_ddl_blr.cstr_length = length;
		ddl->p_ddl_blr.cstr_address = dyn;

		send_and_receive(status, rdb, packet);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Attachment::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	g d s _ d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Close down a database.
 *
 **************************************/
	try
	{
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RemotePortGuard portGuard(port, FB_FUNCTION);

		try
		{
			if (!(port->port_flags & PORT_rdb_shutdown))
			{
				release_object(status, rdb, op_detach, rdb->rdb_id);
			}
		}
		catch (const status_exception& ex)
		{
			// If something other than a network error occurred, just return.  Otherwise
			// we need to free up the associated structures, close the socket and
			// scream.  By the way, we should probably create an entry in the log
			// telling the user that an unrecoverable network error occurred and that
			// if there was any uncommitted work, its gone......  Oh well....
			ex.stuffException(status);

			if (!fb_utils::isNetworkError(status->getErrors()[1]) && (!force))
			{
				return;
			}
		}

		while (rdb->rdb_events)
			release_event(rdb->rdb_events);

		while (rdb->rdb_requests)
			release_request(rdb->rdb_requests);

		while (rdb->rdb_sql_requests)
			release_sql_request(rdb->rdb_sql_requests);

		while (rdb->rdb_transactions)
			release_transaction(rdb->rdb_transactions);

		if (port->port_statement)
			release_statement(&port->port_statement);

		// If there is a network error, don't try to send another packet, just
		// free the packet and disconnect the port. Put something into firebird.log
		// informing the user of the following.

		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		{
			iscLogStatus("REMOTE INTERFACE/gds__detach: Unsuccessful detach from "
					"database.\n\tUncommitted work may have been lost.", status);
			reset(status);
		}

		disconnect(port);
		rdb = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Attachment::internalDetach(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Close down a database.
 *
 **************************************/
	reset(status);
	freeClientData(status);
}


void Attachment::detach(CheckStatusWrapper* status)
{
	internalDetach(status);
	if (status->isEmpty())
		release();
}


void Attachment::deprecatedDetach(CheckStatusWrapper* status)
{
	internalDetach(status);
}


void Attachment::internalDropDatabase(CheckStatusWrapper* status)
{
/**************************************
 *
 *	i s c _ d r o p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Close down and purge a database.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RemotePortGuard portGuard(port, FB_FUNCTION);

		try
		{
			release_object(status, rdb, op_drop_database, rdb->rdb_id);
		}
		catch (const status_exception& ex)
		{
			ex.stuffException(status);
			if (ex.value()[1] != isc_drdb_completed_with_errs)
			{
				return;
			}
		}

		while (rdb->rdb_events)
			release_event(rdb->rdb_events);

		while (rdb->rdb_requests)
			release_request(rdb->rdb_requests);

		while (rdb->rdb_sql_requests)
			release_sql_request(rdb->rdb_sql_requests);

		while (rdb->rdb_transactions)
			release_transaction(rdb->rdb_transactions);

		if (port->port_statement)
			release_statement(&port->port_statement);

		disconnect(port);
		rdb = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Attachment::dropDatabase(CheckStatusWrapper* status)
{
	internalDropDatabase(status);
	if (status->isEmpty())
		release();
}


void Attachment::deprecatedDropDatabase(CheckStatusWrapper* status)
{
	internalDropDatabase(status);
}


SLONG Attachment::getSingleInfo(CheckStatusWrapper* status, UCHAR infoItem)
{
	UCHAR buff[16];

	getInfo(status, 1, &infoItem, sizeof(buff), buff);
	if (status->getState() & IStatus::STATE_ERRORS)
		return 0;

	const UCHAR* p = buff;
	const UCHAR* const end = buff + sizeof(buff);
	UCHAR item;
	while ((item = *p++) != isc_info_end && p < end - 1)
	{
		const SLONG length = gds__vax_integer(p, 2);
		p += 2;

		if (item == infoItem)
			return gds__vax_integer(p, (SSHORT)length);

		fb_assert(false);

		p += length;
	}
	return 0;
}


void Attachment::execWithCheck(CheckStatusWrapper* status, const string& stmt)
{
/**************************************
 *
 *	Used to execute "SET xxx TIMEOUT" statements. Checks for protocol version
 *  and convert expected SQL error into isc_wish_list error. The only possible
 *  case is when modern network server works with legacy engine.
 *
 **************************************/
	if (rdb->rdb_port->port_protocol >= PROTOCOL_STMT_TOUT)
	{
		execute(status, NULL, stmt.length(), stmt.c_str(), SQL_DIALECT_CURRENT, NULL, NULL, NULL, NULL);

		if (!(status->getState() & IStatus::STATE_ERRORS))
			return;

		// handle isc_dsql_token_unk_err
		const ISC_STATUS* errs = status->getErrors();

		if (!fb_utils::containsErrorCode(errs, isc_sqlerr) ||
			!fb_utils::containsErrorCode(errs, isc_dsql_token_unk_err))
		{
			return;
		}

		status->init();
	}

	status->setErrors(Arg::Gds(isc_wish_list).value());
}


unsigned int Attachment::getIdleTimeout(CheckStatusWrapper* status)
{
	if (rdb->rdb_port->port_protocol >= PROTOCOL_STMT_TOUT)
		return getSingleInfo(status, fb_info_ses_idle_timeout_att);

	status->setErrors(Arg::Gds(isc_wish_list).value());
	return 0;
}


void Attachment::setIdleTimeout(CheckStatusWrapper* status, unsigned int timeOut)
{
	string stmt;
	stmt.printf("SET SESSION IDLE TIMEOUT %lu", timeOut);

	execWithCheck(status, stmt);
}


unsigned int Attachment::getStatementTimeout(CheckStatusWrapper* status)
{
	if (rdb->rdb_port->port_protocol >= PROTOCOL_STMT_TOUT)
		return getSingleInfo(status, fb_info_statement_timeout_att);

	status->setErrors(Arg::Gds(isc_wish_list).value());
	return 0;
}


void Attachment::setStatementTimeout(CheckStatusWrapper* status, unsigned int timeOut)
{
	string stmt;
	stmt.printf("SET STATEMENT TIMEOUT %lu", timeOut);

	execWithCheck(status, stmt);
}


Batch* Attachment::createBatch(CheckStatusWrapper* status, ITransaction* transaction,
	unsigned stmtLength, const char* sqlStmt, unsigned dialect,
	IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par)
{
/**************************************
 *
 *	c r e a t e B a t c h
 *
 **************************************
 *
 * Functional description
 *	Create jdbc-style batch for SQL statement.
 *
 **************************************/
	Statement* stmt = prepare(status, transaction, stmtLength, sqlStmt, dialect, 0);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
	{
		return NULL;
	}

	Batch* rc = stmt->createBatch(status, inMetadata, parLength, par);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
	{
		stmt->release();
		return NULL;
	}

	rc->tmpStatement = true;
	return rc;
}


Batch* Statement::createBatch(CheckStatusWrapper* status, IMessageMetadata* inMetadata,
	unsigned parLength, const unsigned char* par)
{
/**************************************
 *
 *	c r e a t e B a t c h
 *
 **************************************
 *
 * Functional description
 *	Create jdbc-style batch for prepared statement.
 *
 **************************************/

	try
	{
		reset(status);

		// Check and validate handles, etc.
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;

		if (port->port_protocol < PROTOCOL_VERSION16)
			unsupported();

		// Build input BLR
		RefPtr<IMessageMetadata> meta;
		if (!inMetadata)
		{
			meta.assignRefNoIncr(getInputMetadata(status));
			check(status);
			inMetadata = meta;
		}

		BlrFromMessage inBlr(inMetadata, dialect, port->port_protocol);
		const unsigned int in_blr_length = inBlr.getLength();
		const UCHAR* const in_blr = inBlr.getBytes();

		// Validate data length
		CHECK_LENGTH(port, in_blr_length);

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		delete statement->rsr_bind_format;
		statement->rsr_bind_format = NULL;

		if (port->port_statement)
		{
			delete port->port_statement->rsr_select_format;
			port->port_statement->rsr_select_format = NULL;
		}

		// Parse the blr describing the message, if there is any.
		if (in_blr_length)
			statement->rsr_bind_format = PARSE_msg_format(in_blr, in_blr_length);

		RMessage* message = NULL;
		if (!statement->rsr_buffer)
		{
			statement->rsr_buffer = message = FB_NEW RMessage(0);
			statement->rsr_message = message;

			message->msg_next = message;

			statement->rsr_fmt_length = 0;
		}
		else
			message = statement->rsr_message = statement->rsr_buffer;

		statement->rsr_flags.clear(Rsr::FETCHED);
		statement->rsr_format = statement->rsr_bind_format;
		statement->rsr_batch_stream.blobRemaining = 0;
		statement->clearException();

		// set up the packet for the other guy...

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_batch_create;
		P_BATCH_CREATE* batch = &packet->p_batch_create;
		batch->p_batch_statement = statement->rsr_id;
		batch->p_batch_blr.cstr_length = in_blr_length;
		batch->p_batch_blr.cstr_address = in_blr;
		batch->p_batch_msglen = inMetadata->getMessageLength(status);
		check(status);
		batch->p_batch_pb.cstr_length = parLength;
		batch->p_batch_pb.cstr_address = par;

		if (port->port_flags & PORT_lazy)
		{
			send_partial_packet(port, packet);
			defer_packet(port, packet, true);
		}
		else {
			send_and_receive(status, rdb, packet);
		}

		message->msg_address = NULL;

		Batch* b = FB_NEW Batch(this, inMetadata, parLength, par);
		b->addRef();
		return b;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}


Batch::Batch(Statement* s, IMessageMetadata* inFmt, unsigned parLength, const unsigned char* par)
	: messageStream(0), blobStream(nullptr), sizePointer(nullptr),
	  messageSize(0), alignedSize(0), blobBufferSize(0), messageBufferSize(0), flags(0),
	  stmt(s), format(inFmt), blobAlign(0), blobPolicy(BLOB_NONE),
	  segmented(false), defSegmented(false), batchActive(false),
	  messageCount(0), blobCount(0), serverSize(0), blobHeadSize(0),
	  tmpStatement(false)
{
	LocalStatus ls;
	CheckStatusWrapper st(&ls);

	messageSize = format->getMessageLength(&st);
	check(&st);
	alignedSize = format->getAlignedLength(&st);
	check(&st);

	memset(&genId, 0, sizeof(genId));

	ClumpletReader rdr(ClumpletReader::WideTagged, par, parLength);

	for (rdr.rewind(); !rdr.isEof(); rdr.moveNext())
	{
		UCHAR t = rdr.getClumpTag();

		switch (t)
		{
		case TAG_MULTIERROR:
		case TAG_RECORD_COUNTS:
			if (rdr.getInt())
				flags |= (1 << t);
			else
				flags &= ~(1 << t);
			break;

		case TAG_BLOB_POLICY:
			blobPolicy = rdr.getInt();

			switch (blobPolicy)
			{
			case BLOB_ID_ENGINE:
			case BLOB_ID_USER:
			case BLOB_STREAM:
				break;
			default:
				blobPolicy = BLOB_NONE;
				break;
			}

			break;
		}
	}
	s->getStatement()->rsr_batch_flags = flags;

	// allocate buffers
	Rsr* statement = stmt->getStatement();
	CHECK_HANDLE(statement, isc_bad_req_handle);
	Rdb* rdb = statement->rsr_rdb;
	CHECK_HANDLE(rdb, isc_bad_db_handle);
	rem_port* port = rdb->rdb_port;
	blobBufferSize = port->getPortConfig()->getClientBatchBuffer();
	messageBufferSize = blobBufferSize / alignedSize;
	if (!messageBufferSize)
		messageBufferSize = 1;

	messageStreamBuffer.reset(FB_NEW UCHAR[messageBufferSize * alignedSize]);
	if (blobPolicy != BLOB_NONE)
	{
		blobStreamBuffer.reset(FB_NEW UCHAR[blobBufferSize]);
		blobStream = blobStreamBuffer;
	}
}


void Batch::add(CheckStatusWrapper* status, unsigned count, const void* inBuffer)
{
	try
	{
		// Check and validate handles, etc.

		if (!stmt)
		{
			Arg::Gds(isc_bad_req_handle).raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;

		if (count == 0)
			return;

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);
		putMessageData(count, inBuffer);

		batchActive = true;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Batch::sendMessagePacket(unsigned count, const UCHAR* ptr, bool flash)
{
	Rsr* statement = stmt->getStatement();
	CHECK_HANDLE(statement, isc_bad_req_handle);
	Rdb* rdb = statement->rsr_rdb;
	CHECK_HANDLE(rdb, isc_bad_db_handle);
	rem_port* port = rdb->rdb_port;

	PACKET* packet = &rdb->rdb_packet;
	packet->p_operation = op_batch_msg;
	P_BATCH_MSG* batch = &packet->p_batch_msg;
	batch->p_batch_statement = statement->rsr_id;
	batch->p_batch_messages = count;
	batch->p_batch_data.cstr_address = const_cast<UCHAR*>(ptr);
	statement->rsr_batch_size = alignedSize;

	sendDeferredPacket(nullptr, port, packet, flash);
	messageCount += count;
}


void Batch::addBlob(CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
	unsigned parLength, const unsigned char* par)
{
	try
	{
		// Check and validate handles, etc.
		if (!stmt)
		{
			Arg::Gds(isc_bad_req_handle).raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Policy check
		switch (blobPolicy)
		{
		case IBatch::BLOB_ID_ENGINE:
			genBlobId(blobId);
			break;
		case IBatch::BLOB_ID_USER:
			break;
		default:
			(Arg::Gds(isc_batch_policy) << "addBlob").raise();
		}

		// Build blob HDR in stream
		newBlob();
		putBlobData(sizeof *blobId, blobId);
		setSizePointer();
		putBlobData(sizeof parLength, &parLength);
		putBlobData(sizeof parLength, &parLength);
		putBlobData(parLength, par);
		segmented = parLength ? fb_utils::isBpbSegmented(parLength, par) : defSegmented;

		// Store blob data
		putSegment(length, inBuffer);

		batchActive = true;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Batch::appendBlobData(CheckStatusWrapper* status, unsigned length, const void* inBuffer)
{
	try
	{
		// Check and validate handles, etc.
		if (!stmt)
		{
			Arg::Gds(isc_bad_req_handle).raise();
		}

		// Policy check
		switch (blobPolicy)
		{
		case IBatch::BLOB_ID_USER:
		case IBatch::BLOB_ID_ENGINE:
			break;
		default:
			(Arg::Gds(isc_batch_policy) << "appendBlobData").raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Store blob data
		putSegment(length, inBuffer);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Batch::addBlobStream(CheckStatusWrapper* status, unsigned length, const void* inBuffer)
{
	try
	{
		// Check and validate handles, etc.
		if (!stmt)
		{
			Arg::Gds(isc_bad_req_handle).raise();
		}

		// Policy check
		if (blobPolicy != IBatch::BLOB_STREAM)
		{
			(Arg::Gds(isc_batch_policy) << "addBlobStream").raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Store stream data
		putBlobData(length, inBuffer);

		batchActive = true;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Batch::sendBlobPacket(unsigned size, const UCHAR* ptr, bool flash)
{
	Rsr* statement = stmt->getStatement();
	Rdb* rdb = statement->rsr_rdb;
	rem_port* port = rdb->rdb_port;

	setServerInfo();
	fb_assert(!(size % blobAlign));

	PACKET* packet = &rdb->rdb_packet;
	packet->p_operation = op_batch_blob_stream;
	P_BATCH_BLOB* batch = &packet->p_batch_blob;
	batch->p_batch_statement = statement->rsr_id;
	batch->p_batch_blob_data.cstr_address = const_cast<UCHAR*>(ptr);
	batch->p_batch_blob_data.cstr_length = size;

	sendDeferredPacket(nullptr, port, packet, flash);

	blobCount += size;
}


void Batch::sendDeferredPacket(IStatus* status, rem_port* port, PACKET* packet, bool flash)
{
	if (port->port_flags & PORT_lazy)
	{
		send_partial_packet(port, packet);
		defer_packet(port, packet, true);

		if ((port->port_protocol >= PROTOCOL_VERSION17) &&
			((port->port_deferred_packets->getCount() >= DEFER_BATCH_LIMIT) || flash))
		{
			packet->p_operation = op_batch_sync;
			send_packet(port, packet);
			receive_packet(port, packet);

			LocalStatus warning;
			port->checkResponse(&warning, packet, false);
			Rsr* statement = stmt->getStatement();
			if (statement->haveException())
			{
				cleanup();
				statement->raiseException();
			}
		}
	}
	else if (status)
	{
		send_and_receive(status, port->port_context, packet);
	}
	else
	{
		LocalStatus local;
		send_and_receive(&local, port->port_context, packet);
	}
}


void Batch::setDefaultBpb(CheckStatusWrapper* status, unsigned parLength, const unsigned char* par)
{
	try
	{
		// Check and validate handles, etc.
		if (!stmt)
			Arg::Gds(isc_bad_req_handle).raise();

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Check for presence of any data in batch buffers
		if (batchHasData())
			Arg::Gds(isc_batch_defbpb).raise();

		// Set default segmentation flag
		defSegmented = fb_utils::isBpbSegmented(parLength, par);

		// Prepare and send the packet
		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_batch_set_bpb;
		P_BATCH_SETBPB* batch = &packet->p_batch_setbpb;
		batch->p_batch_statement = statement->rsr_id;
		batch->p_batch_blob_bpb.cstr_address = par;
		batch->p_batch_blob_bpb.cstr_length = parLength;

		sendDeferredPacket(status, port, packet, true);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


unsigned Batch::getBlobAlignment(CheckStatusWrapper* status)
{
	try
	{
		setServerInfo();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return blobAlign;
}


void Batch::setServerInfo()
{
	if (blobAlign)
		return;

	// Check and validate handles, etc.
	if (!stmt)
	{
		Arg::Gds(isc_bad_req_handle).raise();
	}

	Rsr* statement = stmt->getStatement();
	CHECK_HANDLE(statement, isc_bad_req_handle);
	Rdb* rdb = statement->rsr_rdb;
	CHECK_HANDLE(rdb, isc_bad_db_handle);
	rem_port* port = rdb->rdb_port;
	RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

	LocalStatus ls;
	CheckStatusWrapper s(&ls);

	if (port->port_protocol < PROTOCOL_VERSION17)
	{
		UCHAR item = isc_info_sql_stmt_blob_align;
		UCHAR buffer[16];
		info(&s, rdb, op_info_sql, statement->rsr_id, 0,
			 1, &item, 0, 0, sizeof(buffer), buffer);
		check(&s);

		// Extract from buffer
		if (buffer[0] != item)
			Arg::Gds(isc_batch_align).raise();

		int len = gds__vax_integer(&buffer[1], 2);
		statement->rsr_batch_stream.alignment = blobAlign = gds__vax_integer(&buffer[3], len);

		if (!blobAlign)
			Arg::Gds(isc_batch_align).raise();

		return;
	}

	// Perform info call to server
	UCHAR items[] = {IBatch::INF_BLOB_ALIGNMENT, IBatch::INF_BUFFER_BYTES_SIZE, IBatch::INF_BLOB_HEADER};
	UCHAR buffer[64];
	info(&s, rdb, op_info_batch, statement->rsr_id, 0,
		 sizeof(items), items, 0, 0, sizeof(buffer), buffer);
	check(&s);

	// Extract from buffer
	ClumpletReader out(ClumpletReader::InfoResponse, buffer, sizeof(buffer));
	for (out.rewind(); !out.isEof(); out.moveNext())
	{
		UCHAR item = out.getClumpTag();
		if (item == isc_info_end)
			break;

		switch(item)
		{
		case IBatch::INF_BLOB_ALIGNMENT:
			statement->rsr_batch_stream.alignment = blobAlign = out.getInt();
			break;
		case IBatch::INF_BUFFER_BYTES_SIZE:
			serverSize = out.getInt();
			break;
		case IBatch::INF_BLOB_HEADER:
			blobHeadSize = out.getInt();
			break;
		case isc_info_error:
			(Arg::Gds(isc_batch_align) << Arg::Gds(out.getInt())).raise();
		case isc_info_truncated:
			(Arg::Gds(isc_batch_align) << Arg::Gds(isc_random) << "truncated").raise();
		default:
			{
				string msg;
				msg.printf("Wrong info item %u", item);
				(Arg::Gds(isc_batch_align) << Arg::Gds(isc_random) << msg).raise();
			}
		}
	}

	if (! (blobAlign && serverSize && blobHeadSize))
		Arg::Gds(isc_batch_align).raise();
}


IMessageMetadata* Batch::getMetadata(CheckStatusWrapper* status)
{
	reset(status);

	format->addRef();
	return format;
}


void Batch::registerBlob(CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId)
{
	try
	{
		// Check and validate handles, etc.

		if (!stmt)
		{
			Arg::Gds(isc_bad_req_handle).raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		if (blobPolicy == IBatch::BLOB_ID_ENGINE)
			genBlobId(blobId);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_batch_regblob;
		P_BATCH_REGBLOB* batch = &packet->p_batch_regblob;
		batch->p_batch_statement = statement->rsr_id;
		batch->p_batch_exist_id = *existingBlob;
		batch->p_batch_blob_id = *blobId;

		sendDeferredPacket(status, port, packet, true);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


IBatchCompletionState* Batch::execute(CheckStatusWrapper* status, ITransaction* apiTra)
{
	try
	{
		// Check and validate handles, etc.

		if (!stmt)
		{
			Arg::Gds(isc_bad_req_handle).raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);

		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);

		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = NULL;
		Transaction* rt = stmt->getAttachment()->remoteTransactionInterface(apiTra);
		if (rt)
		{
			transaction = rt->getTransaction();
			CHECK_HANDLE(transaction, isc_bad_trans_handle);
		}

		// Sanity checks complete - flash data in buffers
		flashBatch();

		// Prepare and send execute packet
		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_batch_exec;
		P_BATCH_EXEC* batch = &packet->p_batch_exec;
		batch->p_batch_statement = statement->rsr_id;
		batch->p_batch_transaction = transaction->rtr_id;
		send_packet(port, packet);

		statement->rsr_batch_size = alignedSize;
		AutoPtr<BatchCompletionState, SimpleDispose>
			cs(FB_NEW BatchCompletionState(flags & (1 << IBatch::TAG_RECORD_COUNTS), 256));
		statement->rsr_batch_cs = cs;
		receive_packet(port, packet);
		statement->rsr_batch_cs = nullptr;

		if (packet->p_operation == op_batch_cs)
		{
			// when working with 4.0.0 server we could not raise it in advance...
			statement->clearException();

			return cs.release();
		}

		REMOTE_check_response(status, rdb, packet);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return nullptr;
}


void Batch::cancel(CheckStatusWrapper* status)
{
	try
	{
		// Check and validate handles, etc.
		if (!stmt)
		{
			Arg::Gds(isc_dsql_cursor_err).raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Cleanup local data
		cleanup();
		batchActive = false;

		// Prepare packet
		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_batch_cancel;

		P_BATCH_FREE_CANCEL* batch = &packet->p_batch_free_cancel;
		batch->p_batch_statement = statement->rsr_id;

		send_and_receive(status, rdb, packet);

		batchActive = false;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Batch::freeClientData(CheckStatusWrapper* status, bool force)
{
	try
	{
		// Check and validate handles, etc.

		if (!stmt)
		{
			Arg::Gds(isc_dsql_cursor_err).raise();
		}

		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_batch_rls;

		P_BATCH_FREE_CANCEL* batch = &packet->p_batch_free_cancel;
		batch->p_batch_statement = statement->rsr_id;

		if (rdb->rdb_port->port_flags & PORT_lazy)
		{
			defer_packet(rdb->rdb_port, packet);
			packet->p_resp.p_resp_object = statement->rsr_id;
		}
		else
		{
			try
			{
				send_and_receive(status, rdb, packet);
			}
			catch (const Exception&)
			{
				if (!force)
					throw;
			}
		}

		releaseStatement();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Batch::internalClose(CheckStatusWrapper* status)
{
	reset(status);
	freeClientData(status);
}


void Batch::close(CheckStatusWrapper* status)
{
	internalClose(status);
	if (status->isEmpty())
		release();
}


void Batch::deprecatedClose(CheckStatusWrapper* status)
{
	internalClose(status);
}


void Batch::getInfo(CheckStatusWrapper* status, unsigned int itemsLength, const unsigned char* items,
	unsigned int bufferLength, unsigned char* buffer)
{
	try
	{
		ClumpletReader it(ClumpletReader::InfoItems, items, itemsLength);
		ClumpletWriter out(ClumpletReader::InfoResponse, bufferLength - 1);		// place for isc_info_end / isc_info_truncated

		for (it.rewind(); !it.isEof(); it.moveNext())
		{
			UCHAR item = it.getClumpTag();
			if (item == isc_info_end)
				break;

			try
			{
				switch(item)
				{
				case IBatch::INF_BUFFER_BYTES_SIZE:
					setServerInfo();
					if (serverSize)
						out.insertInt(item, serverSize);
					break;
				case IBatch::INF_DATA_BYTES_SIZE:
					out.insertInt(item, (messageCount + messageStream) * alignedSize);
					break;
				case IBatch::INF_BLOBS_BYTES_SIZE:
					if (blobStream)
						out.insertInt(item, blobCount + (blobStream - blobStreamBuffer));
					break;
				case IBatch::INF_BLOB_ALIGNMENT:
					setServerInfo();
					out.insertInt(item, blobAlign);
					break;
				case IBatch::INF_BLOB_HEADER:
					setServerInfo();
					out.insertInt(item, blobHeadSize);
					break;
				default:
					out.insertInt(isc_info_error, isc_infunk);
					break;
				}
			}
			catch(const fatal_exception&)
			{
				// here it's sooner of all caused by writer overflow but anyway check that
				if (out.hasOverflow())
				{
					memcpy(buffer, out.getBuffer(), out.getBufferLength());
					buffer += out.getBufferLength();
					*buffer++ = isc_info_truncated;
					if (out.getBufferLength() <= bufferLength - 2)
						*buffer++ = isc_info_end;
					return;
				}
				else
					throw;
			}
		}

		memcpy(buffer, out.getBuffer(), out.getBufferLength());
		buffer += out.getBufferLength();
		*buffer++ = isc_info_end;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Batch::releaseStatement()
{
	if (tmpStatement)
	{
		stmt->release();
	}

	stmt = NULL;
}


Replicator* Attachment::createReplicator(CheckStatusWrapper* status)
{
/**************************************
 *
 *	c r e a t e R e p l i c a t o r
 *
 **************************************
 *
 * Functional description
 *	Create data replication interface.
 *
 **************************************/

	try
	{
		reset(status);

		// Check and validate handles, etc.
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;

		if (port->port_protocol < PROTOCOL_VERSION16)
			unsupported();

		if (!replicator)
			replicator = FB_NEW Replicator(this);

		replicator->addRef();
		return replicator;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}


void Replicator::process(CheckStatusWrapper* status, unsigned length, const unsigned char* data)
{
	try
	{
		reset(status);

		Rdb* rdb = attachment->getRdb();
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;

		if (port->port_protocol < PROTOCOL_VERSION16)
			unsupported();

		// Validate data length
		CHECK_LENGTH(port, length);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_repl_data;
		P_REPLICATE* repl = &packet->p_replicate;
		repl->p_repl_database = rdb->rdb_id;
		repl->p_repl_data.cstr_length = length;
		repl->p_repl_data.cstr_address = data;

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		send_and_receive(status, rdb, packet);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Replicator::internalClose(CheckStatusWrapper* status)
{
	reset(status);
	freeClientData(status);
}


void Replicator::close(CheckStatusWrapper* status)
{
	internalClose(status);
	if (status->isEmpty())
		release();
}


void Replicator::deprecatedClose(CheckStatusWrapper* status)
{
	internalClose(status);
}


void Replicator::freeClientData(CheckStatusWrapper* status, bool force)
{
	try
	{
		reset(status);

		if (attachment && attachment->replicator)
		{
			Rdb* rdb = attachment->getRdb();
			CHECK_HANDLE(rdb, isc_bad_db_handle);
			rem_port* port = rdb->rdb_port;

			if (port->port_protocol < PROTOCOL_VERSION16)
				unsupported();

			PACKET* packet = &rdb->rdb_packet;
			packet->p_operation = op_repl_data;
			P_REPLICATE* repl = &packet->p_replicate;
			repl->p_repl_database = rdb->rdb_id;
			repl->p_repl_data.cstr_length = 0;

			RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

			try
			{
				send_and_receive(status, rdb, packet);
			}
			catch (const Exception&)
			{
				if (!force)
					throw;
			}

			attachment->replicator = NULL;
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


ITransaction* Statement::execute(CheckStatusWrapper* status, ITransaction* apiTra,
	IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, void* outBuffer)
{
/**************************************
 *
 *	d s q l _ e x e c u t e 2
 *
 **************************************
 *
 * Functional description
 *	Execute a non-SELECT dynamic SQL statement.
 *
 **************************************/

	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);

		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);

		rem_port* port = rdb->rdb_port;

		BlrFromMessage inBlr(inMetadata, dialect, port->port_protocol);
		const unsigned int in_blr_length = inBlr.getLength();
		const UCHAR* const in_blr = inBlr.getBytes();
		const unsigned int in_msg_length = inBlr.getMsgLength();
		UCHAR* const in_msg = static_cast<UCHAR*>(inBuffer);

		BlrFromMessage outBlr(outMetadata, dialect, port->port_protocol);
		const unsigned int out_blr_length = outBlr.getLength();
		const UCHAR* const out_blr = outBlr.getBytes();
		const unsigned int out_msg_length = outBlr.getMsgLength();
		UCHAR* const out_msg = static_cast<UCHAR*>(outBuffer);

		// Validate data length

		CHECK_LENGTH(port, in_blr_length);
		CHECK_LENGTH(port, in_msg_length);
		CHECK_LENGTH(port, out_blr_length);
		CHECK_LENGTH(port, out_msg_length);

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = NULL;
		Transaction* rt = remAtt->remoteTransactionInterface(apiTra);
		if (rt)
		{
			transaction = rt->getTransaction();
			CHECK_HANDLE(transaction, isc_bad_trans_handle);
		}

		// 24-Mar-2004 Nickolay Samofatov
		// Unconditionally deallocate existing formats that are left from
		// previous executions (possibly with different statement if
		// isc_dsql_prepare is called multiple times).
		// This should cure SF#919246
		delete statement->rsr_bind_format;
		statement->rsr_bind_format = NULL;
		if (port->port_statement)
		{
			delete port->port_statement->rsr_select_format;
			port->port_statement->rsr_select_format = NULL;
		}

		// Parse the blr describing the message, if there is any.

		if (in_blr_length)
			statement->rsr_bind_format = PARSE_msg_format(in_blr, in_blr_length);

		// Parse the blr describing the output message.  This is not the fetch
		// message!  That comes later.

		if (out_blr_length)
		{
			if (!port->port_statement)
				port->port_statement = FB_NEW Rsr;

			port->port_statement->rsr_select_format = PARSE_msg_format(out_blr, out_blr_length);

			if (!port->port_statement->rsr_buffer)
			{
				RMessage* message2 = FB_NEW RMessage(0);
				port->port_statement->rsr_buffer = message2;
				port->port_statement->rsr_message = message2;
				message2->msg_next = message2;
				port->port_statement->rsr_fmt_length = 0;
			}
		}

		RMessage* message = NULL;
		if (!statement->rsr_buffer)
		{
			statement->rsr_buffer = message = FB_NEW RMessage(0);
			statement->rsr_message = message;

			message->msg_next = message;

			statement->rsr_fmt_length = 0;
		}
		else {
			message = statement->rsr_message = statement->rsr_buffer;
		}

		message->msg_address = const_cast<UCHAR*>(in_msg);
		statement->rsr_flags.clear(Rsr::FETCHED);
		statement->rsr_format = statement->rsr_bind_format;
		statement->clearException();

		// set up the packet for the other guy...

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = out_msg_length ? op_execute2 : op_execute;
		P_SQLDATA* sqldata = &packet->p_sqldata;
		sqldata->p_sqldata_statement = statement->rsr_id;
		sqldata->p_sqldata_transaction = transaction ? transaction->rtr_id : 0;
		sqldata->p_sqldata_blr.cstr_length = in_blr_length;
		sqldata->p_sqldata_blr.cstr_address = const_cast<UCHAR*>(in_blr); // safe, see protocol.cpp and server.cpp
		sqldata->p_sqldata_message_number = 0;
		sqldata->p_sqldata_messages = (statement->rsr_bind_format) ? 1 : 0;
		sqldata->p_sqldata_out_blr.cstr_length = out_blr_length;
		sqldata->p_sqldata_out_blr.cstr_address = const_cast<UCHAR*>(out_blr);
		sqldata->p_sqldata_out_message_number = 0;	// out_msg_type
		sqldata->p_sqldata_timeout = statement->rsr_timeout;
		sqldata->p_sqldata_cursor_flags = 0;

		send_packet(port, packet);

		// Set up the response packet.  We may receive an SQL response followed
		// by a normal response packet or simply a response packet.

		message->msg_address = NULL;
		if (out_msg_length)
			port->port_statement->rsr_message->msg_address = out_msg;

		receive_packet(port, packet);

		if (packet->p_operation != op_sql_response)
			REMOTE_check_response(status, rdb, packet);
		else
		{
			port->port_statement->rsr_message->msg_address = NULL;
			receive_response(status, rdb, packet);
		}

		if (transaction && !packet->p_resp.p_resp_object)
		{
			REMOTE_cleanup_transaction(transaction);
			release_transaction(transaction);
			transaction = NULL;
			rt->clear();
			statement->rsr_rtr = NULL;
			return NULL;
		}
		else if (!transaction && packet->p_resp.p_resp_object)
		{
			transaction = make_transaction(rdb, packet->p_resp.p_resp_object);
			statement->rsr_rtr = transaction;
			Transaction* newTrans = FB_NEW Transaction(transaction, remAtt);
			newTrans->addRef();
			return newTrans;
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return apiTra;
}


ResultSet* Statement::openCursor(CheckStatusWrapper* status, Firebird::ITransaction* apiTra,
	IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outFormat, unsigned int flags)
{
/**************************************
 *
 *	d s q l _ e x e c u t e 2
 *
 **************************************
 *
 * Functional description
 *	Execute a non-SELECT dynamic SQL statement.
 *
 **************************************/

	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);

		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);

		rem_port* port = rdb->rdb_port;

		BlrFromMessage inBlr(inMetadata, dialect, port->port_protocol);
		const unsigned int in_blr_length = inBlr.getLength();
		const UCHAR* const in_blr = inBlr.getBytes();
		const unsigned int in_msg_length = inBlr.getMsgLength();
		UCHAR* const in_msg = static_cast<UCHAR*>(inBuffer);

		RefPtr<IMessageMetadata> defaultOutputFormat;
		if (!outFormat)
		{
			defaultOutputFormat.assignRefNoIncr(this->getOutputMetadata(status));
			if (status->getState() & Firebird::IStatus::STATE_ERRORS)
			{
				return NULL;
			}
			if (defaultOutputFormat)
			{
				outFormat = defaultOutputFormat;
			}
		}

		BlrFromMessage outBlr((outFormat == DELAYED_OUT_FORMAT ? NULL : outFormat), dialect, port->port_protocol);
		const unsigned int out_blr_length = outBlr.getLength();
		const UCHAR* const out_blr = outBlr.getBytes();

		// Validate data length

		CHECK_LENGTH(port, in_blr_length);
		CHECK_LENGTH(port, in_msg_length);
		CHECK_LENGTH(port, out_blr_length);

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = NULL;
		Transaction* rt = remAtt->remoteTransactionInterface(apiTra);
		if (rt)
		{
			transaction = rt->getTransaction();
			CHECK_HANDLE(transaction, isc_bad_trans_handle);
		}

		// 24-Mar-2004 Nickolay Samofatov
		// Unconditionally deallocate existing formats that are left from
		// previous executions (possibly with different statement if
		// isc_dsql_prepare is called multiple times).
		// This should cure SF#919246
		delete statement->rsr_bind_format;
		statement->rsr_bind_format = NULL;
		if (port->port_statement)
		{
			delete port->port_statement->rsr_select_format;
			port->port_statement->rsr_select_format = NULL;
		}

		// Parse the blr describing the message, if there is any.

		if (in_blr_length)
			statement->rsr_bind_format = PARSE_msg_format(in_blr, in_blr_length);

		RMessage* message = NULL;
		if (!statement->rsr_buffer)
		{
			statement->rsr_buffer = message = FB_NEW RMessage(0);
			statement->rsr_message = message;

			message->msg_next = message;

			statement->rsr_fmt_length = 0;
		}
		else {
			message = statement->rsr_message = statement->rsr_buffer;
		}

		message->msg_address = const_cast<UCHAR*>(in_msg);
		statement->rsr_flags.clear(Rsr::FETCHED);
		statement->rsr_format = statement->rsr_bind_format;
		statement->clearException();

		// set up the packet for the other guy...

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_execute;
		P_SQLDATA* sqldata = &packet->p_sqldata;
		sqldata->p_sqldata_statement = statement->rsr_id;
		sqldata->p_sqldata_transaction = transaction ? transaction->rtr_id : 0;
		sqldata->p_sqldata_blr.cstr_length = in_blr_length;
		sqldata->p_sqldata_blr.cstr_address = const_cast<UCHAR*>(in_blr); // safe, see protocol.cpp and server.cpp
		sqldata->p_sqldata_message_number = 0;
		sqldata->p_sqldata_messages = (statement->rsr_bind_format) ? 1 : 0;
		sqldata->p_sqldata_out_blr.cstr_length = out_blr_length;
		sqldata->p_sqldata_out_blr.cstr_address = const_cast<UCHAR*>(out_blr);
		sqldata->p_sqldata_out_message_number = 0;	// out_msg_type
		sqldata->p_sqldata_timeout = statement->rsr_timeout;
		sqldata->p_sqldata_cursor_flags = flags;

		send_partial_packet(port, packet);
		defer_packet(port, packet, true);
		message->msg_address = NULL;

		ResultSet* rs = FB_NEW ResultSet(this, outFormat, flags);
		rs->addRef();
		return rs;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


IResultSet* Attachment::openCursor(CheckStatusWrapper* status, ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned dialect,
		IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata,
		const char* cursorName, unsigned int cursorFlags)
{
	Statement* stmt = prepare(status, transaction, stmtLength, sqlStmt, dialect,
		(outMetadata ? 0 : IStatement::PREPARE_PREFETCH_OUTPUT_PARAMETERS));
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
	{
		return NULL;
	}

	ResultSet* rc = stmt->openCursor(status, transaction, inMetadata, inBuffer, outMetadata, cursorFlags);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
	{
		stmt->release();
		return NULL;
	}

	if (cursorName)
	{
		stmt->setCursorName(status, cursorName);
		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		{
			rc->release();
			stmt->release();
			return NULL;
		}
	}

	rc->tmpStatement = true;
	return rc;
}


ITransaction* Attachment::execute(CheckStatusWrapper* status, ITransaction* apiTra,
	unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
	IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, void* outBuffer)
{
/**************************************
 *
 *	d s q l _ e x e c u t e _ i m m e d i a t e 2
 *
 **************************************
 *
 * Functional description
 *	Prepare and execute a statement.
 *
 **************************************/

	try
	{
		// Check and validate handles, etc.

		CHECK_HANDLE(rdb, isc_bad_db_handle);

		rem_port* port = rdb->rdb_port;

		BlrFromMessage inBlr(inMetadata, dialect, port->port_protocol);
		const unsigned int in_blr_length = inBlr.getLength();
		const UCHAR* const in_blr = inBlr.getBytes();
		const unsigned int in_msg_length = inBlr.getMsgLength();
		UCHAR* const in_msg = static_cast<UCHAR*>(inBuffer);

		BlrFromMessage outBlr(outMetadata, dialect, port->port_protocol);
		const unsigned int out_blr_length = outBlr.getLength();
		const UCHAR* const out_blr = outBlr.getBytes();
		const unsigned int out_msg_length = outBlr.getMsgLength();
		UCHAR* const out_msg = static_cast<UCHAR*>(outBuffer);

		// Validate data length

		CHECK_LENGTH(port, in_blr_length);
		CHECK_LENGTH(port, in_msg_length);
		CHECK_LENGTH(port, out_blr_length);
		CHECK_LENGTH(port, out_msg_length);

		if (sqlStmt && !stmtLength)
			stmtLength = static_cast<ULONG>(strlen(sqlStmt));

		// Validate string length

		CHECK_LENGTH(port, stmtLength);

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = NULL;
		Transaction* rt = remoteTransactionInterface(apiTra);
		if (rt)
		{
			transaction = rt->getTransaction();
			CHECK_HANDLE(transaction, isc_bad_trans_handle);
		}

		if (dialect > 10)
		{
			// dimitr: adjust dialect received after
			//		   a multi-hop transmission to be
			//		   redirected in its original value.
			dialect /= 10;
		}

		reset(status);

		Rsr* statement = port->port_statement;
		if (!statement) {
			statement = port->port_statement = FB_NEW Rsr;
		}

		// reset statement buffers

		clear_queue(rdb->rdb_port);

		REMOTE_reset_statement(statement);

		delete statement->rsr_bind_format;
		statement->rsr_bind_format = NULL;
		delete statement->rsr_select_format;
		statement->rsr_select_format = NULL;

		if (in_msg_length || out_msg_length)
		{
			if (in_blr_length)
				statement->rsr_bind_format = PARSE_msg_format(in_blr, in_blr_length);

			if (out_blr_length)
				statement->rsr_select_format = PARSE_msg_format(out_blr, out_blr_length);
		}

		RMessage* message = 0;
		if (!statement->rsr_buffer)
		{
			statement->rsr_buffer = message = FB_NEW RMessage(0);
			statement->rsr_message = message;
			message->msg_next = message;
			statement->rsr_fmt_length = 0;
		}
		else {
			message = statement->rsr_message = statement->rsr_buffer;
		}

		message->msg_address = const_cast<UCHAR*>(in_msg);

		statement->clearException();

		// set up the packet for the other guy...

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = (in_msg_length || out_msg_length) ?
			op_exec_immediate2 : op_exec_immediate;
		P_SQLST* ex_now = &packet->p_sqlst;
		ex_now->p_sqlst_transaction = transaction ? transaction->rtr_id : 0;
		ex_now->p_sqlst_SQL_dialect = dialect;
		ex_now->p_sqlst_SQL_str.cstr_length = stmtLength;
		ex_now->p_sqlst_SQL_str.cstr_address = reinterpret_cast<const UCHAR*>(sqlStmt);
		ex_now->p_sqlst_items.cstr_length = 0;
		ex_now->p_sqlst_buffer_length = 0;
		ex_now->p_sqlst_blr.cstr_length = in_blr_length;
		ex_now->p_sqlst_blr.cstr_address = const_cast<UCHAR*>(in_blr);
		ex_now->p_sqlst_message_number = 0;
		ex_now->p_sqlst_messages = (in_msg_length && statement->rsr_bind_format) ? 1 : 0;
		ex_now->p_sqlst_out_blr.cstr_length = out_blr_length;
		ex_now->p_sqlst_out_blr.cstr_address = const_cast<unsigned char*>(out_blr);
		ex_now->p_sqlst_out_message_number = 0;	// out_msg_type

		send_packet(port, packet);

		// SEND could have changed the message

		message = statement->rsr_message;

		// Set up the response packet.  We may receive an SQL response followed
		// by a normal response packet or simply a response packet.

		if (in_msg_length || out_msg_length)
			port->port_statement->rsr_message->msg_address = out_msg;

		receive_packet(rdb->rdb_port, packet);

		if (packet->p_operation != op_sql_response)
			REMOTE_check_response(status, rdb, packet);
		else
		{
			message->msg_address = NULL;
			receive_response(status, rdb, packet);
		}

		if (transaction && !packet->p_resp.p_resp_object)
		{
			REMOTE_cleanup_transaction(transaction);
			release_transaction(transaction);
			transaction = NULL;
			rt->clear();
			return NULL;
		}
		else if (!transaction && packet->p_resp.p_resp_object)
		{
			transaction = make_transaction(rdb, packet->p_resp.p_resp_object);
			Firebird::ITransaction* newTrans = FB_NEW Transaction(transaction, this);
			newTrans->addRef();
			return newTrans;
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return apiTra;
}


void Statement::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	d s q l _ f r e e _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Release request for a Dynamic SQL statement
 *
 **************************************/

	try
	{
		// Check and validate handles, etc.

		if (!statement)
		{
			return;
		}

		CHECK_HANDLE(statement, isc_bad_req_handle);

		Rdb* rdb = statement->rsr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		fb_assert(statement->haveException() == 0);
		statement->clearException();

		if (statement->rsr_flags.test(Rsr::LAZY))
		{
			release_sql_request(statement);
			statement = NULL;

			return;
		}

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_free_statement;
		P_SQLFREE* free_stmt = &packet->p_sqlfree;
		free_stmt->p_sqlfree_statement = statement->rsr_id;
		free_stmt->p_sqlfree_option = DSQL_drop;

		if (rdb->rdb_port->port_flags & PORT_lazy)
		{
			send_packet(rdb->rdb_port, packet);
			defer_packet(rdb->rdb_port, packet, true);
			packet->p_resp.p_resp_object = statement->rsr_id;
		}
		else
		{
			try
			{
				send_and_receive(status, rdb, packet);
			}
			catch (const Exception&)
			{
				if (!force)
					throw;
			}
		}

		if (packet->p_resp.p_resp_object == INVALID_OBJECT)
		{
			release_sql_request(statement);
		}
		else
		{
			statement->rsr_flags.clear(Rsr::FETCHED);
			statement->rsr_rtr = NULL;

			clear_queue(rdb->rdb_port);
			REMOTE_reset_statement(statement);
		}
		statement = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Statement::internalFree(CheckStatusWrapper* status)
{
/**************************************
 *
 *	d s q l _ f r e e _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Release request for a Dynamic SQL statement
 *
 **************************************/

	reset(status);
	freeClientData(status);
}


void Statement::free(CheckStatusWrapper* status)
{
	internalFree(status);
	if (status->isEmpty())
		release();
}


void Statement::deprecatedFree(CheckStatusWrapper* status)
{
	internalFree(status);
}


Statement* Attachment::createStatement(CheckStatusWrapper* status, unsigned dialect)
{
	reset(status);

	Rsr* statement = NULL;
	if (rdb->rdb_port->port_flags & PORT_lazy)
	{
		statement = FB_NEW Rsr;
		statement->rsr_rdb = rdb;
		statement->rsr_id = INVALID_OBJECT;
		statement->rsr_flags.set(Rsr::LAZY);
	}
	else
	{
		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_allocate_statement;
		packet->p_rlse.p_rlse_object = rdb->rdb_id;

		send_and_receive(status, rdb, packet);

		// Allocate SQL request block

		statement = FB_NEW Rsr;
		statement->rsr_rdb = rdb;
		statement->rsr_id = packet->p_resp.p_resp_object;

		// register the object

		SET_OBJECT(rdb, statement, statement->rsr_id);
	}

	statement->rsr_next = rdb->rdb_sql_requests;
	rdb->rdb_sql_requests = statement;

	Statement* s = FB_NEW Statement(statement, this, dialect);
	s->addRef();
	return s;
}


Statement* Attachment::prepare(CheckStatusWrapper* status, ITransaction* apiTra,
	unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags)
{
/**************************************
 *
 *	d s q l _ p r e p a r e
 *
 **************************************
 *
 * Functional description
 *	Prepare a dynamic SQL statement for execution.
 *
 **************************************/

	Statement* stmt = NULL;

	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = NULL;
		if (apiTra)
		{
			transaction = remoteTransaction(apiTra);
			CHECK_HANDLE(transaction, isc_bad_trans_handle);
		}

		if (sqlStmt && !stmtLength)
			stmtLength = static_cast<ULONG>(strlen(sqlStmt));

		// Validate string length

		CHECK_LENGTH(port, stmtLength);

		if (dialect > 10)
		{
			// dimitr: adjust dialect received after
			//		   a multi-hop transmission to be
			//		   redirected in its original value.
			dialect /= 10;
		}

		// create new statement

		stmt = createStatement(status, dialect);
		Rsr* statement = stmt->getStatement();

		// reset current statement

		clear_queue(rdb->rdb_port);
		REMOTE_reset_statement(statement);

		// set up the packet for the other guy...

		PACKET* packet = &rdb->rdb_packet;

		if (statement->rsr_flags.test(Rsr::LAZY))
		{
			packet->p_operation = op_allocate_statement;
			packet->p_rlse.p_rlse_object = rdb->rdb_id;

			send_partial_packet(rdb->rdb_port, packet);
		}

		Array<UCHAR> items, buffer;
		buffer.resize(StatementMetadata::buildInfoItems(items, flags));

		// Validate data length

		CHECK_LENGTH(port, items.getCount());
		CHECK_LENGTH(port, buffer.getCount());

		packet->p_operation = op_prepare_statement;
		P_SQLST* prepare = &packet->p_sqlst;
		prepare->p_sqlst_transaction = transaction ? transaction->rtr_id : 0;
		prepare->p_sqlst_statement = statement->rsr_id;
		prepare->p_sqlst_SQL_dialect = dialect;
		prepare->p_sqlst_SQL_str.cstr_length = stmtLength;
		prepare->p_sqlst_SQL_str.cstr_address = reinterpret_cast<const UCHAR*>(sqlStmt);
		prepare->p_sqlst_items.cstr_length = (ULONG) items.getCount();
		prepare->p_sqlst_items.cstr_address = items.begin();
		prepare->p_sqlst_buffer_length = (ULONG) buffer.getCount();

		send_packet(rdb->rdb_port, packet);

		statement->rsr_flags.clear(Rsr::DEFER_EXECUTE);

		// Set up for the response packet.

		if (statement->rsr_flags.test(Rsr::LAZY))
		{
			receive_response(status, rdb, packet);

			statement->rsr_id = packet->p_resp.p_resp_object;
			SET_OBJECT(rdb, statement, statement->rsr_id);

			statement->rsr_flags.clear(Rsr::LAZY);
		}

		P_RESP* response = &packet->p_resp;
		SaveString temp(response->p_resp_data, buffer.getCount(), buffer.begin());

		try
		{
			receive_response(status, rdb, packet);
			stmt->parseMetadata(buffer);
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}

		if (rdb->rdb_port->port_flags & PORT_lazy)
		{
			if (response->p_resp_object & STMT_DEFER_EXECUTE) {
				statement->rsr_flags.set(Rsr::DEFER_EXECUTE);
			}
		}
		else
		{
			fb_assert(!response->p_resp_object);
			response->p_resp_object = 0;
		}

		if (!(status->getState() & Firebird::IStatus::STATE_ERRORS))
		{
			return stmt;
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	// free statement in case of error
	if (stmt)
	{
		stmt->release();
	}
	return NULL;
}


void Statement::getInfo(CheckStatusWrapper* status,
						unsigned int itemsLength, const unsigned char* items,
						unsigned int bufferLength, unsigned char* buffer)
{
/**************************************
 *
 *	d s q l _ s q l _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on sql object.
 *
 **************************************/
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		if (!metadata.fillFromCache(itemsLength, items, bufferLength, buffer))
		{
			info(status, rdb, op_info_sql, statement->rsr_id, 0,
				 itemsLength, items, 0, 0, bufferLength, buffer);

			metadata.parse(bufferLength, buffer);
		}

		statement->raiseException();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


unsigned Statement::getType(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		return metadata.getType();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return 0;
}


unsigned Statement::getFlags(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		if (port->port_protocol >= PROTOCOL_VERSION13)
		{
			// we are in luck - use flags from server
			return metadata.getFlags();
		}

		// Need to guess flags based on statement type
		unsigned value = IStatement::FLAG_REPEAT_EXECUTE;
		switch (metadata.getType())
		{
		case isc_info_sql_stmt_ddl:
			value &= ~IStatement::FLAG_REPEAT_EXECUTE;
			break;
		case isc_info_sql_stmt_select:
		case isc_info_sql_stmt_select_for_upd:
			value |= IStatement::FLAG_HAS_CURSOR;
			break;
		}

		return value;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return 0;
}


const char* Statement::getPlan(CheckStatusWrapper* status, FB_BOOLEAN detailed)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		return metadata.getPlan(detailed);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}


IMessageMetadata* Statement::getInputMetadata(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		return metadata.getInputMetadata();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}


IMessageMetadata* Statement::getOutputMetadata(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		return metadata.getOutputMetadata();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}


ISC_UINT64 Statement::getAffectedRecords(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		return metadata.getAffectedRecords();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return 0;
}


void Statement::setCursorName(CheckStatusWrapper* status, const char* cursor)
{
/*****************************************
 *
 *	d s q l _ s e t _ c u r s o r
 *
 *****************************************
 *
 * Functional Description
 *	Declare a cursor for a dynamic request.
 *
 *	Note:  prior to version 6.0, this function terminated the
 *	cursor name at the first blank.  With delimited cursor
 *	name support that is no longer sufficient.  We now pass
 *	the entire NULL-Terminated cursor name to the server, and let
 *	the server deal with blank termination or not.
 *	NOTE:  THIS NOW MEANS THAT IF CURSOR is NOT null terminated
 *	we will have inconsistant results with version 5.x.  The only
 *	"normal" way this happens is if this API is called from a
 *	non-C host language.   If that results in a later problem we
 *	must provide a new API that takes a "cursor_name_length"
 *	parameter.
 *
 *****************************************/

	try
	{
		reset(status);

		// Check and validate handles, etc.

		Rsr* statement = getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->raiseException();

		// set up the packet for the other guy...

		PACKET* packet = &rdb->rdb_packet;

		if (statement->rsr_flags.test(Rsr::LAZY))
		{
			packet->p_operation = op_allocate_statement;
			packet->p_rlse.p_rlse_object = rdb->rdb_id;

			send_partial_packet(rdb->rdb_port, packet);
		}

		packet->p_operation = op_set_cursor;
		P_SQLCUR* sqlcur = &packet->p_sqlcur;
		sqlcur->p_sqlcur_statement = statement->rsr_id;

		const ULONG name_l = static_cast<ULONG>(strlen(cursor));
		sqlcur->p_sqlcur_cursor_name.cstr_length = name_l + 1;
		sqlcur->p_sqlcur_cursor_name.cstr_address = reinterpret_cast<const UCHAR*>(cursor);
		sqlcur->p_sqlcur_type = 0;	// type

		send_packet(port, packet);

		if (statement->rsr_flags.test(Rsr::LAZY))
		{
			receive_response(status, rdb, packet);

			statement->rsr_id = packet->p_resp.p_resp_object;
			SET_OBJECT(rdb, statement, statement->rsr_id);

			statement->rsr_flags.clear(Rsr::LAZY);
		}

		receive_response(status, rdb, packet);

		statement->raiseException();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void ResultSet::setDelayedOutputFormat(CheckStatusWrapper* status, IMessageMetadata* format)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.
		if (!delayedFormat)
		{
			(Arg::Gds(isc_dsql_cursor_err) << Arg::Gds(isc_bad_req_handle)).raise();
		}

		outputFormat = format;
		delayedFormat = false;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


bool ResultSet::fetch(CheckStatusWrapper* status, void* buffer, P_FETCH operation, int position)
{
/**************************************
 *
 *	d s q l _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Fetch next record from a dynamic SQL cursor.
 *
 **************************************/

	reset(status);

	// Check and validate handles, etc.

	if (delayedFormat || !stmt)
	{
		(Arg::Gds(isc_dsql_cursor_err) << Arg::Gds(isc_bad_req_handle)).raise();
	}

	Rsr* const statement = stmt->getStatement();
	CHECK_HANDLE(statement, isc_bad_req_handle);

	Rdb* const rdb = statement->rsr_rdb;
	CHECK_HANDLE(rdb, isc_bad_db_handle);

	rem_port* const port = rdb->rdb_port;

	// Scrolling is not available in older protocols
	if (operation != fetch_next && port->port_protocol < PROTOCOL_FETCH_SCROLL)
		unsupported();

	// Whether we're fetching relatively to the current position
	const bool relative =
		(operation == fetch_next || operation == fetch_prior || operation == fetch_relative);

	BlrFromMessage outBlr(outputFormat, stmt->getDialect(), port->port_protocol);
	unsigned int blr_length = outBlr.getLength();
	const UCHAR* blr = outBlr.getBytes();
	const unsigned int msg_length = outBlr.getMsgLength();
	UCHAR* msg = static_cast<UCHAR*>(buffer);

	// Validate data length

	CHECK_LENGTH(port, blr_length);
	CHECK_LENGTH(port, msg_length);

	RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

	if (!statement->rsr_flags.test(Rsr::FETCHED))
	{
		// On first fetch, clear the end-of-stream flag & reset the message buffers

		statement->raiseException();

		statement->rsr_flags.clear(Rsr::STREAM_END | Rsr::PAST_END | Rsr::STREAM_ERR);
		statement->rsr_rows_pending = 0;
		statement->rsr_fetch_operation = operation;
		statement->rsr_fetch_position = position;
		statement->clearException();

		RMessage* message = statement->rsr_message;
		if (message)
		{
			statement->rsr_buffer = message;

			while (true)
			{
				message->msg_address = NULL;
				message = message->msg_next;

				if (message == statement->rsr_message)
					break;
			}
		}
	}
	else if (!relative)
	{
		// Clear the end-of-stream flag if the fetch is positioned absolutely
		statement->rsr_flags.clear(Rsr::STREAM_END | Rsr::PAST_END);
	}
	else if (statement->rsr_flags.test(Rsr::PAST_END))
	{
		// If we're already at BOF/EOF and the requested fetch operation
		// cannot change our position, just do nothing

		if (operation == fetch_relative && position == 0)
			return false;

		if ((operation == fetch_next || (operation == fetch_relative && position > 0)) &&
			statement->rsr_flags.test(Rsr::PAST_EOF))
		{
			return false;
		}

		if ((operation == fetch_prior || (operation == fetch_relative && position < 0)) &&
			statement->rsr_flags.test(Rsr::PAST_BOF))
		{
			return false;
		}
	}

	// Parse the blr describing the message, if there is any.

	if (blr_length)
	{
		if (statement->rsr_user_select_format &&
			statement->rsr_user_select_format != statement->rsr_select_format)
		{
			delete statement->rsr_user_select_format;
		}

		statement->rsr_user_select_format = PARSE_msg_format(blr, blr_length);

		if (statement->rsr_flags.test(Rsr::FETCHED))
			blr_length = 0;
		else
		{
			delete statement->rsr_select_format;
			statement->rsr_select_format = statement->rsr_user_select_format;
		}
	}

	if (!statement->rsr_buffer)
	{
		statement->rsr_buffer = FB_NEW RMessage(0);
		statement->rsr_message = statement->rsr_buffer;
		statement->rsr_message->msg_next = statement->rsr_message;
		statement->rsr_fmt_length = 0;
	}

	RMessage* message = statement->rsr_message;

#ifdef DEBUG
	fprintf(stdout, "Rows Pending in REM_fetch=%lu\n", statement->rsr_rows_pending);
#endif

	// If the fetch direction was changed, we don't need the batched rows anymore.
	// Swallow them and reset the stream for subsequent fetches.

	if (operation != statement->rsr_fetch_operation ||
		position != statement->rsr_fetch_position)
	{
		while (statement->rsr_rows_pending)
			receive_queued_packet(port, statement->rsr_id);

		if (statement->rsr_flags.test(Rsr::STREAM_ERR))
		{
			statement->rsr_flags.clear(Rsr::STREAM_ERR);

			// hvlad: prevent subsequent fetches
			statement->rsr_flags.set(Rsr::STREAM_END);
			statement->raiseException();
		}

		const SLONG adjustment = statement->getCursorAdjustment();
		statement->rsr_flags.clear(Rsr::STREAM_END | Rsr::PAST_END);

		// We have some messages in the queue. Reset them for reuse.

		if (statement->rsr_msgs_waiting)
		{
			fb_assert(statement->rsr_fetch_operation == fetch_next ||
					  statement->rsr_fetch_operation == fetch_prior);

			RMessage* message = statement->rsr_message;
			if (message)
			{
				statement->rsr_buffer = message;

				while (true)
				{
					message->msg_address = NULL;
					message = message->msg_next;

					if (message == statement->rsr_message)
						break;
				}
			}

			statement->rsr_msgs_waiting = 0;
		}

		// If we had some rows batched and the requested scrolling is relative,
		// then move the server cursor to the actual client's position before proceeding.
		// We don't know the absolute client's position, but it's not really necessary.
		// rsr_msgs_waiting shows how much we're ahead the server, so we may re-position
		// the cursor relatively.

		if (relative && adjustment)
		{
			const bool isAhead = (statement->rsr_fetch_operation == fetch_next);

			PACKET* packet = &rdb->rdb_packet;
			packet->p_operation = op_fetch_scroll;
			P_SQLDATA* sqldata = &packet->p_sqldata;
			sqldata->p_sqldata_statement = statement->rsr_id;
			sqldata->p_sqldata_blr.cstr_length = 0;
			sqldata->p_sqldata_blr.cstr_address = nullptr;
			sqldata->p_sqldata_message_number = 0;	// msg_type
			sqldata->p_sqldata_messages = statement->rsr_select_format ? 1 : 0;
			sqldata->p_sqldata_fetch_op = fetch_relative;
			sqldata->p_sqldata_fetch_pos = adjustment;

			send_packet(port, packet);

			// Receive response packets. If everything is OK, there should be two of them:
			// first with packet->p_sqldata.p_sqldata_messages == 1 and second with
			// packet->p_sqldata.p_sqldata_messages == 0 (end-of-batch).

			do
			{
				receive_packet(rdb->rdb_port, packet);

				// If we get an error, handle it
				if (packet->p_operation != op_fetch_response)
				{
					statement->rsr_flags.set(Rsr::STREAM_ERR);
					REMOTE_check_response(status, rdb, packet);
					break;
				}

				// If we get end-of-stream, something went seriously wrong, thus punt
				if (packet->p_sqldata.p_sqldata_status == 100)
					Arg::Gds(isc_req_sync).raise();

				// We should get either the requested row or the end-of-batch marker
				fb_assert(packet->p_sqldata.p_sqldata_messages == 0 ||
						  packet->p_sqldata.p_sqldata_messages == 1);

				// Release the received message, we don't need it
				const auto message = statement->rsr_message;
				if (message && message->msg_address)
				{
					statement->rsr_message = message->msg_next;
					message->msg_address = NULL;
				}
			}
			while (packet->p_sqldata.p_sqldata_messages);
		}

		// These are the necessary conditions to continue fetching (see below)
		fb_assert(!statement->rsr_flags.test(Rsr::STREAM_END | Rsr::STREAM_ERR));
		fb_assert(!statement->rsr_message->msg_address);
		fb_assert(!statement->rsr_rows_pending);
	}

	// Check to see if data is waiting.  If not, solicite data.

	if ((!statement->rsr_flags.test(Rsr::STREAM_END | Rsr::STREAM_ERR) &&
		!statement->rsr_message->msg_address && !statement->rsr_rows_pending) ||
		(	// Low in inventory
			(statement->rsr_rows_pending <= statement->rsr_reorder_level) &&
			(statement->rsr_msgs_waiting <= statement->rsr_reorder_level) &&
			// not using named pipe on NT
			// Pipelining causes both server & client to
			// write at the same time. In named pipes, writes
			// block for the other end to read -  and so when both
			// attempt to write simultaneously, they end up
			// waiting indefinitely for the other end to read
			(port->port_type != rem_port::PIPE) &&
			(port->port_type != rem_port::XNET) &&
			// We're fetching either forward or backward
			(operation == fetch_next || operation == fetch_prior) &&
			// We've reached end-of-stream or there was an error
			!statement->rsr_flags.test(Rsr::STREAM_END | Rsr::STREAM_ERR) &&
			// No error pending
			!statement->haveException() ))
	{
		// set up the packet for the other guy...

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = (operation == fetch_next) ? op_fetch : op_fetch_scroll;
		P_SQLDATA* sqldata = &packet->p_sqldata;
		sqldata->p_sqldata_statement = statement->rsr_id;
		sqldata->p_sqldata_blr.cstr_length = blr_length;
		sqldata->p_sqldata_blr.cstr_address = const_cast<unsigned char*>(blr);
		sqldata->p_sqldata_message_number = 0;	// msg_type
		sqldata->p_sqldata_messages = statement->rsr_select_format ? 1 : 0;
		sqldata->p_sqldata_fetch_op = operation;
		sqldata->p_sqldata_fetch_pos = position;

		if (statement->rsr_select_format)
		{
			if (operation == fetch_next || operation == fetch_prior)
			{
				sqldata->p_sqldata_messages = REMOTE_compute_batch_size(
					port, 0, op_fetch_response, statement->rsr_select_format);
			}

			// Reorder data when the local buffer is half empty

			statement->rsr_reorder_level = sqldata->p_sqldata_messages / 2;
#ifdef DEBUG
			fprintf(stdout, "Recalculating Rows Pending in REM_fetch=%lu\n",
					   statement->rsr_rows_pending);
#endif
		}

		statement->rsr_rows_pending += sqldata->p_sqldata_messages;

		// We've either got data, or some is on the way, or we have an error, or we have EOF

		if (!(statement->rsr_msgs_waiting ||
			statement->rsr_rows_pending ||
			statement->haveException() ||
			statement->rsr_flags.test(Rsr::STREAM_END)))
		{
			// We were asked to fetch from the statement, not ready for it.
			// Give up before sending something to the server.
			Arg::Gds(isc_req_sync).raise();
		}

		// Make the batch request - and force the packet over the wire

		send_packet(port, packet);

		statement->rsr_batch_count++;
		statement->rsr_fetch_operation = operation;
		statement->rsr_fetch_position = position;

		// Queue up receipt of the pending data

		enqueue_receive(port, batch_dsql_fetch, rdb, statement, NULL);

		fb_assert(statement->rsr_rows_pending || !statement->rsr_select_format);
	}

	// Receive queued responses until we have some data for this cursor
	// or an error status has been received.

	// We've either got data, or some is on the way, or we have an error, or we have EOF

	fb_assert(statement->rsr_msgs_waiting || statement->rsr_rows_pending ||
			  statement->haveException() || statement->rsr_flags.test(Rsr::STREAM_END));

	while (!statement->haveException() &&			// received a database error
		!statement->rsr_flags.test(Rsr::STREAM_END) &&	// reached end of stream
		statement->rsr_msgs_waiting < 2	&&			// Have looked ahead for end of batch
		statement->rsr_rows_pending)
	{
		// Hit end of batch
		receive_queued_packet(port, statement->rsr_id);
	}

	if (!statement->rsr_msgs_waiting)
	{
		if (statement->rsr_flags.test(Rsr::STREAM_END))
		{
			// hvlad: we may have queued fetch packet but received end-of-stream before start
			// handling of this packet. Handle it now.
			clear_stmt_que(port, statement);

			// hvlad: as we processed all queued packets at code above we can leave Rsr::EOF_SET flag.
			// It allows us to return EOF for all subsequent isc_dsql_fetch calls until statement
			// will be re-executed (and without roundtrip to remote server).
			//statement->rsr_flags.clear(Rsr::STREAM_END);

			if (statement->rsr_flags.test(Rsr::BOF_SET))
				statement->rsr_flags.set(Rsr::PAST_BOF);

			if (statement->rsr_flags.test(Rsr::EOF_SET))
				statement->rsr_flags.set(Rsr::PAST_EOF);

			return false;
		}

		if (statement->rsr_flags.test(Rsr::STREAM_ERR))
		{
			// The previous batch of receives ended with an error status.
			// We're all done returning data in the local queue.
			// Return that error status vector to the user.

			// Stuff in the error result to the user's vector

			statement->rsr_flags.clear(Rsr::STREAM_ERR);

			// hvlad: prevent subsequent fetches
			statement->rsr_flags.set(Rsr::STREAM_END);
			statement->raiseException();
		}
	}

	statement->rsr_msgs_waiting--;

	message = statement->rsr_message;
	statement->rsr_message = message->msg_next;

	if (statement->rsr_user_select_format->fmt_length != msg_length)
	{
		status_exception::raise(Arg::Gds(isc_port_len) <<
			Arg::Num(msg_length) << Arg::Num(statement->rsr_user_select_format->fmt_length));
	}

	if (statement->rsr_user_select_format == statement->rsr_select_format)
	{
		if (!msg || !message->msg_address)
		{
			move_error(Arg::Gds(isc_dsql_sqlda_err));
			// Msg 263 SQLDA missing or wrong number of variables
		}

		memcpy(msg, message->msg_address, msg_length);
	}
	else
	{
		mov_dsql_message(message->msg_address, statement->rsr_select_format, msg,
						 statement->rsr_user_select_format);
	}

	message->msg_address = NULL;
	return true;
}


int ResultSet::fetchNext(CheckStatusWrapper* user_status, void* buffer)
{
	try
	{
		return fetch(user_status, buffer, fetch_next) ?
			IStatus::RESULT_OK : IStatus::RESULT_NO_DATA;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(user_status);
	}

	return IStatus::RESULT_ERROR;
}


int ResultSet::fetchPrior(CheckStatusWrapper* user_status, void* buffer)
{
	try
	{
		if (!(flags & IStatement::CURSOR_TYPE_SCROLLABLE))
			(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("PRIOR")).raise();

		return fetch(user_status, buffer, fetch_prior) ?
			IStatus::RESULT_OK : IStatus::RESULT_NO_DATA;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(user_status);
	}

	return IStatus::RESULT_ERROR;
}


int ResultSet::fetchFirst(CheckStatusWrapper* user_status, void* buffer)
{
	try
	{
		if (!(flags & IStatement::CURSOR_TYPE_SCROLLABLE))
			(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("FIRST")).raise();

		return fetch(user_status, buffer, fetch_first) ?
			IStatus::RESULT_OK : IStatus::RESULT_NO_DATA;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(user_status);
	}

	return IStatus::RESULT_ERROR;
}


int ResultSet::fetchLast(CheckStatusWrapper* user_status, void* buffer)
{
	try
	{
		if (!(flags & IStatement::CURSOR_TYPE_SCROLLABLE))
			(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("LAST")).raise();

		return fetch(user_status, buffer, fetch_last) ?
			IStatus::RESULT_OK : IStatus::RESULT_NO_DATA;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(user_status);
	}

	return IStatus::RESULT_ERROR;
}


int ResultSet::fetchAbsolute(CheckStatusWrapper* user_status, int position, void* buffer)
{
	try
	{
		if (!(flags & IStatement::CURSOR_TYPE_SCROLLABLE))
			(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("ABSOLUTE")).raise();

		return fetch(user_status, buffer, fetch_absolute, position) ?
			IStatus::RESULT_OK : IStatus::RESULT_NO_DATA;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(user_status);
	}

	return IStatus::RESULT_ERROR;
}


int ResultSet::fetchRelative(CheckStatusWrapper* user_status, int offset, void* buffer)
{
	try
	{
		if (!(flags & IStatement::CURSOR_TYPE_SCROLLABLE))
			(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("RELATIVE")).raise();

		return fetch(user_status, buffer, fetch_relative, offset) ?
			IStatus::RESULT_OK : IStatus::RESULT_NO_DATA;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(user_status);
	}

	return IStatus::RESULT_ERROR;
}


FB_BOOLEAN ResultSet::isEof(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		if (!stmt)
		{
			Arg::Gds(isc_dsql_cursor_err).raise();
		}
		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);

		if (!statement->rsr_flags.test(Rsr::FETCHED))
			return FB_FALSE;

		return statement->rsr_flags.test(Rsr::PAST_EOF) ? FB_TRUE : FB_FALSE;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return FB_FALSE;
}


FB_BOOLEAN ResultSet::isBof(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		// Check and validate handles, etc.

		if (!stmt)
		{
			Arg::Gds(isc_dsql_cursor_err).raise();
		}
		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);

		if (!statement->rsr_flags.test(Rsr::FETCHED))
			return FB_TRUE;

		return statement->rsr_flags.test(Rsr::PAST_BOF) ? FB_TRUE : FB_FALSE;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return FB_FALSE;
}


IMessageMetadata* ResultSet::getMetadata(CheckStatusWrapper* status)
{
	if (!outputFormat)
	{
		status->setErrors(Arg::Gds(isc_no_output_format).value());
		return NULL;
	}

	reset(status);

	outputFormat->addRef();
	return outputFormat;
}

void ResultSet::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	d s q l _ f r e e _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Close SQL cursor
 *
 **************************************/

	try
	{
		// Check and validate handles, etc.

		if (!stmt)
		{
			Arg::Gds(isc_dsql_cursor_err).raise();
		}
		Rsr* statement = stmt->getStatement();
		CHECK_HANDLE(statement, isc_bad_req_handle);
		Rdb* rdb = statement->rsr_rdb;
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		statement->clearException();

		if (statement->rsr_flags.test(Rsr::LAZY))
		{
			statement->rsr_flags.clear(Rsr::FETCHED);
			statement->rsr_rtr = NULL;

			clear_queue(rdb->rdb_port);
			REMOTE_reset_statement(statement);

			releaseStatement();
			return;
		}

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_free_statement;
		P_SQLFREE* free_stmt = &packet->p_sqlfree;
		free_stmt->p_sqlfree_statement = statement->rsr_id;
		free_stmt->p_sqlfree_option = DSQL_close;

		if (rdb->rdb_port->port_flags & PORT_lazy)
		{
			defer_packet(rdb->rdb_port, packet);
			packet->p_resp.p_resp_object = statement->rsr_id;
			statement->clearException();
		}
		else
		{
			try
			{
				send_and_receive(status, rdb, packet);
			}
			catch (const Exception&)
			{
				if (!force)
					throw;
			}
		}

		statement->rsr_flags.clear(Rsr::FETCHED);
		statement->rsr_rtr = NULL;
		clear_queue(rdb->rdb_port);
		REMOTE_reset_statement(statement);
		releaseStatement();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void ResultSet::internalClose(CheckStatusWrapper* status)
{
/**************************************
 *
 *	d s q l _ f r e e _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Close SQL cursor
 *
 **************************************/

	reset(status);
	freeClientData(status);
}


void ResultSet::close(CheckStatusWrapper* status)
{
	internalClose(status);
	if (status->isEmpty())
		release();
}


void ResultSet::deprecatedClose(CheckStatusWrapper* status)
{
	internalClose(status);
}


void ResultSet::releaseStatement()
{
	if (tmpStatement)
	{
		stmt->release();
	}
	stmt = NULL;
}


int Blob::getSegment(CheckStatusWrapper* status, unsigned int bufferLength, void* buffer,
	unsigned int* segmentLength)
{
/**************************************
 *
 *	g d s _ g e t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Buffer segments of a blob and pass
 *	them one by one to the caller.
 *
 **************************************/

	try
	{
		reset(status);

		UCHAR* bufferPtr = static_cast<UCHAR*>(buffer);

		// Sniff out handles, etc, and find the various blocks.

		CHECK_HANDLE(blob, isc_bad_segstr_handle);

		Rdb* rdb = blob->rbl_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Build the primary packet to get the operation started.

		PACKET* packet = &rdb->rdb_packet;
		P_SGMT* segment = &packet->p_sgmt;
		P_RESP* response = &packet->p_resp;
		SaveString temp(response->p_resp_data, bufferLength, bufferPtr);

		// Handle a blob that has been created rather than opened (this should yield an error)

		if (blob->rbl_flags & Rbl::CREATE)
		{
			packet->p_operation = op_get_segment;
			segment->p_sgmt_length = bufferLength;
			segment->p_sgmt_blob = blob->rbl_id;
			segment->p_sgmt_segment.cstr_length = 0;

			send_packet(port, packet);
			receive_response(status, rdb, packet);

			if (segmentLength)
				*segmentLength = response->p_resp_data.cstr_length;
			return IStatus::RESULT_OK;
		}

		// New protocol -- ask for a 1K chunk of blob and
		// fill segment requests from it until its time to
		// get the next section.  In other words, get a bunch,
		// pass it out piece by piece, then when there isn't
		// enough left, ask for more.

		unsigned int length = 0;

		// if we're already done, stop now

		if (blob->rbl_flags & Rbl::EOF_SET)
		{
			if (segmentLength)
				*segmentLength = length;
			return IStatus::RESULT_NO_DATA;
		}

		// Here's the loop, passing out data from our basket & refilling it.
		//   Our buffer (described by the structure blob) is counted strings
		//   <count word> <string> <count word> <string>...

		int code = IStatus::RESULT_OK;
		while (true)
		{
			// If there's data to be given away, give some away (p points to the local data)

			if (blob->rbl_length)
			{
				UCHAR* p = blob->rbl_ptr;

				// If there was a fragment left over last time use it

				USHORT l = blob->rbl_fragment_length;
				if (l) {
					blob->rbl_fragment_length = 0;
				}
				else
				{
					// otherwise pick up the count word as the length, & decrement the local length
					l = *p++;
					l += *p++ << 8;
					blob->rbl_length -= 2;
				}

				// Now check that what we've got fits.
				// If not, set up the fragment pointer and set the status vector

				if (l > bufferLength)
				{
					blob->rbl_fragment_length = l - bufferLength;
					l = bufferLength;
					code = IStatus::RESULT_SEGMENT;
				}

				// and, just for yucks, see if we're exactly using up the fragment
				// part of a previous incomplete read - if so mark this as an
				// incomplete read

				if (l == bufferLength && l == blob->rbl_length && (blob->rbl_flags & Rbl::SEGMENT))
				{
					code = IStatus::RESULT_SEGMENT;
				}

				// finally set up the return length, decrement the current length,
				// copy the data, and indicate where to start next time.

				length += l;
				blob->rbl_length -= l;
				blob->rbl_offset += l;
				bufferLength -= l;

				if (l) {
					memcpy(bufferPtr, p, l);
				}

				bufferPtr += l;
				p += l;
				blob->rbl_ptr = p;

				// return if we've filled up the caller's buffer, or completed a segment

				if (!bufferLength || blob->rbl_length || !(blob->rbl_flags & Rbl::SEGMENT))
				{
					break;
				}
			}

			// We're done with buffer.  If this was the last, we're done

			if (blob->rbl_flags & Rbl::EOF_PENDING)
			{
				blob->rbl_flags |= Rbl::EOF_SET;
				code = IStatus::RESULT_NO_DATA;
				break;
			}

			// Preparatory to asking for more data, use input buffer length
			// to cue more efficient blob buffering.

			// Allocate 2 extra bytes to handle the special case where the
			// segment size of blob in the database is equal to the buffer
			// size that the user has passed.

			// Do not go into this loop if we already have a buffer
			// of size 65535 or 65534.

			if (bufferLength > blob->rbl_buffer_length - sizeof(USHORT) &&
				blob->rbl_buffer_length <= MAX_USHORT - sizeof(USHORT))
			{
				ULONG new_size = bufferLength + sizeof(USHORT);

				if (new_size > MAX_USHORT)	// Check if we've overflown
					new_size = bufferLength;
				blob->rbl_ptr = blob->rbl_buffer = blob->rbl_data.getBuffer(new_size);
				blob->rbl_buffer_length = (USHORT) new_size;
			}

			// We need more data.  Ask for it politely

			packet->p_operation = op_get_segment;
			segment->p_sgmt_length = blob->rbl_buffer_length;
			segment->p_sgmt_blob = blob->rbl_id;
			segment->p_sgmt_segment.cstr_length = 0;
			send_packet(rdb->rdb_port, packet);

			response->p_resp_data.cstr_allocated = blob->rbl_buffer_length;
			response->p_resp_data.cstr_address = blob->rbl_buffer;

			receive_response(status, rdb, packet);

			blob->rbl_length = (USHORT) response->p_resp_data.cstr_length;
			blob->rbl_ptr = blob->rbl_buffer;
			blob->rbl_flags &= ~Rbl::SEGMENT;
			if (response->p_resp_object == 1)
				blob->rbl_flags |= Rbl::SEGMENT;
			else if (response->p_resp_object == 2)
				blob->rbl_flags |= Rbl::EOF_PENDING;
		}

		if (segmentLength)
			*segmentLength = length;
		return code;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}


int Attachment::getSlice(CheckStatusWrapper* status, ITransaction* apiTra, ISC_QUAD* array_id,
						  unsigned int sdl_length, const unsigned char* sdl,
						  unsigned int param_length, const unsigned char* param,
						  int slice_length, unsigned char* slice)
{
/**************************************
 *
 *	g d s _ g e t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Snatch a slice of an array.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		// Validate data length

		CHECK_LENGTH(port, sdl_length);
		CHECK_LENGTH(port, param_length);

		// Parse the sdl in case blr_d_float must be converted to blr_double

		const UCHAR* new_sdl = sdl;

		// CVC: Modified this horrible idea: don't touch input parameters!
		// The modified (perhaps) sdl is send to the remote connection.  The
		// original sdl is used to process the slice data when it is received.
		// (This is why both 'new_sdl' and 'sdl' are saved in the packet.)
		HalfStaticArray<UCHAR, 128> sdl_buffer;
		UCHAR* old_sdl = sdl_buffer.getBuffer(sdl_length);
		memcpy(old_sdl, sdl, sdl_length);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_get_slice;
		P_SLC* data = &packet->p_slc;
		data->p_slc_transaction = transaction->rtr_id;
		data->p_slc_id = *array_id;
		data->p_slc_length = slice_length;
		data->p_slc_sdl.cstr_length = sdl_length;
		data->p_slc_sdl.cstr_address = const_cast<UCHAR*>(new_sdl);
		data->p_slc_parameters.cstr_length = param_length;
		data->p_slc_parameters.cstr_address = const_cast<UCHAR*>(param);

		data->p_slc_slice.lstr_length = 0;
		data->p_slc_slice.lstr_address = slice;

		P_SLR* response = &packet->p_slr;
		response->p_slr_sdl = old_sdl; //const_cast<UCHAR*>(sdl);
		response->p_slr_sdl_length = sdl_length;
		response->p_slr_slice.lstr_address = slice;
		response->p_slr_slice.lstr_length = slice_length;

		send_packet(rdb->rdb_port, packet);
		receive_packet(rdb->rdb_port, packet);

		if (packet->p_operation != op_slice)
		{
			REMOTE_check_response(status, rdb, packet);
		}

		return response->p_slr_length;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return 0;
}


IBlob* Attachment::openBlob(CheckStatusWrapper* status, ITransaction* apiTra, ISC_QUAD* id,
	unsigned int bpb_length, const unsigned char* bpb)
{
/**************************************
 *
 *	g d s _ o p e n _ b l o b 2
 *
 **************************************
 *
 * Functional description
 *	Open an existing blob.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		// Validate data length

		CHECK_LENGTH(port, bpb_length);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_open_blob2;
		P_BLOB* p_blob = &packet->p_blob;
		p_blob->p_blob_transaction = transaction->rtr_id;
		p_blob->p_blob_id = *id;
		p_blob->p_blob_bpb.cstr_length = bpb_length;
		fb_assert(!p_blob->p_blob_bpb.cstr_allocated ||
			p_blob->p_blob_bpb.cstr_allocated < p_blob->p_blob_bpb.cstr_length);
		// CVC: Should we ensure here that cstr_allocated < bpb_length???
		// Otherwise, xdr_cstring() calling alloc_string() to decode would
		// cause memory problems on the client side for SS, as the client
		// would try to write to the application's provided R/O buffer.
		p_blob->p_blob_bpb.cstr_address = bpb;

		send_and_receive(status, rdb, packet);

		// CVC: It's not evident to me why these two lines that I've copied
		// here as comments are only found in create_blob calls.
		// I think they should be enabled to avoid whatever buffer corruption.
		//p_blob->p_blob_bpb.cstr_length = 0;
		//p_blob->p_blob_bpb.cstr_address = NULL;

		Rbl* blob = FB_NEW Rbl;
		blob->rbl_rdb = rdb;
		blob->rbl_rtr = transaction;
		blob->rbl_id = packet->p_resp.p_resp_object;
		SET_OBJECT(rdb, blob, blob->rbl_id);
		blob->rbl_next = transaction->rtr_blobs;
		transaction->rtr_blobs = blob;

		Firebird::IBlob* b = FB_NEW Blob(blob);
		b->addRef();
		return b;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


void Transaction::prepare(CheckStatusWrapper* status, unsigned int msg_length, const unsigned char* msg)
{
/**************************************
 *
 *	g d s _ p r e p a r e
 *
 **************************************
 *
 * Functional description
 *	Prepare a transaction for commit.  First phase of a two
 *	phase commit.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = transaction->rtr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Validate data length

		CHECK_LENGTH(port, msg_length);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_prepare2;
		packet->p_prep.p_prep_transaction = transaction->rtr_id;
		packet->p_prep.p_prep_data.cstr_length = msg_length;
		packet->p_prep.p_prep_data.cstr_address = msg;

		send_packet(rdb->rdb_port, packet);
		receive_response(status, rdb, packet);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Blob::putSegment(CheckStatusWrapper* status, unsigned int segment_length, const void* segment)
{
/**************************************
 *
 *	g d s _ p u t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Emit a blob segment.  If the protocol allows,
 *	the segment is buffered locally for a later
 *	batch put.
 *
 **************************************/

	try
	{
		reset(status);

		const UCHAR* segmentPtr = static_cast<const UCHAR*>(segment);

		// Sniff out handles, etc, and find the various blocks.

		CHECK_HANDLE(blob, isc_bad_segstr_handle);

		Rdb* rdb = blob->rbl_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Handle a blob that has been opened rather than created (this should yield an error)

		if (!(blob->rbl_flags & Rbl::CREATE))
		{
			send_blob(status, blob, segment_length, segmentPtr);
			fb_assert(false);
		}

		// If the buffer can't hold the complete incoming segment, flush out the
		// buffer.  If the incoming segment is too large to fit into the blob
		// buffer, just send it as a single segment.

		UCHAR* p = blob->rbl_ptr;
		const unsigned int l = blob->rbl_buffer_length - (p - blob->rbl_buffer);

		if (segment_length + 2 > l)
		{
			if (blob->rbl_ptr > blob->rbl_buffer)
			{
				send_blob(status, blob, 0, NULL);
			}
			if ((ULONG) segment_length + 2 > blob->rbl_buffer_length)
			{
				send_blob(status, blob, segment_length, segmentPtr);
				return;
			}
			p = blob->rbl_buffer;
		}

		// Move segment length and data into blob buffer

		*p++ = (UCHAR) segment_length;
		*p++ = segment_length >> 8;

		if (segment_length) {
			memcpy(p, segmentPtr, segment_length);
		}

		blob->rbl_ptr = p + segment_length;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Attachment::putSlice(CheckStatusWrapper* status, ITransaction* apiTra, ISC_QUAD* id,
						   unsigned int sdl_length, const unsigned char* sdl,
						   unsigned int param_length, const unsigned char* param,
						   int sliceLength, unsigned char* slice)
{
/**************************************
 *
 *	g d s _ p u t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Store a slice of an array.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		// Validate data length

		CHECK_LENGTH(port, sdl_length);
		CHECK_LENGTH(port, param_length);

		// Parse the sdl in case blr_d_float must be converted to blr_double

		const UCHAR* new_sdl = sdl;

		// CVC: Modified this horrible idea: don't touch input parameters!
		// The modified (perhaps) sdl is sent to the remote connection.  The
		// original sdl is used to process the slice data before it is sent.
		// (This is why both 'new_sdl' and 'sdl' are saved in the packet.)
		HalfStaticArray<UCHAR, 128> sdl_buffer;
		UCHAR* old_sdl = sdl_buffer.getBuffer(sdl_length);
		memcpy(old_sdl, sdl, sdl_length);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_put_slice;
		P_SLC* data = &packet->p_slc;
		data->p_slc_transaction = transaction->rtr_id;
		data->p_slc_id = *id;
		data->p_slc_length = sliceLength;
		data->p_slc_sdl.cstr_length = sdl_length;
		data->p_slc_sdl.cstr_address = const_cast<UCHAR*>(new_sdl);
		data->p_slc_parameters.cstr_length = param_length;
		data->p_slc_parameters.cstr_address = const_cast<UCHAR*>(param);
		data->p_slc_slice.lstr_length = sliceLength;
		data->p_slc_slice.lstr_address = slice;

		P_SLR* response = &packet->p_slr;
		response->p_slr_sdl = old_sdl; //const_cast<UCHAR*>(sdl);
		response->p_slr_sdl_length = sdl_length;
		response->p_slr_slice.lstr_address = slice;
		response->p_slr_slice.lstr_length = sliceLength;

		send_and_receive(status, rdb, packet);

		*id = packet->p_resp.p_resp_blob_id;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


Firebird::IEvents* Attachment::queEvents(CheckStatusWrapper* status, Firebird::IEventCallback* callback,
									 unsigned int length, const unsigned char* events)
{
/**************************************
 *
 *	g d s _ $ q u e _ e v e n t s
 *
 **************************************
 *
 * Functional description
 *	Queue a request for event notification.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Validate data length

		CHECK_LENGTH(port, length);

		PACKET* packet = &rdb->rdb_packet;

		// If there isn't a auxiliary asynchronous port, make one now

		if (!port->port_async)
		{
			packet->p_operation = op_connect_request;
			P_REQ* request = &packet->p_req;
			request->p_req_object = rdb->rdb_id;
			request->p_req_type = P_REQ_async;
			send_packet(port, packet);
			receive_response(status, rdb, packet);
			port->connect(packet);

			rem_port* port_async = port->port_async;
			port_async->port_events_threadId =
				Thread::start(event_thread, port_async, THREAD_high, &port_async->port_events_thread);

			port_async->port_context = rdb;
		}

		// Add event block to port's list of active remote events

		Rvnt* rem_event = add_event(port);

		rem_event->rvnt_callback = callback;
		rem_event->rvnt_port = port->port_async;
		rem_event->rvnt_length = length;
		rem_event->rvnt_rdb = rdb;

		// Build the primary packet to get the operation started.

		packet = &rdb->rdb_packet;
		packet->p_operation = op_que_events;

		P_EVENT* event = &packet->p_event;
		event->p_event_database = rdb->rdb_id;
		event->p_event_items.cstr_length = length;
		event->p_event_items.cstr_address = events;
		event->p_event_ast = 0;
		event->p_event_arg = 0;
		event->p_event_rid = rem_event->rvnt_id;

		send_packet(port, packet);
		receive_response(status, rdb, packet);

		Firebird::IEvents* rc = FB_NEW Events(rem_event);
		rc->addRef();
		return rc;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


void Request::receive(CheckStatusWrapper* status, int level, unsigned int msg_type,
					  unsigned int msg_length, void* msg)
{
/**************************************
 *
 *	g d s _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Give a client program a record.  Ask the
 *	Remote server to send it to us if necessary.
 *
 **************************************/
	try
	{
		reset(status);

		// Check handles and environment, then set up error handling

		CHECK_HANDLE(rq, isc_bad_req_handle);
		Rrq* request = REMOTE_find_request(rq, level);

		Rdb* rdb = request->rrq_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rrq::rrq_repeat* tail = &request->rrq_rpt[msg_type];

		RMessage* message = tail->rrq_message;

#ifdef DEBUG
		fprintf(stdout, "Rows Pending in REM_receive=%d\n", tail->rrq_rows_pending);
#endif

		// Check to see if data is waiting.  If not, solicit data.
		// Solicit data either when we've run out, or there's a low
		// inventory of messages in local buffers & no shipments on the
		// ether being sent to us.

		if (request->rrqStatus.isSuccess() &&	// No error pending
			((!message->msg_address && tail->rrq_rows_pending == 0) ||	// No message waiting
				(tail->rrq_rows_pending <= tail->rrq_reorder_level &&	// Low in inventory
					tail->rrq_msgs_waiting <= tail->rrq_reorder_level &&
					// Pipelining causes both server & client to
					// write at the same time. In named pipes, writes
					// block for the other end to read -  and so when both
					// attempt to write simultaenously, they end up
					// waiting indefinetly for the other end to read
					(port->port_type != rem_port::PIPE) &&	// not named pipe on NT
					(port->port_type != rem_port::XNET) &&	// not shared memory on NT
					request->rrq_max_msg <= 1)))
		{
			// there's only one message type

#ifdef DEBUG
			fprintf(stderr, "Rows Pending %d\n", tail->rrq_rows_pending);
			if (!message->msg_address)
				fprintf(stderr, "Out of data - reordering\n");
			else
				fprintf(stderr, "Low on inventory - reordering\n");
#endif

			// Format a request for data

			PACKET *packet = &rdb->rdb_packet;
			packet->p_operation = op_receive;
			P_DATA* data = &packet->p_data;
			data->p_data_request = request->rrq_id;
			data->p_data_message_number = msg_type;
			data->p_data_incarnation = level;

			// Compute how many to send in a batch.  While this calculation
			// is the same for each batch (June 1996), perhaps in the future it
			// could dynamically adjust batching sizes based on fetch patterns

			data->p_data_messages = REMOTE_compute_batch_size(port, 0, op_send, tail->rrq_format);
			tail->rrq_reorder_level = data->p_data_messages / 2;
			tail->rrq_rows_pending += data->p_data_messages;

#ifdef DEBUG
			fprintf(stdout, "Recalculating Rows Pending in REM_receive=%d\n",
					   tail->rrq_rows_pending);
#endif

#ifdef DEBUG
			fprintf(stderr, "port_flags %d max_msg %d\n", port->port_flags, request->rrq_max_msg);
			fprintf(stderr, "Fetch: Req One batch of %d messages\n", data->p_data_messages);
#endif

			send_packet(port, packet);
			tail->rrq_batch_count++;

#ifdef DEBUG
			fprintf(stderr, "Rows Pending %d\n", tail->rrq_rows_pending);
#endif

			// Queue up receipt of the pending data

			enqueue_receive(port, batch_gds_receive, rdb, request, tail);
		}

		// Receive queued responses until we have some data for this cursor
		// or an error status has been received.

		// We've either got data, or some is on the way, or we have an error

		fb_assert(message->msg_address || tail->rrq_rows_pending > 0 || (!request->rrqStatus.isSuccess()));

		while (!message->msg_address && request->rrqStatus.isSuccess())
		{
			receive_queued_packet(port, request->rrq_id);
		}

		if (!message->msg_address && !request->rrqStatus.isSuccess())
		{
			// The previous batch of receives ended with an error status.
			// We're all done returning data in the local queue.
			// Return that error status vector to the user.

			// Stuff in the error result to the user's vector

			request->rrqStatus.raise();
		}

		// Copy data from the message buffer to the client buffer

		if (tail->rrq_format->fmt_length != msg_length)
		{
			status_exception::raise(Arg::Gds(isc_port_len) <<
				Arg::Num(msg_length) << Arg::Num(tail->rrq_format->fmt_length));
		}

		message = tail->rrq_message;
		memcpy(msg, message->msg_address, msg_length);

		// Move the head-of-full-buffer-queue pointer forward

		tail->rrq_message = message->msg_next;

		// Mark the buffer the message came from as available for reuse

		message->msg_address = NULL;

		tail->rrq_msgs_waiting--;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


Firebird::ITransaction* Attachment::reconnectTransaction(CheckStatusWrapper* status,
	unsigned int length, const unsigned char* id)
{
/**************************************
 *
 *	g d s _ r e c o n n e c t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Validate data length

		CHECK_LENGTH(port, length);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_reconnect;
		P_STTR* trans = &packet->p_sttr;
		trans->p_sttr_database = rdb->rdb_id;
		trans->p_sttr_tpb.cstr_length = length;
		trans->p_sttr_tpb.cstr_address = id;

		send_and_receive(status, rdb, packet);

		Firebird::ITransaction* t = FB_NEW Transaction(make_transaction(rdb, packet->p_resp.p_resp_object), this);
		t->addRef();
		return t;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


void Request::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	g d s _ r e l e a s e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Release a request.
 *
 **************************************/
	try
	{
		CHECK_HANDLE(rq, isc_bad_req_handle);

		Rdb* rdb = rq->rrq_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		try
		{
			release_object(status, rdb, op_release, rq->rrq_id);
		}
		catch (const Exception&)
		{
			if (!force)
				throw;
		}
		release_request(rq);
		rq = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Request::internalFree(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ r e l e a s e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Release a request.
 *
 **************************************/
	reset(status);
	freeClientData(status);
}


void Request::free(CheckStatusWrapper* status)
{
	internalFree(status);
	if (status->isEmpty())
		release();
}


void Request::deprecatedFree(CheckStatusWrapper* status)
{
	internalFree(status);
}


void Request::getInfo(CheckStatusWrapper* status, int level,
					  unsigned int itemsLength, const unsigned char* items,
					  unsigned int bufferLength, unsigned char* buffer)
{
/**************************************
 *
 *	g d s _ r e q u e s t _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on request object.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rq, isc_bad_req_handle);
		Rrq* request = REMOTE_find_request(rq, level);
		CHECK_HANDLE(request, isc_bad_req_handle);

		Rdb* rdb = request->rrq_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Check for buffered message.  If there is, report on it locally.
		const Rrq::rrq_repeat* tail= request->rrq_rpt.begin();
		for (const Rrq::rrq_repeat* const end = tail + request->rrq_max_msg; tail <= end; tail++)
		{
			RMessage* msg = tail->rrq_message;
			if (!msg || !msg->msg_address) {
				continue;
			}

			// We've got a pending message, respond locally

			const rem_fmt* format = tail->rrq_format;
			UCHAR* out = buffer;
			const UCHAR* infoItems = items;
			const UCHAR* const endItems = infoItems + itemsLength;

			while (infoItems < endItems)
			{
				USHORT data = 0;
				const UCHAR item = *infoItems++;
				switch (item)
				{
				case isc_info_end:
					break;

				case isc_info_state:
					data = isc_info_req_send;
					break;

				case isc_info_message_number:
					data = msg->msg_number;
					break;

				case isc_info_message_size:
					data = format->fmt_length;
					break;

				default:
					goto punt;
				}

				*out++ = item;
				if (item == isc_info_end)
					break;

				*out++ = 2;
				*out++ = 2 >> 8;
				*out++ = (UCHAR) data;
				*out++ = data >> 8;
			}
		}

		// No message pending, request status from other end

punt:

		info(status, rdb, op_info_request, request->rrq_id, level,
			 itemsLength, items, 0, 0, bufferLength, buffer);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Transaction::rollbackRetaining(CheckStatusWrapper* status)
{
/**************************************
 *
 *	i s c _ r o l l b a c k _ r e t a i n i n g
 *
 **************************************
 *
 * Functional description
 *	Abort a transaction but keep its environment valid
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = transaction->rtr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		release_object(status, rdb, op_rollback_retaining, transaction->rtr_id);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Transaction::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	g d s _ r o l l b a c k
 *
 **************************************
 *
 * Functional description
 *	Abort a transaction.
 *
 **************************************/
	try
	{
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = transaction->rtr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		try
		{
			release_object(status, rdb, op_rollback, transaction->rtr_id);
		}
		catch (const Exception&)
		{
			if (!force)
				throw;
		}

		REMOTE_cleanup_transaction(transaction);
		release_transaction(transaction);
		transaction = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Transaction::internalRollback(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ r o l l b a c k
 *
 **************************************
 *
 * Functional description
 *	Abort a transaction.
 *
 **************************************/
	reset(status);
	freeClientData(status);
}


void Transaction::rollback(CheckStatusWrapper* status)
{
	internalRollback(status);
	if (status->isEmpty())
		release();
}


void Transaction::deprecatedRollback(CheckStatusWrapper* status)
{
	internalRollback(status);
}


void Transaction::internalDisconnect(CheckStatusWrapper* status)
{
	try
	{
		reset(status);

		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = transaction->rtr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);

		// ASF: Looks wrong that this method is ignored in the engine and remote providers.
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Transaction::disconnect(CheckStatusWrapper* status)
{
	internalDisconnect(status);
	if (status->isEmpty())
		release();
}


void Transaction::deprecatedDisconnect(CheckStatusWrapper* status)
{
	internalDisconnect(status);
}


int Blob::seek(CheckStatusWrapper* status, int mode, int offset)
{
/**************************************
 *
 *	g d s _ s e e k _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Seek into a blob.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(blob, isc_bad_segstr_handle);

		Rdb* rdb = blob->rbl_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_seek_blob;
		P_SEEK* seek = &packet->p_seek;
		seek->p_seek_blob = blob->rbl_id;
		seek->p_seek_mode = mode;
		seek->p_seek_offset = offset;

		if (mode == 1)
		{
			seek->p_seek_mode = 0;
			seek->p_seek_offset = blob->rbl_offset + offset;
		}

		send_and_receive(status, rdb, packet);

		blob->rbl_offset = packet->p_resp.p_resp_blob_id.gds_quad_low;
		blob->rbl_length = 0;
		blob->rbl_fragment_length = 0;
		blob->rbl_flags &= ~(Rbl::EOF_SET | Rbl::EOF_PENDING | Rbl::SEGMENT);

		return blob->rbl_offset;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return 0;
}


void Request::send(CheckStatusWrapper* status, int level, unsigned int msg_type,
				   unsigned int /*length*/, const void* msg)
{
/**************************************
 *
 *	g d s _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Send a message to the server.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rq, isc_bad_req_handle);
		Rrq* request = REMOTE_find_request(rq, level);

		Rdb* rdb = request->rrq_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		if (msg_type > request->rrq_max_msg)
		{
			handle_error(isc_badmsgnum);
		}

		RMessage* message = request->rrq_rpt[msg_type].rrq_message;
		// We are lying here, but the interface shows for years this param as const
		message->msg_address = const_cast<unsigned char*>(static_cast<const unsigned char*>(msg));

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_send;
		P_DATA* data = &packet->p_data;
		data->p_data_request = request->rrq_id;
		data->p_data_message_number = msg_type;
		data->p_data_incarnation = level;

		send_packet(port, packet);

		// Bump up the message pointer to resync with rrq_xdr (rrq_xdr
		// was incremented by xdr_request in the SEND call).

		message->msg_address = NULL;
		request->rrq_rpt[msg_type].rrq_message = message->msg_next;

		receive_response(status, rdb, packet);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


Firebird::IService* RProvider::attachSvc(CheckStatusWrapper* status, const char* service,
	unsigned int spbLength, const unsigned char* spb, bool loopback)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ a t t a c h
 *
 **************************************
 *
 * Functional description
 *	Connect to a Firebird service.
 *
 **************************************/
	try
	{
		reset(status);

		PathName node_name, expanded_name(service);

		ClumpletWriter newSpb(ClumpletReader::spbList, MAX_DPB_SIZE, spb, spbLength);
		const bool user_verification = get_new_dpb(newSpb, spbParam);

		ClntAuthBlock cBlock(NULL, &newSpb, &spbParam);
		unsigned flags = 0;

		if (user_verification)
			flags |= ANALYZE_USER_VFY;

		if (loopback)
			flags |= ANALYZE_LOOPBACK;

		flags |= ANALYZE_EMP_NAME;

		PathName refDbName;
		if (newSpb.find(isc_spb_expected_db))
			newSpb.getPath(refDbName);

		rem_port* port = analyze(cBlock, expanded_name, flags, newSpb, spbParam, node_name, &refDbName, cryptCallback);

		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);
		Rdb* rdb = port->port_context;

		// The client may have set a parameter for dummy_packet_interval.  Add that to the
		// the SPB so the server can pay attention to it.  Note: allocation code must
		// ensure sufficient space has been added.

		add_other_params(port, newSpb, spbParam);

		IntlSpb intl;
		init(status, cBlock, port, op_service_attach, expanded_name, newSpb, intl, cryptCallback);

		Firebird::IService* s = FB_NEW Service(rdb);
		s->addRef();
		return s;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


Firebird::IService* RProvider::attachServiceManager(CheckStatusWrapper* status, const char* service,
	unsigned int spbLength, const unsigned char* spb)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ a t t a c h
 *
 **************************************
 *
 * Functional description
 *	Connect to a Firebird service.
 *
 **************************************/

	return attachSvc(status, service, spbLength, spb, false);
}


Firebird::IService* Loopback::attachServiceManager(CheckStatusWrapper* status, const char* service,
	unsigned int spbLength, const unsigned char* spb)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ a t t a c h
 *
 **************************************
 *
 * Functional description
 *	Connect to a Firebird service.
 *
 **************************************/

	return attachSvc(status, service, spbLength, spb, true);
}


void Service::freeClientData(CheckStatusWrapper* status, bool force)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Close down a connection to a Firebird service.
 *
 **************************************/
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(rdb, isc_bad_svc_handle);
		rem_port* port = rdb->rdb_port;
		RemotePortGuard portGuard(port, FB_FUNCTION);

		try
		{
			release_object(status, rdb, op_service_detach, rdb->rdb_id);
		}
		catch (const Exception&)
		{
			if (!force)
				throw;
		}
		disconnect(port);
		rdb = NULL;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Service::internalDetach(CheckStatusWrapper* status)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Close down a connection to a Firebird service.
 *
 **************************************/
	reset(status);
	freeClientData(status);
}


void Service::detach(CheckStatusWrapper* status)
{
	internalDetach(status);
	if (status->isEmpty())
		release();
}


void Service::deprecatedDetach(CheckStatusWrapper* status)
{
	internalDetach(status);
}


void Service::query(CheckStatusWrapper* status,
					unsigned int sendLength, const unsigned char* sendItems,
					unsigned int receiveLength, const unsigned char* receiveItems,
					unsigned int bufferLength, unsigned char* buffer)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ q u e r y
 *
 **************************************
 *
 * Functional description
 *	Provide information on service object.
 *
 **************************************/
	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(rdb, isc_bad_svc_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		info(status, rdb, op_service_info, rdb->rdb_id, 0,
			 sendLength, sendItems, receiveLength, receiveItems,
			 bufferLength, buffer);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Service::start(CheckStatusWrapper* status,
					unsigned int spbLength, const unsigned char* spb)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start a Firebird service
 *
 **************************************/

	try
	{
		reset(status);

		// Check and validate handles, etc.

		CHECK_HANDLE(rdb, isc_bad_svc_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		svcstart(status, rdb, op_service_start, rdb->rdb_id, 0, spbLength, spb);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Request::startAndSend(CheckStatusWrapper* status, Firebird::ITransaction* apiTra, int level,
						   unsigned int msg_type, unsigned int /*length*/, const void* msg)
{
/**************************************
 *
 *	g d s _ s t a r t _ a n d _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Get a record from the host program.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rq, isc_bad_req_handle);
		Rrq* request = REMOTE_find_request(rq, level);

		Rtr* transaction = remAtt->remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = request->rrq_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);

		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		if (msg_type > request->rrq_max_msg)
		{
			handle_error(isc_badmsgnum);
		}

		if (transaction->rtr_rdb != rdb)
		{
			Arg::Gds(isc_trareqmis).raise();
		}

		clear_queue(rdb->rdb_port);

		REMOTE_reset_request(request, 0);
		RMessage* message = request->rrq_rpt[msg_type].rrq_message;
		message->msg_address = const_cast<unsigned char*>(static_cast<const unsigned char*>(msg));

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_start_send_and_receive;
		P_DATA* data = &packet->p_data;
		data->p_data_request = request->rrq_id;
		data->p_data_transaction = transaction->rtr_id;
		data->p_data_message_number = msg_type;
		data->p_data_incarnation = level;

		send_packet(port, packet);

		// Bump up the message pointer to resync with rrq_xdr (rrq_xdr
		// was incremented by xdr_request in the SEND call).

		message->msg_address = NULL;
		request->rrq_rpt[msg_type].rrq_message = message->msg_next;

		receive_response(status, rdb, packet);

		// Save the request's transaction.

		request->rrq_rtr = transaction;

		if (packet->p_operation == op_response_piggyback)
		{
			receive_after_start(request, packet->p_resp.p_resp_object);
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Request::start(CheckStatusWrapper* status, Firebird::ITransaction* apiTra, int level)
{
/**************************************
 *
 *	g d s _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Get a record from the host program.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rq, isc_bad_req_handle);
		Rrq* request = REMOTE_find_request(rq, level);

		Rtr* transaction = remAtt->remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = request->rrq_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);

		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		if (transaction->rtr_rdb != rdb)
		{
			Arg::Gds(isc_trareqmis).raise();
		}

		clear_queue(rdb->rdb_port);

		REMOTE_reset_request(request, 0);
		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_start_and_receive;
		P_DATA* data = &packet->p_data;
		data->p_data_request = request->rrq_id;
		data->p_data_transaction = transaction->rtr_id;
		data->p_data_message_number = 0;
		data->p_data_incarnation = level;

		send_and_receive(status, rdb, packet);

		// Save the request's transaction.

		request->rrq_rtr = transaction;

		if (packet->p_operation == op_response_piggyback)
		{
			receive_after_start(request, packet->p_resp.p_resp_object);
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


Firebird::ITransaction* Attachment::startTransaction(CheckStatusWrapper* status, unsigned int tpbLength,
	const unsigned char* tpb)
{
/**************************************
 *
 *	g d s _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Start a transaction.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		if (/***tpbLength < 0 ||***/ (tpbLength > 0 && !tpb))
		{
			status_exception::raise(Arg::Gds(isc_bad_tpb_form));
		}

		// Validate data length

		CHECK_LENGTH(port, tpbLength);

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_transaction;
		P_STTR* trans = &packet->p_sttr;
		trans->p_sttr_database = rdb->rdb_id;
		trans->p_sttr_tpb.cstr_length = tpbLength;
		trans->p_sttr_tpb.cstr_address = tpb;

		send_and_receive(status, rdb, packet);

		Firebird::ITransaction* t = FB_NEW Transaction(make_transaction(rdb, packet->p_resp.p_resp_object), this);
		t->addRef();
		return t;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


void Attachment::transactRequest(CheckStatusWrapper* status, ITransaction* apiTra,
								  unsigned int blr_length, const unsigned char* blr,
								  unsigned int in_msg_length, const unsigned char* in_msg,
								  unsigned int out_msg_length, unsigned char* out_msg)
{
/**************************************
 *
 *	i s c _ t r a n s a c t _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Execute a procedure on remote host.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		Rtr* transaction = remoteTransaction(apiTra);
		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		// Validate data length

		CHECK_LENGTH(port, blr_length);
		CHECK_LENGTH(port, in_msg_length);
		CHECK_LENGTH(port, out_msg_length);

		Rpr* procedure = port->port_rpr;
		if (!procedure) {
			procedure = port->port_rpr = FB_NEW Rpr;
		}

		// Parse the blr describing the messages

		delete procedure->rpr_in_msg;
		procedure->rpr_in_msg = NULL;
		delete procedure->rpr_in_format;
		procedure->rpr_in_format = NULL;
		delete procedure->rpr_out_msg;
		procedure->rpr_out_msg = NULL;
		delete procedure->rpr_out_format;
		procedure->rpr_out_format = NULL;

		RMessage* message = PARSE_messages(blr, blr_length);
		while (message)
		{
			switch (message->msg_number)
			{
			case 0:
				procedure->rpr_in_msg = message;
				procedure->rpr_in_format = (rem_fmt*) message->msg_address;
				message->msg_address = const_cast<unsigned char*>(in_msg);
				message = message->msg_next;
				procedure->rpr_in_msg->msg_next = NULL;
				break;
			case 1:
				procedure->rpr_out_msg = message;
				procedure->rpr_out_format = (rem_fmt*) message->msg_address;
				message->msg_address = out_msg;
				message = message->msg_next;
				procedure->rpr_out_msg->msg_next = NULL;
				break;
			default:
				RMessage* temp = message;
				message = message->msg_next;
				delete temp;
				break;
			}
		}


		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_transact;
		P_TRRQ* trrq = &packet->p_trrq;
		trrq->p_trrq_database = rdb->rdb_id;
		trrq->p_trrq_transaction = transaction->rtr_id;
		trrq->p_trrq_blr.cstr_length = blr_length;
		trrq->p_trrq_blr.cstr_address = const_cast<unsigned char*>(blr);
		trrq->p_trrq_messages = in_msg_length ? 1 : 0;

		send_packet(port, packet);

		// Two types of responses are possible, op_transact_response or
		// op_response.  When there is an error op_response packet is returned
		// and it modifies the status vector to indicate the error which occurred.
		// But when success occurs a packet with op_transact_response comes back
		// which does not change the status vector.

		receive_packet(port, packet);

		if (packet->p_operation != op_transact_response)
		{
			REMOTE_check_response(status, rdb, packet);
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Transaction::getInfo(CheckStatusWrapper* status,
						  unsigned int itemsLength, const unsigned char* items,
						  unsigned int bufferLength, unsigned char* buffer)
{
/**************************************
 *
 *	g d s _ t r a n s a c t i o n _ i n f o
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	Array<unsigned char> newItemsBuffer;

	try
	{
		reset(status);

		CHECK_HANDLE(transaction, isc_bad_trans_handle);

		Rdb* rdb = transaction->rtr_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		fb_utils::getDbPathInfo(itemsLength, items, bufferLength, buffer,
			newItemsBuffer, remAtt->getDbPath());

		info(status, rdb, op_info_transaction, transaction->rtr_id, 0,
			 itemsLength, items, 0, 0, bufferLength, buffer);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Request::unwind(CheckStatusWrapper* status, int level)
{
/**************************************
 *
 *	g d s _ u n w i n d
 *
 **************************************
 *
 * Functional description
 *	Unwind a running request.
 *
 **************************************/
	try
	{
		reset(status);

		Rrq* request = REMOTE_find_request(rq, level);
		CHECK_HANDLE(request, isc_bad_req_handle);

		Rdb* rdb = request->rrq_rdb;
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


void Attachment::ping(CheckStatusWrapper* status)
{
/**************************************
 *
 *	p i n g
 *
 **************************************
 *
 * Functional description
 *	Check the attachment handle for persistent errors.
 *
 **************************************/
	try
	{
		reset(status);

		CHECK_HANDLE(rdb, isc_bad_db_handle);
		rem_port* port = rdb->rdb_port;
		RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);

		// Make sure protocol support action

		if (rdb->rdb_port->port_protocol < PROTOCOL_VERSION13)
			unsupported();

		PACKET* packet = &rdb->rdb_packet;
		packet->p_operation = op_ping;

		send_and_receive(status, rdb, packet);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

static Rvnt* add_event( rem_port* port)
{
/*************************************
 *
 * 	a d d _ e v e n t
 *
 **************************************
 *
 * Functional description
 *	Add remote event block to active chain.
 *
 **************************************/
	Rdb* rdb = port->port_context;

	// Find unused event block or, if necessary, a new one

	Rvnt* event;
	for (event = rdb->rdb_events; event; event = event->rvnt_next)
	{
		if (!event->rvnt_id)
			break;
	}

	if (!event)
	{
		event = FB_NEW Rvnt;
		event->rvnt_next = rdb->rdb_events;
		rdb->rdb_events = event;
	}

	event->rvnt_id = ++remote_event_id;

	return event;
}


static void add_other_params(rem_port* port, ClumpletWriter& dpb, const ParametersSet& par)
{
/**************************************
 *
 *	a d d _ o t h e r _ p a r a m s
 *
 **************************************
 *
 * Functional description
 *	Add parameters to a dpb to describe client-side
 *	settings that the server should know about.
 *
 **************************************/
	if (port->port_flags & PORT_dummy_pckt_set)
	{
		dpb.deleteWithTag(par.dummy_packet_interval);
		dpb.insertInt(par.dummy_packet_interval, port->port_dummy_packet_interval);
	}

	// Older version of engine not understand new tags and may process whole
	// DPB incorrectly. Check for protocol version is an poor attempt to make
	// guess about remote engine's version
	if (port->port_protocol >= PROTOCOL_VERSION11)
	{
		dpb.deleteWithTag(par.process_id);
		dpb.insertInt(par.process_id, getpid());

		if (!dpb.find(par.process_name))
		{
			PathName path(fb_utils::get_process_name());

			ISC_systemToUtf8(path);
			ISC_escape(path);

			if (!dpb.find(isc_dpb_utf8_filename))
				ISC_utf8ToSystem(path);

			dpb.insertString(par.process_name, path);
		}
	}

	if (port->port_protocol >= PROTOCOL_VERSION13)
	{
		dpb.deleteWithTag(par.client_version);
		dpb.insertString(par.client_version, FB_VERSION);
	}
}


static void add_working_directory(ClumpletWriter& dpb, const PathName& node_name)
{
/************************************************
 *
 *      a d d _ w o r k i n g _ d i r e c t o r y
 *
 ************************************************
 *
 * Functional description
 *      Add parameters to a dpb or spb to describe client-side
 *      settings that the server should know about.
 *
 ************************************************/
	if (dpb.find(isc_dpb_working_directory))
	{
		return;
	}

	PathName cwd;

	// for WNet local node_name should be compared with "\\\\." ?
	if (node_name == "localhost")
	{
		fb_utils::getCwd(cwd);

		ISC_systemToUtf8(cwd);
		ISC_escape(cwd);

		if (!dpb.find(isc_dpb_utf8_filename))
			ISC_utf8ToSystem(cwd);
	}

	dpb.insertString(isc_dpb_working_directory, cwd);
}


static void authenticateStep0(ClntAuthBlock& cBlock)
{
	LocalStatus ls;
	CheckStatusWrapper s(&ls);
	for (; cBlock.plugins.hasData(); cBlock.plugins.next())
	{
		HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authenticateStep0(%s)\n", cBlock.plugins.name()));
		switch(cBlock.plugins.plugin()->authenticate(&s, &cBlock))
		{
		case IAuth::AUTH_SUCCESS:
		case IAuth::AUTH_MORE_DATA:
			return;
		case IAuth::AUTH_FAILED:
			if (s.getState() & Firebird::IStatus::STATE_ERRORS)
			{
				iscLogStatus("Authentication, client plugin:", &s);
			}
			(Arg::Gds(isc_login_error)
#ifdef DEV_BUILD
								 << Arg::StatusVector(&s)
#endif
								 ).raise();
			break;	// compiler silencer
		}
	}
}


static void secureAuthentication(ClntAuthBlock& cBlock, rem_port* port)
{
	HANDSHAKE_DEBUG(fprintf(stderr, "Cli: secureAuthentication\n"));

	if (!port)
		return;

	Rdb* rdb = port->port_context;
	fb_assert(rdb);
	PACKET* packet = &rdb->rdb_packet;

	HANDSHAKE_DEBUG(fprintf(stderr, "Cli: secureAuthentication: port OK, op=%d\n", packet->p_operation));

	if (packet->p_operation == op_cond_accept)
	{
		LocalStatus ls;
		CheckStatusWrapper st(&ls);
		authReceiveResponse(true, cBlock, port, rdb, &st, packet, true);

		if (st.getState() & Firebird::IStatus::STATE_ERRORS)
			status_exception::raise(&st);
	}
	else
	{
		// try to start crypt
		cBlock.tryNewKeys(port);
	}
}


static rem_port* analyze(ClntAuthBlock& cBlock, PathName& attach_name, unsigned flags,
	ClumpletWriter& pb, const ParametersSet& parSet, PathName& node_name, PathName* ref_db_name,
	Firebird::ICryptKeyCallback* cryptCb)
{
/**************************************
 *
 *	a n a l y z e
 *
 **************************************
 *
 * Functional description
 *	Analyze an attach specification and determine whether
 *	a remote server is required, and if so, what protocol
 *	to use.  If the target can be accessed via the
 *	remote subsystem, return address of a port block
 *	with which to communicate with the server.
 *	Otherwise, return NULL.
 *
 *	NOTE: The file name must have been expanded prior to this call.
 *
 **************************************/

	rem_port* port = NULL;
	int inet_af = AF_UNSPEC;

	cBlock.loadClnt(pb, &parSet);
	authenticateStep0(cBlock);

	bool needFile = !(flags & ANALYZE_EMP_NAME);

#ifdef WIN_NT
	if (ISC_analyze_protocol(PROTOCOL_XNET, attach_name, node_name, NULL, needFile))
		port = XNET_analyze(&cBlock, attach_name, flags & ANALYZE_USER_VFY, cBlock.getConfig(), ref_db_name);
	else if (ISC_analyze_protocol(PROTOCOL_WNET, attach_name, node_name, WNET_SEPARATOR, needFile) ||
		ISC_analyze_pclan(attach_name, node_name))
	{
		if (node_name.isEmpty())
			node_name = WNET_LOCALHOST;
		else
		{
			ISC_unescape(node_name);
			ISC_utf8ToSystem(node_name);
		}

		port = WNET_analyze(&cBlock, attach_name, node_name.c_str(), flags & ANALYZE_USER_VFY,
			cBlock.getConfig(), ref_db_name);
	}
	else
#endif

	if (ISC_analyze_protocol(PROTOCOL_INET4, attach_name, node_name, INET_SEPARATOR, needFile))
		inet_af = AF_INET;
	else if (ISC_analyze_protocol(PROTOCOL_INET6, attach_name, node_name, INET_SEPARATOR, needFile))
		inet_af = AF_INET6;

	if (inet_af != AF_UNSPEC ||
		ISC_analyze_protocol(PROTOCOL_INET, attach_name, node_name, INET_SEPARATOR, needFile) ||
		ISC_analyze_tcp(attach_name, node_name, needFile))
	{
		if (node_name.isEmpty())
			node_name = INET_LOCALHOST;
		else
		{
			ISC_unescape(node_name);
			ISC_utf8ToSystem(node_name);
		}

		port = INET_analyze(&cBlock, attach_name, node_name.c_str(), flags & ANALYZE_USER_VFY, pb,
			cBlock.getConfig(), ref_db_name, cryptCb, inet_af);
	}

	// We have a local connection string. If it's a file on a network share,
	// try to connect to the corresponding host remotely.
	if (flags & ANALYZE_MOUNTS)
	{
#ifdef WIN_NT
		if (!port)
		{
			PathName expanded_name = attach_name;
			ISC_expand_share(expanded_name);

			if (ISC_analyze_pclan(expanded_name, node_name))
			{
				ISC_unescape(node_name);
				ISC_utf8ToSystem(node_name);

				port = WNET_analyze(&cBlock, expanded_name, node_name.c_str(), flags & ANALYZE_USER_VFY,
					cBlock.getConfig(), ref_db_name);
			}
		}
#endif

#ifndef NO_NFS
		if (!port)
		{
			PathName expanded_name = attach_name;
			if (ISC_analyze_nfs(expanded_name, node_name))
			{
				ISC_unescape(node_name);
				ISC_utf8ToSystem(node_name);

				port = INET_analyze(&cBlock, expanded_name, node_name.c_str(), flags & ANALYZE_USER_VFY, pb,
					cBlock.getConfig(), ref_db_name, cryptCb);
			}
		}
#endif
	}

	if ((flags & ANALYZE_LOOPBACK) && !port)
	{
		// We have a local connection string.
		// If we are in loopback mode attempt connect to a localhost.

		if (node_name.isEmpty())
		{
#ifdef WIN_NT
			if (!port)
			{
				port = XNET_analyze(&cBlock, attach_name, flags & ANALYZE_USER_VFY,
					cBlock.getConfig(), ref_db_name);
			}

			if (!port)
			{
				port = WNET_analyze(&cBlock, attach_name, WNET_LOCALHOST, flags & ANALYZE_USER_VFY,
					cBlock.getConfig(), ref_db_name);
			}
#endif
			if (!port)
			{
				port = INET_analyze(&cBlock, attach_name, INET_LOCALHOST, flags & ANALYZE_USER_VFY, pb,
					cBlock.getConfig(), ref_db_name, cryptCb);
			}
		}
	}

	if (!port)
		Arg::Gds(isc_unavailable).raise();

	try
	{
		secureAuthentication(cBlock, port);
	}
	catch (const Exception&)
	{
		disconnect(port);
		throw;
	}

	return port;
}


static void clear_stmt_que(rem_port* port, Rsr* statement)
{
/**************************************
 *
 *	c l e a r _ s t m t _ q u e
 *
 **************************************
 *
 * Functional description
 *
 * Receive and handle all queued packets for a completely fetched statement.
 * There must be no more than one such packet.
 *
 **************************************/
	fb_assert(statement->rsr_batch_count <= 1);

	while (statement->rsr_batch_count)
		receive_queued_packet(port, statement->rsr_id);

	// hvlad: clear isc_req_sync error as it is received because of our batch
	// fetching code, not because of wrong client application.
	// dimitr: modern engine versions do not pass isc_req_sync to the client,
	// but it's possible if we're connected to the older one.
	if (statement->haveException() == isc_req_sync)
		statement->clearException();
}

static void batch_dsql_fetch(rem_port*	port,
							 rmtque*	que_inst,
							 USHORT		id)
{
/**************************************
 *
 *	b a t c h _ d s q l _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Receive a batch of messages that were queued
 *	on the wire.
 *
 *	This function will be invoked whenever we need to wait
 *	for something to come over on the wire, and there are
 *	items in the queue for receipt.
 *
 *	Note on error handing:  Actual networking errors
 *	need to be reported to status - which is bubbled
 *	upwards to the API call which initiated this receive.
 *	A status vector being returned as part of the cursor
 *	fetch needs to be stored away for later return to the
 *	client in the proper place in the stream.
 *
 **************************************/

	fb_assert(port);
	fb_assert(que_inst);
	fb_assert(que_inst->rmtque_function == batch_dsql_fetch);

	Rdb* rdb = que_inst->rmtque_rdb;
	Rsr* statement = static_cast<Rsr*>(que_inst->rmtque_parm);
	PACKET* packet = &rdb->rdb_packet;

	fb_assert(port == rdb->rdb_port);

	// Setup the packet structures so it knows what statement we
	// are trying to receive at this point in time

	packet->p_sqldata.p_sqldata_statement = statement->rsr_id;

	// We'll either receive the whole batch, until end-of-batch is seen,
	// or we'll just fetch one.  We'll fetch one when we've run out of
	// local data to return to the client, so we grab one "hot off the wire"
	// to handoff to them.  We'll grab the whole batch when we need to
	// receive a response for a DIFFERENT network request on the wire,
	// so we have to clear the wire before the response can be received
	// In addition to the above we grab all the records in case of XNET as
	// we need to clear the queue.
	const bool clear_queue = (id != statement->rsr_id || port->port_type == rem_port::XNET);

	statement->rsr_flags.set(Rsr::FETCHED);
	while (true)
	{
		LocalStatus ls;
		CheckStatusWrapper status(&ls);

		// Swallow up data. If a buffer isn't available, allocate another.

		RMessage* message = statement->rsr_buffer;
		if (message->msg_address)
		{
			RMessage* new_msg = FB_NEW RMessage(statement->rsr_fmt_length);
			statement->rsr_buffer = new_msg;

			new_msg->msg_next = message;

			while (message->msg_next != new_msg->msg_next)
				message = message->msg_next;

			message->msg_next = new_msg;
		}

		try {
			receive_packet_noqueue(port, packet);
		}
		catch (const Exception&)
		{
			// Must be a network error
			statement->rsr_rows_pending = 0;
			--statement->rsr_batch_count;
			dequeue_receive(port);

			throw;
		}

		if (packet->p_operation != op_fetch_response)
		{
			statement->rsr_flags.set(Rsr::STREAM_ERR);

			try
			{
				REMOTE_check_response(&status, rdb, packet);
				statement->saveException(&status, false);
			}
			catch (const Exception& ex)
			{
				// Queue errors within the batched request
				statement->saveException(ex, false);
			}

			statement->rsr_rows_pending = 0;
			--statement->rsr_batch_count;
			dequeue_receive(port);

			break;
		}

		// See if we're at end of the batch

		if (packet->p_sqldata.p_sqldata_status || !packet->p_sqldata.p_sqldata_messages)
		{
			if (packet->p_sqldata.p_sqldata_status == 100)
			{
				const auto operation = statement->rsr_fetch_operation;
				const auto position = statement->rsr_fetch_position;

				const bool forward =
					(operation == fetch_next || operation == fetch_last ||
					((operation == fetch_absolute || operation == fetch_relative) && position > 0));

				if (forward)
					statement->rsr_flags.set(Rsr::EOF_SET);
				else
					statement->rsr_flags.set(Rsr::BOF_SET);

				statement->rsr_rows_pending = 0;
#ifdef DEBUG
				fprintf(stdout, "Resetting Rows Pending in batch_dsql_fetch=%lu\n",
						   statement->rsr_rows_pending);
#endif
			}

			if (--statement->rsr_batch_count == 0)
				statement->rsr_rows_pending = 0;

			dequeue_receive(port);

			// clear next queued batch(es) if present
			if (packet->p_sqldata.p_sqldata_status == 100)
			{
				try
				{
					clear_stmt_que(port, statement);
				}
				catch (const Exception&) { }
			}
			break;
		}

		statement->rsr_msgs_waiting++;
		statement->rsr_rows_pending--;

#ifdef DEBUG
		fprintf(stdout, "Decrementing Rows Pending in batch_dsql_fetch=%lu\n",
				   statement->rsr_rows_pending);
#endif

		if (!clear_queue)
			break;
	}
}


static void batch_gds_receive(rem_port*		port,
							  rmtque*	que_inst,
							  USHORT		id)
{
/**************************************
 *
 *	b a t c h _ g d s _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Receive a batch of messages that were queued
 *	on the wire.
 *
 *	This function will be invoked whenever we need to wait
 *	for something to come over on the wire, and there are
 *	items in the queue for receipt.
 *
 *	Note on error handing:  Actual networking errors
 *	need to be reported to status - which is bubbled
 *	upwards to the API call which initiated this receive.
 *	A status vector being returned as part of the cursor
 *	fetch needs to be stored away for later return to the
 *	client in the proper place in the stream.
 *
 **************************************/

	fb_assert(port);
	fb_assert(que_inst);
	fb_assert(que_inst->rmtque_function == batch_gds_receive);

	Rdb* rdb = que_inst->rmtque_rdb;
	Rrq* request = static_cast<Rrq*>(que_inst->rmtque_parm);
	Rrq::rrq_repeat* tail = que_inst->rmtque_message;
	PACKET *packet = &rdb->rdb_packet;

	fb_assert(port == rdb->rdb_port);

	bool clear_queue = false;
	// indicates whether queue is just being emptied, not retrieved

	// always clear the complete queue for XNET, as we might
	// have incomplete packets
	if (id != request->rrq_id || port->port_type == rem_port::XNET)
	{
		clear_queue = true;
	}

	// Receive the whole batch of records, until end-of-batch is seen

	while (true)
	{
		RMessage* message = tail->rrq_xdr;	// First free buffer

		// If the buffer queue is full, allocate a new message and
		// place it in the queue--if we are clearing the queue, don't
		// read records into messages linked list so that we don't
		// mess up the record cache for scrolling purposes.

		if (message->msg_address)
		{
			const rem_fmt* format = tail->rrq_format;
			RMessage* new_msg = FB_NEW RMessage(format->fmt_length);
			tail->rrq_xdr = new_msg;
			new_msg->msg_next = message;
			new_msg->msg_number = message->msg_number;

			// Walk the que until we find the predecessor of message

			while (message->msg_next != new_msg->msg_next)
			{
				message = message->msg_next;
			}
			message->msg_next = new_msg;
		}

		// Note: not receive_packet
		try
		{
			receive_packet_noqueue(rdb->rdb_port, packet);
		}
		catch (const Exception&)
		{
			// Must be a network error
			tail->rrq_rows_pending = 0;
			--tail->rrq_batch_count;
			dequeue_receive(port);
			throw;
		}

		if (packet->p_operation != op_send)
		{
			tail->rrq_rows_pending = 0;
			--tail->rrq_batch_count;
			try
			{
				LocalStatus ls;
				CheckStatusWrapper status(&ls);
				REMOTE_check_response(&status, rdb, packet);
#ifdef DEBUG
				fprintf(stderr, "End of batch. rows pending = %d\n", tail->rrq_rows_pending);
#endif
				request->saveStatus(&status);
			}
			catch (const Exception& ex)
			{
#ifdef DEBUG
				fprintf(stderr, "Got batch error %ld Max message = %d\n",
						ex->value()[1], request->rrq_max_msg);
#endif
				// Queue errors within the batched request
				request->saveStatus(ex);
			}
			dequeue_receive(port);
			break;
		}

		tail->rrq_msgs_waiting++;
		tail->rrq_rows_pending--;
#ifdef DEBUG
		fprintf(stdout, "Decrementing Rows Pending in batch_gds_receive=%d\n",
				   tail->rrq_rows_pending);
#endif

		// See if we're at end of the batch

		if (!packet->p_data.p_data_messages)
		{
			if (!(--tail->rrq_batch_count))
				tail->rrq_rows_pending = 0;
#ifdef DEBUG
			fprintf(stderr, "End of batch waiting %d\n", tail->rrq_rows_pending);
#endif
			dequeue_receive(port);
			break;
		}

		// one packet is enough unless we are trying to clear the queue

		if (!clear_queue)
			break;
	}
}


static void clear_queue(rem_port* port)
{
/**************************************
 *
 *	c l e a r _ q u e u e
 *
 **************************************
 *
 * Functional description
 *	Clear the queue of batched packets - in preparation
 *	for waiting for a specific response, or when we are
 *	about to reuse an internal request.
 * Return codes:
 *	true  - no errors.
 *	false - Network error occurred, error code in status
 **************************************/

	while (port->port_receive_rmtque)
	{
		receive_queued_packet(port, (USHORT) -1);
	}
}


static void disconnect( rem_port* port)
{
/**************************************
 *
 *	d i s c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Disconnect a port and free its memory.
 *
 **************************************/

	// Send a disconnect to the server so that it
	// gracefully terminates.

	Rdb* rdb = port->port_context;
	if (rdb)
	{
		PACKET* packet = &rdb->rdb_packet;

		// Deliver the pending deferred packets
		if (port->port_deferred_packets)
		{
			for (rem_que_packet* p = port->port_deferred_packets->begin();
				 p < port->port_deferred_packets->end();
				 p++)
			{
				if (!p->sent)
					port->send(&p->packet);
			}
		}

		// BAND-AID:
		// It seems as if we are disconnecting the port
		// on both the server and client side.  For now
		// let the server handle this for named pipes

		// 8-Aug-1997  M.  Duquette
		// R.  Kumar
		// M.  Romanini

		if (port->port_type != rem_port::PIPE)
		{
			packet->p_operation = op_disconnect;
			port->send(packet);
		}
		REMOTE_free_packet(port, packet);
	}

	// Cleanup the queue

	delete port->port_deferred_packets;

	// Clear context reference for the associated event handler
	// to avoid SEGV during shutdown

	if (port->port_async)
	{
		port->port_async->port_context = NULL;
		port->port_async->port_flags |= PORT_disconnect;
	}

	// Perform physical network disconnect and release
	// memory for remote database context.

	port->port_flags |= PORT_disconnect;
	port->disconnect();
	delete rdb;
}


static THREAD_ENTRY_DECLARE event_thread(THREAD_ENTRY_PARAM arg)
{
/**************************************
 *
 *	e v e n t _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *	Wait on auxilary mailbox for event notification.
 *
 **************************************/
	rem_port* port = (rem_port*)arg;
//	Reference portRef(*port);
	PACKET packet;

	while (!(port->port_flags & PORT_disconnect))
	{
		// zero packet

		zap_packet(&packet);

		// read what should be an event message

		rem_port* stuff = NULL;
		P_OP operation = op_void;
		{	// scope
			RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);
			try
			{
				stuff = port->receive(&packet);
			}
			catch(status_exception&)
			{
				// ignore
			}

			operation = packet.p_operation;

			if (!stuff || operation == op_exit || operation == op_disconnect)
			{
				// Actually, the remote server doing the watching died.
				// Clean up and leave.

				REMOTE_free_packet(port, &packet);
				server_death(port);
				break;
			}
		} // end scope

		// If the packet was an event, we handle it

		if (operation == op_event)
		{
			P_EVENT* pevent = &packet.p_event;

			Rvnt* event = NULL;
			{	// scope
				RefMutexGuard portGuard(*port->port_sync, FB_FUNCTION);
				event = find_event(port, pevent->p_event_rid);
			}

			if (event)
			{
				// Call the asynchronous event routine associated
				// with this event
				const ULONG length = pevent->p_event_items.cstr_length;
				if (length <= event->rvnt_length)
				{
					event->rvnt_callback->eventCallbackFunction(length, pevent->p_event_items.cstr_address);
				}
				//else {....
				//This is error condition, but we have absolutely no ways to report it.
				//Therefore simply ignore such bad packet.

				// Finished processing this event
				// Callback above should release event and another thread could reuse it meanwhile.
				// Make sure we don't release such reused event.
				if (event->rvnt_id == pevent->p_event_rid)
					event->rvnt_id = 0;
			}

		}						// end of event handling for op_event

		REMOTE_free_packet(port, &packet);
	}							// end of infinite for loop
	// to make compilers happy
	return 0;
}


static Rvnt* find_event( rem_port* port, SLONG id)
{
/*************************************
 *
 * 	f i n d _ e v e n t
 *
 **************************************
 *
 * Functional description
 *	Find event with specified event_id.
 *
 **************************************/
	Rdb* rdb = port->port_context;

	if (rdb && !(port->port_flags & PORT_disconnect))
	{
		for (Rvnt* event = rdb->rdb_events; event; event = event->rvnt_next)
		{
			if (event->rvnt_id == id)
				return event;
		}
	}

	return NULL;
}


static bool get_new_dpb(ClumpletWriter& dpb, const ParametersSet& par)
{
/**************************************
 *
 *	g e t _ n e w _ d p b
 *
 **************************************
 *
 * Functional description
 *	Fetch user_string out of dpb.
 *	Analyze and prepare dpb for attachment to remote server.
 *
 **************************************/
	bool redirection = Config::getRedirection();
    if (((!redirection) && dpb.find(par.address_path)) || dpb.find(par.map_attach))
	{
		status_exception::raise(Arg::Gds(isc_unavailable));
	}

	return dpb.find(par.user_name);
}


static void info(CheckStatusWrapper* status,
				 Rdb* rdb,
				 P_OP operation,
				 USHORT object,
				 USHORT incarnation,
				 USHORT item_length,
				 const UCHAR* items,
				 USHORT recv_item_length,
				 const UCHAR* recv_items,
				 ULONG buffer_length,
				 UCHAR* buffer)
{
/**************************************
 *
 *	i n f o
 *
 **************************************
 *
 * Functional description
 *	Solicit and receive information.
 *
 **************************************/

	// Build the primary packet to get the operation started.

	PACKET* packet = &rdb->rdb_packet;
	packet->p_operation = operation;
	P_INFO* information = &packet->p_info;
	information->p_info_object = object;
	information->p_info_incarnation = incarnation;
	information->p_info_items.cstr_length = item_length;
	information->p_info_items.cstr_address = items;
	if (operation == op_service_info)
	{
		information->p_info_recv_items.cstr_length = recv_item_length;
		information->p_info_recv_items.cstr_address = recv_items;
	}
	information->p_info_buffer_length = buffer_length;

	send_packet(rdb->rdb_port, packet);

	// Set up for the response packet.

	P_RESP* response = &packet->p_resp;
	SaveString temp(response->p_resp_data, buffer_length, buffer);

	receive_response(status, rdb, packet);
}

static bool useLegacyAuth(const char* nm, int protocol, ClumpletWriter& dpb)
{
	LegacyPlugin legacyAuth = REMOTE_legacy_auth(nm, protocol);
	if (!legacyAuth)
		return false;

	int requestedAuth = dpb.find(isc_dpb_user_name) ? PLUGIN_LEGACY : PLUGIN_TRUSTED;
	return legacyAuth == requestedAuth;
}

// Let plugins try to add data to DPB in order to avoid extra network roundtrip
static void authFillParametersBlock(ClntAuthBlock& cBlock, ClumpletWriter& dpb,
	const ParametersSet* tags, rem_port* port)
{
	if (cBlock.authComplete)
		return;		// Already authenticated

	LocalStatus ls;
	CheckStatusWrapper s(&ls);

	cBlock.resetDataFromPlugin();

	for (; cBlock.plugins.hasData(); cBlock.plugins.next())
	{
		if (port->port_protocol >= PROTOCOL_VERSION13 ||
			useLegacyAuth(cBlock.plugins.name(), port->port_protocol, dpb))
		{
			// OK to use plugin
			cBlock.resetDataFromPlugin();
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authFillParametersBlock(%s)\n", cBlock.plugins.name()));
			int authRc = cBlock.plugins.plugin()->authenticate(&s, &cBlock);

			switch (authRc)
			{
			case IAuth::AUTH_SUCCESS:
			case IAuth::AUTH_MORE_DATA:
				HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authFillParametersBlock: plugin %s is OK\n",
					cBlock.plugins.name()));
				cleanDpb(dpb, tags);
				cBlock.extractDataFromPluginTo(dpb, tags, port->port_protocol);
				return;

			case IAuth::AUTH_CONTINUE:
				continue;

			case IAuth::AUTH_FAILED:
				HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authFillParametersBlock: plugin %s FAILED\n",
					cBlock.plugins.name()));
				(Arg::Gds(isc_login) << Arg::StatusVector(&s)).raise();
				break;	// compiler silencer
			}
		}

		HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authFillParametersBlock: try next plugin, %s skipped\n",
			cBlock.plugins.name()));
	}
}

#ifdef NOT_USED_OR_REPLACED
static CSTRING* REMOTE_dup_string(const CSTRING* from)
{
	if (from && from->cstr_length)
	{
		CSTRING* rc = FB_NEW_POOL(*getDefaultMemoryPool()) CSTRING;
		memset(rc, 0, sizeof(CSTRING));
		rc->cstr_length = from->cstr_length;
		rc->cstr_allocated = rc->cstr_length;
		rc->cstr_address = FB_NEW_POOL(*getDefaultMemoryPool()) UCHAR[rc->cstr_length];
		memcpy(rc->cstr_address, from->cstr_address, rc->cstr_length);
		return rc;
	}

	return NULL;
}

static void REMOTE_free_string(CSTRING* tmp)
{
	if (tmp)
	{
		if (tmp->cstr_address)
		{
			fb_assert(tmp->cstr_allocated >= tmp->cstr_length);
			delete[] tmp->cstr_address;
		}
		delete tmp;
	}
}
#endif // NOT_USED_OR_REPLACED

static void authReceiveResponse(bool havePacket, ClntAuthBlock& cBlock, rem_port* port,
	Rdb* rdb, IStatus* status, PACKET* packet, bool checkKeys)
{
	LocalStatus ls;
	CheckStatusWrapper s(&ls);

	for (;;)
	{
		// Get response
		if (!havePacket)
			receive_packet(port, packet);
		else
			fb_assert(packet->p_operation == op_cond_accept);

		havePacket = false;		// havePacket means first packet is already received

		// Check response
		cstring* n = NULL;
		cstring* d = NULL;

		switch(packet->p_operation)
		{
		case op_trusted_auth:
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authReceiveResponse: trusted_auth\n"));
			d = &packet->p_trau.p_trau_data;
			break;

		case op_cont_auth:
			d = &packet->p_auth_cont.p_data;
			n = &packet->p_auth_cont.p_name;
			port->addServerKeys(&packet->p_auth_cont.p_keys);
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authReceiveResponse: cont_auth d=%d n=%d '%.*s' 0x%x\n",
				d->cstr_length, n->cstr_length,
				n->cstr_length, n->cstr_address, n->cstr_address ? n->cstr_address[0] : 0));
			break;

		case op_cond_accept:
			d = &packet->p_acpd.p_acpt_data;
			n = &packet->p_acpd.p_acpt_plugin;
			port->addServerKeys(&packet->p_acpd.p_acpt_keys);
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authReceiveResponse: cond_accept d=%d n=%d '%.*s' 0x%x\n",
				d->cstr_length, n->cstr_length,
				n->cstr_length, n->cstr_address, n->cstr_address ? n->cstr_address[0] : 0));
			if (packet->p_acpd.p_acpt_type & pflag_compress)
			{
				port->initCompression();
				port->port_flags |= PORT_compressed;
			}
			packet->p_acpd.p_acpt_type &= ptype_MASK;
			break;

		default:
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authReceiveResponse: Default answer\n"));
			REMOTE_check_response(status, rdb, packet, checkKeys);
			// successfully attached
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: authReceiveResponse: OK!\n"));
			cBlock.authComplete = true;
			rdb->rdb_id = packet->p_resp.p_resp_object;

			// try to start crypt
			cBlock.tryNewKeys(port);
			return;
		}

		if (n && n->cstr_length && cBlock.plugins.hasData())
		{
			// if names match, do not change instance
			if (strlen(cBlock.plugins.name()) == n->cstr_length &&
				memcmp(cBlock.plugins.name(), n->cstr_address, n->cstr_length) == 0)
			{
				n = NULL;
			}
		}

		if (n && n->cstr_length)
		{
			// switch to other plugin
			PathName tmp(n->cstr_address, n->cstr_length);
			if (!cBlock.checkPluginName(tmp))
			{
				break;
			}
			cBlock.plugins.set(tmp.c_str());
		}

		if (!cBlock.plugins.hasData())
		{
			break;
		}

		cBlock.resetDataFromPlugin();
		cBlock.storeDataForPlugin(d->cstr_length, d->cstr_address);
		HANDSHAKE_DEBUG(fprintf(stderr, "Cli: receiveResponse: authenticate(%s)\n", cBlock.plugins.name()));
		if (cBlock.plugins.plugin()->authenticate(&s, &cBlock) == IAuth::AUTH_FAILED)
		{
			break;
		}

		// send answer (may be empty) to server
		if (port->port_protocol >= PROTOCOL_VERSION13)
		{
			packet->p_operation = op_cont_auth;
			cBlock.extractDataFromPluginTo(&packet->p_auth_cont);
		}
		else
		{
			packet->p_operation = op_trusted_auth;
			cBlock.extractDataFromPluginTo(&packet->p_trau.p_trau_data);
		}
		send_packet(port, packet);
		REMOTE_free_packet(port, packet, true);
		memset(&packet->p_auth_cont, 0, sizeof packet->p_auth_cont);
	}

	// If we have exited from the cycle, this mean auth failed
	(Arg::Gds(isc_login) << Arg::StatusVector(&s)).raise();
}

static void init(CheckStatusWrapper* status, ClntAuthBlock& cBlock, rem_port* port, P_OP op, PathName& file_name,
	ClumpletWriter& dpb, IntlParametersBlock& intlParametersBlock, ICryptKeyCallback* cryptCallback)
{
/**************************************
 *
 *	i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize for database access.  First call from both CREATE and
 *	OPEN.
 *
 **************************************/
	try
	{
		Rdb* rdb = port->port_context;
		PACKET* packet = &rdb->rdb_packet;

		MemoryPool& pool = *getDefaultMemoryPool();
		port->port_deferred_packets = FB_NEW_POOL(pool) PacketQueue(pool);

		if (port->port_protocol < PROTOCOL_VERSION12)
		{
			// This is FB < 2.5. Lets remove that not recognized DPB/SPB and convert the UTF8
			// strings to the OS codepage.
			intlParametersBlock.fromUtf8(dpb);
			ISC_unescape(file_name);
			ISC_utf8ToSystem(file_name);
		}

		const ParametersSet* const ps = (op == op_service_attach ? &spbParam : &dpbParam);

		HANDSHAKE_DEBUG(fprintf(stderr, "Cli: init calls authFillParametersBlock\n"));
		authFillParametersBlock(cBlock, dpb, ps, port);

		port->port_client_crypt_callback = cryptCallback;
		cBlock.createCryptCallback(&port->port_client_crypt_callback);

		// Make attach packet
		P_ATCH* attach = &packet->p_atch;
		packet->p_operation = op;
		attach->p_atch_file.cstr_length = (ULONG) file_name.length();
		attach->p_atch_file.cstr_address = reinterpret_cast<const UCHAR*>(file_name.c_str());
		attach->p_atch_dpb.cstr_length = (ULONG) dpb.getBufferLength();
		attach->p_atch_dpb.cstr_address = dpb.getBuffer();

		send_packet(port, packet);

		authReceiveResponse(false, cBlock, port, rdb, status, packet, true);
	}
	catch (const Exception&)
	{
		disconnect(port);
		throw;
	}
}


static Rtr* make_transaction( Rdb* rdb, USHORT id)
{
/**************************************
 *
 *	m a k e _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Create a local transaction handle.
 *
 **************************************/
	Rtr* transaction = FB_NEW Rtr;
	transaction->rtr_rdb = rdb;
	transaction->rtr_id = id;
	transaction->rtr_next = rdb->rdb_transactions;
	rdb->rdb_transactions = transaction;
	SET_OBJECT(rdb, transaction, id);

	return transaction;
}


static void mov_dsql_message(const UCHAR* from_msg,
							 const rem_fmt* from_fmt,
							 UCHAR* to_msg,
							 const rem_fmt* to_fmt)
{
/**************************************
 *
 *	m o v _ d s q l _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Move data using formats.
 *
 **************************************/

	if (!from_msg || !from_fmt || !to_msg || !to_fmt ||
		from_fmt->fmt_desc.getCount() != to_fmt->fmt_desc.getCount())
	{
		move_error(Arg::Gds(isc_dsql_sqlda_err));
		// Msg 263 SQLDA missing or wrong number of variables
	}

	const dsc* from_desc = from_fmt->fmt_desc.begin();
	const dsc* to_desc = to_fmt->fmt_desc.begin();
	for (const dsc* const end_desc = to_fmt->fmt_desc.end();
		to_desc < end_desc; from_desc++, to_desc++)
	{
		dsc from = *from_desc;
		dsc to = *to_desc;
		// Safe const cast, we are going to move from it to anywhere.
		from.dsc_address = const_cast<UCHAR*>(from_msg) + (IPTR) from.dsc_address;
		to.dsc_address = to_msg + (IPTR) to.dsc_address;
		CVT_move(&from, &to, DecimalStatus(FB_DEC_Errors), move_error);
	}
}


static void move_error(const Arg::StatusVector& v)
{
/**************************************
 *
 *	m o v e _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	A conversion error occurred.  Complain.
 *
 **************************************/

	Arg::Gds status_vector(isc_random);
	status_vector << "Dynamic SQL Error" << Arg::Gds(isc_sqlerr) << Arg::Num(-303);

	// append any other arguments which may have been handed to us, then post the error
	status_vector.append(v);

	status_exception::raise(status_vector);
}


static void receive_after_start(Rrq* request, USHORT msg_type)
{
/*****************************************
 *
 *	r e c e i v e _ a f t e r _ s t a r t
 *
 *****************************************
 *
 * Functional Description
 *	Some opcodes, such as "start_and_send" automatically start the
 *	cursor being started, under protcol 8 we then receive the first
 *	batch of records without having to ask for them.
 *
 *	Note: if a network error occurs during this receive, we do not
 *	recognize it in the "gds_start" API call that initiated this
 *	action.  It will be stored with the queue of records for the
 *	cursor that is being fetched.  This is not ideal - but compabile
 *	with how the code worked prior to pipelining work done
 *	1996-Jul-15 David Schnepper
 *
 *****************************************/

	// Check to see if any data is waiting to happen

	Rdb* rdb = request->rrq_rdb;
	PACKET* packet = &rdb->rdb_packet;
	Rrq::rrq_repeat* tail = &request->rrq_rpt[msg_type];
	// CVC: I commented this line because it's overwritten immediately in the loop.
	// RMessage* message = tail->rrq_message;
	const rem_fmt* format = tail->rrq_format;

	// Swallow up data.  If a buffer isn't available, allocate another

	while (true)
	{
		RMessage* message = tail->rrq_xdr;
		if (message->msg_address)
		{
			RMessage* new_msg = FB_NEW RMessage(format->fmt_length);
			tail->rrq_xdr = new_msg;
			new_msg->msg_next = message;
			new_msg->msg_number = message->msg_number;

			while (message->msg_next != new_msg->msg_next)
				message = message->msg_next;
			message->msg_next = new_msg;
		}

		// Note: not receive_packet
		try
		{
			receive_packet_noqueue(rdb->rdb_port, packet);
		}
		catch (const Exception& ex)
		{
			request->saveStatus(ex);
			return;
		}

		// Did an error response come back ?
		if (packet->p_operation != op_send)
		{
			try
			{
				LocalStatus ls;
				CheckStatusWrapper status(&ls);
				REMOTE_check_response(&status, rdb, packet);
				request->saveStatus(&status);
			}
			catch (const Exception& ex)
			{
				request->saveStatus(ex);
			}
			return;
		}

		tail->rrq_msgs_waiting++;

		// Reached end of batch

		if (!packet->p_data.p_data_messages)
			break;
	}
}


static void receive_packet(rem_port* port, PACKET* packet)
{
/**************************************
 *
 *	r e c e i v e _ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	Clear the queue of any pending receives, then receive the
 *	response to a sent request, blocking if necessary until
 *	the response is present.
 *
 * Return codes:
 *	true  - no errors.
 *	false - Network error occurred, error code in status
 *
 **************************************/

	// Must clear the wire of any queued receives before fetching
	// the desired packet

	clear_queue(port);
	receive_packet_noqueue(port, packet);
}


static void receive_packet_with_callback(rem_port* port, PACKET* packet)
{
/**************************************
 *
 *	r e c e i v e _ p a c k e t _ w i t h _ c a l l b a c k
 *
 **************************************
 *
 * Functional description
 *	If received packet is request from callback info from user,
 *	send requested info (or no data if callback is not set) and
 *	wait for next packet.
 *
 **************************************/

	UCharBuffer buf;
	for (;;)
	{
		if (!port->receive(packet))
		{
			Arg::Gds(isc_net_read_err).raise();
		}

		switch (packet->p_operation)
		{
		case op_crypt_key_callback:
			{
				P_CRYPT_CALLBACK* cc = &packet->p_cc;

				if (port->port_client_crypt_callback)
				{
					if (cc->p_cc_reply <= 0)
					{
						cc->p_cc_reply = 1;
					}
					UCHAR* reply = buf.getBuffer(cc->p_cc_reply);
					unsigned l = port->port_client_crypt_callback->callback(cc->p_cc_data.cstr_length,
						cc->p_cc_data.cstr_address, cc->p_cc_reply, reply);

					REMOTE_free_packet(port, packet, true);
					cc->p_cc_data.cstr_length = l;
					cc->p_cc_data.cstr_address = reply;
				}
				else
				{
					REMOTE_free_packet(port, packet, true);
					cc->p_cc_data.cstr_length = 0;
				}

				packet->p_operation = op_crypt_key_callback;
				cc->p_cc_reply = 0;
				port->send(packet);
			}
			break;
		default:
			return;
		}
	}
}


static void receive_packet_noqueue(rem_port* port, PACKET* packet)
{
/**************************************
 *
 *	r e c e i v e _ p a c k e t _ n o q u e u e
 *
 **************************************
 *
 * Functional description
 *	Receive a packet and check for a network
 *	error on the receive.
 *	Note: SOME of the network lower level protocols
 *	will set up a status vector when errors
 *	occur, but other ones won't.
 *	So this routine sets up an error result
 *	for the vector prior to going into the
 *	network layer.  Note that we can't
 *	RESET the status vector as one thing
 *	that can be received is a new status vector
 *
 *	See also cousin routine: send_packet, send_partial_packet
 *
 **************************************/

	// Receive responses for all deferred packets that were already sent

	if (port->port_deferred_packets)
	{
		while (port->port_deferred_packets->getCount())
		{
			rem_que_packet* const p = port->port_deferred_packets->begin();
			if (!p->sent)
				break;

			OBJCT stmt_id = 0;
			bool bCheckResponse = false, bFreeStmt = false, bAssign = false;

			switch (p->packet.p_operation)
			{
			case op_execute:
				stmt_id = p->packet.p_sqldata.p_sqldata_statement;
				bCheckResponse = true;
				bAssign = true;
				break;

			case op_batch_msg:
				stmt_id = p->packet.p_batch_msg.p_batch_statement;
				bCheckResponse = true;
				break;

			case op_batch_create:
				stmt_id = p->packet.p_batch_create.p_batch_statement;
				bCheckResponse = true;
				break;

			case op_free_statement:
				stmt_id = p->packet.p_sqlfree.p_sqlfree_statement;
				bFreeStmt = (p->packet.p_sqlfree.p_sqlfree_option == DSQL_drop);
				break;
			}

			receive_packet_with_callback(port, &p->packet);

			Rsr* statement = NULL;
			if (bCheckResponse || bFreeStmt)
				statement = port->port_objects[stmt_id];

			if (bCheckResponse)
			{
				try
				{
					Rdb* rdb = port->port_context;
					LocalStatus ls;
					CheckStatusWrapper status(&ls);
					REMOTE_check_response(&status, rdb, &p->packet);
					statement->saveException(&status, false);
				}
				catch (const Exception& ex)
				{
					// save error within the corresponding statement
					statement->saveException(ex, false);
					bAssign = false;
				}

				if (bAssign)
				{
					// assign statement to transaction
					const OBJCT tran_id = p->packet.p_sqldata.p_sqldata_transaction;
					Rtr* transaction = port->port_objects[tran_id];
					statement->rsr_rtr = transaction;
				}
			}

			if (bFreeStmt && p->packet.p_resp.p_resp_object == INVALID_OBJECT)
				release_sql_request(statement);

			// free only part of packet we worked with
			REMOTE_free_packet(port, &p->packet, true);
			port->port_deferred_packets->remove(p);
		}
	}

	receive_packet_with_callback(port, packet);
}


static void receive_queued_packet(rem_port* port, USHORT id)
{
/**************************************
 *
 *	r e c e i v e _ q u e u e d_ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	We're marked as having pending receives on the
 *	wire.  Grab the first pending receive and return.
 *
 **************************************/
	// Trivial case, nothing pending on the wire

	if (!port->port_receive_rmtque)
	{
		return;
	}

	// Grab first queue entry

	rmtque* que_inst = port->port_receive_rmtque;

	// Receive the data

	(que_inst->rmtque_function) (port, que_inst, id);
}


static void enqueue_receive(rem_port* port,
							t_rmtque_fn fn,
							Rdb* rdb,
							void* parm,
							Rrq::rrq_repeat* parm1)
{
/**************************************
 *
 *	e n q u e u e _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	rmtque* const que_inst = FB_NEW rmtque;

	// Prepare a queue entry

	que_inst->rmtque_next = NULL;
	que_inst->rmtque_function = fn;
	que_inst->rmtque_parm = parm;
	que_inst->rmtque_message = parm1;
	que_inst->rmtque_rdb = rdb;

	// Walk to the end of the current queue
	rmtque** queptr = &port->port_receive_rmtque;
	while (*queptr)
		queptr = &(*queptr)->rmtque_next;

	// Add the new entry to the end of the queue

	*queptr = que_inst;
}


static void dequeue_receive( rem_port* port)
{
/**************************************
 *
 *	d e q u e u e _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	// Grab first queue entry & de-queue it

	rmtque* que_inst = port->port_receive_rmtque;
	port->port_receive_rmtque = que_inst->rmtque_next;
	que_inst->rmtque_next = NULL;

	// Add queue entry onto free queue

	delete que_inst;
}


static void receive_response(IStatus* status, Rdb* rdb, PACKET* packet)
{
/**************************************
 *
 *	r e c e i v e _ r e s p o n s e
 *
 **************************************
 *
 * Functional description
 *	Check response to a remote call.
 *
 **************************************/

	receive_packet(rdb->rdb_port, packet);
	REMOTE_check_response(status, rdb, packet);
}


static void release_blob( Rbl* blob)
{
/**************************************
 *
 *	r e l e a s e _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Release a blob block and friends.
 *
 **************************************/
	Rtr* transaction = blob->rbl_rtr;
	Rdb* rdb = blob->rbl_rdb;
	rdb->rdb_port->releaseObject(blob->rbl_id);

	for (Rbl** p = &transaction->rtr_blobs; *p; p = &(*p)->rbl_next)
	{
		if (*p == blob)
		{
			*p = blob->rbl_next;
			break;
		}
	}

	delete blob;
}


static void release_event( Rvnt* event)
{
/**************************************
 *
 *	r e l e a s e _ e v e n t
 *
 **************************************
 *
 * Functional description
 *	Release an event block.
 *
 **************************************/
	Rdb* rdb = event->rvnt_rdb;

	for (Rvnt** p = &rdb->rdb_events; *p; p = &(*p)->rvnt_next)
	{
		if (*p == event)
		{
			*p = event->rvnt_next;
			break;
		}
	}

	delete event;
}


static void release_object(IStatus* status, Rdb* rdb, P_OP op, USHORT id)
{
/**************************************
 *
 *	r e l e a s e _ o b j e c t
 *
 **************************************
 *
 * Functional description
 *	Tell the server to zap an object.  This doesn't necessary
 *	release the object, but usually does.
 *
 **************************************/
	PACKET* packet = &rdb->rdb_packet;
	packet->p_operation = op;
	packet->p_rlse.p_rlse_object = id;

	if (rdb->rdb_port->port_flags & PORT_lazy)
	{
		switch (op)
		{
			case op_close_blob:
			case op_cancel_blob:
			case op_release:
				defer_packet(rdb->rdb_port, packet);
				return;
			default:
				break;
		}
	}

	send_packet(rdb->rdb_port, packet);
	receive_response(status, rdb, packet);
}


static void release_request( Rrq* request)
{
/**************************************
 *
 *	r e l e a s e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Release a request block and friends.
 *
 **************************************/
	Rdb* rdb = request->rrq_rdb;
	rdb->rdb_port->releaseObject(request->rrq_id);
	REMOTE_release_request(request);
}


static void release_statement( Rsr** statement)
{
/**************************************
 *
 *	r e l e a s e _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Release a GDML or SQL statement block ?
 *
 **************************************/

	delete (*statement)->rsr_bind_format;
	if ((*statement)->rsr_user_select_format &&
		(*statement)->rsr_user_select_format != (*statement)->rsr_select_format)
	{
		delete (*statement)->rsr_user_select_format;
	}
	delete (*statement)->rsr_select_format;
	(*statement)->releaseException();

	REMOTE_release_messages((*statement)->rsr_message);
	delete *statement;
	*statement = NULL;
}


static void release_sql_request( Rsr* statement)
{
/**************************************
 *
 *	r e l e a s e _ s q l _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Release an SQL request block.
 *
 **************************************/
	Rdb* rdb = statement->rsr_rdb;
	rdb->rdb_port->releaseObject(statement->rsr_id);

	for (Rsr** p = &rdb->rdb_sql_requests; *p; p = &(*p)->rsr_next)
	{
		if (*p == statement)
		{
			*p = statement->rsr_next;
			break;
		}
	}

	release_statement(&statement);
}


static void release_transaction( Rtr* transaction)
{
/**************************************
 *
 *	r e l e a s e _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Release a transaction block and friends.
 *
 **************************************/
	Rdb* rdb = transaction->rtr_rdb;
	rdb->rdb_port->releaseObject(transaction->rtr_id);

	while (transaction->rtr_blobs)
		release_blob(transaction->rtr_blobs);

	for (Rtr** p = &rdb->rdb_transactions; *p; p = &(*p)->rtr_next)
	{
		if (*p == transaction)
		{
			*p = transaction->rtr_next;
			break;
		}
	}

	delete transaction;
}


static void send_and_receive(IStatus* status, Rdb* rdb, PACKET* packet)
{
/**************************************
 *
 *	s e n d _ a n d _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Send a packet, check status, receive a packet, and check status.
 *
 **************************************/

	send_packet(rdb->rdb_port, packet);
	receive_response(status, rdb, packet);
}


static void send_blob(CheckStatusWrapper*		status,
					  Rbl*			blob,
					  USHORT		buffer_length,
					  const UCHAR*	buffer)
{
/**************************************
 *
 *	s e n d _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Actually send blob data (which might be buffered)
 *
 **************************************/
	Rdb* rdb = blob->rbl_rdb;
	PACKET* packet = &rdb->rdb_packet;
	packet->p_operation = op_put_segment;

	// If we aren't passed a buffer address, this is a batch send.  Pick up the
	// address and length from the blob buffer and blast away

	if (!buffer)
	{
		buffer = blob->rbl_buffer;
		buffer_length = blob->rbl_ptr - buffer;
		blob->rbl_ptr = blob->rbl_buffer;
		packet->p_operation = op_batch_segments;
	}

	P_SGMT* segment = &packet->p_sgmt;
	CSTRING_CONST temp = segment->p_sgmt_segment;
	segment->p_sgmt_blob = blob->rbl_id;
	segment->p_sgmt_segment.cstr_length = buffer_length;
	segment->p_sgmt_segment.cstr_address = buffer;
	segment->p_sgmt_length = buffer_length;

	send_packet(rdb->rdb_port, packet);

     // restore the string; "buffer" is not referenced anymore, hence no
     // possibility to overwrite it accidentally.
	segment->p_sgmt_segment = temp;

	// Set up for the response packet.

	receive_response(status, rdb, packet);
}


static void send_packet(rem_port* port, PACKET* packet)
{
/**************************************
 *
 *	s e n d _ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	Send a packet and check for a network error
 *	on the send.
 *	Make up a status vector for any error.
 *	Note: SOME of the network lower level protocols
 *	will set up a status vector when errors
 *	occur, but other ones won't.
 *	So this routine sets up an error result
 *	for the vector and resets it to true
 *	if the packet send occurred.
 *
 *	See also cousin routine: receive_packet
 *
 **************************************/

	RefMutexGuard guard(*port->port_write_sync, FB_FUNCTION);

	// Send packets that were deferred

	if (port->port_deferred_packets)
	{
		for (rem_que_packet* p = port->port_deferred_packets->begin();
			 p < port->port_deferred_packets->end();
			 ++p)
		{
			if (!p->sent)
			{
				if (!port->send_partial(&p->packet))
					(Arg::Gds(isc_net_write_err) <<
					 Arg::Gds(isc_random) << "send_packet/send_partial").raise();

				p->sent = true;
			}
		}
	}

	if (!port->send(packet))
	{
		(Arg::Gds(isc_net_write_err)<< Arg::Gds(isc_random) << "send_packet/send").raise();
	}
}

static void send_partial_packet(rem_port* port, PACKET* packet)
{
/**************************************
 *
 *	s e n d _ p a r t i a l _ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	Send a packet and check for a network error
 *	on the send.
 *	Make up a status vector for any error.
 *	Note: SOME of the network lower level protocols
 *	will set up a status vector when errors
 *	occur, but other ones won't.
 *	So this routine sets up an error result
 *	for the vector and resets it to true
 *	if the packet send occurred.
 *
 *	See also cousin routine: receive_packet, send_packet
 *
 **************************************/

	RefMutexGuard guard(*port->port_write_sync, FB_FUNCTION);

	// Send packets that were deferred

	for (rem_que_packet* p = port->port_deferred_packets->begin();
		p < port->port_deferred_packets->end(); p++)
	{
		if (!p->sent)
		{
			if (!port->send_partial(&p->packet))
			{
				(Arg::Gds(isc_net_write_err) <<
				 Arg::Gds(isc_random) << "send_partial_packet/send_partial").raise();
			}
			p->sent = true;
		}
	}

	if (!port->send_partial(packet))
	{
		(Arg::Gds(isc_net_write_err) <<
		 Arg::Gds(isc_random) << "send_partial_packet/send").raise();
	}
}

static void server_death(rem_port* port)
{
/**************************************
 *
 *	s e r v e r _ d e a t h
 *
 **************************************
 *
 * Functional description
 *	Received "EOF" from remote server
 *	Cleanup events.
 *
 **************************************/
	Rdb* rdb = port->port_context;

	if (rdb && !(port->port_flags & PORT_disconnect))
	{
		for (Rvnt* event = rdb->rdb_events; event; event = event->rvnt_next)
		{
			if (event->rvnt_id)
			{
				event->rvnt_id = 0;
				event->rvnt_callback->eventCallbackFunction(0, NULL);
			}
		}
	}
}


static void svcstart(CheckStatusWrapper*	status,
					 Rdb*		rdb,
					 P_OP		operation,
					 USHORT		object,
					 USHORT		incarnation,
					 USHORT		item_length,
					 const UCHAR* items)
 {
/**************************************
 *
 *	s v c s t a r t
 *
 **************************************
 *
 * Functional description
 *	Instruct the server to start a service
 *
 **************************************/

	ClumpletWriter send(ClumpletReader::SpbStart, MAX_DPB_SIZE, items, item_length);
	if (rdb->rdb_port->port_protocol < PROTOCOL_VERSION13)
	{
		// This is FB < 3.0. Lets convert the UTF8 strings to the OS codepage.
		IntlSpbStart().fromUtf8(send);
	}

	// Build the primary packet to get the operation started.
	PACKET* packet = &rdb->rdb_packet;
	packet->p_operation = operation;
	P_INFO* information = &packet->p_info;
	information->p_info_object = object;
	information->p_info_incarnation = incarnation;
	information->p_info_items.cstr_length = (ULONG) send.getBufferLength();
	information->p_info_items.cstr_address = send.getBuffer();
	information->p_info_buffer_length = (ULONG) send.getBufferLength();

	send_packet(rdb->rdb_port, packet);

	// Set up for the response packet.
	P_RESP* response = &packet->p_resp;
	SaveString temp(response->p_resp_data, 0, NULL);
	response->p_resp_data.cstr_length = 0;

	receive_response(status, rdb, packet);
}


static void unsupported()
{
/**************************************
 *
 *	u n s u p p o r t e d
 *
 **************************************
 *
 * Functional description
 *	No_entrypoint is called if there is not entrypoint for a given routine.
 *
 **************************************/

	Arg::Gds(isc_wish_list).raise();
}


static void zap_packet(PACKET* packet)
{
/**************************************
 *
 *	z a p _ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	Zero out a packet block.
 *
 **************************************/

	memset(packet, 0, sizeof(struct packet));
}


void Attachment::cancelOperation(CheckStatusWrapper* status, int kind)
{
/*************************************
 *
 * 	G D S _ C A N C E L _ O P E R A T I O N
 *
 **************************************
 *
 * Functional description
 *	Asynchronously cancel requests, running with db_handle on remote server.
 *
 **************************************/

	try {
		reset(status);
		CHECK_HANDLE(rdb, isc_bad_db_handle);
		RemPortPtr port(rdb->rdb_port);

		if (kind == fb_cancel_abort)
		{
			port->force_close();
			return;
		}

		if (port->port_protocol < PROTOCOL_VERSION12 || port->port_type != rem_port::INET)
		{
			unsupported();
		}

		MutexEnsureUnlock guard(rdb->rdb_async_lock, FB_FUNCTION);	// This is async operation
		if (!guard.tryEnter())
		{
			Arg::Gds(isc_async_active).raise();
		}

		PACKET packet;
		packet.p_operation = op_cancel;
		P_CANCEL_OP* cancel = &packet.p_cancel_op;
		cancel->p_co_kind = kind;

		send_packet(rdb->rdb_port, &packet);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

Rtr* Attachment::remoteTransaction(ITransaction* apiTra)
{
	Transaction* rt = remoteTransactionInterface(apiTra);
	return rt ? rt->getTransaction() : NULL;
}

Transaction* Attachment::remoteTransactionInterface(ITransaction* apiTra)
{
	if (!apiTra)
		return NULL;

	LocalStatus ls;
	CheckStatusWrapper dummy(&ls);
	ITransaction* valid = apiTra->validate(&dummy, this);
	if (!valid)
		return NULL;

	// If validation is successfull, this means that this attachment and valid transaction
	// use same provider. I.e. the following cast is safe.
	return static_cast<Transaction*>(valid);
}

static void cleanDpb(Firebird::ClumpletWriter& dpb, const ParametersSet* tags)
{
	dpb.deleteWithTag(tags->password);
	dpb.deleteWithTag(tags->password_enc);
	dpb.deleteWithTag(tags->trusted_auth);
}

} //namespace Remote


RmtAuthBlock::RmtAuthBlock(const Firebird::AuthReader::AuthBlock& aBlock)
	: buffer(*getDefaultMemoryPool(), aBlock),
	  rdr(*getDefaultMemoryPool(), buffer),
	  info(*getDefaultMemoryPool())
{
	FbLocalStatus st;
	first(&st);
	check(&st);
}

const char* RmtAuthBlock::getType()
{
	return info.type.nullStr();
}

const char* RmtAuthBlock::getName()
{
	return info.name.nullStr();
}

const char* RmtAuthBlock::getPlugin()
{
	return info.plugin.nullStr();
}

const char* RmtAuthBlock::getSecurityDb()
{
	return info.secDb.nullStr();
}

const char* RmtAuthBlock::getOriginalPlugin()
{
	return info.origPlug.nullStr();
}

FB_BOOLEAN RmtAuthBlock::next(Firebird::CheckStatusWrapper* status)
{
	try
	{
		rdr.moveNext();
		return loadInfo();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return FB_FALSE;
}

FB_BOOLEAN RmtAuthBlock::first(Firebird::CheckStatusWrapper* status)
{
	try
	{
		rdr.rewind();
		return loadInfo();
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return FB_FALSE;
}

FB_BOOLEAN RmtAuthBlock::loadInfo()
{
	if (rdr.isEof())
		return FB_FALSE;
	rdr.getInfo(info);
	return FB_TRUE;
}


ClntAuthBlock::ClntAuthBlock(const Firebird::PathName* fileName, Firebird::ClumpletReader* dpb,
							 const ParametersSet* tags)
	: pluginList(getPool()), serverPluginList(getPool()),
	  cliUserName(getPool()), cliPassword(getPool()), cliOrigUserName(getPool()),
	  dataForPlugin(getPool()), dataFromPlugin(getPool()),
	  cryptKeys(getPool()), dpbConfig(getPool()), dpbPlugins(getPool()),
	  createdInterface(nullptr),
	  plugins(IPluginManager::TYPE_AUTH_CLIENT), authComplete(false), firstTime(true)
{
	if (dpb && tags)
	{
		if (dpb->find(tags->config_text))
			dpb->getString(dpbConfig);

		if (dpb->find(tags->plugin_list))
			dpb->getPath(dpbPlugins);

		if (dpb->find(tags->auth_block))
		{
			AuthReader::AuthBlock plain;
			plain.add(dpb->getBytes(), dpb->getClumpLength());
			remAuthBlock.reset(FB_NEW RmtAuthBlock(plain));
		}
	}
	clntConfig = REMOTE_get_config(fileName, &dpbConfig);
	resetClnt();
}

void ClntAuthBlock::resetDataFromPlugin()
{
	dataFromPlugin.clear();
}

void ClntAuthBlock::extractDataFromPluginTo(Firebird::ClumpletWriter& dpb,
									  const ParametersSet* tags,
									  int protocol)
{
	if (!dataFromPlugin.hasData())
	{
		return;
	}

	PathName pluginName = getPluginName();
	if (protocol >= PROTOCOL_VERSION13)
	{
		if (firstTime)
		{
			fb_assert(tags->plugin_name && tags->plugin_list);

			if (pluginName.hasData())
				dpb.insertString(tags->plugin_name, pluginName);

			dpb.deleteWithTag(tags->plugin_list);
			dpb.insertString(tags->plugin_list, pluginList);
			firstTime = false;
			HANDSHAKE_DEBUG(fprintf(stderr,
				"Cli: extractDataFromPluginTo: first time - added plugName & pluginList\n"));
		}
		fb_assert(tags->specific_data);
		dpb.insertBytes(tags->specific_data, dataFromPlugin.begin(), dataFromPlugin.getCount());

		HANDSHAKE_DEBUG(fprintf(stderr,
			"Cli: extractDataFromPluginTo: Added %u bytes of spec data with tag %d\n",
			static_cast<unsigned>(dataFromPlugin.getCount()), tags->specific_data));

		return;
	}

	if (REMOTE_legacy_auth(pluginName.c_str(), PROTOCOL_VERSION10))	// dataFromPlugin is encrypted password
	{
		fb_assert(tags->password_enc);
		dpb.insertBytes(tags->password_enc, dataFromPlugin.begin(), dataFromPlugin.getCount());
		return;
	}

	fb_assert(REMOTE_legacy_auth(pluginName.c_str(), protocol));		// dataFromPlugin must be trustedAuth
	fb_assert(tags->trusted_auth);
	dpb.insertBytes(tags->trusted_auth, dataFromPlugin.begin(), dataFromPlugin.getCount());
}

static inline void makeUtfString(bool uft8Convert, Firebird::string& s)
{
	if (uft8Convert)
	{
		ISC_systemToUtf8(s);
	}
	ISC_unescape(s);
}

void ClntAuthBlock::loadClnt(Firebird::ClumpletWriter& dpb, const ParametersSet* tags)
{
	bool uft8Convert = !dpb.find(tags->utf8_filename);

	for (dpb.rewind(); !dpb.isEof(); dpb.moveNext())
	{
		const UCHAR t = dpb.getClumpTag();
		if (t == tags->user_name)
		{
			dpb.getString(cliUserName);
			makeUtfString(uft8Convert, cliUserName);
			cliOrigUserName = cliUserName;
			fb_utils::dpbItemUpper(cliUserName);
			HANDSHAKE_DEBUG(fprintf(stderr, "Cli: loadClnt: Loaded from PB user = %s(was %s)\n",
				cliUserName.c_str(), cliOrigUserName.c_str()));
		}
		else if (t == tags->password)
		{
			dpb.getString(cliPassword);
			makeUtfString(uft8Convert, cliPassword);
			HANDSHAKE_DEBUG(fprintf(stderr,
				"Cli: loadClnt: Loaded from PB cliPassword = %s\n", cliPassword.c_str()));
		}
		else if (t == tags->encrypt_key)
		{
			HANDSHAKE_DEBUG(fprintf(stderr,
				"Cli: loadClnt: PB contains crypt key\n"));
		}
	}

	dpb.deleteWithTag(tags->password);
}

void ClntAuthBlock::extractDataFromPluginTo(CSTRING* to)
{
	to->cstr_length = (ULONG) dataFromPlugin.getCount();
	to->cstr_address = dataFromPlugin.begin();
	to->cstr_allocated = 0;
}

void ClntAuthBlock::extractDataFromPluginTo(P_AUTH_CONT* to)
{
	extractDataFromPluginTo(&to->p_data);

	PathName pluginName = getPluginName();
	to->p_name.cstr_length = (ULONG) pluginName.length();
	to->p_name.cstr_address = FB_NEW_POOL(*getDefaultMemoryPool()) UCHAR[to->p_name.cstr_length];
	to->p_name.cstr_allocated = to->p_name.cstr_length;
	memcpy(to->p_name.cstr_address, pluginName.c_str(), to->p_name.cstr_length);

	HANDSHAKE_DEBUG(fprintf(stderr, "Cli: extractDataFromPluginTo: added plugin name (%d) and data (%d)\n",
				to->p_name.cstr_length, to->p_data.cstr_length));

	if (firstTime)
	{
		to->p_list.cstr_length = (ULONG) pluginList.length();
		to->p_list.cstr_address = (UCHAR*) pluginList.c_str();
		to->p_list.cstr_allocated = 0;
		HANDSHAKE_DEBUG(fprintf(stderr,
			"Cli: extractDataFromPluginTo: added plugin list (%d len) to packet\n",
			to->p_list.cstr_length));
		firstTime = false;
	}
	else
	{
		to->p_list.cstr_length = 0;
	}
}

const char* ClntAuthBlock::getLogin()
{
	return cliUserName.nullStr();
}

const char* ClntAuthBlock::getPassword()
{
	return cliPassword.nullStr();
}

IAuthBlock* ClntAuthBlock::getAuthBlock(CheckStatusWrapper* status)
{
	return remAuthBlock;
}

const unsigned char* ClntAuthBlock::getData(unsigned int* length)
{
	*length = (ULONG) dataForPlugin.getCount();
	return *length ? dataForPlugin.begin() : NULL;
}

void ClntAuthBlock::putData(CheckStatusWrapper* status, unsigned int length, const void* data)
{
	try
	{
		void* to = dataFromPlugin.getBuffer(length);
		memcpy(to, data, length);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

bool ClntAuthBlock::checkPluginName(Firebird::PathName& nameToCheck)
{
	Firebird::ParsedList parsed(pluginList);
	for (unsigned i = 0; i < parsed.getCount(); ++i)
	{
		if (parsed[i] == nameToCheck)
		{
			return true;
		}
	}
	return false;
}

Firebird::ICryptKey* ClntAuthBlock::newKey(CheckStatusWrapper* status)
{
	status->init();
	try
	{
		InternalCryptKey* k = FB_NEW InternalCryptKey;

		fb_assert(plugins.hasData());
		k->keyName = plugins.name();
		WIRECRYPT_DEBUG(fprintf(stderr, "Cli: newkey %s\n", k->keyName.c_str());)
		cryptKeys.add(k);

		return k;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}

void ClntAuthBlock::tryNewKeys(rem_port* port)
{
	for (unsigned k = cryptKeys.getCount(); k--; )
	{
		if (port->tryNewKey(cryptKeys[k]))
		{
			releaseKeys(k);
			cryptKeys.clear();
			return;
		}
	}

	cryptKeys.clear();
}

void ClntAuthBlock::releaseKeys(unsigned from)
{
	while (from < cryptKeys.getCount())
	{
		delete cryptKeys[from++];
	}
}

void ClntAuthBlock::createCryptCallback(Firebird::ICryptKeyCallback** callback)
{
	if (*callback)
		return;

	*callback = clientCrypt.create(clntConfig);
	if (*callback)
		createdInterface = callback;
}

Firebird::ICryptKeyCallback* ClntAuthBlock::ClientCrypt::create(const Config* conf)
{
	pluginItr.set(conf);

	return pluginItr.hasData() ? this : nullptr;
}

unsigned ClntAuthBlock::ClientCrypt::callback(unsigned dlen, const void* data, unsigned blen, void* buffer)
{
	HANDSHAKE_DEBUG(fprintf(stderr, "dlen=%d blen=%d\n", dlen, blen));

	int loop = 0;
	while (loop < 2)
	{
		for (; pluginItr.hasData(); pluginItr.next())
		{
			if (!currentIface)
			{
				LocalStatus ls;
				CheckStatusWrapper st(&ls);

				HANDSHAKE_DEBUG(fprintf(stderr, "Try plugin %s\n", pluginItr.name()));
				currentIface = pluginItr.plugin()->chainHandle(&st);
				// if plugin does not support chaining - silently ignore it
				check(&st, isc_wish_list);
				HANDSHAKE_DEBUG(fprintf(stderr, "Use plugin %s, ptr=%p\n", pluginItr.name(), currentIface));
			}

			// if we have an iface - try it
			if (currentIface)
			{
				unsigned retlen = currentIface->callback(dlen, data, blen, buffer);
				HANDSHAKE_DEBUG(fprintf(stderr, "Iface %p returned %d\n", currentIface, retlen));
				if (retlen)
					return retlen;
			}

			// no success with iface - clear it
			// appropriate data structures to be released by plugin cleanup code
			currentIface = nullptr;
		}

		++loop;
		// prepare iterator for next use
		pluginItr.rewind();
	}

	// no luck with suggested data
	return 0;
}
