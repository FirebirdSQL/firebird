/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ini_proto.h
 *	DESCRIPTION:	Prototype header file for ini.cpp
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

#ifndef JRD_INI_PROTO_H
#define JRD_INI_PROTO_H

namespace Firebird::Jrd
{
	struct jrd_trg;
	class dsql_dbb;

	void	INI_format(thread_db*, const string&);
	void	INI_init(thread_db*);
	void	INI_init_sys_relations(thread_db*);
	void	INI_init_dsql(thread_db*, dsql_dbb* database);
	string INI_owner_privileges();
	void	INI_upgrade(thread_db*);
}	// namespace Firebird::Jrd

#endif // JRD_INI_PROTO_H
