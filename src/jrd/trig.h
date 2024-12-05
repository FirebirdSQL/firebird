/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		trig.h
 *	DESCRIPTION:	System-defined triggers
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

#ifndef JRD_TRIG_H
#define JRD_TRIG_H

/* This file contains the trigger blr for the system-defined triggers
   on system relations.  The GDEF source for these triggers is in
   GRANT.GDL in the JRD component.  When modifying a system trigger,
   check out GRANT.GDL, modify the trigger source to generate the
   proper blr, replace the blr in TRIG.H, and check both files back in. */

namespace Jrd {

/* trigger definition structure */

struct jrd_trg
{
	const char*		trg_name;
	UCHAR			trg_relation;
	UCHAR			trg_type;
	USHORT			trg_length;
	const UCHAR*	trg_blr;
	USHORT			trg_flags;
	USHORT			trg_ods_version;
};

/* trigger message structure */

struct trigger_msg
{
	const char*		trigmsg_name;
	USHORT			trigmsg_number;
	const char*		trigmsg_text;
	USHORT			trg_ods_version;
};

/* generator definition structure */

struct gen
{
	const char*		gen_name;
	USHORT			gen_id;
	const char*		gen_description;
	USHORT			gen_ods_version;
};

} //namespace Jrd

/* generators needed by the system triggers */
// Keep in sync with constants.h
static const Jrd::gen generators[] =
{
	{ "RDB$SECURITY_CLASS", 1, NULL, ODS_13_0 },
	{ "SQL$DEFAULT", 2, NULL, ODS_13_0 },
	{ PROCEDURES_GENERATOR, 3, "Procedure ID", ODS_13_0 },
	{ "RDB$EXCEPTIONS", 4, "Exception ID", ODS_13_0 },
	{ "RDB$CONSTRAINT_NAME", 5, "Implicit constraint name", ODS_13_0 },
	{ "RDB$FIELD_NAME", 6, "Implicit domain name", ODS_13_0 },
	{ "RDB$INDEX_NAME", 7, "Implicit index name", ODS_13_0 },
	{ "RDB$TRIGGER_NAME", 8, "Implicit trigger name", ODS_13_0 },
	{ "RDB$BACKUP_HISTORY", 9, "Nbackup technology", ODS_13_0 },
	{ FUNCTIONS_GENERATOR, 10, "Function ID", ODS_13_0 },
	{ "RDB$GENERATOR_NAME", 11, "Implicit generator name", ODS_13_0 },
	{ nullptr, 0, nullptr, 0 }
};


#endif /* JRD_TRIG_H */
