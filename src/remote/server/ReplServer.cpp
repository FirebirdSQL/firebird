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

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/common.h"
#include "../jrd/constants.h"
#include "../jrd/ibase.h"
#include "../jrd/license.h"
#include "../jrd/ods.h"
#include "../common/os/guid.h"
#include "../common/os/os_utils.h"
#include "../common/os/path_utils.h"
#include "../common/isc_proto.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/MetaName.h"
#include "../common/ThreadStart.h"
#include "../common/utils_proto.h"
#include "../common/utils_proto.h"

#include "../jrd/replication/Applier.h"
#include "../jrd/replication/ChangeLog.h"
#include "../jrd/replication/Config.h"
#include "../jrd/replication/Protocol.h"
#include "../jrd/replication/Utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef WIN_NT
#include <io.h>
#endif

#include "ReplServer.h"

#if defined(O_DSYNC)
#define SYNC		O_DSYNC
#elif defined(O_SYNC)
#define SYNC		O_SYNC
#elif defined(O_FSYNC)
#define SYNC		O_FSYNC
#else
#define SYNC		0
#endif

#ifndef O_BINARY
#define O_BINARY	0
#endif

// Debugging facilities
//#define NO_DATABASE
//#define PRESERVE_LOG

using namespace Firebird;
using namespace Replication;

namespace
{
	const char CTL_SIGNATURE[] = "FBREPLCTL";

	const USHORT CTL_VERSION1 = 1;
	const USHORT CTL_CURRENT_VERSION = CTL_VERSION1;

	volatile bool* shutdownPtr = NULL;
	AtomicCounter activeThreads;

	struct ActiveTransaction
	{
		ActiveTransaction()
			: tra_id(0), sequence(0)
		{}

		ActiveTransaction(TraNumber id, FB_UINT64 seq)
			: tra_id(id), sequence(seq)
		{}

	    static const TraNumber& generate(const ActiveTransaction& item)
		{
			return item.tra_id;
	    }

	    TraNumber tra_id;
		FB_UINT64 sequence;
	};

	typedef SortedArray<ActiveTransaction, EmptyStorage<ActiveTransaction>, TraNumber, ActiveTransaction> TransactionList;

	FB_UINT64 getOldestSequence(const TransactionList& transactions)
	{
		if (transactions.isEmpty())
			return 0;

		FB_UINT64 sequence = MAX_UINT64;

		for (const ActiveTransaction* iter = transactions.begin(); iter != transactions.end(); ++iter)
			sequence = MIN(sequence, iter->sequence);

		fb_assert(sequence > 0 && sequence < MAX_UINT64);

		return sequence;
	}

	class ControlFile : public AutoFile
	{
		struct DataV1
		{
			char signature[10];
			USHORT version;
			ULONG txn_count;
			FB_UINT64 sequence;
			ULONG offset;
			FB_UINT64 db_sequence;
		};

		typedef DataV1 Data;

	public:
		ControlFile(const PathName& directory,
					const Guid& guid, FB_UINT64 sequence,
					TransactionList& transactions)
			: AutoFile(init(directory, guid))
		{
			char guidStr[GUID_BUFF_SIZE];
			GuidToString(guidStr, &guid);

			const PathName filename = directory + guidStr;

#ifdef WIN_NT
			string name;
			name.printf("firebird_replctl_%s", guidStr);
			m_mutex = CreateMutex(NULL, FALSE, name.c_str());
			if (WaitForSingleObject(m_mutex, INFINITE) != WAIT_OBJECT_0)
#else // POSIX
#ifdef HAVE_FLOCK
			if (flock(m_handle, LOCK_EX))
#else
			if (lockf(m_handle, F_LOCK, 0))
#endif
#endif
			{
				raiseError("Control file %s lock failed (error: %d)", filename.c_str(), ERRNO);
			}

			memset(&m_data, 0, sizeof(Data));
			strcpy(m_data.signature, CTL_SIGNATURE);
			m_data.version = CTL_CURRENT_VERSION;

			const size_t length = (size_t) lseek(m_handle, 0, SEEK_END);

			if (!length)
			{
				m_data.sequence = sequence ? sequence - 1 : 0;
				m_data.offset = 0;
				m_data.db_sequence = 0;

				lseek(m_handle, 0, SEEK_SET);
				if (write(m_handle, &m_data, sizeof(Data)) != sizeof(Data))
					raiseError("Control file %s cannot be written", filename.c_str());
 			}
			else if (length >= sizeof(DataV1))
			{
				lseek(m_handle, 0, SEEK_SET);
				if (read(m_handle, &m_data, sizeof(DataV1)) != sizeof(DataV1))
					raiseError("Control file %s appears corrupted", filename.c_str());

				if (strcmp(m_data.signature, CTL_SIGNATURE) ||
					(m_data.version != CTL_VERSION1))
				{
					raiseError("Control file %s appears corrupted", filename.c_str());
				}

				ActiveTransaction* const ptr =
					m_data.txn_count ? transactions.getBuffer(m_data.txn_count) : NULL;
				const ULONG txn_size = m_data.txn_count * sizeof(ActiveTransaction);

				if (txn_size)
				{
					if (read(m_handle, ptr, txn_size) != txn_size)
						raiseError("Control file %s appears corrupted", filename.c_str());
				}
			}
			else
				raiseError("Control file %s appears corrupted", filename.c_str());
		}

		~ControlFile()
		{
#ifdef WIN_NT
			ReleaseMutex(m_mutex);
			CloseHandle(m_mutex);
#endif
		}

		FB_UINT64 getSequence() const
		{
			return m_data.sequence;
		}

		ULONG getOffset() const
		{
			return m_data.offset;
		}

		FB_UINT64 getDbSequence() const
		{
			return m_data.db_sequence;
		}

		void saveDbSequence(FB_UINT64 db_sequence)
		{
			m_data.db_sequence = db_sequence;

			lseek(m_handle, 0, SEEK_SET);
			write(m_handle, &m_data, sizeof(Data));
		}

		void savePartial(FB_UINT64 sequence, ULONG offset, const TransactionList& transactions)
		{
			bool update = false;

			if (sequence > m_data.sequence)
			{
				m_data.sequence = sequence;
				fb_assert(!m_data.offset);
				m_data.offset = offset;
				update = true;
			}
			else if (sequence == m_data.sequence && offset > m_data.offset)
			{
				m_data.offset = offset;
				update = true;
			}

			if (update)
			{
				m_data.txn_count = (ULONG) transactions.getCount();

				const ULONG txn_size = m_data.txn_count * sizeof(ActiveTransaction);

				lseek(m_handle, 0, SEEK_SET);
				write(m_handle, &m_data, sizeof(Data));
				write(m_handle, transactions.begin(), txn_size);
			}
		}

		void saveComplete(FB_UINT64 sequence, const TransactionList& transactions)
		{
			if (sequence >= m_data.sequence)
			{
				m_data.sequence = sequence;
				m_data.offset = 0;

				m_data.txn_count = (ULONG) transactions.getCount();

				const ULONG txn_size = m_data.txn_count * sizeof(ActiveTransaction);

				lseek(m_handle, 0, SEEK_SET);
				write(m_handle, &m_data, sizeof(Data));
				write(m_handle, transactions.begin(), txn_size);
			}
		}

	private:
		static int init(const PathName& directory, const Guid& guid)
		{
#ifdef WIN_NT
			const mode_t ACCESS_MODE = DEFAULT_OPEN_MODE;
#else
			const mode_t ACCESS_MODE = 0664;
#endif
			char guidStr[GUID_BUFF_SIZE];
			GuidToString(guidStr, &guid);

			const PathName filename = directory + guidStr;

			const int fd = os_utils::open(filename.c_str(),
				O_CREAT | O_RDWR | O_BINARY | SYNC, ACCESS_MODE);

			if (fd < 0)
				raiseError("Control file %s open failed (error: %d)", filename.c_str(), ERRNO);

			return fd;
		}

		Data m_data;

#ifdef WIN_NT
		HANDLE m_mutex;
#endif
	};

	class Target : public GlobalStorage
	{
	public:
		explicit Target(const Replication::Config* config)
			: m_config(config),
			  m_lastError(getPool()),
			  m_attachment(nullptr), m_replicator(nullptr),
			  m_sequence(0), m_connected(false)
		{
		}

		~Target()
		{
			shutdown();
		}

		const Replication::Config* getConfig() const
		{
			return m_config;
		}

		bool checkGuid(const Guid& guid)
		{
			if (!m_config->sourceGuid.alignment)
				return true;

			if (!memcmp(&guid, &m_config->sourceGuid, sizeof(Guid)))
				return true;

			return false;
		}

		FB_UINT64 initReplica()
		{
			if (m_connected)
				return m_sequence;

			verbose("Connecting to database (%s)", m_config->dbName.c_str());

			ClumpletWriter dpb(ClumpletReader::Tagged, MAX_DPB_SIZE, isc_dpb_version1);

			dpb.insertString(isc_dpb_user_name, DBA_USER_NAME);
			dpb.insertString(isc_dpb_config, EMBEDDED_PROVIDERS, fb_strlen(EMBEDDED_PROVIDERS));

#ifndef NO_DATABASE
			DispatcherPtr provider;
			FbLocalStatus localStatus;

			m_attachment =
				provider->attachDatabase(&localStatus, m_config->dbName.c_str(),
								   	     dpb.getBufferLength(), dpb.getBuffer());
			localStatus.check();

			m_replicator = m_attachment->createReplicator(&localStatus);
			localStatus.check();

			fb_assert(!m_sequence);

			const auto transaction = m_attachment->startTransaction(&localStatus, 0, NULL);
			localStatus.check();

			const char* sql =
				"select rdb$get_context('SYSTEM', 'REPLICATION_SEQUENCE') from rdb$database";

			FB_MESSAGE(Result, CheckStatusWrapper,
				(FB_BIGINT, sequence)
			) result(&localStatus, fb_get_master_interface());

			m_attachment->execute(&localStatus, transaction, 0, sql, SQL_DIALECT_V6,
								  NULL, NULL, result.getMetadata(), result.getData());
			localStatus.check();

			transaction->commit(&localStatus);
			localStatus.check();

			m_sequence = result->sequence;
#endif
			m_connected = true;

			return m_sequence;
		}

		void shutdown()
		{
			if (m_attachment)
			{
				verbose("Disconnecting from database (%s)", m_config->dbName.c_str());

#ifndef NO_DATABASE
				FbLocalStatus localStatus;
				m_replicator->close(&localStatus);
				m_attachment->detach(&localStatus);
#endif
				m_replicator = NULL;
				m_attachment = NULL;
				m_sequence = 0;
			}

			m_connected = false;
		}

		bool replicate(FbLocalStatus& status, ULONG length, const UCHAR* data)
		{
#ifdef NO_DATABASE
			return true;
#else
			m_replicator->process(&status, length, data);
			return status.isSuccess();
#endif
		}

		bool isShutdown() const
		{
			return (m_attachment == NULL);
		}

		const PathName& getDirectory() const
		{
			return m_config->logSourceDirectory;
		}

		void logMessage(const string& message, LogMsgType type) const
		{
			logReplicaMessage(m_config->dbName, message, type);
		}

		void logError(const string& message)
		{
			if (message != m_lastError)
			{
				logMessage(message, ERROR_MSG);
				m_lastError = message;
			}
		}

		void verbose(const char* msg, ...) const
		{
			if (m_config->verboseLogging)
			{
				char buffer[BUFFER_LARGE];

				va_list ptr;
				va_start(ptr, msg);
				VSNPRINTF(buffer, sizeof(buffer), msg, ptr);
				va_end(ptr);

				logMessage(buffer, VERBOSE_MSG);
			}
		}

	private:
		AutoPtr<const Replication::Config> m_config;
		string m_lastError;
		IAttachment* m_attachment;
		IReplicator* m_replicator;
		FB_UINT64 m_sequence;
		bool m_connected;
	};

	typedef Array<Target*> TargetList;

	struct LogSegment
	{
		explicit LogSegment(MemoryPool& pool, const PathName& fname, const SegmentHeader& hdr)
			: filename(pool, fname)
		{
			memcpy(&header, &hdr, sizeof(SegmentHeader));
		}

		void remove()
		{
#ifdef PRESERVE_LOG
			PathName path, name, newname;
			PathUtils::splitLastComponent(path, name, filename);
			PathUtils::concatPath(newname, path, "~" + name);

			if (rename(filename.c_str(), newname.c_str()) < 0)
				raiseError("Log file %s rename failed (error: %d)", filename.c_str(), ERRNO);
#else
			if (unlink(filename.c_str()) < 0)
				raiseError("Log file %s unlink failed (error: %d)", filename.c_str(), ERRNO);
#endif
		}

		static const FB_UINT64& generate(const LogSegment* item)
		{
			return item->header.hdr_sequence;
		}

		const PathName filename;
		SegmentHeader header;
	};

	typedef SortedArray<LogSegment*, EmptyStorage<LogSegment*>, FB_UINT64, LogSegment> ProcessQueue;

	void readConfig(TargetList& targets)
	{
		Array<Replication::Config*> replicas;
		Replication::Config::enumerate(replicas);

		for (auto replica : replicas)
			targets.add(FB_NEW Target(replica));
	}

	bool validateHeader(const SegmentHeader* header)
	{
		if (strcmp(header->hdr_signature, LOG_SIGNATURE))
			return false;

		if (header->hdr_version != LOG_CURRENT_VERSION)
			return false;

		if (header->hdr_state != SEGMENT_STATE_FREE &&
			header->hdr_state != SEGMENT_STATE_USED &&
			header->hdr_state != SEGMENT_STATE_FULL &&
			header->hdr_state != SEGMENT_STATE_ARCH)
		{
			return false;
		}

		if (header->hdr_protocol != PROTOCOL_VERSION1)
			return false;

		return true;
	}

	bool replicate(FbLocalStatus& status, FB_UINT64 sequence,
				   Target* target, TransactionList& transactions,
				   ULONG offset, ULONG length, const UCHAR* data,
				   bool rewind)
	{
		const Block* const header = (Block*) data;

		const auto traNumber = header->traNumber;

		if (!rewind || !traNumber || transactions.exist(traNumber))
		{
			if (!target->replicate(status, length, data))
				return false;
		}

		if (header->flags & BLOCK_END_TRANS)
		{
			if (traNumber)
			{
				FB_SIZE_T pos;
				if (transactions.find(traNumber, pos))
					transactions.remove(pos);
			}
			else if (!rewind)
			{
				transactions.clear();
			}
		}
		else if (header->flags & BLOCK_BEGIN_TRANS)
		{
			fb_assert(traNumber);

			if (!rewind && !transactions.exist(traNumber))
				transactions.add(ActiveTransaction(traNumber, sequence));
		}

		return true;
	}

	enum ProcessStatus { PROCESS_SUSPEND, PROCESS_CONTINUE, PROCESS_ERROR };

	ProcessStatus process_archive(MemoryPool& pool, Target* target)
	{
		FbLocalStatus localStatus;

		ProcessQueue queue(pool);

		ProcessStatus ret = PROCESS_SUSPEND;

		try
		{
			target->verbose("Scanning directory (%s)", target->getDirectory().c_str());

			// First pass: create the processing queue

			for (auto iter = PathUtils::newDirIterator(pool, target->getConfig()->logSourceDirectory);
				*iter; ++(*iter))
			{
				const auto filename = **iter;

#ifdef PRESERVE_LOG
				PathName path, name;
				PathUtils::splitLastComponent(path, name, filename);

				if (name.find('~') == 0)
					continue;
#endif

				if (filename.find('{') != PathName::npos &&
					filename.find('}') != PathName::npos &&
					filename.find('-') != PathName::npos)
				{
					continue;
				}

				const int fd = os_utils::open(filename.c_str(), O_RDONLY | O_BINARY);
				if (fd < 0)
				{
					if (errno == EACCES || errno == EAGAIN)
					{
						target->verbose("Skipping file (%s) due to sharing violation", filename.c_str());
						continue;
					}

					raiseError("Log file %s open failed (error: %d)", filename.c_str(), ERRNO);
				}

				AutoFile file(fd);

				struct stat stats;
				if (fstat(file, &stats) < 0)
					raiseError("Log file %s fstat failed (error: %d)", filename.c_str(), ERRNO);

				const size_t fileSize = stats.st_size;

				if (fileSize < sizeof(SegmentHeader))
				{
					target->verbose("Skipping file (%s) as being too small (at least %u bytes expected, %u bytes detected)",
									filename.c_str(), sizeof(SegmentHeader), fileSize);
					continue;
				}

				if (lseek(file, 0, SEEK_SET) != 0)
					raiseError("Log file %s seek failed (error: %d)", filename.c_str(), ERRNO);

				SegmentHeader header;

				if (read(file, &header, sizeof(SegmentHeader)) != sizeof(SegmentHeader))
					raiseError("Log file %s read failed (error: %d)", filename.c_str(), ERRNO);

				if (!validateHeader(&header))
				{
					target->verbose("Skipping file (%s) due to unknown format", filename.c_str());
					continue;
				}

				if (fileSize < header.hdr_length)
				{
					target->verbose("Skipping file (%s) as being too small (at least %u bytes expected, %u bytes detected)",
									filename.c_str(), header.hdr_length, fileSize);
					continue;
				}

				if (header.hdr_state == SEGMENT_STATE_FREE)
				{
					target->verbose("Deleting file (%s) due to incorrect state (expected either FULL or ARCH, found FREE)",
									filename.c_str());
					file.release();
					unlink(filename.c_str());
					continue;
				}

				if (!target->checkGuid(header.hdr_guid))
				{
					char buff[GUID_BUFF_SIZE];
					GuidToString(buff, &header.hdr_guid);
					const string guidStr(buff);
					target->verbose("Skipping file (%s) due to GUID mismatch (found %s)",
									filename.c_str(), guidStr.c_str());
					continue;
				}
/*
				if (header.hdr_state != SEGMENT_STATE_ARCH)
					continue;
*/
				queue.add(FB_NEW_POOL(pool) LogSegment(pool, filename, header));
			}

			if (queue.isEmpty())
			{
				target->verbose("No suitable files found");
				return ret;
			}

			target->verbose("Added %u segments to the processing queue", (ULONG) queue.getCount());

			// Second pass: replicate the chain of contiguous segments

			Array<UCHAR> buffer(pool);
			TransactionList transactions(pool);

			FB_UINT64 next_sequence = 0;
			const bool restart = target->isShutdown();

			for (LogSegment** iter = queue.begin(); iter != queue.end(); ++iter)
			{
				LogSegment* const segment = *iter;
				const FB_UINT64 sequence = segment->header.hdr_sequence;
				const Guid& guid = segment->header.hdr_guid;

				ControlFile control(target->getDirectory(), guid, sequence, transactions);

				FB_UINT64 last_sequence = control.getSequence();
				ULONG last_offset = control.getOffset();

				const FB_UINT64 db_sequence = target->initReplica();
				const FB_UINT64 last_db_sequence = control.getDbSequence();

				if (sequence <= db_sequence)
				{
					target->verbose("Deleting file (%s) due to fast forward", segment->filename.c_str());
					segment->remove();
					continue;
				}

				if (db_sequence != last_db_sequence)
				{
					target->verbose("Resetting replication to continue from segment %" UQUADFORMAT, db_sequence + 1);
					control.saveDbSequence(db_sequence);
					transactions.clear();
					control.saveComplete(db_sequence, transactions);
					last_sequence = db_sequence;
					last_offset = 0;
				}

				FB_UINT64 oldest_sequence = getOldestSequence(transactions);

				const FB_UINT64 threshold = oldest_sequence ? oldest_sequence :
					(last_offset ? last_sequence : last_sequence + 1);

				if (sequence < threshold)
				{
					target->verbose("Deleting file (%s) as priorly replicated", segment->filename.c_str());
					segment->remove();
					continue;
				}

				if (!next_sequence)
					next_sequence = restart ? threshold : last_sequence + 1;

 				if (sequence > next_sequence)
 					raiseError("Required segment %" UQUADFORMAT " is missing", next_sequence);

				if (sequence < next_sequence)
					continue;

				target->verbose("Replicating file (%s), segment %" UQUADFORMAT,
								segment->filename.c_str(), sequence);

				const FB_UINT64 org_oldest_sequence = oldest_sequence;

				const int fd = os_utils::open(segment->filename.c_str(), O_RDONLY | O_BINARY);
				if (fd < 0)
				{
					if (errno == EACCES || errno == EAGAIN)
					{
						target->verbose("Stopping to process the queue, sharing violation for file (%s)",
										segment->filename.c_str());
						break;
					}

					raiseError("Log file %s open failed (error: %d)", segment->filename.c_str(), ERRNO);
				}

				AutoFile file(fd);

				SegmentHeader header;

				if (read(file, &header, sizeof(SegmentHeader)) != sizeof(SegmentHeader))
					raiseError("Log file %s read failed (error: %d)", segment->filename.c_str(), ERRNO);

				if (memcmp(&header, &segment->header, sizeof(SegmentHeader)))
					raiseError("Log file %s was unexpectedly changed", segment->filename.c_str());

				ULONG totalLength = sizeof(SegmentHeader);
				while (totalLength < segment->header.hdr_length)
				{
					Block header;
					if (read(file, &header, sizeof(Block)) != sizeof(Block))
						raiseError("Log file %s read failed (error %d)", segment->filename.c_str(), ERRNO);

					const auto blockLength = header.dataLength + header.metaLength;
					const auto length = sizeof(Block) + blockLength;

					if (blockLength)
					{
						const bool rewind = (sequence < last_sequence ||
							(sequence == last_sequence && (!last_offset || totalLength < last_offset)));

						UCHAR* const data = buffer.getBuffer(length);
						memcpy(data, &header, sizeof(Block));

						if (read(file, data + sizeof(Block), blockLength) != blockLength)
							raiseError("Log file %s read failed (error %d)", segment->filename.c_str(), ERRNO);

						const bool success =
							replicate(localStatus, sequence,
									  target, transactions,
									  totalLength, length, data,
									  rewind);

						if (!success)
						{
							oldest_sequence = getOldestSequence(transactions);

							target->verbose("Last segment:offset %" UQUADFORMAT ":%u, oldest segment %" UQUADFORMAT,
								control.getSequence(), control.getOffset(), oldest_sequence);

							localStatus.raise();
						}
					}

					totalLength += length;

					control.savePartial(sequence, totalLength, transactions);
				}

				control.saveComplete(sequence, transactions);

				file.release();

				target->verbose("Successfully replicated %u bytes in segment %" UQUADFORMAT,
								totalLength, sequence);

				oldest_sequence = getOldestSequence(transactions);
				next_sequence = sequence + 1;

				target->verbose("Last segment:offset %" UQUADFORMAT ":%u, oldest segment %" UQUADFORMAT,
					control.getSequence(), control.getOffset(), oldest_sequence);

				if (org_oldest_sequence && oldest_sequence != org_oldest_sequence)
				{
					const FB_UINT64 threshold =
						oldest_sequence ? MIN(oldest_sequence, sequence) : sequence;

					FB_SIZE_T pos;
					if (queue.find(org_oldest_sequence, pos))
					{
						do
						{
							LogSegment* const segment = queue[pos++];

							if (segment->header.hdr_sequence >= threshold)
								break;

							target->verbose("Deleting file (%s) as already replicated",
											segment->filename.c_str());

							segment->remove();
						} while (pos < queue.getCount());
					}
				}

				if (oldest_sequence)
				{
					target->verbose("Preserving file (%s) due to uncommitted transactions",
									segment->filename.c_str());
				}
				else
				{
					target->verbose("Deleting file (%s) as already replicated",
									segment->filename.c_str());
				}

				if (!oldest_sequence)
					segment->remove();

				ret = PROCESS_CONTINUE;
			}
		}
		catch (const Exception& ex)
		{
			LocalStatus localStatus;
			CheckStatusWrapper statusWrapper(&localStatus);
			ex.stuffException(&statusWrapper);

			string message;

			char temp[BUFFER_LARGE];
			const ISC_STATUS* status_ptr = localStatus.getErrors();
			while (fb_interpret(temp, sizeof(temp), &status_ptr))
			{
				if (!message.isEmpty())
					message += "\n\t";

				message += temp;
			}

			if (message.find("Replication") == string::npos)
				target->logError(message);

			ret = PROCESS_ERROR;
		}

		while (queue.hasData())
			delete queue.pop();

		return ret;
	}

	THREAD_ENTRY_DECLARE process_thread(THREAD_ENTRY_PARAM arg)
	{
		fb_assert(shutdownPtr);

		AutoPtr<Target> target(static_cast<Target*>(arg));
		const auto config = target->getConfig();

		target->verbose("Started replication thread");

		while (!*shutdownPtr)
		{
			AutoMemoryPool workingPool(MemoryPool::createPool());
			ContextPoolHolder threadContext(workingPool);

			const ProcessStatus ret = process_archive(*workingPool, target);

			if (ret == PROCESS_CONTINUE)
				continue;

			target->shutdown();

			if (!*shutdownPtr)
			{
				const ULONG timeout =
					(ret == PROCESS_SUSPEND) ? config->applyIdleTimeout : config->applyErrorTimeout;

				target->verbose("Going to sleep for %u seconds", timeout);

				Thread::sleep(timeout * 1000);
			}
		}

		target->verbose("Finished replication thread");

		--activeThreads;

		return 0;
	}
}

bool REPL_server(CheckStatusWrapper* status, bool wait, bool* aShutdownPtr)
{
	try
	{
		shutdownPtr = aShutdownPtr;

		TargetList targets;
		readConfig(targets);

		for (auto target : targets)
		{
			++activeThreads;
			Thread::start((ThreadEntryPoint*) process_thread, target, THREAD_medium, NULL);
		}

		if (wait)
		{
			do {
				Thread::sleep(100);
			} while (activeThreads.value());
		}
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return false;
	}

	return true;
}
