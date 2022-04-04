/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		why.cpp
 *	DESCRIPTION:	Universal Y-valve
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
 * Dmitry Yemanov
 * Sean Leyne
 * Alex Peshkoff
 * Adriano dos Santos Fernandes
 *
 */

#include "firebird.h"

#define FB_UsedInYValve true
#include "firebird/Interface.h"

#include "memory_routines.h"
#include "iberror.h"
#include "../yvalve/YObjects.h"
#include "firebird/impl/sqlda_pub.h"
#include "../common/gdsassert.h"
#include "../common/db_alias.h"
#include "../common/StatementMetadata.h"
#include "../common/StatusHolder.h"
#include "../common/ThreadStart.h"
#include "../common/isc_proto.h"
#include "../common/isc_f_proto.h"
#include "../common/utils_proto.h"
#include "../common/IntlParametersBlock.h"
#include "../common/os/isc_i_proto.h"
#include "../common/os/path_utils.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/stack.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/fb_atomic.h"
#include "../common/classes/init.h"
#include "../common/classes/rwlock.h"
#include "../common/classes/semaphore.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/FpeControl.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/GetPlugins.h"
#include "../common/classes/fb_tls.h"
#include "../common/status.h"
#include "../common/classes/InternalMessageBuffer.h"
#include "../yvalve/utl_proto.h"
#include "../yvalve/why_proto.h"
#include "../yvalve/MasterImplementation.h"
#include "../yvalve/PluginManager.h"
#include "../jrd/acl.h"
#include "../jrd/align.h"
#include "firebird/impl/blr.h"
#include "../common/msg_encode.h"
#include "firebird/impl/inf_pub.h"

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <functional>

using namespace Firebird;
using namespace Why;


static void badHandle(ISC_STATUS code);
static bool isNetworkError(const IStatus* status);
static void nullCheck(const FB_API_HANDLE* ptr, ISC_STATUS code);
//static void saveErrorString(ISC_STATUS* status);
static void badSqldaVersion(const short version);
static int sqldaTruncateString(char* buffer, FB_SIZE_T size, const char* s);
static void sqldaDescribeParameters(XSQLDA* sqlda, IMessageMetadata* parameters);
static ISC_STATUS openOrCreateBlob(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, isc_blob_handle* blobHandle, ISC_QUAD* blobId,
	USHORT bpbLength, const UCHAR* bpb, bool createFlag);


//-------------------------------------


static const USHORT PREPARE_BUFFER_SIZE = 32768;	// size of buffer used in isc_dsql_prepare call
static const USHORT DESCRIBE_BUFFER_SIZE = 1024;	// size of buffer used in isc_dsql_describe_xxx call

namespace Why {
	class StatusVector;
	extern UtilInterface utilInterface;
};

namespace {

static const struct {
	int fac_code;
	const char* facility;
} facilities[] = {
	{FB_IMPL_MSG_FACILITY_JRD,			"JRD       "},
	{FB_IMPL_MSG_FACILITY_GFIX,			"GFIX      "},
	{FB_IMPL_MSG_FACILITY_DSQL,			"DSQL      "},
	{FB_IMPL_MSG_FACILITY_DYN,			"DYN       "},
	{FB_IMPL_MSG_FACILITY_GBAK,			"GBAK      "},
	{FB_IMPL_MSG_FACILITY_SQLERR,		"SQLERR    "},
	{FB_IMPL_MSG_FACILITY_GSEC,			"GSEC      "},
	{FB_IMPL_MSG_FACILITY_GSTAT,		"GSTAT     "},
	{FB_IMPL_MSG_FACILITY_FBSVCMGR,		"FBSVCMGR  "},
	{FB_IMPL_MSG_FACILITY_UTL,			"UTL       "},
	{FB_IMPL_MSG_FACILITY_NBACKUP,		"NBACKUP   "},
	{FB_IMPL_MSG_FACILITY_FBTRACEMGR,	"FBTRACEMGR"},
	{0, NULL}
};

// Class-wrapper around external SQLDA.
// Can be used as local variable, but do it with care
class SQLDAMetadata final :
	public RefCntIface<IMessageMetadataImpl<SQLDAMetadata, CheckStatusWrapper> >
{
friend class SQLDAMetadataLauncher;
private:
	int detach(); // is called from launcher class when it goes off

	// Fool-proof requested by Alex
	// Private memory operators to be sure that this class is used in heap only with launcher
	void* operator new (size_t s, Firebird::MemoryPool& pool ALLOC_PARAMS) { return pool.allocate(s ALLOC_PASS_ARGS); }
	void operator delete (void* mem, Firebird::MemoryPool& ALLOC_PARAMS) { MemoryPool::globalFree(mem); }
	void operator delete (void* mem) { MemoryPool::globalFree(mem); }

public:
	typedef UCharBuffer DataBuffer;

	SQLDAMetadata(const XSQLDA* aSqlda);
	~SQLDAMetadata() { delete[] offsets; }

	unsigned getCount(CheckStatusWrapper* status);
	const char* getField(CheckStatusWrapper* status, unsigned index);
	const char* getRelation(CheckStatusWrapper* status, unsigned index);
	const char* getOwner(CheckStatusWrapper* status, unsigned index);
	const char* getAlias(CheckStatusWrapper* status, unsigned index);
	unsigned getType(CheckStatusWrapper* status, unsigned index);
	FB_BOOLEAN isNullable(CheckStatusWrapper* status, unsigned index);
	int getSubType(CheckStatusWrapper* status, unsigned index);
	unsigned getLength(CheckStatusWrapper* status, unsigned index);
	int getScale(CheckStatusWrapper* status, unsigned index);
	unsigned getCharSet(CheckStatusWrapper* status, unsigned index);
	unsigned getOffset(CheckStatusWrapper* status, unsigned index);
	unsigned getNullOffset(CheckStatusWrapper* status, unsigned index);

	IMetadataBuilder* getBuilder(CheckStatusWrapper* status);
	unsigned getMessageLength(CheckStatusWrapper* status);
	unsigned getAlignment(CheckStatusWrapper* status);
	unsigned getAlignedLength(CheckStatusWrapper* status);

	void gatherData(DataBuffer& to);	// Copy data from SQLDA into target buffer.
	void scatterData(DataBuffer& from);

private:
	void assign(); // copy metadata from sqlda and calculate length and offsets

	const XSQLDA* sqlda;
	unsigned count;

	// Internal structure to copy metadata from sqlda
	struct OffsetItem
	{
		unsigned type;
		int subType;
		unsigned length;
		int scale;
		unsigned charSet;
		unsigned offset;
		unsigned indOffset;
	} *offsets;

	unsigned length, alignment;
	bool speedHackEnabled; // May be user by stupid luck use right buffer format even with SQLDA interface?..
};

// Initial owner for SQLDAMetadata. It must not live longer that XSQLDA it is created for is know to be valid and fixed.
// Mostly it means that instance of this class has to be a local variable at the same or nested level with the original XSQLDA*.
// When launcher is dying, the rocket must either die too or switch to internal sources and take off
// Using of this class brings overhead of dynamic memory and pointers, but with current architecture (reference counted interfaces
// only plain buffers) - inevitable.
class SQLDAMetadataLauncher
{
public:
	SQLDAMetadata* metadata;

	explicit SQLDAMetadataLauncher(const XSQLDA* sqlda) : metadata(NULL)
	{
		if (sqlda)
		{
			metadata = FB_NEW SQLDAMetadata(sqlda);
			metadata->addRef();
		}
	}

	~SQLDAMetadataLauncher()
	{
		if (metadata)
			metadata->detach();
	}

	UCHAR* getBuffer(); // Provide empty data buffer with the same live time

	void gatherData(SQLDAMetadata::DataBuffer& to)
	{
		if (metadata)
			metadata->gatherData(to);
		else
			to.clear();
	}

 	void scatterData()
	{
		if (metadata)
			metadata->scatterData(buffer);
	}

private:
	// Private operator new prevents this class from being allocated in heap, so it can be used only as stack or static variable
	void* operator new (size_t);

	SQLDAMetadata::DataBuffer buffer;
};

SQLDAMetadata::SQLDAMetadata(const XSQLDA* aSqlda)
	: sqlda(aSqlda), count(0), offsets(NULL), length(0), alignment(0), speedHackEnabled(false)
{
	if (sqlda && sqlda->version != SQLDA_VERSION1)
	{
		badSqldaVersion(sqlda->version);
	}
}

unsigned SQLDAMetadata::getCount(CheckStatusWrapper* status)
{
	if (sqlda)
		return sqlda->sqld;

	return count;
}

const char* SQLDAMetadata::getField(CheckStatusWrapper* status, unsigned index)
{
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int)index);
		return sqlda->sqlvar[index].sqlname;
	}

	// we are in free fly. It is only possible for input sqlda which usually have no names inside
	return "";	// Old conversion sqlda->BLR->metadata dropped them anyway
}

const char* SQLDAMetadata::getRelation(CheckStatusWrapper* status, unsigned index)
{
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int)index);
		return sqlda->sqlvar[index].relname;
	}

	return "";
}

const char* SQLDAMetadata::getOwner(CheckStatusWrapper* status, unsigned index)
{
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		return sqlda->sqlvar[index].ownname;
	}

	return "";
}

const char* SQLDAMetadata::getAlias(CheckStatusWrapper* status, unsigned index)
{
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		return sqlda->sqlvar[index].aliasname;
	}

	return "";
}

unsigned SQLDAMetadata::getType(CheckStatusWrapper* status, unsigned index)
{
	if (offsets)
	{
		fb_assert(count > index);
		return offsets[index].type & ~1;
	}
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		return sqlda->sqlvar[index].sqltype & ~1;
	}

	fb_assert(false);
	return 0;
}

FB_BOOLEAN SQLDAMetadata::isNullable(CheckStatusWrapper* status, unsigned index)
{
	if (offsets)
	{
		fb_assert(count > index);
		return offsets[index].type & 1;
	}
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		return sqlda->sqlvar[index].sqltype & 1;
	}

	fb_assert(false);
	return FB_FALSE;
}

int SQLDAMetadata::getSubType(CheckStatusWrapper* status, unsigned index)
{
	if (offsets)
	{
		fb_assert(count > index);
		return offsets[index].subType;
	}
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		ISC_SHORT sqltype = sqlda->sqlvar[index].sqltype & ~1;
		if (sqltype == SQL_VARYING || sqltype == SQL_TEXT)
			return sqlda->sqlvar[index].sqlsubtype == CS_BINARY ? fb_text_subtype_binary : fb_text_subtype_text;
		return sqlda->sqlvar[index].sqlsubtype;
	}

	fb_assert(false);
	return 0;
}

unsigned SQLDAMetadata::getLength(CheckStatusWrapper* status, unsigned index)
{
	if (offsets)
	{
		fb_assert(count > index);
		return offsets[index].length;
	}
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		return sqlda->sqlvar[index].sqllen;
	}

	fb_assert(false);
	return 0;
}

int SQLDAMetadata::getScale(CheckStatusWrapper* status, unsigned index)
{
	if (offsets)
	{
		fb_assert(count > index);
		return offsets[index].scale;
	}
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		if ((sqlda->sqlvar[index].sqltype & ~1) == SQL_BLOB)
			return 0;
		return sqlda->sqlvar[index].sqlscale;
	}

	fb_assert(false);
	return 0;
}

unsigned SQLDAMetadata::getCharSet(CheckStatusWrapper* status, unsigned index)
{
	if (offsets)
	{
		fb_assert(count > index);
		return offsets[index].charSet;
	}
	if (sqlda)
	{
		fb_assert(sqlda->sqld > (int) index);
		switch (sqlda->sqlvar[index].sqltype & ~1)
		{
		case SQL_TEXT:
		case SQL_VARYING:
			return sqlda->sqlvar[index].sqlsubtype;
		case SQL_BLOB:
			return sqlda->sqlvar[index].sqlscale;
		default:
			return 0;
		}
	}

	fb_assert(false);
	return 0;
}

unsigned SQLDAMetadata::getOffset(CheckStatusWrapper* status, unsigned index)
{
	if (!offsets)
		assign();

	fb_assert(count > index);
	return offsets[index].offset;
}

unsigned SQLDAMetadata::getNullOffset(CheckStatusWrapper* status, unsigned index)
{
	if (!offsets)
		assign();

	fb_assert(count > index);
	return offsets[index].indOffset;
}

// ATTENTION!!! Keep this method in sync with MsgMetadata::MakeOffsets()
void SQLDAMetadata::assign()
{
	if (offsets)
		return; // already assigned

	if (sqlda->sqld <= 0) // malformed sqlda?
		return;

	count = (USHORT) sqlda->sqld;
	speedHackEnabled = true; // May be we are lucky...
	ISC_SCHAR* const base = sqlda->sqlvar[0].sqldata;

	offsets = FB_NEW OffsetItem[count];
	for (unsigned i = 0; i < count; i++)
	{
		const XSQLVAR& var = sqlda->sqlvar[i];
		OffsetItem& it = offsets[i];

		it.type = var.sqltype;
		it.subType = var.sqlsubtype;
		it.length = var.sqllen;
		it.scale = var.sqlscale;
		switch (var.sqltype & ~1)
		{
		case SQL_TEXT:
		case SQL_VARYING:
			it.charSet = var.sqlsubtype;
			it.subType = 0;
			break;
		case SQL_BLOB:
			it.charSet = var.sqlscale;
			it.scale = 0;
			break;
		default:
			it.charSet = 0;
		}
		// No matter how good or bad is the way data is placed in message buffer, it cannot be changed
		// because changing of it on current codebase will completely kill remote module and may be the engine as well
		unsigned dtype;
		length = fb_utils::sqlTypeToDsc(length, var.sqltype, var.sqllen,
			&dtype, NULL /*length*/, &it.offset, &it.indOffset);
		if (it.offset != var.sqldata - base || it.indOffset != ((ISC_SCHAR*) (var.sqlind)) - base)
			speedHackEnabled = false; // No luck

		if (dtype < DTYPE_TYPE_MAX)
			alignment = MAX(alignment, type_alignments[dtype]);
	}
}

unsigned SQLDAMetadata::getMessageLength(CheckStatusWrapper* status)
{
	if (!offsets)
		assign();
	return length;
}

unsigned SQLDAMetadata::getAlignment(CheckStatusWrapper* status)
{
	if (!offsets)
		assign();
	return alignment;
}

unsigned SQLDAMetadata::getAlignedLength(CheckStatusWrapper* status)
{
	if (!offsets)
		assign();
	return FB_ALIGN(length, alignment);
}

void SQLDAMetadata::gatherData(DataBuffer& to)
{
	fb_assert(sqlda); // Ensure that data is gathered before take off because later they can be already changed
	if (sqlda->sqld <= 0)
	{
		to.clear();
		return;
	}

	const unsigned l = getMessageLength(NULL);
	if (speedHackEnabled)
	{
		to.assign(reinterpret_cast<UCHAR*>(sqlda->sqlvar[0].sqldata), l);
		return;
	}

	UCHAR* result = to.getBuffer(l, false);
	for (unsigned i = 0; i < count; i++)
	{
		SSHORT* nullInd = (SSHORT*)(result + offsets[i].indOffset);
		UCHAR* data = result + offsets[i].offset;

		const XSQLVAR& var = sqlda->sqlvar[i];
		unsigned len = var.sqllen;
		if ((var.sqltype & ~1) == SQL_VARYING)
			len += sizeof(USHORT);

		if (var.sqltype & 1)
		{
			// Make sure user has specified a location for null indicator.
			if (!var.sqlind)
			{
				(Arg::Gds(isc_dsql_sqlda_value_err) <<
				 Arg::Gds(isc_dsql_no_sqlind) <<
				 Arg::Gds(isc_dsql_sqlvar_index) << Arg::Num(i)
				).raise();
			}

			*nullInd = *var.sqlind;
		}
		else
			*nullInd = 0;

		// Make sure user has specified a data location (unless NULL).
		if (!var.sqldata && !*nullInd && (var.sqltype & ~1) != SQL_NULL)
		{
			(Arg::Gds(isc_dsql_sqlda_value_err) <<
				Arg::Gds(isc_dsql_no_sqldata) <<
				Arg::Gds(isc_dsql_sqlvar_index) << Arg::Num(i)
			).raise();
		}

		// This is assertion instead of error check because such situation is next to impossible: buffer size is calculated a routine above
		fb_assert(data + len < result + length);

		// Copy data - unless known to be NULL.
		if (!*nullInd)
			memcpy(data, var.sqldata, len);
	}
}

void SQLDAMetadata::scatterData(DataBuffer& from)
{
	fb_assert(sqlda);
	///fb_assert(offsets); // Not reliable, but still check that input buffer can come from this metadata

	if (speedHackEnabled)
		return; // Data is already in user's buffer

	fb_assert(length == from.getCount());

	for (unsigned i = 0; i < count; i++)
	{
		const XSQLVAR& var = sqlda->sqlvar[i];

		if ((var.sqltype & ~1) != SQL_NULL)
		{
			unsigned len = var.sqllen;

			// Make sure user has specified a data location.
			if (!var.sqldata)
			{
				(Arg::Gds(isc_dsql_sqlda_value_err) <<
					Arg::Gds(isc_dsql_no_sqldata) <<
					Arg::Gds(isc_dsql_sqlvar_index) << Arg::Num(i)
				).raise();
			}

			if ((var.sqltype & ~1) == SQL_VARYING)
				len += sizeof(SSHORT);

			memcpy(var.sqldata, from.begin() + offsets[i].offset, len);
		}

		if (var.sqltype & 1)
		{
			// Make sure user has specified a location for null indicator.
			if (!var.sqlind)
			{
				(Arg::Gds(isc_dsql_sqlda_value_err) <<
					Arg::Gds(isc_dsql_no_sqlind) <<
					Arg::Gds(isc_dsql_sqlvar_index) << Arg::Num(i)
				).raise();
			}

			*var.sqlind = *reinterpret_cast<ISC_SHORT*>(from.begin() + offsets[i].indOffset);
		}
	}

}

UCHAR* SQLDAMetadataLauncher::getBuffer()
{
	if (metadata)
	{
		// ASF: It's important to call getMessageLength before check speedHackEnabled.
		unsigned length = metadata->getMessageLength(NULL);

		if (metadata->speedHackEnabled)
			return reinterpret_cast<UCHAR*>(metadata->sqlda->sqlvar[0].sqldata);

		return buffer.getBuffer(length, false);
	}
	else
	{
		buffer.clear();
		return NULL;
	}
}

IMetadataBuilder* SQLDAMetadata::getBuilder(CheckStatusWrapper* status)
{
	// no way to construct SQLDA
	fb_assert(false);
	return NULL;
}

int SQLDAMetadata::detach()
{
	// This code can lead to unnecessary work but not to segfault
	if (refCounter.value() > 1)
	{
		if (!offsets) // in current codebase this is never true because offsets are allocated on buffer size calculation
			assign();
	}
	sqlda = NULL;
	speedHackEnabled = false;

	return release();
}


GlobalPtr<RWLock> handleMappingLock;
GlobalPtr<GenericMap<Pair<NonPooled<isc_svc_handle, YService*> > > > services;
GlobalPtr<GenericMap<Pair<NonPooled<isc_db_handle, YAttachment*> > > > attachments;
GlobalPtr<GenericMap<Pair<NonPooled<FB_API_HANDLE, YEvents*> > > > events;
GlobalPtr<GenericMap<Pair<NonPooled<isc_tr_handle, YTransaction*> > > > transactions;
GlobalPtr<GenericMap<Pair<NonPooled<isc_stmt_handle, IscStatement*> > > > statements;
GlobalPtr<GenericMap<Pair<NonPooled<isc_req_handle, YRequest*> > > > requests;
GlobalPtr<GenericMap<Pair<NonPooled<isc_blob_handle, YBlob*> > > > blobs;

bool shutdownStarted = false;


//-------------------------------------


// CVC: I'm following types_pub.h here. If there's a better solution, commit it, please.
#if defined(_LP64) || defined(__LP64__) || defined(__arch64__) || defined(_WIN64)
inline FB_API_HANDLE ULONG_TO_FB_API_HANDLE(ULONG h) { return static_cast<FB_API_HANDLE>(h); }
inline ULONG FB_API_HANDLE_TO_ULONG(FB_API_HANDLE h) { return h; }
#else
inline FB_API_HANDLE ULONG_TO_FB_API_HANDLE(ULONG h) { return reinterpret_cast<FB_API_HANDLE>(h); }
inline ULONG FB_API_HANDLE_TO_ULONG(FB_API_HANDLE h) { return reinterpret_cast<ULONG>(h); }
#endif


template <typename T>
void makeHandle(GenericMap<Pair<NonPooled<FB_API_HANDLE, T*> > >* map, T* obj, FB_API_HANDLE& h)
{
	static ULONG sequenceNumber = 0;

	WriteLockGuard sync(handleMappingLock, FB_FUNCTION);
	if (h)		// Avoid 2 different handles for same object
		return;

	ULONG handle = 0;

	// Loop until we find an empty handle slot. This is to care of case when counter rolls over.
	do
	{
		// Generate handle number using rolling counter.
		// This way we tend to give out unique handle numbers and closed
		// handles do not appear as valid to our clients.
		handle = ++sequenceNumber;

		// Avoid generating NULL handle when sequence number wraps
		if (!handle)
			handle = ++sequenceNumber;
	} while (map->put(ULONG_TO_FB_API_HANDLE(handle), obj));

	h = ULONG_TO_FB_API_HANDLE(handle);
}

template <typename T>
void removeHandle(GenericMap<Pair<NonPooled<FB_API_HANDLE, T*> > >* map, FB_API_HANDLE& handle)
{
	if (handle)
	{
		WriteLockGuard sync(handleMappingLock, FB_FUNCTION);
		bool removed = map->remove(handle);

		fb_assert(removed);
		(void) removed;	// avoid warning in prod build
		handle = 0;
	}
}

template <typename T>
RefPtr<T> translateHandle(GlobalPtr<GenericMap<Pair<NonPooled<FB_API_HANDLE, T*> > > >& map,
	FB_API_HANDLE* handle)
{
	if (!handle)
		status_exception::raise(Arg::Gds(T::ERROR_CODE));

	ReadLockGuard sync(handleMappingLock, FB_FUNCTION);

	T** obj = map->get(*handle);

	if (!obj)
		status_exception::raise(Arg::Gds(T::ERROR_CODE));

	return RefPtr<T>(*obj);
}

//-------------------------------------

const int SHUTDOWN_TIMEOUT = 10000;	// 10 sec

class ShutdownInit
{
public:
	explicit ShutdownInit(MemoryPool&)
	{
		InstanceControl::registerShutdown(atExitShutdown);
	}

private:
	static void atExitShutdown()
	{
		if (MasterInterfacePtr()->getProcessExiting())
			InstanceControl::cancelCleanup();
		else
			fb_shutdown(SHUTDOWN_TIMEOUT, fb_shutrsn_exit_called);
	}
};

#ifdef UNIX
	int killed;
	bool procInt, procTerm;
	SignalSafeSemaphore* shutdownSemaphore = NULL;

	THREAD_ENTRY_DECLARE shutdownThread(THREAD_ENTRY_PARAM)
	{
		for (;;)
		{
			killed = 0;
			try
			{
				if (shutdownSemaphore)
					shutdownSemaphore->enter();
			}
			catch (status_exception& e)
			{
				TEXT buffer[BUFFER_LARGE];
				const ISC_STATUS* vector = e.value();

				if (! (vector && fb_interpret(buffer, sizeof(buffer), &vector)))
					strcpy(buffer, "Unknown failure in shutdown thread in shutdownSemaphore->enter()");

				fb_utils::logAndDie(buffer);
			}

			if (!killed)
				break;

			// perform shutdown
			if (fb_shutdown(SHUTDOWN_TIMEOUT, fb_shutrsn_signal) == FB_SUCCESS)
			{
				InstanceControl::registerShutdown(0);
				break;	//exit(0);
			}
		}

		return 0;
	}

	void handler(int sig)
	{
		if (!killed && shutdownSemaphore)
		{
			killed = sig;
			shutdownSemaphore->release();
		}
	}

	void handlerInt(void*)
	{
		handler(SIGINT);
	}

	void handlerTerm(void*)
	{
		handler(SIGTERM);
	}

	class CtrlCHandler : public ShutdownInit
	{
	public:
		SignalSafeSemaphore semaphore;

		explicit CtrlCHandler(MemoryPool& p)
			: ShutdownInit(p)
		{
			Thread::start(shutdownThread, 0, 0, &handle);

			procInt = ISC_signal(SIGINT, handlerInt, 0);
			procTerm = ISC_signal(SIGTERM, handlerTerm, 0);
			shutdownSemaphore = &semaphore;
		}

		~CtrlCHandler()
		{
			ISC_signal_cancel(SIGINT, handlerInt, 0);
			ISC_signal_cancel(SIGTERM, handlerTerm, 0);

			if (!killed)
			{
				// Must be done to let shutdownThread close.
				shutdownSemaphore->release();
				shutdownSemaphore = NULL;
				Thread::waitForCompletion(handle);
			}
		}
	private:
		Thread::Handle handle;
	};
#endif // UNIX

	void signalInit()
	{
#ifdef UNIX
		static GlobalPtr<CtrlCHandler> ctrlCHandler;
#else
		static GlobalPtr<ShutdownInit> shutdownInit;
#endif // UNIX
	}

} // anonymous namespace


//-------------------------------------

namespace Why
{
	// StatusVector:	Provides correct status vector for operation and init() it.
	class StatusVector : public AutoIface<BaseStatus<StatusVector> >
	{
	public:
		explicit StatusVector(ISC_STATUS* v = NULL) throw()
			: localVector(v ? v : localStatus)
		{ }

		operator const ISC_STATUS*()
		{
			merge();
			return localVector;
		}

		~StatusVector()
		{
			merge();
#ifdef DEV_BUILD
			checkStatusVector(localVector);
#endif
		}

		// IStatus implementation
		void dispose()
		{ }

#ifdef DEV_BUILD
		// Validate that a status vector looks valid.
		static void checkStatusVector(const ISC_STATUS* status)
		{
#define SV_MSG(x)	\
	do {	\
		gds__log ("%s %d check_status_vector: %s", __FILE__, __LINE__, (x)); BREAKPOINT (__LINE__);	\
	} while (false)

			const ISC_STATUS* s = status;
			if (!s)
			{
				SV_MSG("Invalid status vector");
				return;
			}

			if (*s != isc_arg_gds)
			{
				SV_MSG("Must start with isc_arg_gds");
				return;
			}

			// Vector [2] could either end the vector, or start a warning
			// in either case the status vector is a success.
			if (s[1] == FB_SUCCESS && s[2] != isc_arg_end && s[2] != isc_arg_gds &&
				s[2] != isc_arg_warning)
			{
				SV_MSG("Bad success vector format");
			}

			ULONG length;

			while (*s != isc_arg_end)
			{
				const ISC_STATUS code = *s++;

				switch (code)
				{
					case isc_arg_warning:
					case isc_arg_gds:
						// The next element must either be 0 (indicating no error) or a
						// valid isc error message, let's check.
						if (*s && (*s & ISC_MASK) != ISC_MASK)
						{
							if (code == isc_arg_warning)
								SV_MSG("warning code not a valid ISC message");
							else
								SV_MSG("error code not a valid ISC message");
						}

						// If the error code is valid, then I better be able to retrieve a
						// proper facility code from it ... let's find out.
						if (*s && (*s & ISC_MASK) == ISC_MASK)
						{
							bool found = false;

							const auto* facs = facilities;
							const int fac_code = GET_FACILITY(*s);

							while (facs->facility)
							{
								if (facs->fac_code == fac_code)
								{
									found = true;
									break;
								}

								++facs;
							}

							if (!found)
							{
								if (code == isc_arg_warning)
									SV_MSG("warning code does not contain a valid facility");
								else
									SV_MSG("error code does not contain a valid facility");
							}
						}

						++s;
						break;

					case isc_arg_interpreted:
					case isc_arg_string:
					case isc_arg_sql_state:
						length = static_cast<ULONG>(strlen((const char*) *s));
						// This check is heuristic, not deterministic.
						if (length > 1024 - 1)
							SV_MSG("suspect length value");
						if (*((const UCHAR*) *s) == 0xCB)
							SV_MSG("string in freed memory");
						++s;
						break;

					case isc_arg_cstring:
						length = (ULONG) *s;
						++s;
						// This check is heuristic, not deterministic.
						// Note: This can never happen anyway, as the length is coming from a byte value.
						if (length > 1024 - 1)
							SV_MSG("suspect length value");
						if (*((const UCHAR*) *s) == 0xCB)
							SV_MSG("string in freed memory");
						++s;
						break;

					case isc_arg_number:
					case isc_arg_vms:
					case isc_arg_unix:
					case isc_arg_win32:
						++s;
						break;

					default:
						SV_MSG("invalid status code");
						return;
				}

				if (s - status >= ISC_STATUS_LENGTH)
					SV_MSG("vector too long");
			}

#undef SV_MSG
		}
#endif

	private:
		void merge()
		{
			fb_utils::mergeStatus(localVector, FB_NELEM(localStatus), this);
			makePermanentVector(localVector);
		}

		ISC_STATUS_ARRAY localStatus;
		ISC_STATUS* localVector;
	};

	class ShutChain : public GlobalStorage
	{
	private:
		ShutChain(ShutChain* link, FB_SHUTDOWN_CALLBACK cb, const int m, void* a)
			: next(link),
			  callBack(cb),
			  mask(m),
			  arg(a)
		{
		}

	private:
		static ShutChain* list;
		static GlobalPtr<Mutex> shutdownCallbackMutex;

		ShutChain* next;
		FB_SHUTDOWN_CALLBACK callBack;
		int mask;
		void* arg;

	public:
		static void add(FB_SHUTDOWN_CALLBACK cb, const int m, void* a)
		{
			MutexLockGuard guard(shutdownCallbackMutex, FB_FUNCTION);

			for (ShutChain* chain = list; chain; chain = chain->next)
			{
				if (chain->callBack == cb && chain->arg == a)
				{
					if (m)
						chain->mask |= m;
					else
						chain->mask = 0;

					return;
				}
			}

			list = FB_NEW ShutChain(list, cb, m, a);
		}

		static int run(const int m, const int reason)
		{
			int rc = FB_SUCCESS;
			MutexLockGuard guard(shutdownCallbackMutex, FB_FUNCTION);

			for (ShutChain* chain = list; chain; chain = chain->next)
			{
				if ((chain->mask & m) && (chain->callBack(reason, m, chain->arg) != FB_SUCCESS))
					rc = FB_FAILURE;
			}

			return rc;
		}
	};

	ShutChain* ShutChain::list = NULL;
	GlobalPtr<Mutex> ShutChain::shutdownCallbackMutex;

	template <typename T, typename CleanupRoutine>	// T = YAttachment or YTransaction
	class CleanupCallbackImpl : public CleanupCallback
	{
	public:
		CleanupCallbackImpl(T* aObject, CleanupRoutine* aRoutine, void* aArg)
			: object(aObject),
			  routine(aRoutine),
			  arg(aArg)
		{
		}

		virtual void cleanupCallbackFunction()
		{
			call(routine);
			delete this;
		}

	private:
		void call(AttachmentCleanupRoutine* routine)
		{
			routine(&object->getHandle(), arg);
		}

		void call(TransactionCleanupRoutine* routine)
		{
			routine(object->getHandle(), arg);
		}

	private:
		T* object;
		CleanupRoutine* routine;
		void* arg;
	};

	static AtomicCounter dispCounter;

	enum CHECK_ENTRY { CHECK_NONE, CHECK_ALL, CHECK_WARN_ZERO_HANDLE };

	template <typename Y>
	class YEntry : public FpeControl	//// TODO: move FpeControl to the engine
	{
	public:
		YEntry(CheckStatusWrapper* aStatus, Y* object, CHECK_ENTRY checkAttachment = CHECK_ALL)
			: ref(object->attachment.get()), nextRef(NULL)
		{
			aStatus->init();
			init(object->next);

			if (checkAttachment != CHECK_NONE && !(nextRef.hasData()))
				nextIsEmpty(aStatus, checkAttachment);

			if (checkAttachment != CHECK_NONE && ref && ref->savedStatus.getError())
			{
				const IStatus* err = ref->savedStatus.value();
				fini();
				status_exception::raise(err);
			}
		}

		~YEntry()
		{
			fini();
		}

		void init(typename Y::NextInterface* nxt)
		{
			signalInit();

			if (ref)
			{
				MutexLockGuard guard(ref->enterMutex, FB_FUNCTION);
				++ref->enterCount;
				nextRef = nxt;
			}
			else
			{
				++dispCounter;
				nextRef = nxt;
			}

			if (shutdownStarted)
			{
				fini();
				Arg::Gds(isc_att_shutdown).raise();
			}
		}

		void fini()
		{
			if (ref)
			{
				MutexLockGuard guard(ref->enterMutex, FB_FUNCTION);
				nextRef = NULL;
				--ref->enterCount;
			}
			else
			{
				nextRef = NULL;
				--dispCounter;
			}
		}

		typename Y::NextInterface* next()
		{
			return nextRef;
		}

	private:
		YEntry(const YEntry&);	// prohibit copy constructor

		void nextIsEmpty(CheckStatusWrapper* aStatus, CHECK_ENTRY check)
		{
			if (check == CHECK_WARN_ZERO_HANDLE)
				Arg::Warning(Y::ERROR_CODE).appendTo(aStatus);
			else
			{
				fini();
				Arg::Gds(Y::ERROR_CODE).raise();
			}
		}

	private:
		RefPtr<typename Y::YRef> ref;
		RefPtr<typename Y::NextInterface> nextRef;
	};

	class IscStatement : public RefCounted, public GlobalStorage, public YObject
	{
	public:
		static const ISC_STATUS ERROR_CODE = isc_bad_stmt_handle;

		explicit IscStatement(YAttachment* aAttachment)
			: attachment(aAttachment),
			  cursorName(getPool()),
			  statement(NULL),
			  userHandle(NULL),
			  pseudoOpened(false),
			  delayedFormat(false)
		{ }

		~IscStatement() override;

		isc_stmt_handle& getHandle();
		void destroy(unsigned);

		void openCursor(CheckStatusWrapper* status, isc_tr_handle* traHandle,
			IMessageMetadata* inMetadata, UCHAR* buffer, IMessageMetadata* outMetadata);

		void closeCursor(CheckStatusWrapper* status, bool raise);
		void closeStatement(CheckStatusWrapper* status);

		void execute(CheckStatusWrapper* status, isc_tr_handle* traHandle,
			IMessageMetadata* inMetadata, UCHAR* inBuffer, IMessageMetadata* outMetadata, UCHAR* outBuffer);

		FB_BOOLEAN fetch(CheckStatusWrapper* status, IMessageMetadata* outMetadata, UCHAR* outBuffer);

		void checkPrepared(ISC_STATUS code = isc_unprepared_stmt) const
		{
			if (!statement)
				Arg::Gds(code).raise();
		}

		void checkCursorOpened() const
		{
			if (!statement || !statement->cursor)
				(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
					Arg::Gds(isc_dsql_cursor_err) <<
					Arg::Gds(isc_dsql_cursor_not_open)).raise();
		}

		void checkCursorClosed() const
		{
			if (statement && statement->cursor)
				(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
					Arg::Gds(isc_dsql_cursor_open_err)).raise();
		}

		IStatement* getInterface()
		{
			fb_assert(statement);
			statement->addRef();
			return statement;
		}

		AtomicAttPtr attachment;
		string cursorName;
		YStatement* statement;
		isc_stmt_handle* userHandle;
		bool pseudoOpened, delayedFormat;
	};

	template <>
	YEntry<YAttachment>::YEntry(CheckStatusWrapper* aStatus, YAttachment* aAttachment, CHECK_ENTRY checkAttachment)
		: ref(aAttachment), nextRef(NULL)
	{
		aStatus->init();
		init(aAttachment->next);

		if (checkAttachment != CHECK_NONE && !(nextRef.hasData()))
			nextIsEmpty(aStatus, checkAttachment);

		if (checkAttachment != CHECK_NONE && aAttachment->savedStatus.getError())
		{
			fini();
			status_exception::raise(aAttachment->savedStatus.value());
		}
	}

	template <>
	YEntry<YService>::YEntry(CheckStatusWrapper* aStatus, YService* aService, CHECK_ENTRY checkService)
		: ref(aService), nextRef(NULL)
	{
		aStatus->init();
		init(aService->next);

		if (checkService != CHECK_NONE && !nextRef.hasData())
			nextIsEmpty(aStatus, checkService);
	}

	class DispatcherEntry : public FpeControl	//// TODO: move FpeControl to the engine
	{
	public:
		explicit DispatcherEntry(CheckStatusWrapper* aStatus, bool p_shutdownMode = false)
			: shutdownMode(p_shutdownMode)
		{
			aStatus->init();
			signalInit();

			if (!shutdownMode)
			{
				++dispCounter;
				if (shutdownStarted)
				{
					--dispCounter;
					Arg::Gds(isc_att_shutdown).raise();
				}
			}
		}

		~DispatcherEntry()
		{
			if (!shutdownMode)
			{
				--dispCounter;
			}
		}

	private:
		DispatcherEntry(const DispatcherEntry&);	// prohibit copy constructor
		bool shutdownMode;
	};


	template <class Y>
	void done(CheckStatusWrapper* status, YEntry<Y>& entry, Y* y, std::function<void()> newClose, std::function<void()> oldClose)
	{
		if (entry.next())
			newClose();

		if (!(status->getState() & IStatus::STATE_ERRORS))
			y->destroy(Y::DF_RELEASE | Y::DF_KEEP_NEXT);

		else if (status->getErrors()[1] == isc_interface_version_too_old)
		{
			status->init();
			if (entry.next())
				oldClose();

			if (!(status->getState() & IStatus::STATE_ERRORS))
				y->destroy(Y::DF_RELEASE);
		}
	}

}	// namespace Why

struct TEB
{
	isc_db_handle* teb_database;
	int teb_tpb_length;
	const UCHAR* teb_tpb;
};


//-------------------------------------


static void badHandle(ISC_STATUS code)
{
	status_exception::raise(Arg::Gds(code));
}

static bool isNetworkError(const IStatus* status)
{
	ISC_STATUS code = status->getErrors()[1];
	return fb_utils::isNetworkError(code);
}

static void nullCheck(const FB_API_HANDLE* ptr, ISC_STATUS code)
{
	// This function is called for incoming API handles, proposed to be created by the call.
	if (!ptr || *ptr)
		badHandle(code);
}

static void badSqldaVersion(const short version)
{
	(Arg::Gds(isc_dsql_sqlda_value_err) <<
		Arg::Gds(isc_dsql_invalid_sqlda_version) <<
		Arg::Num(SQLDA_VERSION1) << Arg::Num(SQLDA_VERSION1) << Arg::Num(version)
	).raise();
}

// Set charset info in SQLVAR according to legacy rules
static void setTextType(XSQLVAR* var, unsigned charSet)
{
	switch(var->sqltype & ~1)
	{
	case SQL_VARYING:
	case SQL_TEXT:
		var->sqlsubtype = charSet;
		break;
	case SQL_BLOB:
		var->sqlscale = charSet;
		break;
	}
}

static int sqldaTruncateString(char* buffer, FB_SIZE_T size, const char* s)
{
	int ret = fb_utils::snprintf(buffer, size, "%s", s);
	return MIN(ret, size - 1);
}

// Describe parameters metadata in an sqlda.
static void sqldaDescribeParameters(XSQLDA* sqlda, IMessageMetadata* parameters)
{
	if (!sqlda)
		return;

	if (sqlda->version != SQLDA_VERSION1)
		badSqldaVersion(sqlda->version);

	StatusVector status(NULL);
	CheckStatusWrapper statusWrapper(&status);

	unsigned parametersCount = parameters->getCount(&statusWrapper);
	status.check();
	sqlda->sqld = (USHORT) parametersCount;

	// If necessary, inform the application that more sqlda items are needed.
	if (sqlda->sqld > sqlda->sqln)
		return;

	for (unsigned i = 0; i < parametersCount; ++i)
	{
		XSQLVAR* var = &sqlda->sqlvar[i];
		const char* s;

		var->sqltype = parameters->getType(&statusWrapper, i);
		status.check();

		var->sqltype |= (parameters->isNullable(&statusWrapper, i) ? 1 : 0);
		status.check();

		var->sqlsubtype = parameters->getSubType(&statusWrapper, i);
		status.check();

		var->sqllen = parameters->getLength(&statusWrapper, i);
		status.check();

		var->sqlscale = parameters->getScale(&statusWrapper, i);
		status.check();

		unsigned charSet = parameters->getCharSet(&statusWrapper, i);
		status.check();
		setTextType(var, charSet);

		s = parameters->getField(&statusWrapper, i);
		status.check();
		var->sqlname_length = sqldaTruncateString(var->sqlname, sizeof(var->sqlname), s);

		s = parameters->getRelation(&statusWrapper, i);
		status.check();
		var->relname_length = sqldaTruncateString(var->relname, sizeof(var->relname), s);

		s = parameters->getOwner(&statusWrapper, i);
		status.check();
		var->ownname_length = sqldaTruncateString(var->ownname, sizeof(var->ownname), s);

		s = parameters->getAlias(&statusWrapper, i);
		status.check();
		var->aliasname_length = sqldaTruncateString(var->aliasname, sizeof(var->aliasname), s);
	}
}

// Open or create an existing blob (extended edition).
static ISC_STATUS openOrCreateBlob(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, isc_blob_handle* blobHandle, ISC_QUAD* blobId,
	USHORT bpbLength, const UCHAR* bpb, bool createFlag)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		nullCheck(blobHandle, isc_bad_segstr_handle);

		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		YBlob* blob = createFlag ?
			attachment->createBlob(&statusWrapper, transaction, blobId, bpbLength, bpb) :
			attachment->openBlob(&statusWrapper, transaction, blobId, bpbLength, bpb);

		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		*blobHandle = blob->getHandle();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


//-------------------------------------


static TLS_DECLARE(ICryptKeyCallback*, legacyCryptCallback);

ISC_STATUS API_ROUTINE fb_database_crypt_callback(ISC_STATUS* userStatus, void* cb)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		TLS_SET(legacyCryptCallback, (ICryptKeyCallback*) cb);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


//-------------------------------------


Firebird::IAttachment* handleToIAttachment(CheckStatusWrapper* status, isc_db_handle* handle)
{
	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, handle));
		attachment->addRef();
		return attachment;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}


Firebird::ITransaction* handleToITransaction(CheckStatusWrapper* status, isc_tr_handle* handle)
{
	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, handle));
		transaction->addRef();
		return transaction;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}


//-------------------------------------


// Attach a database through the first subsystem that recognizes it.
ISC_STATUS API_ROUTINE isc_attach_database(ISC_STATUS* userStatus, SSHORT fileLength,
	const TEXT* filename, isc_db_handle* publicHandle, SSHORT dpbLength, const SCHAR* dpb)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		nullCheck(publicHandle, isc_bad_db_handle);

		if (!filename)
			status_exception::raise(Arg::Gds(isc_bad_db_format) << Arg::Str(""));

		PathName pathName(filename, fileLength ? fileLength : fb_strlen(filename));

		RefPtr<Dispatcher> dispatcher(FB_NEW Dispatcher);

		dispatcher->setDbCryptCallback(&statusWrapper, TLS_GET(legacyCryptCallback));
		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		YAttachment* attachment = dispatcher->attachDatabase(&statusWrapper, pathName.c_str(),
			dpbLength, reinterpret_cast<const UCHAR*>(dpb));
		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		*publicHandle = attachment->getHandle();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Provide information on blob object.
ISC_STATUS API_ROUTINE isc_blob_info(ISC_STATUS* userStatus, isc_blob_handle* blobHandle,
	SSHORT itemLength, const SCHAR* items, SSHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YBlob> blob(translateHandle(blobs, blobHandle));

		blob->getInfo(&statusWrapper, itemLength, reinterpret_cast<const UCHAR*>(items),
			bufferLength, reinterpret_cast<UCHAR*>(buffer));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Abort a partially completed blob.
ISC_STATUS API_ROUTINE isc_cancel_blob(ISC_STATUS* userStatus, isc_blob_handle* blobHandle)
{
	if (!*blobHandle)
	{
		if (userStatus)
			fb_utils::init_status(userStatus);

		return FB_SUCCESS;
	}

	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YBlob> blob(translateHandle(blobs, blobHandle));

		blob->cancel(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*blobHandle = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Try to cancel an event.
ISC_STATUS API_ROUTINE isc_cancel_events(ISC_STATUS* userStatus, isc_db_handle* handle, SLONG* id)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, handle));
		RefPtr<YEvents> event;
		try
		{
			// This cast appears awful, but as long as handles are 32-bit entities it's OK.
			event = translateHandle(events, (FB_API_HANDLE*) id);
		}
		catch (const status_exception& ex)
		{
			if (ex.value()[1] == isc_bad_events_handle)
			{
				// Ignore invalid handle/ID in cancelation call for backward compatibility
				return status[1];
			}

			throw;
		}

		if (event->attachment.get() != attachment)
			Arg::Gds(isc_bad_events_handle).raise();

		event->cancel(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*id = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Try to cancel an operation.
ISC_STATUS API_ROUTINE fb_cancel_operation(ISC_STATUS* userStatus, isc_db_handle* handle,
	USHORT option)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, handle));
		attachment->cancelOperation(&statusWrapper, option);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Close a blob opened either for reading or writing (creation).
ISC_STATUS API_ROUTINE isc_close_blob(ISC_STATUS* userStatus, isc_blob_handle* blobHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YBlob> blob(translateHandle(blobs, blobHandle));

		blob->close(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*blobHandle = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Commit a transaction.
ISC_STATUS API_ROUTINE isc_commit_transaction(ISC_STATUS* userStatus, isc_tr_handle* traHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));
		transaction->commit(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*traHandle = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Do a commit retaining.
// N.B., the transaction cleanup handlers are NOT called here since, conceptually, the transaction
// is still running.
ISC_STATUS API_ROUTINE isc_commit_retaining(ISC_STATUS* userStatus, isc_tr_handle* traHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));
		transaction->commitRetaining(&statusWrapper);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


ISC_STATUS API_ROUTINE isc_compile_request(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_req_handle* reqHandle, ISC_USHORT blrLength, const ISC_SCHAR* blr)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);
	YRequest* request = NULL;

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));

		nullCheck(reqHandle, isc_bad_stmt_handle);

		request = attachment->compileRequest(&statusWrapper, blrLength,
			reinterpret_cast<const UCHAR*>(blr));

		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		*reqHandle = request->getHandle();
	}
	catch (const Exception& e)
	{
		if (request)
		{
			*reqHandle = 0;
			StatusVector temp(NULL);
			CheckStatusWrapper tempCheckStatusWrapper(&temp);
			request->free(&tempCheckStatusWrapper);
		}

		e.stuffException(&statusWrapper);
	}

	return status[1];
}


ISC_STATUS API_ROUTINE isc_compile_request2(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_req_handle* reqHandle, USHORT blrLength, const SCHAR* blr)
{
	ISC_STATUS rc = isc_compile_request(userStatus, dbHandle, reqHandle, blrLength, blr);
	if (rc)
		return rc;

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));
		request->userHandle = reqHandle;

		return 0;
	}
	catch (const Exception& e)
	{
		ISC_STATUS_ARRAY temp;
		isc_release_request(temp, reqHandle);

		StatusVector status(userStatus);
		CheckStatusWrapper statusWrapper(&status);

		e.stuffException(&statusWrapper);
		return status[1];
	}
}


// Open an existing blob.
ISC_STATUS API_ROUTINE isc_create_blob(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, isc_blob_handle* blobHandle, ISC_QUAD* blobId)
{
	return openOrCreateBlob(userStatus, dbHandle, traHandle, blobHandle, blobId, 0, NULL, true);
}


// Create a new blob.
ISC_STATUS API_ROUTINE isc_create_blob2(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, isc_blob_handle* blobHandle, ISC_QUAD* blobId,
	SSHORT bpbLength, const SCHAR* bpb)
{
	return openOrCreateBlob(userStatus, dbHandle, traHandle, blobHandle, blobId,
		bpbLength, reinterpret_cast<const UCHAR*>(bpb), true);
}


// Create a nice, squeaky clean database, uncorrupted by user data.
ISC_STATUS API_ROUTINE isc_create_database(ISC_STATUS* userStatus, USHORT fileLength,
	const TEXT* filename, isc_db_handle* publicHandle, USHORT dpbLength, const SCHAR* dpb,
	USHORT /*db_type*/)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		nullCheck(publicHandle, isc_bad_db_handle);

		if (!filename)
			status_exception::raise(Arg::Gds(isc_bad_db_format) << Arg::Str(""));

		PathName pathName(filename, fileLength ? fileLength : fb_strlen(filename));

		RefPtr<Dispatcher> dispatcher(FB_NEW Dispatcher);

		dispatcher->setDbCryptCallback(&statusWrapper, TLS_GET(legacyCryptCallback));
		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		ClumpletWriter newDpb(ClumpletReader::dpbList, MAX_DPB_SIZE, reinterpret_cast<const UCHAR*>(dpb), dpbLength);
		if (!newDpb.find(isc_dpb_sql_dialect))
		{
			newDpb.insertInt(isc_dpb_sql_dialect, 1);	// legacy behavior in legacy interface
			dpb = reinterpret_cast<const SCHAR*>(newDpb.getBuffer());
			dpbLength = newDpb.getBufferLength();
		}

		YAttachment* attachment = dispatcher->createDatabase(&statusWrapper, pathName.c_str(),
			dpbLength, reinterpret_cast<const UCHAR*>(dpb));
		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		*publicHandle = attachment->getHandle();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Register an attachment specific cleanup handler.
ISC_STATUS API_ROUTINE isc_database_cleanup(ISC_STATUS* userStatus, isc_db_handle* handle,
	AttachmentCleanupRoutine* routine, void* arg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, handle));

		AutoPtr<CleanupCallbackImpl<YAttachment, AttachmentCleanupRoutine> > callback(
			FB_NEW CleanupCallbackImpl<YAttachment, AttachmentCleanupRoutine>(attachment, routine, arg));

		attachment->addCleanupHandler(&statusWrapper, callback);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			callback.release();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Provide information on database object.
ISC_STATUS API_ROUTINE isc_database_info(ISC_STATUS* userStatus, isc_db_handle* handle,
	SSHORT itemLength, const SCHAR* items, SSHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, handle));
		attachment->getInfo(&statusWrapper, itemLength, reinterpret_cast<const UCHAR*>(items),
			bufferLength, reinterpret_cast<UCHAR*>(buffer));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Do meta-data update.
ISC_STATUS API_ROUTINE isc_ddl(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, SSHORT length, const SCHAR* dyn)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		attachment->executeDyn(&statusWrapper, transaction, length, reinterpret_cast<const UCHAR*>(dyn));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Close down an attachment.
ISC_STATUS API_ROUTINE isc_detach_database(ISC_STATUS* userStatus, isc_db_handle* handle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, handle));
		attachment->detach(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*handle = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Disable access to a specific subsystem. If no subsystem has been explicitly disabled, all are
// available.
int API_ROUTINE gds__disable_subsystem(TEXT* /*subsystem*/)
{
	return FALSE;
}


// Close down a database and then purge it.
ISC_STATUS API_ROUTINE isc_drop_database(ISC_STATUS* userStatus, isc_db_handle* handle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, handle));
		attachment->dropDatabase(&statusWrapper);

		if ((!(status.getState() & IStatus::STATE_ERRORS)) ||
			status[1] == isc_drdb_completed_with_errs)
		{
			*handle = 0;
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


ISC_STATUS API_ROUTINE isc_dsql_alloc_statement(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_stmt_handle* stmtHandle)
{
	return isc_dsql_allocate_statement(userStatus, dbHandle, stmtHandle);
}


// Allocate a statement handle.
ISC_STATUS API_ROUTINE isc_dsql_alloc_statement2(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_stmt_handle* stmtHandle)
{
	ISC_STATUS rc = isc_dsql_alloc_statement(userStatus, dbHandle, stmtHandle);
	if (rc)
		return rc;

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));
		statement->userHandle = stmtHandle;

		return 0;
	}
	catch (const Exception& e)
	{
		ISC_STATUS_ARRAY temp;
		isc_dsql_free_statement(temp, stmtHandle, DSQL_drop);

		StatusVector status(userStatus);
		CheckStatusWrapper statusWrapper(&status);

		e.stuffException(&statusWrapper);
		return status[1];
	}
}


// Allocate a statement handle.
ISC_STATUS API_ROUTINE isc_dsql_allocate_statement(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_stmt_handle* stmtHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);
	IscStatement* statement = NULL;

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));

		nullCheck(stmtHandle, isc_bad_stmt_handle);

		statement = FB_NEW IscStatement(attachment);
		statement->addRef();
		attachment->childIscStatements.add(statement);
		*stmtHandle = statement->getHandle();
	}
	catch (const Exception& e)
	{
		if (statement)
		{
			*stmtHandle = 0;
			statement->release();
		}
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Describe output parameters (columns) for a prepared statement.
ISC_STATUS API_ROUTINE isc_dsql_describe(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	USHORT /*dialect*/, XSQLDA* sqlda)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		statement->checkPrepared(isc_info_unprepared_stmt);

		RefPtr<IMessageMetadata> columns(REF_NO_INCR,
			statement->statement->next->getOutputMetadata(&statusWrapper));
		status.check();

		sqldaDescribeParameters(sqlda, columns);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Describe input parameters for a prepared statement.
ISC_STATUS API_ROUTINE isc_dsql_describe_bind(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	USHORT /*dialect*/, XSQLDA* sqlda)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		statement->checkPrepared(isc_info_unprepared_stmt);

		RefPtr<IMessageMetadata> parameters(REF_NO_INCR,
			statement->statement->next->getInputMetadata(&statusWrapper));
		status.check();

		sqldaDescribeParameters(sqlda, parameters);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Execute a non-SELECT dynamic SQL statement.
ISC_STATUS API_ROUTINE isc_dsql_execute(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	isc_stmt_handle* stmtHandle, USHORT dialect, const XSQLDA* sqlda)
{
	return isc_dsql_execute2(userStatus, traHandle, stmtHandle, dialect, sqlda, NULL);
}


// Execute a non-SELECT dynamic SQL statement.
ISC_STATUS API_ROUTINE isc_dsql_execute2(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	isc_stmt_handle* stmtHandle, USHORT dialect, const XSQLDA* inSqlda, const XSQLDA* outSqlda)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		statement->checkPrepared();

		const unsigned flags = statement->statement->getFlags(&statusWrapper);
		if (status.getState() & IStatus::STATE_ERRORS)
		{
			return status[1];
		}

		SQLDAMetadataLauncher inMessage(inSqlda);
		SQLDAMetadata::DataBuffer inMsgBuffer;
		inMessage.gatherData(inMsgBuffer);

		if ((flags & IStatement::FLAG_HAS_CURSOR) && !outSqlda)
		{
			statement->checkCursorClosed();

			statement->openCursor(&statusWrapper, traHandle,
				inMessage.metadata, inMsgBuffer.begin(), DELAYED_OUT_FORMAT);
			if (status.getState() & IStatus::STATE_ERRORS)
			{
				return status[1];
			}

			fb_assert(statement->statement->cursor);
		}
		else
		{
			SQLDAMetadataLauncher outMessage(outSqlda);

			statement->execute(&statusWrapper, traHandle,
				inMessage.metadata, inMsgBuffer.begin(), outMessage.metadata, outMessage.getBuffer());
			if (!(status.getState() & IStatus::STATE_ERRORS))
				outMessage.scatterData();
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Execute a non-SELECT dynamic SQL statement.
ISC_STATUS API_ROUTINE isc_dsql_execute_m(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	isc_stmt_handle* stmtHandle, USHORT blrLength, const SCHAR* blr,
	USHORT msgType, USHORT msgLength, SCHAR* msg)
{
	return isc_dsql_execute2_m(userStatus, traHandle, stmtHandle, blrLength, blr,
		msgType, msgLength, msg, 0, NULL, 0, 0, NULL);
}


// Execute a non-SELECT dynamic SQL statement.
ISC_STATUS API_ROUTINE isc_dsql_execute2_m(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	isc_stmt_handle* stmtHandle, USHORT inBlrLength, const SCHAR* inBlr, USHORT inMsgType,
	USHORT inMsgLength, const SCHAR* inMsg, USHORT outBlrLength,
	SCHAR* outBlr, USHORT /*outMsgType*/, USHORT outMsgLength, SCHAR* outMsg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		if (((SSHORT) inMsgType) == -1)	// dirty hack to support old esql code
		{
			statement->pseudoOpened = true;
			return status[1];
		}

		statement->checkPrepared();

		InternalMessageBuffer inMsgBuffer(inBlrLength, (const UCHAR*) inBlr, inMsgLength, (UCHAR*) inMsg);
		InternalMessageBuffer outMsgBuffer(outBlrLength, (const UCHAR*) outBlr, outMsgLength, (UCHAR*) outMsg);

		const unsigned flags = statement->statement->getFlags(&statusWrapper);
		if (!(status.getState() & IStatus::STATE_ERRORS))
		{
			if ((flags & IStatement::FLAG_HAS_CURSOR) && (outMsgLength == 0))
			{
				statement->openCursor(&statusWrapper, traHandle,
					inMsgBuffer.metadata, inMsgBuffer.buffer,
					(outBlrLength ? outMsgBuffer.metadata : DELAYED_OUT_FORMAT));
			}
			else
			{
				statement->execute(&statusWrapper, traHandle,
					inMsgBuffer.metadata, inMsgBuffer.buffer, outMsgBuffer.metadata, outMsgBuffer.buffer);
			}
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Is this really API function? Where is it declared?
ISC_STATUS API_ROUTINE isc_dsql_exec_immediate(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	const XSQLDA* sqlda)
{
	return isc_dsql_execute_immediate(userStatus, dbHandle, traHandle, stmtLength, sqlStmt,
		dialect, sqlda);
}


// Prepare a statement for execution.
ISC_STATUS API_ROUTINE isc_dsql_execute_immediate(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	const XSQLDA* sqlda)
{
	return isc_dsql_exec_immed2(userStatus, dbHandle, traHandle, stmtLength, sqlStmt,
		dialect, sqlda, NULL);
}


// Prepare a statement for execution.
ISC_STATUS API_ROUTINE isc_dsql_exec_immed2(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	const XSQLDA* inSqlda, const XSQLDA* outSqlda)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		FB_BOOLEAN stmtIsCrDb = FB_FALSE;
		YAttachment* att = utilInterface.executeCreateDatabase(&statusWrapper, stmtLength,
			sqlStmt, dialect, &stmtIsCrDb);

		if (stmtIsCrDb)
		{
			if (!(status.getState() & IStatus::STATE_ERRORS))
				*dbHandle = att->getHandle();

			return status[1];
		}

		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));

		SQLDAMetadata::DataBuffer inMessageBuffer;

		SQLDAMetadataLauncher inMessage(inSqlda), outMessage(outSqlda);

		inMessage.gatherData(inMessageBuffer);

		attachment->execute(&statusWrapper, traHandle, stmtLength, sqlStmt,
			dialect, inMessage.metadata, inMessageBuffer.begin(),
			outMessage.metadata, outMessage.getBuffer());

		if (!(status.getState() & IStatus::STATE_ERRORS))
		{
			outMessage.scatterData();
		}

	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return 	status[1];
}


// Is this really API function? Where is it declared?
ISC_STATUS API_ROUTINE isc_dsql_exec_immediate_m(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	USHORT blrLength, USHORT msgType, USHORT msgLength, SCHAR* blr, SCHAR* msg)
{
	return isc_dsql_execute_immediate_m(userStatus, dbHandle, traHandle, stmtLength, sqlStmt,
		dialect, blrLength, blr, msgType, msgLength, msg);
}


// Prepare a statement for execution.
ISC_STATUS API_ROUTINE isc_dsql_execute_immediate_m(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	USHORT blrLength, SCHAR* blr, USHORT msgType, USHORT msgLength, SCHAR* msg)
{
	return isc_dsql_exec_immed2_m(userStatus, dbHandle, traHandle, stmtLength, sqlStmt, dialect,
		blrLength, blr, msgType, msgLength, msg, 0, NULL, 0, 0, NULL);
}


// Prepare a statement for execution.
ISC_STATUS API_ROUTINE isc_dsql_exec_immed2_m(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	USHORT inBlrLength, SCHAR* inBlr, USHORT inMsgType, USHORT inMsgLength, const SCHAR* inMsg,
	USHORT outBlrLength, SCHAR* outBlr, USHORT outMsgType, USHORT outMsgLength, SCHAR* outMsg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	FB_BOOLEAN stmtIsCrDb = FB_FALSE;
	YAttachment* att = utilInterface.executeCreateDatabase(&statusWrapper, stmtLength,
		sqlStmt, dialect, &stmtIsCrDb);

	if (stmtIsCrDb)
	{
		if (!(status.getState() & IStatus::STATE_ERRORS))
			*dbHandle = att->getHandle();

		return status[1];
	}

	return isc_dsql_exec_immed3_m(userStatus, dbHandle, traHandle, stmtLength, sqlStmt, dialect,
		inBlrLength, inBlr, inMsgType, inMsgLength, inMsg,
		outBlrLength, outBlr, outMsgType, outMsgLength, outMsg);
}


// Execute statement immediately.
ISC_STATUS API_ROUTINE isc_dsql_exec_immed3_m(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	USHORT inBlrLength, const SCHAR* inBlr, USHORT inMsgType, USHORT inMsgLength, const SCHAR* inMsg,
	USHORT outBlrLength, SCHAR* outBlr, USHORT /*outMsgType*/, USHORT outMsgLength, SCHAR* outMsg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));

		InternalMessageBuffer inMsgBuffer(inBlrLength, reinterpret_cast<const unsigned char*>(inBlr),
										  inMsgLength, reinterpret_cast<UCHAR*>(const_cast<SCHAR*>(inMsg)));
		InternalMessageBuffer outMsgBuffer(outBlrLength, reinterpret_cast<unsigned char*>(outBlr),
										   outMsgLength, reinterpret_cast<unsigned char*>(outMsg));

		attachment->execute(&statusWrapper, traHandle, stmtLength, sqlStmt,
			dialect, inMsgBuffer.metadata, inMsgBuffer.buffer,
			outMsgBuffer.metadata, outMsgBuffer.buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}

// Fetch next record from a dynamic SQL cursor
ISC_STATUS API_ROUTINE isc_dsql_fetch(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	USHORT dialect, const XSQLDA* sqlda)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		if (!sqlda)
			status_exception::raise(Arg::Gds(isc_dsql_sqlda_err) << Arg::Gds(isc_dsql_no_output_sqlda));

		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		SQLDAMetadataLauncher outMessage(sqlda);

		if (statement->fetch(&statusWrapper, outMessage.metadata, outMessage.getBuffer()))
			outMessage.scatterData();
		else if (!(status.getState() & IStatus::STATE_ERRORS))
			return 100;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Fetch next record from a dynamic SQL cursor
ISC_STATUS API_ROUTINE isc_dsql_fetch_m(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	USHORT blrLength, SCHAR* blr, USHORT /*msgType*/, USHORT msgLength, SCHAR* msg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		InternalMessageBuffer msgBuffer(blrLength, reinterpret_cast<UCHAR*>(blr), msgLength, reinterpret_cast<UCHAR*>(msg));

		if (!msgBuffer.metadata)
		{
			(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				Arg::Gds(isc_dsql_cursor_open_err)).raise();
		}

		if (!statement->fetch(&statusWrapper, msgBuffer.metadata, reinterpret_cast<UCHAR*>(msg)) &&
			!(status.getState() & IStatus::STATE_ERRORS))
		{
			return 100;
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Release request for an sql statement
ISC_STATUS API_ROUTINE isc_dsql_free_statement(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	USHORT option)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		if (option & DSQL_drop)
		{
			// Release everything
			statement->closeCursor(&statusWrapper, false);
			statement->closeStatement(&statusWrapper);
			// statement->userHandle is not erased here because this routine can be called
			// against a copy of original variable.
			// This call must release statement and clean handles
			statement->destroy(0);
 			*stmtHandle = 0;
		}
		else if (option & DSQL_unprepare)
		{
			// Release everything but the request itself
			statement->closeCursor(&statusWrapper, false);
			statement->closeStatement(&statusWrapper);
		}
		else if (option & DSQL_close)
		{
			// Only close the cursor
			statement->closeCursor(&statusWrapper, true);
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Insert a BLOB into a dynamic SQL cursor. (deprecated)
ISC_STATUS API_ROUTINE isc_dsql_insert(ISC_STATUS* userStatus, isc_stmt_handle* /*stmtHandle*/,
	USHORT /*dialect*/, XSQLDA* /*sqlda*/)
{
	(Arg::Gds(isc_feature_removed) << Arg::Str("isc_dsql_insert")).copyTo(userStatus);
	return userStatus[1];
}


// Insert a BLOB into a dynamic SQL cursor. (deprecated)
ISC_STATUS API_ROUTINE isc_dsql_insert_m(ISC_STATUS* userStatus, isc_stmt_handle* /*stmtHandle*/,
	USHORT /*blrLength*/, const SCHAR* /*blr*/,
	USHORT /*msgType*/, USHORT /*msgLength*/, const SCHAR* /*msg*/)
{
	(Arg::Gds(isc_feature_removed) << Arg::Str("isc_dsql_insert")).copyTo(userStatus);
	return userStatus[1];
}


// Prepare a statement for execution.
ISC_STATUS API_ROUTINE isc_dsql_prepare(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	isc_stmt_handle* stmtHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	XSQLDA* sqlda)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));
		RefPtr<YTransaction> transaction;

		if (statement->statement)
		{
			statement->closeStatement(&statusWrapper);
			if (status.getState() & IStatus::STATE_ERRORS)
				return status[1];
		}

		statement->cursorName = "";

		if (traHandle && *traHandle)
			transaction = translateHandle(transactions, traHandle);

		statement->statement = statement->attachment.get()->prepare(&statusWrapper, transaction,
			stmtLength, sqlStmt, dialect, IStatement::PREPARE_PREFETCH_METADATA);

		if (!(status.getState() & IStatus::STATE_ERRORS))
		{
			StatusVector tempStatus(NULL);
			CheckStatusWrapper tempCheckStatusWrapper(&tempStatus);

			RefPtr<IMessageMetadata> parameters(REF_NO_INCR,
				statement->statement->next->getOutputMetadata(&tempCheckStatusWrapper));
			tempStatus.check();

			sqldaDescribeParameters(sqlda, parameters);
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Prepare a statement for execution.
ISC_STATUS API_ROUTINE isc_dsql_prepare_m(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	isc_stmt_handle* stmtHandle, USHORT stmtLength, const SCHAR* sqlStmt, USHORT dialect,
	USHORT itemLength, const SCHAR* items, USHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));
		RefPtr<YTransaction> transaction;

		if (statement->statement)
		{
			statement->closeStatement(&statusWrapper);
			if (status.getState() & IStatus::STATE_ERRORS)
				return status[1];
		}

		statement->cursorName = "";

		if (traHandle && *traHandle)
			transaction = translateHandle(transactions, traHandle);

		unsigned flags = StatementMetadata::buildInfoFlags(
			itemLength, reinterpret_cast<const UCHAR*>(items));

		statement->statement = statement->attachment.get()->prepare(&statusWrapper, transaction,
			stmtLength, sqlStmt, dialect, flags);

		if (!(status.getState() & IStatus::STATE_ERRORS))
		{
			StatusVector tempStatus(NULL);
			CheckStatusWrapper tempCheckStatusWrapper(&tempStatus);
			statement->statement->getInfo(&tempCheckStatusWrapper,
				itemLength, reinterpret_cast<const UCHAR*>(items),
				bufferLength, reinterpret_cast<UCHAR*>(buffer));
			tempStatus.check();
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Set a cursor name for a dynamic request.
ISC_STATUS API_ROUTINE isc_dsql_set_cursor_name(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	const SCHAR* cursorName, USHORT /*cursorType*/)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		if (statement->cursorName.hasData() && statement->cursorName != cursorName)
		{
			(Arg::Gds(isc_dsql_decl_err) <<
			 Arg::Gds(isc_dsql_cursor_redefined) << statement->cursorName).raise();
		}

		statement->cursorName = cursorName;
		if (statement->statement)
			statement->statement->setCursorName(&statusWrapper, cursorName);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Set statement timeout.
ISC_STATUS API_ROUTINE fb_dsql_set_timeout(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	ULONG timeout)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));

		if (statement->statement)
			statement->statement->setTimeout(&statusWrapper, timeout);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get interface by legacy handle
/*ISC_STATUS API_ROUTINE fb_get_statement_interface(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	void** stmtIface)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));
		statement->checkPrepared();

		fb_assert(statement->statement);
		IStatement* rc = statement->statement;
		rc->addRef();
		*stmtIface = rc;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}
*/

// Provide information on sql statement.
ISC_STATUS API_ROUTINE isc_dsql_sql_info(ISC_STATUS* userStatus, isc_stmt_handle* stmtHandle,
	SSHORT itemLength, const SCHAR* items, SSHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<IscStatement> statement(translateHandle(statements, stmtHandle));
		statement->checkPrepared(isc_info_unprepared_stmt);

		statement->statement->getInfo(&statusWrapper,
			USHORT(itemLength), reinterpret_cast<const UCHAR*>(items),
			USHORT(bufferLength), reinterpret_cast<UCHAR*>(buffer));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Enable access to a specific subsystem. If no subsystem has been explicitly enabled, all are
// available.
int API_ROUTINE gds__enable_subsystem(TEXT* /*subsystem*/)
{
	return FALSE;
}


namespace
{
	class WaitCallback final :
		public RefCntIface<IEventCallbackImpl<WaitCallback, CheckStatusWrapper> >
	{
	public:
		explicit WaitCallback(UCHAR* aBuffer)
			: buffer(aBuffer)
		{
		}

		// IEventCallback implementation
		virtual void eventCallbackFunction(unsigned int length, const UCHAR* events)
		{
			try
			{
				memcpy(buffer, events, length);
				sem.release();
			}
			catch (const Exception&)
			{ }
		}

		UCHAR* buffer;
		Semaphore sem;
	};

}

// Que request for event notification.
ISC_STATUS API_ROUTINE isc_wait_for_event(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	USHORT length, const UCHAR* eventsData, UCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);
	YEvents* events = NULL;

	RefPtr<WaitCallback> callback(FB_NEW WaitCallback(buffer));

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));

		events = attachment->queEvents(&statusWrapper, callback, length, eventsData);

		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		callback->sem.enter();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	if (events)
	{
		events->release();
	}

	return status[1];
}


namespace
{
	class QueCallback final : public RefCntIface<IEventCallbackImpl<QueCallback, CheckStatusWrapper> >
	{
	public:
		QueCallback(FPTR_EVENT_CALLBACK aAst, void* aArg)
			: ast(aAst),
			  arg(aArg),
			  events(NULL)
		{
			mtx.enter(FB_FUNCTION);
		}

		void setEvents(YEvents* e)
		{
			fb_assert(!events);
			events = e;
			mtx.leave();
		}

		// IEventCallback implementation
		virtual void eventCallbackFunction(unsigned int length, const UCHAR* list)
		{
			RefPtr<YEvents> tmp;
			try
			{
				MutexLockGuard g(mtx, FB_FUNCTION);
				if (events)
				{
					ast(arg, length, list);

					tmp = events;
					events = NULL;
				}
			}
			catch (const Exception&)
			{ }

			if (tmp)
			{
				LocalStatus status;
				CheckStatusWrapper statusWrapper(&status);
				tmp->cancel(&statusWrapper);
			}
		}

		Mutex mtx;
		FPTR_EVENT_CALLBACK ast;
		void* arg;
		YEvents* events;
	};
}

// Que request for event notification.
ISC_STATUS API_ROUTINE isc_que_events(ISC_STATUS* userStatus, isc_db_handle* dbHandle, SLONG* id,
	USHORT length, const UCHAR* eventsData, FPTR_EVENT_CALLBACK ast, void* arg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);
	RefPtr<YEvents> events;

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));

		RefPtr<QueCallback> callback(FB_NEW QueCallback(ast, arg));
		events = attachment->queEvents(&statusWrapper, callback, length, eventsData);
		if (events)
			*id = FB_API_HANDLE_TO_ULONG(events->getHandle());

		callback->setEvents(events);		// should be called in case of NULL events too
		if (status.getState() & IStatus::STATE_ERRORS)
		{
			return status[1];
		}
	}
	catch (const Exception& e)
	{
		if (events)
		{
			*id = 0;
			events->release();
		}

		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get a segment from a blob opened for reading.
ISC_STATUS API_ROUTINE isc_get_segment(ISC_STATUS* userStatus, isc_blob_handle* blobHandle,
	USHORT* returnLength, USHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YBlob> blob(translateHandle(blobs, blobHandle));
		unsigned int length;
		int cc = blob->getSegment(&statusWrapper, bufferLength, reinterpret_cast<UCHAR*>(buffer), &length);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*returnLength = length;

		// Raise pseudo errors
		switch (cc)
		{
		case IStatus::RESULT_NO_DATA:
			Arg::Gds(isc_segstr_eof).raise();
			break;
		case IStatus::RESULT_SEGMENT:
			Arg::Gds(isc_segment).raise();
			break;
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Snatch a slice of an array.
ISC_STATUS API_ROUTINE isc_get_slice(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, ISC_QUAD* arrayId, USHORT sdlLength, const UCHAR* sdl,
	USHORT paramLength, const SLONG* param, SLONG sliceLength, void* slice, SLONG* returnLength)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		int length = attachment->getSlice(&statusWrapper, transaction, arrayId, sdlLength, sdl,
			paramLength, reinterpret_cast<const UCHAR*>(param), sliceLength, reinterpret_cast<UCHAR*>(slice));

		if (!(status.getState() & IStatus::STATE_ERRORS) && returnLength)
			*returnLength = length;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Clean up a dangling transaction handle.
ISC_STATUS API_ROUTINE fb_disconnect_transaction(ISC_STATUS* userStatus, isc_tr_handle* traHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));
		transaction->disconnect(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*traHandle = 0;	// ASF: Previous versions wasn't cleaning the handle.
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Open an existing blob.
ISC_STATUS API_ROUTINE isc_open_blob(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, isc_blob_handle* blobHandle, ISC_QUAD* blobId)
{
	return openOrCreateBlob(userStatus, dbHandle, traHandle, blobHandle, blobId, 0, 0, false);
}


// Open an existing blob (extended edition).
ISC_STATUS API_ROUTINE isc_open_blob2(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, isc_blob_handle* blobHandle, ISC_QUAD* blobId,
	ISC_USHORT bpbLength, const UCHAR* bpb)
{
	return openOrCreateBlob(userStatus, dbHandle, traHandle, blobHandle, blobId,
		bpbLength, bpb, false);
}


// Prepare a transaction for commit. First phase of a two phase commit.
ISC_STATUS API_ROUTINE isc_prepare_transaction(ISC_STATUS* userStatus, isc_tr_handle* traHandle)
{
	return isc_prepare_transaction2(userStatus, traHandle, 0, NULL);
}


// Prepare a transaction for commit. First phase of a two phase commit.
ISC_STATUS API_ROUTINE isc_prepare_transaction2(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	USHORT msgLength, const UCHAR* msg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));
		transaction->prepare(&statusWrapper, msgLength, msg);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Put a segment in a blob opened for writing (creation).
ISC_STATUS API_ROUTINE isc_put_segment(ISC_STATUS* userStatus, isc_blob_handle* blobHandle,
	USHORT bufferLength, const SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YBlob> blob(translateHandle(blobs, blobHandle));
		blob->putSegment(&statusWrapper, bufferLength, reinterpret_cast<const UCHAR*>(buffer));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Put a slice in an array.
ISC_STATUS API_ROUTINE isc_put_slice(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, ISC_QUAD* arrayId, USHORT sdlLength, const SCHAR* sdl,
	USHORT paramLength, const SLONG* param, SLONG sliceLength, void* slice)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		attachment->putSlice(&statusWrapper, transaction, arrayId, sdlLength, reinterpret_cast<const UCHAR*>(sdl),
			paramLength, reinterpret_cast<const UCHAR*>(param), sliceLength, static_cast<UCHAR*>(slice));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Send a record to the host program.
ISC_STATUS API_ROUTINE isc_receive(ISC_STATUS* userStatus, isc_req_handle* reqHandle, USHORT msgType,
	USHORT msgLength, void* msg, SSHORT level)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));
		request->receive(&statusWrapper, level, msgType, msgLength, reinterpret_cast<UCHAR*>(msg));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Connect to a transaction in limbo.
ISC_STATUS API_ROUTINE isc_reconnect_transaction(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT length, const SCHAR* id)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		nullCheck(traHandle, isc_bad_trans_handle);

		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));

		YTransaction* transaction = attachment->reconnectTransaction(&statusWrapper, length,
			reinterpret_cast<const UCHAR*>(id));

		if (status.getState() & IStatus::STATE_ERRORS)
			status_exception::raise(status);

		*traHandle = transaction->getHandle();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Release a request.
ISC_STATUS API_ROUTINE isc_release_request(ISC_STATUS* userStatus, isc_req_handle* reqHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));
		request->free(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*reqHandle = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Provide information on blob object.
ISC_STATUS API_ROUTINE isc_request_info(ISC_STATUS* userStatus, isc_req_handle* reqHandle,
	SSHORT level, SSHORT itemLength, const SCHAR* items, SSHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));

		request->getInfo(&statusWrapper, level, itemLength, reinterpret_cast<const UCHAR*>(items),
			bufferLength, reinterpret_cast<UCHAR*>(buffer));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}

// API to be used to tell Firebird to reset it's FPE handler - eg: client has an FPE of it's own
// and just changed it. Returns prior setting of the FPE reset flag.
#if defined (SOLARIS) || defined (WIN_NT)
extern "C"
#endif
SLONG API_ROUTINE isc_reset_fpe(USHORT /*fpe_status*/)
{
	// A client-only API call, isc_reset_fpe() is deprecated - we do not use
	// the FPE handler anymore, it can't be used in multithreaded library.
	// Parameter is ignored, it always returns FPE_RESET_ALL_API_CALL,
	// this is the most close code to what we are doing now.

	//static const USHORT FPE_RESET_INIT_ONLY		= 0x0;	// Don't reset FPE after init
	//static const USHORT FPE_RESET_NEXT_API_CALL	= 0x1;	// Reset FPE on next gds call
	static const USHORT FPE_RESET_ALL_API_CALL		= 0x2;	// Reset FPE on all gds call

	return FPE_RESET_ALL_API_CALL;
}


// Abort a transaction, but keep all cursors open.
ISC_STATUS API_ROUTINE isc_rollback_retaining(ISC_STATUS* userStatus, isc_tr_handle* traHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));
		transaction->rollbackRetaining(&statusWrapper);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Abort a transaction.
ISC_STATUS API_ROUTINE isc_rollback_transaction(ISC_STATUS* userStatus, isc_tr_handle* traHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));
		transaction->rollback(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*traHandle = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Seek a blob.
ISC_STATUS API_ROUTINE isc_seek_blob(ISC_STATUS* userStatus, isc_blob_handle* blobHandle,
	SSHORT mode, SLONG offset, SLONG* result)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YBlob> blob(translateHandle(blobs, blobHandle));
		unsigned int pos = blob->seek(&statusWrapper, mode, offset);

		if (!(status.getState() & IStatus::STATE_ERRORS) && result)
			*result = pos;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get a record from the host program.
ISC_STATUS API_ROUTINE isc_send(ISC_STATUS* userStatus, isc_req_handle* reqHandle, USHORT msgType,
	USHORT msgLength, const void* msg, SSHORT level)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));
		request->send(&statusWrapper, level, msgType,
			msgLength, static_cast<const UCHAR*>(msg));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Attach a service through the first subsystem that recognizes it.
ISC_STATUS API_ROUTINE isc_service_attach(ISC_STATUS* userStatus, USHORT serviceLength,
	const TEXT* serviceName, isc_svc_handle* publicHandle, USHORT spbLength, const SCHAR* spb)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);
	YService* service = NULL;

	try
	{
		nullCheck(publicHandle, isc_bad_svc_handle);

		if (!serviceName)
			status_exception::raise(Arg::Gds(isc_service_att_err) << Arg::Gds(isc_svc_name_missing));

		string svcName(serviceName, serviceLength ? serviceLength : fb_strlen(serviceName));

		RefPtr<Dispatcher> dispatcher(FB_NEW Dispatcher);

		dispatcher->setDbCryptCallback(&statusWrapper, TLS_GET(legacyCryptCallback));
		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		service = dispatcher->attachServiceManager(&statusWrapper, svcName.c_str(),
			spbLength, reinterpret_cast<const UCHAR*>(spb));
		if (status.getState() & IStatus::STATE_ERRORS)
			return status[1];

		*publicHandle = service->getHandle();
	}
	catch (const Exception& e)
	{
		if (service)
		{
			StatusVector temp(NULL);
			CheckStatusWrapper tempCheckStatusWrapper(&temp);
			service->detach(&tempCheckStatusWrapper);
		}

		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Close down a service.
ISC_STATUS API_ROUTINE isc_service_detach(ISC_STATUS* userStatus, isc_svc_handle* handle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YService> service(translateHandle(services, handle));
		service->detach(&statusWrapper);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			*handle = 0;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Provide information on service object.
// NOTE: The parameter RESERVED must not be used for any purpose as there are networking issues
// involved (as with any handle that goes over the network). This parameter will be implemented at
// a later date.
ISC_STATUS API_ROUTINE isc_service_query(ISC_STATUS* userStatus, isc_svc_handle* handle,
	isc_resv_handle* /*reserved*/, USHORT sendItemLength, const SCHAR* sendItems, USHORT recvItemLength,
	const SCHAR* recvItems, USHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YService> service(translateHandle(services, handle));

		service->query(&statusWrapper, sendItemLength, reinterpret_cast<const unsigned char*>(sendItems),
			recvItemLength, reinterpret_cast<const unsigned char*>(recvItems),
			bufferLength, reinterpret_cast<unsigned char*>(buffer));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Starts a service thread.
// NOTE: The parameter RESERVED must not be used for any purpose as there are networking issues
// involved (as with any handle that goes over the network). This parameter will be implemented at
// a later date.
ISC_STATUS API_ROUTINE isc_service_start(ISC_STATUS* userStatus, isc_svc_handle* handle,
	isc_resv_handle* /*reserved*/, USHORT spbLength, const SCHAR* spb)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YService> service(translateHandle(services, handle));
		service->start(&statusWrapper, spbLength, reinterpret_cast<const unsigned char*>(spb));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get a record from the host program.
ISC_STATUS API_ROUTINE isc_start_and_send(ISC_STATUS* userStatus, isc_req_handle* reqHandle,
	isc_tr_handle* traHandle, USHORT msgType, USHORT msgLength, const void* msg, SSHORT level)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		request->startAndSend(&statusWrapper, transaction, level, msgType,
			msgLength, reinterpret_cast<const UCHAR*>(msg));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get a record from the host program.
ISC_STATUS API_ROUTINE isc_start_request(ISC_STATUS* userStatus, isc_req_handle* reqHandle,
	isc_tr_handle* traHandle, SSHORT level)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		request->start(&statusWrapper, transaction, level);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Start a transaction.
ISC_STATUS API_ROUTINE isc_start_multiple(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	SSHORT count, void* vec)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);
	TEB* vector = (TEB*) vec;
	YTransaction* multiTrans = NULL;

	try
	{
		nullCheck(traHandle, isc_bad_trans_handle);

		if (count <= 0 || !vector)
			status_exception::raise(Arg::Gds(isc_bad_teb_form));

		if (count == 1)
		{
			RefPtr<YAttachment> attachment(translateHandle(attachments, vector->teb_database));

			YTransaction* transaction = attachment->startTransaction(&statusWrapper,
				vector->teb_tpb_length, vector->teb_tpb);

			if (transaction)
				*traHandle = transaction->getHandle();

			return status[1];
		}

		DtcStart* ds = MasterImplementation::dtc->startBuilder(&statusWrapper);
		if (statusWrapper.getState() & IStatus::STATE_ERRORS)
			return status[1];

		for (SSHORT i = 0; i < count; ++i, ++vector)
		{
			RefPtr<YAttachment> attachment(translateHandle(attachments, vector->teb_database));
			ds->addWithTpb(&statusWrapper, attachment, vector->teb_tpb_length,
				reinterpret_cast<const unsigned char*>(vector->teb_tpb));

			if (statusWrapper.getState() & IStatus::STATE_ERRORS)
			{
				ds->dispose();
				return status[1];
			}
		}

		multiTrans = ds->start(&statusWrapper);
		if (!multiTrans)
		{
			ds->dispose();
			return status[1];
		}

		*traHandle = multiTrans->getHandle();
	}
	catch (const Exception& e)
	{
		if (multiTrans)
			multiTrans->release();

		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Start a transaction..
ISC_STATUS API_ROUTINE_VARARG isc_start_transaction(ISC_STATUS* userStatus,
	isc_tr_handle* traHandle, SSHORT count, ...)
{
	try
	{
		HalfStaticArray<TEB, 16> tebs;
		TEB* teb = tebs.getBuffer(count);

		const TEB* const end = teb + count;
		va_list ptr;
		va_start(ptr, count);

		for (TEB* teb_iter = teb; teb_iter < end; teb_iter++)
		{
			teb_iter->teb_database = va_arg(ptr, isc_db_handle*);
			teb_iter->teb_tpb_length = va_arg(ptr, int);
			teb_iter->teb_tpb = va_arg(ptr, UCHAR *);
		}

		va_end(ptr);

		return isc_start_multiple(userStatus, traHandle, count, teb);
	}
	catch (const Exception& e)
	{
		StatusVector status(userStatus);
		CheckStatusWrapper statusWrapper(&status);

		e.stuffException(&statusWrapper);
		return status[1];
	}
}


// Execute a procedure.
ISC_STATUS API_ROUTINE isc_transact_request(ISC_STATUS* userStatus, isc_db_handle* dbHandle,
	isc_tr_handle* traHandle, USHORT blrLength, SCHAR* blr, USHORT inMsgLength, SCHAR* inMsg,
	USHORT outMsgLength, SCHAR* outMsg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		attachment->transactRequest(&statusWrapper, transaction,
			blrLength, reinterpret_cast<unsigned char*>(blr),
			inMsgLength, reinterpret_cast<unsigned char*>(inMsg),
			outMsgLength, reinterpret_cast<unsigned char*>(outMsg));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Register a transaction specific cleanup handler.
ISC_STATUS API_ROUTINE gds__transaction_cleanup(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	TransactionCleanupRoutine* routine, void* arg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		AutoPtr<CleanupCallbackImpl<YTransaction, TransactionCleanupRoutine> > callback(
			FB_NEW CleanupCallbackImpl<YTransaction, TransactionCleanupRoutine>(transaction, routine, arg));

		transaction->addCleanupHandler(&statusWrapper, callback);

		if (!(status.getState() & IStatus::STATE_ERRORS))
			callback.release();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Provide information on transaction object.
ISC_STATUS API_ROUTINE isc_transaction_info(ISC_STATUS* userStatus, isc_tr_handle* traHandle,
	SSHORT itemLength, const SCHAR* items, SSHORT bufferLength, SCHAR* buffer)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YTransaction> transaction(translateHandle(transactions, traHandle));

		transaction->getInfo(&statusWrapper, itemLength, reinterpret_cast<const UCHAR*>(items),
			bufferLength, reinterpret_cast<UCHAR*>(buffer));
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Unwind a running request. This is potentially nasty since it can be called asynchronously.
ISC_STATUS API_ROUTINE isc_unwind_request(ISC_STATUS* userStatus, isc_req_handle* reqHandle,
	SSHORT level)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YRequest> request(translateHandle(requests, reqHandle));
		request->unwind(&statusWrapper, level);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Shutdown firebird.
int API_ROUTINE fb_shutdown(unsigned int timeout, const int reason)
{
	StatusVector status(NULL);
	CheckStatusWrapper statusWrapper(&status);

	RefPtr<Dispatcher> dispatcher(FB_NEW Dispatcher);

	dispatcher->shutdown(&statusWrapper, timeout, reason);
	return (status.getState() & IStatus::STATE_ERRORS) ? FB_FAILURE : FB_SUCCESS;
}


// Register client callback to be called when FB is going down.
ISC_STATUS API_ROUTINE fb_shutdown_callback(ISC_STATUS* userStatus, FB_SHUTDOWN_CALLBACK callBack,
	const int mask, void* arg)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		ShutChain::add(callBack, mask, arg);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Check the attachment handle for persistent errors.
ISC_STATUS API_ROUTINE fb_ping(ISC_STATUS* userStatus, isc_db_handle* dbHandle)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		RefPtr<YAttachment> attachment(translateHandle(attachments, dbHandle));
		attachment->ping(&statusWrapper);
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get the legacy handle of a database.
ISC_STATUS API_ROUTINE fb_get_database_handle(ISC_STATUS* userStatus, isc_db_handle* handle,
	void* obj)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		if (!obj)
			status_exception::raise(Arg::Gds(isc_bad_db_handle));

		// Must first cast to the base interface.
		YAttachment* yObject = static_cast<YAttachment*>(static_cast<IAttachment*>(obj));
		*handle = yObject->getHandle();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get the legacy handle of a transaction.
ISC_STATUS API_ROUTINE fb_get_transaction_handle(ISC_STATUS* userStatus, isc_tr_handle* handle,
	void* obj)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		if (!obj)
			status_exception::raise(Arg::Gds(isc_bad_trans_handle));

		// Must first cast to the base interface.
		YTransaction* yObject = static_cast<YTransaction*>(static_cast<ITransaction*>(obj));
		*handle = yObject->getHandle();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get transaction interface by legacy handle
ISC_STATUS API_ROUTINE fb_get_transaction_interface(ISC_STATUS* userStatus, void* iPtr,
	isc_tr_handle* hndlPtr)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		ITransaction** tra = (ITransaction**) iPtr;
		if (*tra)
			(Arg::Gds(isc_random) << "Interface must be null").raise();

		RefPtr<YTransaction> transaction(translateHandle(transactions, hndlPtr));

		transaction->addRef();
		*tra = transaction;
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


// Get statement interface by legacy handle
ISC_STATUS API_ROUTINE fb_get_statement_interface(ISC_STATUS* userStatus, void* iPtr,
	isc_stmt_handle* hndlPtr)
{
	StatusVector status(userStatus);
	CheckStatusWrapper statusWrapper(&status);

	try
	{
		IStatement** stmt = (IStatement**) iPtr;
		if (*stmt)
			(Arg::Gds(isc_random) << "Interface must be null").raise();

		RefPtr<IscStatement> statement(translateHandle(statements, hndlPtr));
		statement->checkPrepared(isc_info_unprepared_stmt);

		*stmt = statement->getInterface();
	}
	catch (const Exception& e)
	{
		e.stuffException(&statusWrapper);
	}

	return status[1];
}


//-------------------------------------

namespace Why {

IAttachment* MasterImplementation::registerAttachment(IProvider* provider, IAttachment* attachment)
{
	IAttachment* rc = FB_NEW YAttachment(provider, attachment, "");
	rc->addRef();
	attachment->addRef();
	return rc;
}

ITransaction* MasterImplementation::registerTransaction(IAttachment* attachment,
	ITransaction* transaction)
{
	ITransaction* rc = FB_NEW YTransaction(static_cast<YAttachment*>(attachment), transaction);
	rc->addRef();
	transaction->addRef();
	return rc;
}


//-------------------------------------


YEvents::YEvents(YAttachment* aAttachment, IEvents* aNext, IEventCallback* aCallback)
	: YHelper(aNext), attachment(aAttachment), callback(aCallback)
{
	attachment.get()->childEvents.add(this);
}

FB_API_HANDLE& YEvents::getHandle()
{
	if (!handle)
		makeHandle(&events, this, handle);
	return handle;
}

void YEvents::destroy(unsigned dstrFlags)
{
	YAttachment* att = attachment.release();
	if (att)
		att->childEvents.remove(this);
	removeHandle(&events, handle);

	if (!(dstrFlags & DF_RELEASE))
	{
		const bool allowCancel = destroyed.compareExchange(0, 1);
		if (!allowCancel)
			return;
	}

	destroy2(dstrFlags);
}

void YEvents::cancel(CheckStatusWrapper* status)
{
	const bool allowCancel = destroyed.compareExchange(0, 1);
	if (!allowCancel)
		return;

	try
	{
		YEntry<YEvents> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{
				entry.next()->cancel(status);
				if (status->getErrors()[1] == isc_att_shutdown)
					status->init();
			}, [&]{entry.next()->deprecatedCancel(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YEvents::deprecatedCancel(CheckStatusWrapper* status)
{
	cancel(status);
}


//-------------------------------------


YRequest::YRequest(YAttachment* aAttachment, IRequest* aNext)
	: YHelper(aNext),
	  attachment(aAttachment),
	  userHandle(NULL)
{
	attachment.get()->childRequests.add(this);
}

isc_req_handle& YRequest::getHandle()
{
	if (!handle)
		makeHandle(&requests, this, handle);
	return handle;
}

void YRequest::destroy(unsigned dstrFlags)
{
	if (userHandle)
	{
		*userHandle = 0;
		userHandle = NULL;
	}

	YAttachment* att = attachment.release();
	if (att)
		att->childRequests.remove(this);

	removeHandle(&requests, handle);

	destroy2(dstrFlags);
}

void YRequest::receive(CheckStatusWrapper* status, int level, unsigned int msgType,
	unsigned int length, void* message)
{
	try
	{
		YEntry<YRequest> entry(status, this);
		entry.next()->receive(status, level, msgType, length, message);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YRequest::send(CheckStatusWrapper* status, int level, unsigned int msgType,
	unsigned int length, const void* message)
{
	try
	{
		YEntry<YRequest> entry(status, this);
		entry.next()->send(status, level, msgType, length, message);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YRequest::getInfo(CheckStatusWrapper* status, int level, unsigned int itemsLength,
	const unsigned char* items, unsigned int bufferLength, unsigned char* buffer)
{
	try
	{
		YEntry<YRequest> entry(status, this);
		entry.next()->getInfo(status, level, itemsLength, items, bufferLength, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YRequest::start(CheckStatusWrapper* status, ITransaction* transaction, int level)
{
	try
	{
		YEntry<YRequest> entry(status, this);

		NextTransaction trans;
		attachment.get()->getNextTransaction(status, transaction, trans);
		entry.next()->start(status, trans, level);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YRequest::startAndSend(CheckStatusWrapper* status, ITransaction* transaction, int level,
	unsigned int msgType, unsigned int length, const void* message)
{
	try
	{
		YEntry<YRequest> entry(status, this);

		NextTransaction trans;
		attachment.get()->getNextTransaction(status, transaction, trans);
		entry.next()->startAndSend(status, trans, level, msgType, length, message);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YRequest::unwind(CheckStatusWrapper* status, int level)
{
	try
	{
		YEntry<YRequest> entry(status, this);
		entry.next()->unwind(status, level);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YRequest::free(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YRequest> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->free(status);}, [&]{entry.next()->deprecatedFree(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YRequest::deprecatedFree(CheckStatusWrapper* status)
{
	free(status);
}


//-------------------------------------


YBlob::YBlob(YAttachment* aAttachment, YTransaction* aTransaction, IBlob* aNext)
	: YHelper(aNext),
	  attachment(aAttachment),
	  transaction(aTransaction)
{
	aAttachment->childBlobs.add(this);
	aTransaction->childBlobs.add(this);
}

isc_blob_handle& YBlob::getHandle()
{
	if (!handle)
		makeHandle(&blobs, this, handle);
	return handle;
}

void YBlob::destroy(unsigned dstrFlags)
{
	YAttachment* att = attachment.release();
	if (att)
		att->childBlobs.remove(this);

	YTransaction* tra = transaction.release();
	if (tra)
		tra->childBlobs.remove(this);

	removeHandle(&blobs, handle);
	destroy2(dstrFlags);
}

void YBlob::getInfo(CheckStatusWrapper* status, unsigned int itemsLength,
	const unsigned char* items, unsigned int bufferLength, unsigned char* buffer)
{
	try
	{
		YEntry<YBlob> entry(status, this);
		entry.next()->getInfo(status, itemsLength, items, bufferLength, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

int YBlob::getSegment(CheckStatusWrapper* status, unsigned int bufferLength,
	void* buffer, unsigned int* segmentLength)
{
	try
	{
		YEntry<YBlob> entry(status, this);
		return entry.next()->getSegment(status, bufferLength, buffer, segmentLength);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}

void YBlob::putSegment(CheckStatusWrapper* status, unsigned int length, const void* buffer)
{
	try
	{
		YEntry<YBlob> entry(status, this);
		entry.next()->putSegment(status, length, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YBlob::cancel(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YBlob> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->cancel(status);}, [&]{entry.next()->deprecatedCancel(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YBlob::deprecatedCancel(CheckStatusWrapper* status)
{
	cancel(status);
}

void YBlob::close(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YBlob> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->close(status);}, [&]{entry.next()->deprecatedClose(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YBlob::deprecatedClose(CheckStatusWrapper* status)
{
	close(status);
}

int YBlob::seek(CheckStatusWrapper* status, int mode, int offset)
{
	try
	{
		YEntry<YBlob> entry(status, this);
		return entry.next()->seek(status, mode, offset);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}


//-------------------------------------


YStatement::YStatement(YAttachment* aAttachment, IStatement* aNext)
	: YHelper(aNext),
	  attachment(aAttachment), cursor(NULL), input(true), output(false)
{
	attachment.get()->childStatements.add(this);
}

void YStatement::destroy(unsigned dstrFlags)
{
	{	// scope
		MutexLockGuard guard(statementMutex, FB_FUNCTION);
		if (cursor)
		{
			cursor->destroy(DF_RELEASE);
			cursor = NULL;
		}
	}

	YAttachment* att = attachment.release();
	if (att)
		att->childStatements.remove(this);

	destroy2(dstrFlags);
}

void YStatement::getInfo(CheckStatusWrapper* status, unsigned int itemsLength,
	const unsigned char* items, unsigned int bufferLength, unsigned char* buffer)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		entry.next()->getInfo(status, itemsLength, items, bufferLength, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

unsigned YStatement::getType(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		return entry.next()->getType(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}

unsigned YStatement::getFlags(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		return entry.next()->getFlags(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}

const char* YStatement::getPlan(CheckStatusWrapper* status, FB_BOOLEAN detailed)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		return entry.next()->getPlan(status, detailed);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

IMessageMetadata* YMetadata::get(IStatement* next, YStatement* statement)
{
	if (!flag)
	{
		MutexLockGuard guard(statement->statementMutex, FB_FUNCTION);
		if (!flag)
		{
			RefPtr<IMessageMetadata> nextMeta(REF_NO_INCR, statement->getMetadata(input, next));
			metadata = FB_NEW MsgMetadata(nextMeta);

			flag = true;
		}
	}

	metadata->addRef();
	return metadata;
}

IMessageMetadata* YStatement::getMetadata(bool in, IStatement* next)
{
	LocalStatus status;
	CheckStatusWrapper statusWrapper(&status);
	IMessageMetadata* rc = in ?
		next->getInputMetadata(&statusWrapper) : next->getOutputMetadata(&statusWrapper);

	if (status.getState() & IStatus::STATE_ERRORS)
		status_exception::raise(&status);

	return rc;
}

IMessageMetadata* YStatement::getInputMetadata(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		return input.get(entry.next(), this);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

IMessageMetadata* YStatement::getOutputMetadata(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		return output.get(entry.next(), this);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

ISC_UINT64 YStatement::getAffectedRecords(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		return entry.next()->getAffectedRecords(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}

ITransaction* YStatement::execute(CheckStatusWrapper* status, ITransaction* transaction,
	IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, void* outBuffer)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		NextTransaction trans;
		if (transaction)
			attachment.get()->getNextTransaction(status, transaction, trans);

		ITransaction* newTrans = entry.next()->execute(status, trans, inMetadata, inBuffer,
			outMetadata, outBuffer);

		if (newTrans == trans)
			newTrans = transaction;
		else
		{
			if (transaction)
			{
				if (trans)
					trans->addRef();	// Will be decremented by release on next line
				transaction->release();
				transaction = NULL;		// Get ready for correct return in OOM case
			}

			if (newTrans)
			{
				newTrans = FB_NEW YTransaction(attachment.get(), newTrans);
				newTrans->addRef();
			}
		}

		return newTrans;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return transaction;
}

IResultSet* YStatement::openCursor(CheckStatusWrapper* status, ITransaction* transaction,
	IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, unsigned int flags)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		NextTransaction trans;
		if (transaction)
			attachment.get()->getNextTransaction(status, transaction, trans);

		IResultSet* rs = entry.next()->openCursor(status, trans, inMetadata, inBuffer, outMetadata, flags);
		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return NULL;
		}
		fb_assert(rs);

		YTransaction* const yTra = attachment.get()->getTransaction(transaction);

		YResultSet* r = FB_NEW YResultSet(attachment.get(), yTra, this, rs);
		r->addRef();
		return r;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

void YStatement::free(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YStatement> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->free(status);}, [&]{entry.next()->deprecatedFree(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YStatement::deprecatedFree(CheckStatusWrapper* status)
{
	free(status);
}


YBatch* YStatement::createBatch(CheckStatusWrapper* status, IMessageMetadata* inMetadata, unsigned parLength,
	const unsigned char* par)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		IBatch* batch = entry.next()->createBatch(status, inMetadata, parLength, par);
		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return NULL;
		}

		YBatch*	newBatch = FB_NEW YBatch(attachment.get(), batch);
		newBatch->addRef();
		return newBatch;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

//-------------------------------------

IscStatement::~IscStatement()
{
	if (userHandle)
	{
		*userHandle = 0;
		userHandle = nullptr;
	}

	removeHandle(&statements, handle);
}

void IscStatement::destroy(unsigned)
{
	YAttachment* att = attachment.release();
	if (att)
		att->childIscStatements.remove(this);

	release();
}

isc_stmt_handle& IscStatement::getHandle()
{
	if (!handle)
		makeHandle(&statements, this, handle);
	return handle;
}

void IscStatement::openCursor(CheckStatusWrapper* status, isc_tr_handle* traHandle,
	IMessageMetadata* inMetadata, UCHAR* buffer, IMessageMetadata* outMetadata)
{
	checkCursorClosed();

	// Transaction is not optional for statement returning result set
	RefPtr<YTransaction> transaction = translateHandle(transactions, traHandle);

	statement->openCursor(status, transaction, inMetadata, buffer, outMetadata, 0);

	if (status->getState() & IStatus::STATE_ERRORS)
		return;

	fb_assert(statement->cursor);

	delayedFormat = (outMetadata == DELAYED_OUT_FORMAT);
}

void IscStatement::closeCursor(CheckStatusWrapper* status, bool raise)
{
	if (statement && statement->cursor)
	{
		fb_assert(!pseudoOpened);

		statement->cursor->close(status);
		if (status->getState() & IStatus::STATE_ERRORS)
			status_exception::raise(status);

		statement->cursor = NULL;
	}
	else if (pseudoOpened)
		pseudoOpened = false;
	else if (raise)
		Arg::Gds(isc_dsql_cursor_close_err).raise();
}

void IscStatement::closeStatement(CheckStatusWrapper* status)
{
	if (statement)
	{
		statement->free(status);
		if (status->getState() & IStatus::STATE_ERRORS)
			status_exception::raise(status);

		statement = NULL;
	}
}

void IscStatement::execute(CheckStatusWrapper* status, isc_tr_handle* traHandle,
	IMessageMetadata* inMetadata, UCHAR* inBuffer, IMessageMetadata* outMetadata,
	UCHAR* outBuffer)
{
	checkCursorClosed();

	RefPtr<YTransaction> transaction;
	if (traHandle && *traHandle)
		transaction = translateHandle(transactions, traHandle);

	ITransaction* newTrans = statement->execute(status, transaction,
		inMetadata, inBuffer, outMetadata, outBuffer);

	if (!(status->getState() & IStatus::STATE_ERRORS))
	{
		if (transaction && !newTrans)
		{
			transaction->destroy(0);
			*traHandle = 0;
		}
		else if (!transaction && newTrans)
		{
			// in this case we know for sure that newTrans points to YTransaction
			if (traHandle)
				*traHandle = static_cast<YTransaction*>(newTrans)->getHandle();
		}
	}
}

FB_BOOLEAN IscStatement::fetch(CheckStatusWrapper* status, IMessageMetadata* outMetadata, UCHAR* outBuffer)
{
	checkCursorOpened();

	if (delayedFormat)
	{
		statement->cursor->setDelayedOutputFormat(status, outMetadata);

		if (status->getState() & IStatus::STATE_ERRORS)
			return FB_FALSE;

		delayedFormat = false;
	}

	return statement->cursor->fetchNext(status, outBuffer) == IStatus::RESULT_OK;
}

unsigned int YStatement::getTimeout(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YStatement> entry(status, this);
		return entry.next()->getTimeout(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}


void YStatement::setTimeout(CheckStatusWrapper* status, unsigned int timeOut)
{
	try
	{
		YEntry<YStatement> entry(status, this);
		entry.next()->setTimeout(status, timeOut);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


//-------------------------------------


YResultSet::YResultSet(YAttachment* anAttachment, YTransaction* aTransaction, IResultSet* aNext)
	: YHelper(aNext),
	  attachment(anAttachment),
	  transaction(aTransaction),
	  statement(NULL)
{
	fb_assert(aTransaction && aNext);
	aTransaction->childCursors.add(this);
}

YResultSet::YResultSet(YAttachment* anAttachment, YTransaction* aTransaction,
			YStatement* aStatement, IResultSet* aNext)
	: YHelper(aNext),
	  attachment(anAttachment),
	  transaction(aTransaction),
	  statement(aStatement)
{
	fb_assert(aTransaction && aNext);
	aTransaction->childCursors.add(this);

	MutexLockGuard guard(statement->statementMutex, FB_FUNCTION);

	if (statement->cursor)
		Arg::Gds(isc_cursor_already_open).raise();

	statement->cursor = this;
}

void YResultSet::destroy(unsigned dstrFlags)
{
	if (statement)
	{
		MutexLockGuard guard(statement->statementMutex, FB_FUNCTION);
		fb_assert(!statement->cursor || statement->cursor == this);
		statement->cursor = NULL;
	}

	YTransaction* tra = transaction.release();
	if (tra)
		tra->childCursors.remove(this);

	destroy2(dstrFlags);
}

void YStatement::setCursorName(CheckStatusWrapper* status, const char* name)
{
	try
	{
		YEntry<YStatement> entry(status, this);

		entry.next()->setCursorName(status, name);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YResultSet::setDelayedOutputFormat(CheckStatusWrapper* status, IMessageMetadata* format)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		entry.next()->setDelayedOutputFormat(status, format);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

int YResultSet::fetchNext(CheckStatusWrapper* status, void* buffer)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->fetchNext(status, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}

int YResultSet::fetchPrior(CheckStatusWrapper* status, void* buffer)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->fetchPrior(status, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}

int YResultSet::fetchFirst(CheckStatusWrapper* status, void* buffer)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->fetchFirst(status, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}

int YResultSet::fetchLast(CheckStatusWrapper* status, void* buffer)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->fetchLast(status, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}

int YResultSet::fetchAbsolute(CheckStatusWrapper* status, int position, void* buffer)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->fetchAbsolute(status, position, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}

int YResultSet::fetchRelative(CheckStatusWrapper* status, int offset, void* buffer)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->fetchRelative(status, offset, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return IStatus::RESULT_ERROR;
}

FB_BOOLEAN YResultSet::isEof(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->isEof(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return FB_FALSE;
}

FB_BOOLEAN YResultSet::isBof(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->isBof(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return FB_FALSE;
}

IMessageMetadata* YResultSet::getMetadata(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YResultSet> entry(status, this);

		return entry.next()->getMetadata(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}


void YResultSet::close(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YResultSet> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->close(status);}, [&]{entry.next()->deprecatedClose(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YResultSet::deprecatedClose(CheckStatusWrapper* status)
{
	close(status);
}


//-------------------------------------


YBatch::YBatch(YAttachment* anAttachment, IBatch* aNext)
	: YHelper(aNext),
	  attachment(anAttachment)
{ }


void YBatch::destroy(unsigned dstrFlags)
{
	destroy2(dstrFlags);
}


void YBatch::add(CheckStatusWrapper* status, unsigned count, const void* inBuffer)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->add(status, count, inBuffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


void YBatch::addBlob(CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
	unsigned parLength, const unsigned char* par)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->addBlob(status, length, inBuffer, blobId, parLength, par);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


void YBatch::appendBlobData(CheckStatusWrapper* status, unsigned length, const void* inBuffer)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->appendBlobData(status, length, inBuffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


void YBatch::addBlobStream(CheckStatusWrapper* status, unsigned length, const void* inBuffer)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->addBlobStream(status, length, inBuffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


unsigned YBatch::getBlobAlignment(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		return entry.next()->getBlobAlignment(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}


void YBatch::setDefaultBpb(CheckStatusWrapper* status, unsigned parLength, const unsigned char* par)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->setDefaultBpb(status, parLength, par);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


IMessageMetadata* YBatch::getMetadata(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		return entry.next()->getMetadata(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}


void YBatch::registerBlob(CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->registerBlob(status, existingBlob, blobId);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


IBatchCompletionState* YBatch::execute(CheckStatusWrapper* status, ITransaction* transaction)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		return entry.next()->execute(status, transaction);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}


void YBatch::cancel(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->cancel(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


void YBatch::close(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YBatch> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->close(status);}, [&]{entry.next()->deprecatedClose(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


void YBatch::deprecatedClose(CheckStatusWrapper* status)
{
	close(status);
}


void YBatch::getInfo(CheckStatusWrapper* status, unsigned int itemsLength, const unsigned char* items,
	unsigned int bufferLength, unsigned char* buffer)
{
	try
	{
		YEntry<YBatch> entry(status, this);

		entry.next()->getInfo(status, itemsLength, items, bufferLength, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


//-------------------------------------


YReplicator::YReplicator(YAttachment* anAttachment, IReplicator* aNext)
	: YHelper(aNext),
	  attachment(anAttachment)
{ }


void YReplicator::destroy(unsigned dstrFlags)
{
	destroy2(dstrFlags);
}


void YReplicator::process(CheckStatusWrapper* status, unsigned length, const unsigned char* data)
{
	try
	{
		YEntry<YReplicator> entry(status, this);
		entry.next()->process(status, length, data);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


void YReplicator::close(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YReplicator> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->close(status);}, [&]{entry.next()->deprecatedClose(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


void YReplicator::deprecatedClose(CheckStatusWrapper* status)
{
	close(status);
}


//-------------------------------------


YTransaction::YTransaction(YAttachment* aAttachment, ITransaction* aNext)
	: YHelper(aNext),
	  attachment(aAttachment),
	  childBlobs(getPool()),
	  childCursors(getPool()),
	  cleanupHandlers(getPool())
{
	if (aAttachment)
		aAttachment->childTransactions.add(this);
}

isc_tr_handle& YTransaction::getHandle()
{
	// Not assigning handle in ctor is essential for correct operation
	// of YTransaction::destroy - change it when changing handle behavior.
	if (!handle)
		makeHandle(&transactions, this, handle);
	return handle;
}

void YTransaction::destroy(unsigned dstrFlags)
{
	for (CleanupCallback** handler = cleanupHandlers.begin();
		 handler != cleanupHandlers.end();
		 ++handler)
	{
		(*handler)->cleanupCallbackFunction();
	}

	cleanupHandlers.clear();

	// If handle is not null then application works with ISC API and
	// can't release cursors by itself. See also CORE-6067.
	const bool releaseCursors = handle;

	childBlobs.destroy(dstrFlags & ~(DF_RELEASE | DF_KEEP_NEXT));
	childCursors.destroy((releaseCursors ? dstrFlags : dstrFlags & ~DF_RELEASE) & ~DF_KEEP_NEXT);

	YAttachment* att = attachment.release();
	if (att)
		att->childTransactions.remove(this);

	removeHandle(&transactions, handle);

	destroy2(dstrFlags);
}

void YTransaction::getInfo(CheckStatusWrapper* status, unsigned int itemsLength,
	const unsigned char* items, unsigned int bufferLength, unsigned char* buffer)
{
	Array<unsigned char> newItemsBuffer;

	try
	{
		YEntry<YTransaction> entry(status, this);

		fb_utils::getDbPathInfo(itemsLength, items, bufferLength, buffer,
								newItemsBuffer, attachment.get()->dbPath);

		entry.next()->getInfo(status, itemsLength, items, bufferLength, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YTransaction::prepare(CheckStatusWrapper* status, unsigned int msgLength,
	const unsigned char* message)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		entry.next()->prepare(status, msgLength, message);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YTransaction::commit(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		done(status, entry, this, [&]{entry.next()->commit(status);}, [&]{entry.next()->deprecatedCommit(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YTransaction::commitRetaining(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		entry.next()->commitRetaining(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YTransaction::rollback(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YTransaction> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{
				entry.next()->rollback(status);
				if (isNetworkError(status))
					status->init();
			}, [&]{entry.next()->deprecatedRollback(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YTransaction::rollbackRetaining(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		entry.next()->rollbackRetaining(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YTransaction::disconnect(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		/*** ASF: We must call the provider, but this makes the shutdown to crash currently.
		for (YTransaction* i = this; i; i = i->sub)
		{
			if (i->next)
			{
				i->next->disconnect(status);

				if (status->getState() & IStatus::STATE_ERRORS)
					return;

				i->next = NULL;
			}
		}
		***/

		destroy(DF_RELEASE);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YTransaction::deprecatedCommit(CheckStatusWrapper* status)
{
	commit(status);
}

void YTransaction::deprecatedRollback(CheckStatusWrapper* status)
{
	rollback(status);
}

void YTransaction::deprecatedDisconnect(CheckStatusWrapper* status)
{
	disconnect(status);
}


void YTransaction::addCleanupHandler(CheckStatusWrapper* status, CleanupCallback* callback)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		cleanupHandlers.add(callback);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

void YTransaction::selfCheck()
{
	if (!next)
		Arg::Gds(isc_bad_trans_handle).raise();
}

ITransaction* YTransaction::join(CheckStatusWrapper* status, ITransaction* transaction)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		return DtcInterfacePtr()->join(status, this, transaction);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}

ITransaction* YTransaction::validate(CheckStatusWrapper* status, IAttachment* testAtt)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		// Do not raise error in status - just return NULL if attachment does not match
		if (attachment.get() == testAtt)
			return this;

		return entry.next()->validate(status, testAtt);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}

YTransaction* YTransaction::enterDtc(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YTransaction> entry(status, this);

		YTransaction* copy = FB_NEW YTransaction(this);
		// copy is created with zero handle
		copy->addRef();
		copy->addRef();
		next->addRef();		// We use NoIncr in YTransaction ctor

		YAttachment* att = attachment.release();
		if (att)
			att->childTransactions.remove(this);

		removeHandle(&transactions, handle);
		next = NULL;
		release();

		return copy;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return NULL;
}


//-------------------------------------


YAttachment::YAttachment(IProvider* aProvider, IAttachment* aNext, const PathName& aDbPath)
	: YHelper(aNext),
	  provider(aProvider),
	  dbPath(getPool(), aDbPath),
	  childBlobs(getPool()),
	  childEvents(getPool()),
	  childRequests(getPool()),
	  childStatements(getPool()),
	  childIscStatements(getPool()),
	  childTransactions(getPool()),
	  cleanupHandlers(getPool())
{
	provider->addRef();
	makeHandle(&attachments, this, handle);
}

isc_db_handle& YAttachment::getHandle()
{
	fb_assert(handle);
	return handle;
}

YAttachment::~YAttachment()
{
	if (provider)
		PluginManagerInterfacePtr()->releasePlugin(provider);
}

void YAttachment::destroy(unsigned dstrFlags)
{
	for (CleanupCallback** handler = cleanupHandlers.begin();
		 handler != cleanupHandlers.end();
		 ++handler)
	{
		(*handler)->cleanupCallbackFunction();
	}

	cleanupHandlers.clear();

	unsigned childFlags = dstrFlags & ~(DF_KEEP_NEXT | DF_RELEASE);
	childRequests.destroy(childFlags);
	childStatements.destroy(childFlags);
	childIscStatements.destroy(childFlags);
	childBlobs.destroy(childFlags);
	childEvents.destroy(childFlags);
	childTransactions.destroy(childFlags);

	removeHandle(&attachments, handle);

	destroy2(dstrFlags);
}

void YAttachment::shutdown()
{
	if (provider)
	{
		destroy(0);
		PluginManagerInterfacePtr()->releasePlugin(provider);
		provider = NULL;
	}
}

void YAttachment::getInfo(CheckStatusWrapper* status, unsigned int itemsLength,
	const unsigned char* items, unsigned int bufferLength, unsigned char* buffer)
{
	try
	{
		YEntry<YAttachment> entry(status, this);
		entry.next()->getInfo(status, itemsLength, items, bufferLength, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

YStatement* YAttachment::prepare(CheckStatusWrapper* status, ITransaction* transaction,
	unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		if (transaction)
			getNextTransaction(status, transaction, trans);

		IStatement* st = entry.next()->prepare(status, trans, stmtLength, sqlStmt, dialect, flags);
		if (!(status->getState() & IStatus::STATE_ERRORS))
		{
			YStatement* r = FB_NEW YStatement(this, st);
			r->addRef();
			return r;
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

YTransaction* YAttachment::startTransaction(CheckStatusWrapper* status, unsigned int tpbLength,
	const unsigned char* tpb)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		ITransaction* transaction = entry.next()->startTransaction(status, tpbLength, tpb);
		if (transaction)
		{
			YTransaction* r = FB_NEW YTransaction(this, transaction);
			r->addRef();
			return r;
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

YTransaction* YAttachment::reconnectTransaction(CheckStatusWrapper* status, unsigned int length,
	const unsigned char* id)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		ITransaction* transaction = entry.next()->reconnectTransaction(status, length, id);

		if (transaction)
		{
			YTransaction* r = FB_NEW YTransaction(this, transaction);
			r->addRef();
			return r;
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

YRequest* YAttachment::compileRequest(CheckStatusWrapper* status, unsigned int blrLength,
	const unsigned char* blr)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		IRequest* request = entry.next()->compileRequest(status, blrLength, blr);
		YRequest* r = NULL;
		if (request != NULL)
		{
			r = FB_NEW YRequest(this, request);
			r->addRef();
		}
		return r;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

void YAttachment::transactRequest(CheckStatusWrapper* status, ITransaction* transaction,
	unsigned int blrLength, const unsigned char* blr,
	unsigned int inMsgLength, const unsigned char* inMsg,
	unsigned int outMsgLength, unsigned char* outMsg)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		getNextTransaction(status, transaction, trans);

		entry.next()->transactRequest(status, trans, blrLength, blr, inMsgLength, inMsg,
			outMsgLength, outMsg);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

YBlob* YAttachment::createBlob(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
	unsigned int bpbLength, const unsigned char* bpb)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		YTransaction* yTra = getTransaction(transaction);
		NextTransaction nextTra(yTra->next);

		IBlob* blob = entry.next()->createBlob(status, nextTra, id, bpbLength, bpb);
		YBlob* r = NULL;
		if (blob != NULL)
		{
			r = FB_NEW YBlob(this, yTra, blob);
			r->addRef();
		}
		return r;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

YBlob* YAttachment::openBlob(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
	unsigned int bpbLength, const unsigned char* bpb)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		YTransaction* yTra = getTransaction(transaction);
		NextTransaction nextTra(yTra->next);

		IBlob* blob = entry.next()->openBlob(status, nextTra, id, bpbLength, bpb);
		YBlob* r = NULL;
		if (blob != NULL)
		{
			r = FB_NEW YBlob(this, yTra, blob);
			r->addRef();
		}
		return r;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

int YAttachment::getSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
	unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
	const unsigned char* param, int sliceLength, unsigned char* slice)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		getNextTransaction(status, transaction, trans);

		return entry.next()->getSlice(status, trans, id, sdlLength, sdl, paramLength, param,
			sliceLength, slice);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}

void YAttachment::putSlice(CheckStatusWrapper* status, ITransaction* transaction, ISC_QUAD* id,
	unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
	const unsigned char* param, int sliceLength, unsigned char* slice)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		getNextTransaction(status, transaction, trans);

		entry.next()->putSlice(status, trans, id, sdlLength, sdl, paramLength, param, sliceLength, slice);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YAttachment::executeDyn(CheckStatusWrapper* status, ITransaction* transaction, unsigned int length,
	const unsigned char* dyn)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		getNextTransaction(status, transaction, trans);

		return entry.next()->executeDyn(status, trans, length, dyn);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

IResultSet* YAttachment::openCursor(CheckStatusWrapper* status, ITransaction* transaction,
	unsigned int length, const char* string, unsigned int dialect,
	IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata,
	const char* cursorName, unsigned int cursorFlags)
{
	IResultSet* rs = NULL;
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		if (transaction)
			getNextTransaction(status, transaction, trans);

		rs = entry.next()->openCursor(status, trans, length, string, dialect,
			inMetadata, inBuffer, outMetadata, cursorName, cursorFlags);
		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return NULL;
		}
		fb_assert(rs);

		YTransaction* const yTra = getTransaction(transaction);

		YResultSet* r = FB_NEW YResultSet(this, yTra, rs);
		r->addRef();
		return r;
	}
	catch (const Exception& e)
	{
		if (rs)
			rs->close(status);
		e.stuffException(status);
	}

	return NULL;
}

ITransaction* YAttachment::execute(CheckStatusWrapper* status, ITransaction* transaction,
	unsigned int length, const char* string, unsigned int dialect,
	IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, void* outBuffer)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		if (transaction)
			getNextTransaction(status, transaction, trans);

		ITransaction* newTrans = entry.next()->execute(status, trans, length, string, dialect,
			inMetadata, inBuffer, outMetadata, outBuffer);

		if (newTrans == trans)
			newTrans = transaction;
		else
		{
			if (transaction)
			{
				if (trans)
					trans->addRef();	// Will be decremented by release on next line
				transaction->release();
				transaction = NULL;		// Get ready for correct return in OOM case
			}

			if (newTrans)
			{
				newTrans = FB_NEW YTransaction(this, newTrans);
				newTrans->addRef();
			}
		}

		return newTrans;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return transaction;
}


void YAttachment::execute(CheckStatusWrapper* status, isc_tr_handle* traHandle,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		IMessageMetadata* inMetadata, void* inBuffer,
		IMessageMetadata* outMetadata, void* outBuffer)
{
	RefPtr<YTransaction> transaction;

	if (traHandle && *traHandle)
		transaction = translateHandle(transactions, traHandle);

	ITransaction* newTrans = execute(status, transaction, stmtLength, sqlStmt,
		dialect, inMetadata, inBuffer, outMetadata, outBuffer);

	if (!(status->getState() & IStatus::STATE_ERRORS))
	{
		if (transaction && !newTrans)
		{
			transaction->destroy(0);
			*traHandle = 0;
		}
		else if (!transaction && newTrans)
		{
			// in this case we know for sure that newTrans points to YTransaction
			if (traHandle)
				*traHandle = static_cast<YTransaction*>(newTrans)->getHandle();
		}
	}
}

YEvents* YAttachment::queEvents(CheckStatusWrapper* status, IEventCallback* callback,
	unsigned int length, const unsigned char* eventsData)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		IEvents* events = entry.next()->queEvents(status, callback, length, eventsData);
		YEvents* r = NULL;
		if (events != NULL)
		{
			r = FB_NEW YEvents(this, events, callback);
			r->addRef();
		}
		return r;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

void YAttachment::cancelOperation(CheckStatusWrapper* status, int option)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		// Mutex will be locked here for a really long time.
		MutexLockGuard guard(enterMutex, FB_FUNCTION);

		if (enterCount > 1 || option != fb_cancel_raise)
			entry.next()->cancelOperation(status, option);
		else
			status_exception::raise(Arg::Gds(isc_nothing_to_cancel));
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YAttachment::ping(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		entry.next()->ping(status);

		if (status->getState() & IStatus::STATE_ERRORS)
		{
			if (!savedStatus.getError())
				savedStatus.save(status);

			StatusVector temp(NULL);
			CheckStatusWrapper tempCheckStatusWrapper(&temp);
			entry.next()->detach(&tempCheckStatusWrapper);
			next = NULL;

			status_exception::raise(savedStatus.value());
		}
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YAttachment::detach(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YAttachment> entry(status, this, CHECK_NONE);

		done(status, entry, this, [&]{
				entry.next()->detach(status);
				if (isNetworkError(status))
					status->init();
			}, [&]{entry.next()->deprecatedDetach(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YAttachment::deprecatedDetach(CheckStatusWrapper* status)
{
	detach(status);
}

void YAttachment::dropDatabase(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		done(status, entry, this, [&]{entry.next()->dropDatabase(status);}, [&]{entry.next()->deprecatedDropDatabase(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YAttachment::deprecatedDropDatabase(CheckStatusWrapper* status)
{
	dropDatabase(status);
}

void YAttachment::addCleanupHandler(CheckStatusWrapper* status, CleanupCallback* callback)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		cleanupHandlers.add(callback);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}


YTransaction* YAttachment::getTransaction(ITransaction* tra)
{
	if (!tra)
		Arg::Gds(isc_bad_trans_handle).raise();

	// If validation is successfull, this means that this attachment and valid transaction
	// use same provider. I.e. the following cast is safe.
	FbLocalStatus status;
	YTransaction* yt = static_cast<YTransaction*>(tra->validate(&status, this));
	status.check();
	if (!yt)
		Arg::Gds(isc_bad_trans_handle).raise();

	yt->selfCheck();
	return yt;
}


void YAttachment::getNextTransaction(CheckStatusWrapper* status, ITransaction* tra,
	NextTransaction& next)
{
	next = getTransaction(tra)->next;
	if (!next.hasData())
		Arg::Gds(isc_bad_trans_handle).raise();
}


unsigned int YAttachment::getIdleTimeout(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YAttachment> entry(status, this);
		return entry.next()->getIdleTimeout(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}


void YAttachment::setIdleTimeout(CheckStatusWrapper* status, unsigned int timeOut)
{
	try
	{
		YEntry<YAttachment> entry(status, this);
		entry.next()->setIdleTimeout(status, timeOut);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


unsigned int YAttachment::getStatementTimeout(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YAttachment> entry(status, this);
		return entry.next()->getStatementTimeout(status);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return 0;
}


void YAttachment::setStatementTimeout(CheckStatusWrapper* status, unsigned int timeOut)
{
	try
	{
		YEntry<YAttachment> entry(status, this);
		entry.next()->setStatementTimeout(status, timeOut);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


YBatch* YAttachment::createBatch(CheckStatusWrapper* status, ITransaction* transaction,
	unsigned stmtLength, const char* sqlStmt, unsigned dialect,
	IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		NextTransaction trans;
		if (transaction)
			getNextTransaction(status, transaction, trans);

		IBatch* batch = entry.next()->createBatch(status, trans, stmtLength, sqlStmt, dialect,
			inMetadata, parLength, par);
		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return NULL;
		}

		YBatch*	newBatch = FB_NEW YBatch(this, batch);
		newBatch->addRef();
		return newBatch;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}


YReplicator* YAttachment::createReplicator(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YAttachment> entry(status, this);

		IReplicator* replicator = entry.next()->createReplicator(status);

		if (status->getState() & IStatus::STATE_ERRORS)
		{
			return NULL;
		}

		YReplicator* newReplicator = FB_NEW YReplicator(this, replicator);
		newReplicator->addRef();
		return newReplicator;
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}


//-------------------------------------


YService::YService(IProvider* aProvider, IService* aNext, bool utf8)
	: YHelper(aNext),
	  provider(aProvider),
	  utf8Connection(utf8)
{
	provider->addRef();
	makeHandle(&services, this, handle);
}

isc_svc_handle& YService::getHandle()
{
	fb_assert(handle);
	return handle;
}

YService::~YService()
{
	if (provider)
		PluginManagerInterfacePtr()->releasePlugin(provider);
}

void YService::destroy(unsigned dstrFlags)
{
	removeHandle(&services, handle);
	destroy2(dstrFlags);
}

void YService::shutdown()
{
	if (provider)
	{
		destroy(0);
		PluginManagerInterfacePtr()->releasePlugin(provider);
		provider = NULL;
	}
}

void YService::detach(CheckStatusWrapper* status)
{
	try
	{
		YEntry<YService> entry(status, this, CHECK_WARN_ZERO_HANDLE);

		done(status, entry, this, [&]{entry.next()->detach(status);}, [&]{entry.next()->deprecatedDetach(status);});
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YService::deprecatedDetach(CheckStatusWrapper* status)
{
	detach(status);
}

void YService::query(CheckStatusWrapper* status, unsigned int sendLength, const unsigned char* sendItems,
	unsigned int receiveLength, const unsigned char* receiveItems,
	unsigned int bufferLength, unsigned char* buffer)
{
	try
	{
		YEntry<YService> entry(status, this);
		entry.next()->query(status, sendLength, sendItems,
			receiveLength, receiveItems, bufferLength, buffer);
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}

void YService::start(CheckStatusWrapper* status, unsigned int spbLength, const unsigned char* spbItems)
{
	try
	{
		ClumpletWriter spb(ClumpletReader::SpbStart, MAX_DPB_SIZE, spbItems, spbLength);
		if (!utf8Connection)
		{
			IntlSpbStart().toUtf8(spb);
		}

		YEntry<YService> entry(status, this);
		entry.next()->start(status, spb.getBufferLength(), spb.getBuffer());
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}
}


//-------------------------------------


// Attach a database through the first subsystem that recognizes it.
YAttachment* Dispatcher::attachDatabase(CheckStatusWrapper* status, const char* filename,
	unsigned int dpbLength, const unsigned char* dpb)
{
	return attachOrCreateDatabase(status, false, filename, dpbLength, dpb);
}

// Create new database using the first subsystem that can do it.
YAttachment* Dispatcher::createDatabase(CheckStatusWrapper* status, const char* filename,
	unsigned int dpbLength, const unsigned char* dpb)
{
	return attachOrCreateDatabase(status, true, filename, dpbLength, dpb);
}

YAttachment* Dispatcher::attachOrCreateDatabase(CheckStatusWrapper* status, bool createFlag,
	const char* filename, unsigned int dpbLength, const unsigned char* dpb)
{
	try
	{
		DispatcherEntry entry(status);

		if (!filename)
			status_exception::raise(Arg::Gds(isc_bad_db_format) << Arg::Str(""));

		if (dpbLength > 0 && !dpb)
			status_exception::raise(Arg::Gds(isc_bad_dpb_form));

		ClumpletWriter newDpb(ClumpletReader::dpbList, MAX_DPB_SIZE, dpb, dpbLength);
		bool utfData = newDpb.find(isc_dpb_utf8_filename);

		// Take care about DPB
		setLogin(newDpb, false);

		if (!newDpb.find(isc_dpb_session_time_zone))
		{
			const char* defaultTimeZone = Config::getDefaultTimeZone();

			if (defaultTimeZone && defaultTimeZone[0])
				newDpb.insertString(isc_dpb_session_time_zone, defaultTimeZone);
		}

		if (!utfData)
			IntlDpb().toUtf8(newDpb);

		// Take care about filename
		PathName orgFilename(filename);
		if (utfData)
		{
			ISC_utf8ToSystem(orgFilename);
		}
		orgFilename.rtrim();

		PathName expandedFilename;
		RefPtr<const Config> config;
		if (expandDatabaseName(orgFilename, expandedFilename, &config))
		{
			expandedFilename = orgFilename;
		}

		if (newDpb.find(isc_dpb_config))
		{
			string dpb_config;
			newDpb.getString(dpb_config);
			Config::merge(config, &dpb_config);
		}

		// Convert to UTF8
		ISC_systemToUtf8(orgFilename);
		ISC_systemToUtf8(expandedFilename);

		// Add original filename to DPB
		if (orgFilename != expandedFilename && !newDpb.find(isc_dpb_org_filename))
			newDpb.insertString(isc_dpb_org_filename, orgFilename);

		StatusVector temp(NULL);
		CheckStatusWrapper tempCheckStatusWrapper(&temp);
		CheckStatusWrapper* currentStatus = status;

		for (GetPlugins<IProvider> providerIterator(IPluginManager::TYPE_PROVIDER, config);
			 providerIterator.hasData();
			 providerIterator.next())
		{
			IProvider* provider = providerIterator.plugin();

			if (cryptCallback)
			{
				provider->setDbCryptCallback(currentStatus, cryptCallback);
				if (currentStatus->getState() & IStatus::STATE_ERRORS)
					continue;
			}

			IAttachment* attachment = createFlag ?
				provider->createDatabase(currentStatus,	expandedFilename.c_str(),
					newDpb.getBufferLength(), newDpb.getBuffer()) :
				provider->attachDatabase(currentStatus,	expandedFilename.c_str(),
					newDpb.getBufferLength(), newDpb.getBuffer());

			if (!(currentStatus->getState() & IStatus::STATE_ERRORS))
			{
				if (createFlag)
				{
					config->notify();
#ifdef WIN_NT
	            	// Now we can expand, the file exists
					ISC_utf8ToSystem(orgFilename);
					if (expandDatabaseName(orgFilename, expandedFilename, NULL))
					{
						expandedFilename = orgFilename;
					}
					ISC_systemToUtf8(expandedFilename);
#endif
				}

				if (status != currentStatus)
				{
					status->setErrors(currentStatus->getErrors());
					status->setWarnings(currentStatus->getWarnings());
				}
				YAttachment* r = FB_NEW YAttachment(provider, attachment, expandedFilename);
				r->addRef();
				return r;
			}

			switch (currentStatus->getErrors()[1])
			{
			case isc_io_error:
			case isc_lock_dir_access:
			case isc_no_priv:
			case isc_wrong_ods:
				currentStatus = &tempCheckStatusWrapper;
				// fall down...
			case isc_unavailable:
				break;

			default:
				return NULL;
			}

			currentStatus->init();
		}

		if (status->getErrors()[1] == 0)
			Arg::Gds(isc_unavailable).raise();
	}
	catch (const Exception& e)
	{
		e.stuffException(status);
	}

	return NULL;
}

// Attach a service through the first subsystem that recognizes it.
YService* Dispatcher::attachServiceManager(CheckStatusWrapper* status, const char* serviceName,
	unsigned int spbLength, const unsigned char* spb)
{
	IService* service = NULL;
	status->init();

	try
	{
		DispatcherEntry entry(status);

		if (!serviceName)
			status_exception::raise(Arg::Gds(isc_service_att_err) << Arg::Gds(isc_svc_name_missing));

		if (spbLength > 0 && !spb)
			status_exception::raise(Arg::Gds(isc_bad_spb_form) <<
 									Arg::Gds(isc_null_spb));

		PathName svcName(serviceName);
		svcName.trim();

		ClumpletWriter spbWriter(ClumpletReader::spbList, MAX_DPB_SIZE, spb, spbLength);
		bool utfData = spbWriter.find(isc_spb_utf8_filename);

		// Take care about SPB
		setLogin(spbWriter, true);
		if (!utfData)
		{
			IntlSpb().toUtf8(spbWriter);
		}

		// Build correct config
		RefPtr<const Config> config(Config::getDefaultConfig());
		if (spbWriter.find(isc_spb_config))
		{
			string spb_config;
			spbWriter.getString(spb_config);
			Config::merge(config, &spb_config);
		}

		StatusVector temp(NULL);
		CheckStatusWrapper tempCheckStatusWrapper(&temp);
		CheckStatusWrapper* currentStatus = status;

		for (GetPlugins<IProvider> providerIterator(IPluginManager::TYPE_PROVIDER, config);
			 providerIterator.hasData();
			 providerIterator.next())
		{
			IProvider* p = providerIterator.plugin();

			if (cryptCallback)
			{
				p->setDbCryptCallback(currentStatus, cryptCallback);
				if (currentStatus->getState() & IStatus::STATE_ERRORS)
					continue;
			}

			service = p->attachServiceManager(currentStatus, svcName.c_str(),
				spbWriter.getBufferLength(), spbWriter.getBuffer());

			if (!(currentStatus->getState() & IStatus::STATE_ERRORS))
			{
				if (status != currentStatus)
				{
					status->setErrors(currentStatus->getErrors());
					status->setWarnings(currentStatus->getWarnings());
				}
				YService* r = FB_NEW YService(p, service, utfData);
				r->addRef();
				return r;
			}

			switch (currentStatus->getErrors()[1])
			{
				case isc_service_att_err:
					currentStatus = &tempCheckStatusWrapper;
					// fall down...
				case isc_unavailable:
					break;

				default:
					return NULL;
			}

			currentStatus->init();
		}

		if (!(status->getState() & IStatus::STATE_ERRORS))
		{
			(Arg::Gds(isc_service_att_err) <<
			 Arg::Gds(isc_no_providers)).copyTo(status);
		}
	}
	catch (const Exception& e)
	{
		if (service)
		{
			StatusVector temp(NULL);
			CheckStatusWrapper tempCheckStatusWrapper(&temp);
			service->detach(&tempCheckStatusWrapper);
		}

		e.stuffException(status);
	}

	return NULL;
}

void Dispatcher::shutdown(CheckStatusWrapper* userStatus, unsigned int timeout, const int reason)
{
	// set "process exiting" state
	if (reason == fb_shutrsn_emergency)
		abortShutdown();

	// can't syncronize with already killed threads, just exit
	if (MasterInterfacePtr()->getProcessExiting())
		return;

	try
	{
		DispatcherEntry entry(userStatus, true);

		static GlobalPtr<Mutex> singleShutdown;
		MutexLockGuard guard(singleShutdown, FB_FUNCTION);

		if (shutdownStarted)
			return;

		StatusVector status(NULL);
		CheckStatusWrapper statusWrapper(&status);

#ifdef DEV_BUILD
		// Make timeout huge in debug build: hard to debug something during 5-10 sec
		if (timeout)
			timeout = 10 * 3600 * 1000;		// 10 hours
#endif

		// In some cases it's OK to stop plugin manager even now.
		if (reason == fb_shutrsn_exit_called)
			PluginManager::shutdown();

		const Arg::Gds error(isc_att_shutdown);	//// TODO: review

		// Ask clients about shutdown confirmation.
		if (ShutChain::run(fb_shut_confirmation, reason) != FB_SUCCESS)
		{
			userStatus->setErrors(error.value());
			return;	// Do not perform former shutdown
		}

		// Shut known components, internal for libfbclient
		PluginManager::shutdown();
		shutdownTimers();

		// Shutdown clients before providers
		if (ShutChain::run(fb_shut_preproviders, reason) != FB_SUCCESS)
			userStatus->setErrors(error.value());

		// Shutdown yValve
		// Since this moment no new thread will be able to enter yValve.
		// Existing threads continue to run inside it - later do our best to close them.
		shutdownStarted = true;

		// Shutdown providers (if any present).
		for (GetPlugins<IProvider> providerIterator(IPluginManager::TYPE_PROVIDER);
			 providerIterator.hasData();
			 providerIterator.next())
		{
			IProvider* provider = providerIterator.plugin();

			StatusVector status2(NULL);
			CheckStatusWrapper statusWrapper2(&status2);
			provider->shutdown(&statusWrapper2, timeout, reason);

			if (status2[1])
				userStatus->setErrors(error.value());
		}

		// Close all known interfaces from providers...
		for (bool hasThreads = true; hasThreads; )
		{
			Thread::yield();

			if (dispCounter.value() > 0)
			{
				continue;
			}

			hasThreads = false;

			Stack<YService*, 64> svcStack;
			{
				WriteLockGuard sync(handleMappingLock, FB_FUNCTION);
				GenericMap<Pair<NonPooled<isc_svc_handle, YService*> > >::Accessor accessor(&services);

				if (accessor.getFirst())
				{
					do
					{
						YService* service = accessor.current()->second;
						if (service->enterCount)
							hasThreads = true;
						else
						{
							service->addRef();
							svcStack.push(service);
						}
					} while (accessor.getNext());
				}
			}

			while (svcStack.hasData())
			{
				YService* service = svcStack.pop();
				service->shutdown();
				service->release();
			}

			if (hasThreads)
				continue;

			Stack<YAttachment*, 64> attStack;
			{
				WriteLockGuard sync(handleMappingLock, FB_FUNCTION);
				GenericMap<Pair<NonPooled<isc_db_handle, YAttachment*> > >::Accessor accessor(&attachments);

				if (accessor.getFirst())
				{
					do
					{
						YAttachment* attachment = accessor.current()->second;
						if (attachment->enterCount)
							hasThreads = true;
						else
						{
							attachment->addRef();
							attStack.push(attachment);
						}
					} while (accessor.getNext());
				}
			}

			while (attStack.hasData())
			{
				YAttachment* attachment = attStack.pop();
				attachment->shutdown();
				attachment->release();
			}

		}

		// ... and wait for all providers to go away
		PluginManager::waitForType(IPluginManager::TYPE_PROVIDER);

		// Shutdown clients after providers.
		if (ShutChain::run(fb_shut_postproviders, reason) != FB_SUCCESS)
			userStatus->setErrors(error.value());

		// Finish shutdown.
		if (ShutChain::run(fb_shut_finish, reason) != FB_SUCCESS)
			userStatus->setErrors(error.value());

		// At this step callbacks are welcome to exit (or take actions to make main thread do it).
		if (ShutChain::run(fb_shut_exit, reason) != FB_SUCCESS)
			userStatus->setErrors(error.value());
	}
	catch (const Exception& e)
	{
		e.stuffException(userStatus);
		iscLogStatus(NULL, userStatus);
	}
}

void Dispatcher::setDbCryptCallback(CheckStatusWrapper* status, ICryptKeyCallback* callback)
{
	status->init();
	cryptCallback = callback;
}

} // namespace Why
