/*
 *	MODULE:		TraceSession.h
 *	DESCRIPTION:
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef JRD_TRACESESSION_H
#define JRD_TRACESESSION_H

#include <time.h>

#include "firebird.h"
#include "../../common/classes/fb_string.h"
#include "../../common/classes/ClumpletReader.h"


namespace Firebird {

const int trs_admin			= 0x0001;	// session created by server administrator
const int trs_active		= 0x0002;	// session is active
const int trs_system		= 0x0004;	// session created by engine itself
const int trs_log_full		= 0x0008;	// session trace log is full

class TraceSession
{
public:
	explicit TraceSession(MemoryPool& pool) :
		ses_id(0),
		ses_name(pool),
		ses_auth(pool),
		ses_user(pool),
		ses_config(pool),
		ses_start(0),
		ses_flags(0),
		ses_logfile(pool),
		ses_role(pool)
	{}

	TraceSession(MemoryPool& pool, TraceSession& other) :
		ses_id(other.ses_id),
		ses_name(pool, other.ses_name),
		ses_auth(pool, other.ses_auth),
		ses_user(pool, other.ses_user),
		ses_config(pool, other.ses_config),
		ses_start(other.ses_start),
		ses_flags(other.ses_flags),
		ses_logfile(pool, other.ses_logfile),
		ses_role(pool, other.ses_role)
	{}

	~TraceSession() {}

	void clear()
	{
		ses_id = 0;
		ses_name = "";
		ses_auth.clear();
		ses_user = "";
		ses_config = "";
		ses_start = 0;
		ses_flags = 0;
		ses_logfile = "";
		ses_role = "";
	}

	ULONG	ses_id;
	string	ses_name;
	AuthReader::AuthBlock ses_auth;
	string	ses_user;
	string	ses_config;
	time_t	ses_start;
	int		ses_flags;
	PathName ses_logfile;
	string	ses_role;
};

} // namespace Firebird

#endif // JRD_TRACESESSION_H
