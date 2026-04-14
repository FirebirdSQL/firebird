/*
 *	PROGRAM:	JRD International support
 *	MODULE:		intl.h
 *	DESCRIPTION:	International text handling definitions
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

#ifndef JRD_INTL_H
#define JRD_INTL_H

// Maps a Character_set_id & collation_id to a text_type (driver ID)

struct TTypeId;
struct CSetId;
struct CollId;

struct IdStorage
{
	constexpr explicit IdStorage(USHORT id) : val(id) { }

	constexpr operator USHORT() const { return val; }
	bool operator==(const IdStorage& id) const { return val == id.val; }
	bool operator!=(const IdStorage& id) const { return val != id.val; }

private:
	USHORT val;
};

struct TTypeId : public IdStorage
{
	TTypeId() : IdStorage(0) { }
	explicit TTypeId(USHORT id) : IdStorage(id) { }
	constexpr TTypeId(CSetId id);
	TTypeId(CSetId cs, CollId col);
};

struct CSetId : public IdStorage
{
	CSetId() : IdStorage(0) { }
	constexpr explicit CSetId(USHORT id) : IdStorage(id & 0xFF) { }
	constexpr CSetId(TTypeId tt) : IdStorage(USHORT(tt) & 0xFF) { }
};

struct CollId : public IdStorage
{
	CollId() : IdStorage(0) { }
	constexpr explicit CollId(USHORT id) : IdStorage(id) { }
	constexpr CollId(TTypeId tt) : IdStorage(USHORT(tt) >> 8) { }
};

inline TTypeId::TTypeId(CSetId cs, CollId col)
	: IdStorage((USHORT(col) << 8) | (USHORT(cs) & 0xFF))
{ }

inline constexpr TTypeId::TTypeId(CSetId cs)
	: IdStorage(USHORT(cs) & 0xFF)
{ }

#include "../intl/charsets.h"

inline const BYTE ASCII_SPACE			= 32;			// ASCII code for space

/*
 *  Default character set name for specification of COLLATE clause without
 *  a CHARACTER SET clause.
 *
 *  NATIONAL_CHARACTER_SET is used for SQL's NATIONAL character type.
 */
#define DEFAULT_CHARACTER_SET_NAME	"ISO8859_1"
#define NATIONAL_CHARACTER_SET		DEFAULT_CHARACTER_SET_NAME

#define DEFAULT_DB_CHARACTER_SET_NAME	"SYSTEM.NONE"

// Character Set used for system metadata information

#define CS_METADATA			CS_UTF8	// metadata charset

// text type definitions

#define ttype_none				TTypeId(CS_NONE)		// 0
#define ttype_binary			TTypeId(CS_BINARY)		// 1
#define ttype_ascii				TTypeId(CS_ASCII)		// 2
#define ttype_utf8				TTypeId(CS_UTF8)		// 4
#define ttype_last_internal		ttype_utf8				// 4

#define ttype_dynamic			TTypeId(CS_dynamic)		// use att_charset

#define ttype_sort_key			ttype_binary
#define	ttype_metadata			ttype_utf8

// Note:
// changing the value of ttype_metadata is an ODS System Metadata change
// changing the value of CS_METADATA    is an ODS System Metadata change



#define	COLLATE_NONE			CollId(0)	// No special collation, use codepoint order

#define INTL_GET_CHARSET(dsc)	((dsc)->getCharSet())
#define INTL_GET_COLLATE(dsc)	((dsc)->getCollation())


// Define tests for international data

#define	INTL_TTYPE(desc)		((desc)->getTextType())

#define	INTL_SET_TTYPE(desc, a)	((desc)->setTextType((a)))

#define INTERNAL_TTYPE(d)	(((USHORT)((d)->getTextType())) <= ttype_last_internal)

#define IS_INTL_DATA(d)		((d)->dsc_dtype <= dtype_any_text &&    \
				 (((USHORT)((d)->getTextType())) > ttype_last_internal))

#define INTL_DYNAMIC_CHARSET(desc)	(INTL_GET_CHARSET(desc) == CS_dynamic)



/*
 * There are several ways text types are used internally to Firebird
 *  1) As a CHARACTER_SET_ID & COLLATION_ID pair (in metadata).
 *  2) As a CHARACTER_SET_ID (when collation isn't relevent, like UDF parms)
 *  3) As an index type - (btr.h)
 *  4) As a driver ID (used to lookup the code which implements the locale)
 *     This is also known as dsc::getTextType() (aka text subtype).
 *
 * In Descriptors (DSC) the data is encoded as:
 *	dsc_charset	overloaded into dsc_scale
 *	dsc_collate	overloaded into dsc_sub_type
 *
 * Index types are converted to driver ID's via INTL_INDEX_TYPE
 *
 */

/* There must be a 1-1 mapping between index types and International text
 * subtypes -
 * Index-to-subtype: to compute a KEY from a Text string we must know both
 *	the TEXT format and the COLLATE routine to use (eg: the subtype info).
 * 	We need the text-format as the datavalue for key creation may not
 * 	match that needed for the index.
 * Subtype-to-index: When creating indices, they are assigned an
 *	Index type, which is derived from the datatype of the target.
 *
 */
#define INTL_INDEX_TO_TEXT(idxType) TTypeId((USHORT)((idxType) - idx_offset_intl_range))

// Maps a text_type to an index ID
#define INTL_TEXT_TO_INDEX(tType)   ((USHORT)((tType)   + idx_offset_intl_range))

#define MAP_CHARSET_TO_TTYPE(cs)	(cs & 0x00FF)

#define	INTL_RES_TTYPE(desc)	(INTL_DYNAMIC_CHARSET(desc) ?\
			MAP_CHARSET_TO_TTYPE(tdbb->getCharSet()) :\
		 	(desc)->getTextType())

#define INTL_INDEX_TYPE(desc)	INTL_TEXT_TO_INDEX (INTL_RES_TTYPE (desc))

#endif	// JRD_INTL_H
