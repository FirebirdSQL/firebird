/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		err.cpp
 *	DESCRIPTION:	Error handlers
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
 * 27 Nov 2001  Ann W. Harrison - preserve string arguments in
 *              ERRD_post_warning
 *
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>

#include "../dsql/dsql.h"
#include "../dsql/sqlda.h"
#include "gen/iberror.h"
#include "../jrd/jrd.h"
#include "../dsql/errd_proto.h"

// This is the only one place in dsql code, where we need both
// dsql.h and err_proto.h.
// To avoid warnings, undefine some macro's here
//#undef BUGCHECK
//#undef IBERROR

#include "../jrd/err_proto.h"

// To ensure that suspicious macro's not used in this file,
// undefine them once more.
//#undef BUGCHECK
//#undef IBERROR

#include "../yvalve/gds_proto.h"
#include "../common/utils_proto.h"

using namespace Jrd;
using namespace Firebird;


static void internal_post(const Arg::StatusVector& v);

#ifdef DEV_BUILD
/**

 	ERRD_assert_msg

    @brief	Generate an assertion failure with a message


    @param msg
    @param file
    @param lineno

 **/
void ERRD_assert_msg(const char* msg, const char* file, ULONG lineno)
{

	char buffer[MAXPATHLEN + 100];

	fb_utils::snprintf(buffer, sizeof(buffer),
			"Assertion failure: %s File: %s Line: %ld\n",	//dev build
			(msg ? msg : ""), (file ? file : ""), lineno);
	ERRD_bugcheck(buffer);
}
#endif // DEV_BUILD


/**

 	ERRD_bugcheck

    @brief	Somebody has screwed up.  Bugcheck.


    @param text

 **/
void ERRD_bugcheck(const char* text)
{
	TEXT s[MAXPATHLEN + 120];
	fb_utils::snprintf(s, sizeof(s), "INTERNAL: %s", text);	// TXNN
	ERRD_error(s);
}


/**

 	ERRD_error

    @brief	This routine should only be used by fatal
 	error messages, those that cannot use the
 	normal error routines because something
 	is very badly wrong.  ERRD_post() should
  	be used by most error messages, especially
 	so that strings will be handled.


    @param code
    @param text

 **/
void ERRD_error(const char* text)
{
	TEXT s[MAXPATHLEN + 140];
	fb_utils::snprintf(s, sizeof(s), "** DSQL error: %s **\n", text);
	TRACE(s);

	status_exception::raise(Arg::Gds(isc_random) << Arg::Str(s));
}


/**

 ERRD_post_warning

    @brief      Post a warning to the current status vector.


    @param status
    @param

 **/
void ERRD_post_warning(const Firebird::Arg::StatusVector& v)
{
    fb_assert(v.value()[0] == isc_arg_warning);

	Jrd::FbStatusVector* status_vector = JRD_get_thread_data()->tdbb_status_vector;

	Arg::StatusVector cur(status_vector->getWarnings());
	cur << v;
	status_vector->setWarnings2(cur.length(), cur.value());
}


/**

 	ERRD_post

    @brief	Post an error, copying any potentially
 	transient data before we punt.


    @param statusVector
    @param

 **/
void ERRD_post(const Arg::StatusVector& v)
{
    fb_assert(v.value()[0] == isc_arg_gds);

	internal_post(v);
}


/**

 	internal_post

    @brief	Post an error, copying any potentially
 	transient data before we punt.


    @param tmp_status
    @param

 **/
static void internal_post(const Arg::StatusVector& v)
{
	// start building resulting vector
	Jrd::FbStatusVector* status_vector = JRD_get_thread_data()->tdbb_status_vector;
	Arg::StatusVector final(status_vector->getErrors());
	if (final.length() == 0)
	{
		// this is a blank status vector
		final << Arg::Gds(isc_dsql_error);
	}

	// check for duplicated error code
	if (fb_utils::subStatus(final.value(), final.length(), v.value(), v.length()) == ~0u)
	{
		// no dup - append new vector to old one
		final << v;
	}

	// save & punt
	status_vector->setErrors2(final.length(), final.value());
	ERRD_punt();
}


/**

 	ERRD_punt

    @brief	Error stuff has been copied to
 	status vector.  Now punt.



 **/
void ERRD_punt(const Jrd::FbStatusVector* local)
{
	thread_db* tdbb = JRD_get_thread_data();

	if (local)
	{
		fb_utils::copyStatus(tdbb->tdbb_status_vector, local);
	}

	// Give up whatever we were doing and return to the user.
	status_exception::raise(tdbb->tdbb_status_vector);
}
