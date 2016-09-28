/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		utl.cpp
 *	DESCRIPTION:	User callable routines
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
 * 2001.06.14 Claudio Valderrama: isc_set_path() will append slash if missing,
 *            so ISC_PATH environment variable won't fail for this cause.
 * 2002.02.15 Sean Leyne - Code Cleanup is required of obsolete "EPSON", "XENIX" ports
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "Apollo" port
 * 23-Feb-2002 Dmitry Yemanov - Events wildcarding
 *
 * 2002.10.27 Sean Leyne - Completed removal of obsolete "DG_X86" port
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "UNIXWARE" port
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix" port
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix/MIPS" port
 *
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#include "firebird.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../jrd/license.h"
#include <stdarg.h>
#include "../common/gdsassert.h"

#include "../jrd/ibase.h"
#include "../yvalve/msg.h"
#include "../jrd/event.h"
#include "../yvalve/gds_proto.h"
#include "../yvalve/utl_proto.h"
#include "../yvalve/YObjects.h"
#include "../yvalve/why_proto.h"
#include "../yvalve/prepa_proto.h"
#include "../jrd/constants.h"
#include "../jrd/build_no.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/utils_proto.h"
#include "../common/classes/MetaName.h"
#include "../common/classes/TempFile.h"
#include "../common/classes/DbImplementation.h"
#include "../common/ThreadStart.h"
#include "../common/isc_f_proto.h"
#include "../common/StatusHolder.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/fb_tls.h"
#include "../common/os/os_utils.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#if defined(WIN_NT)
#include <io.h> // mktemp, unlink ..
#include <process.h>
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif


using namespace Firebird;

IAttachment* handleToIAttachment(CheckStatusWrapper*, FB_API_HANDLE*);
ITransaction* handleToITransaction(CheckStatusWrapper*, FB_API_HANDLE*);

// Bug 7119 - BLOB_load will open external file for read in BINARY mode.

#ifdef WIN_NT
static const char* const FOPEN_READ_TYPE		= "rb";
static const char* const FOPEN_WRITE_TYPE		= "wb";
static const char* const FOPEN_READ_TYPE_TEXT	= "rt";
static const char* const FOPEN_WRITE_TYPE_TEXT	= "wt";
#else
static const char* const FOPEN_READ_TYPE		= "r";
static const char* const FOPEN_WRITE_TYPE		= "w";
static const char* const FOPEN_READ_TYPE_TEXT	= FOPEN_READ_TYPE;
static const char* const FOPEN_WRITE_TYPE_TEXT	= FOPEN_WRITE_TYPE;
#endif

#define LOWER7(c) ( (c >= 'A' && c<= 'Z') ? c + 'a' - 'A': c )


// Blob stream stuff

const int BSTR_input	= 0;
const int BSTR_output	= 1;
const int BSTR_alloc	= 2;

static void get_ods_version(CheckStatusWrapper*, IAttachment*, USHORT*, USHORT*);
static void isc_expand_dpb_internal(const UCHAR** dpb, SSHORT* dpb_size, ...);


// Blob info stuff

static const char blob_items[] =
{
	isc_info_blob_max_segment, isc_info_blob_num_segments,
	isc_info_blob_total_length
};


// gds__version stuff

static const unsigned char info[] =
	{ isc_info_firebird_version, isc_info_implementation, fb_info_implementation, isc_info_end };

static const unsigned char ods_info[] =
	{ isc_info_ods_version, isc_info_ods_minor_version, isc_info_end };

static const TEXT* const impl_class[] =
{
	NULL,						// 0
	"access method",			// 1
	"Y-valve",					// 2
	"remote interface",			// 3
	"remote server",			// 4
	NULL,						// 5
	NULL,						// 6
	"pipe interface",			// 7
	"pipe server",				// 8
	"central interface",		// 9
	"central server",			// 10
	"gateway",					// 11
	"classic server",			// 12
	"super server"				// 13
};


namespace {

class VersionCallback : public AutoIface<IVersionCallbackImpl<VersionCallback, CheckStatusWrapper> >
{
public:
	VersionCallback(FPTR_VERSION_CALLBACK routine, void* user)
		: func(routine), arg(user)
	{ }

	// IVersionCallback implementation
	void callback(CheckStatusWrapper*, const char* text)
	{
		func(arg, text);
	}

private:
	FPTR_VERSION_CALLBACK func;
	void* arg;
};

void load(CheckStatusWrapper* status, ISC_QUAD* blobId, IAttachment* att, ITransaction* tra, FILE* file)
{
/**************************************
 *
 *     l o a d
 *
 **************************************
 *
 * Functional description
 *      Load a blob from a file.
 *
 **************************************/
	LocalStatus ls;
	CheckStatusWrapper temp(&ls);

	// Open the blob.  If it failed, what the hell -- just return failure
	IBlob* blob = att->createBlob(status, tra, blobId, 0, NULL);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		return;

	// Copy data from file to blob.  Make up boundaries at end of line.
	TEXT buffer[512];
	TEXT* p = buffer;
	const TEXT* const buffer_end = buffer + sizeof(buffer);

	for (;;)
	{
		const SSHORT c = fgetc(file);
		if (feof(file))
			break;

		*p++ = static_cast<TEXT>(c);

		if (c != '\n' && p < buffer_end)
			continue;

		const SSHORT l = p - buffer;

		blob->putSegment(status, l, buffer);
		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		{
			blob->close(&temp);
			return;
		}

		p = buffer;
	}

	const SSHORT l = p - buffer;
	if (l != 0)
		blob->putSegment(status, l, buffer);

	blob->close(&temp);
	return;
}

void dump(CheckStatusWrapper* status, ISC_QUAD* blobId, IAttachment* att, ITransaction* tra, FILE* file)
{
/**************************************
 *
 *	d u m p
 *
 **************************************
 *
 * Functional description
 *	Dump a blob into a file.
 *
 **************************************/
	// Open the blob.  If it failed, what the hell -- just return failure

	IBlob* blob = att->openBlob(status, tra, blobId, 0, NULL);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		return;

	// Copy data from blob to scratch file

	SCHAR buffer[256];
	const SSHORT short_length = sizeof(buffer);

	for (bool cond = true; cond; )
	{
		unsigned l = 0;
		switch (blob->getSegment(status, short_length, buffer, &l))
		{
		case Firebird::IStatus::RESULT_ERROR:
		case Firebird::IStatus::RESULT_NO_DATA:
			cond = false;
			break;
		}

		if (l)
			FB_UNUSED(fwrite(buffer, 1, l, file));
	}

	// Close the blob

	LocalStatus ls;
	CheckStatusWrapper temp(&ls);
	blob->close(&temp);
}


FB_BOOLEAN edit(CheckStatusWrapper* status, ISC_QUAD* blob_id, IAttachment* att, ITransaction* tra,
	int type, const char* field_name)
{
/**************************************
 *
 *	e d i t
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to a file, allow the user to edit the
 *	window, and dump the data back into a blob.  If the user
 *	bails out, return FALSE, otherwise return TRUE.
 *
 *	If the field name coming in is too big, truncate it.
 *
 **************************************/
#if (defined WIN_NT)
	TEXT buffer[9];
#else
	TEXT buffer[25];
#endif

	const TEXT* q = field_name;
	if (!q)
		q = "gds_edit";

	TEXT* p;
	for (p = buffer; *q && p < buffer + sizeof(buffer) - 1; q++)
	{
		if (*q == '$')
			*p++ = '_';
		else
			*p++ = LOWER7(*q);
	}
	*p = 0;

	// Moved this out of #ifndef mpexl to get mktemp/mkstemp to work for Linux
	// This has been done in the inprise tree some days ago.
	// Would have saved me a lot of time, if I had seen this earlier :-(
	// FSG 15.Oct.2000
	PathName tmpf = TempFile::create(status, buffer);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		return FB_FALSE;

	FILE* file = os_utils::fopen(tmpf.c_str(), FOPEN_WRITE_TYPE_TEXT);
	if (!file)
	{
		unlink(tmpf.c_str());
		system_error::raise("fopen");
	}

	dump(status, blob_id, att, tra, file);

	if (status->getState() & IStatus::STATE_ERRORS)
	{
		isc_print_status(status->getErrors());
		fclose(file);
		unlink(tmpf.c_str());
		return FB_FALSE;
	}

	fclose(file);

	if (gds__edit(tmpf.c_str(), type))
	{

		if (!(file = os_utils::fopen(tmpf.c_str(), FOPEN_READ_TYPE_TEXT)))
		{
			unlink(tmpf.c_str());
			system_error::raise("fopen");
		}

		load(status, blob_id, att, tra, file);

		fclose(file);
		return status->getState() & IStatus::STATE_ERRORS ? FB_FALSE : FB_TRUE;
	}

	unlink(tmpf.c_str());
	return FB_FALSE;
}

} // anonymous namespace


namespace Why {

UtilInterface utilInterface;

void UtilInterface::dumpBlob(CheckStatusWrapper* status, ISC_QUAD* blobId,
	IAttachment* att, ITransaction* tra, const char* file_name, FB_BOOLEAN txt)
{
	FILE* file = os_utils::fopen(file_name, txt ? FOPEN_WRITE_TYPE_TEXT : FOPEN_WRITE_TYPE);
	try
	{
		if (!file)
			system_error::raise("fopen");

		if (!att)
			Arg::Gds(isc_bad_db_handle).raise();

		dump(status, blobId, att, tra, file);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	if (file)
		fclose(file);
}

void UtilInterface::loadBlob(CheckStatusWrapper* status, ISC_QUAD* blobId,
	IAttachment* att, ITransaction* tra, const char* file_name, FB_BOOLEAN txt)
{
/**************************************
 *
 *	l o a d B l o b
 *
 **************************************
 *
 * Functional description
 *	Load a blob with the contents of a file.
 *
 **************************************/
	FILE* file = os_utils::fopen(file_name, txt ? FOPEN_READ_TYPE_TEXT : FOPEN_READ_TYPE);
	try
	{
		if (!file)
			system_error::raise("fopen");

		if (!att)
			Arg::Gds(isc_bad_db_handle).raise();

		load(status, blobId, att, tra, file);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	if (file)
		fclose(file);
}

void UtilInterface::getFbVersion(CheckStatusWrapper* status, IAttachment* att,
	IVersionCallback* callback)
{
/**************************************
 *
 *	g d s _ $ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Obtain and print information about a database.
 *
 **************************************/
	try
	{
		if (!att)
			Arg::Gds(isc_bad_db_handle).raise();

		UCharBuffer buffer;
		USHORT buf_len = 256;
		UCHAR* buf = buffer.getBuffer(buf_len);

		const TEXT* versions = 0;
		const TEXT* implementations = 0;
		const UCHAR* dbis = NULL;
		bool redo;

		do
		{
			att->getInfo(status, sizeof(info), info, buf_len, buf);
			if (status->getState() & Firebird::IStatus::STATE_ERRORS)
				return;

			const UCHAR* p = buf;
			redo = false;

			while (!redo && *p != isc_info_end && p < buf + buf_len)
			{
				const UCHAR item = *p++;
				const USHORT len = static_cast<USHORT>(gds__vax_integer(p, 2));

				p += 2;

				switch (item)
				{
				case isc_info_firebird_version:
					versions = (TEXT*) p;
					break;

				case isc_info_implementation:
					implementations = (TEXT*) p;
					break;

				case fb_info_implementation:
					dbis = p;
					if (dbis[0] * 6 + 1 > len)
					{
						// fb_info_implementation value appears incorrect
						dbis = NULL;
					}
					break;

				case isc_info_error:
					// old server does not understand fb_info_implementation
					break;

				case isc_info_truncated:
					redo = true;
					break;

				default:
					(Arg::Gds(isc_random) << "Invalid info item").raise();
				}

				p += len;
			}

			// Our buffer wasn't large enough to hold all the information,
			// make a larger one and try again.
			if (redo)
			{
				buf_len += 1024;
				buf = buffer.getBuffer(buf_len);
			}
		} while (redo);

		UCHAR count = MIN(*versions, *implementations);
		++versions;
		++implementations;

		UCHAR diCount = 0;
		if (dbis)
			diCount = *dbis++;

		string s;

		UCHAR diCurrent = 0;

		for (UCHAR level = 0; level < count; ++level)
		{
			const USHORT implementation_nr = *implementations++;
			const USHORT impl_class_nr = *implementations++;
			const int l = *versions++; // it was UCHAR
			const TEXT* implementation_string;
			string dbi_string;

			if (dbis && dbis[diCurrent * 6 + 5] == level)
			{
				dbi_string = DbImplementation::pick(&dbis[diCurrent * 6]).implementation();
				implementation_string = dbi_string.c_str();

				if (++diCurrent >= diCount)
					dbis = NULL;
			}
			else
			{
				dbi_string = DbImplementation::fromBackwardCompatibleByte(implementation_nr).implementation();
				implementation_string = dbi_string.nullStr();

				if (!implementation_string)
					implementation_string = "**unknown**";
			}

			const TEXT* class_string;

			if (impl_class_nr >= FB_NELEM(impl_class) || !(class_string = impl_class[impl_class_nr]))
				class_string = "**unknown**";

			s.printf("%s (%s), version \"%.*s\"", implementation_string, class_string, l, versions);

			callback->callback(status, s.c_str());
			if (status->getState() & Firebird::IStatus::STATE_ERRORS)
				return;
			versions += l;
		}

		USHORT ods_version, ods_minor_version;
		get_ods_version(status, att, &ods_version, &ods_minor_version);
		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
			return;

		s.printf("on disk structure version %d.%d", ods_version, ods_minor_version);
		callback->callback(status, s.c_str());
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}
}

YAttachment* UtilInterface::executeCreateDatabase(
	Firebird::CheckStatusWrapper* status, unsigned stmtLength, const char* creatDBstatement,
	unsigned dialect, FB_BOOLEAN* stmtIsCreateDb)
{
	try
	{
		bool stmtEaten;
		YAttachment* att = NULL;

		if (stmtIsCreateDb)
			*stmtIsCreateDb = FB_FALSE;

		if (!PREPARSE_execute(status, &att, stmtLength, creatDBstatement, &stmtEaten, dialect))
			return NULL;

		if (stmtIsCreateDb)
			*stmtIsCreateDb = FB_TRUE;

		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
			return NULL;

		LocalStatus tempStatus;
		CheckStatusWrapper tempCheckStatusWrapper(&tempStatus);

		ITransaction* crdbTrans = att->startTransaction(status, 0, NULL);

		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		{
			att->dropDatabase(&tempCheckStatusWrapper);
			return NULL;
		}

		bool v3Error = false;

		if (!stmtEaten)
		{
			att->execute(status, crdbTrans, stmtLength, creatDBstatement, dialect, NULL, NULL, NULL, NULL);
			if (status->getState() & Firebird::IStatus::STATE_ERRORS)
			{
				crdbTrans->rollback(&tempCheckStatusWrapper);
				att->dropDatabase(&tempCheckStatusWrapper);
				return NULL;
			}
		}

		crdbTrans->commit(status);
		if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		{
			crdbTrans->rollback(&tempCheckStatusWrapper);
			att->dropDatabase(&tempCheckStatusWrapper);
			return NULL;
		}

		return att;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return NULL;
	}
}

void UtilInterface::decodeDate(ISC_DATE date, unsigned* year, unsigned* month, unsigned* day)
{
	tm times;
	isc_decode_sql_date(&date, &times);

	if (year)
		*year = times.tm_year + 1900;
	if (month)
		*month = times.tm_mon + 1;
	if (day)
		*day = times.tm_mday;
}

void UtilInterface::decodeTime(ISC_TIME time,
	unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions)
{
	tm times;
	isc_decode_sql_time(&time, &times);

	if (hours)
		*hours = times.tm_hour;
	if (minutes)
		*minutes = times.tm_min;
	if (seconds)
		*seconds = times.tm_sec;
	if (fractions)
		*fractions = time % ISC_TIME_SECONDS_PRECISION;
}

ISC_DATE UtilInterface::encodeDate(unsigned year, unsigned month, unsigned day)
{
	tm times;
	times.tm_year = year - 1900;
	times.tm_mon = month - 1;
	times.tm_mday = day;

	ISC_DATE date;
	isc_encode_sql_date(&times, &date);

	return date;
}

ISC_TIME UtilInterface::encodeTime(unsigned hours, unsigned minutes, unsigned seconds,
	unsigned fractions)
{
	tm times;
	times.tm_hour = hours;
	times.tm_min = minutes;
	times.tm_sec = seconds;

	ISC_TIME time;
	isc_encode_sql_time(&times, &time);
	time += fractions;

	return time;
}

unsigned UtilInterface::formatStatus(char* buffer, unsigned bufferSize, IStatus* status)
{
	unsigned state = status->getState();
	unsigned states[] = {IStatus::STATE_ERRORS, IStatus::STATE_WARNINGS};
	const ISC_STATUS* vectors[] = {status->getErrors(), status->getWarnings()};
	string s;

	for (int i = 0; i < 2; ++i)
	{
		if (state & states[i])
		{
			const ISC_STATUS* vector = vectors[i];
			SLONG n;

			while ((n = fb_interpret(buffer, bufferSize, &vector)) != 0)
			{
				if (!s.empty())
					s += "\n-";

				s += string(buffer, n);
			}
		}
	}

	unsigned ret = MIN((unsigned) s.length(), bufferSize);

	memcpy(buffer, s.c_str(), ret);
	if (ret < bufferSize)
		buffer[ret] = 0;

	return ret;
}

unsigned UtilInterface::getClientVersion()
{
	int fv[] = { FILE_VER_NUMBER };
	return fv[0] * 0x100 + fv[1];
}

// End-user proxy for ClumpletReader & Writer
class XpbBuilder FB_FINAL : public DisposeIface<IXpbBuilderImpl<XpbBuilder, CheckStatusWrapper> >
{
public:
	XpbBuilder(unsigned kind, const unsigned char* buf, unsigned len)
		: pb(NULL), strVal(getPool())
	{
		ClumpletReader::Kind k;
		UCHAR tag = 0;
		const ClumpletReader::KindList* kl = NULL;

		switch(kind)
		{
		case DPB:
			kl = ClumpletReader::dpbList;
			break;
		case SPB_ATTACH:
			kl = ClumpletReader::spbList;
			break;
		case SPB_START:
			k = ClumpletReader::SpbStart;
			break;
		case TPB:
			k = ClumpletReader::Tpb;
			tag = isc_tpb_version3;
			break;
		default:
			fatal_exception::raiseFmt("Wrong parameters block kind %d, should be from %d to %d", kind, DPB, TPB);
			break;
		}

		if (!buf)
		{
			if (kl)
				pb = FB_NEW_POOL(getPool()) ClumpletWriter(getPool(), kl, MAX_DPB_SIZE);
			else
				pb = FB_NEW_POOL(getPool()) ClumpletWriter(getPool(), k, MAX_DPB_SIZE, tag);
		}
		else
		{
			if (kl)
				pb = FB_NEW_POOL(getPool()) ClumpletWriter(getPool(), kl, MAX_DPB_SIZE, buf, len);
			else
				pb = FB_NEW_POOL(getPool()) ClumpletWriter(getPool(), k, MAX_DPB_SIZE, buf, len);
		}
	}

	// IXpbBuilder implementation
	void clear(CheckStatusWrapper* status)
	{
		try
		{
			pb->clear();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void removeCurrent(CheckStatusWrapper* status)
	{
		try
		{
			pb->deleteClumplet();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void insertInt(CheckStatusWrapper* status, unsigned char tag, int value)
	{
		try
		{
			pb->insertInt(tag, value);
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void insertBigInt(CheckStatusWrapper* status, unsigned char tag, ISC_INT64 value)
	{
		try
		{
			pb->insertBigInt(tag, value);
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void insertBytes(CheckStatusWrapper* status, unsigned char tag, const void* bytes, unsigned length)
	{
		try
		{
			pb->insertBytes(tag, bytes, length);
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void insertString(CheckStatusWrapper* status, unsigned char tag, const char* str)
	{
		try
		{
			pb->insertString(tag, str, strlen(str));
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void insertTag(CheckStatusWrapper* status, unsigned char tag)
	{
		try
		{
			pb->insertTag(tag);
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	FB_BOOLEAN isEof(CheckStatusWrapper* status)
	{
		try
		{
			return pb->isEof() ? FB_TRUE : FB_FALSE;
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return FB_TRUE;
		}
	}

	void moveNext(CheckStatusWrapper* status)
	{
		try
		{
			pb->moveNext();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void rewind(CheckStatusWrapper* status)
	{
		try
		{
			pb->rewind();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	FB_BOOLEAN findFirst(CheckStatusWrapper* status, unsigned char tag)
	{
		try
		{
			nextTag = tag;
			return pb->find(nextTag) ? FB_TRUE : FB_FALSE;
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return FB_FALSE;
		}
	}

	FB_BOOLEAN findNext(CheckStatusWrapper* status)
	{
		try
		{
			return pb->next(nextTag) ? FB_TRUE : FB_FALSE;
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return FB_FALSE;
		}
	}

	unsigned char getTag(CheckStatusWrapper* status)
	{
		try
		{
			return pb->getClumpTag();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return 0;
		}
	}

	unsigned getLength(CheckStatusWrapper* status)
	{
		try
		{
			return pb->getClumpLength();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return 0;
		}
	}

	int getInt(CheckStatusWrapper* status)
	{
		try
		{
			return pb->getInt();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return 0;
		}
	}

	ISC_INT64 getBigInt(CheckStatusWrapper* status)
	{
		try
		{
			return pb->getBigInt();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return 0;
		}
	}

	const char* getString(CheckStatusWrapper* status)
	{
		try
		{
			pb->getString(strVal);
			return strVal.c_str();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return NULL;
		}
	}

	const unsigned char* getBytes(CheckStatusWrapper* status)
	{
		try
		{
			return pb->getBytes();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return NULL;
		}
	}

	unsigned getBufferLength(CheckStatusWrapper* status)
	{
		try
		{
			return pb->getBufferLength();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return 0;
		}
	}

	const unsigned char* getBuffer(CheckStatusWrapper* status)
	{
		try
		{
			return pb->getBuffer();
		}
		catch (const Exception& ex)
		{
			ex.stuffException(status);
			return NULL;
		}
	}

	void dispose()
	{
		delete this;
	}

private:
	AutoPtr<ClumpletWriter> pb;
	unsigned char nextTag;
	string strVal;
};

IXpbBuilder* UtilInterface::getXpbBuilder(CheckStatusWrapper* status,
	unsigned kind, const unsigned char* buf, unsigned len)
{
	try
	{
		return FB_NEW XpbBuilder(kind, buf, len);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return NULL;
	}
}

unsigned UtilInterface::setOffsets(CheckStatusWrapper* status, IMessageMetadata* metadata,
	IOffsetsCallback* callback)
{
	try
	{
		unsigned count = metadata->getCount(status);
		check(status);

		unsigned length = 0;

		for (unsigned n = 0; n < count; ++n)
		{
			unsigned type = metadata->getType(status, n);
			check(status);
			unsigned len = metadata->getLength(status, n);
			check(status);

			unsigned offset, nullOffset;
			length = fb_utils::sqlTypeToDsc(length, type, len,
				NULL /*dtype*/, NULL /*length*/, &offset, &nullOffset);

			callback->setOffset(status, n, offset, nullOffset);
			check(status);
		}

		return length;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
	}

	return 0;
}

// Deal with events
class EventBlock FB_FINAL : public DisposeIface<IEventBlockImpl<EventBlock, CheckStatusWrapper> >
{
public:
	EventBlock(const char** events)
		: values(getPool()), buffer(getPool()), counters(getPool())
	{
		if (!events[0])
		{
			(Arg::Gds(isc_random) << "No events passed as an argument"
				<< Arg::SqlState("HY024")).raise();
				// HY024: Invalid attribute value
		}

		unsigned num = 0;
		values.push(EPB_version1);

		for (const char** e = events; *e; ++e)
		{
			++num;

			string ev(*e);
			ev.rtrim();

			if (ev.length() > 255)
			{
				(Arg::Gds(isc_random) << ("Too long event name: " + ev)
					<< Arg::SqlState("HY024")).raise();
					// HY024: Invalid attribute value
			}
			values.push(ev.length());
			values.push(reinterpret_cast<const unsigned char*>(ev.begin()), ev.length());
			values.push(0);
			values.push(0);
			values.push(0);
			values.push(0);
		}

		// allocate memory for various buffers
		buffer.getBuffer(values.getCount());
		counters.getBuffer(num);
	}

	unsigned getLength()
	{
		return values.getCount();
	}

	unsigned char* getValues()
	{
		return values.begin();
	}

	unsigned char* getBuffer()
	{
		return buffer.begin();
	}

	unsigned getCount()
	{
		return counters.getCount();
	}

	unsigned* getCounters()
	{
		return (unsigned*) counters.begin();
	}

	void counts()
	{
		isc_event_counts(counters.begin(), values.getCount(), values.begin(), buffer.begin());
	}

	void dispose()
	{
		delete this;
	}

private:
	UCharBuffer values;
	UCharBuffer buffer;
	HalfStaticArray<ULONG, 16> counters;
};

IEventBlock* UtilInterface::createEventBlock(CheckStatusWrapper* status, const char** events)
{
	try
	{
		return FB_NEW EventBlock(events);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return NULL;
	}
}

} // namespace Why


#if (defined SOLARIS ) || (defined __cplusplus)
extern "C" {
#endif
// Avoid C++ linkage API functions


int API_ROUTINE gds__blob_size(FB_API_HANDLE* b, SLONG* size, SLONG* seg_count, SLONG* max_seg)
{
/**************************************
 *
 *	g d s _ $ b l o b _ s i z e
 *
 **************************************
 *
 * Functional description
 *	Get the size, number of segments, and max
 *	segment length of a blob.  Return TRUE
 *	if it happens to succeed.
 *
 **************************************/
	ISC_STATUS_ARRAY status_vector;
	SCHAR buffer[64];

	if (isc_blob_info(status_vector, b, sizeof(blob_items), blob_items, sizeof(buffer), buffer))
	{
		isc_print_status(status_vector);
		return FALSE;
	}

	const UCHAR* p = reinterpret_cast<UCHAR*>(buffer);
	UCHAR item;
	while ((item = *p++) != isc_info_end)
	{
		const USHORT l = gds__vax_integer(p, 2);
		p += 2;
		const SLONG n = gds__vax_integer(p, l);
		p += l;
		switch (item)
		{
		case isc_info_blob_max_segment:
			if (max_seg)
				*max_seg = n;
			break;

		case isc_info_blob_num_segments:
			if (seg_count)
				*seg_count = n;
			break;

		case isc_info_blob_total_length:
			if (size)
				*size = n;
			break;

		default:
			return FALSE;
		}
	}

	return TRUE;
}


// 17 May 2001 - isc_expand_dpb is DEPRECATED
void API_ROUTINE_VARARG isc_expand_dpb(SCHAR** dpb, SSHORT* dpb_size, ...)
{
/**************************************
 *
 *	i s c _ e x p a n d _ d p b
 *
 **************************************
 *
 * Functional description
 *	Extend a database parameter block dynamically
 *	to include runtime info.  Generated
 *	by gpre to provide host variable support for
 *	READY statement	options.
 *	This expects the list of variable args
 *	to be zero terminated.
 *
 *	Note: dpb_size is signed short only for compatibility
 *	with other calls (isc_attach_database) that take a dpb length.
 *
 * TMN: Note: According to Ann Harrison:
 * That routine should be deprecated.  It doesn't do what it
 * should, and does do things it shouldn't, and is harder to
 * use than the natural alternative.
 *
 **************************************/
	SSHORT length;
	UCHAR* p = NULL;
	const char*	q;
	va_list	args;
	USHORT type;
	UCHAR* new_dpb;

	// calculate length of database parameter block, setting initial length to include version

	SSHORT new_dpb_length;
	if (!*dpb || !(new_dpb_length = *dpb_size))
	{
		new_dpb_length = 1;
	}

	va_start(args, dpb_size);

	while (type = va_arg(args, int))
	{
		switch (type)
		{
		case isc_dpb_user_name:
		case isc_dpb_password:
		case isc_dpb_sql_role_name:
		case isc_dpb_lc_messages:
		case isc_dpb_lc_ctype:
		case isc_dpb_reserved:
			q = va_arg(args, char*);
			if (q)
			{
				length = static_cast<SSHORT>(strlen(q));
				new_dpb_length += 2 + length;
			}
			break;

		default:
			va_arg(args, int);
			break;
		}
	}
	va_end(args);

	// if items have been added, allocate space
	// for the new dpb and copy the old one over

	if (new_dpb_length > *dpb_size)
	{
		// Note: gds__free done by GPRE generated code

		new_dpb = (UCHAR*) gds__alloc((SLONG)(sizeof(UCHAR) * new_dpb_length));
		p = new_dpb;
		// FREE: done by client process in GPRE generated code
		if (!new_dpb)
		{
			// NOMEM: don't trash existing dpb
			DEV_REPORT("isc_extend_dpb: out of memory");
			return;				// NOMEM: not really handled
		}

		q = *dpb;
		for (length = *dpb_size; length; length--)
		{
			*p++ = *q++;
		}

	}
	else
	{
		// CVC: Notice this case is new_dpb_length <= *dpb_size, but since
		// we have new_dpb_length = MAX(*dpb_size, 1) our case is reduced
		// to new_dpb_length == *dpb_size. Therefore, this code is a waste
		// of time, since the function didn't find any param to add and thus,
		// the loop below won't find anything worth adding either.
		// Notice, too that the original input dpb is used, yet the pointer "p"
		// is positioned exactly at the end, so if something was added at the
		// tail, it would be a memory failure, unless the caller lies and is
		// always passing a dpb bigger than *dpb_size.
		new_dpb = reinterpret_cast<UCHAR*>(*dpb);
		p = new_dpb + *dpb_size;
	}

	if (!*dpb_size)
		*p++ = isc_dpb_version1;

	// copy in the new runtime items

	va_start(args, dpb_size);

	while (type = va_arg(args, int))
	{
		switch (type)
		{
		case isc_dpb_user_name:
		case isc_dpb_password:
		case isc_dpb_sql_role_name:
		case isc_dpb_lc_messages:
		case isc_dpb_lc_ctype:
		case isc_dpb_reserved:
			q = va_arg(args, char*);
			if (q)
			{
				length = static_cast<SSHORT>(strlen(q));
				fb_assert(type <= CHAR_MAX);
				*p++ = (UCHAR) type;
				fb_assert(length <= CHAR_MAX);
				*p++ = (UCHAR) length;
				while (length--)
					*p++ = *q++;
			}
			break;

		default:
			va_arg(args, int);
			break;
		}
	}
	va_end(args);

	*dpb_size = p - new_dpb;
	*dpb = reinterpret_cast<SCHAR*>(new_dpb);
}


int API_ROUTINE isc_modify_dpb(SCHAR**	dpb,
							   SSHORT*	dpb_size,
							   USHORT	type,
							   const SCHAR*	str,
							   SSHORT	str_len)
{
/**************************************
 *
 *	i s c _ m o d i f y _ d p b
 *
 **************************************
 * CVC: This is exactly the same logic as isc_expand_dpb, but for one param.
 * However, the difference is that when presented with a dpb type it that's
 * unknown, it returns FB_FAILURE immediately. In contrast, isc_expand_dpb
 * doesn't complain and instead treats those as integers and tries to skip
 * them, hoping to sync in the next iteration.
 *
 * Functional description
 *	Extend a database parameter block dynamically
 *	to include runtime info.  Generated
 *	by gpre to provide host variable support for
 *	READY statement	options.
 *	This expects one arg at a time.
 *      the length of the string is passed by the caller and hence
 * 	is not expected to be null terminated.
 * 	this call is a variation of isc_expand_dpb without a variable
 * 	arg parameters.
 * 	Instead, this function is called recursively
 *	Alternatively, this can have a parameter list with all possible
 *	parameters either nulled or with proper value and type.
 *
 *  	**** This can be modified to be so at a later date, making sure
 *	**** all callers follow the same convention
 *
 *	Note: dpb_size is signed short only for compatibility
 *	with other calls (isc_attach_database) that take a dpb length.
 *
 **************************************/

	// calculate length of database parameter block, setting initial length to include version

	SSHORT new_dpb_length;
	if (!*dpb || !(new_dpb_length = *dpb_size))
	{
		new_dpb_length = 1;
	}

	switch (type)
	{
	case isc_dpb_user_name:
	case isc_dpb_password:
	case isc_dpb_sql_role_name:
	case isc_dpb_lc_messages:
	case isc_dpb_lc_ctype:
	case isc_dpb_reserved:
		new_dpb_length += 2 + str_len;
		break;

	default:
		return FB_FAILURE;
	}

	// if items have been added, allocate space
	// for the new dpb and copy the old one over

	UCHAR* new_dpb;
	if (new_dpb_length > *dpb_size)
	{
		// Note: gds__free done by GPRE generated code

		new_dpb = (UCHAR*) gds__alloc((SLONG)(sizeof(UCHAR) * new_dpb_length));

		// FREE: done by client process in GPRE generated code
		if (!new_dpb)
		{
			// NOMEM: don't trash existing dpb
			DEV_REPORT("isc_extend_dpb: out of memory");
			return FB_FAILURE;		// NOMEM: not really handled
		}

		memcpy(new_dpb, *dpb, *dpb_size);
	}
	else
		new_dpb = reinterpret_cast<UCHAR*>(*dpb);

	UCHAR* p = new_dpb + *dpb_size;

	if (!*dpb_size)
	{
		*p++ = isc_dpb_version1;
	}

	// copy in the new runtime items

	switch (type)
	{
	case isc_dpb_user_name:
	case isc_dpb_password:
	case isc_dpb_sql_role_name:
	case isc_dpb_lc_messages:
	case isc_dpb_lc_ctype:
	case isc_dpb_reserved:
		{
			const UCHAR* q = reinterpret_cast<const UCHAR*>(str);
			if (q)
			{
				SSHORT length = str_len;
				fb_assert(type <= MAX_UCHAR);
				*p++ = (UCHAR) type;
				fb_assert(length <= MAX_UCHAR);
				*p++ = (UCHAR) length;
				while (length--)
				{
					*p++ = *q++;
				}
			}
			break;
		}

	default:
		return FB_FAILURE;
	}

	*dpb_size = p - new_dpb;
	*dpb = (SCHAR*) new_dpb;

	return FB_SUCCESS;
}


#if defined(UNIX) || defined(WIN_NT)
int API_ROUTINE gds__edit(const TEXT* file_name, USHORT /*type*/)
{
/**************************************
 *
 *	g d s _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Edit a file.
 *
 **************************************/
	string editor;

#ifndef WIN_NT
	if (!fb_utils::readenv("VISUAL", editor) && !fb_utils::readenv("EDITOR", editor))
		editor = "vi";
#else
	if (!fb_utils::readenv("EDITOR", editor))
		editor = "Notepad";
#endif

	struct STAT before;
	os_utils::stat(file_name, &before);
	// The path of the editor + the path of the file + quotes + one space.
	// We aren't using quotes around the editor for now.
	TEXT buffer[MAXPATHLEN * 2 + 5];
	fb_utils::snprintf(buffer, sizeof(buffer), "%s \"%s\"", editor.c_str(), file_name);

	FB_UNUSED(system(buffer));

	struct STAT after;
	os_utils::stat(file_name, &after);

	return (before.st_mtime != after.st_mtime || before.st_size != after.st_size);
}
#endif


SLONG API_ROUTINE gds__event_block(UCHAR** event_buffer, UCHAR** result_buffer, USHORT count, ...)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Create an initialized event parameter block from a
 *	variable number of input arguments.
 *	Return the size of the block.
 *
 *	Return 0 as the size if the event parameter block cannot be
 *	created for any reason.
 *
 **************************************/
	UCHAR* p;
	SCHAR* q;
	SLONG length;
	va_list ptr;
	USHORT i;

	va_start(ptr, count);

	// calculate length of event parameter block,
	// setting initial length to include version
	// and counts for each argument

	length = 1;
	i = count;
	while (i--)
	{
		q = va_arg(ptr, SCHAR*);
		length += static_cast<SLONG>(strlen(q)) + 5;
	}

	p = *event_buffer = (UCHAR*) gds__alloc((SLONG) (sizeof(UCHAR) * length));
	// FREE: unknown
	if (!*event_buffer)			// NOMEM:
		return 0;
	*result_buffer = (UCHAR*) gds__alloc((SLONG) (sizeof(UCHAR) * length));
	// FREE: unknown
	if (!*result_buffer)
	{
		// NOMEM:
		gds__free(*event_buffer);
		*event_buffer = NULL;
		return 0;
	}

#ifdef DEBUG_GDS_ALLOC
	// I can't find anywhere these items are freed
	// 1994-October-25 David Schnepper
	gds_alloc_flag_unfreed((void*) *event_buffer);
	gds_alloc_flag_unfreed((void*) *result_buffer);
#endif // DEBUG_GDS_ALLOC

	// initialize the block with event names and counts

	*p++ = EPB_version1;

	va_start(ptr, count);

	i = count;
	while (i--)
	{
		q = va_arg(ptr, SCHAR*);

		// Strip trailing blanks from string

		const SCHAR* end = q + strlen(q);
		while (--end >= q && *end == ' ')
			; // empty loop
		*p++ = end - q + 1;
		while (q <= end)
			*p++ = *q++;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
	}

	return p - *event_buffer;
}


USHORT API_ROUTINE gds__event_block_a(SCHAR** event_buffer,
									  SCHAR** result_buffer,
									  SSHORT count,
									  SCHAR** name_buffer)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ b l o c k _ a
 *
 **************************************
 *
 * Functional description
 *	Create an initialized event parameter block from a
 *	vector of input arguments. (Ada needs this)
 *	Assume all strings are 31 characters long.
 *	Return the size of the block.
 *
 **************************************/
	const int MAX_NAME_LENGTH = 31;
	// calculate length of event parameter block,
	// setting initial length to include version
	// and counts for each argument

	USHORT i = count;
	const SCHAR* const* nb = name_buffer;
	SLONG length = 0;
	while (i--)
	{
		const SCHAR* q = *nb++;

		// Strip trailing blanks from string
		const SCHAR* end = q + MAX_NAME_LENGTH;
		while (--end >= q && *end == ' '); // null body
		length += end - q + 1 + 5;
	}

	i = count;
	SCHAR* p = *event_buffer = (SCHAR*) gds__alloc((SLONG) (sizeof(SCHAR) * length));
	// FREE: unknown
	if (!*event_buffer)			// NOMEM:
		return 0;
	*result_buffer = (SCHAR*) gds__alloc((SLONG) (sizeof(SCHAR) * length));
	// FREE: unknown
	if (!*result_buffer)
	{
		// NOMEM:
		gds__free(*event_buffer);
		*event_buffer = NULL;
		return 0;
	}

#ifdef DEBUG_GDS_ALLOC
	// I can't find anywhere these items are freed
	// 1994-October-25 David Schnepper
	gds_alloc_flag_unfreed((void*) *event_buffer);
	gds_alloc_flag_unfreed((void*) *result_buffer);
#endif // DEBUG_GDS_ALLOC

	*p++ = EPB_version1;

	nb = name_buffer;

	while (i--)
	{
		const SCHAR* q = *nb++;

		// Strip trailing blanks from string
		const SCHAR* end = q + MAX_NAME_LENGTH;
		while (--end >= q && *end == ' ')
			; // null body
		*p++ = end - q + 1;
		while (q <= end)
			*p++ = *q++;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
	}

	return (p - *event_buffer);
}


void API_ROUTINE gds__event_block_s(SCHAR** event_buffer,
									SCHAR** result_buffer,
									SSHORT count,
									SCHAR** name_buffer,
									SSHORT* return_count)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ b l o c k _ s
 *
 **************************************
 *
 * Functional description
 *	THIS IS THE COBOL VERSION of gds__event_block_a for Cobols
 *	that don't permit return values.
 *
 **************************************/

	*return_count = gds__event_block_a(event_buffer, result_buffer, count, name_buffer);
}


void API_ROUTINE isc_event_counts(ULONG* result_vector,
								  SSHORT buffer_length,
								  UCHAR* event_buffer,
								  const UCHAR* result_buffer)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ c o u n t s
 *
 **************************************
 *
 * Functional description
 *	Get the delta between two events in an event
 *	parameter block.  Used to update gds_events
 *	for GPRE support of events.
 *
 **************************************/
	ULONG* vec = result_vector;
	const UCHAR* p = event_buffer;
	const UCHAR* q = result_buffer;
	USHORT length = buffer_length;
	const UCHAR* const end = p + length;

	// analyze the event blocks, getting the delta for each event

	p++;
	q++;
	while (p < end)
	{
		// skip over the event name

		const USHORT i = (USHORT)* p++;
		p += i;
		q += i + 1;

		// get the change in count

		const ULONG initial_count = gds__vax_integer(p, sizeof(SLONG));
		p += sizeof(SLONG);
		const ULONG new_count = gds__vax_integer(q, sizeof(SLONG));
		q += sizeof(SLONG);
		*vec++ = new_count - initial_count;
	}

	// copy over the result to the initial block to prepare
	// for the next call to gds__event_wait

	memcpy(event_buffer, result_buffer, length);
}


void API_ROUTINE isc_get_client_version(SCHAR* buffer)
{
/**************************************
 *
 *	g d s _ $ g e t _ c l i e n t _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	if (buffer)
		strcpy(buffer, ISC_VERSION);
}


int API_ROUTINE isc_get_client_major_version()
{
/**************************************
 *
 *	g d s _ $ g e t _ c l i e n t _ m a j o r _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	return atoi(ISC_MAJOR_VER);
}


int API_ROUTINE isc_get_client_minor_version()
{
/**************************************
 *
 *	g d s _ $ g e t _ c l i e n t _ m i n o r _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	return atoi(ISC_MINOR_VER);
}


void API_ROUTINE gds__map_blobs(int* /*handle1*/, int* /*handle2*/)
{
/**************************************
 *
 *	g d s _ $ m a p _ b l o b s
 *
 **************************************
 *
 * Functional description
 *	Deprecated API function.
 *
 **************************************/
}


void API_ROUTINE isc_set_debug(int /*value*/)
{
/**************************************
 *
 *	G D S _ S E T _ D E B U G
 *
 **************************************
 *
 * Functional description
 *	Deprecated API function.
 *
 **************************************/

}


void API_ROUTINE isc_set_login(const UCHAR** dpb, SSHORT* dpb_size)
{
/**************************************
 *
 *	i s c _ s e t _ l o g i n
 *
 **************************************
 *
 * Functional description
 *	Pickup any ISC_USER and ISC_PASSWORD
 *	environment variables, and stuff them
 *	into the dpb (if there is no user name
 *	or password already referenced).
 *
 **************************************/

	// look for the environment variables

	string username, password;
	if (!fb_utils::readenv(ISC_USER, username) && !fb_utils::readenv(ISC_PASSWORD, password))
		return;

	// figure out whether the username or password have already been specified

	bool user_seen = false, password_seen = false;

	if (*dpb && *dpb_size)
	{
	    const UCHAR* p = *dpb;
		for (const UCHAR* const end_dpb = p + *dpb_size; p < end_dpb;)
		{
			const int item = *p++;
			switch (item)
			{
			case isc_dpb_version1:
				continue;

			case isc_dpb_user_name:
				user_seen = true;
				break;

			case isc_dpb_password:
			case isc_dpb_password_enc:
				password_seen = true;
				break;
			}

			// get the length and increment past the parameter.
			const USHORT l = *p++;
			p += l;
		}
	}

	if (username.length() && !user_seen)
	{
		if (password.length() && !password_seen)
			isc_expand_dpb_internal(dpb, dpb_size, isc_dpb_user_name, username.c_str(),
									isc_dpb_password, password.c_str(), 0);
		else
			isc_expand_dpb_internal(dpb, dpb_size, isc_dpb_user_name, username.c_str(), 0);
	}
	else if (password.length() && !password_seen)
		isc_expand_dpb_internal(dpb, dpb_size, isc_dpb_password, password.c_str(), 0);
}


void API_ROUTINE isc_set_single_user(const UCHAR** dpb, SSHORT* dpb_size, const TEXT* single_user)
{
/****************************************
 *
 *      i s c _ s e t _ s i n g l e _ u s e r
 *
 ****************************************
 *
 * Functional description
 *      Stuff the single_user flag into the dpb
 *      if the flag doesn't already exist in the dpb.
 *
 ****************************************/

	// Discover if single user access has already been specified

	bool single_user_seen = false;

	if (*dpb && *dpb_size)
	{
		const UCHAR* p = *dpb;
		for (const UCHAR* const end_dpb = p + *dpb_size; p < end_dpb;)
		{
			const int item = *p++;
			switch (item)
			{
			case isc_dpb_version1:
				continue;
			case isc_dpb_reserved:
				single_user_seen = true;
				break;
			}

			// Get the length and increment past the parameter.

			const USHORT l = *p++;
			p += l;

		}
	}

	if (!single_user_seen)
		isc_expand_dpb_internal(dpb, dpb_size, isc_dpb_reserved, single_user, 0);

}


static void print_version(void*, const char* version)
{
	printf("\t%s\n", version);
}

int API_ROUTINE isc_version(FB_API_HANDLE* handle, FPTR_VERSION_CALLBACK routine, void* user_arg)
{
/**************************************
 *
 *	g d s _ $ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Obtain and print information about a database.
 *
 **************************************/
	if (!routine)
		routine = print_version;

	LocalStatus ls;
	CheckStatusWrapper st(&ls);
	RefPtr<IAttachment> att(REF_NO_INCR, handleToIAttachment(&st, handle));
	if (st.getState() & IStatus::STATE_ERRORS)
		return FB_FAILURE;

	VersionCallback callback(routine, user_arg);
	UtilInterfacePtr()->getFbVersion(&st, att, &callback);

	return st.getState() & IStatus::STATE_ERRORS ? FB_FAILURE : FB_SUCCESS;
}


void API_ROUTINE isc_format_implementation(USHORT impl_nr,
										   USHORT ibuflen, TEXT* ibuf,
										   USHORT impl_class_nr,
										   USHORT cbuflen, TEXT* cbuf)
{
/**************************************
 *
 *	i s c _ f o r m a t _ i m p l e m e n t a t i o n
 *
 **************************************
 *
 * Functional description
 *	Convert the implementation and class codes to strings
 * 	by looking up their values in the internal tables.
 *
 **************************************/
	if (ibuflen > 0)
	{
		string imp =
			DbImplementation::fromBackwardCompatibleByte(impl_nr).implementation();
		imp.copyTo(ibuf, ibuflen);
	}

	if (cbuflen > 0)
	{
		if (impl_class_nr >= FB_NELEM(impl_class) || !(impl_class[impl_class_nr]))
			fb_utils::copy_terminate(cbuf, "**unknown**", cbuflen);
		else
			fb_utils::copy_terminate(cbuf, impl_class[impl_class_nr], cbuflen);
	}

}


uintptr_t API_ROUTINE isc_baddress(SCHAR* object)
{
/**************************************
 *
 *        i s c _ b a d d r e s s
 *
 **************************************
 *
 * Functional description
 *      Return the address of whatever is passed in
 *
 **************************************/

	return (uintptr_t) object;
}


void API_ROUTINE isc_baddress_s(const SCHAR* object, uintptr_t* address)
{
/**************************************
 *
 *        i s c _ b a d d r e s s _ s
 *
 **************************************
 *
 * Functional description
 *      Copy the address of whatever is passed in to the 2nd param.
 *
 **************************************/

	*address = (uintptr_t) object;
}


int API_ROUTINE BLOB_close(FB_BLOB_STREAM blobStream)
{
/**************************************
 *
 *	B L O B _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Close a blob stream.
 *
 **************************************/
	ISC_STATUS_ARRAY status_vector;

	if (!blobStream->bstr_blob)
		return FALSE;

	if (blobStream->bstr_mode & BSTR_output)
	{
		const USHORT l = (blobStream->bstr_ptr - blobStream->bstr_buffer);
		if (l > 0)
		{
			if (isc_put_segment(status_vector, &blobStream->bstr_blob, l, blobStream->bstr_buffer))
			{
				return FALSE;
			}
		}
	}

	isc_close_blob(status_vector, &blobStream->bstr_blob);

	if (blobStream->bstr_mode & BSTR_alloc)
		gds__free(blobStream->bstr_buffer);

	gds__free(blobStream);

	return TRUE;
}


int API_ROUTINE blob__display(SLONG blob_id[2],
							  FB_API_HANDLE* database,
							  FB_API_HANDLE* transaction,
							  const TEXT* field_name, const SSHORT* name_length)
{
/**************************************
 *
 *	b l o b _ $ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	PASCAL callable version of EDIT_blob.
 *
 **************************************/
	const MetaName temp(field_name, *name_length);

	return BLOB_display(reinterpret_cast<ISC_QUAD*>(blob_id), *database, *transaction, temp.c_str());
}


int API_ROUTINE BLOB_display(ISC_QUAD* blob_id,
							 FB_API_HANDLE database,
							 FB_API_HANDLE transaction,
							 const TEXT* /*field_name*/)
{
/**************************************
 *
 *	B L O B _ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to stdout
 *
 **************************************/
	LocalStatus st;
	CheckStatusWrapper statusWrapper(&st);

	RefPtr<IAttachment> att(REF_NO_INCR, handleToIAttachment(&statusWrapper, &database));
	if (st.getState() & IStatus::STATE_ERRORS)
		return FB_FAILURE;
	RefPtr<ITransaction> tra(REF_NO_INCR, handleToITransaction(&statusWrapper, &transaction));
	if (st.getState() & IStatus::STATE_ERRORS)
		return FB_FAILURE;

	try
	{
		dump(&statusWrapper, blob_id, att, tra, stdout);
	}
	catch (const Exception& ex)
	{
		ex.stuffException(&statusWrapper);
	}

	if (statusWrapper.getState() & Firebird::IStatus::STATE_ERRORS)
	{
		isc_print_status(statusWrapper.getErrors());
		return FB_FAILURE;
	}

	return FB_SUCCESS;
}


int API_ROUTINE blob__dump(SLONG blob_id[2],
						   FB_API_HANDLE* database,
						   FB_API_HANDLE* transaction,
						   const TEXT* file_name, const SSHORT* name_length)
{
/**************************************
 *
 *	b l o b _ $ d u m p
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable dump
 *	into an internal dump call.
 *
 **************************************/
	// CVC: The old logic passed garbage to BLOB_dump if !*name_length
	TEXT temp[129];
	USHORT l = *name_length;
	if (l != 0)
	{
		if (l >= sizeof(temp))
			l = sizeof(temp) - 1;

		memcpy(temp, file_name, l);
	}
	temp[l] = 0;

	return BLOB_dump(reinterpret_cast<ISC_QUAD*>(blob_id), *database, *transaction, temp);
}


static int any_text_dump(ISC_QUAD* blob_id,
						 FB_API_HANDLE database,
						 FB_API_HANDLE transaction,
						 const SCHAR* file_name,
						 FB_BOOLEAN txt)
{
/**************************************
 *
 *	a n y _ t e x t _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Dump a blob into a file.
 *
 **************************************/
	LocalStatus ls;
	CheckStatusWrapper st(&ls);
	RefPtr<IAttachment> att(REF_NO_INCR, handleToIAttachment(&st, &database));
	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
		return FB_FAILURE;
	RefPtr<ITransaction> tra(REF_NO_INCR, handleToITransaction(&st, &transaction));
	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
		return FB_FAILURE;

	UtilInterfacePtr()->dumpBlob(&st, blob_id, att, tra, file_name, txt);

	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
	{
		isc_print_status(st.getErrors());
		return FB_FAILURE;
	}

	return FB_SUCCESS;
}


int API_ROUTINE BLOB_text_dump(ISC_QUAD* blob_id,
							   FB_API_HANDLE database,
							   FB_API_HANDLE transaction,
							   const SCHAR* file_name)
{
/**************************************
 *
 *	B L O B _ t e x t _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Dump a blob into a file.
 *      This call does CR/LF translation on NT.
 *
 **************************************/
	return any_text_dump(blob_id, database, transaction, file_name, FB_TRUE);
}


int API_ROUTINE BLOB_dump(ISC_QUAD* blob_id,
						  FB_API_HANDLE database,
						  FB_API_HANDLE transaction,
						  const SCHAR* file_name)
{
/**************************************
 *
 *	B L O B _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Dump a blob into a file.
 *
 **************************************/
	return any_text_dump(blob_id, database, transaction, file_name, FB_FALSE);
}


int API_ROUTINE blob__edit(SLONG blob_id[2],
						   FB_API_HANDLE* database,
						   FB_API_HANDLE* transaction,
						   const TEXT* field_name, const SSHORT* name_length)
{
/**************************************
 *
 *	b l o b _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable edit
 *	into an internal edit call.
 *
 **************************************/
	const MetaName temp(field_name, *name_length);

	return BLOB_edit(reinterpret_cast<ISC_QUAD*>(blob_id), *database, *transaction, temp.c_str());
}


int API_ROUTINE BLOB_edit(ISC_QUAD* blob_id,
						  FB_API_HANDLE database,
						  FB_API_HANDLE transaction,
						  const SCHAR* field_name)
{
/**************************************
 *
 *	B L O B _ e d i t
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to a file, allow the user to edit the
 *	window, and dump the data back into a blob.  If the user
 *	bails out, return FALSE, otherwise return TRUE.
 *
 **************************************/

	LocalStatus st;
	CheckStatusWrapper statusWrapper(&st);

	RefPtr<IAttachment> att(REF_NO_INCR, handleToIAttachment(&statusWrapper, &database));
	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
		return FB_FAILURE;
	RefPtr<ITransaction> tra(REF_NO_INCR, handleToITransaction(&statusWrapper, &transaction));
	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
		return FB_FAILURE;

	int rc = FB_SUCCESS;

	try
	{
		rc = edit(&statusWrapper, blob_id, att, tra, TRUE, field_name) ? FB_SUCCESS : FB_FAILURE;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(&statusWrapper);
	}

	if (statusWrapper.getState() & Firebird::IStatus::STATE_ERRORS)
		isc_print_status(statusWrapper.getErrors());

	return rc;
}


int API_ROUTINE BLOB_get(FB_BLOB_STREAM blobStream)
{
/**************************************
 *
 *	B L O B _ g e t
 *
 **************************************
 *
 * Functional description
 *	Return the next byte of a blob.  If the blob is exhausted, return
 *	EOF.
 *
 **************************************/
	ISC_STATUS_ARRAY status_vector;

	if (!blobStream->bstr_buffer)
		return EOF;

	while (true)
	{
		if (--blobStream->bstr_cnt >= 0)
			return *blobStream->bstr_ptr++ & 0377;

		isc_get_segment(status_vector, &blobStream->bstr_blob,
			// safe - cast from short, alignment is OK
			reinterpret_cast<USHORT*>(&blobStream->bstr_cnt),
			blobStream->bstr_length, blobStream->bstr_buffer);
		if (status_vector[1] && status_vector[1] != isc_segment)
		{
			blobStream->bstr_ptr = 0;
			blobStream->bstr_cnt = 0;
			if (status_vector[1] != isc_segstr_eof)
				isc_print_status(status_vector);
			return EOF;
		}
		blobStream->bstr_ptr = blobStream->bstr_buffer;
	}
}


int API_ROUTINE blob__load(SLONG blob_id[2],
						   FB_API_HANDLE* database,
						   FB_API_HANDLE* transaction,
						   const TEXT* file_name, const SSHORT* name_length)
{
/**************************************
 *
 *	b l o b _ $ l o a d
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable load
 *	into an internal load call.
 *
 **************************************/
	// CVC: The old logic passed garbage to BLOB_load if !*name_length
	TEXT temp[129];
	USHORT l = *name_length;
	if (l != 0)
	{
		if (l >= sizeof(temp))
			l = sizeof(temp) - 1;

		memcpy(temp, file_name, l);
	}
	temp[l] = 0;

	return BLOB_load(reinterpret_cast<ISC_QUAD*>(blob_id), *database, *transaction, temp);
}


static int any_text_load(ISC_QUAD* blob_id,
						  FB_API_HANDLE database,
						  FB_API_HANDLE transaction,
						  const TEXT* file_name,
						  FB_BOOLEAN flag)
{
/**************************************
 *
 *	a n y _ t e x t _ l o a d
 *
 **************************************
 *
 * Functional description
 *	Load a  blob with the contents of a file.
 *      Return TRUE is successful.
 *
 **************************************/
	LocalStatus ls;
	CheckStatusWrapper st(&ls);
	RefPtr<IAttachment> att(REF_NO_INCR, handleToIAttachment(&st, &database));
	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
		return FB_FAILURE;
	RefPtr<ITransaction> tra(REF_NO_INCR, handleToITransaction(&st, &transaction));
	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
		return FB_FAILURE;

	UtilInterfacePtr()->loadBlob(&st, blob_id, att, tra, file_name, flag);

	if (st.getState() & Firebird::IStatus::STATE_ERRORS)
	{
		isc_print_status(st.getErrors());
		return FB_FAILURE;
	}

	return FB_SUCCESS;
}


int API_ROUTINE BLOB_text_load(ISC_QUAD* blob_id,
							   FB_API_HANDLE database,
							   FB_API_HANDLE transaction,
							   const TEXT* file_name)
{
/**************************************
 *
 *	B L O B _ t e x t _ l o a d
 *
 **************************************
 *
 * Functional description
 *	Load a  blob with the contents of a file.
 *      This call does CR/LF translation on NT.
 *      Return TRUE is successful.
 *
 **************************************/
 	return any_text_load(blob_id, database, transaction, file_name, FB_TRUE);
}


int API_ROUTINE BLOB_load(ISC_QUAD* blob_id,
						  FB_API_HANDLE database,
						  FB_API_HANDLE transaction,
						  const TEXT* file_name)
{
/**************************************
 *
 *	B L O B _ l o a d
 *
 **************************************
 *
 * Functional description
 *	Load a blob with the contents of a file.  Return TRUE is successful.
 *
 **************************************/
 	return any_text_load(blob_id, database, transaction, file_name, FB_FALSE);
}


FB_BLOB_STREAM API_ROUTINE Bopen(ISC_QUAD* blob_id,
								 FB_API_HANDLE database,
								 FB_API_HANDLE transaction,
								 const SCHAR* mode)
{
/**************************************
 *
 *	B o p e n
 *
 **************************************
 *
 * Functional description
 *	Initialize a blob-stream block.
 *
 **************************************/
	// bpb is irrelevant, not used.
	const USHORT bpb_length = 0;
	const UCHAR* bpb = NULL;

	FB_API_HANDLE blob = 0;
	ISC_STATUS_ARRAY status_vector;

	switch (*mode)
	{
	case 'w':
	case 'W':
		if (isc_create_blob2(status_vector, &database, &transaction, &blob, blob_id,
							 bpb_length, reinterpret_cast<const char*>(bpb)))
		{
			return NULL;
		}
		break;
	case 'r':
	case 'R':
		if (isc_open_blob2(status_vector, &database, &transaction, &blob, blob_id,
						   bpb_length, bpb))
		{
			return NULL;
		}
		break;
	default:
		return NULL;
	}

	FB_BLOB_STREAM blobStream = BLOB_open(blob, NULL, 0);

	if (*mode == 'w' || *mode == 'W')
	{
		blobStream->bstr_mode |= BSTR_output;
		blobStream->bstr_cnt = blobStream->bstr_length;
		blobStream->bstr_ptr = blobStream->bstr_buffer;
	}
	else
	{
		blobStream->bstr_cnt = 0;
		blobStream->bstr_mode |= BSTR_input;
	}

	return blobStream;
}


// CVC: This routine doesn't open a blob really!
FB_BLOB_STREAM API_ROUTINE BLOB_open(FB_API_HANDLE blob, SCHAR* buffer, int length)
{
/**************************************
 *
 *	B L O B _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Initialize a blob-stream block.
 *
 **************************************/
	if (!blob)
		return NULL;

	FB_BLOB_STREAM blobStream = (FB_BLOB_STREAM) gds__alloc((SLONG) sizeof(struct bstream));
	// FREE: This structure is freed by BLOB_close
	if (!blobStream)				// NOMEM:
		return NULL;

#ifdef DEBUG_gds__alloc
	// This structure is handed to the user process, we depend on the client
	// to call BLOB_close() for it to be freed.
	gds_alloc_flag_unfreed((void*) blobStream);
#endif

	blobStream->bstr_blob = blob;
	blobStream->bstr_length = length ? length : 512;
	blobStream->bstr_mode = 0;
	blobStream->bstr_cnt = 0;
	blobStream->bstr_ptr = 0;

	if (!(blobStream->bstr_buffer = buffer))
	{
		blobStream->bstr_buffer = (SCHAR*) gds__alloc((SLONG) (sizeof(SCHAR) * blobStream->bstr_length));
		// FREE: This structure is freed in BLOB_close()
		if (!blobStream->bstr_buffer)
		{
			// NOMEM:
			gds__free(blobStream);
			return NULL;
		}
#ifdef DEBUG_gds__alloc
		// This structure is handed to the user process, we depend on the client
		// to call BLOB_close() for it to be freed.
		gds_alloc_flag_unfreed((void*) blobStream->bstr_buffer);
#endif

		blobStream->bstr_mode |= BSTR_alloc;
	}

	return blobStream;
}


int API_ROUTINE BLOB_put(SCHAR x, FB_BLOB_STREAM blobStream)
{
/**************************************
 *
 *	B L O B _ p u t
 *
 **************************************
 *
 * Functional description
 *	Write a segment to a blob. First
 *	stick in the final character, then
 *	compute the length and send off the
 *	segment.  Finally, set up the buffer
 *	block and retun TRUE if all is well.
 *
 **************************************/
	if (!blobStream->bstr_buffer)
		return FALSE;

	*blobStream->bstr_ptr++ = (x & 0377);
	const USHORT l = (blobStream->bstr_ptr - blobStream->bstr_buffer);

	ISC_STATUS_ARRAY status_vector;
	if (isc_put_segment(status_vector, &blobStream->bstr_blob, l, blobStream->bstr_buffer))
	{
		return FALSE;
	}
	blobStream->bstr_cnt = blobStream->bstr_length;
	blobStream->bstr_ptr = blobStream->bstr_buffer;
	return TRUE;
}


int API_ROUTINE gds__thread_start(FPTR_INT_VOID_PTR* entrypoint,
								  void* arg,
								  int priority,
								  int /*flags*/,
								  void* thd_id)
{
/**************************************
 *
 *	g d s _ _ t h r e a d _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start a thread.
 *
 **************************************/

	int rc = 0;
	try
	{
		Thread::start((ThreadEntryPoint*) entrypoint, arg, priority, (Thread::Handle*) thd_id);
	}
	catch (const status_exception& status)
	{
		rc = status.value()[1];
	}
	return rc;
}

#if (defined SOLARIS ) || (defined __cplusplus)
} //extern "C" {
#endif


static void get_ods_version(CheckStatusWrapper* status, IAttachment* att,
	USHORT* ods_version, USHORT* ods_minor_version)
{
/**************************************
 *
 *	g e t _ o d s _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Obtain information about a on-disk structure (ods) versions
 *	of the database.
 *
 **************************************/
	UCHAR buffer[16];

	att->getInfo(status, sizeof(ods_info), ods_info, sizeof(buffer), buffer);

	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		return;

	const UCHAR* p = buffer;
	UCHAR item;

	while ((item = *p++) != isc_info_end)
	{
		const USHORT l = static_cast<USHORT>(gds__vax_integer(p, 2));
		p += 2;
		const USHORT n = static_cast<USHORT>(gds__vax_integer(p, l));
		p += l;
		switch (item)
		{
		case isc_info_ods_version:
			*ods_version = n;
			break;

		case isc_info_ods_minor_version:
			*ods_minor_version = n;
			break;

		default:
			(Arg::Gds(isc_random) << "Invalid info item").raise();
		}
	}
}


// CVC: I just made this alternative function to let the original unchanged.
// However, the original logic doesn't make sense.
static void isc_expand_dpb_internal(const UCHAR** dpb, SSHORT* dpb_size, ...)
{
/**************************************
 *
 *	i s c _ e x p a n d _ d p b _ i n t e r n a l
 *
 **************************************
 *
 * Functional description
 *	Extend a database parameter block dynamically
 *	to include runtime info.  Generated
 *	by gpre to provide host variable support for
 *	READY statement	options.
 *	This expects the list of variable args
 *	to be zero terminated.
 *
 *	Note: dpb_size is signed short only for compatibility
 *	with other calls (isc_attach_database) that take a dpb length.
 *
 * TMN: Note: According to Ann Harrison:
 * That routine should be deprecated.  It doesn't do what it
 * should, and does do things it shouldn't, and is harder to
 * use than the natural alternative.
 *
 * CVC: This alternative version returns either the original dpb or a
 * new one, but never overwrites the original dpb. More accurately, it's
 * clearer than the original function that really never modifies its source
 * dpb, but there appears to be a logic failure on an impossible path.
 * Also, since the change from UCHAR** to const UCHAR** is not transparent,
 * a new version was needed to make sure the old world wasn't broken.
 *
 **************************************/
	SSHORT	length;
	unsigned char* p = 0;
	const char* q;
	const unsigned char* uq;
	va_list	args;
	USHORT	type;
	UCHAR* new_dpb;

	// calculate length of database parameter block, setting initial length to include version

	SSHORT new_dpb_length;
	if (!*dpb || !(new_dpb_length = *dpb_size))
	{
		new_dpb_length = 1;
	}

	va_start(args, dpb_size);

	while (type = va_arg(args, int))
	{
		switch (type)
		{
		case isc_dpb_user_name:
		case isc_dpb_password:
		case isc_dpb_sql_role_name:
		case isc_dpb_lc_messages:
		case isc_dpb_lc_ctype:
		case isc_dpb_reserved:
			q = va_arg(args, char*);
			if (q)
			{
				length = static_cast<SSHORT>(strlen(q));
				new_dpb_length += 2 + length;
			}
			break;

		default:
			va_arg(args, int);
			break;
		}
	}
	va_end(args);

	// if items have been added, allocate space for the new dpb and copy the old one over

	if (new_dpb_length > *dpb_size)
	{
		// Note: gds__free done by GPRE generated code

		new_dpb = (UCHAR*) gds__alloc((SLONG)(sizeof(UCHAR) * new_dpb_length));
		p = new_dpb;
		// FREE: done by client process in GPRE generated code
		if (!new_dpb)
		{
			// NOMEM: don't trash existing dpb
			DEV_REPORT("isc_extend_dpb: out of memory");
			return;				// NOMEM: not really handled
		}

		uq = *dpb;
		for (length = *dpb_size; length; length--)
		{
			*p++ = *uq++;
		}

	}
	else
	{
		// CVC: Notice the initialization is: new_dpb_length = *dpb_size
		// Therefore, the worst case is new_dpb_length == *dpb_size
		// Also, if *dpb_size == 0, then new_dpb_length is set to 1,
		// so there will be again a bigger new buffer.
		// Hence, this else just means "we found nothing that we can
		// recognize in the variable params list to add and thus,
		// there's nothing to do". The case for new_dpb_length being less
		// than the original length simply can't happen. Therefore,
		// the input can be declared const.
		return;
	}

	if (!*dpb_size)
		*p++ = isc_dpb_version1;

	// copy in the new runtime items

	va_start(args, dpb_size);

	while (type = va_arg(args, int))
	{
		switch (type)
		{
		case isc_dpb_user_name:
		case isc_dpb_password:
		case isc_dpb_sql_role_name:
		case isc_dpb_lc_messages:
		case isc_dpb_lc_ctype:
		case isc_dpb_reserved:
		    q = va_arg(args, char*);
			if (q)
			{
				length = static_cast<SSHORT>(strlen(q));
				fb_assert(type <= CHAR_MAX);
				*p++ = (unsigned char) type;
				fb_assert(length <= CHAR_MAX);
				*p++ = (unsigned char) length;
				while (length--)
					*p++ = *q++;
			}
			break;

		default:
			va_arg(args, int);
			break;
		}
	}
	va_end(args);

	*dpb_size = p - new_dpb;
	*dpb = new_dpb;
}


// new utl
static void setTag(ClumpletWriter& dpb, UCHAR tag, const char* env, bool utf8)
{
	string value;

	if (fb_utils::readenv(env, value) && !dpb.find(tag))
	{
		if (utf8)
			ISC_systemToUtf8(value);

		dpb.insertString(tag, value);
	}
}

void setLogin(ClumpletWriter& dpb, bool spbFlag)
{
	const UCHAR address_path = spbFlag ? isc_spb_address_path : isc_dpb_address_path;
	const UCHAR trusted_auth = spbFlag ? isc_spb_trusted_auth : isc_dpb_trusted_auth;
	const UCHAR auth_block = spbFlag ? isc_spb_auth_block : isc_dpb_auth_block;
	const UCHAR utf8Tag = spbFlag ? isc_spb_utf8_filename : isc_dpb_utf8_filename;
	// username and password tags match for both SPB and DPB

	if (!(dpb.find(trusted_auth) || dpb.find(address_path) || dpb.find(auth_block)))
	{
		bool utf8 = dpb.find(utf8Tag);

		setTag(dpb, isc_dpb_user_name, ISC_USER, utf8);
		if (!dpb.find(isc_dpb_password_enc))
			setTag(dpb, isc_dpb_password, ISC_PASSWORD, utf8);

		if (spbFlag)
			setTag(dpb, isc_spb_expected_db, "FB_EXPECTED_DB", utf8);
	}
}


//
// circularAlloc()
//

#ifdef WIN_NT
#include <windows.h>
#endif

namespace {

class ThreadCleanup
{
public:
	static void add(FPTR_VOID_PTR cleanup, void* arg);
	static void remove(FPTR_VOID_PTR cleanup, void* arg);
	static void destructor(void*);

	static void assertNoCleanupChain()
	{
		fb_assert(!chain);
	}

private:
	FPTR_VOID_PTR function;
	void* argument;
	ThreadCleanup* next;

	static ThreadCleanup* chain;
	static GlobalPtr<Mutex> cleanupMutex;

	ThreadCleanup(FPTR_VOID_PTR cleanup, void* arg, ThreadCleanup* chain)
		: function(cleanup), argument(arg), next(chain) { }

	static void initThreadCleanup();
	static void finiThreadCleanup();

	static ThreadCleanup** findCleanup(FPTR_VOID_PTR cleanup, void* arg);
};

ThreadCleanup* ThreadCleanup::chain = NULL;
GlobalPtr<Mutex> ThreadCleanup::cleanupMutex;

#ifdef USE_POSIX_THREADS

pthread_key_t key;
pthread_once_t keyOnce = PTHREAD_ONCE_INIT;
bool keySet = false;

void makeKey()
{
	int err = pthread_key_create(&key, ThreadCleanup::destructor);
	if (err)
	{
		Firebird::system_call_failed("pthread_key_create", err);
	}
	keySet = true;
}

void ThreadCleanup::initThreadCleanup()
{
	int err = pthread_once(&keyOnce, makeKey);
	if (err)
	{
		Firebird::system_call_failed("pthread_once", err);
	}

	err = pthread_setspecific(key, &key);
	if (err)
	{
		Firebird::system_call_failed("pthread_setspecific", err);
	}
}

void ThreadCleanup::finiThreadCleanup()
{
	pthread_setspecific(key, NULL);
}


class FiniThreadCleanup
{
public:
	FiniThreadCleanup(Firebird::MemoryPool&)
	{ }

	~FiniThreadCleanup()
	{
		ThreadCleanup::assertNoCleanupChain();
		if (keySet)
		{
			int err = pthread_key_delete(key);
			if (err)
				Firebird::system_call_failed("pthread_key_delete", err);
		}
	}
};

Firebird::GlobalPtr<FiniThreadCleanup> thrCleanup;		// needed to call dtor

#endif // USE_POSIX_THREADS

#ifdef WIN_NT
void ThreadCleanup::initThreadCleanup()
{
}

void ThreadCleanup::finiThreadCleanup()
{
}
#endif // #ifdef WIN_NT

ThreadCleanup** ThreadCleanup::findCleanup(FPTR_VOID_PTR cleanup, void* arg)
{
	for (ThreadCleanup** ptr = &chain; *ptr; ptr = &((*ptr)->next))
	{
		if ((*ptr)->function == cleanup && (*ptr)->argument == arg)
		{
			return ptr;
		}
	}

	return NULL;
}

void ThreadCleanup::destructor(void*)
{
	MutexLockGuard guard(cleanupMutex, FB_FUNCTION);

	for (ThreadCleanup* ptr = chain; ptr; ptr = ptr->next)
	{
		ptr->function(ptr->argument);
	}

	finiThreadCleanup();
}

void ThreadCleanup::add(FPTR_VOID_PTR cleanup, void* arg)
{
	Firebird::MutexLockGuard guard(cleanupMutex, FB_FUNCTION);

	initThreadCleanup();

	if (findCleanup(cleanup, arg))
	{
		return;
	}

	chain = FB_NEW_POOL(*getDefaultMemoryPool()) ThreadCleanup(cleanup, arg, chain);
}

void ThreadCleanup::remove(FPTR_VOID_PTR cleanup, void* arg)
{
	MutexLockGuard guard(cleanupMutex, FB_FUNCTION);

	ThreadCleanup** ptr = findCleanup(cleanup, arg);
	if (!ptr)
	{
		return;
	}

	ThreadCleanup* toDelete = *ptr;
	*ptr = toDelete->next;
	delete toDelete;
}

class ThreadBuffer : public GlobalStorage
{
private:
	const static size_t BUFFER_SIZE = 8192;		// make it match with call stack limit == 2048
	char buffer[BUFFER_SIZE];
	char* buffer_ptr;

public:
	ThreadBuffer() : buffer_ptr(buffer) { }

	const char* alloc(const char* string, size_t length)
	{
		// if string is already in our buffer - return it
		// it was already saved in our buffer once
		if (string >= buffer && string < &buffer[BUFFER_SIZE])
			return string;

		// if string too long, truncate it
		if (length > BUFFER_SIZE / 4)
			length = BUFFER_SIZE / 4;

		// If there isn't any more room in the buffer, start at the beginning again
		if (buffer_ptr + length + 1 > buffer + BUFFER_SIZE)
			buffer_ptr = buffer;

		char* new_string = buffer_ptr;
		memcpy(new_string, string, length);
		new_string[length] = 0;
		buffer_ptr += length + 1;

		return new_string;
	}
};

TLS_DECLARE(ThreadBuffer*, threadBuffer);

void cleanupAllStrings(void*)
{
	///fprintf(stderr, "Cleanup is called\n");

	delete TLS_GET(threadBuffer);
	TLS_SET(threadBuffer, NULL);

	///fprintf(stderr, "Buffer removed\n");
}

ThreadBuffer* getThreadBuffer()
{
	ThreadBuffer* rc = TLS_GET(threadBuffer);
	if (!rc)
	{
		ThreadCleanup::add(cleanupAllStrings, NULL);
		rc = FB_NEW ThreadBuffer;
		TLS_SET(threadBuffer, rc);
	}

	return rc;
}

// Needed to call dtor
class Strings
{
public:
	Strings(MemoryPool&)
	{ }

	~Strings()
	{
		ThreadCleanup::remove(cleanupAllStrings, NULL);
	}
};
Firebird::GlobalPtr<Strings> cleanStrings;

const char* circularAlloc(const char* s, unsigned len)
{
	return getThreadBuffer()->alloc(s, len);
}

// CVC: Do not let "perm" be incremented before "trans", because it may lead to serious memory errors,
// since our code blindly passes the same vector twice.
void makePermanentVector(ISC_STATUS* perm, const ISC_STATUS* trans) throw()
{
	try
	{
		while (true)
		{
			const ISC_STATUS type = *perm++ = *trans++;

			switch (type)
			{
			case isc_arg_end:
				return;

			case isc_arg_cstring:
				{
					perm [-1] = isc_arg_string;
					const size_t len = *trans++;
					const char* temp = reinterpret_cast<char*>(*trans++);
					*perm++ = (ISC_STATUS)(IPTR) circularAlloc(temp, len);
				}
				break;

			case isc_arg_string:
			case isc_arg_interpreted:
			case isc_arg_sql_state:
				{
					const char* temp = reinterpret_cast<char*>(*trans++);
					const size_t len = strlen(temp);
					*perm++ = (ISC_STATUS)(IPTR) circularAlloc(temp, len);
				}
				break;

			default:
				*perm++ = *trans++;
				break;
			}
		}
	}
	catch (const system_call_failed& ex)
	{
		memcpy(perm, ex.value(), sizeof(ISC_STATUS_ARRAY));
	}
	catch (const BadAlloc&)
	{
		*perm++ = isc_arg_gds;
		*perm++ = isc_virmemexh;
		*perm++ = isc_arg_end;
	}
	catch (...)
	{
		*perm++ = isc_arg_gds;
		*perm++ = isc_random;
		*perm++ = isc_arg_string;
		*perm++ = (ISC_STATUS)(IPTR) "Unexpected exception in makePermanentVector()";
		*perm++ = isc_arg_end;
	}
}

} // anonymous namespace

void makePermanentVector(ISC_STATUS* v) throw()
{
	makePermanentVector(v, v);
}

#ifdef WIN_NT
namespace Why
{
	// This is called from ibinitdll.cpp:DllMain()
	void threadCleanup()
	{
		ThreadCleanup::destructor(NULL);
	}
}
#endif
