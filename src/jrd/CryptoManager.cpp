/*
 *	PROGRAM:		JRD access method
 *	MODULE:			CryptoManager.cpp
 *	DESCRIPTION:	Database encryption
 *
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2012 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "firebird/Interface.h"
#include "gen/iberror.h"
#include "../jrd/CryptoManager.h"

#include "../common/classes/alloc.h"
#include "../jrd/Database.h"
#include "../common/ThreadStart.h"
#include "../common/StatusArg.h"
#include "../common/StatusHolder.h"
#include "../jrd/lck.h"
#include "../jrd/jrd.h"
#include "../jrd/pag.h"
#include "../jrd/nbak.h"
#include "../jrd/cch_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/inf_pub.h"
#include "../jrd/Monitoring.h"
#include "../jrd/os/pio_proto.h"
#include "../common/isc_proto.h"
#include "../common/classes/GetPlugins.h"
#include "../common/classes/RefMutex.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/sha.h"

using namespace Firebird;

namespace {
	THREAD_ENTRY_DECLARE cryptThreadStatic(THREAD_ENTRY_PARAM p)
	{
		Jrd::CryptoManager* cryptoManager = (Jrd::CryptoManager*) p;
		cryptoManager->cryptThread();

		return 0;
	}

	const UCHAR CRYPT_RELEASE = LCK_SR;
	const UCHAR CRYPT_NORMAL = LCK_PR;
	const UCHAR CRYPT_CHANGE = LCK_PW;
	const UCHAR CRYPT_INIT = LCK_EX;

	const int MAX_PLUGIN_NAME_LEN = 31;
}


namespace Jrd {

	class Header
	{
	protected:
		Header()
			: header(NULL)
		{ }

		void setHeader(void* buf)
		{
			header = static_cast<Ods::header_page*>(buf);
		}

		void setHeader(Ods::header_page* newHdr)
		{
			header = newHdr;
		}

		Ods::header_page* getHeader()
		{
			return header;
		}

	public:
		const Ods::header_page* operator->() const
		{
			return header;
		}

		operator const Ods::header_page*() const
		{
			return header;
		}

		// This routine is getting clumplets from header page but is not ready to handle continuation
		// Fortunately, modern pages of size 4k and bigger can fit everything on one page.
		void getClumplets(ClumpletWriter& writer) const
		{
			writer.reset(header->hdr_data, header->hdr_end - HDR_SIZE);
		}

	private:
		Ods::header_page* header;
	};


	class CchHdr : public Header
	{
	public:
		CchHdr(Jrd::thread_db* p_tdbb, USHORT lockType)
			: window(Jrd::HEADER_PAGE_NUMBER),
			  tdbb(p_tdbb),
			  wrk(NULL),
			  buffer(*tdbb->getDefaultPool())
		{
			void* h = CCH_FETCH(tdbb, &window, lockType, pag_header);
			if (!h)
			{
				ERR_punt();
			}
			setHeader(h);
		}

		Ods::header_page* write()
		{
			if (!wrk)
			{
				Ods::header_page* hdr = getHeader();
				wrk = reinterpret_cast<Ods::header_page*>(buffer.getBuffer(hdr->hdr_page_size));
				memcpy(wrk, hdr, hdr->hdr_page_size);

				// swap headers
				setHeader(wrk);
				wrk = hdr;
			}
			return getHeader();
		}

		void flush()
		{
			if (wrk)
			{
				CCH_MARK_MUST_WRITE(tdbb, &window);
				memcpy(wrk, getHeader(), wrk->hdr_page_size);
			}
		}

		void setClumplets(const ClumpletWriter& writer)
		{
			Ods::header_page* hdr = write();
			UCHAR* const to = hdr->hdr_data;
			UCHAR* const end = reinterpret_cast<UCHAR*>(hdr) + hdr->hdr_page_size;
			const unsigned limit = (end - to) - 1;

			const unsigned length = writer.getBufferLength();
			fb_assert(length <= limit);
			if (length > limit)
				(Arg::Gds(isc_random) << "HDR page clumplets overflow").raise();

			memcpy(to, writer.getBuffer(), length);
			to[length] = Ods::HDR_end;
			hdr->hdr_end = HDR_SIZE + length;
		}

		~CchHdr()
		{
			CCH_RELEASE(tdbb, &window);
		}

	private:
		Jrd::WIN window;
		Jrd::thread_db* tdbb;
		Ods::header_page* wrk;
		Array<UCHAR> buffer;
	};

	class PhysHdr : public Header
	{
	public:
		explicit PhysHdr(Jrd::thread_db* tdbb)
		{
			// Can't use CCH_fetch_page() here cause it will cause infinite recursion

			Jrd::Database* dbb = tdbb->getDatabase();
			Jrd::BufferControl* bcb = dbb->dbb_bcb;
			Jrd::BufferDesc bdb(bcb);
			bdb.bdb_page = Jrd::HEADER_PAGE_NUMBER;

			UCHAR* h = FB_NEW_POOL(*Firebird::MemoryPool::getContextPool()) UCHAR[dbb->dbb_page_size + PAGE_ALIGNMENT];
			buffer.reset(h);
			h = FB_ALIGN(h, PAGE_ALIGNMENT);
			bdb.bdb_buffer = (Ods::pag*) h;

			Jrd::FbStatusVector* const status = tdbb->tdbb_status_vector;

			Ods::pag* page = bdb.bdb_buffer;

			Jrd::PageSpace* pageSpace = dbb->dbb_page_manager.findPageSpace(Jrd::DB_PAGE_SPACE);
			fb_assert(pageSpace);

			Jrd::jrd_file* file = pageSpace->file;
			const bool isTempPage = pageSpace->isTemporary();

			Jrd::BackupManager::StateReadGuard stateGuard(tdbb);
			Jrd::BackupManager* bm = dbb->dbb_backup_manager;
			int bak_state = bm->getState();

			fb_assert(bak_state != Ods::hdr_nbak_unknown);

			ULONG diff_page = 0;
			if (bak_state != Ods::hdr_nbak_normal)
				diff_page = bm->getPageIndex(tdbb, bdb.bdb_page.getPageNum());

			if (bak_state == Ods::hdr_nbak_normal || !diff_page)
			{
				// Read page from disk as normal
				int retryCount = 0;

				while (!PIO_read(tdbb, file, &bdb, page, status))
		 		{
					if (!CCH_rollover_to_shadow(tdbb, dbb, file, false))
 						ERR_punt();;

					if (file != pageSpace->file)
						file = pageSpace->file;
					else
					{
						if (retryCount++ == 3)
						{
							gds__log("IO error loop Unwind to avoid a hang\n");
							ERR_punt();
						}
					}
				}
			}
			else
			{
				if (!bm->readDifference(tdbb, diff_page, page))
					ERR_punt();
			}

			setHeader(h);
		}

	private:
		AutoPtr<UCHAR, ArrayDelete<UCHAR> > buffer;
	};

	CryptoManager::CryptoManager(thread_db* tdbb)
		: PermanentStorage(*tdbb->getDatabase()->dbb_permanent),
		  sync(this),
		  keyName(getPool()),
		  keyHolderPlugins(getPool()),
		  cryptThreadId(0),
		  cryptPlugin(NULL),
		  dbb(*tdbb->getDatabase()),
		  cryptAtt(NULL),
		  slowIO(0),
		  crypt(false),
		  process(false),
		  down(false),
		  run(false)
	{
		stateLock = FB_NEW_RPT(getPool(), 0)
			Lock(tdbb, 0, LCK_crypt_status, this, blockingAstChangeCryptState);
		threadLock = FB_NEW_RPT(getPool(), 0) Lock(tdbb, 0, LCK_crypt);
	}

	CryptoManager::~CryptoManager()
	{
		if (cryptThreadId)
			Thread::waitForCompletion(cryptThreadId);

		delete stateLock;
		delete threadLock;
	}

	void CryptoManager::shutdown(thread_db* tdbb)
	{
		terminateCryptThread(tdbb);

		if (cryptPlugin)
		{
			PluginManagerInterfacePtr()->releasePlugin(cryptPlugin);
			cryptPlugin = NULL;
		}

		LCK_release(tdbb, stateLock);
	}

	void CryptoManager::doOnTakenWriteSync(thread_db* tdbb)
	{
		fb_assert(stateLock);
		if (stateLock->lck_physical > CRYPT_RELEASE)
			return;

		fb_assert(tdbb);
		lockAndReadHeader(tdbb, CRYPT_HDR_NOWAIT);
	}

	void CryptoManager::lockAndReadHeader(thread_db* tdbb, unsigned flags)
	{
		if (flags & CRYPT_HDR_INIT)
		{
			if (LCK_lock(tdbb, stateLock, CRYPT_INIT, LCK_NO_WAIT))
			{
				LCK_write_data(tdbb, stateLock, 1);
				if (!LCK_convert(tdbb, stateLock, CRYPT_NORMAL, LCK_NO_WAIT))
				{
					fb_assert(tdbb->tdbb_status_vector->getState() & IStatus::STATE_ERRORS);
					ERR_punt();
				}
			}
			else if (!LCK_lock(tdbb, stateLock, CRYPT_NORMAL, LCK_WAIT))
			{
				fb_assert(false);
			}
		}
		else
		{
			if (!LCK_convert(tdbb, stateLock, CRYPT_NORMAL,
					(flags & CRYPT_HDR_NOWAIT) ? LCK_NO_WAIT : LCK_WAIT))
			{
				// Failed to take state lock - switch to slow IO mode
				slowIO = LCK_read_data(tdbb, stateLock);
				fb_assert(slowIO);
			}
			else
				slowIO = 0;
		}
		tdbb->tdbb_status_vector->init();

		PhysHdr hdr(tdbb);
		crypt = hdr->hdr_flags & Ods::hdr_encrypted;
		process = hdr->hdr_flags & Ods::hdr_crypt_process;

		if ((crypt || process) && !cryptPlugin)
		{
			ClumpletWriter hc(ClumpletWriter::UnTagged, hdr->hdr_page_size);
			hdr.getClumplets(hc);
			if (hc.find(Ods::HDR_crypt_key))
				hc.getString(keyName);
			else
				keyName = "";

			loadPlugin(hdr->hdr_crypt_plugin);

			string valid;
			calcValidation(valid);
			if (hc.find(Ods::HDR_crypt_hash))
			{
				string hash;
				hc.getString(hash);
				if (hash != valid)
					(Arg::Gds(isc_bad_crypt_key) << keyName).raise();
			}
		}

		if (flags & CRYPT_HDR_INIT)
			checkDigitalSignature(hdr);
	}

	void CryptoManager::loadPlugin(const char* pluginName)
	{
		if (cryptPlugin)
		{
			return;
		}

		MutexLockGuard guard(pluginLoadMtx, FB_FUNCTION);
		if (cryptPlugin)
		{
			return;
		}

		GetPlugins<IDbCryptPlugin> cryptControl(IPluginManager::TYPE_DB_CRYPT, dbb.dbb_config, pluginName);
		if (!cryptControl.hasData())
		{
			(Arg::Gds(isc_no_crypt_plugin) << pluginName).raise();
		}

		// do not assign cryptPlugin directly before key init complete
		IDbCryptPlugin* p = cryptControl.plugin();
		keyHolderPlugins.init(p, keyName.c_str());
		cryptPlugin = p;
		cryptPlugin->addRef();
	}

	void CryptoManager::prepareChangeCryptState(thread_db* tdbb, const MetaName& plugName,
		const MetaName& key)
	{
		if (plugName.length() > MAX_PLUGIN_NAME_LEN)
		{
			(Arg::Gds(isc_cp_name_too_long) << Arg::Num(MAX_PLUGIN_NAME_LEN)).raise();
		}

		const bool newCryptState = plugName.hasData();

		int bak_state = Ods::hdr_nbak_unknown;
		{	// scope
			BackupManager::StateReadGuard stateGuard(tdbb);
			bak_state = dbb.dbb_backup_manager->getState();
		}

		{	// window scope
			CchHdr hdr(tdbb, LCK_read);

			// Check header page for flags
			if (hdr->hdr_flags & Ods::hdr_crypt_process)
			{
				(Arg::Gds(isc_cp_process_active)).raise();
			}

			bool headerCryptState = hdr->hdr_flags & Ods::hdr_encrypted;
			if (headerCryptState == newCryptState)
			{
				(Arg::Gds(isc_cp_already_crypted)).raise();
			}

			if (bak_state != Ods::hdr_nbak_normal)
			{
				(Arg::Gds(isc_wish_list) << Arg::Gds(isc_random) <<
					"Cannot crypt: please wait for nbackup completion").raise();
			}

			// Load plugin
			if (newCryptState)
			{
				if (cryptPlugin)
				{
					if (!headerCryptState)
					{
						// unload old plugin
						PluginManagerInterfacePtr()->releasePlugin(cryptPlugin);
						cryptPlugin = NULL;
					}
					else
						Arg::Gds(isc_cp_already_crypted).raise();
				}

				keyName = key;
				loadPlugin(plugName.c_str());
			}
		}
	}

	void CryptoManager::calcValidation(string& valid)
	{
		// crypt verifier
		const char* sample = "0123456789ABCDEF";
		char result[16];
		FbLocalStatus sv;
		cryptPlugin->encrypt(&sv, sizeof(result), sample, result);
		if (sv->getState() & IStatus::STATE_ERRORS)
			Arg::StatusVector(&sv).raise();

		// calculate its hash
		const string verifier(result, sizeof(result));
		Sha1::hashBased64(valid, verifier);
	}

	void CryptoManager::changeCryptState(thread_db* tdbb, const string& plugName)
	{
		if (plugName.length() > 31)
		{
			(Arg::Gds(isc_cp_name_too_long) << Arg::Num(31)).raise();
		}

		const bool newCryptState = plugName.hasData();

		try
		{
			BarSync::LockGuard writeGuard(tdbb, sync);

			// header scope
			CchHdr hdr(tdbb, LCK_write);
			writeGuard.lock();

			// Nbak's lock was taken in prepareChangeCryptState()
			// If it was invalidated it's enough reason not to continue now
			int bak_state = dbb.dbb_backup_manager->getState();
			if (bak_state != Ods::hdr_nbak_normal)
			{
				(Arg::Gds(isc_wish_list) << Arg::Gds(isc_random) <<
					"Cannot crypt: please wait for nbackup completion").raise();
			}

			// Check header page for flags
			if (hdr->hdr_flags & Ods::hdr_crypt_process)
			{
				(Arg::Gds(isc_cp_process_active)).raise();
			}

			bool headerCryptState = hdr->hdr_flags & Ods::hdr_encrypted;
			if (headerCryptState == newCryptState)
			{
				(Arg::Gds(isc_cp_already_crypted)).raise();
			}

			fb_assert(stateLock);
			// Trigger lock on ChangeCryptState
			if (!LCK_convert(tdbb, stateLock, CRYPT_CHANGE, LCK_WAIT))
			{
				fb_assert(tdbb->tdbb_status_vector->getState() & IStatus::STATE_ERRORS);
				ERR_punt();
			}
			fb_utils::init_status(tdbb->tdbb_status_vector);

			// Load plugin
			if (newCryptState)
			{
				loadPlugin(plugName.c_str());
			}
			crypt = newCryptState;

			// Write modified header page
			Ods::header_page* header = hdr.write();
			ClumpletWriter hc(ClumpletWriter::UnTagged, header->hdr_page_size);
			hdr.getClumplets(hc);

			if (crypt)
			{
				header->hdr_flags |= Ods::hdr_encrypted;
				plugName.copyTo(header->hdr_crypt_plugin, sizeof(header->hdr_crypt_plugin));
				string hash;
				calcValidation(hash);
				hc.deleteWithTag(Ods::HDR_crypt_hash);
				hc.insertString(Ods::HDR_crypt_hash, hash);

				hc.deleteWithTag(Ods::HDR_crypt_key);
				if (keyName.hasData())
					hc.insertString(Ods::HDR_crypt_key, keyName);
			}
			else
				header->hdr_flags &= ~Ods::hdr_encrypted;

			hdr.setClumplets(hc);

			// Setup hdr_crypt_page for crypt thread
			header->hdr_crypt_page = 1;
			header->hdr_flags |= Ods::hdr_crypt_process;
			process = true;

			digitalySignDatabase(hdr);
			hdr.flush();
		}
		catch (const Exception&)
		{
			if (stateLock->lck_physical != CRYPT_NORMAL)
			{
				try
				{
					if (!LCK_convert(tdbb, stateLock, CRYPT_RELEASE, LCK_NO_WAIT))
						fb_assert(false);
					lockAndReadHeader(tdbb);
				}
				catch (const Exception&)
				{ }
			}
			throw;
		}

		SINT64 next = LCK_read_data(tdbb, stateLock) + 1;
		LCK_write_data(tdbb, stateLock, next);

		if (!LCK_convert(tdbb, stateLock, CRYPT_RELEASE, LCK_NO_WAIT))
			fb_assert(false);
		lockAndReadHeader(tdbb);
		fb_utils::init_status(tdbb->tdbb_status_vector);

		startCryptThread(tdbb);
	}

	void CryptoManager::blockingAstChangeCryptState()
	{
		AsyncContextHolder tdbb(&dbb, FB_FUNCTION);

		if (stateLock->lck_physical != CRYPT_CHANGE && stateLock->lck_physical != CRYPT_INIT)
		{
			sync.ast(tdbb);
		}
	}

	void CryptoManager::doOnAst(thread_db* tdbb)
	{
		fb_assert(stateLock);
		LCK_convert(tdbb, stateLock, CRYPT_RELEASE, LCK_NO_WAIT);
	}

	void CryptoManager::attach(thread_db* tdbb, Attachment* att)
	{
		keyHolderPlugins.attach(att, dbb.dbb_config);

		lockAndReadHeader(tdbb, CRYPT_HDR_INIT);
	}

	void CryptoManager::terminateCryptThread(thread_db*, bool wait)
	{
		down = true;
		if (wait && cryptThreadId)
		{
			Thread::waitForCompletion(cryptThreadId);
			cryptThreadId = 0;
		}
	}

	void CryptoManager::stopThreadUsing(thread_db* tdbb, Attachment* att)
	{
		if (att == cryptAtt)
			terminateCryptThread(tdbb);
	}

	void CryptoManager::startCryptThread(thread_db* tdbb)
	{
		// Try to take crypt mutex
		// If can't take that mutex - nothing to do, cryptThread already runs in our process
		MutexEnsureUnlock guard(cryptThreadMtx, FB_FUNCTION);
		if (!guard.tryEnter())
			return;

		// Check for recursion
		if (run)
			return;

		// Take exclusive threadLock
		// If can't take that lock - nothing to do, cryptThread already runs somewhere
		if (!LCK_lock(tdbb, threadLock, LCK_EX, LCK_NO_WAIT))
		{
			// Cleanup lock manager error
			fb_utils::init_status(tdbb->tdbb_status_vector);

			return;
		}

		bool releasingLock = false;
		try
		{
			// Cleanup resources
			terminateCryptThread(tdbb);
			down = false;

			// Determine current page from the header
			CchHdr hdr(tdbb, LCK_read);
			process = hdr->hdr_flags & Ods::hdr_crypt_process ? true : false;
			if (!process)
			{
				releasingLock = true;
				LCK_release(tdbb, threadLock);
				return;
			}

			currentPage = hdr->hdr_crypt_page;

			// Refresh encryption flag
			crypt = hdr->hdr_flags & Ods::hdr_encrypted ? true : false;

			// If we are going to start crypt thread, we need plugin to be loaded
			loadPlugin(hdr->hdr_crypt_plugin);

			releasingLock = true;
			LCK_release(tdbb, threadLock);
			releasingLock = false;

			// ready to go
			guard.leave();		// release in advance to avoid races with cryptThread()
			Thread::start(cryptThreadStatic, (THREAD_ENTRY_PARAM) this, THREAD_medium, &cryptThreadId);
		}
		catch (const Firebird::Exception&)
		{
			if (!releasingLock)		// avoid secondary exception in catch
			{
				try
				{
					LCK_release(tdbb, threadLock);
				}
				catch (const Firebird::Exception&)
				{ }
			}

			throw;
		}
	}

	void CryptoManager::cryptThread()
	{
		FbLocalStatus status_vector;
		bool lckRelease = false;

		try
		{
			// Try to take crypt mutex
			// If can't take that mutex - nothing to do, cryptThread already runs in our process
			MutexEnsureUnlock guard(cryptThreadMtx, FB_FUNCTION);
			if (!guard.tryEnter())
			{
				return;
			}

			// Establish temp context
			// Needed to take crypt thread lock
			UserId user;
			user.setUserName("(Crypt thread)");

			Jrd::Attachment* const attachment = Jrd::Attachment::create(&dbb);
			RefPtr<SysStableAttachment> sAtt(FB_NEW SysStableAttachment(attachment));
			attachment->setStable(sAtt);
			attachment->att_filename = dbb.dbb_filename;
			attachment->att_user = &user;
			BackgroundContextHolder tempDbb(&dbb, attachment, &status_vector, FB_FUNCTION);

			LCK_init(tempDbb, LCK_OWNER_attachment);
			sAtt->initDone();

			// Take exclusive threadLock
			// If can't take that lock - nothing to do, cryptThread already runs somewhere
			if (!LCK_lock(tempDbb, threadLock, LCK_EX, LCK_NO_WAIT))
			{
				Monitoring::cleanupAttachment(tempDbb);
				attachment->releaseLocks(tempDbb);
				LCK_fini(tempDbb, LCK_OWNER_attachment);
				return;
			}

			try
			{
				// Set running flag
				AutoSetRestore<bool> runFlag(&run, true);

				// Establish context
				// Need real attachment in order to make classic mode happy
				ClumpletWriter writer(ClumpletReader::Tagged, MAX_DPB_SIZE, isc_dpb_version1);
				writer.insertString(isc_dpb_user_name, DBA_USER_NAME);
				writer.insertByte(isc_dpb_no_db_triggers, TRUE);

				// Avoid races with release_attachment() in jrd.cpp
				MutexEnsureUnlock releaseGuard(cryptAttMutex, FB_FUNCTION);
				releaseGuard.enter();
				if (!down)
				{
				RefPtr<JAttachment> jAtt(REF_NO_INCR, dbb.dbb_provider->attachDatabase(&status_vector,
					dbb.dbb_filename.c_str(), writer.getBufferLength(), writer.getBuffer()));
				check(&status_vector);

				MutexLockGuard attGuard(*(jAtt->getStable()->getMutex()), FB_FUNCTION);
				Attachment* att = jAtt->getHandle();
				if (!att)
					Arg::Gds(isc_att_shutdown).raise();
				att->att_flags |= ATT_crypt_thread;
				releaseGuard.leave();

				ThreadContextHolder tdbb(att->att_database, att, &status_vector);
				tdbb->tdbb_quantum = SWEEP_QUANTUM;

				DatabaseContextHolder dbHolder(tdbb);

				class UseCountHolder
				{
				public:
					explicit UseCountHolder(Attachment* a)
						: att(a)
					{
						att->att_use_count++;
					}
					~UseCountHolder()
					{
						att->att_use_count--;
					}
				private:
					Attachment* att;
				};
				UseCountHolder use_count(att);

				// get ready...
				AutoSetRestore<Attachment*> attSet(&cryptAtt, att);
				ULONG lastPage = getLastPage(tdbb);

				do
				{
					// Check is there some job to do
					while (currentPage < lastPage)
					{
						// forced terminate
						if (down)
						{
							break;
						}

						// scheduling
						if (--tdbb->tdbb_quantum < 0)
						{
							JRD_reschedule(tdbb, SWEEP_QUANTUM, true);
						}

						// nbackup state check
						int bak_state = Ods::hdr_nbak_unknown;
						{	// scope
							BackupManager::StateReadGuard stateGuard(tdbb);
							bak_state = dbb.dbb_backup_manager->getState();
						}

						if (bak_state != Ods::hdr_nbak_normal)
						{
							EngineCheckout checkout(tdbb, FB_FUNCTION);
							Thread::sleep(10);
							continue;
						}

						// writing page to disk will change it's crypt status in usual way
						WIN window(DB_PAGE_SPACE, currentPage);
						Ods::pag* page = CCH_FETCH(tdbb, &window, LCK_write, pag_undefined);
						if (page && page->pag_type <= pag_max &&
							(bool(page->pag_flags & Ods::crypted_page) != crypt) &&
							Ods::pag_crypt_page[page->pag_type])
						{
							CCH_MARK_MUST_WRITE(tdbb, &window);
						}
						CCH_RELEASE_TAIL(tdbb, &window);

						// sometimes save currentPage into DB header
						++currentPage;
						if ((currentPage & 0x3FF) == 0)
						{
							writeDbHeader(tdbb, currentPage);
						}
					}

					// forced terminate
					if (down)
					{
						break;
					}

					// At this moment of time all pages with number < lastpage
					// are guaranteed to change crypt state. Check for added pages.
					lastPage = getLastPage(tdbb);

				} while (currentPage < lastPage);

				// Finalize crypt
				if (!down)
				{
					writeDbHeader(tdbb, 0);
				}
				}

				// Release exclusive lock on StartCryptThread
				lckRelease = true;
				LCK_release(tempDbb, threadLock);
				Monitoring::cleanupAttachment(tempDbb);
				attachment->releaseLocks(tempDbb);
				LCK_fini(tempDbb, LCK_OWNER_attachment);
			}
			catch (const Exception&)
			{
				try
				{
					if (!lckRelease)
					{
						// Release exclusive lock on StartCryptThread
						LCK_release(tempDbb, threadLock);
						Monitoring::cleanupAttachment(tempDbb);
						attachment->releaseLocks(tempDbb);
						LCK_fini(tempDbb, LCK_OWNER_attachment);
					}
				}
				catch (const Exception&)
				{ }

				throw;
			}
		}
		catch (const Exception& ex)
		{
			// Error during context creation - we can't even release lock
			iscLogException("Crypt thread:", ex);
		}
	}

	void CryptoManager::writeDbHeader(thread_db* tdbb, ULONG runpage)
	{
		CchHdr hdr(tdbb, LCK_write);

		Ods::header_page* header = hdr.write();
		header->hdr_crypt_page = runpage;
		if (!runpage)
		{
			header->hdr_flags &= ~Ods::hdr_crypt_process;
			process = false;

			if (!crypt)
			{
				ClumpletWriter hc(ClumpletWriter::UnTagged, header->hdr_page_size);
				hdr.getClumplets(hc);
				hc.deleteWithTag(Ods::HDR_crypt_hash);
				hc.deleteWithTag(Ods::HDR_crypt_key);
				hdr.setClumplets(hc);
			}
		}

		digitalySignDatabase(hdr);
		hdr.flush();
	}

	bool CryptoManager::read(thread_db* tdbb, FbStatusVector* sv, Ods::pag* page, IOCallback* io)
	{
		// Code calling us is not ready to process exceptions correctly
		// Therefore use old (status vector based) method
		try
		{
			// Normal case (almost always get here)
			// Take shared lock on crypto manager and read data
			if (!slowIO)
			{
				BarSync::IoGuard ioGuard(tdbb, sync);
				if (!slowIO)
					return internalRead(tdbb, sv, page, io) == SUCCESS_ALL;
			}

			// Slow IO - we need exclusive lock on crypto manager.
			// That may happen only when another process changed DB encyption.
			BarSync::LockGuard lockGuard(tdbb, sync);
			lockGuard.lock();
			for (SINT64 previous = slowIO; ; previous = slowIO)
			{
				switch (internalRead(tdbb, sv, page, io))
				{
				case SUCCESS_ALL:
					if (!slowIO)				// if we took a lock last time
						return true;			// nothing else left to do - IO complete

					// An attempt to take a lock, if it fails
					// we get fresh data from lock needed to validate state of encryption.
					// Notice - if lock was taken that's also a kind of state
					// change and first time we must proceed with one more read.
					lockAndReadHeader(tdbb, CRYPT_HDR_NOWAIT);
					if (slowIO == previous)		// if crypt state did not change
						return true;			// we successfully completed IO
					break;

				case FAILED_IO:
					return false;				// not related with crypto manager error

				case FAILED_CRYPT:
					if (!slowIO)				// if we took a lock last time
						return false;			// we can't recover from error here

					lockAndReadHeader(tdbb, CRYPT_HDR_NOWAIT);
					if (slowIO == previous)		// if crypt state did not change
						return false;			// we can't recover from error here
					break;
				}
			}
		}
		catch (const Exception& ex)
		{
			ex.stuffException(sv);
		}
		return false;
	}

	CryptoManager::IoResult CryptoManager::internalRead(thread_db* tdbb, FbStatusVector* sv,
		Ods::pag* page, IOCallback* io)
	{
		if (!io->callback(tdbb, sv, page))
			return FAILED_IO;

		if (page->pag_flags & Ods::crypted_page)
		{
			fb_assert(cryptPlugin);
			if (!cryptPlugin)
			{
				Arg::Gds(isc_decrypt_error).copyTo(sv);
				return FAILED_CRYPT;
			}

			cryptPlugin->decrypt(sv, dbb.dbb_page_size - sizeof(Ods::pag),
				&page[1], &page[1]);
			if (sv->getState() & IStatus::STATE_ERRORS)
				return FAILED_CRYPT;
		}

		return SUCCESS_ALL;
	}

	bool CryptoManager::write(thread_db* tdbb, FbStatusVector* sv, Ods::pag* page, IOCallback* io)
	{
		// Code calling us is not ready to process exceptions correctly
		// Therefore use old (status vector based) method
		try
		{
			// Sanity check
			if (page->pag_type > pag_max)
				Arg::Gds(isc_page_type_err).raise();

			// Page is never going to be encrypted. No locks needed.
			if (!Ods::pag_crypt_page[page->pag_type])
				return internalWrite(tdbb, sv, page, io) == SUCCESS_ALL;

			// Normal case (almost always get here)
			// Take shared lock on crypto manager and write data
			if (!slowIO)
			{
				BarSync::IoGuard ioGuard(tdbb, sync);
				if (!slowIO)
					return internalWrite(tdbb, sv, page, io) == SUCCESS_ALL;
			}

			// Have to use slow method - see full comments in read() function
			BarSync::LockGuard lockGuard(tdbb, sync);
			lockGuard.lock();
			for (SINT64 previous = slowIO; ; previous = slowIO)
			{
				switch (internalWrite(tdbb, sv, page, io))
				{
				case SUCCESS_ALL:
					if (!slowIO)
						return true;

					lockAndReadHeader(tdbb, CRYPT_HDR_NOWAIT);
					if (slowIO == previous)
						return true;
					break;

				case FAILED_IO:
					return false;

				case FAILED_CRYPT:
					if (!slowIO)
						return false;

					lockAndReadHeader(tdbb, CRYPT_HDR_NOWAIT);
					if (slowIO == previous)
						return false;
					break;
				}
			}
		}
		catch (const Exception& ex)
		{
			ex.stuffException(sv);
		}
		return false;
	}

	CryptoManager::IoResult CryptoManager::internalWrite(thread_db* tdbb, FbStatusVector* sv,
		Ods::pag* page, IOCallback* io)
	{
		Buffer to;
		Ods::pag* dest = page;
		UCHAR savedFlags = page->pag_flags;

		if (crypt && Ods::pag_crypt_page[page->pag_type])
		{
			fb_assert(cryptPlugin);
			if (!cryptPlugin)
			{
				Arg::Gds(isc_encrypt_error).copyTo(sv);
				return FAILED_CRYPT;
			}

			to[0] = page[0];
			cryptPlugin->encrypt(sv, dbb.dbb_page_size - sizeof(Ods::pag),
				&page[1], &to[1]);
			if (sv->getState() & IStatus::STATE_ERRORS)
				return FAILED_CRYPT;

			to->pag_flags |= Ods::crypted_page;		// Mark page that is going to be written as encrypted
			page->pag_flags |= Ods::crypted_page;	// Set the mark for page in cache as well
			dest = to;								// Choose correct destination
		}
		else
		{
			page->pag_flags &= ~Ods::crypted_page;
		}

		if (!io->callback(tdbb, sv, dest))
		{
			page->pag_flags = savedFlags;
			return FAILED_IO;
		}

		return SUCCESS_ALL;
	}

	int CryptoManager::blockingAstChangeCryptState(void* object)
	{
		((CryptoManager*) object)->blockingAstChangeCryptState();
		return 0;
	}

	ULONG CryptoManager::getCurrentPage() const
	{
		return process ? currentPage : 0;
	}

	ULONG CryptoManager::getLastPage(thread_db* tdbb)
	{
		return PAG_last_page(tdbb) + 1;
	}

    UCHAR CryptoManager::getCurrentState() const
	{
		return (crypt ? fb_info_crypt_encrypted : 0) | (process ? fb_info_crypt_process : 0);
	}

	CryptoManager::HolderAttachments::HolderAttachments(MemoryPool& p)
		: keyHolder(NULL), attachments(p)
	{ }

	void CryptoManager::HolderAttachments::setPlugin(IKeyHolderPlugin* kh)
	{
		keyHolder = kh;
		keyHolder->addRef();
	}

	CryptoManager::HolderAttachments::~HolderAttachments()
	{
		if (keyHolder)
		{
			PluginManagerInterfacePtr()->releasePlugin(keyHolder);
		}
	}

	void CryptoManager::HolderAttachments::registerAttachment(Attachment* att)
	{
		attachments.add(att);
	}

	bool CryptoManager::HolderAttachments::unregisterAttachment(Attachment* att)
	{
		unsigned i = attachments.getCount();
		while (i--)
		{
			if (attachments[i] == att)
			{
				attachments.remove(i);
				break;
			}
		}
		return attachments.getCount() == 0;
	}

	bool CryptoManager::HolderAttachments::operator==(IKeyHolderPlugin* kh) const
	{
		// ASF: I think there should be a better way to do this.
		return keyHolder->cloopVTable == kh->cloopVTable;
	}

	void CryptoManager::KeyHolderPlugins::attach(Attachment* att, Config* config)
	{
		MutexLockGuard g(holdersMutex, FB_FUNCTION);

		for (GetPlugins<IKeyHolderPlugin> keyControl(IPluginManager::TYPE_KEY_HOLDER, config);
			keyControl.hasData(); keyControl.next())
		{
			IKeyHolderPlugin* keyPlugin = keyControl.plugin();
			FbLocalStatus st;
			if (keyPlugin->keyCallback(&st, att->att_crypt_callback) > 0)
			{
				// holder accepted attachment's key
				HolderAttachments* ha = NULL;

				for (unsigned i = 0; i < knownHolders.getCount(); ++i)
				{
					if (knownHolders[i] == keyPlugin)
					{
						ha = &knownHolders[i];
						break;
					}
				}

				if (!ha)
				{
					ha = &(knownHolders.add());
					ha->setPlugin(keyPlugin);
				}

				ha->registerAttachment(att);
				break;		// Do not need >1 key from attachment to single DB
			}
			else
				st.check();
		}
	}

	void CryptoManager::KeyHolderPlugins::detach(Attachment* att)
	{
		MutexLockGuard g(holdersMutex, FB_FUNCTION);

		unsigned i = knownHolders.getCount();
		while (i--)
		{
			if (knownHolders[i].unregisterAttachment(att))
			{
				knownHolders.remove(i);
			}
		}
	}

	void CryptoManager::KeyHolderPlugins::init(IDbCryptPlugin* crypt, const char* keyName)
	{
		MutexLockGuard g(holdersMutex, FB_FUNCTION);

		Firebird::HalfStaticArray<Firebird::IKeyHolderPlugin*, 64> holdersVector;
		unsigned int length = knownHolders.getCount();
		IKeyHolderPlugin** vector = holdersVector.getBuffer(length);
		for (unsigned i = 0; i < length; ++i)
		{
			vector[i] = knownHolders[i].getPlugin();
		}

		FbLocalStatus st;
		crypt->setKey(&st, length, vector, keyName);
		st.check();
	}

	void CryptoManager::addClumplet(string& signature, ClumpletReader& block, UCHAR tag)
	{
		if (block.find(tag))
		{
			string tmp;
			block.getString(tmp);
			signature += ' ';
			signature += tmp;
		}
	}

	void CryptoManager::calcDigitalSignature(string& signature, const Header& hdr)
	{
		/*
		We use the following items to calculate digital signature (hash of encrypted string)
		for database:
			hdr_flags & (hdr_crypt_process | hdr_encrypted)
			hdr_crypt_page
			hdr_crypt_plugin
			HDR_crypt_key
			HDR_crypt_hash
		*/

		signature.printf("%d %d %d %s",
			(hdr->hdr_flags & Ods::hdr_crypt_process ? 1 : 0),
			(hdr->hdr_flags & Ods::hdr_encrypted ? 1 : 0),
			hdr->hdr_crypt_page,
			hdr->hdr_crypt_plugin);

		ClumpletWriter hc(ClumpletWriter::UnTagged, hdr->hdr_page_size);
		hdr.getClumplets(hc);

		addClumplet(signature, hc, Ods::HDR_crypt_key);
		addClumplet(signature, hc, Ods::HDR_crypt_hash);

		const unsigned QUANTUM = 16;
		signature += string(QUANTUM - 1, '$');
		unsigned len = signature.length();
		len &= ~(QUANTUM - 1);

		loadPlugin(hdr->hdr_crypt_plugin);

		string enc;
		FbLocalStatus sv;
		cryptPlugin->encrypt(&sv, len, signature.c_str(), enc.getBuffer(len));
		if (sv->getState() & IStatus::STATE_ERRORS)
			Arg::StatusVector(&sv).raise();

		Sha1::hashBased64(signature, enc);
	}


	void CryptoManager::digitalySignDatabase(CchHdr& hdr)
	{
		ClumpletWriter hc(ClumpletWriter::UnTagged, hdr->hdr_page_size);
		hdr.getClumplets(hc);

		bool wf = hc.find(Ods::HDR_crypt_checksum);
		hc.deleteWithTag(Ods::HDR_crypt_checksum);

		if (hdr->hdr_flags & (Ods::hdr_crypt_process | Ods::hdr_encrypted))
		{
			wf = true;
			string signature;
			calcDigitalSignature(signature, hdr);
			hc.insertString(Ods::HDR_crypt_checksum, signature);
		}

		if (wf)
			hdr.setClumplets(hc);
	}

	void CryptoManager::checkDigitalSignature(const Header& hdr)
	{
		if (hdr->hdr_flags & (Ods::hdr_crypt_process | Ods::hdr_encrypted))
		{
			ClumpletWriter hc(ClumpletWriter::UnTagged, hdr->hdr_page_size);
			hdr.getClumplets(hc);
			if (!hc.find(Ods::HDR_crypt_checksum))
				Arg::Gds(isc_crypt_checksum).raise();

			string sig1, sig2;
			hc.getString(sig1);
			calcDigitalSignature(sig2, hdr);
			if (sig1 != sig2)
				Arg::Gds(isc_crypt_checksum).raise();
		}
	}

} // namespace Jrd
