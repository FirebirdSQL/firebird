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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2014 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */


#ifndef JRD_REPLICATION_CHANGELOG_H
#define JRD_REPLICATION_CHANGELOG_H

#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/semaphore.h"
#include "../common/classes/fb_string.h"
#include "../common/os/guid.h"
#include "../common/isc_s_proto.h"

#include "Utils.h"

namespace Replication
{
	struct Config;

	enum SegmentState : USHORT
	{
		SEGMENT_STATE_FREE = 0,
		SEGMENT_STATE_USED = 1,
		SEGMENT_STATE_FULL = 2,
		SEGMENT_STATE_ARCH = 3
	};

	struct SegmentHeader
	{
		char hdr_signature[12];
		USHORT hdr_version;
		SegmentState hdr_state;
		UUID hdr_guid;
		FB_UINT64 hdr_sequence;
		FB_UINT64 hdr_length;
	};

	const char CHANGELOG_SIGNATURE[] = "FBCHANGELOG";

	// FIXME: increase version
	const USHORT CHANGELOG_VERSION_1 = 1;
	const USHORT CHANGELOG_CURRENT_VERSION = CHANGELOG_VERSION_1;

	class ChangeLog : protected Firebird::PermanentStorage, public Firebird::IpcObject
	{
		// Shared state of the changelog

		struct State : public Firebird::MemoryHeader
		{
			ULONG version;				// changelog version
			time_t timestamp;			// timestamp of last write
			ULONG generation;			// segments reload marker
			ULONG flushMark;			// last flush mark
			FB_UINT64 sequence;			// sequence number of the last segment
			ULONG pidLower;				// lower boundary mark in the PID array
			ULONG pidUpper;				// upper boundary mark in the PID array
			int pids[1];				// PIDs attached to the state
		};

		// Shared memory layout format
		static const USHORT STATE_VERSION = 1;
		// Mapping size (not extendable for the time being)
		static const ULONG STATE_MAPPING_SIZE = 64 * 1024;	// 64 KB
		// Max number of processes accessing the shared state
		static const ULONG PID_CAPACITY = (STATE_MAPPING_SIZE - offsetof(State, pids)) / sizeof(int); // ~16K

		// RAII helper to lock the shared state

		class LockGuard
		{
		public:
			LockGuard(ChangeLog* log)
				: m_log(log)
			{
				m_log->lockState();
			}

			~LockGuard()
			{
				if (m_log)
					m_log->unlockState();
			}

			void release()
			{
				if (m_log)
				{
					m_log->unlockState();
					m_log = NULL;
				}
			}

		private:
			ChangeLog* m_log;
		};

		// RAII helper to unlock the shared state

		class LockCheckout
		{
		public:
			LockCheckout(ChangeLog* log)
				: m_log(log && !log->m_shutdown ? log : NULL)
			{
				if (m_log)
					m_log->unlockState();
			}

			~LockCheckout()
			{
				if (m_log)
					m_log->lockState();
			}

		private:
			ChangeLog* const m_log;
		};

		// Changelog segment (physical file on disk)

		class Segment : public Firebird::RefCounted
		{
		public:
			Segment(MemoryPool& pool, const Firebird::PathName& filename, int handle);
			virtual ~Segment();

			void init(FB_UINT64 sequence, const Firebird::Guid& guid);
			bool validate(const Firebird::Guid& guid) const;
			void append(ULONG length, const UCHAR* data);
			void copyTo(const Firebird::PathName& filename) const;

			bool isEmpty() const
			{
				return (m_header->hdr_length == sizeof(SegmentHeader));
			}

			bool hasData() const
			{
				return (m_header->hdr_length > sizeof(SegmentHeader));
			}

			ULONG getLength() const
			{
				return m_header->hdr_length;
			}

			FB_UINT64 getSequence() const
			{
				return m_header->hdr_sequence;
			}

			SegmentState getState() const
			{
				return m_header->hdr_state;
			}

			void setState(SegmentState state);

			void truncate();
			void flush(bool data);

			Firebird::PathName getFileName() const;

			const Firebird::PathName& getPathName() const
			{
				return m_filename;
			}

			void closeFile();

		private:
			void mapHeader();
			void unmapHeader();

			Firebird::PathName m_filename;
			int m_handle;
			SegmentHeader* m_header;
			SegmentHeader m_builtinHeader;		// used by free segments when there is no mapping

	#ifdef WIN_NT
			HANDLE m_mapping;
	#endif
		};

	public:
		ChangeLog(Firebird::MemoryPool& pool,
				  const Firebird::string& dbId,
				  const Firebird::Guid& guid,
				  const FB_UINT64 sequence,
				  const Config* config);
		virtual ~ChangeLog();

		void forceSwitch();
		FB_UINT64 write(ULONG length, const UCHAR* data, bool sync);

		void bgArchiver();

	private:
		void initSharedFile();

		void lockState();
		void unlockState();

		void linkSelf();
		bool unlinkSelf();

		bool initialize(Firebird::SharedMemoryBase* shmem, bool init) override;
		void mutexBug(int osErrorCode, const char* text) override;

		USHORT getType() const override { return Firebird::SharedMemoryBase::SRAM_CHANGELOG_STATE; }
		USHORT getVersion() const override { return STATE_VERSION; };
		const char* getName() const override { return "ChangeLog"; }

		bool validateSegment(const Segment* segment)
		{
			return segment->validate(m_guid);
		}

		void initSegments();
		void clearSegments();

		Segment* createSegment();
		Segment* reuseSegment(Segment* segment);
		Segment* getSegment(ULONG segment);

		bool archiveExecute(Segment*);
		bool archiveSegment(Segment*);

		void switchActiveSegment();

		const Firebird::string& m_dbId;
		const Firebird::Guid& m_guid;
		const Config* const m_config;
		Firebird::Array<Segment*> m_segments;
		Firebird::AutoPtr<Firebird::SharedMemory<State> > m_sharedMemory;
		Firebird::Mutex m_localMutex;
		const FB_UINT64 m_sequence;
		ULONG m_generation;

		Firebird::Semaphore m_startupSemaphore;
		Firebird::Semaphore m_cleanupSemaphore;
		Firebird::Semaphore m_workingSemaphore;

		volatile bool m_shutdown;
	};

};

#endif // JRD_REPLICATION_CHANGELOG_H
