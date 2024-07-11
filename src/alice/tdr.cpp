//____________________________________________________________
//
//		PROGRAM:	Alice (All Else) Utility
//		MODULE:		tdr.cpp
//		DESCRIPTION:	Routines for automated transaction recovery
//
//  The contents of this file are subject to the Interbase Public
//  License Version 1.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy
//  of the License at http://www.Inprise.com/IPL.html
//
//  Software distributed under the License is distributed on an
//  "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
//  or implied. See the License for the specific language governing
//  rights and limitations under the License.
//
//  The Original Code was created by Inprise Corporation
//  and its predecessors. Portions created by Inprise Corporation are
//  Copyright (C) Inprise Corporation.
//
//  All Rights Reserved.
//  Contributor(s): ______________________________________.
//
//
//____________________________________________________________
//
//
// 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "Apollo" port
//
// 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix" port
//
//

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include "memory_routines.h"
#include "ibase.h"
#include "../alice/alice.h"
#include "../common/classes/Switches.h"
#include "../alice/aliceswi.h"
#include "../alice/alice_proto.h"
#include "../alice/alice_meta.h"
#include "../alice/tdr_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../jrd/constants.h"
#include "../common/classes/ClumpletWriter.h"

using MsgFormat::SafeArg;


static SINT64 ask();
static void print_description(const tdr*);
static void reattach_database(tdr*);
static void reattach_databases(tdr*);
static bool reconnect(FB_API_HANDLE, TraNumber, const TEXT*, SINT64);


static const UCHAR limbo_info[] = { isc_info_limbo, isc_info_end };


//
// The following routines are shared by the command line gfix and
// the windows server manager.  These routines should not contain
// any direct screen I/O (i.e. printf/getc statements).
//


//____________________________________________________________
//
//		Determine the proper action to take
//		based on the state of the various
//		transactions.
//

USHORT TDR_analyze(const tdr* trans)
{
	if (trans == NULL)
		return TRA_none;

	// if the tdr for the first transaction is missing,
	// we can assume it was committed

	USHORT advice = TRA_none;
	USHORT state = trans->tdr_state;
	if (state == TRA_none)
		state = TRA_commit;
	else if (state == TRA_unknown)
		advice = TRA_unknown;

	for (trans = trans->tdr_next; trans; trans = trans->tdr_next)
	{
		switch (trans->tdr_state)
		{
			// an explicitly committed transaction necessitates a check for the
			// perverse case of a rollback, otherwise a commit if at all possible

		case TRA_commit:
			if (state == TRA_rollback)
			{
				ALICE_print(105);
				// msg 105: Warning: Multidatabase transaction is in inconsistent state for recovery.
				ALICE_print(106, SafeArg() << trans->tdr_id);
				// msg 106: Transaction %ld was committed, but prior ones were rolled back.
				return 0;
			}
			else
				advice = TRA_commit;
			break;

			// a prepared transaction requires a commit if there are missing
			// records up to now, otherwise only do something if somebody else
			// already has

		case TRA_limbo:
			switch (state)
			{
			case TRA_none:
			case TRA_commit:
				advice = TRA_commit;
				break;
			case TRA_rollback:
				advice = TRA_rollback;
				break;
			}
			break;

			// an explicitly rolled back transaction requires a rollback unless a
			// transaction has committed or is assumed committed

		case TRA_rollback:
			if ((state == TRA_commit) || (state == TRA_none))
			{
				ALICE_print(105);
				// msg 105: Warning: Multidatabase transaction is in inconsistent state for recovery.
				ALICE_print(107, SafeArg() << trans->tdr_id);
				// msg 107: Transaction %ld was rolled back, but prior ones were committed.

				return 0;
			}
			advice = TRA_rollback;
			break;

			// a missing TDR indicates a committed transaction if a limbo one hasn't
			// been found yet, otherwise it implies that the transaction wasn't
			// prepared

		case TRA_none:
			if (state == TRA_commit)
				advice = TRA_commit;
			else if (state == TRA_limbo)
				advice = TRA_rollback;
			break;

			// specifically advise TRA_unknown to prevent assumption that all are
			// in limbo

		case TRA_unknown:
			if (!advice)
				advice = TRA_unknown;
			break;

		default:
			ALICE_print(67, SafeArg() << trans->tdr_state);
			// msg 67: Transaction state %d not in valid range.
			return 0;
		}
	}

	return advice;
}



//____________________________________________________________
//
//		Attempt to attach a database with a given pathname.
//

bool TDR_attach_database(ISC_STATUS* status_vector, tdr* trans, const TEXT* pathname)
{
	AliceGlobals* tdgbl = AliceGlobals::getSpecific();

	if (tdgbl->ALICE_data.ua_debug)
		ALICE_print(68, SafeArg() << pathname); // msg 68: ATTACH_DATABASE: attempted attach of %s

	Firebird::ClumpletWriter dpb(Firebird::ClumpletReader::dpbList, MAX_DPB_SIZE);
	dpb.insertTag(isc_dpb_no_garbage_collect);
	dpb.insertTag(isc_dpb_gfix_attach);
	tdgbl->uSvc->fillDpb(dpb);
	if (tdgbl->ALICE_data.ua_user) {
		dpb.insertString(isc_dpb_user_name, tdgbl->ALICE_data.ua_user, fb_strlen(tdgbl->ALICE_data.ua_user));
	}
	if (tdgbl->ALICE_data.ua_role) {
		dpb.insertString(isc_dpb_sql_role_name, tdgbl->ALICE_data.ua_role, fb_strlen(tdgbl->ALICE_data.ua_role));
	}
	if (tdgbl->ALICE_data.ua_password)
	{
		dpb.insertString(tdgbl->uSvc->isService() ? isc_dpb_password_enc : isc_dpb_password,
						tdgbl->ALICE_data.ua_password, fb_strlen(tdgbl->ALICE_data.ua_password));
	}

	trans->tdr_db_handle = 0;

	isc_attach_database(status_vector, 0, pathname,
						 &trans->tdr_db_handle, dpb.getBufferLength(),
						 reinterpret_cast<const char*>(dpb.getBuffer()));

	if (status_vector[1])
	{
		if (tdgbl->ALICE_data.ua_debug)
		{
			ALICE_print(69);	// msg 69:  failed
			ALICE_print_status(false, status_vector);
		}
		return false;
	}

	MET_set_capabilities(status_vector, trans);

	if (tdgbl->ALICE_data.ua_debug)
		ALICE_print(70);	// msg 70:  succeeded

	return true;
}



//____________________________________________________________
//
//		Get the state of the various transactions
//		in a multidatabase transaction.
//

void TDR_get_states(tdr* trans)
{
	ISC_STATUS_ARRAY status_vector;

	for (tdr* ptr = trans; ptr; ptr = ptr->tdr_next)
		MET_get_state(status_vector, ptr);
}



//____________________________________________________________
//
//		Detach all databases associated with
//		a multidatabase transaction.
//

void TDR_shutdown_databases(tdr* trans)
{
	ISC_STATUS_ARRAY status_vector;

	for (tdr* ptr = trans; ptr; ptr = ptr->tdr_next)
		isc_detach_database(status_vector, &ptr->tdr_db_handle);
}



//
// The following routines are only for the command line utility.
// This should really be split into two files...
//


//____________________________________________________________
//
//		List transaction stuck in limbo.  If the prompt switch is set,
//		prompt for commit, rollback, or leave well enough alone.
//

void TDR_list_limbo(FB_API_HANDLE handle, const TEXT* name, const SINT64 switches)
{
	UCHAR buffer[1024];
	ISC_STATUS_ARRAY status_vector;
	AliceGlobals* tdgbl = AliceGlobals::getSpecific();

	if (isc_database_info(status_vector, &handle, sizeof(limbo_info),
						   reinterpret_cast<const char*>(limbo_info),
						   sizeof(buffer),
						   reinterpret_cast<char*>(buffer)))
	{
		ALICE_print_status(true, status_vector);
		return;
	}

    TraNumber id;
   	tdr* trans;

	for (Firebird::ClumpletReader p(Firebird::ClumpletReader::InfoResponse, buffer, sizeof(buffer));
		!p.isEof(); p.moveNext())
	{
		UCHAR item = p.getClumpTag();
		if (item == isc_info_end)
			break;

		const USHORT length = (USHORT) p.getClumpLength();
		switch (item)
		{
		case isc_info_limbo:
			id = p.getBigInt();
			if (switches & (sw_commit | sw_rollback | sw_two_phase | sw_prompt))
			{
				TDR_reconnect_multiple(handle, id, name, switches);
				break;
			}
			if (!tdgbl->uSvc->isService())
			{
				ALICE_print(71, SafeArg() << id);
				// msg 71: Transaction %d is in limbo.
			}
			if (trans = MET_get_transaction(status_vector, handle, id))
			{
				if (id > TraNumber(MAX_SLONG))
					tdgbl->uSvc->putSInt64(isc_spb_multi_tra_id_64, id);
				else
					tdgbl->uSvc->putSLong(isc_spb_multi_tra_id, (SLONG) id);
				reattach_databases(trans);
				TDR_get_states(trans);
				TDR_shutdown_databases(trans);
				print_description(trans);
			}
			else if (id > TraNumber(MAX_SLONG))
				tdgbl->uSvc->putSInt64(isc_spb_single_tra_id_64, id);
			else
				tdgbl->uSvc->putSLong(isc_spb_single_tra_id, (SLONG) id);
			break;

		case isc_info_truncated:
			if (!tdgbl->uSvc->isService())
			{
				ALICE_print(72);
				// msg 72: More limbo transactions than fit.  Try again
				// And how it's going to retry with a bigger buffer if the buffer is fixed size?
			}
			break;

		default:
			if (!tdgbl->uSvc->isService())
			{
				ALICE_print(73, SafeArg() << item);
				// msg 73: Unrecognized info item %d
			}
		}
	}
}



//____________________________________________________________
//
//		Check a transaction's TDR to see if it is
//		a multi-database transaction.  If so, commit
//		or rollback according to the user's wishes.
//		Object strongly if the transaction is in a
//		state that would seem to preclude committing
//		or rolling back, but essentially do what the
//		user wants.  Intelligence is assumed for the
//		gfix user.
//

bool TDR_reconnect_multiple(FB_API_HANDLE handle, TraNumber id, const TEXT* name, SINT64 switches)
{
	ISC_STATUS_ARRAY status_vector;

	// get the state of all the associated transactions

	tdr* trans = MET_get_transaction(status_vector, handle, id);
	if (!trans)
		return reconnect(handle, id, name, switches);

	reattach_databases(trans);
	TDR_get_states(trans);

	// analyze what to do with them; if the advice contradicts the user's
	// desire, make them confirm it; otherwise go with the flow.

	const USHORT advice = TDR_analyze(trans);

	if (!advice)
	{
		print_description(trans);
		switches = ask();
	}
	else
	{
		switch (advice)
		{
		case TRA_rollback:
			if (switches & sw_commit)
			{
				ALICE_print(74, SafeArg() << trans->tdr_id);
				// msg 74: A commit of transaction %ld will violate two-phase commit.
				print_description(trans);
				switches = ask();
			}
			else if (switches & sw_rollback)
				switches |= sw_rollback;
			else if (switches & sw_two_phase)
				switches |= sw_rollback;
			else if (switches & sw_prompt)
			{
				ALICE_print(75, SafeArg() << trans->tdr_id);
				// msg 75: A rollback of transaction %ld is needed to preserve two-phase commit.
				print_description(trans);
				switches = ask();
			}
			break;

		case TRA_commit:
			if (switches & sw_rollback)
			{
				ALICE_print(76, SafeArg() << trans->tdr_id);
				// msg 76: Transaction %ld has already been partially committed.
				ALICE_print(77);
				// msg 77: A rollback of this transaction will violate two-phase commit.
				print_description(trans);
				switches = ask();
			}
			else if (switches & sw_commit)
				switches |= sw_commit;
			else if (switches & sw_two_phase)
				switches |= sw_commit;
			else if (switches & sw_prompt)
			{
				ALICE_print(78, SafeArg() << trans->tdr_id);
				// msg 78: Transaction %ld has been partially committed.
				ALICE_print(79);
				// msg 79: A commit is necessary to preserve the two-phase commit.
				print_description(trans);
				switches = ask();
			}
			break;

		case TRA_unknown:
			ALICE_print(80);
			// msg 80: Insufficient information is available to determine
			ALICE_print(81, SafeArg() << trans->tdr_id);
			// msg 81: a proper action for transaction %ld.
			print_description(trans);
			switches = ask();
			break;

		default:
			if (!(switches & (sw_commit | sw_rollback)))
			{
				ALICE_print(82, SafeArg() << trans->tdr_id);
				// msg 82: Transaction %ld: All subtransactions have been prepared.
				ALICE_print(83);
				// msg 83: Either commit or rollback is possible.
				print_description(trans);
				switches = ask();
			}
		}
	}

    bool error = false;
	if (switches != ULONG(~0))
	{
		// now do the required operation with all the subtransactions

		if (switches & (sw_commit | sw_rollback))
		{
			for (tdr* ptr = trans; ptr; ptr = ptr->tdr_next)
			{
				if (ptr->tdr_state == TRA_limbo)
				{
					reconnect(ptr->tdr_db_handle, ptr->tdr_id, ptr->tdr_filename.c_str(), switches);
				}
			}
		}
	}
	else
	{
		ALICE_print(84);	// msg 84: unexpected end of input
		error = true;
	}

	// shutdown all the databases for cleanliness' sake

	TDR_shutdown_databases(trans);

	return error;
}



//____________________________________________________________
//
//		format and print description of a transaction in
//		limbo, including all associated transactions
//		in other databases.
//

static void print_description(const tdr* trans)
{
	AliceGlobals* tdgbl = AliceGlobals::getSpecific();

	if (!trans)
		return;

	if (!tdgbl->uSvc->isService())
		ALICE_print(92);	// msg 92:   Multidatabase transaction:

	bool prepared_seen = false;
	for (const tdr* ptr = trans; ptr; ptr = ptr->tdr_next)
	{
		const auto host_site = ptr->tdr_host_site.nullStr();
		if (host_site)
		{
			if (!tdgbl->uSvc->isService())
			{
				// msg 93: Host Site: %s
				ALICE_print(93, SafeArg() << host_site);
			}
			tdgbl->uSvc->putLine(isc_spb_tra_host_site, host_site);
		}

		if (ptr->tdr_id)
		{
			if (!tdgbl->uSvc->isService())
			{
				// msg 94: Transaction %ld
				ALICE_print(94, SafeArg() << ptr->tdr_id);
			}
			if (ptr->tdr_id > TraNumber(MAX_SLONG))
				tdgbl->uSvc->putSInt64(isc_spb_tra_id_64, ptr->tdr_id);
			else
				tdgbl->uSvc->putSLong(isc_spb_tra_id, (SLONG) ptr->tdr_id);
		}

		switch (ptr->tdr_state)
		{
		case TRA_limbo:
			if (!tdgbl->uSvc->isService())
			{
				ALICE_print(95);	// msg 95: has been prepared.
			}
			tdgbl->uSvc->putChar(isc_spb_tra_state, isc_spb_tra_state_limbo);
			prepared_seen = true;
			break;

		case TRA_commit:
			if (!tdgbl->uSvc->isService())
			{
				ALICE_print(96);	// msg 96: has been committed.
			}
			tdgbl->uSvc->putChar(isc_spb_tra_state, isc_spb_tra_state_commit);
			break;

		case TRA_rollback:
			if (!tdgbl->uSvc->isService())
			{
				ALICE_print(97);	// msg 97: has been rolled back.
			}
			tdgbl->uSvc->putChar(isc_spb_tra_state, isc_spb_tra_state_rollback);
			break;

		case TRA_unknown:
			if (!tdgbl->uSvc->isService())
			{
				ALICE_print(98);	// msg 98: is not available.
			}
			tdgbl->uSvc->putChar(isc_spb_tra_state, isc_spb_tra_state_unknown);
			break;

		default:
			if (!tdgbl->uSvc->isService())
			{
				// msg 99: is not found, assumed not prepared.
				// msg 100: is not found, assumed to be committed.
				ALICE_print(prepared_seen ? 99 : 100);
			}
			break;
		}

		const auto remote_site = ptr->tdr_remote_site.nullStr();
		if (remote_site)
		{
			if (!tdgbl->uSvc->isService())
			{
				// msg 101: Remote Site: %s
				ALICE_print(101, SafeArg() << remote_site);
			}
			tdgbl->uSvc->putLine(isc_spb_tra_remote_site, remote_site);
		}

		const auto fullpath = ptr->tdr_fullpath.nullStr();
		if (fullpath)
		{
			if (!tdgbl->uSvc->isService())
			{
				// msg 102: Database Path: %s
				ALICE_print(102, SafeArg() << fullpath);
			}
			tdgbl->uSvc->putLine(isc_spb_tra_db_path, fullpath);
		}
	}

	// let the user know what the suggested action is

	switch (TDR_analyze(trans))
	{
	case TRA_commit:
		if (!tdgbl->uSvc->isService())
		{
			// msg 103: Automated recovery would commit this transaction.
			ALICE_print(103);
		}
		tdgbl->uSvc->putChar(isc_spb_tra_advise, isc_spb_tra_advise_commit);
		break;

	case TRA_rollback:
		if (!tdgbl->uSvc->isService())
		{
			// msg 104: Automated recovery would rollback this transaction.
			ALICE_print(104);
		}
		tdgbl->uSvc->putChar(isc_spb_tra_advise, isc_spb_tra_advise_rollback);
		break;

	default:
		tdgbl->uSvc->putChar(isc_spb_tra_advise, isc_spb_tra_advise_unknown);
		break;
	}
}



//____________________________________________________________
//
//		Ask the user whether to commit or rollback.
//

static SINT64 ask()
{
	AliceGlobals* tdgbl = AliceGlobals::getSpecific();

	if (tdgbl->uSvc->isService())
		return ~SINT64(0);

	char response[32];
	SINT64 switches = 0;

	while (true)
	{
		ALICE_print(85);
		// msg 85: Commit, rollback, or neither (c, r, or n)?
		int c;
		const char* end = response + sizeof(response) - 1;
		char* p = response;
		while ((c = getchar()) != '\n' && !feof(stdin) && !ferror(stdin) && p < end)
			*p++ = c;
		if (p == response)
			return ~SINT64(0);
		*p = 0;
		ALICE_upper_case(response, response, sizeof(response));
		if (!strcmp(response, "N") || !strcmp(response, "C") || !strcmp(response, "R"))
		{
			  break;
		}
	}

	if (response[0] == 'C')
		switches |= sw_commit;
	else if (response[0] == 'R')
		switches |= sw_rollback;

	return switches;
}


//____________________________________________________________
//
//		Generate pathnames for a given database
//		until the database is successfully attached.
//

static void reattach_database(tdr* trans)
{
	ISC_STATUS_ARRAY status_vector;
	char buffer[BUFFER_LARGE];
	// sizeof(buffer) - 1 => leave space for the terminator.
	const char* const end = buffer + sizeof(buffer) - 1;
	AliceGlobals* tdgbl = AliceGlobals::getSpecific();

	if (trans->tdr_fullpath.hasData())
	{
		Firebird::string hostname;
		ISC_get_host(hostname);

		// if this is being run from the same host,
		// try to reconnect using the same pathname

		if (trans->tdr_host_site == hostname)
		{
			if (TDR_attach_database(status_vector, trans, trans->tdr_fullpath.c_str()))
				return;
		}
		else if (trans->tdr_host_site.hasData())
		{
			//  try going through the previous host with all available
			//  protocols, using chaining to try the same method of
			//  attachment originally used from that host
			const Firebird::string pathname = trans->tdr_host_site + ':' + trans->tdr_fullpath;
			if (TDR_attach_database(status_vector, trans, pathname.c_str()))
				return;
		}

		// attaching using the old method didn't work;
		// try attaching to the remote node directly

		if (trans->tdr_remote_site.hasData())
		{
			const Firebird::string pathname = trans->tdr_remote_site + ':' + trans->tdr_filename;
			if (TDR_attach_database(status_vector, trans, pathname.c_str()))
				return;
		}

	}

	// we have failed to reattach; notify the user
	// and let them try to succeed where we have failed

	ALICE_print(86, SafeArg() << trans->tdr_id);
	// msg 86: Could not reattach to database for transaction %ld.
	ALICE_print(87, SafeArg() << (trans->tdr_fullpath.hasData() ? trans->tdr_fullpath.c_str() : "unknown"));
	// msg 87: Original path: %s

	if (tdgbl->uSvc->isService())
		ALICE_exit(FINI_ERROR, tdgbl);

	for (;;)
	{
		ALICE_print(88);	// msg 88: Enter a valid path:
		char* p = buffer;
		while (p < end && (*p = getchar()) != '\n' && !feof(stdin) && !ferror(stdin))
			++p;
		*p = 0;
		if (!buffer[0])
			break;
		p = buffer;
		while (*p == ' ')
			++p;
		if (TDR_attach_database(status_vector, trans, p))
		{
			trans->tdr_fullpath.assign(p);
			trans->tdr_filename = trans->tdr_fullpath;
			return;
		}
		ALICE_print(89);	// msg 89: Attach unsuccessful.
	}
}



//____________________________________________________________
//
//		Attempt to locate all databases used in
//		a multidatabase transaction.
//

static void reattach_databases(tdr* trans)
{
	for (tdr* ptr = trans; ptr; ptr = ptr->tdr_next)
		reattach_database(ptr);
}



//____________________________________________________________
//
//		Commit or rollback a named transaction.
//

static bool reconnect(FB_API_HANDLE handle, TraNumber number, const TEXT* name, SINT64 switches)
{
	ISC_STATUS_ARRAY status_vector;

	UCHAR numbuf[sizeof(TraNumber)];
	USHORT numlen;

	if (number > TraNumber(MAX_SLONG))
	{
		put_vax_int64(numbuf, number);
		numlen = sizeof(SINT64);
	}
	else
	{
		put_vax_long(numbuf, (SLONG) number);
		numlen = sizeof(SLONG);
	}

	FB_API_HANDLE transaction = 0;
	if (isc_reconnect_transaction(status_vector, &handle, &transaction,
								  numlen, reinterpret_cast<const char*>(numbuf)))
	{
		ALICE_print(90, SafeArg() << name);
		// msg 90: failed to reconnect to a transaction in database %s
		ALICE_print_status(true, status_vector);
		return true;
	}

	if (!(switches & (sw_commit | sw_rollback)))
	{
		ALICE_print(91, SafeArg() << number);
		// msg 91: Transaction %ld:
		switches = ask();
		if (switches == ~SINT64(0))
		{
			ALICE_print(84);
			// msg 84: unexpected end of input
			return true;
		}
	}

	if (switches & sw_commit)
		isc_commit_transaction(status_vector, &transaction);
	else if (switches & sw_rollback)
		isc_rollback_transaction(status_vector, &transaction);
	else
		return false;

	if (status_vector[1])
	{
		ALICE_print_status(true, status_vector);
		return true;
	}

	return false;
}
