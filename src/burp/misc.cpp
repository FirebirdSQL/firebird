/*
 *	PROGRAM:	JRD Backup and Restore Program
 *	MODULE:		misc.cpp
 *	DESCRIPTION:	Miscellaneous useful routines
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
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "UNIXWARE" port
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include "../burp/burp.h"
#include "../burp/burp_proto.h"
#include "../burp/misc_proto.h"


// Since this code appears everywhere, it makes more sense to isolate it
// in a function visible to all gbak components.
// Given a request, if it's non-zero (compiled), deallocate it but
// without caring about a possible error.
void MISC_release_request_silent(Firebird::IRequest*& req_handle)
{
	if (req_handle)
	{
		req_handle->release();
		req_handle = nullptr;
	}
}


int MISC_symbol_length( const TEXT* symbol, ULONG size_len)
{
/**************************************
 *
 *	M I S C _ s y m b o l _ l e n g t h
 *
 **************************************
 *
 * Functional description
 * Compute length of null terminated symbol.
 *      CVC: This function should acknowledge embedded blanks.
 *
 **************************************/
	if (size_len < 2) {
		return 0;
	}

	--size_len;

	const TEXT* p = symbol;
	const TEXT* const q = p + size_len;

	while (*p && p < q) {  // find end of string (null or end).
		p++;
	}

	--p;

	while (p >= symbol && *p == ' ') {  // skip trailing blanks
		--p;
	}

	return p + 1 - symbol;
}


void MISC_terminate(const TEXT* from, TEXT* to, ULONG length, ULONG max_length)
{
/**************************************
 *
 *	M I S C _ t e r m i n a t e
 *
 **************************************
 *
 * Functional description
 *	Null-terminate a possibly non-
 *	null-terminated string with max
 *	buffer room.
 *
 **************************************/

	fb_assert(max_length != 0);
	if (length)
	{
		length = MIN(length, max_length - 1);
		do {
			*to++ = *from++;
		} while (--length);
		*to++ = '\0';
	}
	else
	{
		while (max_length-- && (*to++ = *from++));
		*--to = '\0';
	}
}
