/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		remote.h
 *	DESCRIPTION:	Common descriptions
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
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#ifndef REMOTE_REMOTE_H
#define REMOTE_REMOTE_H

#include "gen/iberror.h"
#include "../remote/remote_def.h"
#include "../common/ThreadData.h"
#include "../common/ThreadStart.h"
#include "../common/Auth.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/RefMutex.h"
#include "../common/StatusHolder.h"
#include "../common/classes/RefCounted.h"
#include "../common/classes/GetPlugins.h"
#include "../common/classes/RefMutex.h"

#include "firebird/Interface.h"

#ifndef WIN_NT
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  -1
#endif
#endif // !WIN_NT

#if defined(HAVE_ZLIB_H)
#define WIRE_COMPRESS_SUPPORT 1
//#define COMPRESS_DEBUG 1
#include "../common/classes/zip.h"
#endif

#define DEB_RBATCH(x)	((void) 0)

#define REM_SEND_OFFSET(bs) (0)
#define REM_RECV_OFFSET(bs) (bs)

// Uncomment this line if you need to trace module activity
//#define REMOTE_DEBUG

#ifdef REMOTE_DEBUG
DEFINE_TRACE_ROUTINE(remote_trace);
#define REMOTE_TRACE(args) remote_trace args
#else
#define REMOTE_TRACE(args) // nothing
#endif

#ifdef DEV_BUILD
// Debug packet/XDR memory allocation

// Temporarily disabling DEBUG_XDR_MEMORY
// #define DEBUG_XDR_MEMORY

#endif

const int BLOB_LENGTH		= 16384;

#include "../remote/protocol.h"
#include "fb_blk.h"
#include "firebird/Interface.h"

// Prefetch constants

const ULONG MAX_PACKETS_PER_BATCH = 16;

const ULONG MIN_ROWS_PER_BATCH = 10;
const ULONG MAX_ROWS_PER_BATCH = 1000;

const ULONG MAX_BATCH_CACHE_SIZE = 1024 * 1024; // 1 MB

// fwd. decl.
namespace Firebird {
	class Exception;
	class BatchCompletionState;
}

#ifdef WIN_NT
#include <WinSock2.h>
#else
typedef int SOCKET;
#endif

namespace os_utils
{
	// force descriptor to have O_CLOEXEC set
	SOCKET socket(int domain, int type, int protocol);
	SOCKET accept(SOCKET sockfd, sockaddr *addr, socklen_t *addrlen);
}

struct rem_port;

typedef Firebird::AutoPtr<UCHAR, Firebird::ArrayDelete > UCharArrayAutoPtr;

typedef Firebird::RefPtr<Firebird::IAttachment> ServAttachment;
typedef Firebird::RefPtr<Firebird::IBlob> ServBlob;
typedef Firebird::RefPtr<Firebird::ITransaction> ServTransaction;
typedef Firebird::RefPtr<Firebird::IStatement> ServStatement;
typedef Firebird::RefPtr<Firebird::IResultSet> ServCursor;
typedef Firebird::RefPtr<Firebird::IBatch> ServBatch;
typedef Firebird::RefPtr<Firebird::IRequest> ServRequest;
typedef Firebird::RefPtr<Firebird::IEvents> ServEvents;
typedef Firebird::RefPtr<Firebird::IService> ServService;


// this set of parameters helps using same functions
// for both services and databases attachments
struct ParametersSet
{
	UCHAR dummy_packet_interval, user_name, auth_block,
		  password, password_enc, trusted_auth,
		  plugin_name, plugin_list, specific_data,
		  address_path, process_id, process_name,
		  encrypt_key, client_version, remote_protocol,
		  host_name, os_user, config_text,
		  utf8_filename, map_attach;
};

extern const ParametersSet dpbParam, spbParam, connectParam;


struct Svc : public Firebird::GlobalStorage
{
	ServService					svc_iface;		// service interface
	Svc() :
		svc_iface(NULL)
	{ }
};


struct Rdb : public Firebird::GlobalStorage, public TypedHandle<rem_type_rdb>
{
	ServAttachment	rdb_iface;				// attachment interface
	rem_port*		rdb_port;				// communication port
	Firebird::AutoPtr<Svc>	rdb_svc;		// service-specific block
	struct Rtr*		rdb_transactions;		// linked list of transactions
	struct Rrq*		rdb_requests;			// compiled requests
	struct Rvnt*	rdb_events;				// known events
	struct Rsr*		rdb_sql_requests;		// SQL requests
	PACKET			rdb_packet;				// Communication structure
	USHORT			rdb_id;

private:
	ThreadId		rdb_async_thread_id;	// Id of async thread (when active)

public:
	Firebird::Mutex	rdb_async_lock;			// Sync to avoid 2 async calls at once

public:
	Rdb() :
		rdb_iface(NULL), rdb_port(0),
		rdb_transactions(0), rdb_requests(0), rdb_events(0), rdb_sql_requests(0),
		rdb_id(0), rdb_async_thread_id(0)
	{
	}

	static ISC_STATUS badHandle() { return isc_bad_db_handle; }
};


struct Rtr : public Firebird::GlobalStorage, public TypedHandle<rem_type_rtr>
{
	Rdb*			rtr_rdb;
	Rtr*			rtr_next;
	struct Rbl*		rtr_blobs;
	ServTransaction	rtr_iface;
	USHORT			rtr_id;
	bool			rtr_limbo;

	Firebird::Array<Rsr*> rtr_cursors;
	Rtr**			rtr_self;

public:
	Rtr() :
		rtr_rdb(0), rtr_next(0), rtr_blobs(0),
		rtr_iface(NULL), rtr_id(0), rtr_limbo(0),
		rtr_cursors(getPool()), rtr_self(NULL)
	{ }

	~Rtr()
	{
		if (rtr_self && *rtr_self == this)
			*rtr_self = NULL;
	}

	static ISC_STATUS badHandle() { return isc_bad_trans_handle; }
};


struct Rbl : public Firebird::GlobalStorage, public TypedHandle<rem_type_rbl>
{
	Firebird::HalfStaticArray<UCHAR, BLOB_LENGTH> rbl_data;
	Rdb*		rbl_rdb;
	Rtr*		rbl_rtr;
	Rbl*		rbl_next;
	UCHAR*		rbl_buffer;
	UCHAR*		rbl_ptr;
	ServBlob	rbl_iface;
	SLONG		rbl_offset;			// Apparent (to user) offset in blob
	USHORT		rbl_id;
	USHORT		rbl_flags;
	USHORT		rbl_buffer_length;
	USHORT		rbl_length;
	USHORT		rbl_fragment_length;
	USHORT		rbl_source_interp;	// source interp (for writing)
	USHORT		rbl_target_interp;	// destination interp (for reading)
	Rbl**		rbl_self;

public:
	// Values for rbl_flags
	enum {
		EOF_SET = 1,
		SEGMENT = 2,
		EOF_PENDING = 4,
		CREATE = 8
	};

public:
	Rbl() :
		rbl_data(getPool()), rbl_rdb(0), rbl_rtr(0), rbl_next(0),
		rbl_buffer(rbl_data.getBuffer(BLOB_LENGTH)), rbl_ptr(rbl_buffer), rbl_iface(NULL),
		rbl_offset(0), rbl_id(0), rbl_flags(0),
		rbl_buffer_length(BLOB_LENGTH), rbl_length(0), rbl_fragment_length(0),
		rbl_source_interp(0), rbl_target_interp(0), rbl_self(NULL)
	{ }

	~Rbl()
	{
		if (rbl_self && *rbl_self == this)
			*rbl_self = NULL;

		if (rbl_iface)
			rbl_iface->release();
	}

	static ISC_STATUS badHandle() { return isc_bad_segstr_handle; }
};


struct Rvnt : public Firebird::GlobalStorage, public TypedHandle<rem_type_rev>
{
	Rvnt*		rvnt_next;
	Rdb*		rvnt_rdb;
	Firebird::RefPtr<Firebird::IEventCallback> rvnt_callback;
	ServEvents	rvnt_iface;
	rem_port*	rvnt_port;	// used to id server from whence async came
	SLONG		rvnt_id;	// used to store client-side id
	USHORT		rvnt_length;
	Rvnt**		rvnt_self;
	Firebird::AtomicCounter rvnt_destroyed;

public:
	Rvnt() :
		rvnt_next(NULL), rvnt_rdb(NULL), rvnt_callback(NULL), rvnt_iface(NULL),
		rvnt_port(NULL), rvnt_id(0), rvnt_length(0), rvnt_self(NULL)
	{ }

	~Rvnt()
	{
		if (rvnt_self && *rvnt_self == this)
			*rvnt_self = NULL;
	}
};


struct rem_str : public pool_alloc_rpt<SCHAR>
{
	USHORT		str_length;
	SCHAR		str_data[2];
};


// Include definition of descriptor

#include "../common/dsc.h"


struct rem_fmt : public Firebird::GlobalStorage
{
	ULONG		fmt_length;
	ULONG		fmt_net_length;
	Firebird::Array<dsc> fmt_desc;

public:
	explicit rem_fmt(FB_SIZE_T rpt) :
		fmt_length(0), fmt_net_length(0),
		fmt_desc(getPool(), rpt)
	{
		fmt_desc.grow(rpt);
	}
};

// Windows declares a msg structure, so rename the structure
// to avoid overlap problems.

struct RMessage : public Firebird::GlobalStorage
{
	RMessage*	msg_next;			// Next available message
	USHORT		msg_number;			// Message number
	UCHAR*		msg_address;		// Address of message
	UCharArrayAutoPtr msg_buffer;	// Allocated message

public:
	explicit RMessage(size_t rpt) :
		msg_next(0), msg_number(0), msg_address(0), msg_buffer(FB_NEW_POOL(getPool()) UCHAR[rpt])
	{
		memset(msg_buffer, 0, rpt);
	}
};


// remote stored procedure request
struct Rpr : public Firebird::GlobalStorage
{
	Rdb*		rpr_rdb;
	Rtr*		rpr_rtr;
	RMessage*	rpr_in_msg;		// input message
	RMessage*	rpr_out_msg;	// output message
	rem_fmt*	rpr_in_format;	// Format of input message
	rem_fmt*	rpr_out_format;	// Format of output message

public:
	Rpr() :
		rpr_rdb(0), rpr_rtr(0),
		rpr_in_msg(0), rpr_out_msg(0), rpr_in_format(0), rpr_out_format(0)
	{ }
};

struct Rrq : public Firebird::GlobalStorage, public TypedHandle<rem_type_rrq>
{
	Rdb*	rrq_rdb;
	Rtr*	rrq_rtr;
	Rrq*	rrq_next;
	Rrq*	rrq_levels;		// RRQ block for next level
	ServRequest rrq_iface;
	USHORT		rrq_id;
	USHORT		rrq_max_msg;
	USHORT		rrq_level;
	Firebird::StatusHolder	rrqStatus;

	struct		rrq_repeat
	{
		rem_fmt*	rrq_format;		// format for this message
		RMessage*	rrq_message; 	// beginning or end of cache, depending on whether it is client or server
		RMessage*	rrq_xdr;		// point at which cache is read or written by xdr
		USHORT		rrq_msgs_waiting;	// count of full rrq_messages
		USHORT		rrq_rows_pending;	// How many rows in waiting
		USHORT		rrq_reorder_level;	// Reorder when rows_pending < this level
		USHORT		rrq_batch_count;	// Count of batches in pipeline

	};
	Firebird::Array<rrq_repeat> rrq_rpt;
	Rrq**	rrq_self;

public:
	explicit Rrq(FB_SIZE_T rpt) :
		rrq_rdb(0), rrq_rtr(0), rrq_next(0), rrq_levels(0),
		rrq_iface(NULL), rrq_id(0), rrq_max_msg(0), rrq_level(0),
		rrq_rpt(getPool(), rpt), rrq_self(NULL)
	{
		//memset(rrq_status_vector, 0, sizeof rrq_status_vector);
		rrq_rpt.grow(rpt);
	}

	~Rrq()
	{
		if (rrq_self && *rrq_self == this)
			*rrq_self = NULL;

		if (rrq_iface)
			rrq_iface->release();
	}

	Rrq* clone() const
	{
		Rrq* rc = FB_NEW Rrq(rrq_rpt.getCount());
		*rc = *this;
		rc->rrq_self = NULL;
		return rc;
	}

	static ISC_STATUS badHandle() { return isc_bad_req_handle; }

	void saveStatus(const Firebird::Exception& ex) throw();
	void saveStatus(Firebird::IStatus* ex) throw();
};


template <typename T>
class RFlags
{
public:
	RFlags() :
		m_flags(0)
	{
		// Require base flags field to be unsigned.
		// This is a compile-time assertion; it won't build if you use a signed flags field.
		typedef int dummy[T(-1) > 0];
	}
	explicit RFlags(const T flags) :
		m_flags(flags)
	{}
	// At least one bit in the parameter is 1 in the object.
	bool test(const T flags) const
	{
		return m_flags & flags;
	}
	// All bits received as parameter are 1 in the object.
	bool testAll(const T flags) const
	{
		return (m_flags & flags) == flags;
	}
	void set(const T flags)
	{
		m_flags |= flags;
	}
	void clear(const T flags)
	{
		m_flags &= ~flags;
	}
	void reset()
	{
		m_flags = 0;
	}
private:
	T m_flags;
};


// remote SQL request
struct Rsr : public Firebird::GlobalStorage, public TypedHandle<rem_type_rsr>
{
	Rsr*			rsr_next;
	Rdb*			rsr_rdb;
	Rtr*			rsr_rtr;
	ServStatement	rsr_iface;
	ServCursor		rsr_cursor;
	ServBatch		rsr_batch;
	rem_fmt*		rsr_bind_format;		// Format of bind message
	rem_fmt*		rsr_select_format;		// Format of select message
	rem_fmt*		rsr_user_select_format; // Format of user's select message
	rem_fmt*		rsr_format;				// Format of current message
	RMessage*		rsr_message;			// Next message to process
	RMessage*		rsr_buffer;				// Next buffer to use
	Firebird::StatusHolder* rsr_status;		// saved status for buffered errors
	USHORT			rsr_id;
	RFlags<USHORT>	rsr_flags;
	ULONG			rsr_fmt_length;

	ULONG			rsr_rows_pending;	// How many rows are pending
	USHORT			rsr_msgs_waiting; 	// count of full rsr_messages
	USHORT			rsr_reorder_level; 	// Trigger pipelining at this level
	USHORT			rsr_batch_count; 	// Count of batches in pipeline

	Firebird::string rsr_cursor_name;	// Name for cursor to be set on open
	bool			rsr_delayed_format;	// Out format was delayed on execute, set it on fetch
	unsigned int	rsr_timeout;		// Statement timeout to be set on open\execute
	Rsr**			rsr_self;

	ULONG			rsr_batch_size;		// Aligned message size for IBatch operations
	ULONG			rsr_batch_flags;	// Flags for batch processing
	union								// BatchCS passed to XDR protocol
	{
		Firebird::IBatchCompletionState* rsr_batch_ics;	// server
		Firebird::BatchCompletionState* rsr_batch_cs;	// client
	};

	struct BatchStream
	{
		BatchStream()
			: curBpb(*getDefaultMemoryPool()), hdrPrevious(0), segmented(false)
		{ }

		static const ULONG SIZEOF_BLOB_HEAD = sizeof(ISC_QUAD) + 2 * sizeof(ULONG);

		typedef Firebird::HalfStaticArray<UCHAR, 64> Bpb;
		Bpb curBpb;
		UCHAR hdr[SIZEOF_BLOB_HEAD];
		ULONG blobRemaining;			// Remaining to transfer size of blob data
		ULONG bpbRemaining;				// Remaining to transfer size of BPB
		ULONG segRemaining;				// Remaining to transfer size of segment data
		USHORT alignment;				// Alignment in BLOB stream
		USHORT hdrPrevious;				// Header data left from previous block (in hdr)
		bool segmented;					// Current blob kind

		void saveData(const UCHAR* data, ULONG size)
		{
			fb_assert(size + hdrPrevious <= SIZEOF_BLOB_HEAD);
			memcpy(&hdr[hdrPrevious], data, size);
			hdrPrevious += size;
		}
	};
	BatchStream		rsr_batch_stream;

public:
	// Values for rsr_flags.
	enum {
		FETCHED = 1,		// Cleared by execute, set by fetch
		EOF_SET = 2,		// End-of-stream encountered
		//BLOB = 4,			// Statement relates to blob op
		NO_BATCH = 8,		// Do not batch fetch rows
		STREAM_ERR = 16,	// There is an error pending in the batched rows
		LAZY = 32,			// To be allocated at the first reference
		DEFER_EXECUTE = 64,	// op_execute can be deferred
		PAST_EOF = 128		// EOF was returned by fetch from this statement
	};

public:
	Rsr() :
		rsr_next(0), rsr_rdb(0), rsr_rtr(0), rsr_iface(NULL), rsr_cursor(NULL), rsr_batch(NULL),
		rsr_bind_format(0), rsr_select_format(0), rsr_user_select_format(0),
		rsr_format(0), rsr_message(0), rsr_buffer(0), rsr_status(0),
		rsr_id(0), rsr_fmt_length(0),
		rsr_rows_pending(0), rsr_msgs_waiting(0), rsr_reorder_level(0), rsr_batch_count(0),
		rsr_cursor_name(getPool()), rsr_delayed_format(false), rsr_timeout(0), rsr_self(NULL)
	{ }

	~Rsr()
	{
		if (rsr_self && *rsr_self == this)
			*rsr_self = NULL;

		if (rsr_cursor)
			rsr_cursor->release();

		if (rsr_batch)
			rsr_batch->release();

		if (rsr_iface)
			rsr_iface->release();

		delete rsr_status;
	}

	void saveException(Firebird::IStatus* status, bool overwrite);
	void saveException(const Firebird::Exception& ex, bool overwrite);
	void clearException();
	ISC_STATUS haveException();
	void raiseException();
	void releaseException();

	static ISC_STATUS badHandle() { return isc_bad_req_handle; }
	void checkIface(ISC_STATUS code = isc_unprepared_stmt);
	void checkCursor();
	void checkBatch();
};


// Makes it possible to safely store all handles in single array
class RemoteObject
{
private:
	union {
		Rdb* rdb;
		Rtr* rtr;
		Rbl* rbl;
		Rrq* rrq;
		Rsr* rsr;
	} ptr;

public:
	RemoteObject() { ptr.rdb = 0; }

	template <typename R>
	R* get(R* r)
	{
		if (!r || !r->checkHandle())
		{
			Firebird::status_exception::raise(Firebird::Arg::Gds(R::badHandle()));
		}
		return r;
	}

	void operator=(Rdb* v) { ptr.rdb = v; }
	void operator=(Rtr* v) { ptr.rtr = v; }
	void operator=(Rbl* v) { ptr.rbl = v; }
	void operator=(Rrq* v) { ptr.rrq = v; }
	void operator=(Rsr* v) { ptr.rsr = v; }

	operator Rdb*() { return get(ptr.rdb); }
	operator Rtr*() { return get(ptr.rtr); }
	operator Rbl*() { return get(ptr.rbl); }
	operator Rrq*() { return get(ptr.rrq); }
	operator Rsr*() { return get(ptr.rsr); }

	bool isMissing() const { return ptr.rdb == NULL; }
	void release() { ptr.rdb = 0; }
};



inline void Rsr::saveException(Firebird::IStatus* status, bool overwrite)
{
	if (!rsr_status) {
		rsr_status = FB_NEW Firebird::StatusHolder();
	}
	if (overwrite || !rsr_status->getError()) {
		rsr_status->save(status);
	}
}

inline void Rsr::clearException()
{
	if (rsr_status)
		rsr_status->clear();
}

inline ISC_STATUS Rsr::haveException()
{
	return (rsr_status ? rsr_status->getError() : 0);
}

inline void Rsr::raiseException()
{
	if (rsr_status)
		rsr_status->raise();
}

inline void Rsr::releaseException()
{
	delete rsr_status;
	rsr_status = NULL;
}

#include "../common/xdr.h"


// Generalized port definition.

//////////////////////////////////////////////////////////////////
// fwd. decl.
struct p_cnct;
struct rmtque;
struct xcc; // defined in xnet.h

// Queue of deferred packets

struct rem_que_packet
{
	PACKET packet;
	bool sent;
};

typedef Firebird::Array<rem_que_packet> PacketQueue;

class ServerAuthBase
{
public:
	enum AuthenticateFlags {
		NO_FLAGS =			0x0,
		CONT_AUTH =			0x1,
		USE_COND_ACCEPT =	0x2
	};

	virtual ~ServerAuthBase();
	virtual bool authenticate(PACKET* send, AuthenticateFlags flags = NO_FLAGS) = 0;
};

class ServerCallbackBase
{
public:
	virtual ~ServerCallbackBase();
	virtual void wakeup(unsigned int length, const void* data) = 0;
	virtual Firebird::ICryptKeyCallback* getInterface() = 0;
	virtual void stop() = 0;
};

// CryptKey implementation
class InternalCryptKey FB_FINAL :
	public Firebird::VersionedIface<Firebird::ICryptKeyImpl<InternalCryptKey, Firebird::CheckStatusWrapper> >,
	public Firebird::GlobalStorage
{
public:
	InternalCryptKey()
		: t(getPool())
	{ }

	// ICryptKey implementation
	void setSymmetric(Firebird::CheckStatusWrapper* status, const char* type, unsigned keyLength, const void* key);
	void setAsymmetric(Firebird::CheckStatusWrapper* status, const char* type, unsigned encryptKeyLength,
		const void* encryptKey, unsigned decryptKeyLength, const void* decryptKey);
	const void* getEncryptKey(unsigned* length);
	const void* getDecryptKey(unsigned* length);

	class Key : public Firebird::UCharBuffer
	{
	public:
		Key()
			: Firebird::UCharBuffer(getPool())
		{ }

		void set(unsigned keyLength, const void* key)
		{
			assign(static_cast<const UCHAR*>(key), keyLength);
		}

		const void* get(unsigned* length) const
		{
			if (getCount() > 0)
			{
				if (length)
					*length = getCount();
				return begin();
			}
			return NULL;
		}
	};

	Key encrypt, decrypt;
	Firebird::PathName t;
};


typedef Firebird::GetPlugins<Firebird::IClient> AuthClientPlugins;

// Representation of authentication data, visible for plugin
// Transfered in format, depending upon type of the packet (phase of handshake)
class ClntAuthBlock FB_FINAL :
	public Firebird::RefCntIface<Firebird::IClientBlockImpl<ClntAuthBlock, Firebird::CheckStatusWrapper> >
{
private:
	Firebird::PathName pluginList;				// To be passed to server
	Firebird::PathName serverPluginList;		// Received from server
	Firebird::string cliUserName, cliPassword;	// Used by plugin, taken from DPB
	Firebird::string cliOrigUserName;			// Original user name, passed to server
	// These two are legacy encrypted password, trusted auth data and so on - what plugin needs
	Firebird::UCharBuffer dataForPlugin, dataFromPlugin;
	Firebird::HalfStaticArray<InternalCryptKey*, 1> cryptKeys;		// Wire crypt keys that came from plugin(s) last time
	Firebird::string dpbConfig;				// User's configuration parameters
	Firebird::PathName dpbPlugins;			// User's plugin list
	Firebird::RefPtr<const Config> clntConfig;	// Used to get plugins list and pass to port
	unsigned nextKey;						// First key to be analyzed

public:
	AuthClientPlugins plugins;
	bool authComplete;						// Set as response from client that authentication accepted
	bool firstTime;							// Invoked first time after reset

	ClntAuthBlock(const Firebird::PathName* fileName, Firebird::ClumpletReader* dpb,
		const ParametersSet* tags);

	~ClntAuthBlock()
	{
		releaseKeys(0);
	}

	void storeDataForPlugin(unsigned int length, const unsigned char* data);
	void resetDataFromPlugin();
	void extractDataFromPluginTo(Firebird::ClumpletWriter& dpb, const ParametersSet* tags, int protocol);
	void extractDataFromPluginTo(CSTRING* to);
	void extractDataFromPluginTo(P_AUTH_CONT* to);
	void loadClnt(Firebird::ClumpletWriter& dpb, const ParametersSet*);
	void extractDataFromPluginTo(Firebird::ClumpletWriter& user_id);
	void resetClnt(const Firebird::PathName* fileName, const CSTRING* listStr = NULL);
	bool checkPluginName(Firebird::PathName& nameToCheck);
	Firebird::PathName getPluginName();
	void tryNewKeys(rem_port*);
	void releaseKeys(unsigned from);
	Firebird::RefPtr<const Config>* getConfig();

	// Firebird::IClientBlock implementation
	int release();
	const char* getLogin();
	const char* getPassword();
	const unsigned char* getData(unsigned int* length);
	void putData(Firebird::CheckStatusWrapper* status, unsigned int length, const void* data);
	Firebird::ICryptKey* newKey(Firebird::CheckStatusWrapper* status);
};

// Representation of authentication data, visible for plugin
// Transfered from client data in format, suitable for plugins access
typedef Firebird::GetPlugins<Firebird::IServer> AuthServerPlugins;

class SrvAuthBlock FB_FINAL :
	public Firebird::VersionedIface<Firebird::IServerBlockImpl<SrvAuthBlock, Firebird::CheckStatusWrapper> >,
	public Firebird::GlobalStorage
{
private:
	rem_port* port;
	Firebird::string userName;
	Firebird::PathName pluginName, pluginList;
	// These two may be legacy encrypted password, trusted auth data and so on
	Firebird::UCharBuffer dataForPlugin, dataFromPlugin;
	Firebird::ClumpletWriter lastExtractedKeys;
	Firebird::HalfStaticArray<InternalCryptKey*, 8> newKeys;
	bool flComplete, firstTime;

public:
	AuthServerPlugins* plugins;
	Auth::WriterImplementation authBlockWriter;

	// extractNewKeys flags
	static const ULONG EXTRACT_PLUGINS_LIST = 0x1;
	static const ULONG ONLY_CLEANUP = 0x2;

	explicit SrvAuthBlock(rem_port* p_port)
		: port(p_port),
		  userName(getPool()), pluginName(getPool()), pluginList(getPool()),
		  dataForPlugin(getPool()), dataFromPlugin(getPool()),
		  lastExtractedKeys(getPool(), Firebird::ClumpletReader::UnTagged, MAX_DPB_SIZE),
		  newKeys(getPool()),
		  flComplete(false), firstTime(true),
		  plugins(NULL)
	{
	}

	~SrvAuthBlock()
	{
		delete plugins;
	}

	void extractDataFromPluginTo(cstring* to);
	void extractDataFromPluginTo(P_AUTH_CONT* to);
	void extractDataFromPluginTo(P_ACPD* to);
	bool authCompleted(bool flag = false);
	void setLogin(const Firebird::string& user);
	void load(Firebird::ClumpletReader& userId);
	const char* getPluginName();
	void setPluginList(const Firebird::string& name);
	const char* getPluginList();
	void setPluginName(const Firebird::string& name);
	void extractPluginName(cstring* to);
	void setDataForPlugin(const Firebird::UCharBuffer& data);
	void setDataForPlugin(const cstring& data);
	void createPluginsItr();
	void setDataForPlugin(const p_auth_continue* data);
	void reset();
	bool extractNewKeys(CSTRING* to, ULONG flags);
	bool hasDataForPlugin();

	// Firebird::IServerBlock implementation
	const char* getLogin();
	const unsigned char* getData(unsigned int* length);
	void putData(Firebird::CheckStatusWrapper* status, unsigned int length, const void* data);
	Firebird::ICryptKey* newKey(Firebird::CheckStatusWrapper* status);
};


// Type of known by server key, received from it by client
class KnownServerKey : public Firebird::AutoStorage
{
public:
	Firebird::PathName type, plugins;

	KnownServerKey()
		: Firebird::AutoStorage(), type(getPool()), plugins(getPool())
	{ }

	explicit KnownServerKey(Firebird::MemoryPool& p)
		: Firebird::AutoStorage(p), type(getPool()), plugins(getPool())
	{ }

	KnownServerKey(Firebird::MemoryPool& p, const KnownServerKey& v)
		: Firebird::AutoStorage(p), type(getPool(), v.type), plugins(getPool(), v.plugins)
	{ }

private:
	KnownServerKey(const KnownServerKey&);
	KnownServerKey& operator=(const KnownServerKey&);
};

// Tags for clumplets, passed from server to client
const UCHAR TAG_KEY_TYPE		= 0;
const UCHAR TAG_KEY_PLUGINS		= 1;
const UCHAR TAG_KNOWN_PLUGINS	= 2;

const signed char WIRECRYPT_BROKEN		= -1;
const signed char WIRECRYPT_DISABLED	= 0;
const signed char WIRECRYPT_ENABLED		= 1;
const signed char WIRECRYPT_REQUIRED	= 2;

// port_flags
const USHORT PORT_symmetric		= 0x0001;	// Server/client architectures are symmetic
const USHORT PORT_async			= 0x0002;	// Port is asynchronous channel for events
const USHORT PORT_no_oob		= 0x0004;	// Don't send out of band data
const USHORT PORT_disconnect	= 0x0008;	// Disconnect is in progress
const USHORT PORT_dummy_pckt_set= 0x0010;	// A dummy packet interval is set
const USHORT PORT_partial_data	= 0x0020;	// Physical packet doesn't contain all API packet
const USHORT PORT_lazy			= 0x0040;	// Deferred operations are allowed
const USHORT PORT_server		= 0x0080;	// Server (not client) port
const USHORT PORT_detached		= 0x0100;	// op_detach, op_drop_database or op_service_detach was processed
const USHORT PORT_rdb_shutdown	= 0x0200;	// Database is shut down
const USHORT PORT_connecting	= 0x0400;	// Aux connection waits for a channel to be activated by client
const USHORT PORT_z_data		= 0x0800;	// Zlib incoming buffer has data left after decompression
const USHORT PORT_compressed	= 0x1000;	// Compress outgoing stream (does not affect incoming)

// forward decl
class RemotePortGuard;

// Port itself

typedef rem_port* (*t_port_connect)(rem_port*, PACKET*);

typedef Firebird::RefPtr<rem_port> RemPortPtr;

struct rem_port : public Firebird::GlobalStorage, public Firebird::RefCounted
{
#ifdef DEV_BUILD
	static Firebird::AtomicCounter portCounter;
#endif

	// sync objects
	Firebird::RefPtr<Firebird::RefMutex> port_sync;
	Firebird::RefPtr<Firebird::RefMutex> port_que_sync;
	Firebird::RefPtr<Firebird::RefMutex> port_write_sync;
	Firebird::RefPtr<Firebird::RefMutex> port_cancel_sync;

	// port function pointers (C "emulation" of virtual functions)
	bool			(*port_accept)(rem_port*, const p_cnct*);
	void			(*port_disconnect)(rem_port*);
	void			(*port_force_close)(rem_port*);
	rem_port*		(*port_receive_packet)(rem_port*, PACKET*);
	XDR_INT			(*port_send_packet)(rem_port*, PACKET*);
	XDR_INT			(*port_send_partial)(rem_port*, PACKET*);
	t_port_connect	port_connect;		// Establish secondary connection
	rem_port*		(*port_request)(rem_port*, PACKET*);	// Request to establish secondary connection
	bool			(*port_select_multi)(rem_port*, UCHAR*, SSHORT, SSHORT*, RemPortPtr&);	// get packet from active port
	void			(*port_abort_aux_connection)(rem_port*);	// stop waiting for secondary connection

	enum rem_port_t {
		INET,			// Internet (TCP/IP)
		PIPE,			// Windows NT named pipe connection
		XNET			// Windows NT shared memory connection
	}				port_type;
	enum state_t {
		PENDING,		// connection is pending
		BROKEN,			// connection is broken
		DISCONNECTED	// port is disconnected
	}				port_state;

	rem_port*		port_clients;		// client ports
	rem_port*		port_next;			// next client port
	rem_port*		port_parent;		// parent port (for client ports)
	rem_port*		port_async;			// asynchronous sibling port
	rem_port*		port_async_receive;	// async packets receiver
	struct srvr*	port_server;		// server of port
	USHORT			port_server_flags;	// TRUE if server
	USHORT			port_protocol;		// protocol version number
	USHORT			port_buff_size;		// port buffer size
	USHORT			port_flags;			// Misc flags
	SLONG			port_connect_timeout;   // Connection timeout value
	SLONG			port_dummy_packet_interval; // keep alive dummy packet interval
	SLONG			port_dummy_timeout;	// time remaining until keepalive packet
	SOCKET			port_handle;		// handle for INET socket
	SOCKET			port_channel;		// handle for connection (from by OS)
	struct linger	port_linger;		// linger value as defined by SO_LINGER
	Rdb*			port_context;
	Thread::Handle	port_events_thread;	// handle of thread, handling incoming events
	ThreadId		port_events_threadId;
	RemotePortGuard* port_thread_guard;	// will close port_events_thread in safe way
#ifdef WIN_NT
	HANDLE			port_pipe;			// port pipe handle
	HANDLE			port_event;			// event associated with a port
#endif
	XDR				port_receive;
	XDR				port_send;
#ifdef DEBUG_XDR_MEMORY
	r e m _ v e c*	port_packet_vector;		// Vector of send/receive packets
#endif
	Firebird::Array<RemoteObject> port_objects;
	rem_str*		port_version;
	rem_str*		port_host;				// Our name
	rem_str*		port_connection;		// Name of connection
	Firebird::string port_login;
	Firebird::string port_user_name;
	Firebird::string port_peer_name;
	Firebird::string port_protocol_id;		// String containing protocol name for this port
	Firebird::string port_address;			// Protocol-specific address string for the port
	Rpr*			port_rpr;				// port stored procedure reference
	Rsr*			port_statement;			// Statement for execute immediate
	rmtque*			port_receive_rmtque;	// for client, responses waiting
	Firebird::AtomicCounter	port_requests_queued;	// requests currently queued
	xcc*			port_xcc;				// interprocess structure
	PacketQueue*	port_deferred_packets;	// queue of deferred packets
	OBJCT			port_last_object_id;	// cached last id
	Firebird::ObjectsArray< Firebird::Array<char> > port_queue;
	FB_SIZE_T		port_qoffset;			// current packet in the queue
	Firebird::RefPtr<const Config> port_config;	// connection-specific configuration info

	// Authentication and crypt stuff
	ServerAuthBase*							port_srv_auth;
	SrvAuthBlock*							port_srv_auth_block;
	Firebird::HalfStaticArray<InternalCryptKey*, 2>	port_crypt_keys;	// available wire crypt keys
	bool			port_crypt_complete;	// wire crypt init is complete one way or another,
											// up to being turned off in firebird.conf
	signed char		port_crypt_level;		// encryption level for port
	Firebird::ObjectsArray<KnownServerKey>	port_known_server_keys;	// Server sends to client
											// keys known by it, they are stored here
	Firebird::IWireCryptPlugin* port_crypt_plugin;		// plugin used by port, when not NULL - crypts wire data
	Firebird::ICryptKeyCallback* port_client_crypt_callback;	// client callback to transfer database crypt key
	ServerCallbackBase* port_server_crypt_callback;			// server callback to transfer database crypt key

	UCharArrayAutoPtr	port_buffer;

	FB_UINT64 port_snd_packets;
	FB_UINT64 port_rcv_packets;
	FB_UINT64 port_snd_bytes;
	FB_UINT64 port_rcv_bytes;

#ifdef WIRE_COMPRESS_SUPPORT
	z_stream port_send_stream, port_recv_stream;
	UCharArrayAutoPtr	port_compressed;
#endif

public:
	rem_port(rem_port_t t, size_t rpt) :
		port_sync(FB_NEW_POOL(getPool()) Firebird::RefMutex()),
		port_que_sync(FB_NEW_POOL(getPool()) Firebird::RefMutex()),
		port_write_sync(FB_NEW_POOL(getPool()) Firebird::RefMutex()),
		port_cancel_sync(FB_NEW_POOL(getPool()) Firebird::RefMutex()),
		port_accept(0), port_disconnect(0), port_force_close(0), port_receive_packet(0), port_send_packet(0),
		port_send_partial(0), port_connect(0), port_request(0), port_select_multi(0),
		port_type(t), port_state(PENDING), port_clients(0), port_next(0),
		port_parent(0), port_async(0), port_async_receive(0),
		port_server(0), port_server_flags(0), port_protocol(0), port_buff_size(rpt / 2),
		port_flags(0), port_connect_timeout(0), port_dummy_packet_interval(0),
		port_dummy_timeout(0), port_handle(INVALID_SOCKET), port_channel(INVALID_SOCKET), port_context(0),
		port_events_thread(0), port_events_threadId(0), port_thread_guard(0),
#ifdef WIN_NT
		port_pipe(INVALID_HANDLE_VALUE), port_event(INVALID_HANDLE_VALUE),
#endif
#ifdef DEBUG_XDR_MEMORY
		port_packet_vector(0),
#endif
		port_objects(getPool()), port_version(0), port_host(0),
		port_connection(0), port_login(getPool()),
		port_user_name(getPool()), port_peer_name(getPool()),
		port_protocol_id(getPool()), port_address(getPool()),
		port_rpr(0), port_statement(0), port_receive_rmtque(0),
		port_requests_queued(0), port_xcc(0), port_deferred_packets(0), port_last_object_id(0),
		port_queue(getPool()), port_qoffset(0),
		port_srv_auth(NULL), port_srv_auth_block(NULL),
		port_crypt_keys(getPool()), port_crypt_complete(false), port_crypt_level(WIRECRYPT_REQUIRED),
		port_known_server_keys(getPool()), port_crypt_plugin(NULL),
		port_client_crypt_callback(NULL), port_server_crypt_callback(NULL),
		port_buffer(FB_NEW_POOL(getPool()) UCHAR[rpt]),
		port_snd_packets(0), port_rcv_packets(0), port_snd_bytes(0), port_rcv_bytes(0)
	{
		addRef();
		memset(&port_linger, 0, sizeof port_linger);
		memset(port_buffer, 0, rpt);
#ifdef DEV_BUILD
		++portCounter;
#endif
	}

private:
	~rem_port();	// this is refCounted object - private dtor is OK

public:
	void initCompression();
	static bool checkCompression();
	void linkParent(rem_port* const parent);
	void unlinkParent();
	Firebird::RefPtr<const Config> getPortConfig();
	const Firebird::RefPtr<const Config>& getPortConfig() const;
	void versionInfo(Firebird::string& version) const;

	bool extractNewKeys(CSTRING* to, bool flagPlugList = false)
	{
		return port_srv_auth_block->extractNewKeys(to,
			(flagPlugList ? SrvAuthBlock::EXTRACT_PLUGINS_LIST : 0) |
			(port_crypt_level <= WIRECRYPT_DISABLED ? SrvAuthBlock::ONLY_CLEANUP : 0));
	}

	template <typename T>
	void getHandle(T*& blk, OBJCT id)
	{
		if ((port_flags & PORT_lazy) && (id == INVALID_OBJECT))
		{
			id = port_last_object_id;
		}
		if (id >= port_objects.getCount() || port_objects[id].isMissing())
		{
			Firebird::status_exception::raise(Firebird::Arg::Gds(T::badHandle()));
		}
		blk = port_objects[id];
	}

	template <typename T>
	OBJCT setHandle(T* const object, const OBJCT id)
	{
		if (id >= port_objects.getCount())
		{
			// Prevent the creation of object handles that can't be
			// transferred by the remote protocol.
			if (id > MAX_OBJCT_HANDLES)
			{
				return (OBJCT) 0;
			}

			port_objects.grow(id + 1);
		}

		port_objects[id] = object;
		return id;
	}

	// Allocate an object slot for an object.
	template <typename T>
	OBJCT get_id(T* object)
	{
		// Reserve slot 0 so we can distinguish something from nothing.
		// NOTE: prior to server version 4.5.0 id==0 COULD be used - so
		// only the server side can now depend on id==0 meaning "invalid id"
		unsigned int i = 1;
		for (; i < port_objects.getCount(); ++i)
		{
			if (port_objects[i].isMissing())
			{
				break;
			}
		}

		port_last_object_id = setHandle(object, static_cast<OBJCT>(i));
		return port_last_object_id;
	}

	void releaseObject(OBJCT id)
	{
		if (id != INVALID_OBJECT)
		{
			port_objects[id].release();
		}
	}


public:
	// TMN: Beginning of C++ port
	// TMN: ugly, but at least a start
	bool	accept(p_cnct* cnct);
	void	disconnect();
	void	force_close();
	rem_port*	receive(PACKET* pckt);
	XDR_INT	send(PACKET* pckt);
	XDR_INT	send_partial(PACKET* pckt);
	rem_port*	connect(PACKET* pckt);
	rem_port*	request(PACKET* pckt);
	bool		select_multi(UCHAR* buffer, SSHORT bufsize, SSHORT* length, RemPortPtr& port);
	void		abort_aux_connection();

	bool haveRecvData()
	{
		Firebird::RefMutexGuard queGuard(*port_que_sync, FB_FUNCTION);
		return ((port_receive.x_handy > 0) || (port_qoffset < port_queue.getCount()));
	}

	void clearRecvQue()
	{
		Firebird::RefMutexGuard queGuard(*port_que_sync, FB_FUNCTION);
		port_queue.clear();
		port_qoffset = 0;
		port_receive.x_private = port_receive.x_base;
	}

	class RecvQueState
	{
	public:
		int save_handy;
		FB_SIZE_T save_private;
		FB_SIZE_T save_qoffset;

		RecvQueState(const rem_port* port)
		{
			save_handy = port->port_receive.x_handy;
			save_private = port->port_receive.x_private - port->port_receive.x_base;
			save_qoffset = port->port_qoffset;
		}
	};

	RecvQueState getRecvState() const
	{
		return RecvQueState(this);
	}

	void setRecvState(const RecvQueState& rs)
	{
		if (rs.save_qoffset > 0 && (rs.save_qoffset != port_qoffset))
		{
			Firebird::Array<char>& q = port_queue[rs.save_qoffset - 1];
			memcpy(port_receive.x_base, q.begin(), q.getCount());
		}
		port_qoffset = rs.save_qoffset;
		port_receive.x_private = port_receive.x_base + rs.save_private;
		port_receive.x_handy = rs.save_handy;
	}

	// TMN: The following member functions are conceptually private
	// to server.cpp and should be _made_ private in due time!
	// That is, if we don't factor these method out.

	ISC_STATUS	compile(P_CMPL*, PACKET*);
	ISC_STATUS	ddl(P_DDL*, PACKET*);
	void	disconnect(PACKET*, PACKET*);
	void	drop_database(P_RLSE*, PACKET*);

	ISC_STATUS	end_blob(P_OP, P_RLSE*, PACKET*);
	ISC_STATUS	end_database(P_RLSE*, PACKET*);
	ISC_STATUS	end_request(P_RLSE*, PACKET*);
	ISC_STATUS	end_statement(P_SQLFREE*, PACKET*);
	ISC_STATUS	end_transaction(P_OP, P_RLSE*, PACKET*);
	ISC_STATUS	execute_immediate(P_OP, P_SQLST*, PACKET*);
	ISC_STATUS	execute_statement(P_OP, P_SQLDATA*, PACKET*);
	ISC_STATUS	fetch(P_SQLDATA*, PACKET*);
	ISC_STATUS	get_segment(P_SGMT*, PACKET*);
	ISC_STATUS	get_slice(P_SLC*, PACKET*);
	void		info(P_OP, P_INFO*, PACKET*);
	ISC_STATUS	open_blob(P_OP, P_BLOB*, PACKET*);
	ISC_STATUS	prepare(P_PREP*, PACKET*);
	ISC_STATUS	prepare_statement(P_SQLST*, PACKET*);
	ISC_STATUS	put_segment(P_OP, P_SGMT*, PACKET*);
	ISC_STATUS	put_slice(P_SLC*, PACKET*);
	ISC_STATUS	que_events(P_EVENT*, PACKET*);
	ISC_STATUS	receive_after_start(P_DATA*, PACKET*, Firebird::IStatus*);
	ISC_STATUS	receive_msg(P_DATA*, PACKET*);
	ISC_STATUS	seek_blob(P_SEEK*, PACKET*);
	ISC_STATUS	send_msg(P_DATA*, PACKET*);
	ISC_STATUS	send_response(PACKET*, OBJCT, ULONG, const ISC_STATUS*, bool);
	ISC_STATUS	send_response(PACKET* p, OBJCT obj, ULONG length, const Firebird::IStatus* status, bool defer_flag);
	ISC_STATUS	service_attach(const char*, Firebird::ClumpletWriter*, PACKET*);
	ISC_STATUS	service_end(P_RLSE*, PACKET*);
	void		service_start(P_INFO*, PACKET*);
	ISC_STATUS	set_cursor(P_SQLCUR*, PACKET*);
	ISC_STATUS	start(P_OP, P_DATA*, PACKET*);
	ISC_STATUS	start_and_send(P_OP, P_DATA*, PACKET*);
	ISC_STATUS	start_transaction(P_OP, P_STTR*, PACKET*);
	ISC_STATUS	transact_request(P_TRRQ *, PACKET*);
	SSHORT		asyncReceive(PACKET* asyncPacket, const UCHAR* buffer, SSHORT dataSize);
	void		start_crypt(P_CRYPT*, PACKET*);
	void		batch_create(P_BATCH_CREATE*, PACKET*);
	void		batch_msg(P_BATCH_MSG*, PACKET*);
	void		batch_blob_stream(P_BATCH_BLOB*, PACKET*);
	void		batch_regblob(P_BATCH_REGBLOB*, PACKET*);
	void		batch_exec(P_BATCH_EXEC*, PACKET*);
	void		batch_rls(P_BATCH_FREE*, PACKET*);
	void		batch_bpb(P_BATCH_SETBPB*, PACKET*);

	Firebird::string getRemoteId() const;
	void auxAcceptError(PACKET* packet);
	void addServerKeys(CSTRING* str);
	bool tryNewKey(InternalCryptKey* cryptKey);
	void checkResponse(Firebird::IStatus* warning, PACKET* packet, bool checkKeys = false);

private:
	bool tryKeyType(const KnownServerKey& srvKey, InternalCryptKey* cryptKey);
};


// Port guard is needed to close events delivery thread in safe way
class RemotePortGuard
{
private:
	class WaitThread
	{
	public:
		WaitThread(rem_port* async)
			: asyncPort(async),
			  waitFlag(false)
		{ }

		~WaitThread()
		{
			if (waitFlag)
			{
				Thread::waitForCompletion(waitHandle);

				fb_assert(asyncPort);

				if (asyncPort)
					asyncPort->release();
			}
			else if (asyncPort)
				asyncPort->port_thread_guard = nullptr;
		}

		rem_port* asyncPort;
		Thread::Handle waitHandle;
		bool waitFlag;
	};

public:
	RemotePortGuard(rem_port* port, const char* f)
		: wThr(port->port_async),
		  guard(*port->port_sync, f)
	{
		if (wThr.asyncPort)
			wThr.asyncPort->port_thread_guard = this;
	}

	void setWait(Thread::Handle& handle)
	{
		wThr.waitHandle = handle;
		wThr.waitFlag = true;
		fb_assert(wThr.asyncPort);
		wThr.asyncPort->port_thread_guard = nullptr;
	}

private:
	WaitThread wThr;
	Firebird::RefMutexGuard guard;
};


// Queuing structure for Client batch fetches

typedef void (*t_rmtque_fn)(rem_port*, rmtque*, USHORT);

struct rmtque : public Firebird::GlobalStorage
{
	rmtque*				rmtque_next;	// Next entry in queue
	void*				rmtque_parm;	// What request has response in queue
	Rrq::rrq_repeat*	rmtque_message;	// What message is pending
	Rdb*				rmtque_rdb;		// What database has pending msg

	// Fn that receives queued entry
	t_rmtque_fn			rmtque_function;

public:
	rmtque() :
		rmtque_next(0), rmtque_parm(0), rmtque_message(0), rmtque_rdb(0), rmtque_function(0)
	{ }
};


// contains ports which must be closed at engine shutdown
class PortsCleanup
{
public:
	PortsCleanup() :
	  m_ports(NULL),
	  m_mutex()
	{}

	explicit PortsCleanup(MemoryPool&) :
	  m_ports(NULL),
	  m_mutex()
	{}

	~PortsCleanup()
	{}

	void registerPort(rem_port*);
	void unRegisterPort(rem_port*);

	void closePorts();

private:
	typedef Firebird::SortedArray<rem_port*> PortsArray;
	PortsArray*		m_ports;
	Firebird::Mutex	m_mutex;
};

#endif // REMOTE_REMOTE_H
