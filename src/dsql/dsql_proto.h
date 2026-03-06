/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		dsql_proto.h
 *	DESCRIPTION:	Prototype Header file for dsql.cpp
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
 * Adriano dos Santos Fernandes
 */

#ifndef DSQL_DSQL_PROTO_H
#define DSQL_DSQL_PROTO_H

#include "../common/classes/array.h"
#include "firebird/Interface.h"

namespace Firebird::Jrd {
	class Attachment;
	class thread_db;
	class jrd_tra;
	class DsqlDmlRequest;
	class DsqlRequest;
}

namespace Firebird::Jrd {

void DSQL_execute(thread_db*, jrd_tra**, DsqlRequest*, IMessageMetadata*, const UCHAR*, IMessageMetadata*, UCHAR*);
void DSQL_execute_immediate(thread_db*, Attachment*, jrd_tra**, ULONG, const TEXT*, USHORT,
	IMessageMetadata*, const UCHAR*, IMessageMetadata*, UCHAR*, bool);
void DSQL_free_statement(thread_db*, DsqlRequest*, USHORT);
DsqlRequest* DSQL_prepare(thread_db*, Attachment*, jrd_tra*, ULONG, const TEXT*, USHORT, unsigned,
	Array<UCHAR>*, Array<UCHAR>*, bool);
void DSQL_sql_info(thread_db*, DsqlRequest*, ULONG, const UCHAR*, ULONG, UCHAR*);

} // namespace Firebird::Jrd

#endif //  DSQL_DSQL_PROTO_H
