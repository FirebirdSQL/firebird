/*
 *	PROGRAM:	Security data base manager
 *	MODULE:		gsec.h
 *	DESCRIPTION:	Header file for the GSEC program
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

#ifndef UTILITIES_GSEC_H
#define UTILITIES_GSEC_H

#include "firebird/Interface.h"
#include "../common/ThreadData.h"
#include "../jrd/constants.h"

inline constexpr USHORT GSEC_MSG_FAC = FB_IMPL_MSG_FACILITY_GSEC;
inline constexpr int MSG_LENGTH = 128;

namespace Auth
{

class UserData;

/* structure to hold information from the command line, including the
   operation to perform and any parameters entered (sizes are determined
   by the size of the fields in the USERS relation in USERINFO.GDB) */

inline constexpr unsigned int ADD_OPER		= Firebird::IUser::OP_USER_ADD;
inline constexpr unsigned int MOD_OPER		= Firebird::IUser::OP_USER_MODIFY;
inline constexpr unsigned int DEL_OPER		= Firebird::IUser::OP_USER_DELETE;
inline constexpr unsigned int DIS_OPER		= Firebird::IUser::OP_USER_DISPLAY;
inline constexpr unsigned int MAP_SET_OPER	= Firebird::IUser::OP_USER_SET_MAP;
inline constexpr unsigned int MAP_DROP_OPER	= Firebird::IUser::OP_USER_DROP_MAP;
// Following operations never go to plugins
inline constexpr unsigned int QUIT_OPER		= 101;
inline constexpr unsigned int HELP_OPER		= 102;
inline constexpr unsigned int ADDMOD_OPER	= 103;
inline constexpr unsigned int OLD_DIS_OPER	= 104;

} // namespace Auth

namespace Firebird
{
	class UtilSvc;
}

class tsec : public Firebird::ThreadData
{
public:
	explicit tsec(Firebird::UtilSvc* uf)
		: ThreadData(ThreadData::tddSEC), utilSvc(uf),
		tsec_user_data(NULL),
		tsec_exit_code(0), tsec_throw(false),
		tsec_interactive(false), tsec_sw_version(false)
	{
	}

	Firebird::UtilSvc*	utilSvc;
	Auth::UserData*		tsec_user_data;
	int					tsec_exit_code;
	bool				tsec_throw;
	bool				tsec_interactive;
	bool				tsec_sw_version;

	static inline tsec* getSpecific()
	{
		return (tsec*) ThreadData::getSpecific();
	}
	static inline void putSpecific(tsec* tdsec)
	{
		tdsec->ThreadData::putSpecific();
	}
	static inline void restoreSpecific()
	{
		ThreadData::restoreSpecific();
	}
};



inline constexpr USHORT GsecMsg0	= 0;	// empty message
inline constexpr USHORT GsecMsg1	= 1;	// "GSEC> "  (the prompt)
inline constexpr USHORT GsecMsg2	= 2;	// gsec  (lower case version of the prompt for help display)
inline constexpr USHORT GsecMsg3	= 3;	// ADD          add user
inline constexpr USHORT GsecMsg4	= 4;	// DELETE       delete user
inline constexpr USHORT GsecMsg5	= 5;	// DISPLAY      display user(s)
inline constexpr USHORT GsecMsg6	= 6;	// MODIFY       modify user
inline constexpr USHORT GsecMsg7	= 7;	// PW           user's password
inline constexpr USHORT GsecMsg8	= 8;	// UID          user's ID
inline constexpr USHORT GsecMsg9	= 9;	// GID          user's group ID
inline constexpr USHORT GsecMsg10	= 10;	// PROJ         user's project name
inline constexpr USHORT GsecMsg11	= 11;	// ORG          user's organization name
inline constexpr USHORT GsecMsg12	= 12;	// FNAME        user's first name
inline constexpr USHORT GsecMsg13	= 13;	// MNAME        user's middle name/initial
inline constexpr USHORT GsecMsg14	= 14;	// LNAME        user's last name
inline constexpr USHORT GsecMsg15	= 15;	// gsec - unable to open database
inline constexpr USHORT GsecMsg16	= 16;	// gsec - error in switch specifications
inline constexpr USHORT GsecMsg17	= 17;	// gsec - no operation specified
inline constexpr USHORT GsecMsg18	= 18;	// gsec - no user name specified
inline constexpr USHORT GsecMsg19	= 19;	// gsec - add record error
inline constexpr USHORT GsecMsg20	= 20;	// gsec - modify record error
inline constexpr USHORT GsecMsg21	= 21;	// gsec - find/modify record error
inline constexpr USHORT GsecMsg22	= 22;	// gsec - record not found for user:
inline constexpr USHORT GsecMsg23	= 23;	// gsec - delete record error
inline constexpr USHORT GsecMsg24	= 24;	// gsec - find/delete record error
inline constexpr USHORT GsecMsg25	= 25;	// users defined for node
inline constexpr USHORT GsecMsg26	= 26;	//     user name     uid   gid     project   organization       full name
inline constexpr USHORT GsecMsg27	= 27;	// ---------------- ----- ----- ------------ ------------ --------------------
inline constexpr USHORT GsecMsg28	= 28;	// gsec - find/display record error
inline constexpr USHORT GsecMsg29	= 29;	// gsec - invalid parameter, no switch defined
inline constexpr USHORT GsecMsg30	= 30;	// gsec - operation already specified
inline constexpr USHORT GsecMsg31	= 31;	// gsec - password already specified
inline constexpr USHORT GsecMsg32	= 32;	// gsec - uid already specified
inline constexpr USHORT GsecMsg33	= 33;	// gsec - gid already specified
inline constexpr USHORT GsecMsg34	= 34;	// gsec - project already specified
inline constexpr USHORT GsecMsg35	= 35;	// gsec - organization already specified
inline constexpr USHORT GsecMsg36	= 36;	// gsec - first_name already specified
inline constexpr USHORT GsecMsg37	= 37;	// gsec - middle_name already specified
inline constexpr USHORT GsecMsg38	= 38;	// gsec - last_name already specified
inline constexpr USHORT GsecMsg39	= 39;	// gsec version
inline constexpr USHORT GsecMsg40	= 40;	// gsec - invalid switch specified
inline constexpr USHORT GsecMsg41	= 41;	// gsec - ambiguous switch specified
inline constexpr USHORT GsecMsg42	= 42;	// gsec - no operation specified for parameters
inline constexpr USHORT GsecMsg43	= 43;	// gsec - no parameters allowed for this operation
inline constexpr USHORT GsecMsg44	= 44;	// gsec - incompatible switches specified
inline constexpr USHORT GsecMsg45	= 45;	// gsec utility - maintains user password database"
inline constexpr USHORT GsecMsg46	= 46;	// command line usage:
inline constexpr USHORT GsecMsg47	= 47;	// <command> [ <parameter> ... ]
inline constexpr USHORT GsecMsg48	= 48;	// interactive usage:
inline constexpr USHORT GsecMsg49	= 49;	// available commands:
inline constexpr USHORT GsecMsg50	= 50;	// adding a new user:
inline constexpr USHORT GsecMsg51	= 51;	// add <name> [ <parameter> ... ]
inline constexpr USHORT GsecMsg52	= 52;	// deleting a current user:
inline constexpr USHORT GsecMsg53	= 53;	// delete <name>
inline constexpr USHORT GsecMsg54	= 54;	// displaying all users:
inline constexpr USHORT GsecMsg55	= 55;	// display
inline constexpr USHORT GsecMsg56	= 56;	// displaying one user:
inline constexpr USHORT GsecMsg57	= 57;	// display <name>
inline constexpr USHORT GsecMsg58	= 58;	// modifying a user's parameters:
inline constexpr USHORT GsecMsg59	= 59;	// modify <name> <parameter> [ <parameter> ... ]
inline constexpr USHORT GsecMsg60	= 60;	// help:
inline constexpr USHORT GsecMsg61	= 61;	// ? (interactive only)
inline constexpr USHORT GsecMsg62	= 62;	// help
inline constexpr USHORT GsecMsg63	= 63;	// quit interactive session:
inline constexpr USHORT GsecMsg64	= 64;	// quit (interactive only)
inline constexpr USHORT GsecMsg65	= 65;	// available parameters:
inline constexpr USHORT GsecMsg66	= 66;	// -pw <password>
inline constexpr USHORT GsecMsg67	= 67;	// -uid <uid>
inline constexpr USHORT GsecMsg68	= 68;	// -gid <uid>
inline constexpr USHORT GsecMsg69	= 69;	// -proj <projectname>
inline constexpr USHORT GsecMsg70	= 70;	// -org <organizationname>
inline constexpr USHORT GsecMsg71	= 71;	// -fname <firstname>
inline constexpr USHORT GsecMsg72	= 72;	// -mname <middlename>
inline constexpr USHORT GsecMsg73	= 73;	// -lname <lastname>
inline constexpr USHORT GsecMsg74	= 74;	// gsec - memory allocation error
inline constexpr USHORT GsecMsg75	= 75;	// gsec error
inline constexpr USHORT GsecMsg76	= 76;	// invalid user name (maximum 31 bytes allowed)
inline constexpr USHORT GsecMsg77	= 77;	// invalid password (maximum 16 bytes allowed)
inline constexpr USHORT GsecMsg78	= 78;	// gsec - database already specified
inline constexpr USHORT GsecMsg79	= 79;	// gsec - database administrator name already specified
inline constexpr USHORT GsecMsg80	= 80;	// gsec - database administrator password already specified
inline constexpr USHORT GsecMsg81	= 81;	// gsec - SQL role name already specified
inline constexpr USHORT GsecMsg82	= 82;	// [ <options ... ]
inline constexpr USHORT GsecMsg83	= 83;	// available options:
inline constexpr USHORT GsecMsg84	= 84;	// -user <database administrator name>
inline constexpr USHORT GsecMsg85	= 85;	// -password <database administrator password>
inline constexpr USHORT GsecMsg86	= 86;	// -role <database administrator SQL role name>
inline constexpr USHORT GsecMsg87	= 87;	// -database <security database>
inline constexpr USHORT GsecMsg88	= 88;	// -z
inline constexpr USHORT GsecMsg89	= 89;	// displaying version number:
inline constexpr USHORT GsecMsg90	= 90;	// z (interactive only)
inline constexpr USHORT GsecMsg91	= 91;	// -trusted (use trusted authentication)
inline constexpr USHORT GsecMsg92	= 92;	// invalid switch specified in interactive mode
inline constexpr USHORT GsecMsg93	= 93;	// error closing security database
inline constexpr USHORT GsecMsg94	= 94;	// error releasing request in security database
inline constexpr USHORT GsecMsg95	= 95;	// -fe(tch_password) fetch password from file
inline constexpr USHORT GsecMsg96	= 96;	// error fetching password from file
inline constexpr USHORT GsecMsg97	= 97;	// error changing AUTO ADMINS MAPPING in security database
inline constexpr USHORT GsecMsg98	= 98;	// changing admins mapping to SYSDBA:\n
inline constexpr USHORT GsecMsg99	= 99;	// invalid parameter for -MAPPING, only SET or DROP is accepted
inline constexpr USHORT GsecMsg100	= 100;	// -ma(pping) {set|drop}
inline constexpr USHORT GsecMsg101	= 101;	// use gsec -? to get help
inline constexpr USHORT GsecMsg102	= 102;	// -adm(in) {yes|no}
inline constexpr USHORT GsecMsg103	= 103;	// invalid parameter for -ADMIN, only YES or NO is accepted
inline constexpr USHORT GsecMsg104	= 104;	// not enough privileges to complete operation

#endif // UTILITIES_GSEC_H

