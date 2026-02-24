/*
 *	PROGRAM:	Interactive SQL utility
 *	MODULE:		isql.h
 *	DESCRIPTION:	Component wide include file
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
 * Revision 1.2  2000/11/18 16:49:24  fsg
 * Increased PRINT_BUFFER_LENGTH to 2048 to show larger plans
 * Fixed Bug #122563 in extract.e get_procedure_args
 * Apparently this has to be done in show.e also,
 * but that is for another day :-)
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
 */

#ifndef ISQL_ISQL_H
#define ISQL_ISQL_H

#include "../jrd/flags.h"
#include "../jrd/constants.h"
#include "../jrd/intl.h"
#include <stdio.h>
#include <stdlib.h>
#include <firebird/Interface.h>
#include "firebird/impl/msg_helper.h"

// Define lengths used in isql.e

inline constexpr int PRINT_BUFFER_LENGTH	= 1024;
inline constexpr int MAXTERM_SIZE			= 32;	// SQL termination character
inline constexpr int USER_LENGTH 			= 128;
inline constexpr int PASSWORD_LENGTH		= 8192;
inline constexpr int ROLE_LENGTH			= 128;

/* these constants are purely idiotic; there's no point in having
   a predefined constant with no meaning... */

inline constexpr int BUFFER_LENGTH128	= 128;
inline constexpr int BUFFER_LENGTH256	= 256;
inline constexpr int BUFFER_LENGTH400	= 400;
inline constexpr int BUFFER_LENGTH512	= 512;
inline constexpr int BUFFER_LENGTH60	= 60;
inline constexpr int BUFFER_LENGTH180	= 180;

// Define the possible states of processing commands

enum processing_state {
	FOUND_EOF   =   EOF,
	CONT		=	0,
	EXIT		=	1,
	BACKOUT		=	2,
	ps_ERR		=	3,
	END			=	4,
	SKIP		=	5,
	FAIL		=	6,
	EXTRACT		=	7,
	EXTRACTALL	=	8,
	FETCH		=	9,
	OBJECT_NOT_FOUND = 10,
	TRUNCATED = 11
};

// Which blob subtypes to print

inline constexpr int ALL_BLOBS	= -2;
inline constexpr int NO_BLOBS	= -1;

// Flag to decode all vs sql only objects
enum LegacyTables
{
	SQL_objects,
	ALL_objects
};

inline constexpr size_t QUOTED_NAME_SIZE = MAX_SQL_IDENTIFIER_SIZE + 2 /* quotes */;

inline constexpr size_t CHARSET_COLLATE_SIZE	=
	(MAX_SQL_IDENTIFIER_LEN + 2 /* quotes */) * 2 +	// charset and collate names
	14 +	// CHARACTER SET
	9 +		// NOT NULL
	8 +		// COLLATE
	30 +	// extra space
	1;		// null terminator

static inline constexpr const char* DEFTERM = ";";
inline constexpr unsigned NULL_DISP_LEN = 6;


// Error codes

inline constexpr int MSG_LENGTH = 1024;
inline constexpr int ISQL_MSG_FAC = FB_IMPL_MSG_FACILITY_ISQL;

#define FB_IMPL_MSG_NO_SYMBOL(facility, number, text)

#define FB_IMPL_MSG_SYMBOL(facility, number, symbol, text) \
	inline constexpr int symbol = number;

#define FB_IMPL_MSG(facility, number, symbol, sqlCode, sqlClass, sqlSubClass, text) \
	FB_IMPL_MSG_SYMBOL(facility, number, symbol, text)

#include "firebird/impl/msg/isql.h"

#undef FB_IMPL_MSG_NO_SYMBOL
#undef FB_IMPL_MSG_SYMBOL
#undef FB_IMPL_MSG


// Initialize types

struct sqltypes
{
	int type;
	SCHAR type_name[QUOTED_NAME_SIZE];
};

static inline constexpr sqltypes Column_types[] = {
	{blr_short, "SMALLINT"},
	{blr_long, "INTEGER"},
	{blr_quad, "QUAD"},
	{blr_float, "FLOAT"},
	{blr_text, "CHAR"},
	{blr_double, "DOUBLE PRECISION"},
	{blr_varying, "VARCHAR"},
	{blr_cstring, "CSTRING"},
	{blr_blob_id, "BLOB_ID"},
	{blr_blob, "BLOB"},
	{blr_sql_time, "TIME"},
	{blr_sql_date, "DATE"},
	{blr_timestamp, "TIMESTAMP"},
	{blr_int64, "BIGINT"},
	{blr_bool, "BOOLEAN"},
	{blr_dec64, "DECFLOAT(16)"},
	{blr_dec128, "DECFLOAT(34)"},
	{blr_int128, "INT128"},
	{blr_sql_time_tz, "TIME WITH TIME ZONE"},
	{blr_timestamp_tz, "TIMESTAMP WITH TIME ZONE"},
	{blr_ex_time_tz, "TIME WITH TIME ZONE"},
	{blr_ex_timestamp_tz, "TIMESTAMP WITH TIME ZONE"},
	{0, ""}
};

// Integral subtypes

inline constexpr int MAX_INTSUBTYPES = 2;

static inline constexpr const SCHAR* Integral_subtypes[] = {
	"UNKNOWN",					// Defined type, keyword
	"NUMERIC",					// NUMERIC, keyword
	"DECIMAL"					// DECIMAL, keyword
};

// Text subtypes

inline constexpr int MAX_TEXTSUBTYPES = 1;

static inline constexpr const SCHAR* Text_subtypes[] = {
	"CHAR",
	"BINARY"
};

// Varying subtypes

inline constexpr int MAX_VARYINGSUBTYPES = 1;

static inline constexpr const SCHAR* Varying_subtypes[] = {
	"VARCHAR",
	"VARBINARY"
};

// Blob subtypes

inline constexpr int MAX_BLOBSUBTYPES	= 8;

static inline constexpr const SCHAR* Sub_types[] = {
	"BINARY",					// keyword
	"TEXT",						// keyword
	"BLR",						// keyword
	"ACL",						// keyword
	"RANGES",					// keyword
	"SUMMARY",					// keyword
	"FORMAT",					// keyword
	"TRANSACTION_DESCRIPTION",	// keyword
	"EXTERNAL_FILE_DESCRIPTION"	// keyword
};

/* CVC: Notice that
BY REFERENCE is the default for scalars and can't be specified explicitly;
BY VMS_DESCRIPTOR is known simply as BY DESCRIPTOR and works for FB1;
BY ISC_DESCRIPTOR is the default for BLOBs and can't be used explicitly;
BY SCALAR_ARRAY_DESCRIPTOR is supported in FB2 as BY SCALAR_ARRAY, since
the server has already the capability to deliver arrays to UDFs;
BY REFERENCE_WITH_NULL his supported in FB2 to be able to signal SQL NULL
in input parameters.
The names mentioned here are documented in jrd/types.h. */

inline constexpr int MAX_UDFPARAM_TYPES = 6;

static inline constexpr const char* UDF_param_types[] = {
	" BY VALUE",			// keyword
	"",						// BY REFERENCE
	" BY DESCRIPTOR",		// keyword in FB, internally VMS descriptor
	"",						// BY ISC_DESCRIPTOR => BLOB
	" BY SCALAR_ARRAY",		// keyword in FB v2
	" NULL",				// BY REFERENCE WITH NULL, but only appends NULL to the type
	" ERROR-type-unknown"
};

class IsqlGlobals
{
public:
	FILE* Out;
	FILE* Errfp;
	SCHAR global_Db_name[MAXPATHLEN];
	SCHAR global_Target_db[MAXPATHLEN];
	SCHAR global_Term[MAXTERM_SIZE];
	size_t Termlen;
	SCHAR User[128];
	SCHAR Role[256];
	SCHAR SearchPath[512];
	USHORT SQL_dialect;
	USHORT db_SQL_dialect;
	// from isql.epp
	USHORT major_ods;
	USHORT minor_ods;
	CSetId att_charset;
	Firebird::IDecFloat16* df16;
	Firebird::IDecFloat34* df34;
	Firebird::IInt128* i128;
	void printf(const char* buffer, ...);
	void prints(const char* buffer);

	IsqlGlobals();
};

extern IsqlGlobals isqlGlob;

static inline constexpr const char* SCRATCH = "fb_query_";

inline void STDERROUT(const char* st, bool cr = true)
{
	fprintf (isqlGlob.Errfp, "%s", st);
	if (cr)
		fprintf (isqlGlob.Errfp, "\n");
	fflush (isqlGlob.Errfp);
}

#ifdef DEBUG_GDS_ALLOC
#define ISQL_ALLOC(x)     gds__alloc_debug(x, __FILE__, __LINE__)
#else
#define ISQL_ALLOC(x)     gds__alloc(x)
#endif
#define ISQL_FREE(x)     {gds__free(x); x = NULL;}

static inline constexpr const char* NEWLINE = "\n";
static inline constexpr const char* TAB_AS_SPACES = "        ";

inline constexpr char BLANK		= '\040';
inline constexpr char DBL_QUOTE	= '\042';
inline constexpr char SINGLE_QUOTE	= '\'';

struct IsqlVar
{
	const char* field;
	const char* relation;
	const char* owner;
	const char* alias;
	int subType, scale;
	unsigned type, length;
	CSetId charSet;
	bool nullable;
	short* nullInd;

	union TypeMix
	{
		ISC_TIMESTAMP* asDateTime;
		ISC_TIMESTAMP_TZ* asDateTimeTz;
		ISC_TIMESTAMP_TZ_EX* asDateTimeTzEx;
		ISC_TIME* asTime;
		ISC_TIME_TZ* asTimeTz;
		ISC_TIME_TZ_EX* asTimeTzEx;
		ISC_DATE* asDate;
		SSHORT* asSmallint;
		SLONG* asInteger;
		SINT64* asBigint;
		float* asFloat;
		double* asDouble;
		FB_BOOLEAN* asBoolean;
		ISC_QUAD* blobid;
		vary* asVary;
		char* asChar;
		FB_DEC16* asDec16;
		FB_DEC34* asDec34;
		FB_I128* asInt128;
		void* setPtr;
	};
	TypeMix value;
};

class IsqlWireStats
{
public:
	explicit IsqlWireStats(Firebird::IAttachment* att) noexcept :
		m_att(att)
	{}

	bool print(bool initial);
	bool get(bool initial);

private:

	Firebird::IAttachment* m_att;
	FB_UINT64 m_snd_packets = 0;
	FB_UINT64 m_rcv_packets = 0;
	FB_UINT64 m_out_packets = 0;
	FB_UINT64 m_in_packets = 0;
	FB_UINT64 m_snd_bytes = 0;
	FB_UINT64 m_rcv_bytes = 0;
	FB_UINT64 m_out_bytes = 0;
	FB_UINT64 m_in_bytes = 0;
	FB_UINT64 m_roundtrips = 0;
};

#endif // ISQL_ISQL_H
