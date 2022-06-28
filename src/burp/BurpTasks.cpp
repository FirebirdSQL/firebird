/*
 *	PROGRAM:	JRD Backup and Restore Program
 *	MODULE:		BurpTasks.cpp
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

#include <firebird.h>
#include "../burp/BurpTasks.h"
#include "../common/classes/alloc.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/SafeArg.h"
#include "../burp/burp_proto.h"
#include "../burp/mvol_proto.h"

using MsgFormat::SafeArg;
using namespace Firebird;

namespace Burp
{

// IO buffer should fit at least one blob segment, two is better.
const FB_SIZE_T MIN_IO_BUFFER_SIZE = 128 * 1024;

/// class IOBuffer

IOBuffer::IOBuffer(void* item, FB_SIZE_T size) :
	m_item(item),
	m_memory(*getDefaultMemoryPool()),
	m_aligned(NULL),
	m_size(MAX(size, MIN_IO_BUFFER_SIZE)),
	m_used(0),
	m_recs(0),
	m_next(NULL),
	m_linked(false),
	m_locked(0)

{
	fb_assert(size >= MIN_IO_BUFFER_SIZE);
	m_aligned = m_memory.getBuffer(m_size);
}


class BurpGblHolder
{
public:
	BurpGblHolder(BurpGlobals* gbl, void* item)
	{
		m_prev = BurpGlobals::getSpecific();

		// Avoid threadDataPriorContext == this, it makes a loop in linked list of contexts
		if (m_prev != gbl)
			BurpGlobals::putSpecific(gbl);

		m_prev_item = gbl->taskItem;
		gbl->taskItem = item;

		fb_assert(!m_prev_item || m_prev_item == item);
	}

	~BurpGblHolder()
	{
		BurpGlobals* gbl = BurpGlobals::getSpecific();
		gbl->taskItem = m_prev_item;

		if (m_prev != gbl)
			BurpGlobals::restoreSpecific();

		fb_assert(m_prev == BurpGlobals::getSpecific());
	}
protected:
	BurpGlobals* m_prev;
	void* m_prev_item;
};


/// class BackupRelationTask

BackupRelationTask::BackupRelationTask(BurpGlobals* tdgbl) : Task(),
	m_masterGbl(tdgbl),
	m_relation(NULL),
	m_readers(0),
	m_readDone(false),
	m_nextPP(0),
	m_stop(false),
	m_error(false)
{
	fb_utils::init_status(m_status);

	int workers = tdgbl->gbl_sw_par_workers;
	if (workers <= 0)
		workers = 1;

	MemoryPool* pool = getDefaultMemoryPool();

	// create one writer and N readers
	Item* item = FB_NEW_POOL(*pool) Item(this, true);
	item->m_gbl = m_masterGbl;
	m_items.add(item);

	item = FB_NEW_POOL(*pool) Item(this, false);
	item->m_ownAttach = false;		// will use attach from main thread
	m_items.add(item);

	for (int i = 1; i < workers; i++)
		m_items.add(FB_NEW_POOL(*pool) Item(this, false));

	// allocate IO buffers
	for (int i = 1; i <= workers; i++)
	{
		Item* item = m_items[i];

		// two IO buffers per item is enough
		for (int j = 0; j < 2; j++)
		{
			IOBuffer* buf = FB_NEW_POOL(*pool) IOBuffer(item, tdgbl->mvol_io_buffer_size);
			m_buffers.add(buf);

			item->m_cleanBuffers.add(buf);
		}
	}
}

BackupRelationTask::~BackupRelationTask()
{
	for (Item** p = m_items.begin(); p < m_items.end(); p++)
	{
		freeItem(**p);
		delete *p;
	}

	for (IOBuffer** buf = m_buffers.begin(); buf < m_buffers.end(); buf++)
		delete *buf;
}

void BackupRelationTask::SetRelation(burp_rel* relation)
{
	m_relation = relation;
	m_readers = 0;
	m_readDone = false;
	m_nextPP = 0;

	m_metadata.setRelation(m_relation, getMaxWorkers() > 2);
}

bool BackupRelationTask::handler(WorkItem& _item)
{
	Item* item = reinterpret_cast<Item*>(&_item);

	try
	{
		Item::EnsureUnlockBuffer unlock(item);

		if (item->m_writer)
		{
			BurpGblHolder holder(m_masterGbl, item);
			return fileWriter(*item);
		}

		BurpGlobals gbl(m_masterGbl->uSvc);
		gbl.master = false;

		BurpGblHolder holder(&gbl, item);

		initItem(&gbl, *item);
		return tableReader(*item);
	}
	catch (const LongJump&)
	{
		m_stop = true;
		m_error = true;
		stopItems();
	}
	catch (const Exception&)	// could be different handlers for LongJump and Exception
	{
		m_stop = true;
		m_error = true;
		stopItems();
	}
	return false;
}

bool BackupRelationTask::getWorkItem(BackupRelationTask::WorkItem** pItem)
{
	Item* item = reinterpret_cast<Item*> (*pItem);

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	// if writer ask for new item - its done
	if (item && item->m_writer)
	{
		item->m_inuse = false;
		(*pItem) = NULL;
		return false;
	}

	const bool newReader = (item == NULL);

	if (item == NULL && !m_readDone)
	{
		for (Item** p = m_items.begin(); p < m_items.end(); p++)
		{
			if (!(*p)->m_inuse)
			{
				(*p)->m_inuse = true;
				*pItem = item = *p;
				break;
			}
		}
	}

	if (!item)
		return false;

	if (item->m_writer)
		return true;

	item->m_inuse = (m_nextPP <= m_relation->rel_max_pp);

	if (item->m_inuse)
	{
		item->m_ppSequence = m_nextPP;
		if (m_metadata.haveInputs())
			m_nextPP += 1;
		else
			m_nextPP = m_relation->rel_max_pp + 1;

		if (newReader)
			m_readers++;
	}
	else if (!newReader && --m_readers == 0)
	{
		m_readDone = true;
		m_dirtyCond.notifyAll();
	}

	return (item && item->m_inuse);
}

bool BackupRelationTask::getResult(IStatus* /*status*/)
{
	fb_assert(!m_error || m_dirtyBuffers.isEmpty());

	return !m_error;
}

int BackupRelationTask::getMaxWorkers()
{
	unsigned int readers = m_items.getCount() - 1;

	if (readers > (m_relation->rel_max_pp + 1))
		readers = (m_relation->rel_max_pp + 1);

	return 1 + readers;
}

IOBuffer* BackupRelationTask::getCleanBuffer(Item& item)
{
	IOBuffer* buf = NULL;
	{
		MutexLockGuard guard(item.m_mutex, FB_FUNCTION);

		while (!m_stop && !item.m_cleanBuffers.hasData())
			item.m_cleanCond.wait(item.m_mutex);

		if (m_stop)
			return NULL;

		fb_assert(item.m_cleanBuffers.hasData());

		if (item.m_cleanBuffers.hasData())
			buf = item.m_cleanBuffers.pop();
	}

	fb_assert(buf);
	if (buf)
		buf->lock();

	return buf;
}

void BackupRelationTask::putDirtyBuffer(IOBuffer* buf)
{
	if (buf->isLinked())
	{
		buf->unlock();
		return;
	}

	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		if (m_dirtyBuffers.isEmpty())
			m_dirtyCond.notifyOne();

		m_dirtyBuffers.push(buf);
	}
	buf->unlock();
}

IOBuffer* BackupRelationTask::renewBuffer(BurpGlobals* tdgbl)
{
	fb_assert(!tdgbl->master);

	Item* item = reinterpret_cast<Item*> (tdgbl->taskItem);
	fb_assert(item);
	if (!item)
		return NULL;

	BackupRelationTask* task = item->getBackupTask();

	IOBuffer* oldBuf = item->m_buffer;
	IOBuffer* newBuf = task->getCleanBuffer(*item);

	if (!newBuf)
	{
		if (oldBuf && task->m_stop)
			oldBuf->unlock();

		throw LongJump();
	}

	FB_SIZE_T used = 0;
	UCHAR* const p = newBuf->getBuffer();

	if (oldBuf)
	{
		fb_assert(tdgbl->mvol_io_buffer == oldBuf->getBuffer());
		fb_assert(tdgbl->gbl_io_ptr == tdgbl->mvol_io_buffer + oldBuf->getSize());
		fb_assert(tdgbl->gbl_io_cnt <= 0);

		if (oldBuf->getRecs() > 0)
		{
			used = tdgbl->mvol_io_data - tdgbl->mvol_io_buffer;
			oldBuf->setUsed(used);

			used = newBuf->getSize() - used;
			memcpy(p, tdgbl->mvol_io_data, used);
		}
		else 
		{
			oldBuf->setUsed(oldBuf->getSize());
			oldBuf->linkNext(newBuf);
		}

		task->putDirtyBuffer(oldBuf);
	}

	item->m_buffer = newBuf;
	tdgbl->mvol_io_buffer = p;					// buffer start
	tdgbl->mvol_io_data = p;					// current record start
	tdgbl->gbl_io_ptr = p + used;					// current write pointer
	tdgbl->gbl_io_cnt = newBuf->getSize() - used;	// space left at buffer

	return newBuf;
}

void BackupRelationTask::releaseBuffer(Item& item)
{
	BurpGlobals* tdgbl = item.m_gbl;
	IOBuffer* oldBuf = item.m_buffer;

	fb_assert(tdgbl->mvol_io_buffer == oldBuf->getBuffer());
	fb_assert(tdgbl->gbl_io_ptr == tdgbl->mvol_io_data);

	item.m_buffer = NULL;
	if (oldBuf->getRecs() > 0)
	{
		const FB_SIZE_T used = tdgbl->mvol_io_data - tdgbl->mvol_io_buffer;
		oldBuf->setUsed(used);
		putDirtyBuffer(oldBuf);
	}
	else 
	{
		oldBuf->clear();
		putCleanBuffer(oldBuf);
	}
}

void BackupRelationTask::recordAdded(BurpGlobals* tdgbl)
{
	Item* item = reinterpret_cast<Item*> (tdgbl->taskItem);
	if (!item)
		return;

	IOBuffer* buf = item->m_buffer;
	buf->recordAdded();
	tdgbl->mvol_io_data = tdgbl->gbl_io_ptr;
}

BackupRelationTask* BackupRelationTask::getBackupTask(BurpGlobals* tdgbl)
{
	Item* item = reinterpret_cast<Item*> (tdgbl->taskItem);
	if (item)
		return item->getBackupTask();

	return NULL;
}

IOBuffer* BackupRelationTask::getDirtyBuffer()
{
	IOBuffer* buf = NULL;
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		while (!m_stop && !m_readDone && !m_dirtyBuffers.hasData())
			m_dirtyCond.wait(m_mutex);

		if (m_stop)
			return NULL;

		if (m_dirtyBuffers.hasData())
		{
			const FB_SIZE_T idx = 0;
			buf = m_dirtyBuffers[idx];
			m_dirtyBuffers.remove(idx);
		}
	}

	if (buf)
		buf->lock();

	return buf;
}

void BackupRelationTask::putCleanBuffer(IOBuffer* buf)
{
	Item* item = reinterpret_cast<Item*>(buf->getItem());
	{
		MutexLockGuard guard(item->m_mutex, FB_FUNCTION);

		if (item->m_cleanBuffers.isEmpty())
			item->m_cleanCond.notifyOne();

		item->m_cleanBuffers.push(buf);
	}
	buf->unlock();
}

void BackupRelationTask::initItem(BurpGlobals* tdgbl, Item& item)
{
	item.m_gbl = tdgbl;
	item.m_relation = m_relation;

	// copy some data from master
	tdgbl->gbl_database_file_name = m_masterGbl->gbl_database_file_name;
	tdgbl->gbl_sw_verbose = m_masterGbl->gbl_sw_verbose;
	tdgbl->gbl_sw_ignore_limbo = m_masterGbl->gbl_sw_ignore_limbo;
	tdgbl->gbl_sw_meta = m_masterGbl->gbl_sw_meta;
	tdgbl->gbl_sw_compress = m_masterGbl->gbl_sw_compress;
	tdgbl->gbl_sw_version = m_masterGbl->gbl_sw_version;
	tdgbl->gbl_sw_transportable = m_masterGbl->gbl_sw_transportable;
	tdgbl->gbl_sw_blk_factor = m_masterGbl->gbl_sw_blk_factor;
	tdgbl->gbl_sw_sql_role = m_masterGbl->gbl_sw_sql_role;
	tdgbl->gbl_sw_user = m_masterGbl->gbl_sw_user;
	tdgbl->gbl_sw_password = m_masterGbl->gbl_sw_password;
	//??? tdgbl->gbl_sw_tr_user = m_masterGbl->gbl_sw_tr_user;
	tdgbl->action = m_masterGbl->action;
	tdgbl->sw_redirect = m_masterGbl->sw_redirect;
	tdgbl->gbl_stat_flags = m_masterGbl->gbl_stat_flags;

	if (item.m_ownAttach)
	{
		if (!item.m_att)
		{
			FbLocalStatus status;
			DispatcherPtr provider;

			// attach, start tran, etc

			const unsigned char* dpbBuffer = m_masterGbl->gbl_dpb_data.begin();
			const unsigned int dpbLength = m_masterGbl->gbl_dpb_data.getCount();

			item.m_att = provider->attachDatabase(&status, tdgbl->gbl_database_file_name,
							dpbLength, dpbBuffer);
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				BURP_print_status(true, &status);
				BURP_abort();
			}

			ClumpletWriter tpb(ClumpletReader::Tpb, 128, isc_tpb_version3);
			tpb.insertTag(isc_tpb_concurrency);
			tpb.insertTag(isc_tpb_read);
			if (tdgbl->gbl_sw_ignore_limbo)
				tpb.insertTag(isc_tpb_ignore_limbo);
			tpb.insertTag(isc_tpb_no_auto_undo);
			// add snapshot id
			tpb.insertBigInt(isc_tpb_at_snapshot_number, m_masterGbl->tr_snapshot);

			item.m_tra = item.m_att->startTransaction(&status, 
							tpb.getBufferLength(), tpb.getBuffer());

			if (status->getState() & IStatus::STATE_ERRORS)
			{
				BURP_print_status(true, &status);
				BURP_abort();
			}
		}

		tdgbl->db_handle = item.m_att;
		tdgbl->tr_handle = item.m_tra;
	}
	else
	{
		tdgbl->db_handle = m_masterGbl->db_handle;
		tdgbl->tr_handle = m_masterGbl->tr_handle;
	}

	fb_assert(!item.m_writer);
	fb_assert(!item.m_buffer);
	renewBuffer(tdgbl);
}

void BackupRelationTask::freeItem(Item& item)
{
	FbLocalStatus status;
	item.m_request.release(&status);

	if (item.m_ownAttach)
	{
		if (item.m_tra)
		{
			item.m_tra->rollback(&status);
			item.m_tra = nullptr;
		}

		if (item.m_att)
		{
			item.m_att->detach(&status);
			item.m_att = nullptr;
		}
	}
}

void BackupRelationTask::stopItems()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	for (Item** p = m_items.begin(); p < m_items.end(); p++)
		(*p)->m_cleanCond.notifyAll();

	m_dirtyCond.notifyAll();
}

bool BackupRelationTask::fileWriter(Item& item)
{
	BurpGlobals* tdgbl = item.m_gbl;
	fb_assert(tdgbl == m_masterGbl);

	BURP_verbose(142, m_relation->rel_name);
	// msg 142  writing data for relation %s

	IOBuffer* buf = NULL;
	FB_SIZE_T records = 0;
	FB_SIZE_T verbRecs = -1;
	FB_SIZE_T verb = 0;
	while (!m_stop)
	{
		if (!buf)
			buf = getDirtyBuffer();
		else
			buf->lock();

		if (!buf)
			break;

		const UCHAR* p = buf->getBuffer();
		FB_SIZE_T recs = buf->getRecs();
		FB_SIZE_T len = (recs > 0) ? buf->getUsed() : buf->getSize();

		// very inefficient !
		MVOL_write_block(tdgbl, p, len);

		IOBuffer* next = buf->getNext();
		buf->clear();
		putCleanBuffer(buf);
		buf = next;

		records += recs;

		if (verb != (records / tdgbl->verboseInterval))
		{
			verb = (records / tdgbl->verboseInterval);
			verbRecs = records;
			BURP_verbose(108, SafeArg() << (verb * tdgbl->verboseInterval) /*records*/);
		}
	}

	if (records != verbRecs)
		BURP_verbose(108, SafeArg() << records);

	return true;
}

BackupRelationTask::Item::EnsureUnlockBuffer::~EnsureUnlockBuffer()
{
	if (m_item && m_item->m_buffer)
		m_item->m_buffer->unlock(true);
}


/// class RestoreRelationTask 

RestoreRelationTask::RestoreRelationTask(BurpGlobals* tdgbl) : Task(),
	m_masterGbl(tdgbl),
	m_relation(NULL),
	m_lastRecord(rec_relation_data),
	m_writers(0),
	m_readDone(false),
	m_stop(false),
	m_error(false),
	m_records(0),
	m_verbRecs(0)

{
	fb_utils::init_status(m_status);

	int workers = tdgbl->gbl_sw_par_workers;
	if (workers <= 0)
		workers = 1;

	MemoryPool* pool = getDefaultMemoryPool();

	// create one reader and N writers
	Item* item = FB_NEW_POOL(*pool) Item(this, true);
	item->m_gbl = m_masterGbl;
	m_items.add(item);

	item = FB_NEW_POOL(*pool) Item(this, false);
	item->m_ownAttach = false;		// will use attach from main thread
	m_items.add(item);

	for (int i = 1; i < workers; i++)
		m_items.add(FB_NEW_POOL(*pool) Item(this, false));

	// allocate IO buffers
	for (int i = 1; i <= workers; i++)
	{
		Item* item = m_items[i];

		// two IO buffers per item is enough
		for (int j = 0; j < 2; j++)
		{
			IOBuffer* buf = FB_NEW_POOL(*pool) IOBuffer(item, tdgbl->mvol_io_buffer_size);
			m_buffers.add(buf);

			putCleanBuffer(buf);
		}
	}
}

RestoreRelationTask::~RestoreRelationTask()
{
	for (Item** p = m_items.begin(); p < m_items.end(); p++)
	{
		freeItem(**p, false);
		delete *p;
	}

	for (IOBuffer** buf = m_buffers.begin(); buf < m_buffers.end(); buf++)
		delete *buf;
}

void RestoreRelationTask::SetRelation(BurpGlobals* tdgbl, burp_rel* relation)
{
	m_relation = relation;
	m_writers = 0;
	m_readDone = false;
	m_lastRecord = rec_relation_data;

	m_records = 0;
	m_verbRecs = 0;

	m_metadata.setRelation(tdgbl, m_relation);
}

bool RestoreRelationTask::handler(WorkItem& _item)
{
	Item* item = reinterpret_cast<Item*>(&_item);

	try
	{
		Item::EnsureUnlockBuffer unlock(item);

		if (item->m_reader)
		{
			BurpGblHolder holder(m_masterGbl, item);
			return fileReader(*item);
		}

		BurpGlobals gbl(m_masterGbl->uSvc);
		gbl.master = false;

		BurpGblHolder holder(&gbl, item);

		initItem(&gbl, *item);
		return tableWriter(&gbl, *item);
	}
	catch (const LongJump&)
	{
		m_stop = true;
		m_error = true;
		m_dirtyCond.notifyAll();
		m_cleanCond.notifyAll();
	}
	catch (const Exception&)	// could be different handlers for LongJump and Exception
	{
		m_stop = true;
		m_error = true;
		m_dirtyCond.notifyAll();
		m_cleanCond.notifyAll();
	}
	return false;
}

bool RestoreRelationTask::getWorkItem(WorkItem** pItem)
{
	Item* item = reinterpret_cast<Item*> (*pItem);

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	// if reader ask for new item - its done
	if (item && item->m_reader)
	{
		item->m_inuse = false;
		(*pItem) = NULL;
		return false;
	}

	const bool newWriter = (item == NULL);
	const bool haveWork = (!m_readDone || m_dirtyBuffers.hasData()) && !m_stop;

	if (item == NULL && haveWork)
	{
		for (Item** p = m_items.begin(); p < m_items.end(); p++)
		{
			if (!(*p)->m_inuse && (!(*p)->m_reader || !m_readDone))
			{
				(*p)->m_inuse = true;
				*pItem = item = *p;
				break;
			}
		}
	}

	if (!item)
		return false;

	if (item->m_reader)
		return true;

	item->m_inuse = haveWork;

	if (item->m_inuse)
	{
		if (newWriter)
			m_writers++;
	}
	else if (!newWriter && --m_writers == 0)
	{
		//m_dirtyCond.notifyAll();
	}

	return (item && item->m_inuse);
}

bool RestoreRelationTask::getResult(IStatus* status)
{
	fb_assert(!m_error || m_dirtyBuffers.isEmpty());

	return !m_error;
}

int RestoreRelationTask::getMaxWorkers()
{
	return m_items.getCount();
}

RestoreRelationTask* RestoreRelationTask::getRestoreTask(BurpGlobals* tdgbl)
{
	Item* item = reinterpret_cast<Item*> (tdgbl->taskItem);
	if (item)
		return item->getRestoreTask();

	return NULL;
}

void RestoreRelationTask::verbRecs(FB_UINT64& records, bool total)
{
	if (!total)
		++records;

	const FB_UINT64 verb = m_masterGbl->verboseInterval / getMaxWorkers();
	if (records < verb && !total)
		return;

	FB_UINT64 newRecs = m_records.exchangeAdd(records) + records;
	records = 0;

	FB_UINT64 newVerb = (newRecs / m_masterGbl->verboseInterval) * m_masterGbl->verboseInterval;
	if (newVerb > m_verbRecs)
	{
		m_verbRecs = newVerb;
		BURP_verbose(107, SafeArg() << m_verbRecs);
		// msg 107 %ld records restored
	}
}

void RestoreRelationTask::verbRecsFinal()
{
	if (m_verbRecs < m_records)
	{
		m_verbRecs = m_records;
		BURP_verbose(107, SafeArg() << m_verbRecs);
		// msg 107 %ld records restored
	}
}

bool RestoreRelationTask::finish()
{
	bool ret = true;
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	for (Item** p = m_items.begin(); p < m_items.end(); p++)
	{
		if (!freeItem(**p, true))
			ret = false;
	}

	return ret;
}

void RestoreRelationTask::initItem(BurpGlobals* tdgbl, Item& item)
{
	item.m_gbl = tdgbl;
	item.m_relation = m_relation;

	// copy some data from master
	tdgbl->gbl_database_file_name = m_masterGbl->gbl_database_file_name;
	tdgbl->gbl_sw_verbose = m_masterGbl->gbl_sw_verbose;
	tdgbl->gbl_sw_meta = m_masterGbl->gbl_sw_meta;
	tdgbl->gbl_sw_compress = m_masterGbl->gbl_sw_compress;
	tdgbl->gbl_sw_version = m_masterGbl->gbl_sw_version;
	tdgbl->gbl_sw_transportable = m_masterGbl->gbl_sw_transportable;
	tdgbl->gbl_sw_incremental = m_masterGbl->gbl_sw_incremental;
	tdgbl->gbl_sw_blk_factor = m_masterGbl->gbl_sw_blk_factor;
	tdgbl->gbl_dialect = m_masterGbl->gbl_dialect;
	tdgbl->gbl_sw_sql_role = m_masterGbl->gbl_sw_sql_role;
	tdgbl->gbl_sw_user = m_masterGbl->gbl_sw_user;
	tdgbl->gbl_sw_password = m_masterGbl->gbl_sw_password;
	//??? tdgbl->gbl_sw_tr_user = m_masterGbl->gbl_sw_tr_user;
	tdgbl->action = m_masterGbl->action;
	tdgbl->sw_redirect = m_masterGbl->sw_redirect;
	tdgbl->gbl_stat_flags = m_masterGbl->gbl_stat_flags;
	tdgbl->verboseInterval = m_masterGbl->verboseInterval;

	if (item.m_ownAttach)
	{
		if (!item.m_att)
		{
			// attach, start tran, etc
			FbLocalStatus status;
			DispatcherPtr provider;

			ClumpletWriter dpb(ClumpletReader::dpbList, 128, 
				m_masterGbl->gbl_dpb_data.begin(), 
				m_masterGbl->gbl_dpb_data.getCount());

			dpb.deleteWithTag(isc_dpb_gbak_attach);

			const UCHAR* dpbBuffer = dpb.getBuffer();
			const USHORT dpbLength = dpb.getBufferLength();

			item.m_att = provider->attachDatabase(&status, tdgbl->gbl_database_file_name,
				dpbLength, dpbBuffer);

			if (status->getState() & IStatus::STATE_ERRORS)
			{
				BURP_print_status(true, &status);
				BURP_abort();
			}

			// SET TRANSACTION NO_AUTO_UNDO, see at the end of get_data()

			ClumpletWriter tpb(ClumpletReader::Tpb, 128, isc_tpb_version3);
			tpb.insertTag(isc_tpb_concurrency);
			tpb.insertTag(isc_tpb_no_auto_undo);

			item.m_tra = item.m_att->startTransaction(&status, tpb.getBufferLength(), tpb.getBuffer());

			if (status->getState() & IStatus::STATE_ERRORS)
			{
				BURP_print_status(true, &status);
				BURP_abort();
			}
		}

		tdgbl->db_handle = item.m_att;
		tdgbl->tr_handle = item.m_tra;
	}
	else
	{
		tdgbl->db_handle = m_masterGbl->db_handle;
		tdgbl->tr_handle = m_masterGbl->tr_handle;
	}

	fb_assert(!item.m_reader);
	fb_assert(!item.m_buffer);
}

bool RestoreRelationTask::freeItem(Item& item, bool commit)
{
	FbLocalStatus status;
	item.m_request.release();

	bool ret = true;
	if (item.m_ownAttach)
	{
		if (item.m_tra && commit)
		{
			item.m_tra->commit(&status);
			if (status->getState() & IStatus::STATE_ERRORS)
			{
				ret = false;

				// more detailed message required ?
				BURP_print_status(false, &status);
			}
			item.m_tra = nullptr;
		}

		if (item.m_tra)
		{
			item.m_tra->rollback(&status);
			item.m_tra = nullptr;
		}

		if (item.m_att)
		{
			item.m_att->detach(&status);
			item.m_att = nullptr;
		}
	}

	return ret;
}

IOBuffer* RestoreRelationTask::getCleanBuffer()
{
	IOBuffer* buf = NULL;
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);
		while (!m_stop && !m_cleanBuffers.hasData())
			m_cleanCond.wait(m_mutex);

		if (m_stop)
			return NULL;

		fb_assert(m_cleanBuffers.hasData() /*|| m_readDone*/);

		if (m_cleanBuffers.hasData())
		{
			const FB_SIZE_T idx = 0;
			buf = m_cleanBuffers[idx];
			m_cleanBuffers.remove(idx);
		}
	}

	if (buf)
		buf->lock();

	return buf;
}

void RestoreRelationTask::putDirtyBuffer(IOBuffer* buf)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	if (m_dirtyBuffers.isEmpty())
		m_dirtyCond.notifyOne();
	buf->unlock();
	m_dirtyBuffers.push(buf);
}

IOBuffer* RestoreRelationTask::renewBuffer(BurpGlobals* tdgbl)
{
	// table writer needs new dirty buffer with records 

	fb_assert(!tdgbl->master);

	Item* item = reinterpret_cast<Item*> (tdgbl->taskItem);
	fb_assert(item);
	if (!item)
		ExcReadDone::raise();

	RestoreRelationTask* task = item->getRestoreTask();

	IOBuffer* oldBuf = item->m_buffer;
	IOBuffer* newBuf = NULL;
	if (oldBuf)
	{
		newBuf = oldBuf->getNext();

		item->m_buffer = NULL;
		oldBuf->clear();
		oldBuf->unlock();
		task->putCleanBuffer(oldBuf);
	}

	if (newBuf)
		newBuf->lock();
	else 
		newBuf = task->getDirtyBuffer();

	if (!newBuf)
		ExcReadDone::raise();

	UCHAR* const p = newBuf->getBuffer();
	item->m_buffer = newBuf;
	tdgbl->mvol_io_buffer = p;					// buffer start
	tdgbl->mvol_io_data = p;
	tdgbl->mvol_io_cnt = newBuf->getUsed();
	tdgbl->gbl_io_ptr = p;							// current read pointer
	tdgbl->gbl_io_cnt = tdgbl->mvol_io_cnt;

	return newBuf;
}

void RestoreRelationTask::releaseBuffer(Item& item)
{
	BurpGlobals* tdgbl = item.m_gbl;
	IOBuffer* oldBuf = item.m_buffer;

	if (!oldBuf)
		return;

	fb_assert(tdgbl->mvol_io_buffer == oldBuf->getBuffer());
	//fb_assert(tdgbl->blk_io_ptr == tdgbl->mvol_io_data);

	item.m_buffer = NULL;

	oldBuf->clear();
	oldBuf->unlock();
	putCleanBuffer(oldBuf);
}

IOBuffer* RestoreRelationTask::getDirtyBuffer()
{
	IOBuffer* buf = NULL;
	{
		MutexLockGuard guard(m_mutex, FB_FUNCTION);

		while (!m_dirtyBuffers.hasData() && !m_readDone && !m_stop)
			m_dirtyCond.wait(m_mutex);

		if (m_stop)
			return NULL;

		if (!m_dirtyBuffers.hasData())
		{
			fb_assert(m_readDone);
			return NULL;
		}

		const FB_SIZE_T idx = 0;
		buf = m_dirtyBuffers[idx];
		m_dirtyBuffers.remove(idx);
	}

	buf->lock();
	return buf;
}

void RestoreRelationTask::putCleanBuffer(IOBuffer* buf)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	if (m_cleanBuffers.isEmpty())
		m_cleanCond.notifyOne();
	m_cleanBuffers.push(buf);
}

RestoreRelationTask::Item::EnsureUnlockBuffer::~EnsureUnlockBuffer()
{
	if (m_item && m_item->m_buffer)
		m_item->m_buffer->unlock(true);
}

/// class RestoreRelationTask::ExcReadDone

void RestoreRelationTask::ExcReadDone::stuffByException(StaticStatusVector& status) const throw()
{
	ISC_STATUS sv[] = {isc_arg_gds, isc_random, isc_arg_string,
		(ISC_STATUS)(IPTR) "Unexpected call to RestoreRelationTask::ExcReadDone::stuffException()", isc_arg_end};

	try
	{
		status.assign(sv, FB_NELEM(sv));
	}
	catch (const BadAlloc&)
	{
		processUnexpectedException(status.makeEmergencyStatus());
	}
}

const char* RestoreRelationTask::ExcReadDone::what() const throw()
{
	return "RestoreRelationTask::ExcReadDone";
}

void RestoreRelationTask::ExcReadDone::raise()
{
	throw ExcReadDone();
}

} // namespace Firebird
