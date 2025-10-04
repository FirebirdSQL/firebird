/*
 *	PROGRAM:	JRD Backup and Restore Program
 *	MODULE:		BurpTasks.h
 *	DESCRIPTION:
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
 * The Original Code was created by Khorsun Vladyslav
 * for the Firebird Open Source RDBMS project.
 *
 * Copyright (c) 2019 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 * and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 */

#ifndef BURP_TASKS_H
#define BURP_TASKS_H

#include <stdio.h>
#include "../common/common.h"
#include "../burp/burp.h"
#include "../common/ThreadData.h"
#include "../common/Task.h"
#include "../common/UtilSvc.h"
#include "../common/classes/array.h"
#include "../common/classes/auto.h"
#include "../common/classes/condition.h"
#include "../common/classes/fb_atomic.h"

namespace Burp {

class ReadRelationMeta
{
public:
	ReadRelationMeta() noexcept :
		m_blr(*getDefaultMemoryPool())
	{
		clear();
	}

	void setRelation(const burp_rel* relation, bool partition);
	void clear();

	bool haveInputs() const noexcept
	{
		return m_inMgsNum != m_outMgsNum;
	}

//private:
	const burp_rel* m_relation;
	SSHORT m_fldCount;
	SSHORT m_inMgsNum;
	SSHORT m_outMgsNum;
	Firebird::HalfStaticArray<UCHAR, 256> m_blr;
	RCRD_LENGTH m_outMsgLen;
	RCRD_LENGTH m_outRecLen;
	RCRD_OFFSET m_outEofOffset;
};

class ReadRelationReq
{
public:
	ReadRelationReq() noexcept :
		m_outMsg(*getDefaultMemoryPool())
	{
	}

	~ReadRelationReq()
	{
		clear();
	}

	void reset(const ReadRelationMeta* meta);
	void clear();

	void compile(Firebird::CheckStatusWrapper* status, Firebird::IAttachment* db);
	void setParams(ULONG loPP, ULONG hiPP);
	void start(Firebird::CheckStatusWrapper* status, Firebird::ITransaction* tran);
	void receive(Firebird::CheckStatusWrapper* status);
	void release(Firebird::CheckStatusWrapper* status);

	const ReadRelationMeta* getMeta() const noexcept
	{
		return m_meta;
	}

	const UCHAR* getData() const noexcept
	{
		return m_outMsg.begin();
	}

	bool eof() const noexcept
	{
		return *m_eof;
	}

private:
	struct InMsg
	{
		ULONG loPP;
		ULONG hiPP;
	};

	const burp_rel* m_relation = nullptr;
	const ReadRelationMeta* m_meta = nullptr;
	InMsg m_inMgs{};
	Firebird::Array<UCHAR> m_outMsg;
	SSHORT* m_eof = nullptr;
	Firebird::IRequest* m_request = nullptr;
};


class WriteRelationMeta
{
public:
	WriteRelationMeta() noexcept :
		m_blr(*getDefaultMemoryPool())
	{
		clear();
	}

	void setRelation(BurpGlobals* tdgbl, const burp_rel* relation);
	void clear();

	Firebird::IBatch* createBatch(BurpGlobals* tdgbl, Firebird::IAttachment* att);

//private:
	bool prepareBatch(BurpGlobals* tdgbl);
	void prepareRequest(BurpGlobals* tdgbl);

	const burp_rel* m_relation;
	Firebird::Mutex m_mutex;
	bool m_batchMode;
	bool m_batchOk;
	ULONG m_inMsgLen;
	ULONG m_blobCount;

	// batch mode
	Firebird::string m_sqlStatement;
	Firebird::RefPtr<Firebird::IMessageMetadata> m_batchMeta;
	unsigned m_batchStep;
	unsigned m_batchInlineBlobLimit;

	// request mode
	SSHORT m_inMgsNum;
	Firebird::HalfStaticArray<UCHAR, 256> m_blr;
};

class WriteRelationReq
{
public:
	WriteRelationReq() noexcept :
		m_inMsg(*getDefaultMemoryPool()),
		m_batchMsg(*getDefaultMemoryPool())
	{
	}

	~WriteRelationReq()
	{
		clear();
	}

	void reset(WriteRelationMeta* meta);
	void clear();

	void compile(BurpGlobals* tdgbl, Firebird::IAttachment* att);
	void send(BurpGlobals* tdgbl, Firebird::ITransaction* tran, bool lastRec);
	void execBatch(BurpGlobals * tdgbl);
	void release();

	ULONG getDataLength() const noexcept
	{
		return m_inMsg.getCount();
	}

	UCHAR* getData() noexcept
	{
		return m_inMsg.begin();
	}

	Firebird::IBatch* getBatch() const noexcept
	{
		return m_batch;
	}

	ULONG getBatchMsgLength() const noexcept
	{
		return m_batchMsg.getCount();
	}

	UCHAR* getBatchMsgData() noexcept
	{
		return m_batchMsg.begin();
	}

	unsigned getBatchInlineBlobLimit() const noexcept
	{
		return m_meta->m_batchInlineBlobLimit;
	}

private:
	const burp_rel* m_relation = nullptr;
	WriteRelationMeta* m_meta = nullptr;
	Firebird::Array<UCHAR> m_inMsg;
	Firebird::Array<UCHAR> m_batchMsg;
	Firebird::IBatch* m_batch = nullptr;
	Firebird::IRequest* m_request = nullptr;
	int m_recs = 0;						// total records sent
	int m_batchRecs = 0;				// records in current batch
	bool m_resync = true;
};

// forward declarations
class IOBuffer;
class BurpMaster;
class BurpTask;



// Common base class for backup and restore task items
class BurpTaskItem : public Firebird::Task::WorkItem
{
public:
	BurpTaskItem(BurpTask* task);

	BurpTask* getBurpTask() const noexcept;
};

// Common base class for backup and restore tasks
class BurpTask : public Firebird::Task
{
public:
	BurpTask(BurpGlobals* tdgbl) : Firebird::Task(),
		m_masterGbl(tdgbl)
	{ }

	static BurpTask* getBurpTask(BurpGlobals* tdgbl) noexcept
	{
		if (tdgbl->taskItem)
			return tdgbl->taskItem->getBurpTask();

		return nullptr;
	}

	BurpGlobals* getMasterGbl() const noexcept
	{
		return m_masterGbl;
	}

	bool isBackup() const noexcept
	{
		switch (m_masterGbl->action->act_action)
		{
		case ACT_backup:
		case ACT_backup_split:
		case ACT_backup_fini:
			return true;
		default:
			return false;
		}
	}

	bool isRestore() const noexcept
	{
		switch (m_masterGbl->action->act_action)
		{
		case ACT_restore:
		case ACT_restore_join:
			return true;
		default:
			return false;
		}
	}

protected:
	BurpGlobals* const m_masterGbl;

private:
	friend class BurpMaster;

	Firebird::Mutex burpOutMutex;
};


inline BurpTaskItem::BurpTaskItem(BurpTask* task) :
	Firebird::Task::WorkItem(task)
{
}

inline BurpTask* BurpTaskItem::getBurpTask() const noexcept
{
	return static_cast<BurpTask*>(m_task);
}


class BackupRelationTask : public BurpTask
{
public:
	BackupRelationTask(BurpGlobals* tdgbl);
	~BackupRelationTask();

	void SetRelation(burp_rel* relation);

	bool handler(WorkItem& _item) override;
	bool getWorkItem(WorkItem** pItem) override;
	bool getResult(Firebird::IStatus* status) override;
	int getMaxWorkers() override;

	class Item : public BurpTaskItem
	{
	public:
		Item(BackupRelationTask* task, bool writer) : BurpTaskItem(task),
			m_writer(writer),
			m_ownAttach(!writer),
			m_cleanBuffers(*getDefaultMemoryPool())
		{}

		BackupRelationTask* getBackupTask() const noexcept
		{
			return static_cast<BackupRelationTask*>(m_task);
		}

		class EnsureUnlockBuffer
		{
		public:
			EnsureUnlockBuffer(Item* item) noexcept : m_item(item) {}
			~EnsureUnlockBuffer();

		private:
			Item* m_item;
		};

		bool m_inuse = false;
		bool m_writer;			// file writer or table reader
		bool m_ownAttach;
		BurpGlobals* m_gbl = nullptr;
		Firebird::IAttachment* m_att = nullptr;
		Firebird::ITransaction* m_tra = nullptr;
		burp_rel* m_relation = nullptr;
		ReadRelationReq m_request;
		ULONG m_ppSequence = 0;		// PP to read

		Firebird::Mutex m_mutex;
		Firebird::HalfStaticArray<IOBuffer*, 2> m_cleanBuffers;
		IOBuffer* m_buffer = nullptr;
		Firebird::Condition m_cleanCond;
	};

	static BackupRelationTask* getBackupTask(BurpGlobals* tdgbl) noexcept
	{
		auto task = BurpTask::getBurpTask(tdgbl);
		fb_assert(!task || task->isBackup());

		return static_cast<BackupRelationTask*>(task);
	}

	static void recordAdded(BurpGlobals* tdgbl);			// reader
	static IOBuffer* renewBuffer(BurpGlobals* tdgbl);		// reader

	bool isStopped() const noexcept
	{
		return m_stop;
	}

private:
	void initItem(BurpGlobals* tdgbl, Item& item);
	void freeItem(Item& item);
	void stopItems();
	bool fileWriter(Item& item);
	bool tableReader(Item& item);

	void releaseBuffer(Item& item);			// reader
	IOBuffer* getCleanBuffer(Item& item);	// reader
	void putDirtyBuffer(IOBuffer* buf);		// reader

	IOBuffer* getDirtyBuffer();				// writer
	void putCleanBuffer(IOBuffer* buf);		// writer

	burp_rel*	m_relation;
	ReadRelationMeta m_metadata;
	int m_readers;			// number of active readers, could be less than items allocated
	bool m_readDone;		// true when all readers are done
	ULONG m_nextPP;

	Firebird::Mutex m_mutex;
	Firebird::HalfStaticArray<Item*, 8> m_items;
	volatile bool m_stop;
	bool m_error;

	Firebird::HalfStaticArray<IOBuffer*, 16> m_buffers;
	Firebird::HalfStaticArray<IOBuffer*, 8> m_dirtyBuffers;
	Firebird::Condition m_dirtyCond;
};


class RestoreRelationTask : public BurpTask
{
public:
	RestoreRelationTask(BurpGlobals* tdgbl);
	~RestoreRelationTask();

	void SetRelation(BurpGlobals* tdgbl, burp_rel* relation);

	bool handler(WorkItem& _item) override;
	bool getWorkItem(WorkItem** pItem) override;
	bool getResult(Firebird::IStatus* status) override;
	int getMaxWorkers() override;

	class Item : public BurpTaskItem
	{
	public:
		Item(RestoreRelationTask* task, bool reader) : BurpTaskItem(task),
			m_reader(reader),
			m_ownAttach(!reader)
		{}

		RestoreRelationTask* getRestoreTask() const noexcept
		{
			return static_cast<RestoreRelationTask*>(m_task);
		}

		class EnsureUnlockBuffer
		{
		public:
			EnsureUnlockBuffer(Item* item) noexcept : m_item(item) {}
			~EnsureUnlockBuffer();

		private:
			Item* m_item;
		};

		bool m_inuse = false;
		bool m_reader;			// file reader or table writer
		bool m_ownAttach;
		BurpGlobals* m_gbl = nullptr;
		Firebird::IAttachment* m_att = nullptr;
		Firebird::ITransaction* m_tra = nullptr;
		burp_rel* m_relation = nullptr;
		WriteRelationReq m_request;

		Firebird::Mutex m_mutex;
		IOBuffer* m_buffer = nullptr;
	};

	class ExcReadDone : public Firebird::Exception
	{
	public:
		ExcReadDone() noexcept : Firebird::Exception() { }
		virtual void stuffByException(Firebird::StaticStatusVector& status_vector) const noexcept;
		virtual const char* what() const noexcept;
		[[noreturn]] static void raise();
	};

	static RestoreRelationTask* getRestoreTask(BurpGlobals* tdgbl) noexcept
	{
		const auto* task = BurpTask::getBurpTask(tdgbl);
		fb_assert(!task || task->isRestore());

		return static_cast<RestoreRelationTask*>(BurpTask::getBurpTask(tdgbl));
	}

	static IOBuffer* renewBuffer(BurpGlobals* tdgbl);		// writer

	bool isStopped() const noexcept
	{
		return m_stop;
	}

	rec_type getLastRecord() const noexcept
	{
		return m_lastRecord;
	}

	void verbRecs(FB_UINT64& records, bool total);
	void verbRecsFinal();

	// commit and detach all worker connections
	bool finish();

private:
	void initItem(BurpGlobals* tdgbl, Item& item);
	bool freeItem(Item& item, bool commit);
	bool fileReader(Item& item);
	bool tableWriter(BurpGlobals* tdgbl, Item& item);

	void releaseBuffer(Item& item);			// writer
	// reader needs clean buffer to read backup file into
	IOBuffer* getCleanBuffer();				// reader
	// put buffer full of records to be handled by writer
	void putDirtyBuffer(IOBuffer* buf);		// reader

	IOBuffer* getDirtyBuffer();				// writer
	void putCleanBuffer(IOBuffer* buf);		// writer

	void checkSpace(IOBuffer** pBuf, const FB_SIZE_T length, UCHAR** pData, FB_SIZE_T* pSpace);
	IOBuffer* read_blob(BurpGlobals* tdgbl, IOBuffer* ioBuf);
	IOBuffer* read_array(BurpGlobals* tdgbl, IOBuffer* ioBuf);

	burp_rel*	m_relation;
	rec_type	m_lastRecord;				// last backup record read for relation, usually rec_relation_end
	WriteRelationMeta m_metadata;
	int m_writers;			// number of active writers, could be less than items allocated
	bool m_readDone;		// all records was read

	Firebird::Mutex m_mutex;
	Firebird::HalfStaticArray<Item*, 8> m_items;
	volatile bool m_stop;
	bool m_error;
	Firebird::AtomicCounter m_records;		// records restored for the current relation
	FB_UINT64 m_verbRecs;					// last records count reported

	Firebird::HalfStaticArray<IOBuffer*, 16> m_buffers;
	Firebird::HalfStaticArray<IOBuffer*, 16> m_cleanBuffers;
	Firebird::HalfStaticArray<IOBuffer*, 16> m_dirtyBuffers;
	Firebird::Condition m_cleanCond;
	Firebird::Condition m_dirtyCond;
};


class IOBuffer
{
public:
	IOBuffer(BurpTaskItem*, FB_SIZE_T size);

	UCHAR* getBuffer() const noexcept
	{
		return m_aligned;
	}

	FB_SIZE_T getSize() const noexcept
	{
		return m_size;
	}

	FB_SIZE_T getRecs() const noexcept
	{
		return m_recs;
	}

	FB_SIZE_T getUsed() const noexcept
	{
		return m_used;
	}

	void setUsed(FB_SIZE_T used) noexcept
	{
		fb_assert(used <= m_size);
		m_used = used;
	}

	void clear() noexcept
	{
		m_used = 0;
		m_recs = 0;
		m_next = NULL;
		m_linked = false;
	}

	void recordAdded() noexcept
	{
		m_recs++;
	}

	void linkNext(IOBuffer* buf) noexcept
	{
		m_next = buf;
		m_next->m_linked = true;
	}

	bool isLinked() const noexcept
	{
		return m_linked;
	}

	void lock()
	{
		m_mutex.enter(FB_FUNCTION);
		fb_assert(m_locked >= 0);
		m_locked++;
	}

	void unlock(bool opt = false)
	{
		if (opt)	// unlock only if locked by me
		{
			if (m_locked == 0)
				return;

			if (!m_mutex.tryEnter(FB_FUNCTION))
				return;

			fb_assert(m_locked >= 0);
			const bool lockedByMe = (m_locked != 0);

			m_mutex.leave();

			if (!lockedByMe)
				return;
		}

		fb_assert(m_locked > 0);
		m_locked--;
		m_mutex.leave();
	}

	IOBuffer* getNext() noexcept
	{
		return m_next;
	}

	BurpTaskItem* getItem() const noexcept
	{
		return m_item;
	}

private:
	BurpTaskItem* const m_item;
	Firebird::Array<UCHAR> m_memory;
	UCHAR* m_aligned;
	const FB_SIZE_T m_size;
	FB_SIZE_T m_used;
	FB_SIZE_T m_recs;
	IOBuffer* m_next;
	bool m_linked;
	int m_locked;
	Firebird::Mutex m_mutex;
};



class BurpMaster
{
public:
	BurpMaster()
	{
		m_tdgbl = BurpGlobals::getSpecific();
		m_task = BurpTask::getBurpTask(m_tdgbl);

		if (!m_tdgbl->master)
			m_tdgbl = m_task->getMasterGbl();

		if (m_task)
			m_task->burpOutMutex.enter(FB_FUNCTION);
	}

	~BurpMaster()
	{
		if (m_task)
			m_task->burpOutMutex.leave();
	}

	BurpGlobals* get() const noexcept
	{
		return m_tdgbl;
	}

private:
	BurpTask* m_task;
	BurpGlobals* m_tdgbl;
};

} // namespace Burp

#endif // BURP_TASKS_H
