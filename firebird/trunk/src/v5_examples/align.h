/*
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
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix" port
 *
 */
/*
$Id: align.h,v 1.3 2002-10-29 03:17:45 seanleyne Exp $
*/

#ifdef VMS
#define FB_ALIGN(n,b)              (n)
#endif

#ifdef sun
#ifdef sparc
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif
#endif

#ifdef hpux
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif

#ifdef sgi
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif

#ifdef _AIX
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif
 
#if (defined __osf__ && defined __alpha)
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif

#if (defined(_MSC_VER) && defined(WIN32)) || (defined(__BORLANDC__) \
    && defined(__WIN32__))
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif

#ifndef ALIGN
#define FB_ALIGN(n,b)          ((n+1) & ~1)
#endif
