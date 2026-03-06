/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_proto.h
 *	DESCRIPTION:	Prototype header file for isc.cpp
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

#ifndef JRD_ISC_PROTO_H
#define JRD_ISC_PROTO_H

#include "../common/classes/fb_string.h"
#include "firebird/Interface.h"

bool	ISC_check_process_existence(SLONG);
TEXT*	ISC_get_host(TEXT *, USHORT);
const TEXT*	ISC_get_host(string&);
bool	ISC_get_user(string*, int*, int*);
SLONG	ISC_set_prefix(const TEXT*, const TEXT*);

void	iscDbLogStatus(const TEXT* text, IStatus* status);
// Do not add word "Database" in the beginning like gds__log_status / iscDbLogStatus
void	iscLogStatus(const TEXT* text, const ISC_STATUS* status_vector);
void	iscLogStatus(const TEXT* text, const IStatus* status);
void	iscLogException(const TEXT* text, const Exception& e);

#ifdef WIN_NT
struct _SECURITY_ATTRIBUTES* ISC_get_security_desc();
#endif

void iscPrefixLock(TEXT* string, const TEXT* root, bool createLockDir);
void iscSafeConcatPath(TEXT* resultString, const TEXT* appendString);

#endif // JRD_ISC_PROTO_H
