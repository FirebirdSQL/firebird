/*
 *	PROGRAM:	JRD Access method
 *	MODULE:		tpc_proto.h
 *	DESCRIPTION:	Prototype Header file for tpc.cpp
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
 */

#ifndef JRD_TPC_PROTO_H
#define JRD_TPC_PROTO_H

#include <atomic>
#include "../common/classes/array.h"
#include "../common/classes/SyncObject.h"

namespace Ods {

struct tx_inv_page;

}

namespace Jrd {

class Database;
class thread_db;
class TipCache;
class ActiveSnapshots;

// Special values of CommitNumber reserved for uncommitted transaction states
const CommitNumber
	CN_PREHISTORIC = 1,
	CN_ACTIVE = 0,
	CN_LIMBO = static_cast<CommitNumber>(-1),
	CN_DEAD = static_cast<CommitNumber>(-2),
	CN_MAX_NUMBER = static_cast<CommitNumber>(-3); // Assume CommitNumber is unsigned integer

class TipCache
{
public:
	// XXX: maybe use static factory method to create and initialize instance?
	explicit TipCache(Database* dbb);
	~TipCache();

	// Attach to shared memory objects and populate process-local structures.
	// If shared memory area did not exist - populate initial TIP by reading cache
	// from disk.
	// This function has no in-process synchronization, assuming caller prevents
	// concurrent use of object being initialized.
	void initializeTpc(thread_db *tdbb);

	// Disconnect from shared memory objects
	void finalizeTpc(thread_db* tdbb);

	// Get the current state of a transaction in the cache
	CommitNumber cacheState(TraNumber number);

	// Return the oldest transaction in the given state.
	// Lookup in the [min_number, max_number) bounds.
	// If not found, return zero.
	TraNumber findStates(TraNumber minNumber, TraNumber maxNumber, ULONG mask, int& state);

	// Set state for a transaction in cache. If new state is tra_committed and
	// commit# is not assigned yet, then assign it now.
	// Return new state for a transaction in cache.
	//
	// tra_active is the initial set of a transaction and it cannot be assigned.
	//
	// Valid assignments are:
	//   - tra_active -> tra_committed
	//   - tra_active -> tra_limbo
	//   - tra_active -> tra_dead
	//   - tra_limbo -> tra_committed
	//   - tra_limbo -> tra_dead
	// All other combinations are expressly forbidden with BUGCHECK error.
	CommitNumber setState(TraNumber number, SSHORT state);

	// Compute most current state of a transaction. This is called when we encountered
	// a record produced by this transaction and need to know what to do with this record.
	// This function needs to be very fast as it will be called quite frequently.
	// Since we shall be using snapshots for read committed, concurrency and consistency
	// modes, calling this function is not necessary there. Only system transaction, GC and
	// sweep threads are expected to use it. For all other cases cacheState is sufficient.
	//
	// The function shall perform as follows:
	// 1) read return transaction state (and commit#) from shared memory cache. If
	//   transaction is known committed (with commit#) or dead - just return the state.
	// 2) If transaction is active, or in limbo, query its data using lock manager.
	//   If we get non-zero return - we assume transaction is still active and our cached
	//   state is valid.
	// 3) If our query returned zero and transaction state is still unassigned after that
	//   it means that process handling the transaction has terminated abnormally
	//   (as normal protocol is to release the lock only after we notified global cache
	//   of commit). In this case re-fetch transaction state from disk and update cache.
	//   If transaction has been found committed on disk then assign new commit# to it.
	//   If transaction is marked active on disk, mark it dead.
	CommitNumber snapshotState(thread_db* tdbb, TraNumber number);

	// If engine has recalculated Oldest (interesting) transaction or Oldest Snapshot
	// number for the database (via Sweep or during regular transaction start) it
	// needs to notify TPC, so that it can release cache resources. Any transaction
	// with number less than oldest interesting transaction and oldest snapshot
	// is treated as "prehistoric" committed (CN_PREHISTORIC)
	void updateOldestTransaction(thread_db* tdbb, TraNumber oldest, TraNumber oldestSnapshot);

	// Create snapshot. The snapshot shall use only versions committed
	// before commitNumber (the latest CN when it's 0). Snapshots inhibit GC to some extent.
	// When snapshot is no longer needed you call endSnapshot.
	SnapshotHandle beginSnapshot(thread_db* tdbb, AttNumber attachmentId, CommitNumber& commitNumber);

	// Deallocate snapshot.
	void endSnapshot(thread_db* tdbb, SnapshotHandle handle, AttNumber attachmentId);

	// Get the list of active snapshots for GC purposes. This function can be
	// called multiple times with the same object to obtain most recent information.
	// During initialization it checks that all attachments holding snapshots are
	// actually alive (via lock manager) to prevent GC inhibition in case of sudden
	// death of attachment process. Update is quick and happens only if needed and
	// there is no contention.
	void updateActiveSnapshots(thread_db* tdbb, ActiveSnapshots* activeSnapshots);

	// Transactions, attachments, statements ID management.
	TraNumber generateTransactionId();
	AttNumber generateAttachmentId();
	StmtNumber generateStatementId();
	//void assignLatestTransactionId(TraNumber number);
	void assignLatestAttachmentId(AttNumber number);

	CommitNumber getGlobalCommitNumber() const
	{
		return m_tpcHeader->getHeader()->latest_commit_number.load(std::memory_order_acquire);
	}

private:
	class GlobalTpcHeader : public Firebird::MemoryHeader
	{
	public:
		std::atomic<CommitNumber> latest_commit_number;

		// We do not need hardware barriers when working with this variable, because
		// TPC is essentially log-structured and we have extra large safety margin
		// during shared memory clean-up operations to cover expected fuzziness.
		// The assumption of the code is that it is not possible to process full
		// memory block worth of transactions during the period of cache decoherence
		// of any one CPU accessing this variable
		std::atomic<TraNumber> oldest_transaction;

		// Incremented each time whenever snapshot is released
		std::atomic<ULONG> snapshot_release_count;

		// Shared counters
		std::atomic<TraNumber> latest_transaction_id;
		std::atomic<AttNumber> latest_attachment_id;
		std::atomic<StmtNumber> latest_statement_id;

		// Size of memory chunk with TransactionStatusBlock
		ULONG tpc_block_size; // final
	};

	struct SnapshotData
	{
		std::atomic<AttNumber> attachment_id; // Unused slots have attachment_id == 0
		std::atomic<CommitNumber> snapshot;
	};

	// Note: when maintaining this structure, we are extra careful
	// to keep it consistent at all times, so that the process using it
	// can be killed at any time without adverse consequences.
	class SnapshotList : public Firebird::MemoryHeader
	{
	public:
		std::atomic<ULONG> slots_allocated;
		std::atomic<ULONG> slots_used;
		ULONG min_free_slot; // Position where to start looking for free space
		SnapshotData slots[1];
	};

	class TransactionStatusBlock : public Firebird::MemoryHeader
	{
	public:
		std::atomic<CommitNumber> data[1];
	};

	typedef TransactionStatusBlock* PTransactionStatusBlock;

	class StatusBlockData
	{
	public:
		StatusBlockData(Jrd::thread_db* tdbb, Jrd::TipCache* tipCache, int blkNumber);
		~StatusBlockData();

		int blockNumber;
		Firebird::SharedMemory<TransactionStatusBlock>* memory;
		Lock existenceLock;
		TipCache* cache;

		inline static int& generate(const void* /*sender*/, StatusBlockData* item)
		{
			return item->blockNumber;
		}

		void clear(Jrd::thread_db* tdbb);
	};

	class MemoryInitializer : public Firebird::IpcObject
	{
	public:
		explicit MemoryInitializer(TipCache *cache) : m_cache(cache) {}
		void mutexBug(int osErrorCode, const char* text);
	protected:
		TipCache* m_cache;
	};

	class GlobalTpcInitializer : public MemoryInitializer
	{
	public:
		explicit GlobalTpcInitializer(TipCache *cache) : MemoryInitializer(cache) {}
		bool initialize(Firebird::SharedMemoryBase* sm, bool initFlag);
	};

	class SnapshotsInitializer : public MemoryInitializer
	{
	public:
		explicit SnapshotsInitializer(TipCache *cache) : MemoryInitializer(cache) {}
		bool initialize(Firebird::SharedMemoryBase* sm, bool initFlag);
	};

	class MemBlockInitializer : public MemoryInitializer
	{
	public:
		explicit MemBlockInitializer(TipCache *cache) : MemoryInitializer(cache) {}
		bool initialize(Firebird::SharedMemoryBase* sm, bool initFlag);
	};

	typedef Firebird::BePlusTree<StatusBlockData*, int, Firebird::MemoryPool, StatusBlockData> BlocksMemoryMap;

	static const ULONG TPC_VERSION = 1;
	static const int SAFETY_GAP_BLOCKS = 1;

	Database* m_dbb; // final
	Firebird::SharedMemory<GlobalTpcHeader>* m_tpcHeader; // final
	Firebird::SharedMemory<SnapshotList>* m_snapshots; // final
	ULONG m_transactionsPerBlock; // final. When set, we assume TPC has been initialized.

	GlobalTpcInitializer globalTpcInitializer;
	SnapshotsInitializer snapshotsInitializer;
	MemBlockInitializer memBlockInitializer;

	// Tree with TIP cache memory blocks
	// Reads and writes to the tree are protected with m_sync_status.
	BlocksMemoryMap m_blocks_memory;

	Firebird::SyncObject m_sync_status;

	void initTransactionsPerBlock();

	// Returns block holding transaction state.
	// Returns NULL if requested block is too old and is no longer cached.
	TransactionStatusBlock* getTransactionStatusBlock(int blockNumber);

	// Map shared memory for a block
	TransactionStatusBlock* createTransactionStatusBlock(int blockNumber);

	// Release shared memory blocks, if possible.
	// We utilize one full MemoryBlock as a safety margin to account for possible
	// race conditions during lockless operations, so this operation shall be pretty safe.
	void releaseSharedMemory(thread_db *tdbb, TraNumber oldest_old, TraNumber oldest_new);

	// Populate TIP cache from disk
	void loadInventoryPages(thread_db *tdbb);

	static int tpc_block_blocking_ast(void* arg);

	SnapshotHandle allocateSnapshotSlot();
	void deallocateSnapshotSlot(SnapshotHandle slotNumber);
	void remapSnapshots(bool sync);
};


inline int TPC_cache_state(thread_db* tdbb, TraNumber number)
{
	CommitNumber stateCn = tdbb->getDatabase()->dbb_tip_cache->cacheState(number);
	switch (stateCn)
	{
	case CN_ACTIVE:	return tra_active;
	case CN_LIMBO:	return tra_limbo;
	case CN_DEAD:	return tra_dead;
	default:		return tra_committed;
	}
}

inline TraNumber TPC_find_states(thread_db* tdbb, TraNumber minNumber, TraNumber maxNumber,
	ULONG mask, int& state)
{
	return tdbb->getDatabase()->dbb_tip_cache->findStates(minNumber, maxNumber, mask, state);
}

inline void TPC_set_state(thread_db* tdbb, TraNumber number, SSHORT state)
{
	tdbb->getDatabase()->dbb_tip_cache->setState(number, state);
}

int TPC_snapshot_state(thread_db* tdbb, TraNumber number);

} // namespace Jrd

#endif // JRD_TPC_PROTO_H
