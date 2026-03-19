/*
 *      PROGRAM:        JRD access method
 *      MODULE:         jrd.h
 *      DESCRIPTION:    Common descriptions
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
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 * Claudio Valderrama C.
 * Adriano dos Santos Fernandes
 *
 */

#ifndef JRD_JRD_H
#define JRD_JRD_H

#include "../common/gdsassert.h"
#include "../jrd/tdbb.h"
#include "../common/dsc.h"
#include "../jrd/err_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/obj.h"
#include "../jrd/val.h"
#include "../jrd/vec.h"
#include "../jrd/status.h"
#include "../jrd/Database.h"

#include "../common/classes/fb_atomic.h"
#include "../common/classes/fb_string.h"
#include "../jrd/MetaName.h"
#include "../common/classes/NestConst.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/stack.h"
#include "../common/classes/timestamp.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/Synchronize.h"
#include "../common/utils_proto.h"
#include "../common/StatusHolder.h"
#include "../jrd/RandomGenerator.h"
#include "../common/os/guid.h"
#include "../jrd/sbm.h"
#include "../jrd/scl.h"
#include "../jrd/Routine.h"
#include "../jrd/ExtEngineManager.h"
#include "../jrd/Attachment.h"
#include "firebird/Interface.h"

#ifndef CDS_UNAVAILABLE
#include <cds/threading/model.h>	// cds::threading::Manager
#endif

#define SOFT_BUGCHECK(number)	ERR_soft_bugcheck(number, __FILE__, __LINE__)
#define CORRUPT(number)			ERR_corrupt(number)
#define IBERROR(number)			ERR_error(number)


#define BLKCHK(blk, type)       if (!blk->checkHandle()) BUGCHECK(147)

#define DEV_BLKCHK(blk, type)	do { } while (false)	// nothing


// Thread data block / IPC related data blocks
#include "../common/ThreadData.h"

// Definition of block types for data allocation in JRD
#include "../include/fb_blk.h"

#include "../jrd/blb.h"

// Definition of DatabasePlugins
#include "../jrd/flu.h"

#include "../jrd/pag.h"

#include "../jrd/RuntimeStatistics.h"
//#include "../jrd/Database.h"
#include "../jrd/lck.h"

// Error codes
#include "iberror.h"

struct dsc;

namespace EDS {
	class Connection;
}

namespace Firebird {
	class TextType;
}

namespace Jrd {

inline constexpr unsigned MAX_CALLBACKS = 50;

// fwd. decl.
class thread_db;
class Attachment;
class jrd_tra;
class Request;
class Statement;
class jrd_file;
class Format;
class BufferDesc;
class SparseBitmap;
class jrd_rel;
class ExternalFile;
class ViewContext;
class ArrayField;
struct sort_context;
class vcl;
class Parameter;
class jrd_fld;
class dsql_dbb;
class PreparedStatement;
class TraceManager;
class MessageNode;
class Database;

//
// Transaction element block
//
struct teb
{
	Attachment* teb_database;
	int teb_tpb_length;
	const UCHAR* teb_tpb;
};

typedef teb TEB;

// Window block for loading cached pages into
// CVC: Apparently, the only possible values are HEADER_PAGE==0 and LOG_PAGE==2
// and reside in ods.h, although I watched a place with 1 and others with members
// of a struct.

struct win
{
	PageNumber win_page;
	Ods::pag* win_buffer;
	class BufferDesc* win_bdb;
	SSHORT win_scans;
	USHORT win_flags;
	explicit win(const PageNumber& wp) noexcept
		: win_page(wp), win_bdb(NULL), win_flags(0)
	{}
	win(const USHORT pageSpaceID, const ULONG pageNum) noexcept
		: win_page(pageSpaceID, pageNum), win_bdb(NULL), win_flags(0)
	{}
};

typedef win WIN;

// This is a compilation artifact: I wanted to be sure I would pick all old "win"
// declarations at the top, so "win" was built with a mandatory argument in
// the constructor. This struct satisfies a single place with an array. The
// alternative would be to initialize 16 elements of the array with 16 calls
// to the constructor: win my_array[n] = {win(-1), ... (win-1)};
// When all places are changed, this class can disappear and win's constructor
// may get the default value of ~0 to "wp".
struct win_for_array : public win
{
	win_for_array() noexcept
		: win(DB_PAGE_SPACE, ~0)
	{}
};

// win_flags

inline constexpr USHORT WIN_large_scan			= 1;	// large sequential scan
inline constexpr USHORT WIN_secondary			= 2;	// secondary stream
inline constexpr USHORT WIN_garbage_collector	= 4;	// garbage collector's window
inline constexpr USHORT WIN_garbage_collect		= 8;	// scan left a page for garbage collector

// Helper class to temporarily activate sweeper context
class ThreadSweepGuard
{
public:
	explicit ThreadSweepGuard(thread_db* tdbb) noexcept
		: m_tdbb(tdbb)
	{
		m_tdbb->markAsSweeper();
	}

	~ThreadSweepGuard()
	{
		m_tdbb->tdbb_flags &= ~TDBB_sweeper;
	}

private:
	thread_db* const m_tdbb;
};

// CVC: This class was designed to restore the thread's default status vector automatically.
// In several places, tdbb_status_vector is replaced by a local temporary.
class ThreadStatusGuard
{
public:
	explicit ThreadStatusGuard(thread_db* tdbb)
		: m_tdbb(tdbb), m_old_status(tdbb->tdbb_status_vector)
	{
		m_tdbb->tdbb_status_vector = &m_local_status;
	}

	~ThreadStatusGuard()
	{
		m_tdbb->tdbb_status_vector = m_old_status;
	}

	// copying is prohibited
	ThreadStatusGuard(const ThreadStatusGuard&) = delete;
	ThreadStatusGuard& operator=(const ThreadStatusGuard&) = delete;

	FbStatusVector* restore() noexcept
	{
		m_tdbb->tdbb_status_vector = m_old_status;
		return m_old_status;
	}

	operator FbStatusVector*() noexcept { return &m_local_status; }
	FbStatusVector* operator->() noexcept { return &m_local_status; }

	operator const FbStatusVector*() const noexcept { return &m_local_status; }
	const FbStatusVector* operator->() const noexcept { return &m_local_status; }

	void copyToOriginal() noexcept
	{
		fb_utils::copyStatus(m_old_status, &m_local_status);
	}

private:
	Firebird::FbLocalStatus m_local_status;
	thread_db* const m_tdbb;
	FbStatusVector* const m_old_status;
};


// duplicate context of firebird string
inline char* stringDup(MemoryPool& p, const Firebird::string& s)
{
	char* rc = (char*) p.allocate(s.length() + 1);
	strcpy(rc, s.c_str());
	return rc;
}

inline char* stringDup(MemoryPool& p, const char* s, size_t l)
{
	char* rc = (char*) p.allocate(l + 1);
	memcpy(rc, s, l);
	rc[l] = 0;
	return rc;
}

inline char* stringDup(MemoryPool& p, const char* s)
{
	if (! s)
	{
		return 0;
	}
	return stringDup(p, s, strlen(s));
}

// Used in string conversion calls
typedef Firebird::HalfStaticArray<UCHAR, 256> MoveBuffer;

} //namespace Jrd

inline bool JRD_reschedule(Jrd::thread_db* tdbb, bool force = false)
{
	if (force || --tdbb->tdbb_quantum < 0)
	{
		tdbb->reschedule();
		return true;
	}

	return false;
}

inline Jrd::Database* GET_DBB()
{
	return JRD_get_thread_data()->getDatabase();
}

/*-------------------------------------------------------------------------*
 * macros used to set thread_db and Database pointers when there are not set already *
 *-------------------------------------------------------------------------*/
inline void SET_TDBB(Jrd::thread_db*& tdbb)
{
	if (tdbb == NULL) {
		tdbb = JRD_get_thread_data();
	}
	CHECK_TDBB(tdbb);
}

inline void SET_DBB(Jrd::Database*& dbb)
{
	if (dbb == NULL) {
		dbb = GET_DBB();
	}
	CHECK_DBB(dbb);
}


// global variables for engine

namespace Jrd {
	typedef Firebird::SubsystemContextPoolHolder <Jrd::thread_db, MemoryPool> ContextPoolHolder;

	class DatabaseContextHolder : public Jrd::ContextPoolHolder
	{
	public:
		explicit DatabaseContextHolder(thread_db* tdbb)
			: Jrd::ContextPoolHolder(tdbb, tdbb->getDatabase()->dbb_permanent)
		{}

		// copying is prohibited
		DatabaseContextHolder(const DatabaseContextHolder&) = delete;
		DatabaseContextHolder& operator=(const DatabaseContextHolder&) = delete;
	};

	class BackgroundContextHolder : public ThreadContextHolder, public DatabaseContextHolder,
		public Jrd::Attachment::SyncGuard
	{
	public:
		BackgroundContextHolder(Database* dbb, Jrd::Attachment* att, FbStatusVector* status, const char* f)
			: ThreadContextHolder(dbb, att, status),
			  DatabaseContextHolder(operator thread_db*()),
			  Jrd::Attachment::SyncGuard(att, f)
		{}

		// copying is prohibited
		BackgroundContextHolder(const BackgroundContextHolder&) = delete;
		BackgroundContextHolder& operator=(const BackgroundContextHolder&) = delete;
	};

	class AttachmentHolder
	{
	public:
		static constexpr unsigned ATT_LOCK_ASYNC		= 1;
		static constexpr unsigned ATT_DONT_LOCK			= 2;
		static constexpr unsigned ATT_NO_SHUTDOWN_CHECK	= 4;
		static constexpr unsigned ATT_NON_BLOCKING		= 8;

		AttachmentHolder(thread_db* tdbb, StableAttachmentPart* sa, unsigned lockFlags, const char* from);
		~AttachmentHolder();

		// copying is prohibited
		AttachmentHolder(const AttachmentHolder&) = delete;
		AttachmentHolder& operator =(const AttachmentHolder&) = delete;

	private:
		Firebird::RefPtr<StableAttachmentPart> sAtt;
		bool async;			// async mutex should be locked instead normal
		bool nolock; 		// if locked manually, no need to take lock recursively
		bool blocking;		// holder instance is blocking other instances
	};

	class EngineContextHolder final : public ThreadContextHolder, private AttachmentHolder, private DatabaseContextHolder
	{
	public:
		template <typename I>
		EngineContextHolder(Firebird::CheckStatusWrapper* status, I* interfacePtr, const char* from,
							unsigned lockFlags = 0);
	};

	class AstLockHolder : public Firebird::ReadLockGuard
	{
	public:
		AstLockHolder(Database* dbb, const char* f)
			: Firebird::ReadLockGuard(dbb->dbb_ast_lock, f)
		{
			if (dbb->dbb_flags & DBB_no_ast)
			{
				// usually to be swallowed by the AST, but it allows to skip its execution
				Firebird::status_exception::raise(Firebird::Arg::Gds(isc_unavailable));
			}
		}
	};

	class AsyncContextHolder : public AstLockHolder, public Jrd::Attachment::SyncGuard,
		public ThreadContextHolder, public DatabaseContextHolder
	{
	public:
		AsyncContextHolder(Database* dbb, const char* f, Lock* lck = NULL)
			: AstLockHolder(dbb, f),
			  Jrd::Attachment::SyncGuard(lck ?
				lck->getLockStable() : Firebird::RefPtr<StableAttachmentPart>(), f, true),
			  ThreadContextHolder(dbb, lck ? lck->getLockAttachment() : NULL),
			  DatabaseContextHolder(operator thread_db*())
		{
			if (lck)
			{
				// The lock could be released while we were waiting on the attachment mutex.
				// This may cause a segfault due to lck_attachment == NULL stored in tdbb.

				if (!lck->lck_id)
				{
					// usually to be swallowed by the AST, but it allows to skip its execution
					Firebird::status_exception::raise(Firebird::Arg::Gds(isc_unavailable));
				}

				fb_assert((operator thread_db*())->getAttachment());
			}

			(*this)->tdbb_flags |= TDBB_async;
		}

		// copying is prohibited
		AsyncContextHolder(const AsyncContextHolder&) = delete;
		AsyncContextHolder& operator=(const AsyncContextHolder&) = delete;
	};

	class EngineCheckout
	{
	public:
		enum Type
		{
			REQUIRED,
			UNNECESSARY,
			AVOID
		};

		EngineCheckout(thread_db* tdbb, const char* from, Type type = REQUIRED)
			: m_tdbb(tdbb), m_from(from)
		{
			if (type != AVOID)
			{
				Attachment* const att = tdbb ? tdbb->getAttachment() : NULL;

				if (att)
					m_ref = att->getStable();

				fb_assert(type == UNNECESSARY || m_ref.hasData());

				if (m_ref.hasData())
					m_ref->getSync()->leave();
			}
		}

		EngineCheckout(Attachment* att, const char* from, Type type = REQUIRED)
			: m_tdbb(nullptr), m_from(from)
		{
			if (type != AVOID)
			{
				fb_assert(type == UNNECESSARY || att);

				if (att && att->att_use_count)
				{
					m_ref = att->getStable();
					m_ref->getSync()->leave();
				}
			}
		}

		~EngineCheckout()
		{
			if (m_ref.hasData())
				m_ref->getSync()->enter(m_from);

			// If we were signalled to cancel/shutdown, react as soon as possible.
			// We cannot throw immediately, but we can reschedule ourselves.

			if (m_tdbb && m_tdbb->tdbb_quantum > 0 && m_tdbb->getCancelState() != FB_SUCCESS)
				m_tdbb->tdbb_quantum = 0;
		}

		// copying is prohibited
		EngineCheckout(const EngineCheckout&) = delete;
		EngineCheckout& operator=(const EngineCheckout&) = delete;

	private:
		thread_db* const m_tdbb;
		Firebird::RefPtr<StableAttachmentPart> m_ref;
		const char* m_from;
	};

	class CheckoutLockGuard
	{
	public:
		CheckoutLockGuard(thread_db* tdbb, Firebird::Mutex& mutex,
						  const char* from, bool optional = false)
			: m_mutex(mutex)
		{
			if (!m_mutex.tryEnter(from))
			{
				EngineCheckout cout(tdbb, from, optional ? EngineCheckout::UNNECESSARY : EngineCheckout::REQUIRED);
				m_mutex.enter(from);
			}
		}

		~CheckoutLockGuard()
		{
			m_mutex.leave();
		}

		// copying is prohibited
		CheckoutLockGuard(const CheckoutLockGuard&) = delete;
		CheckoutLockGuard& operator=(const CheckoutLockGuard&) = delete;

	private:
		Firebird::Mutex& m_mutex;
	};

	class CheckoutSyncGuard
	{
	public:
		CheckoutSyncGuard(thread_db* tdbb, Firebird::SyncObject& sync,
						  Firebird::SyncType type,
						  const char* from, bool optional = false)
			: m_sync(&sync, from)
		{
			if (!m_sync.lockConditional(type, from))
			{
				EngineCheckout cout(tdbb, from, optional ? EngineCheckout::UNNECESSARY : EngineCheckout::REQUIRED);
				m_sync.lock(type);
			}
		}

		// copying is prohibited
		CheckoutSyncGuard(const CheckoutSyncGuard&) = delete;
		CheckoutSyncGuard& operator=(const CheckoutSyncGuard&) = delete;

	private:
		Firebird::Sync m_sync;
	};

} // namespace Jrd

#endif // JRD_JRD_H
