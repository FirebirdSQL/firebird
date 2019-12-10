/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		why_proto.h
 *	DESCRIPTION:	Prototype header file for why.cpp
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

#ifndef JRD_WHY_PROTO_H
#define JRD_WHY_PROTO_H

extern "C" {

ISC_STATUS API_ROUTINE isc_dsql_exec_immed2_m(ISC_STATUS*, isc_db_handle*,
												 isc_tr_handle*, USHORT,
												 const SCHAR*, USHORT, USHORT,
												 SCHAR*, USHORT, USHORT,
												 const SCHAR*, USHORT, SCHAR*,
												 USHORT, USHORT, SCHAR*);

ISC_STATUS API_ROUTINE isc_dsql_exec_immed3_m(ISC_STATUS*, isc_db_handle*,
												 isc_tr_handle*,
												 USHORT, const SCHAR*,
												 USHORT, USHORT, const SCHAR*,
												 USHORT, USHORT, const SCHAR*,
												 USHORT, SCHAR*,
												 USHORT, USHORT, SCHAR*);

typedef void AttachmentCleanupRoutine(isc_db_handle*, void*);
typedef void TransactionCleanupRoutine(isc_tr_handle, void*);

ISC_STATUS API_ROUTINE isc_database_cleanup(ISC_STATUS*, isc_db_handle*,
												AttachmentCleanupRoutine*, void*);

int API_ROUTINE gds__disable_subsystem(TEXT*);
int API_ROUTINE gds__enable_subsystem(TEXT*);

ISC_STATUS API_ROUTINE gds__transaction_cleanup(ISC_STATUS*, isc_tr_handle*,
												   TransactionCleanupRoutine*, void*);

} /* extern "C"*/

#endif // JRD_WHY_PROTO_H
