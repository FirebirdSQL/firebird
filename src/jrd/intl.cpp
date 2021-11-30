/*************  history ************
*
*       COMPONENT: JRD  MODULE: INTL.CPP
*       generated by Marion V2.5     2/6/90
*       from dev              db        on 4-JAN-1995
*****************************************************************
*
*       PR	2002-06-02 Added ugly c hack in
*       intl_back_compat_alloc_func_lookup.
*       When someone has time we need to change the references to
*       return (void*) function to something more C++ like
*
*       42 4711 3 11 17  tamlin   2001
*       Added silly numbers before my name, and converted it to C++.
*
*       18850   daves   4-JAN-1995
*       Fix gds__alloc usage
*
*       18837   deej    31-DEC-1994
*       fixing up HARBOR_MERGE
*
*       18821   deej    27-DEC-1994
*       HARBOR MERGE
*
*       18789   jdavid  19-DEC-1994
*       Cast some functions
*
*       17508   jdavid  15-JUL-1994
*       Bring it up to date
*
*       17500   daves   13-JUL-1994
*       Bug 6645: Different calculation of partial keys
*
*       17202   katz    24-MAY-1994
*       PC_PLATFORM requires the .dll extension
*
*       17191   katz    23-MAY-1994
*       OS/2 requires the .dll extension
*
*       17180   katz    23-MAY-1994
*       Define location of DLL on OS/2
*
*       17149   katz    20-MAY-1994
*       In JRD, isc_arg_number arguments are SLONG's not int's
*
*       16633   daves   19-APR-1994
*       Bug 6202: International licensing uses INTERNATIONAL product code
*
*       16555   katz    17-APR-1994
*       The last argument of calls to ERR_post should be 0
*
*       16521   katz    14-APR-1994
*       Borland C needs a decorated symbol to lookup
*
*       16403   daves   8-APR-1994
*       Bug 6441: Emit an error whenever transliteration from ttype_binary attempted
*
*       16141   katz    28-MAR-1994
*       Don't declare return value from ISC_lookup_entrypoint as API_ROUTINE
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
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 *
*/


/*
 *      PROGRAM:        JRD Intl
 *      MODULE:         intl.cpp
 *      DESCRIPTION:    International text support routines
 *
 * copyright (c) 1992, 1993 by Borland International
 */

#include "firebird.h"
#include <string.h>
#include <stdio.h>
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/val.h"
#include "iberror.h"
#include "../jrd/intl.h"
#include "../jrd/intl_classes.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../intl/charsets.h"
#include "../intl/country_codes.h"
#include "../common/gdsassert.h"
#ifdef INTL_BUILTIN
#include "../intl/ld_proto.h"
#endif
#include "../jrd/cvt_proto.h"
#include "../common/cvt.h"
#include "../jrd/err_proto.h"
#include "../jrd/fun_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/intl_proto.h"
#include "../common/isc_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../common/intlobj_new.h"
#include "../jrd/Collation.h"
#include "../jrd/mov_proto.h"
#include "../jrd/IntlManager.h"
#include "../common/classes/init.h"

using namespace Jrd;
using namespace Firebird;

#define IS_TEXT(x)      (((x)->dsc_dtype == dtype_text)   ||\
			 ((x)->dsc_dtype == dtype_varying)||\
			 ((x)->dsc_dtype == dtype_cstring))


static bool allSpaces(CharSet*, const BYTE*, ULONG, ULONG);
static int blocking_ast_collation(void* ast_object);
static void pad_spaces(thread_db*, CHARSET_ID, BYTE *, ULONG);
static INTL_BOOL lookup_texttype(texttype* tt, const SubtypeInfo* info);

static GlobalPtr<Mutex> createCollationMtx;

// Classes and structures used internally to this file and intl implementation
class CharSetContainer
{
public:
	CharSetContainer(MemoryPool& p, USHORT cs_id, const SubtypeInfo* info);

	void release(thread_db* tdbb)
	{
		for (FB_SIZE_T i = 0; i < charset_collations.getCount(); i++)
		{
			if (charset_collations[i])
				charset_collations[i]->release(tdbb);
		}
	}

	void destroy(thread_db* tdbb)
	{
		cs->destroy();
		for (FB_SIZE_T i = 0; i < charset_collations.getCount(); i++)
		{
			if (charset_collations[i])
				charset_collations[i]->destroy(tdbb);
		}
	}

	CharSet* getCharSet() { return cs; }

	Collation* lookupCollation(thread_db* tdbb, USHORT tt_id);
	void unloadCollation(thread_db* tdbb, USHORT tt_id);

	CsConvert lookupConverter(thread_db* tdbb, CHARSET_ID to_cs);

	static CharSetContainer* lookupCharset(thread_db* tdbb, USHORT ttype);
	static Lock* createCollationLock(thread_db* tdbb, USHORT ttype, void* object = NULL);

private:
	static bool lookupInternalCharSet(USHORT id, SubtypeInfo* info);

private:
	Firebird::Array<Collation*> charset_collations;
	CharSet* cs;
};


CharSetContainer* CharSetContainer::lookupCharset(thread_db* tdbb, USHORT ttype)
{
/**************************************
 *
 *      l o o k u p C h a r s e t
 *
 **************************************
 *
 * Functional description
 *
 *      Lookup a character set descriptor.
 *
 *      First, search the appropriate vector that hangs
 *      off the dbb.  If not found, then call the lower
 *      level lookup routine to allocate it, or punt
 *		if we don't know about the charset.
 *
 * Returns:
 *      *charset
 *      <never>         - if error
 *
 **************************************/
	CharSetContainer* cs = NULL;

	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();
	fb_assert(attachment);

	USHORT id = TTYPE_TO_CHARSET(ttype);
	if (id == CS_dynamic)
		id = tdbb->getCharSet();

	if (id >= attachment->att_charsets.getCount())
		attachment->att_charsets.resize(id + 10);
	else
		cs = attachment->att_charsets[id];

	// allocate a new character set object if we couldn't find one.
	if (!cs)
	{
		SubtypeInfo info;

		if (lookupInternalCharSet(id, &info) || MET_get_char_coll_subtype_info(tdbb, id, &info))
		{
			attachment->att_charsets[id] = cs =
				FB_NEW_POOL(*attachment->att_pool) CharSetContainer(*attachment->att_pool, id, &info);
		}
		else
			ERR_post(Arg::Gds(isc_text_subtype) << Arg::Num(ttype));
	}

	return cs;
}


// Lookup a system character set without looking in the database.
bool CharSetContainer::lookupInternalCharSet(USHORT id, SubtypeInfo* info)
{
	if (id == CS_UTF16)
	{
		info->charsetName = "UTF16";
		return true;
	}

	if (id > ttype_last_internal)
		return false;

	// ASF: This linear lookup appears slow, but it should be cached per database so should not
	// cause performance problem.
	for (const IntlManager::CharSetDefinition* csDef = IntlManager::defaultCharSets;
		 csDef->name; ++csDef)
	{
		if (csDef->id != id)
			continue;

		for (const IntlManager::CollationDefinition* colDef = IntlManager::defaultCollations;
			 colDef->name; ++colDef)
		{
			if (colDef->charSetId == id && colDef->collationId == 0)
			{
				info->charsetName = csDef->name;
				info->collationName = colDef->name;
				info->attributes = colDef->attributes;
				info->ignoreAttributes = false;

				if (colDef->specificAttributes)
				{
					info->specificAttributes.push((const UCHAR*) colDef->specificAttributes,
						fb_strlen(colDef->specificAttributes));
				}

				return true;
			}
		}
	}

	return false;
}


Lock* CharSetContainer::createCollationLock(thread_db* tdbb, USHORT ttype, void* object)
{
/**************************************
 *
 *      c r e a t e C o l l a t i o n L o c k
 *
 **************************************
 *
 * Functional description
 *      Create a collation lock.
 *
 **************************************/
	// Could we have an AST on this lock? If yes, it will fail if we don't
	// have lck_object to it, so set ast routine to NULL for safety.

	Lock* lock = FB_NEW_RPT(*tdbb->getAttachment()->att_pool, 0)
		Lock(tdbb, sizeof(SLONG), LCK_tt_exist, object, (object ? blocking_ast_collation : NULL));
	lock->setKey(ttype);

	return lock;
}

CharSetContainer::CharSetContainer(MemoryPool& p, USHORT cs_id, const SubtypeInfo* info)
	: charset_collations(p),
	  cs(NULL)
{
	charset* csL = FB_NEW_POOL(p) charset;
	memset(csL, 0, sizeof(charset));

	if (IntlManager::lookupCharSet(info->charsetName.c_str(), csL) &&
		(csL->charset_flags & CHARSET_ASCII_BASED))
	{
		this->cs = CharSet::createInstance(p, cs_id, csL);
	}
	else
	{
		delete csL;
		ERR_post(Arg::Gds(isc_charset_not_installed) << Arg::Str(info->charsetName));
	}
}

CsConvert CharSetContainer::lookupConverter(thread_db* tdbb, CHARSET_ID toCsId)
{
	if (toCsId == CS_UTF16)
		return CsConvert(cs->getStruct(), NULL);

	CharSet* toCs = INTL_charset_lookup(tdbb, toCsId);
	if (cs->getId() == CS_UTF16)
		return CsConvert(NULL, toCs->getStruct());

	return CsConvert(cs->getStruct(), toCs->getStruct());
}

Collation* CharSetContainer::lookupCollation(thread_db* tdbb, USHORT tt_id)
{
	const USHORT id = TTYPE_TO_COLLATION(tt_id);

	if (id < charset_collations.getCount() && charset_collations[id] != NULL)
	{
		if (!charset_collations[id]->obsolete)
			return charset_collations[id];
	}

	CheckoutLockGuard guard(tdbb, createCollationMtx, FB_FUNCTION); // do we need it ?

	Collation* to_delete = NULL;

	if (id < charset_collations.getCount() && charset_collations[id] != NULL)
	{
		if (charset_collations[id]->obsolete)
		{
			// if obsolete collation is not used delete it immediately,
			// else wait until all references are released
			if (charset_collations[id]->useCount == 0)
			{
				charset_collations[id]->destroy(tdbb);
				delete charset_collations[id];
			}
			else
				to_delete = charset_collations[id];

			charset_collations[id] = NULL;
		}
		else
			return charset_collations[id];
	}

	SubtypeInfo info;
	if (MET_get_char_coll_subtype_info(tdbb, tt_id, &info))
	{
		CharSet* charset = INTL_charset_lookup(tdbb, TTYPE_TO_CHARSET(tt_id));

		if (TTYPE_TO_CHARSET(tt_id) != CS_METADATA)
		{
			Firebird::UCharBuffer specificAttributes;
			ULONG size = info.specificAttributes.getCount() * charset->maxBytesPerChar();

			size = INTL_convert_bytes(tdbb, TTYPE_TO_CHARSET(tt_id),
									  specificAttributes.getBuffer(size), size,
									  CS_METADATA, info.specificAttributes.begin(),
									  info.specificAttributes.getCount(), ERR_post);
			specificAttributes.shrink(size);
			info.specificAttributes = specificAttributes;
		}

		Attachment* const att = tdbb->getAttachment();
		texttype* tt = FB_NEW_POOL(*att->att_pool) texttype;
		memset(tt, 0, sizeof(texttype));

		if (!lookup_texttype(tt, &info))
		{
			delete tt;
			ERR_post(Arg::Gds(isc_collation_not_installed) << Arg::Str(info.collationName) <<
															  Arg::Str(info.charsetName));
		}

		if (charset_collations.getCount() <= id)
			charset_collations.grow(id + 1);

		fb_assert((tt->texttype_canonical_width == 0 && tt->texttype_fn_canonical == NULL) ||
				  (tt->texttype_canonical_width != 0 && tt->texttype_fn_canonical != NULL));

		if (tt->texttype_canonical_width == 0)
		{
			if (charset->isMultiByte())
				tt->texttype_canonical_width = sizeof(ULONG);	// UTF-32
			else
			{
				tt->texttype_canonical_width = charset->minBytesPerChar();
				// canonical is equal to string, then TEXTTYPE_DIRECT_MATCH can be turned on
				tt->texttype_flags |= TEXTTYPE_DIRECT_MATCH;
			}
		}

		charset_collations[id] = Collation::createInstance(*att->att_pool, tt_id, tt, info.attributes, charset);
		charset_collations[id]->name = info.collationName;

		// we don't need a lock in the charset
		if (id != 0)
		{
			Lock* lock = charset_collations[id]->existenceLock =
				CharSetContainer::createCollationLock(tdbb, tt_id, charset_collations[id]);

			fb_assert(charset_collations[id]->useCount == 0);
			fb_assert(!charset_collations[id]->obsolete);

			LCK_lock(tdbb, lock, LCK_SR, LCK_WAIT);

			// as we just obtained SR lock for new collation instance
			// we could safely delete obsolete instance
			if (to_delete)
			{
				to_delete->destroy(tdbb);
				delete to_delete;
			}
		}
	}
	else
	{
		if (to_delete)
		{
			LCK_lock(tdbb, to_delete->existenceLock, LCK_SR, LCK_WAIT);
			to_delete->destroy(tdbb);
			delete to_delete;
		}

		ERR_post(Arg::Gds(isc_text_subtype) << Arg::Num(tt_id));
	}

	return charset_collations[id];
}


void CharSetContainer::unloadCollation(thread_db* tdbb, USHORT tt_id)
{
	const USHORT id = TTYPE_TO_COLLATION(tt_id);
	fb_assert(id != 0);

	if (id < charset_collations.getCount() && charset_collations[id] != NULL)
	{
		if (charset_collations[id]->useCount != 0)
		{
			ERR_post(Arg::Gds(isc_no_meta_update) <<
					 Arg::Gds(isc_obj_in_use) << Arg::Str(charset_collations[id]->name));
		}

		fb_assert(charset_collations[id]->existenceLock);

		if (!charset_collations[id]->obsolete)
		{
			LCK_convert(tdbb, charset_collations[id]->existenceLock, LCK_EX, LCK_WAIT);
			charset_collations[id]->obsolete = true;
			LCK_release(tdbb, charset_collations[id]->existenceLock);
		}
	}
	else
	{
		// signal other processes collation is gone
		Lock* lock = CharSetContainer::createCollationLock(tdbb, tt_id);

		LCK_lock(tdbb, lock, LCK_EX, LCK_WAIT);
		LCK_release(tdbb, lock);

		delete lock;
	}
}


static INTL_BOOL lookup_texttype(texttype* tt, const SubtypeInfo* info)
{
	return IntlManager::lookupCollation(info->baseCollationName.c_str(), info->charsetName.c_str(),
		info->attributes, info->specificAttributes.begin(),
		info->specificAttributes.getCount(), info->ignoreAttributes, tt);
}


void Jrd::Attachment::releaseIntlObjects(thread_db* tdbb)
{
	for (FB_SIZE_T i = 0; i < att_charsets.getCount(); i++)
	{
		if (att_charsets[i])
			att_charsets[i]->release(tdbb);
	}
}


void Jrd::Attachment::destroyIntlObjects(thread_db* tdbb)
{
	for (FB_SIZE_T i = 0; i < att_charsets.getCount(); i++)
	{
		if (att_charsets[i])
		{
			att_charsets[i]->destroy(tdbb);
			att_charsets[i] = NULL;
		}
	}
}


void INTL_adjust_text_descriptor(thread_db* tdbb, dsc* desc)
{
/**************************************
 *
 *      I N T L _ a d j u s t _ t e x t _ d e s c r i p t o r
 *
 **************************************
 *
 * Functional description
 *      This function receives a text descriptor with
 *      dsc_length = numberOfCharacters * maxBytesPerChar
 *      and change dsc_length to number of bytes used by the string.
 *
 **************************************/
	if (desc->dsc_dtype == dtype_text)
	{
		SET_TDBB(tdbb);

		USHORT ttype = INTL_TTYPE(desc);

		CharSet* charSet = INTL_charset_lookup(tdbb, ttype);

		if (charSet->isMultiByte())
		{
			Firebird::HalfStaticArray<UCHAR, BUFFER_SMALL> buffer;

			desc->dsc_length = charSet->substring(TEXT_LEN(desc), desc->dsc_address,
				TEXT_LEN(desc), buffer.getBuffer(TEXT_LEN(desc)), 0,
				TEXT_LEN(desc) / charSet->maxBytesPerChar());
		}
	}
}


CHARSET_ID INTL_charset(thread_db* tdbb, USHORT ttype)
{
/**************************************
 *
 *      I N T L _ c h a r s e t
 *
 **************************************
 *
 * Functional description
 *      Return the character set ID for a piece of text.
 *
 **************************************/

	switch (ttype)
	{
	case ttype_none:
		return (CS_NONE);
	case ttype_ascii:
		return (CS_ASCII);
	case ttype_binary:
		return (CS_BINARY);
	case ttype_dynamic:
		SET_TDBB(tdbb);
		return (tdbb->getCharSet());
	default:
		return (TTYPE_TO_CHARSET(ttype));
	}
}


int INTL_compare(thread_db* tdbb, const dsc* pText1, const dsc* pText2, ErrorFunction err)
{
/**************************************
 *
 *      I N T L _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *      Compare two pieces of international text.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(pText1 != NULL);
	fb_assert(pText2 != NULL);
	fb_assert(IS_TEXT(pText1) && IS_TEXT(pText2));
	fb_assert(INTL_data_or_binary(pText1) || INTL_data_or_binary(pText2));
	fb_assert(err);

	// normal compare routine from CVT_compare
	// trailing spaces in strings are ignored for comparision

	UCHAR* p1;
	USHORT t1;
	ULONG length1 = CVT_get_string_ptr(pText1, &t1, &p1, NULL, 0, tdbb->getAttachment()->att_dec_status, err);

	UCHAR* p2;
	USHORT t2;
	ULONG length2 = CVT_get_string_ptr(pText2, &t2, &p2, NULL, 0, tdbb->getAttachment()->att_dec_status, err);

	// YYY - by SQL II compare_type must be explicit in the
	// SQL statement if there is any doubt

	USHORT compare_type = MAX(t1, t2);	// YYY
	HalfStaticArray<UCHAR, BUFFER_XLARGE> buffer;

	if (t1 != t2)
	{
		CHARSET_ID cs1 = INTL_charset(tdbb, t1);
		CHARSET_ID cs2 = INTL_charset(tdbb, t2);
		if (cs1 != cs2)
		{
			if (compare_type != t2)
			{
				// convert pText2 to pText1's type, if possible
				/* YYY - should failure to convert really return
				   an error here?
				   Support joining a 437 & Latin1 Column, and we
				   pick the compare_type as 437, still only want the
				   equal values....
				   But then, what about < operations, which make no
				   sense if the string cannot be expressed...
				 */

				UCHAR* p = buffer.getBuffer(INTL_convert_bytes(tdbb, cs1, NULL, 0,
					cs2, p2, length2, err));
				length2 = INTL_convert_bytes(tdbb, cs1, p, (ULONG) buffer.getCount(),
					cs2, p2, length2, err);
				p2 = p;
			}
			else
			{
				// convert pText1 to pText2's type, if possible

				UCHAR* p = buffer.getBuffer(INTL_convert_bytes(tdbb, cs2, NULL, 0,
					cs1, p1, length1, err));
				length1 = INTL_convert_bytes(tdbb, cs2, p, (ULONG) buffer.getCount(),
					cs1, p1, length1, err);
				p1 = p;
			}
		}
	}

	TextType* obj = INTL_texttype_lookup(tdbb, compare_type);

	return obj->compare(length1, p1, length2, p2);
}


ULONG INTL_convert_bytes(thread_db* tdbb,
						 CHARSET_ID dest_type,
						 BYTE* dest_ptr,
						 const ULONG dest_len,
						 CHARSET_ID src_type,
						 const BYTE* src_ptr,
						 const ULONG src_len,
						 ErrorFunction err)
{
/**************************************
 *
 *      I N T L _ c o n v e r t _ b y t e s
 *
 **************************************
 *
 * Functional description
 *      Given a string of bytes in one character set, convert it to another
 *      character set.
 *
 *      If (dest_ptr) is NULL, return the count of bytes needed to convert
 *      the string.  This does not guarantee the string can be converted,
 *      the purpose of this is to allocate a large enough buffer.
 *
 * RETURNS:
 *      Length of resulting string, in bytes.
 *      calls (err) if conversion error occurs.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(src_ptr != NULL);
	fb_assert(src_type != dest_type);
	fb_assert(err != NULL);

	dest_type = INTL_charset(tdbb, dest_type);
	src_type = INTL_charset(tdbb, src_type);

	const UCHAR* const start_dest_ptr = dest_ptr;

	if (dest_type == CS_BINARY || dest_type == CS_NONE ||
		src_type == CS_BINARY || src_type == CS_NONE)
	{
		// See if we just need a length estimate
		if (dest_ptr == NULL)
			return (src_len);

		if (dest_type != CS_BINARY && dest_type != CS_NONE)
		{
			CharSet* toCharSet = INTL_charset_lookup(tdbb, dest_type);

			if (!toCharSet->wellFormed(src_len, src_ptr))
				err(Arg::Gds(isc_malformed_string));
		}

		ULONG len = MIN(dest_len, src_len);
		if (len)
		{
			do {
				*dest_ptr++ = *src_ptr++;
			} while (--len);
		}

		// See if only space characters are remaining
		len = src_len - MIN(dest_len, src_len);
		if (len == 0 || allSpaces(INTL_charset_lookup(tdbb, src_type), src_ptr, len, 0))
			return dest_ptr - start_dest_ptr;

		err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
			Arg::Gds(isc_trunc_limits) << Arg::Num(dest_len) << Arg::Num(src_len));
	}
	else if (src_len)
	{
		// character sets are known to be different
		// Do we know an object from cs1 to cs2?

		CsConvert cs_obj = INTL_convert_lookup(tdbb, dest_type, src_type);
		return cs_obj.convert(src_len, src_ptr, dest_len, dest_ptr, NULL, true);
	}

	return 0;
}


CsConvert INTL_convert_lookup(thread_db* tdbb, CHARSET_ID to_cs, CHARSET_ID from_cs)
{
/**************************************
 *
 *      I N T L _ c o n v e r t _ l o o k u p
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	if (from_cs == CS_dynamic)
		from_cs = tdbb->getCharSet();

	if (to_cs == CS_dynamic)
		to_cs = tdbb->getCharSet();

	// Should from_cs == to_cs? be handled better? YYY

	fb_assert(from_cs != CS_dynamic);
	fb_assert(to_cs != CS_dynamic);

	CharSetContainer* charset = CharSetContainer::lookupCharset(tdbb, from_cs);

	return charset->lookupConverter(tdbb, to_cs);
}


int INTL_convert_string(dsc* to, const dsc* from, Firebird::Callbacks* cb)
{
/**************************************
 *
 *      I N T L _ c o n v e r t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *      Convert a string from one type to another
 *
 * RETURNS:
 *      0 if no error in conversion
 *      non-zero otherwise.
 *      CVC: Unfortunately, this function puts the source in the 2nd param,
 *      as opposed to the CVT routines, so const helps mitigating coding mistakes.
 *
 **************************************/

	// Note: This function is called from outside the engine as
	// well as inside - we likely can't get rid of JRD_get_thread_data here
	thread_db* tdbb = JRD_get_thread_data();
	if (tdbb == NULL)			// are we in the Engine?
		return (1);				// no, then can't access intl gah

	fb_assert(to != NULL);
	fb_assert(from != NULL);
	fb_assert(IS_TEXT(to) && IS_TEXT(from));

	const CHARSET_ID from_cs = INTL_charset(tdbb, INTL_TTYPE(from));
	const CHARSET_ID to_cs = INTL_charset(tdbb, INTL_TTYPE(to));

	UCHAR* p = to->dsc_address;
	const UCHAR* start = p;

	// Must convert dtype(cstring,text,vary) and ttype(ascii,binary,..intl..)

	UCHAR* from_ptr;
	USHORT from_type;
	const USHORT from_len = CVT_get_string_ptr(from, &from_type, &from_ptr, NULL, 0,
		tdbb->getAttachment()->att_dec_status, cb->err);

	const ULONG to_size = TEXT_LEN(to);
	ULONG from_fill, to_fill;

	const UCHAR* q = from_ptr;
	CharSet* const toCharSet = INTL_charset_lookup(tdbb, to_cs);
	ULONG toLength;

	switch (to->dsc_dtype)
	{
	case dtype_text:
		if (from_cs != to_cs && to_cs != CS_BINARY && to_cs != CS_NONE && from_cs != CS_NONE)
		{
			const ULONG to_len = INTL_convert_bytes(tdbb, to_cs, to->dsc_address, to_size,
										from_cs, from_ptr, from_len, cb->err);
			toLength = to_len;
			to_fill = to_size - to_len;
			from_fill = 0;		// Convert_bytes handles source truncation
			p += to_len;
		}
		else
		{
			// binary string can always be converted TO by byte-copy

			ULONG to_len = MIN(from_len, to_size);
			if (!toCharSet->wellFormed(to_len, q))
				cb->err(Arg::Gds(isc_malformed_string));
			toLength = to_len;
			from_fill = from_len - to_len;
			to_fill = to_size - to_len;
			if (to_len)
			{
				do
				{
					*p++ = *q++;
				} while (--to_len);
			}
		}

		if (to_fill > 0)
			pad_spaces(tdbb, to_cs, p, to_fill);
		break;

	case dtype_cstring:
		if (from_cs != to_cs && to_cs != CS_BINARY && to_cs != CS_NONE && from_cs != CS_NONE)
		{
			const ULONG to_len = INTL_convert_bytes(tdbb, to_cs, to->dsc_address, to_size,
										from_cs, from_ptr, from_len, cb->err);
			toLength = to_len;
			to->dsc_address[to_len] = 0;
			from_fill = 0;		// Convert_bytes handles source truncation
		}
		else
		{
			// binary string can always be converted TO by byte-copy

			ULONG to_len = MIN(from_len, to_size);
			if (!toCharSet->wellFormed(to_len, q))
				cb->err(Arg::Gds(isc_malformed_string));
			toLength = to_len;
			from_fill = from_len - to_len;
			if (to_len)
			{
				do
				{
					*p++ = *q++;
				} while (--to_len);
			}
			*p = 0;
		}
		break;

	case dtype_varying:
		if (from_cs != to_cs && to_cs != CS_BINARY && to_cs != CS_NONE && from_cs != CS_NONE)
		{
			UCHAR* vstr = reinterpret_cast<UCHAR*>(((vary*) to->dsc_address)->vary_string);
			start = vstr;
			ULONG to_len = INTL_convert_bytes(tdbb, to_cs, vstr,
										to_size, from_cs, from_ptr, from_len, cb->err);

			to_len = cb->validateLength(toCharSet, to_cs, to_len, vstr, to_size);

			toLength = to_len;
			((vary*) to->dsc_address)->vary_length = to_len;
			from_fill = 0;		// Convert_bytes handles source truncation
		}
		else
		{
			// binary string can always be converted TO by byte-copy
			ULONG to_len = MIN(from_len, to_size);
			if (!toCharSet->wellFormed(to_len, q))
				cb->err(Arg::Gds(isc_malformed_string));

			to_len = cb->validateLength(toCharSet, to_cs, to_len, q, to_size);

			toLength = to_len;
			from_fill = from_len - to_len;
			((vary*) p)->vary_length = to_len;
			start = p = reinterpret_cast<UCHAR*>(((vary*) p)->vary_string);
			if (to_len)
			{
				do
				{
					*p++ = *q++;
				} while (--to_len);
			}
		}
		break;
	}

	const ULONG src_len = toCharSet->length(toLength, start, false);
	const ULONG dest_len  = (ULONG) to_size / toCharSet->maxBytesPerChar();

	if (toCharSet->isMultiByte() && src_len > dest_len)
	{
		cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
			Arg::Gds(isc_trunc_limits) << Arg::Num(dest_len) << Arg::Num(src_len));
	}

	if (from_fill)
	{
		// Make sure remaining characters on From string are spaces
		if (!allSpaces(INTL_charset_lookup(tdbb, from_cs), q, from_fill, 0))
		{
			cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
				Arg::Gds(isc_trunc_limits) << Arg::Num(dest_len) << Arg::Num(src_len));
		}
	}

	return 0;
}


bool INTL_data(const dsc* pText)
{
/**************************************
 *
 *      I N T L _ d a t a
 *
 **************************************
 *
 * Functional description
 *      Given an input text descriptor,
 *      return true if the data pointed to represents
 *      international text (subject to user defined or non-binary
 *      collation or comparison).
 *
 **************************************/

	fb_assert(pText != NULL);

	if (!IS_TEXT(pText))
		return false;

	if (!INTERNAL_TTYPE(pText))
		return true;

	return false;
}

bool INTL_data_or_binary(const dsc* pText)
{
/**************************************
 *
 *      I N T L _ d a t a _ o r _ b i n a r y
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	return (INTL_data(pText) || (pText->dsc_ttype() == ttype_binary));
}


bool INTL_defined_type(thread_db* tdbb, USHORT t_type)
{
/**************************************
 *
 *      I N T L _ d e f i n e d _ t y p e
 *
 **************************************
 *
 * Functional description
 *      Is (t_type) a known text type?
 * Return:
 *      false   type is not defined.
 *      true    type is defined
 *
 * Note:
 *      Due to cleanup that must happen in DFW, this routine
 *      must return, and not call ERR directly.
 *
 **************************************/
	SET_TDBB(tdbb);

	try
	{
		ThreadStatusGuard local_status(tdbb);
		INTL_texttype_lookup(tdbb, t_type);
	}
	catch (...)
	{
		return false;
	}

	return true;
}


USHORT INTL_key_length(thread_db* tdbb, USHORT idxType, USHORT iLength)
{
/**************************************
 *
 *      I N T L _ k e y _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *      Given an index type, and a maximum length (iLength)
 *      return the length of the byte string key descriptor to
 *      use when collating text of this type.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(idxType >= idx_first_intl_string);

	const USHORT ttype = INTL_INDEX_TO_TEXT(idxType);

	USHORT key_length;
	if (ttype <= ttype_last_internal)
		key_length = iLength;
	else
	{
		TextType* obj = INTL_texttype_lookup(tdbb, ttype);
		key_length = obj->key_length(iLength);
	}

	// Validity checks on the computed key_length

	if (key_length > MAX_KEY)
		key_length = MAX_KEY;

	if (key_length < iLength)
		key_length = iLength;

	return (key_length);
}


CharSet* INTL_charset_lookup(thread_db* tdbb, USHORT parm1)
{
/**************************************
 *
 *      I N T L _ c h a r s e t _ l o o k u p
 *
 **************************************
 *
 * Functional description
 *
 *      Lookup a character set descriptor.
 *
 *      First, search the appropriate vector that hangs
 *      off the dbb.  If not found, then call the lower
 *      level lookup routine to allocate it, or punt
 *		if we don't know about the charset.
 *
 * Returns:
 *      *charset        - if no errors;
 *      <never>         - if error
 *
 **************************************/
	CharSetContainer *cs = CharSetContainer::lookupCharset(tdbb, parm1);
	return cs->getCharSet();
}


Collation* INTL_texttype_lookup(thread_db* tdbb, USHORT parm1)
{
/**************************************
 *
 *      I N T L _ t e x t t y p e _ l o o k u p
 *
 **************************************
 *
 * Functional description
 *
 *      Lookup either a character set descriptor or
 *      texttype descriptor object.
 *
 *      First, search the appropriate vector that hangs
 *      off the dbb.  If not found, then call the lower
 *      level lookup routine to find it in the libraries.
 *
 * Returns:
 *      *object         - if no errors;
 *      <never>         - if error
 *
 **************************************/
	SET_TDBB(tdbb);

	if (parm1 == ttype_dynamic)
		parm1 = MAP_CHARSET_TO_TTYPE(tdbb->getCharSet());

	CharSetContainer* csc = CharSetContainer::lookupCharset(tdbb, parm1);

	return csc->lookupCollation(tdbb, parm1);
}


void INTL_texttype_unload(thread_db* tdbb, USHORT ttype)
{
/**************************************
 *
 *      I N T L _ t e x t t y p e _ u n l o a d
 *
 **************************************
 *
 * Functional description
 *  Unload a collation from memory.
 *
 **************************************/
	SET_TDBB(tdbb);

	CharSetContainer* csc = CharSetContainer::lookupCharset(tdbb, ttype);
	if (csc)
		csc->unloadCollation(tdbb, ttype);
}


bool INTL_texttype_validate(Jrd::thread_db* tdbb, const SubtypeInfo* info)
{
/**************************************
 *
 *      I N T L _ t e x t t y p e _ v a l i d a t e
 *
 **************************************
 *
 * Functional description
 *  Check if collation attributes are valid.
 *
 **************************************/
	SET_TDBB(tdbb);

	texttype tt;
	memset(&tt, 0, sizeof(tt));

	bool ret = lookup_texttype(&tt, info);

	if (ret && tt.texttype_fn_destroy)
		tt.texttype_fn_destroy(&tt);

	return ret;
}


void INTL_pad_spaces(thread_db* tdbb, DSC* type, UCHAR* string, ULONG length)
{
/**************************************
 *
 *      I N T L _ p a d _ s p a c e s
 *
 **************************************
 *
 * Functional description
 *      Pad a buffer with spaces, using the character
 *      set's defined space character.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(type != NULL);
	fb_assert(IS_TEXT(type));
	fb_assert(string != NULL);

	const USHORT charset = INTL_charset(tdbb, type->dsc_ttype());
	pad_spaces(tdbb, charset, string, length);
}


USHORT INTL_string_to_key(thread_db* tdbb,
						USHORT idxType,
						const dsc* pString,
						DSC* pByte,
						USHORT key_type,
						bool trimTrailing)
{
/**************************************
 *
 *      I N T L _ s t r i n g _ t o _ k e y
 *
 **************************************
 *
 * Functional description
 *      Given an input string, convert it to a byte string
 *      that will collate naturally (byte order).
 *
 *      Return the length of the resulting byte string.
 *
 **************************************/

	SET_TDBB(tdbb);

	fb_assert(idxType >= idx_first_intl_string || idxType == idx_string ||
			  idxType == idx_byte_array || idxType == idx_metadata);
	fb_assert(pString != NULL);
	fb_assert(pByte != NULL);
	fb_assert(pString->dsc_address != NULL);
	fb_assert(pByte->dsc_address != NULL);
	fb_assert(pByte->dsc_dtype == dtype_text);

	UCHAR pad_char;
	USHORT ttype;

	switch (idxType)
	{
	case idx_string:
		pad_char = ' ';
		ttype = ttype_none;
		break;
	case idx_byte_array:
		pad_char = 0;
		ttype = ttype_binary;
		break;
	case idx_metadata:
		pad_char = ' ';
		ttype = ttype_metadata;
		break;
	default:
		ttype = INTL_INDEX_TO_TEXT(idxType);

		if (trimTrailing)
		{
			auto charSet = INTL_charset_lookup(tdbb, ttype);
			pad_char = *charSet->getSpace();
		}

		break;
	}

	// Make a string into the proper type of text

	MoveBuffer temp;
	UCHAR* src;
	USHORT len = MOV_make_string2(tdbb, pString, ttype, &src, temp);

	if (trimTrailing && len)
	{
		const UCHAR* end = src + len;

		while (--end >= src)
		{
			if (*end != pad_char)
				break;
		}

		len = end + 1 - src;
	}

	USHORT outlen;
	UCHAR* dest = pByte->dsc_address;
	USHORT destLen = pByte->dsc_length;

	switch (ttype)
	{
	case ttype_metadata:
	case ttype_binary:
	case ttype_ascii:
	case ttype_none:
		while (len-- && destLen-- > 0)
			*dest++ = *src++;
		outlen = dest - pByte->dsc_address;
		break;
	default:
		TextType* obj = INTL_texttype_lookup(tdbb, ttype);
		outlen = obj->string_to_key(len, src, pByte->dsc_length, dest, key_type);
		break;
	}

	return (outlen);
}


static bool allSpaces(CharSet* charSet, const BYTE* ptr, ULONG len, ULONG offset)
{
/**************************************
 *
 *      a l l _ s p a c e s
 *
 **************************************
 *
 * Functional description
 *      determine if the string at ptr[offset] ... ptr[len] is entirely
 *      spaces, as per the space definition of (charset).
 *      The binary representation of a Space is character-set dependent.
 *      (0x20 for Ascii, 0x0020 for Unicode, 0x20 for SJIS, but must watch for
 *      0x??20, which is NOT a space.
 **************************************/
	fb_assert(ptr != NULL);

	// We are assuming offset points to the first byte which was not
	// consumed in a conversion.  And that offset is pointing
	// to a character boundary

	// Single-octet character sets are optimized here

	if (charSet->getSpaceLength() == 1)
	{
		const BYTE* p = &ptr[offset];
		const BYTE* const end = &ptr[len];
		while (p < end)
		{
			if (*p++ != *charSet->getSpace())
				return false;
		}
	}
	else
	{
		const BYTE* p = &ptr[offset];
		const BYTE* const end = &ptr[len];
		const unsigned char* space = charSet->getSpace();
		const unsigned char* const end_space = &space[charSet->getSpaceLength()];
		while (p < end)
		{
			space = charSet->getSpace();
			while (p < end && space < end_space)
			{
				if (*p++ != *space++)
					return false;
			}
		}
	}

	return true;
}


static int blocking_ast_collation(void* ast_object)
{
/**************************************
 *
 *      b l o c k i n g _ a s t _ c o l l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Someone is trying to drop a collation. If there
 *      are outstanding interests in the existence of
 *      the collation then just mark as blocking and return.
 *      Otherwise, mark the collation as obsolete
 *      and release the collation existence lock.
 *
 **************************************/
	Collation* const tt = static_cast<Collation*>(ast_object);

	try
	{
		Database* const dbb = tt->existenceLock->lck_dbb;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, tt->existenceLock);

		tt->obsolete = true;
		LCK_release(tdbb, tt->existenceLock);
	}
	catch (const Firebird::Exception&)
	{} // no-op


	return 0;
}


static void pad_spaces(thread_db* tdbb, CHARSET_ID charset, BYTE* ptr, ULONG len)
{								/* byte count */
/**************************************
 *
 *      p a d  _ s p a c e s
 *
 **************************************
 *
 * Functional description
 *      Pad a buffer with the character set defined space character.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(ptr != NULL);

	CharSet* obj = INTL_charset_lookup(tdbb, charset);

	// Single-octet character sets are optimized here
	if (obj->getSpaceLength() == 1)
	{
		const BYTE* const end = &ptr[len];
		while (ptr < end)
			*ptr++ = *obj->getSpace();
	}
	else
	{
		const BYTE* const end = &ptr[len];
		const UCHAR* space = obj->getSpace();
		const UCHAR* const end_space = &space[obj->getSpaceLength()];
		while (ptr < end)
		{
			space = obj->getSpace();
			while (ptr < end && space < end_space) {
				*ptr++ = *space++;
			}
			// This fb_assert is checking that we didn't have a buffer-end
			// in the middle of a space character
			fb_assert(!(ptr == end) || (space == end_space));
		}
	}
}
