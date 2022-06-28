/*
 *	PROGRAM:	JRD Backup and Restore Program
 *	MODULE:		multivol.cpp
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
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include "../burp/burp.h"
#include "../burp/burp_proto.h"
#include "../burp/mvol_proto.h"
#include "../burp/split/spit.h"
#include "../burp/BurpTasks.h"
#include "../yvalve/gds_proto.h"
#include "../common/gdsassert.h"
#include "../common/os/os_utils.h"
#include <fcntl.h>
#include <sys/types.h>

#ifdef HAVE_IO_H
#include <io.h>  // isatty
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../common/os/guid.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/GetPlugins.h"
#include "../common/classes/auto.h"
#include "../common/classes/SafeArg.h"
#include "../common/StatusHolder.h"
#include "../common/db_alias.h"
#include "../common/status.h"
#include "../common/classes/zip.h"

using MsgFormat::SafeArg;
using Firebird::FbLocalStatus;
using namespace Burp;

const int open_mask	= 0666;

#ifdef WIN_NT
const char* TERM_INPUT	= "CONIN$";
const char* TERM_OUTPUT	= "CONOUT$";
#else
const char* TERM_INPUT	= "/dev/tty";
const char* TERM_OUTPUT	= "/dev/tty";
#endif

static int	 mvol_read(int*, UCHAR**);
static FB_UINT64 mvol_fini_read(BurpGlobals*);
static void	 mvol_init_read(BurpGlobals*, const char*, USHORT*, int*, UCHAR**);
static UCHAR mvol_write(const UCHAR, int*, UCHAR**);
static const UCHAR*	mvol_write_block(BurpGlobals*, const UCHAR*, ULONG);
static FB_UINT64 mvol_fini_write(BurpGlobals*, int*, UCHAR**);
static void	 mvol_init_write(BurpGlobals*, const char*, int*, UCHAR**);
static void	 brio_fini(BurpGlobals*);

static const int MAX_HEADER_SIZE		= 512;
static const int ZC_BUFSIZE				= IO_BUFFER_SIZE;

static inline int get(BurpGlobals* tdgbl)
{
	if (tdgbl->mvol_io_cnt <= 0)
		mvol_read(NULL, NULL);
	return (--tdgbl->mvol_io_cnt >= 0 ? *tdgbl->mvol_io_ptr++ : 255);
}

static inline void put(BurpGlobals* tdgbl, UCHAR c)
{
	--tdgbl->mvol_io_cnt;
	*tdgbl->mvol_io_ptr++ = c;
}

#ifdef DEBUG
static UCHAR debug_on = 0;		// able to turn this on in debug mode
#endif

const int burp_msg_fac = 12;

#ifdef HAVE_ZLIB_H
static Firebird::InitInstance<Firebird::ZLib> zlib;
#endif // HAVE_ZLIB_H

static void  bad_attribute(int, USHORT);
static void  file_not_empty();
static SLONG get_numeric();
static int   get_text(UCHAR*, SSHORT);
static void  prompt_for_name(SCHAR*, int);
static void  put_asciz(SCHAR, const SCHAR*);
static void  put_numeric(SCHAR, int);
static bool  read_header(DESC, ULONG*, USHORT*, bool);
static bool  write_header(DESC, ULONG, bool);
static DESC	 next_volume(DESC, ULONG, bool);
static void	 os_read(int*, UCHAR**);
static void	 crypt_write_block(BurpGlobals*, const UCHAR*, FB_SIZE_T, bool);
static ULONG crypt_read_block(BurpGlobals*, UCHAR*, FB_SIZE_T);
static void	 zip_write_block(BurpGlobals*, const UCHAR*, FB_SIZE_T, bool);
static ULONG unzip_read_block(BurpGlobals*, UCHAR*, FB_SIZE_T);

// Portion of data passed to crypt plugin
const ULONG CRYPT_STEP = 256;

class DbInfo final : public Firebird::RefCntIface<Firebird::IDbCryptInfoImpl<DbInfo, Firebird::CheckStatusWrapper> >
{
public:
	DbInfo(BurpGlobals* bg)
		: tdgbl(bg)
	{ }

	// IDbCryptInfo implementation
	const char* getDatabaseFullPath(Firebird::CheckStatusWrapper*)
	{
		return tdgbl->gbl_database_file_name;
	}

private:
	BurpGlobals* tdgbl;
};


struct BurpCrypt
{
	BurpCrypt()
		 : crypt_plugin(NULL), db_info(NULL), holder_plugin(NULL), crypt_callback(NULL)
	{ }

	~BurpCrypt()
	{
		if (crypt_plugin)
			Firebird::PluginManagerInterfacePtr()->releasePlugin(crypt_plugin);
		if (holder_plugin)
			Firebird::PluginManagerInterfacePtr()->releasePlugin(holder_plugin);
	}

	Firebird::IDbCryptPlugin* crypt_plugin;
	Firebird::RefPtr<DbInfo> db_info;
	Firebird::IKeyHolderPlugin* holder_plugin;
	Firebird::ICryptKeyCallback* crypt_callback;
};


//____________________________________________________________
//
//
static void calc_hash(Firebird::string& valid, Firebird::IDbCryptPlugin* plugin)
{
	// crypt verifier
	const char* sample = "0123456789ABCDEF";
	char result[16];
	FbLocalStatus sv;

	plugin->encrypt(&sv, sizeof(result), sample, result);
	check(&sv);

	// calculate its hash
	const Firebird::string verifier(result, sizeof(result));
	Firebird::Sha1::hashBased64(valid, verifier);
}

//____________________________________________________________
//
//
static Firebird::IKeyHolderPlugin* mvol_get_holder(BurpGlobals* tdgbl, Firebird::RefPtr<const Firebird::Config>& config)
{
	fb_assert(tdgbl->gbl_sw_keyholder);

	if (!tdgbl->gbl_crypt)
	{
		Firebird::GetPlugins<Firebird::IKeyHolderPlugin>
			keyControl(Firebird::IPluginManager::TYPE_KEY_HOLDER, config, tdgbl->gbl_sw_keyholder);
		if (!keyControl.hasData())
			(Firebird::Arg::Gds(isc_no_keyholder_plugin) << tdgbl->gbl_sw_keyholder).raise();

		BurpCrypt* g = tdgbl->gbl_crypt = FB_NEW_POOL(tdgbl->getPool()) BurpCrypt;
		g->holder_plugin = keyControl.plugin();
		g->holder_plugin->addRef();

		// Also do not forget about keys from services manager
		Firebird::ICryptKeyCallback* cb = tdgbl->uSvc->getCryptCallback();
		if (cb)
			g->holder_plugin->keyCallback(&tdgbl->throwStatus, cb);
	}

	return tdgbl->gbl_crypt->holder_plugin;
}

//____________________________________________________________
//
//
Firebird::ICryptKeyCallback* MVOL_get_crypt(BurpGlobals* tdgbl)
{
	fb_assert(tdgbl->gbl_sw_keyholder);

	if (!tdgbl->gbl_crypt)
	{
		// Get per-DB config
		Firebird::PathName dummy;
		Firebird::RefPtr<const Firebird::Config> config;
		expandDatabaseName(tdgbl->gbl_database_file_name, dummy, &config);

		mvol_get_holder(tdgbl, config);
	}

	BurpCrypt* g = tdgbl->gbl_crypt;
	if (!g->crypt_callback)
	{
		FbLocalStatus status;

		g->crypt_callback = g->holder_plugin->chainHandle(&status);
		check(&status);
	}

	return g->crypt_callback;
}

//____________________________________________________________
//
//
static void start_crypt(BurpGlobals* tdgbl)
{
	fb_assert(tdgbl->gbl_sw_keyholder);
	if (tdgbl->gbl_crypt && tdgbl->gbl_crypt->crypt_plugin)
		return;

	FbLocalStatus status;

	// Get per-DB config
	Firebird::PathName dummy;
	Firebird::RefPtr<const Firebird::Config> config;
	expandDatabaseName(tdgbl->gbl_database_file_name, dummy, &config);

	// Prepare key holders
	Firebird::IKeyHolderPlugin* keyHolder = mvol_get_holder(tdgbl, config);

	// Load crypt plugin
	if (!tdgbl->mvol_crypt)
		tdgbl->mvol_crypt = tdgbl->gbl_sw_crypt;
	if (!tdgbl->mvol_crypt)
		BURP_error(378, true);

	Firebird::GetPlugins<Firebird::IDbCryptPlugin>
		cryptControl(Firebird::IPluginManager::TYPE_DB_CRYPT, config, tdgbl->mvol_crypt);
	if (!cryptControl.hasData())
		(Firebird::Arg::Gds(isc_no_crypt_plugin) << tdgbl->mvol_crypt).raise();

	Firebird::RefPtr<DbInfo> dbInfo(FB_NEW DbInfo(tdgbl));
	Firebird::IDbCryptPlugin* p = cryptControl.plugin();
	p->setInfo(&status, dbInfo);
	if (!status.isSuccess())
	{
		const ISC_STATUS* v = status->getErrors();
		if (v[0] == isc_arg_gds && v[1] != isc_arg_end && v[1] != isc_interface_version_too_old)
			Firebird::status_exception::raise(&status);
	}

	// Initialize key in crypt plugin
	p->setKey(&status, 1, &keyHolder, tdgbl->mvol_keyname);
	check(&status);

	// Validate hash
	if (tdgbl->gbl_key_hash[0])
	{
		Firebird::string hash;
		calc_hash(hash, p);
		if (hash != tdgbl->gbl_key_hash)
			(Firebird::Arg::Gds(isc_bad_crypt_key) << tdgbl->mvol_keyname).raise();
	}

	// crypt plugin is ready
	BurpCrypt* g = tdgbl->gbl_crypt;
	g->db_info.moveFrom(dbInfo);
	g->crypt_plugin = p;
	p->addRef();
}

//____________________________________________________________
//
//
static void crypt_write_block(BurpGlobals* tdgbl, const UCHAR* buffer, FB_SIZE_T buffer_length, bool flash)
{
	if (!(tdgbl->gbl_sw_keyholder))
	{
		mvol_write_block(tdgbl, buffer, buffer_length);
		return;
	}

	start_crypt(tdgbl);
	fb_assert(tdgbl->gbl_crypt);

	while (buffer_length)
	{
		ULONG step = MIN(buffer_length + tdgbl->gbl_crypt_left, ZC_BUFSIZE);
		fb_assert(CRYPT_STEP < ZC_BUFSIZE);

		ULONG move = step - tdgbl->gbl_crypt_left;
		memcpy(tdgbl->gbl_crypt_buffer + tdgbl->gbl_crypt_left, buffer, move);
		buffer_length -= move;
		buffer += move;

		tdgbl->gbl_crypt_left = step % CRYPT_STEP;
		step -= tdgbl->gbl_crypt_left;

		if (flash && buffer_length == 0 && tdgbl->gbl_crypt_left)
		{
			step += CRYPT_STEP;
			tdgbl->gbl_crypt_left = 0;
		}

		FbLocalStatus status;

		for (ULONG offset = 0; offset < step; offset += CRYPT_STEP)
		{
			UCHAR* b = &tdgbl->gbl_crypt_buffer[offset];
			tdgbl->gbl_crypt->crypt_plugin->encrypt(&status, CRYPT_STEP, b, b);
			check(&status);
		}

		mvol_write_block(tdgbl, tdgbl->gbl_crypt_buffer, step);
		memmove(tdgbl->gbl_crypt_buffer, &tdgbl->gbl_crypt_buffer[step], tdgbl->gbl_crypt_left);
	}
}


//____________________________________________________________
//
//
static ULONG crypt_read_block(BurpGlobals* tdgbl, UCHAR* buffer, FB_SIZE_T buffer_length)
{
	ULONG& length = tdgbl->gbl_crypt_left;

	while (length < (tdgbl->gbl_key_hash[0] ? CRYPT_STEP : 1))
	{
		// free bytes in gbl_crypt_buffer
		ULONG count = ZC_BUFSIZE - length;
		UCHAR* endPtr = &tdgbl->gbl_crypt_buffer[length];

		// If IO buffer empty, reload it
		if (tdgbl->blk_io_cnt <= 0)
		{
			*endPtr++ = mvol_read(&tdgbl->blk_io_cnt, &tdgbl->blk_io_ptr);
			--count;
			++length;
		}

		// Copy data from the IO buffer
		const ULONG n = MIN(count, (ULONG) tdgbl->blk_io_cnt);
		memcpy(endPtr, tdgbl->blk_io_ptr, n);
		endPtr += n;
		length += n;

		// Move IO pointer
		tdgbl->blk_io_cnt -= n;
		tdgbl->blk_io_ptr += n;
	}

	UCHAR* ptr = tdgbl->gbl_crypt_buffer;
	buffer_length = MIN(length, buffer_length);

	if (!tdgbl->gbl_key_hash[0])
	{
		memcpy(buffer, ptr, buffer_length);
	}
	else
	{
		start_crypt(tdgbl);
		fb_assert(tdgbl->gbl_crypt);

		ULONG left = buffer_length % CRYPT_STEP;
		buffer_length -= left;

		FbLocalStatus status;

		for (ULONG offset = 0; offset < buffer_length; offset += CRYPT_STEP)
		{
			tdgbl->gbl_crypt->crypt_plugin->decrypt(&status, CRYPT_STEP, &ptr[offset], &buffer[offset]);
			check(&status);
		}
	}

	// Some data may stay in gbl_crypt_buffer for next time
	length -= buffer_length;
	memmove(ptr, ptr + buffer_length, length);

	return buffer_length;
}


//____________________________________________________________
//
//

static ULONG unzip_read_block(BurpGlobals* tdgbl, UCHAR* buffer, FB_SIZE_T buffer_length)
{
	if (!tdgbl->gbl_sw_zip)
	{
		return crypt_read_block(tdgbl, buffer, buffer_length);
	}

#ifdef HAVE_ZLIB_H
	z_stream& strm = tdgbl->gbl_stream;
	strm.avail_out = buffer_length;
	strm.next_out = buffer;

	for (;;)
	{
		if (strm.avail_in)
		{
#ifdef COMPRESS_DEBUG
			fprintf(stderr, "Data to inflate %d port %p\n", strm.avail_in, port);
#if COMPRESS_DEBUG > 1
			for (unsigned n = 0; n < strm.avail_in; ++n) fprintf(stderr, "%02x ", strm.next_in[n]);
			fprintf(stderr, "\n");
#endif
#endif
			ULONG wasAvail = strm.avail_out;
			int ret = zlib().inflate(&strm, Z_NO_FLUSH);
			if (ret == Z_DATA_ERROR && wasAvail != strm.avail_out)
				ret = Z_OK;

			if (ret != Z_OK)
			{
#ifdef COMPRESS_DEBUG
				fprintf(stderr, "Inflate error %d\n", ret);
#endif
				BURP_error(379, true, SafeArg() << ret);
			}
#ifdef COMPRESS_DEBUG
			fprintf(stderr, "Inflated data %d\n", buffer_length - strm.avail_out);
#if COMPRESS_DEBUG > 1
			for (unsigned n = 0; n < buffer_length - strm.avail_out; ++n) fprintf(stderr, "%02x ", buffer[n]);
			fprintf(stderr, "\n");
#endif
#endif
			if (strm.next_out != buffer)
				break;

			UCHAR* compressed = tdgbl->gbl_decompress;
			if (strm.next_in != compressed)
			{
				memmove(compressed, strm.next_in, strm.avail_in);
				strm.next_in = compressed;
			}
		}
		else
			strm.next_in = tdgbl->gbl_decompress;

		ULONG l = ZC_BUFSIZE - strm.avail_in;
		l = crypt_read_block(tdgbl, strm.next_in, l);
		strm.avail_in += l;
	}

	return buffer_length - strm.avail_out;
#else
	(Firebird::Arg::Gds(isc_random) << "No inflate support").raise();
#endif
}

static void zip_write_block(BurpGlobals* tdgbl, const UCHAR* buffer, FB_SIZE_T buffer_length, bool flash)
{
	if (!tdgbl->gbl_sw_zip)
	{
		crypt_write_block(tdgbl, buffer, buffer_length, flash);
		return;
	}

#ifdef HAVE_ZLIB_H
	z_stream& strm = tdgbl->gbl_stream;
	strm.avail_in = buffer_length;
	strm.next_in = (Bytef*)buffer;
	UCHAR* compressed = tdgbl->gbl_decompress;

	if (!strm.next_out)
	{
		strm.avail_out = ZC_BUFSIZE;
		strm.next_out = (Bytef*)compressed;
	}

	bool expectMoreOut = flash;

	while (strm.avail_in || expectMoreOut)
	{
#ifdef COMPRESS_DEBUG
		fprintf(stderr, "Data to deflate %d port %p\n", strm.avail_in, port);
#if COMPRESS_DEBUG>1
		for (unsigned n = 0; n < strm.avail_in; ++n) fprintf(stderr, "%02x ", strm.next_in[n]);
		fprintf(stderr, "\n");
#endif
#endif
		int ret = zlib().deflate(&strm, flash ? Z_FULL_FLUSH : Z_NO_FLUSH);
		if (ret == Z_BUF_ERROR)
			ret = 0;
		if (ret != 0)
		{
#ifdef COMPRESS_DEBUG
			fprintf(stderr, "Deflate error %d\n", ret);
#endif
			BURP_error(380, true, SafeArg() << ret);
		}

#ifdef COMPRESS_DEBUG
		fprintf(stderr, "Deflated data %d\n", ZC_BUFSIZE - strm.avail_out);
#if COMPRESS_DEBUG>1
		for (unsigned n = 0; n < port->port_buff_size - strm.avail_out; ++n)
			fprintf(stderr, "%02x ", compressed[n]);
		fprintf(stderr, "\n");
#endif
#endif

		expectMoreOut = !strm.avail_out;
		if ((ZC_BUFSIZE != strm.avail_out) && (flash || !strm.avail_out))
		{
			crypt_write_block(tdgbl, compressed, ZC_BUFSIZE - strm.avail_out, flash);

			strm.avail_out = ZC_BUFSIZE;
			strm.next_out = (Bytef*)compressed;
		}
	}
#else
	(Firebird::Arg::Gds(isc_random) << "No deflate support").raise();
#endif
}



//____________________________________________________________
//
//
FB_UINT64 MVOL_fini_read()
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

#ifdef HAVE_ZLIB_H
	if (tdgbl->gbl_sw_zip)
	{
		zlib().inflateEnd(&tdgbl->gbl_stream);
	}
#endif

	brio_fini(tdgbl);

	return mvol_fini_read(tdgbl);
}

FB_UINT64 mvol_fini_read(BurpGlobals* tdgbl)
{
	if (!tdgbl->stdIoMode)
	{
		close_platf(tdgbl->file_desc);
	}

	for (burp_fil* file = tdgbl->gbl_sw_backup_files; file; file = file->fil_next)
	{
		if (file->fil_fd == tdgbl->file_desc)
		{
			file->fil_fd = INVALID_HANDLE_VALUE;
		}
	}

	tdgbl->file_desc = INVALID_HANDLE_VALUE;
	BURP_free(tdgbl->mvol_io_buffer);
	tdgbl->mvol_io_buffer = NULL;
	tdgbl->blk_io_cnt = 0;
	tdgbl->blk_io_ptr = NULL;
	return tdgbl->mvol_cumul_count;
}


//____________________________________________________________
//
//
FB_UINT64 MVOL_fini_write()
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	zip_write_block(tdgbl, tdgbl->gbl_compress_buffer, tdgbl->gbl_io_ptr - tdgbl->gbl_compress_buffer, true);

#ifdef HAVE_ZLIB_H
	if (tdgbl->gbl_sw_zip)
	{
		zlib().deflateEnd(&tdgbl->gbl_stream);
	}
#endif

	brio_fini(tdgbl);

	return mvol_fini_write(tdgbl, &tdgbl->blk_io_cnt, &tdgbl->blk_io_ptr);
}

FB_UINT64 mvol_fini_write(BurpGlobals* tdgbl, int* io_cnt, UCHAR** io_ptr)
{
	mvol_write(rec_end, io_cnt, io_ptr);
	flush_platf(tdgbl->file_desc);

	if (!tdgbl->stdIoMode)
	{
		close_platf(tdgbl->file_desc);
	}
	for (burp_fil* file = tdgbl->gbl_sw_backup_files; file; file = file->fil_next)
	{
		if (file->fil_fd == tdgbl->file_desc)
		{
			file->fil_fd = INVALID_HANDLE_VALUE;
		}
	}

	tdgbl->file_desc = INVALID_HANDLE_VALUE;
	BURP_free(tdgbl->mvol_io_memory);
	tdgbl->mvol_io_memory = NULL;
	tdgbl->mvol_io_header = NULL;
	tdgbl->mvol_io_buffer = NULL;
	tdgbl->blk_io_cnt = 0;
	tdgbl->blk_io_ptr = NULL;

	return tdgbl->mvol_cumul_count;
}


//____________________________________________________________
//
//
void MVOL_init(ULONG io_buf_size)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	tdgbl->mvol_io_buffer_size = io_buf_size;

	tdgbl->gbl_compress_buffer = FB_NEW_POOL(tdgbl->getPool()) UCHAR[ZC_BUFSIZE];
	tdgbl->gbl_crypt_buffer = FB_NEW_POOL(tdgbl->getPool()) UCHAR[ZC_BUFSIZE];
	tdgbl->gbl_decompress = FB_NEW_POOL(tdgbl->getPool()) UCHAR[ZC_BUFSIZE];
}


static void brio_fini(BurpGlobals* tdgbl)
{
	delete[] tdgbl->gbl_compress_buffer;
	tdgbl->gbl_compress_buffer = NULL;

	delete[] tdgbl->gbl_crypt_buffer;
	tdgbl->gbl_crypt_buffer = NULL;

	delete[] tdgbl->gbl_decompress;
	tdgbl->gbl_decompress = NULL;;
}


static void checkCompression()
{
#ifdef HAVE_ZLIB_H
	if (!zlib())
	{
		(Firebird::Arg::Gds(isc_random) << "Compession support library not loaded" <<
		 Firebird::Arg::StatusVector(zlib().status)).raise();
	}
#endif
}


//____________________________________________________________
//
// Read init record from backup file
//
void MVOL_init_read(const char* file_name, USHORT* format)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	mvol_init_read(tdgbl, file_name, format, &tdgbl->blk_io_cnt, &tdgbl->blk_io_ptr);

	tdgbl->gbl_io_cnt = 0;
	tdgbl->gbl_io_ptr = NULL;

	if (tdgbl->gbl_sw_zip)
	{
#ifdef HAVE_ZLIB_H
		z_stream& strm = tdgbl->gbl_stream;

		strm.zalloc = Firebird::ZLib::allocFunc;
		strm.zfree = Firebird::ZLib::freeFunc;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		checkCompression();
		int ret = zlib().inflateInit(&strm);
		if (ret != Z_OK)
#endif
			BURP_error(383, true, SafeArg() << 127);
	}
}

void mvol_init_read(BurpGlobals* tdgbl, const char* file_name, USHORT* format, int* cnt, UCHAR** ptr)
{
	tdgbl->mvol_volume_count = 1;
	tdgbl->mvol_empty_file = true;

	if (file_name != NULL)
	{
		strncpy(tdgbl->mvol_old_file, file_name, MAX_FILE_NAME_SIZE);
		tdgbl->mvol_old_file[MAX_FILE_NAME_SIZE - 1] = 0;
	}
	else
	{
		tdgbl->mvol_old_file[0] = 0;
	}

	ULONG temp_buffer_size = tdgbl->mvol_io_buffer_size;
	tdgbl->mvol_actual_buffer_size = temp_buffer_size;
	tdgbl->mvol_io_buffer = BURP_alloc(temp_buffer_size);
	tdgbl->gbl_backup_start_time[0] = 0;

	read_header(tdgbl->file_desc, &temp_buffer_size, format, true);

	if (temp_buffer_size > tdgbl->mvol_actual_buffer_size)
	{
		UCHAR* new_buffer = BURP_alloc(temp_buffer_size);
		memcpy(new_buffer, tdgbl->mvol_io_buffer, tdgbl->mvol_io_buffer_size);
		BURP_free(tdgbl->mvol_io_buffer);
		tdgbl->mvol_io_ptr = new_buffer + (tdgbl->mvol_io_ptr - tdgbl->mvol_io_buffer);
		tdgbl->mvol_io_buffer = new_buffer;
	}
	else
	{
		temp_buffer_size = (tdgbl->mvol_actual_buffer_size / temp_buffer_size) * temp_buffer_size;
	}

	tdgbl->mvol_actual_buffer_size = tdgbl->mvol_io_buffer_size = temp_buffer_size;
	*cnt = tdgbl->mvol_io_cnt;
	*ptr = tdgbl->mvol_io_ptr;
}


//____________________________________________________________
//
// Write init record to the backup file
//
void MVOL_init_write(const char* file_name)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	mvol_init_write(tdgbl, file_name, &tdgbl->blk_io_cnt, &tdgbl->blk_io_ptr);

	tdgbl->gbl_io_cnt = ZC_BUFSIZE;
	tdgbl->gbl_io_ptr = tdgbl->gbl_compress_buffer;

#ifdef HAVE_ZLIB_H
	if (tdgbl->gbl_sw_zip)
	{
		z_stream& strm = tdgbl->gbl_stream;

		strm.zalloc = Firebird::ZLib::allocFunc;
		strm.zfree = Firebird::ZLib::freeFunc;
		strm.opaque = Z_NULL;
		checkCompression();
		int ret = zlib().deflateInit(&strm, Z_DEFAULT_COMPRESSION);
		if (ret != Z_OK)
			BURP_error(384, true, SafeArg() << ret);
		strm.next_out = Z_NULL;
	}
#endif
}

void mvol_init_write(BurpGlobals* tdgbl, const char* file_name, int* cnt, UCHAR** ptr)
{
	tdgbl->mvol_volume_count = 1;
	tdgbl->mvol_empty_file = true;

	if (file_name != NULL)
	{
		strncpy(tdgbl->mvol_old_file, file_name, MAX_FILE_NAME_SIZE);
		tdgbl->mvol_old_file[MAX_FILE_NAME_SIZE - 1] = 0;
	}
	else
	{
		tdgbl->mvol_old_file[0] = 0;
	}

	tdgbl->mvol_actual_buffer_size = tdgbl->mvol_io_buffer_size;
	const ULONG temp_buffer_size = tdgbl->mvol_io_buffer_size * tdgbl->gbl_sw_blk_factor;
	tdgbl->mvol_io_memory = BURP_alloc(temp_buffer_size + MAX_HEADER_SIZE * 2);
	tdgbl->mvol_io_ptr = tdgbl->mvol_io_buffer =
		(UCHAR*) FB_ALIGN((U_IPTR) tdgbl->mvol_io_memory, MAX_HEADER_SIZE);
	tdgbl->mvol_io_cnt = tdgbl->mvol_actual_buffer_size;

	while (!write_header(tdgbl->file_desc, temp_buffer_size, false))
	{
		if (tdgbl->action->act_action == ACT_backup_split)
		{
			BURP_error(269, true, tdgbl->action->act_file->fil_name.c_str());
			// msg 269 can't write a header record to file %s
		}
		tdgbl->file_desc = next_volume(tdgbl->file_desc, MODE_WRITE, false);
	}

	tdgbl->mvol_actual_buffer_size = temp_buffer_size;

	*cnt = tdgbl->mvol_io_cnt;
	*ptr = tdgbl->mvol_io_ptr;
}


//____________________________________________________________
//
// Read a buffer's worth of data. (common)
//
void MVOL_read(BurpGlobals* tdgbl)
{
	// Setup our pointer
	if (!tdgbl->master)
	{
		// hvlad: it will throw ExcReadDone exception when there is nothing to read
		RestoreRelationTask::renewBuffer(tdgbl);
		tdgbl->mvol_io_ptr = tdgbl->mvol_io_buffer;
		return;
	}

	tdgbl->gbl_io_ptr = tdgbl->gbl_compress_buffer;
	tdgbl->gbl_io_cnt = unzip_read_block(tdgbl, tdgbl->gbl_io_ptr, ZC_BUFSIZE);
}

int mvol_read(int* cnt, UCHAR** ptr)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	if (tdgbl->stdIoMode && tdgbl->uSvc->isService())
	{
		tdgbl->uSvc->started();
		tdgbl->mvol_io_cnt = tdgbl->uSvc->getBytes(tdgbl->mvol_io_buffer, tdgbl->mvol_io_buffer_size);
		if (!tdgbl->mvol_io_cnt)
		{
			BURP_error_redirect(0, 220);
			// msg 220 Unexpected I/O error while reading from backup file
		}
		tdgbl->mvol_io_ptr = tdgbl->mvol_io_buffer;
	}
	else
	{
		os_read(cnt, ptr);
	}

	tdgbl->mvol_cumul_count += tdgbl->mvol_io_cnt;
	file_not_empty();

	if (ptr)
		*ptr = tdgbl->mvol_io_ptr + 1;
	if (cnt)
		*cnt = tdgbl->mvol_io_cnt - 1;

	return *tdgbl->mvol_io_ptr;
}


#ifndef WIN_NT
//____________________________________________________________
//
// Read a buffer's worth of data. (non-WIN_NT)
//
static void os_read(int* cnt, UCHAR** ptr)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	fb_assert(tdgbl->master);

	for (;;)
	{
		tdgbl->mvol_io_cnt = read(tdgbl->file_desc, tdgbl->mvol_io_buffer, tdgbl->mvol_io_buffer_size);
		tdgbl->mvol_io_ptr = tdgbl->mvol_io_buffer;

		if (tdgbl->mvol_io_cnt > 0) {
			break;
		}

		if (!tdgbl->mvol_io_cnt || errno == EIO)
		{
			tdgbl->file_desc = next_volume(tdgbl->file_desc, MODE_READ, false);
			if (tdgbl->mvol_io_cnt > 0)
			{
				break;
			}
		}
		else if (!SYSCALL_INTERRUPTED(errno))
		{
			if (cnt)
			{
				BURP_error_redirect(0, 220);
				// msg 220 Unexpected I/O error while reading from backup file
			}
			else
			{
				BURP_error_redirect(0, 50);
				// msg 50 unexpected end of file on backup file
			}
		}
	}
}


#else
//____________________________________________________________
//
// Read a buffer's worth of data. (WIN_NT)
//
static void os_read(int* cnt, UCHAR** ptr)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	fb_assert(tdgbl->master);
	fb_assert(tdgbl->blk_io_cnt <= 0);

	for (;;)
	{
		DWORD bytesRead = 0;
		const BOOL ret = ReadFile(tdgbl->file_desc, tdgbl->mvol_io_buffer,
								  tdgbl->mvol_io_buffer_size, &bytesRead, NULL);
		tdgbl->mvol_io_cnt = bytesRead;
		tdgbl->mvol_io_ptr = tdgbl->mvol_io_buffer;
		if (tdgbl->mvol_io_cnt > 0)
			break;

		if (!tdgbl->mvol_io_cnt && ret)
		{
			tdgbl->file_desc = next_volume(tdgbl->file_desc, MODE_READ, false);
			if (tdgbl->mvol_io_cnt > 0)
				break;
		}
		else if (GetLastError() != ERROR_HANDLE_EOF)
		{
			if (cnt)
				BURP_error_redirect(NULL, 220);
				// msg 220 Unexpected I/O error while reading from backup file
			else
				BURP_error_redirect(NULL, 50);
				// msg 50 unexpected end of file on backup file
		}
	}
}
#endif // !WIN_NT


//____________________________________________________________
//
// Read a chunk of data from the IO buffer.
// Return a pointer to the first position NOT read into.
//
UCHAR* MVOL_read_block(BurpGlobals* tdgbl, UCHAR* ptr, ULONG count)
{
	while (count)
	{
		// If buffer empty, reload it
		if (tdgbl->gbl_io_cnt <= 0)
			MVOL_read(tdgbl);

		const ULONG n = MIN(count, (ULONG) tdgbl->gbl_io_cnt);

		// Copy data from the IO buffer

		memcpy(ptr, tdgbl->gbl_io_ptr, n);
		ptr += n;

		// Skip ahead in current buffer

		count -= n;
		tdgbl->gbl_io_cnt -= n;
		tdgbl->gbl_io_ptr += n;
	}

	return ptr;
}


//____________________________________________________________
//
// Skip head in the IO buffer.  Often used when only
// doing partial restores.
//
void MVOL_skip_block( BurpGlobals* tdgbl, ULONG count)
{
	// To handle tape drives & Multi-volume boundaries, use the normal
	// read function, instead of doing a more optimal seek.


	while (count)
	{
		// If buffer empty, reload it
		if (tdgbl->gbl_io_cnt <= 0)
		{
			MVOL_read(tdgbl);
		}

		const ULONG n = MIN(count, (ULONG) tdgbl->gbl_io_cnt);

		// Skip ahead in current buffer

		count -= n;
		tdgbl->gbl_io_cnt -= n;
		tdgbl->gbl_io_ptr += n;
	}
}


#ifdef WIN_NT
//____________________________________________________________
//
// detect if it's a tape, rewind if so
// and set the buffer size
//
DESC NT_tape_open(const char* name, ULONG mode, ULONG create)
{
	HANDLE handle;
	TAPE_GET_MEDIA_PARAMETERS param;
	DWORD size = sizeof(param);

	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	const DWORD flags = tdgbl->gbl_sw_direct_io ? FILE_FLAG_NO_BUFFERING : 0;

	if (strnicmp(name, "\\\\.\\tape", 8))
	{
		handle = CreateFile(name, mode,
							mode == MODE_WRITE ? 0 : FILE_SHARE_READ,
							NULL, create, FILE_ATTRIBUTE_NORMAL | flags, NULL);
	}
	else
	{
		// it's a tape device
		// Note: we *want* to open the tape in Read-only mode or in
		// write-only mode, but it turns out that on NT SetTapePosition
		// will fail (thereby not rewinding the tape) if the tape is
		// opened write-only, so we will make sure that we always have
		// read access. So much for standards!
		// Ain't Windows wonderful???
		//
		// Note: we *want* to open the tape in FILE_EXCLUSIVE_WRITE, but
		// during testing discovered that several NT tape drives do not
		// work unless we specify FILE_SHARE_WRITE as the open mode.
		// So it goes...
		//
		handle = CreateFile(name, mode | MODE_READ,
							mode == MODE_WRITE ? FILE_SHARE_WRITE : FILE_SHARE_READ,
							NULL, OPEN_EXISTING, flags, NULL);
		if (handle != INVALID_HANDLE_VALUE)
		{
			// emulate UNIX rewinding the tape on open:
			// This MUST be done since Windows does NOT have anything
			// like mt to allow the user to do tape management. The
			// implication here is that we will be able to write ONLY
			// one (1) database per tape. This is bad if the user wishes to
			// backup several small databases.
			// Note: We are intentionally NOT trapping for errors during
			// rewind, since if we can not rewind, we are either a non-rewind
			// device (then it is user controlled) or we have a problem with
			// the physical media.  In the latter case I would rather wait for
			// the write to fail so that we can loop and prompt the user for
			// a different file/device.
			//
			SetTapePosition(handle, TAPE_REWIND, 0, 0, 0, FALSE);
			if (GetTapeParameters(handle, GET_TAPE_MEDIA_INFORMATION, &size, &param) == NO_ERROR)
			{
				tdgbl->io_buffer_size = param.BlockSize;
			}
		}
	}
	return handle;
}
#endif // WIN_NT


//____________________________________________________________
//
// Write a buffer's worth of data.
//
void MVOL_write(BurpGlobals* tdgbl)
{
	if (!tdgbl->master)
	{
		BackupRelationTask::renewBuffer(tdgbl);
		return;
	}

	fb_assert(tdgbl->gbl_io_ptr >= tdgbl->gbl_compress_buffer);
	fb_assert(tdgbl->gbl_io_ptr <= tdgbl->gbl_compress_buffer + ZC_BUFSIZE);

	zip_write_block(tdgbl, tdgbl->gbl_compress_buffer, tdgbl->gbl_io_ptr - tdgbl->gbl_compress_buffer, false);

	tdgbl->gbl_io_cnt = ZC_BUFSIZE;
	tdgbl->gbl_io_ptr = tdgbl->gbl_compress_buffer;
}

UCHAR mvol_write(const UCHAR c, int* io_cnt, UCHAR** io_ptr)
{
	const UCHAR* ptr;
	ULONG cnt = 0;

	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	if (!tdgbl->master)
	{
		BackupRelationTask::renewBuffer(tdgbl);
		*(*io_ptr)++ = c;
		(*io_cnt)--;
		return c;
	}

	const ULONG size_to_write = BURP_UP_TO_BLOCK(*io_ptr - tdgbl->mvol_io_buffer);
	FB_UINT64 left = size_to_write;

	if (tdgbl->stdIoMode && tdgbl->uSvc->isService())
	{
		tdgbl->uSvc->started();
		tdgbl->uSvc->putBytes(tdgbl->mvol_io_buffer, left);
		left = 0;
	}
	else
	{
		for (ptr = tdgbl->mvol_io_buffer; left > 0; ptr += cnt, left -= cnt)
		{
			if (tdgbl->action->act_action == ACT_backup_split)
			{
				// Write to the current file till fil_lingth > 0, otherwise
				// switch to the next one
				if (tdgbl->action->act_file->fil_length == 0)
				{
					if (tdgbl->action->act_file->fil_next)
					{
						close_platf(tdgbl->file_desc);
						for (burp_fil* file = tdgbl->gbl_sw_backup_files; file; file = file->fil_next)
						{
							if (file->fil_fd == tdgbl->file_desc)
								file->fil_fd = INVALID_HANDLE_VALUE;
						}
						tdgbl->action->act_file->fil_fd = INVALID_HANDLE_VALUE;
						tdgbl->action->act_file = tdgbl->action->act_file->fil_next;
						tdgbl->file_desc = tdgbl->action->act_file->fil_fd;
					}
					else
					{
						// This is a last file. Keep writing in a hope that there is
						// enough free disk space ...
						tdgbl->action->act_file->fil_length = MAX_LENGTH;
					}
				}
			}

			FB_UINT64 longBytesToWrite =
				(tdgbl->action->act_action == ACT_backup_split &&
						tdgbl->action->act_file->fil_length < left) ?
			 			tdgbl->action->act_file->fil_length : left;

			if (longBytesToWrite > MAX_ULONG)
			{
				longBytesToWrite = 0xFFFF0000u;		// rounding such block to 64K appears OK
			}

			const size_t nBytesToWrite = size_t(longBytesToWrite);

			bool error = false;
			bool disk_full = false;
			cnt = 0;
#ifndef WIN_NT
			ssize_t ret = write(tdgbl->file_desc, ptr, nBytesToWrite);

			if (ret == -1)
			{
				error = true;

				if (errno == ENOSPC || errno == EIO || errno == ENXIO || errno == EFBIG)
					disk_full = true;
			}
			else
				cnt = ret;
#else
			if (!WriteFile(tdgbl->file_desc, ptr, (DWORD) nBytesToWrite, &cnt, NULL))
			{
				error = true;
				DWORD ret = GetLastError();

				if (ret == ERROR_DISK_FULL || ret == ERROR_HANDLE_DISK_FULL)
					disk_full = true;
			}
#endif // !WIN_NT
			tdgbl->mvol_io_buffer = tdgbl->mvol_io_data;
			if (!error)
			{
				tdgbl->mvol_cumul_count += cnt;
				file_not_empty();
				if (tdgbl->action->act_action == ACT_backup_split)
				{
					if (tdgbl->action->act_file->fil_length < left)
						tdgbl->action->act_file->fil_length = 0;
					else
						tdgbl->action->act_file->fil_length -= left;
				}
			}
			else
			{
				if (disk_full)
				{
					if (tdgbl->action->act_action == ACT_backup_split)
					{
						// Close the current file and switch to the next one.
						// If there is no more specified files left then
						// issue an error and give up
						if (tdgbl->action->act_file->fil_next)
						{
							close_platf(tdgbl->file_desc);
							for (burp_fil* file = tdgbl->gbl_sw_backup_files; file; file = file->fil_next)
							{
								if (file->fil_fd == tdgbl->file_desc)
									file->fil_fd = INVALID_HANDLE_VALUE;
							}

							tdgbl->action->act_file->fil_fd = INVALID_HANDLE_VALUE;
							BURP_print(true, 272, SafeArg() <<
										tdgbl->action->act_file->fil_name.c_str() <<
										tdgbl->action->act_file->fil_length <<
										tdgbl->action->act_file->fil_next->fil_name.c_str());
							// msg 272 Warning -- free disk space exhausted for file %s,
							// the rest of the bytes (%d) will be written to file %s
							tdgbl->action->act_file->fil_next->fil_length +=
								tdgbl->action->act_file->fil_length;
							tdgbl->action->act_file = tdgbl->action->act_file->fil_next;
							tdgbl->file_desc = tdgbl->action->act_file->fil_fd;
						}
						else
						{
							BURP_error(270, true);
							// msg 270 free disk space exhausted
						}
						cnt = 0;
						continue;
					}

					if (tdgbl->uSvc->isService())
					{
						BURP_error(270, true);
						// msg 270 free disk space exhausted
					}

					// Note: there is an assumption here, that if header data is being
					// written, it is really being rewritten, so at least all the
					// header data will be written

					if (left != size_to_write)
					{
						// Wrote some, move remainder up in buffer.

						// NOTE: We should NOT use memcpy here.  We're moving overlapped
						// data and memcpy does not guanantee the order the data
						// is moved in
						memcpy(tdgbl->mvol_io_data, ptr, left);
					}
					left += tdgbl->mvol_io_data - tdgbl->mvol_io_header;
					bool full_buffer;
					if (left >=  tdgbl->mvol_io_buffer_size)
						full_buffer = true;
					else
						full_buffer = false;
					tdgbl->file_desc = next_volume(tdgbl->file_desc, MODE_WRITE, full_buffer);
					if (full_buffer)
					{
						left -= tdgbl->mvol_io_buffer_size;
						memcpy(tdgbl->mvol_io_data,
							   tdgbl->mvol_io_header + tdgbl->mvol_io_buffer_size,
							   left);
						tdgbl->mvol_cumul_count += tdgbl->mvol_io_buffer_size;
						tdgbl->mvol_io_buffer = tdgbl->mvol_io_data;
					}
					else
						tdgbl->mvol_io_buffer = tdgbl->mvol_io_header;
					break;
				}
				else if (!SYSCALL_INTERRUPTED(errno))
				{
					BURP_error_redirect(0, 221);
					// msg 221 Unexpected I/O error while writing to backup file
				}
			}
			if (left < cnt) {	// this is impossible, but...
				cnt = left;
			}

		} // for

#ifdef DEBUG
		{
			int dbg_cnt;
			if (debug_on)
			{
				for (dbg_cnt = 0; dbg_cnt < cnt; dbg_cnt++)
					printf("%d,\n", *(ptr + dbg_cnt));
			}
		}
#endif
	}

	// After the first block of first volume is written (using a default block size)
	// change the block size to one that reflects the user's blocking factor.  By
	// making the first block a standard size we will avoid restore problems.

	tdgbl->mvol_io_buffer_size = tdgbl->mvol_actual_buffer_size;

	UCHAR* newptr = tdgbl->mvol_io_buffer + left;
	*newptr++ = c;
	*io_ptr = newptr;
	*io_cnt = tdgbl->mvol_io_buffer_size - 1 - left;

	return c;
}


//____________________________________________________________
//
// Write a chunk of data to the IO buffer.
// Return a pointer to the first position NOT written from.
//
const UCHAR* MVOL_write_block(BurpGlobals* tdgbl, const UCHAR* ptr, ULONG count)
{
	while (count)
	{
		// If buffer full, write it
		if (tdgbl->gbl_io_cnt <= 0)
		{
			if (!tdgbl->master)
			{
				BackupRelationTask::renewBuffer(tdgbl);
			}
			else
			{
				zip_write_block(tdgbl, tdgbl->gbl_compress_buffer, tdgbl->gbl_io_ptr - tdgbl->gbl_compress_buffer, false);

				tdgbl->gbl_io_ptr = tdgbl->gbl_compress_buffer;
				tdgbl->gbl_io_cnt = ZC_BUFSIZE;
			}
		}

		const ULONG n = MIN(count, (ULONG) tdgbl->gbl_io_cnt);

		// Copy data to the IO buffer
		memcpy(tdgbl->gbl_io_ptr, ptr, n);
		ptr += n;
		count -= n;

		// Skip ahead in current buffer
		tdgbl->gbl_io_cnt -= n;
		tdgbl->gbl_io_ptr += n;
	}

	return ptr;
}

const UCHAR* mvol_write_block(BurpGlobals* tdgbl, const UCHAR* ptr, ULONG count)
{
	// To handle tape drives & Multi-volume boundaries, use the normal
	// write function, instead of doing a more optimal bulk write.

	while (count)
	{
		// If buffer full, dump it
		if (tdgbl->blk_io_cnt <= 0)
		{
			mvol_write(*ptr++, &tdgbl->blk_io_cnt, &tdgbl->blk_io_ptr);

			// One byte was written by mvol_write
			count--;
		}

		const ULONG n = MIN(count, (ULONG) tdgbl->blk_io_cnt);

		// Copy data to the IO buffer

		memcpy(tdgbl->blk_io_ptr, ptr, n);
		ptr += n;

		// Skip ahead in current buffer

		count -= n;
		tdgbl->blk_io_cnt -= n;
		tdgbl->blk_io_ptr += n;

	}
	return ptr;
}


//____________________________________________________________
//
// We ran into an unsupported attribute.  This shouldn't happen,
// but it isn't the end of the world.
//
static void bad_attribute(int attribute, USHORT type)
{
	TEXT name[128];

	BurpGlobals* tdgbl = BurpGlobals::getSpecific();
	static const SafeArg dummy;
	fb_msg_format(NULL, burp_msg_fac, type, sizeof(name), name, dummy);
	BURP_print(true, 80, SafeArg() << name << attribute);
	// msg 80  don't recognize %s attribute %ld -- continuing
	for (int l = get(tdgbl); l; --l)
		get(tdgbl);
}


//____________________________________________________________
//
//
static void file_not_empty()
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	tdgbl->mvol_empty_file = false;
}


//____________________________________________________________
//
// Get a numeric value from the input stream.
//
static SLONG get_numeric()
{
	SLONG value[2];

	SSHORT length = get_text((UCHAR*) value, sizeof(value));

	return gds__vax_integer(reinterpret_cast<const UCHAR*>(value), length);
}


//____________________________________________________________
//
// Move a text attribute to a string and fill.
//
static int get_text(UCHAR* text, SSHORT length)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	ULONG len = get(tdgbl);
	length -= len;
	const ULONG len2 = len;

	if (length < 0)
	{
		BURP_error_redirect(0, 46);	// msg 46 string truncated
	}

	if (len)
	{
		do {
			*text++ = get(tdgbl);
		} while (--len);
	}

	*text = 0;

	return len2;
}


//____________________________________________________________
//
// Get specification for the next volume (tape).
// Try to open it. Return file descriptor.
//
static DESC next_volume( DESC handle, ULONG mode, bool full_buffer)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	// We must close the old handle before the user inserts
	// another tape, or something.

#ifdef WIN_NT
	if (handle != INVALID_HANDLE_VALUE)
#else
	if (handle > -1)
#endif // WIN_NT
	{
		close_platf(handle);
	}

	if (tdgbl->action->act_action == ACT_restore_join)
	{
		tdgbl->action->act_file->fil_fd = INVALID_HANDLE_VALUE;
		if ((tdgbl->action->act_total > tdgbl->action->act_file->fil_seq) &&
			(tdgbl->action->act_file = tdgbl->action->act_file->fil_next) &&
			(tdgbl->action->act_file->fil_fd != INVALID_HANDLE_VALUE))
		{
			return tdgbl->action->act_file->fil_fd;
		}

		BURP_error_redirect(0, 50);	// msg 50 unexpected end of file on backup file
	}

	// If we got here, we've got a live one... Up the volume number unless
	// the old file was empty

	if (!tdgbl->mvol_empty_file)
		++tdgbl->mvol_volume_count;

	tdgbl->mvol_empty_file = true;

	// Loop until we have opened a file successfully

	SCHAR new_file[MAX_FILE_NAME_SIZE];
	DESC new_desc = INVALID_HANDLE_VALUE;
	for (;;)
	{
		// We aim to keep our descriptors clean

		if (new_desc != INVALID_HANDLE_VALUE)
		{
			close_platf(new_desc);
			new_desc = INVALID_HANDLE_VALUE;
		}

		// Get file name to try

		prompt_for_name(new_file, sizeof(new_file));

#ifdef WIN_NT
		new_desc = NT_tape_open(new_file, mode, OPEN_ALWAYS);
		if (new_desc == INVALID_HANDLE_VALUE)
#else
		ULONG mode2 = mode;
		if (mode == MODE_WRITE && tdgbl->gbl_sw_direct_io)
			mode2 |= O_DIRECT;

		new_desc = open(new_file, mode2, open_mask);
		if (new_desc < 0)
#endif // WIN_NT
		{
			BURP_print(true, 222, new_file);
			// msg 222 \n\nCould not open file name \"%s\"\n
			continue;
		}

		// If the file is to be writable, probe it, and make sure it is...

#ifdef WIN_NT
		if (mode == MODE_WRITE)
#else
		if ((O_WRONLY == (mode & O_WRONLY)) || (O_RDWR == (mode & O_RDWR)))
#endif // WIN_NT
		{
			if (!write_header(new_desc, 0L, full_buffer))
			{
				BURP_print(true, 223, new_file);
				// msg223 \n\nCould not write to file \"%s\"\n
				continue;
			}
			else
			{
				BURP_msg_put(false, 261, SafeArg() << tdgbl->mvol_volume_count << new_file);
				// Starting with volume #vol_count, new_file
				BURP_verbose(75, new_file);	// msg 75  creating file %s
			}
		}
		else
		{
			// File is open for read only.  Read the header.

			ULONG temp_buffer_size;
			USHORT format;
			if (!read_header(new_desc, &temp_buffer_size, &format, false))
			{
				BURP_print(true, 224, new_file);
				continue;
			}
			else
			{
				BURP_msg_put(false, 261, SafeArg() << tdgbl->mvol_volume_count << new_file);
				// Starting with volume #vol_count, new_file
				BURP_verbose(100, new_file);	// msg 100  opened file %s
			}
		}

		strcpy(tdgbl->mvol_old_file, new_file);
		return new_desc;
	}
}


//____________________________________________________________
//
//
static void prompt_for_name(SCHAR* name, int length)
{
	FILE*	term_in = NULL;
	FILE*	term_out =  NULL;
	TEXT	msg[BURP_MSG_GET_SIZE];

	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	// Unless we are operating as a service, stdin can't necessarily be trusted.
	// Get a location to read from.
	fb_assert(!tdgbl->uSvc->isService());

	if (isatty(fileno(stdout)) || !(term_out = os_utils::fopen(TERM_OUTPUT, "w")))
	{
		term_out = stdout;
	}

	if (isatty(fileno(stdin)) || !(term_in = os_utils::fopen(TERM_INPUT, "r")))
	{
		term_in = stdin;
	}

	// Loop until we have a file name to try

	for (;;)
	{
		// If there was an old file name, use that prompt

		if (strlen(tdgbl->mvol_old_file) > 0)
		{
			BURP_msg_get(225, msg, SafeArg() << (tdgbl->mvol_volume_count - 1) << tdgbl->mvol_old_file);
			fprintf(term_out, "%s", msg);
			BURP_msg_get(226, msg);
			// \tPress return to reopen that file, or type a new\n\tname
			// followed by return to open a different file.\n
			fprintf(term_out, "%s", msg);
		}
		else	// First volume
		{
			BURP_msg_get(227, msg);
			// Type a file name to open and hit return
			fprintf(term_out, "%s", msg);
		}
		BURP_msg_get(228, msg);	// "  Name: "
		fprintf(term_out, "%s", msg);

		fflush(term_out);
		if (fgets(name, length, term_in) == NULL)
		{
			BURP_msg_get(229, msg);
			// \n\nERROR: Backup incomplete\n
			fprintf(term_out, "%s", msg);
			BURP_exit_local(FINI_ERROR, tdgbl);
		}

		// If the user typed just a carriage return, they
		// want the old file.  If there isn't one, reprompt

		if (name[0] == '\n')
		{
			if (strlen(tdgbl->mvol_old_file) > 0)
			{
				strcpy(name, tdgbl->mvol_old_file);
				break;
			}

			continue; // reprompt
		}

		// OK, its a file name, strip the carriage return

		SCHAR* name_ptr = name;
		while (*name_ptr && *name_ptr != '\n')
		{
			name_ptr++;
		}
		*name_ptr = 0;
		break;
	}

	if (term_out != stdout) {
		fclose(term_out);
	}
	if (term_in != stdin) {
		fclose(term_in);
	}
}


//____________________________________________________________
//
// Write an attribute starting with a null terminated string.
//
static void put_asciz( SCHAR attribute, const TEXT* str)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	USHORT l = strlen(str);
	if (l > MAX_UCHAR)
	{
		BURP_print(false, 343, SafeArg() << int(attribute) << "put_asciz()" << USHORT(MAX_UCHAR));
		// msg 343: text for attribute @1 is too large in @2, truncating to @3 bytes
		l = MAX_UCHAR;
	}

	put(tdgbl, attribute);
	put(tdgbl, l);
	while (l--)
		put(tdgbl, *str++);
}


//____________________________________________________________
//
// Write a numeric value as an attribute.  The number is represented
// low byte first, high byte last, as in VAX.
//
static void put_numeric( SCHAR attribute, int value)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	const ULONG vax_value = gds__vax_integer(reinterpret_cast<const UCHAR*>(&value), sizeof(value));
	const UCHAR* p = (UCHAR*) &vax_value;

	put(tdgbl, attribute);
	put(tdgbl, sizeof(vax_value));

	for (USHORT i = 0; i < sizeof(vax_value); i++) {
		put(tdgbl, *p++);
	}
}


//____________________________________________________________
//
// Functional description
//
static bool read_header(DESC handle, ULONG* buffer_size, USHORT* format, bool init_flag)
{
	TEXT buffer[MAX_FILE_NAME_SIZE], msg[BURP_MSG_GET_SIZE];

	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	// Headers are a version number, and a volume number

	if (tdgbl->stdIoMode && tdgbl->uSvc->isService())
	{
		tdgbl->uSvc->started();
		tdgbl->mvol_io_cnt = tdgbl->uSvc->getBytes(tdgbl->mvol_io_buffer, tdgbl->mvol_io_buffer_size);
	}
	else
	{
#ifndef WIN_NT
		tdgbl->mvol_io_cnt = read(handle, tdgbl->mvol_io_buffer, tdgbl->mvol_actual_buffer_size);
#else
		DWORD bytesRead = 0;
		ReadFile(handle, tdgbl->mvol_io_buffer, tdgbl->mvol_actual_buffer_size, &bytesRead, NULL);
		tdgbl->mvol_io_cnt = bytesRead;
#endif
	}
	if (!tdgbl->mvol_io_cnt)
		BURP_error_redirect(0, 45); // maybe there's a better message

	tdgbl->mvol_io_ptr = tdgbl->mvol_io_buffer;

	int attribute = get(tdgbl);
	if (attribute != rec_burp)
		BURP_error_redirect(0, 45);
		// msg 45 expected backup description record

	int l, maxlen;
	int temp;
	TEXT* p;
	for (attribute = get(tdgbl); attribute != att_end; attribute = get(tdgbl))
	{
		switch (attribute)
		{
		case att_backup_blksize:
			{
				const ULONG temp_buffer_size = get_numeric();
				if (init_flag)
					*buffer_size = temp_buffer_size;
			}
			break;

		case att_backup_compress:
			temp = get_numeric();
			if (init_flag)
				tdgbl->gbl_sw_compress = temp != 0;
			break;

		case att_backup_date:
			l = get(tdgbl);
			if (init_flag)
			{
				p = tdgbl->gbl_backup_start_time;
				maxlen = sizeof(tdgbl->gbl_backup_start_time) - 1;
			}
			else
			{
				p = buffer;
				maxlen = sizeof(buffer) - 1;
			}
			if (l)
			{
				do {
					*p++ = get(tdgbl);
				} while (--l && --maxlen);
				// Discard elements that don't fit in the buffer, possible corrupt backup
				if (l > 0 && !maxlen)
				{
					while (l--)
						get(tdgbl);
				}
			}
			*p = 0;
			if (!init_flag && strcmp(buffer, tdgbl->gbl_backup_start_time))
			{
				BURP_msg_get(230, msg, SafeArg() << tdgbl->gbl_backup_start_time << buffer);
				// Expected backup start time %s, found %s\n
				printf("%s", msg);
				return false;
			}
			break;

		case att_backup_file:
			l = get(tdgbl);
			if (init_flag)
			{
				p = tdgbl->mvol_db_name_buffer;
				maxlen = sizeof(tdgbl->mvol_db_name_buffer) - 1;
			}
			else
			{
				p = buffer;
				maxlen = sizeof(buffer) - 1;
			}
			if (l)
			{
				do {
					*p++ = get(tdgbl);
				} while (--l && --maxlen);
				// Discard elements that don't fit in the buffer, possible corrupt backup
				if (l > 0 && !maxlen)
				{
					while (l--)
						get(tdgbl);
				}
			}
			*p = 0;
			if (!init_flag && strcmp(buffer, tdgbl->mvol_db_name_buffer))
			{
				BURP_msg_get(231, msg, SafeArg() << tdgbl->mvol_db_name_buffer << buffer);
				// Expected backup database %s, found %s\n
				printf("%s", msg);
				return false;
			}
			break;

		case att_backup_format:
			temp = get_numeric();
			if (init_flag)
			{
				*format = temp;
			}
			break;

		case att_backup_transportable:
			temp = get_numeric();
			if (init_flag)
			{
				tdgbl->gbl_sw_transportable = temp != 0;
			}
			break;

		case att_backup_volume:
			temp = get_numeric();
			if (temp != tdgbl->mvol_volume_count)
			{
				BURP_msg_get(232, msg, SafeArg() << tdgbl->mvol_volume_count << temp);
				// Expected volume number %d, found volume %d\n
				printf("%s", msg);
				return false;
			}
			break;

		case att_backup_keyname:
			l = get(tdgbl);
			p = tdgbl->mvol_keyname_buffer;
			maxlen = sizeof(tdgbl->mvol_keyname_buffer) - 1;

			if (l)
			{
				do {
					*p++ = get(tdgbl);
				} while (--l && --maxlen);

				// Discard elements that don't fit in the buffer, possible corrupt backup
				if (l > 0 && !maxlen)
				{
					while (l--)
						get(tdgbl);
				}
			}

			*p = 0;
			tdgbl->mvol_keyname = tdgbl->mvol_keyname_buffer;

			if (!tdgbl->gbl_sw_keyname)
				tdgbl->gbl_sw_keyname = tdgbl->mvol_keyname;

			break;

		case att_backup_crypt:
			l = get(tdgbl);
			p = tdgbl->mvol_crypt_buffer;
			maxlen = sizeof(tdgbl->mvol_crypt_buffer) - 1;

			if (l)
			{
				do {
					*p++ = get(tdgbl);
				} while (--l && --maxlen);

				// Discard elements that don't fit in the buffer, possible corrupt backup
				if (l > 0 && !maxlen)
				{
					while (l--)
						get(tdgbl);
				}
			}

			*p = 0;
			tdgbl->mvol_crypt = tdgbl->mvol_crypt_buffer;

			if (!tdgbl->gbl_sw_crypt)
				tdgbl->gbl_sw_crypt = tdgbl->mvol_crypt;

			break;

		case att_backup_zip:
			if (get_numeric())
				tdgbl->gbl_sw_zip = true;
			break;

		case att_backup_hash:
			if (!tdgbl->gbl_sw_keyholder)
				BURP_error(376, true);

			l = get(tdgbl);
			p = tdgbl->gbl_key_hash;
			maxlen = sizeof(tdgbl->gbl_key_hash) - 1;

			if (l)
			{
				do {
					*p++ = get(tdgbl);
				} while (--l && --maxlen);

				// Discard elements that don't fit in the buffer, possible corrupt backup
				if (l > 0 && !maxlen)
				{
					while (l--)
						get(tdgbl);
				}
			}

			*p = 0;
			break;

		default:
			bad_attribute(attribute, 59);	// msg 59 backup
		}
	}

	return true;
}


//____________________________________________________________
//
//
static bool write_header(DESC handle, ULONG backup_buffer_size, bool full_buffer)
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	if (backup_buffer_size)
	{
		tdgbl->mvol_io_header = tdgbl->mvol_io_buffer;

		put(tdgbl, rec_burp);
		put_numeric(att_backup_format, ATT_BACKUP_FORMAT);

		if (tdgbl->gbl_sw_compress)
			put_numeric(att_backup_compress, 1);

		if (tdgbl->gbl_sw_transportable)
			put_numeric(att_backup_transportable, 1);

		if (tdgbl->gbl_sw_zip)
			put_numeric(att_backup_zip, 1);

		put_numeric(att_backup_blksize, backup_buffer_size);

		tdgbl->mvol_io_volume = tdgbl->mvol_io_ptr + 2;
		put_numeric(att_backup_volume, tdgbl->mvol_volume_count);

		if (tdgbl->gbl_sw_keyname)
		{
			tdgbl->mvol_keyname = tdgbl->gbl_sw_keyname;
			put_asciz(att_backup_keyname, tdgbl->mvol_keyname);
		}

		if (tdgbl->gbl_sw_crypt)
		{
			tdgbl->mvol_crypt = tdgbl->gbl_sw_crypt;
			put_asciz(att_backup_crypt, tdgbl->mvol_crypt);
		}

		put_asciz(att_backup_file, tdgbl->gbl_database_file_name);
		put_asciz(att_backup_date, tdgbl->gbl_backup_start_time);

		if (tdgbl->gbl_sw_keyholder)
		{
			start_crypt(tdgbl);
			fb_assert(tdgbl->gbl_crypt && tdgbl->gbl_crypt->crypt_plugin);
			Firebird::string hash;
			calc_hash(hash, tdgbl->gbl_crypt->crypt_plugin);
			put_asciz(att_backup_hash, hash.c_str());
		}

		put(tdgbl, att_end);

		tdgbl->mvol_io_data = (UCHAR*) FB_ALIGN((U_IPTR) tdgbl->mvol_io_ptr, MAX_HEADER_SIZE);
		fb_assert(tdgbl->mvol_io_data == tdgbl->mvol_io_header + MAX_HEADER_SIZE);
	}
	else
	{
		const ULONG vax_value = gds__vax_integer(reinterpret_cast<const UCHAR*>(
											&tdgbl->mvol_volume_count),
											sizeof(tdgbl->mvol_volume_count));
		const UCHAR* p = (UCHAR*) &vax_value;
		UCHAR* q = tdgbl->mvol_io_volume;
		// CVC: Warning, do we want sizeof(int) or sizeof(some_abstract_FB_type)???
		// It seems to me we want sizeof(ULONG) for safety. => Done.
		for (size_t i = 0; i < sizeof(ULONG); i++)
		{
			*q++ = *p++;
		}
	}

	if (full_buffer)
	{
#ifdef WIN_NT
		DWORD bytes_written = 0;
		const bool err = !WriteFile(handle, tdgbl->mvol_io_header,
									tdgbl->mvol_io_buffer_size, &bytes_written, NULL);
#else
		ULONG bytes_written = write(handle, tdgbl->mvol_io_header,
							  tdgbl->mvol_io_buffer_size);
		const bool err = false;
#endif // WIN_NT

		if (err || bytes_written != tdgbl->mvol_io_buffer_size)
		{
			return false;
		}

		if (tdgbl->action->act_action == ACT_backup_split)
		{
			if (tdgbl->action->act_file->fil_length > bytes_written)
			{
				tdgbl->action->act_file->fil_length -= bytes_written;
			}
			else
			{
				tdgbl->action->act_file->fil_length = 0;
			}
		}
		tdgbl->mvol_empty_file = false;
	}

	return true;
}


//____________________________________________________________
//
// Write a header record for split operation
//
bool MVOL_split_hdr_write()
{
	TEXT buffer[HDR_SPLIT_SIZE + 1];

	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	fb_assert(tdgbl->action->act_action == ACT_backup_split);
	fb_assert(tdgbl->action->act_file->fil_fd != INVALID_HANDLE_VALUE);

	if (tdgbl->action->act_file->fil_length < HDR_SPLIT_SIZE) {
		return false;
	}

	time_t seconds = time(NULL);

	Firebird::string nm = tdgbl->toSystem(tdgbl->action->act_file->fil_name);
	sprintf(buffer, "%s%.24s      , file No. %4d of %4d, %-27.27s",
			HDR_SPLIT_TAG, ctime(&seconds), tdgbl->action->act_file->fil_seq,
			tdgbl->action->act_total, nm.c_str());

#ifdef WIN_NT
	DWORD bytes_written = 0;
	WriteFile(tdgbl->action->act_file->fil_fd, buffer, HDR_SPLIT_SIZE, &bytes_written, NULL);
#else
	ULONG bytes_written = write(tdgbl->action->act_file->fil_fd, buffer, HDR_SPLIT_SIZE);
#endif // WIN_NT

	if (bytes_written != HDR_SPLIT_SIZE) {
		return false;
	}

	tdgbl->action->act_file->fil_length -= bytes_written;
	return true;
}


//____________________________________________________________
//
// Read a header record for join operation
//
bool MVOL_split_hdr_read()
{
	BurpGlobals* tdgbl = BurpGlobals::getSpecific();

	fb_assert(tdgbl->action);
	fb_assert(tdgbl->action->act_file);
	fb_assert(tdgbl->action->act_file->fil_fd != INVALID_HANDLE_VALUE);

	if (tdgbl->action && tdgbl->action->act_file &&
		(tdgbl->action->act_file->fil_fd != INVALID_HANDLE_VALUE))
	{
		TEXT buffer[HDR_SPLIT_SIZE];
		int cnt = 0;

#ifdef WIN_NT
		ReadFile(tdgbl->action->act_file->fil_fd, buffer, HDR_SPLIT_SIZE,
					reinterpret_cast<DWORD*>(&cnt), NULL);
#else
		cnt = read(tdgbl->action->act_file->fil_fd, buffer, HDR_SPLIT_SIZE);
#endif
		if ((cnt >= 0) && ((ULONG) cnt == HDR_SPLIT_SIZE) &&
			((strncmp(buffer, HDR_SPLIT_TAG, (sizeof(HDR_SPLIT_TAG) - 1)) == 0) ||
			(strncmp(buffer, HDR_SPLIT_TAG5, (sizeof(HDR_SPLIT_TAG) - 1)) == 0)))
		{
			const hdr_split* hdr = (hdr_split*) buffer;
			if ((tdgbl->action->act_file->fil_seq = atoi(hdr->hdr_split_sequence)) > 0 &&
				(tdgbl->action->act_total = atoi(hdr->hdr_split_total)) > 0 &&
				(tdgbl->action->act_file->fil_seq <= tdgbl->action->act_total))
			{
				return true;
			}
		}
	}

	return false;
}
