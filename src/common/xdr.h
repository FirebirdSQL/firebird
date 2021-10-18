/*
 *	PROGRAM:	External Data Representation
 *	MODULE:		xdr.h
 *	DESCRIPTION:	GDS version of Sun's XDR Package.
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
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "DELTA" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */

#ifndef COMMON_XDR_H
#define COMMON_XDR_H

#include <sys/types.h>
#ifdef WIN_NT
#include <winsock2.h>
typedef	char* caddr_t;
#else // WIN_NT
#include <netinet/in.h>
#endif // WIN_NT

#if defined(__hpux) && !defined(ntohl)
// this include is only for HP 11i v2.
// ntohl is not defined in <netinet/in.h> when _XOPEN_SOURCE_EXTENDED
// is defined, even though ntohl is defined by POSIX
#include <arpa/inet.h>
#endif

typedef int XDR_INT;
typedef int bool_t;

enum xdr_op { XDR_ENCODE = 0, XDR_DECODE = 1, XDR_FREE = 2 };

struct xdr_t
{
	virtual bool_t x_getbytes(SCHAR *, unsigned);		// get some bytes from "
	virtual bool_t x_putbytes(const SCHAR*, unsigned);	// put some bytes to "
	virtual ~xdr_t();

	xdr_op x_op;		// operation; fast additional param
	caddr_t	x_private;	// pointer to private data
	caddr_t	x_base;		// private used for position info
	unsigned x_handy;	// extra private word
	bool	x_local;	// transmission is known to be local (bytes are in the host order)

	xdr_t() :
		x_op(XDR_ENCODE), x_private(0), x_base(0), x_handy(0), x_local(false)
	{ }

	int create(SCHAR* addr, unsigned len, xdr_op op);
};

#endif // COMMON_XDR_H
