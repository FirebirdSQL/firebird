/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		err.cpp
 *	DESCRIPTION:	Bug check routine
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
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include "gen/iberror.h"
#include <errno.h>
#include "../jrd/jrd.h"
#include "../jrd/os/pio.h"
#include "../jrd/val.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/rse.h"
#include "../jrd/tra.h"
#include "../jrd/cch_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/err_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../common/config/config.h"
#include "../common/utils_proto.h"

using namespace Jrd;
using namespace Firebird;

//#define JRD_FAILURE_SPACE	2048
//#define JRD_FAILURE_UNKNOWN	"<UNKNOWN>"	// Used when buffer fails


static void internal_error(ISC_STATUS status, int number, const TEXT* file = NULL, int line = 0);
static void post_nothrow(const unsigned lenToAdd, const ISC_STATUS* toAdd, FbStatusVector* statusVector);


void ERR_bugcheck(int number, const TEXT* file, int line)
{
/**************************************
 *
 *	E R R _ b u g c h e c k
 *
 **************************************
 *
 * Functional description
 *	Things seem to be going poorly today.
 *
 **************************************/
	thread_db* const tdbb = JRD_get_thread_data();
	Database* const dbb = tdbb->getDatabase();

	dbb->dbb_flags |= DBB_bugcheck;
	CCH_shutdown(tdbb);

	internal_error(isc_bug_check, number, file, line);
}


void ERR_bugcheck_msg(const TEXT* msg)
{
/**************************************
 *
 *	E R R _ b u g c h e c k _ m s g
 *
 **************************************
 *
 * Functional description
 *	Things seem to be going poorly today.
 *
 **************************************/
	thread_db* const tdbb = JRD_get_thread_data();
	Database* const dbb = tdbb->getDatabase();

	dbb->dbb_flags |= DBB_bugcheck;
	CCH_shutdown(tdbb);

	ERR_post(Arg::Gds(isc_bug_check) << Arg::Str(msg));
}


void ERR_soft_bugcheck(int number, const TEXT* file, int line)
{
/**************************************
 *
 *	E R R _ s o f t _ b u g c h e c k
 *
 **************************************
 *
 * Functional description
 *	Things seem to be going poorly today.
 *  Use this function instead of ERR_bugcheck in bugcheck conditions that do not affect
 *  whole server/database/attachment state.
 *
 **************************************/

	fb_assert(false);
	internal_error(isc_bug_check, number, file, line);
}


void ERR_corrupt(int number)
{
/**************************************
 *
 *	E R R _ c o r r u p t
 *
 **************************************
 *
 * Functional description
 *	Things seem to be going poorly today.
 *
 **************************************/

	internal_error(isc_db_corrupt, number);
}


void ERR_error(int number)
{
/**************************************
 *
 *	E R R _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Post a user-level error.  This is a temporary mechanism
 *	that will eventually disappear.
 *
 **************************************/
	TEXT errmsg[MAX_ERRMSG_LEN + 1];

	if (gds__msg_lookup(0, JRD_BUGCHK, number, sizeof(errmsg), errmsg, NULL) < 1)
		sprintf(errmsg, "error code %d", number);

	ERR_post(Arg::Gds(isc_random) << Arg::Str(errmsg));
}


void ERR_log(int facility, int number, const TEXT* message)
{
/**************************************
 *
 *	E R R _ l o g
 *
 **************************************
 *
 * Functional description
 *	Log a message to the firebird.log
 *
 **************************************/
	TEXT errmsg[MAX_ERRMSG_LEN + 1];
	thread_db* tdbb = JRD_get_thread_data();

	if (message)
		fb_utils::copy_terminate(errmsg, message, sizeof(errmsg));
	else if (gds__msg_lookup(0, facility, number, sizeof(errmsg), errmsg, NULL) < 1)
		strcpy(errmsg, "Internal error code");

	const size_t len = strlen(errmsg);
	fb_utils::snprintf(errmsg + len, sizeof(errmsg) - len, " (%d)", number);

	gds__log("Database: %s\n\t%s", (tdbb && tdbb->getAttachment()) ?
		tdbb->getAttachment()->att_filename.c_str() : "", errmsg);
}


void ERR_post_warning(const Arg::StatusVector& v)
{
/**************************************
 *
 *      E R R _ p o s t _ w a r n i n g
 *
 **************************************
 *
 * Functional description
 *	Post a warning to the current status vector.
 *
 **************************************/
	fb_assert(v.value()[0] == isc_arg_warning);

	FbStatusVector* const statusVector = JRD_get_thread_data()->tdbb_status_vector;
	const ISC_STATUS* toAdd = v.value();
	const unsigned lenToAdd = v.length();

	if (!(statusVector->getState() & IStatus::STATE_WARNINGS))
	{
		// this is a blank status vector just stuff the status
		statusVector->setWarnings2(lenToAdd, toAdd);
		return;
	}

	const ISC_STATUS* oldVector = statusVector->getErrors();
	unsigned lenOld = fb_utils::statusLength(oldVector);

	// check for duplicated error code
	if (fb_utils::subStatus(oldVector, lenOld, toAdd, lenToAdd) != ~0u)
		return;

	StaticStatusVector tmpWarn;
	tmpWarn.assign(oldVector, lenOld);
	tmpWarn.append(toAdd, lenToAdd);
	statusVector->setWarnings2(tmpWarn.getCount(), tmpWarn.begin());
}


void ERR_post_nothrow(const Arg::StatusVector& v, FbStatusVector* statusVector)
/**************************************
 *
 *	E R R _ p o s t _ n o t h r o w
 *
 **************************************
 *
 * Functional description
 *	Populate a status vector.
 *
 **************************************/
{
	post_nothrow(v.length(), v.value(), statusVector);
}


void ERR_post_nothrow(const IStatus* v, FbStatusVector* statusVector)
/**************************************
 *
 *	E R R _ p o s t _ n o t h r o w
 *
 **************************************
 *
 * Functional description
 *	Populate a status vector.
 *
 **************************************/
{
	const ISC_STATUS* toAdd = v->getErrors();
	post_nothrow(fb_utils::statusLength(toAdd), toAdd, statusVector);
}


static void post_nothrow(const unsigned lenToAdd, const ISC_STATUS* toAdd, FbStatusVector* statusVector)
/**************************************
 *
 *	E R R _ p o s t _ n o t h r o w
 *
 **************************************
 *
 * Functional description
 *	Populate a status vector.
 *
 **************************************/
{
	// check status to add
	if (lenToAdd == 0)	// nothing to do
		return;
    fb_assert(toAdd[0] == isc_arg_gds);

	// use default from tdbb when no vector specified
	if (!statusVector)
		statusVector = JRD_get_thread_data()->tdbb_status_vector;

	if (!(statusVector->getState() & IStatus::STATE_ERRORS))
	{
		// this is a blank status vector just stuff the status
		statusVector->setErrors2(lenToAdd, toAdd);
		return;
	}

	const ISC_STATUS* oldVector = statusVector->getErrors();
	unsigned lenOld = fb_utils::statusLength(oldVector);

	// check for duplicated error code
	if (fb_utils::subStatus(oldVector, lenOld, toAdd, lenToAdd) != ~0u)
		return;

	// copy memory from/to
	StaticStatusVector tmp;
	tmp.assign(oldVector, lenOld);
	tmp.append(toAdd, lenToAdd);
	statusVector->setErrors2(tmp.getCount(), tmp.begin());
}


void ERR_post(const Arg::StatusVector& v)
/**************************************
 *
 *	E R R _ p o s t
 *
 **************************************
 *
 * Functional description
 *	Create a status vector and return to the user.
 *
 **************************************/
{
	ERR_post_nothrow(v);

	ERR_punt();
}


void ERR_punt()
{
/**************************************
 *
 *	E R R _ p u n t
 *
 **************************************
 *
 * Functional description
 *	Error stuff has been copied to status vector.  Now punt.
 *
 **************************************/

	thread_db* tdbb = JRD_get_thread_data();
	Database* dbb = tdbb->getDatabase();

	if (dbb && (dbb->dbb_flags & DBB_bugcheck))
	{
		iscDbLogStatus(dbb->dbb_filename.nullStr(), tdbb->tdbb_status_vector);
 		if (Config::getBugcheckAbort())
		{
			abort();
		}
	}

	status_exception::raise(tdbb->tdbb_status_vector);
}


void ERR_warning(const Arg::StatusVector& v)
{
/**************************************
 *
 *	E R R _ w a r n i n g
 *
 **************************************
 *
 * Functional description
 *	Write an error out to the status vector but
 *	don't throw an exception.  This allows
 *	sending a warning message back to the user
 *	without stopping execution of a request.  Note
 *	that subsequent errors can supersede this one.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	FbStatusVector* s = tdbb->tdbb_status_vector;

	v.copyTo(s);
	tdbb->getRequest()->req_flags |= req_warning;
}


void ERR_append_status(FbStatusVector* status_vector, const Arg::StatusVector& v)
{
/**************************************
 *
 *	E R R _ a p p e n d _ s t a t u s
 *
 **************************************
 *
 * Functional description
 *	Append the given status vector with the passed arguments.
 *
 **************************************/
	// First build a status vector with the passed one
	Arg::StatusVector passed(status_vector);

	// Now append the newly vector to the passed one
	passed.append(v);

	// Return the result
	passed.copyTo(status_vector);
}


void ERR_build_status(FbStatusVector* status_vector, const Arg::StatusVector& v)
{
/**************************************
 *
 *	E R R _  a p p e n d _ s t a t u s
 *
 **************************************
 *
 * Functional description
 *	Set the given status vector to the passed arguments.
 *
 **************************************/
	v.copyTo(status_vector);
}


static void internal_error(ISC_STATUS status, int number, const TEXT* file, int line)
{
/**************************************
 *
 *	i n t e r n a l _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Things seem to be going poorly today.
 *
 **************************************/
	TEXT errmsg[MAX_ERRMSG_LEN + 1];

	if (gds__msg_lookup(0, JRD_BUGCHK, number, sizeof(errmsg), errmsg, NULL) < 1)
		strcpy(errmsg, "Internal error code");

	const size_t len = strlen(errmsg);

	if (file)
	{
		// Remove path information
		const TEXT* ptr = file + strlen(file);
		for (; ptr > file; ptr--)
		{
			if ((*ptr == '/') || (*ptr == '\\'))
			{
				ptr++;
				break;
			}
		}
		fb_utils::snprintf(errmsg + len, sizeof(errmsg) - len,
			" (%d), file: %s line: %d", number, ptr, line);
	}
	else {
		fb_utils::snprintf(errmsg + len, sizeof(errmsg) - len, " (%d)", number);
	}

	ERR_post(Arg::Gds(status) << Arg::Str(errmsg));
}
