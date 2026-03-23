/*
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
 *  The Original Code was created by Blas Rodriguez Somoza on 24-May-2004
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Blas Rodriguez Somoza
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */


#ifndef COMMON_STUFF_H
#define COMMON_STUFF_H

inline void add_byte(UCHAR* &blr, const int byte) noexcept
{
	*blr++ = (SCHAR) (byte);
}

inline void add_word(UCHAR* &blr, const int word) noexcept
{
	add_byte(blr, word);
	add_byte(blr, (word) >> 8);
}

inline void add_long(UCHAR* &blr, const SLONG lg) noexcept
{
	add_word(blr, lg);
	add_word(blr, (lg) >> 16);
}

inline void add_int64(UCHAR* &blr, const SINT64 i64) noexcept
{
	add_long(blr, i64);
	add_long(blr, (i64) >> 32);
}

inline void add_string(UCHAR* &blr, const TEXT* string) noexcept
{
	add_byte(blr, static_cast<int>(strlen(string)));
	while (*string)
		add_byte(blr, *string++);
}

#endif // COMMON_STUFF_H
