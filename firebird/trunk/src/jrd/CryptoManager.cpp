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
#include "../jrd/os/pio_proto.h"
#include "../common/isc_proto.h"
#include "../common/classes/GetPlugins.h"
#include "../common/classes/RefMutex.h"

using namespace Firebird;

namespace {
	THREAD_ENTRY_DECLARE cryptThreadStatic(THREAD_ENTRY_PARAM p)
	{
		Jrd::CryptoManager* cryptoManager = (Jrd::CryptoManager*) p;
		cryptoManager->cryptThread();

		return 0;
	}

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

	public:
		const Ods::header_page* operator->() const
		{
			return header;
		}

		operator const Ods::header_page*() const
		{
			return header;
		}

		// This routine is looking for a clumplet on header page but is not ready to handle continuation
		// Fortunately, modern pages of size 4k and bigger can contain everything on one page.
		bool searchEntry(UCHAR type, UCHAR& out_len, const UCHAR* &entry)
		{
			const UCHAR* end = ((const UCHAR*) header) + header->hdr_page_size;
			for (const UCHAR* p = header->hdr_data; (p < end - 2) && (*p != Ods::HDR_end); p += 2u + p[1])
			{
				if (*p == type)
				{
					out_len = p[1];
					entry = p + 2;
					if (entry + out_len > end)
						out_len = end - entry;
					return true;
				}
			}
			return false;
		}

	private:
		Ods::header_page* header;
	};


	class CchHdr : public Header
	{
	public:
		CchHdr(Jrd::thread_db* p_tdbb, USHORT lockType)
			: window(Jrd::HEADER_PAGE_NUMBER),
			  tdbb(p_tdbb)
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
			CCH_MARK_MUST_WRITE(tdbb, &window);
			return const_cast<Ods::header_page*>(operator->());
		}

		void depends(Stack<ULONG>& pages)
		{
			while (pages.hasData())
			{
				CCH_precedence(tdbb, &window, pages.pop());
			}
		}

		~CchHdr()
		{
			CCH_RELEASE(tdbb, &window);
		}

	private:
		Jrd::WIN window;
		Jrd::thread_db* tdbb;
	};

	class PhysHdr : public Header
	{
	public:
		explicit PhysHdr(Jrd::thread_db* tdbb)
		{
			Jrd::Database* dbb = tdbb->getDatabase();

			UCHAR* h = FB_NEW_POOL(*Firebird::MemoryPool::getContextPool()) UCHAR[dbb->dbb_page_size + PAGE_ALIGNMENT];
			buffer.reset(h);

			h = FB_ALIGN(h, PAGE_ALIGNMENT);
			PIO_header(tdbb, h, dbb->dbb_page_size);
			setHeader(h);
		}

	private:
		AutoPtr<UCHAR, ArrayDelete<UCHAR> > buffer;
	};

	const UCHAR CRYPT_RELEASE = LCK_SR;
	const UCHAR CRYPT_NORMAL = LCK_PR;
	const UCHAR CRYPT_CHANGE = LCK_PW;
	const UCHAR CRYPT_INIT = LCK_EX;
	
	const int MAX_PLUGIN_NAME_LEN = 31;
}


namespace Jrd {

	CryptoManager::CryptoManager(thread_db* tdbb)
		: PermanentStorage(*tdbb->getDatabase()->dbb_permanent),
		  sync(this),
		  keyHolderPlugins(getPool()),
		  cryptThreadId(0),
		  cryptPlugin(NULL),
		  dbb(*tdbb->getDatabase()),
		  slowIO(0),
		  crypt(false),
		  process(false),
		  down(false)
	{
		stateLock = FB_NEW_RPT(getPool(), 0)
			Lock(tdbb, 0, LCK_crypt_status, this, blockingAstChangeCryptState);
		threadLock = FB_NEW_RPT(getPool(), 0) Lock(tdbb, 0, LCK_crypt);
	}

	CryptoManager::~CryptoManager()
	{
		delete stateLock;
		delete threadLock;
	}

	void CryptoManager::terminateCryptThread(thread_db*)
	{
		if (cryptThreadId)
		{
			down = true;
			Thread::waitForCompletion(cryptThreadId);
			cryptThreadId = 0;
		}
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
				// Failed to take state lock - swith to slow IO mode
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
		if (crypt || process)
		{
			loadPlugin(hdr->hdr_crypt_plugin);
		}
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
		keyHolderPlugins.init(p);
		cryptPlugin = p;
		cryptPlugin->addRef();
	}

	void CryptoManager::prepareChangeCryptState(thread_db* tdbb, const Firebird::MetaName& plugName)
	{
		if (plugName.length() > MAX_PLUGIN_NAME_LEN)
		{
			(Arg::Gds(isc_cp_name_too_long) << Arg::Num(MAX_PLUGIN_NAME_LEN)).raise();
		}

		const bool newCryptState = plugName.hasData();
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

			// Load plugin
			if (newCryptState)
			{
				if (cryptPlugin)
					(Arg::Gds(isc_cp_already_crypted)).raise();

				loadPlugin(plugName.c_str());
			}
		}
	}

	void CryptoManager::changeCryptState(thread_db* tdbb, const Firebird::string& plugName)
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
			if (crypt)
			{
				header->hdr_flags |= Ods::hdr_encrypted;
				plugName.copyTo(header->hdr_crypt_plugin, sizeof(header->hdr_crypt_plugin));
			}
			else
			{
				header->hdr_flags &= ~Ods::hdr_encrypted;
			}

			// Set hdr_crypt_page for crypt thread
			header->hdr_crypt_page = 1;
			header->hdr_flags |= Ods::hdr_crypt_process;
			process = true;
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

	void CryptoManager::startCryptThread(thread_db* tdbb)
	{
		// Try to take crypt mutex
		// If can't take that mutex - nothing to do, cryptThread already runs in our process
		MutexEnsureUnlock guard(cryptThreadMtx, FB_FUNCTION);
		if (!guard.tryEnter())
		{
			return;
		}

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
			currentPage.setValue(hdr->hdr_crypt_page);

			// Refresh encryption flag
			crypt = hdr->hdr_flags & Ods::hdr_encrypted ? true : false;

			// If we are going to start crypt thread, we need plugin to be loaded
			loadPlugin(hdr->hdr_crypt_plugin);

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

	void CryptoManager::attach(thread_db* tdbb, Attachment* att)
	{
		keyHolderPlugins.attach(att, dbb.dbb_config);

		lockAndReadHeader(tdbb, CRYPT_HDR_INIT);
	}

	void CryptoManager::cryptThread()
	{
		FbLocalStatus status_vector;

		try
		{
			// Try to take crypt mutex
			// If can't take that mutex - nothing to do, cryptThread already runs in our process
			MutexEnsureUnlock guard(cryptThreadMtx, FB_FUNCTION);
			if (!guard.tryEnter())
			{
				return;
			}

			// establish context
			UserId user;
			user.usr_user_name = "(Crypt thread)";

			Jrd::Attachment* const attachment = Jrd::Attachment::create(&dbb);
			RefPtr<SysStableAttachment> sAtt(FB_NEW SysStableAttachment(attachment));
			attachment->setStable(sAtt);
			attachment->att_filename = dbb.dbb_filename;
			attachment->att_user = &user;

			BackgroundContextHolder tdbb(&dbb, attachment, &status_vector, FB_FUNCTION);
			tdbb->tdbb_quantum = SWEEP_QUANTUM;

			ULONG lastPage = getLastPage(tdbb);
			ULONG runpage = 1;
			Stack<ULONG> pages;

			// Take exclusive threadLock
			// If can't take that lock - nothing to do, cryptThread already runs somewhere
			if (!LCK_lock(tdbb, threadLock, LCK_EX, LCK_NO_WAIT))
			{
				return;
			}

			bool lckRelease = false;

			try
			{
				do
				{
					// Check is there some job to do
					while ((runpage = currentPage.exchangeAdd(+1)) < lastPage)
					{
						// forced terminate
						if (down)
						{
							break;
						}

						// nbackup state check
						if (dbb.dbb_backup_manager && dbb.dbb_backup_manager->getState() != Ods::hdr_nbak_normal)
						{
							if (static_cast<ULONG>(currentPage.exchangeAdd(-1)) >= lastPage)
							{
								// currentPage was set to last page by thread, closing database
								break;
							}
							Thread::sleep(100);
							continue;
						}

						// scheduling
						if (--tdbb->tdbb_quantum < 0)
						{
							JRD_reschedule(tdbb, SWEEP_QUANTUM, true);
						}

						// writing page to disk will change it's crypt status in usual way
						WIN window(DB_PAGE_SPACE, runpage);
						Ods::pag* page = CCH_FETCH(tdbb, &window, LCK_write, pag_undefined);
						if (page && page->pag_type <= pag_max &&
							(bool(page->pag_flags & Ods::crypted_page) != crypt) &&
							Ods::pag_crypt_page[page->pag_type])
						{
							CCH_MARK(tdbb, &window);
							pages.push(runpage);
						}
						CCH_RELEASE_TAIL(tdbb, &window);

						// sometimes save currentPage into DB header
						++runpage;
						if ((runpage & 0x3FF) == 0)
						{
							writeDbHeader(tdbb, runpage, pages);
						}
					}

					// At this moment of time all pages with number < lastpage
					// are guaranteed to change crypt state. Check for added pages.
					lastPage = getLastPage(tdbb);

					// forced terminate
					if (down)
					{
						break;
					}
				} while (runpage < lastPage);

				// Finalize crypt
				if (!down)
				{
					writeDbHeader(tdbb, 0, pages);
					if (!crypt)
					{
						// Decryption finished, unload plugin as we don't need it anymore
						PluginManagerInterfacePtr()->releasePlugin(cryptPlugin);
						cryptPlugin = NULL;
					}
				}

				// Release exclusive lock on StartCryptThread
				lckRelease = true;
				LCK_release(tdbb, threadLock);
			}
			catch (const Exception&)
			{
				try
				{
					if (!lckRelease)
					{
						// try to save current state of crypt thread
						if (!down)
						{
							writeDbHeader(tdbb, runpage, pages);
						}

						// Release exclusive lock on StartCryptThread
						LCK_release(tdbb, threadLock);
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

	void CryptoManager::writeDbHeader(thread_db* tdbb, ULONG runpage, Stack<ULONG>& pages)
	{
		CchHdr hdr(tdbb, LCK_write);
		hdr.depends(pages);

		Ods::header_page* header = hdr.write();
		header->hdr_crypt_page = runpage;
		if (!runpage)
		{
			header->hdr_flags &= ~Ods::hdr_crypt_process;
			process = false;
		}
	}

	bool CryptoManager::read(thread_db* tdbb, FbStatusVector* sv, jrd_file* file,
		BufferDesc* bdb, Ods::pag* page, bool noShadows, PageSpace* pageSpace)
	{
		// Code calling us is not ready to process exceptions correctly
		// Therefore use old (status vector based) method
		try
		{
			if (!slowIO)
			{
				// Normal case (almost always get here)
				// Take shared lock on crypto manager and read data
				BarSync::IoGuard ioGuard(tdbb, sync);
				if (!slowIO)
					return internalRead(tdbb, sv, file, bdb, page, noShadows, pageSpace) == SUCCESS_ALL;
			}

			// Slow IO - we need exclusive lock on crypto manager.
			// That may happen only when another process changed DB encyption.
			BarSync::LockGuard lockGuard(tdbb, sync);
			for (SINT64 previous = slowIO; ; previous = slowIO)
			{
				switch (internalRead(tdbb, sv, file, bdb, page, noShadows, pageSpace))
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
		jrd_file* file, BufferDesc* bdb, Ods::pag* page, bool noShadows, PageSpace* pageSpace)
	{
		fb_assert(pageSpace || noShadows);
		int retryCount = 0;

		while (!PIO_read(tdbb, file, bdb, page, sv))
		{
			if (noShadows)
				return FAILED_IO;

			if (!CCH_rollover_to_shadow(tdbb, &dbb, file, false))
				return FAILED_IO;

			if (file != pageSpace->file)
				file = pageSpace->file;
			else
			{
				if (retryCount++ == 3)
				{
					gds__log("IO error loop Unwind to avoid a hang");
					return FAILED_IO;
				}
			}
		}

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

	bool CryptoManager::write(thread_db* tdbb, FbStatusVector* sv, jrd_file* file,
		BufferDesc* bdb, Ods::pag* page)
	{
		// Code calling us is not ready to process exceptions correctly
		// Therefore use old (status vector based) method
		try
		{
			if (!slowIO)
			{
				// Normal case (almost always get here)
				// Take shared lock on crypto manager and write data
				BarSync::IoGuard ioGuard(tdbb, sync);
				if (!slowIO)
				return internalWrite(tdbb, sv, file, bdb, page) == SUCCESS_ALL;
			}

			// Have to use slow method - see full comments in read() function
			BarSync::LockGuard lockGuard(tdbb, sync);
			for (SINT64 previous = slowIO; ; previous = slowIO)
			{
				switch (internalWrite(tdbb, sv, file, bdb, page))
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
		jrd_file* file, BufferDesc* bdb, Ods::pag* page)
	{
		Buffer to;
		Ods::pag* dest = page;
		UCHAR savedFlags = page->pag_flags;

		fb_assert((!crypt) || cryptPlugin);
		if (crypt && Ods::pag_crypt_page[page->pag_type % (pag_max + 1)])
		{
			if (!cryptPlugin)
			{
				Arg::Gds(isc_decrypt_error).copyTo(sv);	//!!!!
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

		if (!PIO_write(tdbb, file, bdb, dest, sv))
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

	ULONG CryptoManager::getCurrentPage()
	{
		return process ? currentPage.value() : 0;
	}

	ULONG CryptoManager::getLastPage(thread_db* tdbb)
	{
		return PAG_last_page(tdbb) + 1;
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
			if (keyPlugin->keyCallback(&st, att->att_crypt_callback) == 1)	//// FIXME: 1 ???
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

	void CryptoManager::KeyHolderPlugins::init(IDbCryptPlugin* crypt)
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
		crypt->setKey(&st, length, vector);
		st.check();
	}

} // namespace Jrd
