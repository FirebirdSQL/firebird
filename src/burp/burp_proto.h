/*
 *	PROGRAM:	JRD Backup and Restore program
 *	MODULE:		burp_proto.h
 *	DESCRIPTION:	Prototype header file for burp.cpp
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

#ifndef BURP_BURP_PROTO_H
#define BURP_BURP_PROTO_H

#include "../common/ThreadData.h"
#include "../common/classes/MsgPrint.h"
#include "../common/classes/fb_string.h"
#include "../common/UtilSvc.h"

class BurpGlobals;

int		BURP_main(Firebird::UtilSvc*);
int		gbak(Firebird::UtilSvc*);

void	BURP_abort(Firebird::IStatus* status = nullptr);
void	BURP_error(USHORT, bool, const MsgFormat::SafeArg& arg = MsgFormat::SafeArg());
void	BURP_error(USHORT, bool, const char* str);
void	BURP_error_redirect(Firebird::IStatus*, USHORT, const MsgFormat::SafeArg& arg = MsgFormat::SafeArg());
void	BURP_makeSymbol(BurpGlobals*, Firebird::string&);
void	BURP_msg_partial(bool, USHORT, const MsgFormat::SafeArg& arg = MsgFormat::SafeArg());
void	BURP_msg_put(bool, USHORT, const MsgFormat::SafeArg& arg);
const int BURP_MSG_GET_SIZE = 128; // Use it for buffers passed to this function.
void	BURP_msg_get(USHORT, TEXT*, const MsgFormat::SafeArg& arg = MsgFormat::SafeArg());
void	BURP_output_version(void*, const TEXT*);
void	BURP_print(bool err, USHORT, const MsgFormat::SafeArg& arg = MsgFormat::SafeArg());
void	BURP_print(bool err, USHORT, const char* str);
void	BURP_print_status(bool err, Firebird::IStatus* status, USHORT secondNumber = 0);
void	BURP_print_warning(Firebird::IStatus* status);
void	BURP_verbose(USHORT, const MsgFormat::SafeArg& arg = MsgFormat::SafeArg());
void	BURP_verbose(USHORT, const char* str);
void	BURP_message(USHORT, const MsgFormat::SafeArg& arg = MsgFormat::SafeArg(), bool totals = false);

#endif	//  BURP_BURP_PROTO_H
