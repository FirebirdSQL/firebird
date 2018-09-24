/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		types_pub.h
 *	DESCRIPTION:	Types that are used both internally and externally
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef INCLUDE_TYPES_PUB_H
#define INCLUDE_TYPES_PUB_H

#include <stddef.h>

#if defined(__GNUC__) || defined (__HP_cc) || defined (__HP_aCC)
#include <inttypes.h>
#else

#if !defined(_INTPTR_T_DEFINED)
#if defined(_WIN64)
typedef __int64 intptr_t;
typedef unsigned __int64 uintptr_t;
#else
typedef long intptr_t;
typedef unsigned long uintptr_t;
#endif
#endif

#endif

/******************************************************************/
/* API handles                                                    */
/******************************************************************/

#if defined(_LP64) || defined(__LP64__) || defined(__arch64__) || defined(_WIN64)
typedef unsigned int	FB_API_HANDLE;
#else
typedef void*		FB_API_HANDLE;
#endif

/******************************************************************/
/* Sizes of memory blocks                                         */
/******************************************************************/

#ifdef FB_USE_SIZE_T
/* NS: This is how things were done in original Firebird port to 64-bit platforms
   Basic classes use these quantities. However in many places in the engine and
   external libraries 32-bit quantities are used to hold sizes of objects.
   This produces many warnings. This also produces incredibly dirty interfaces,
   when functions take size_t as argument, but only handle 32 bits internally
   without any bounds checking.                                                    */
typedef size_t FB_SIZE_T;
typedef intptr_t FB_SSIZE_T;
#else
/* NS: This is more clean way to handle things for now. We admit that engine is not
   prepared to handle 64-bit memory blocks in most places, and it is not necessary really. */
typedef unsigned int FB_SIZE_T;
typedef int FB_SSIZE_T;
#endif

/******************************************************************/
/* Status vector                                                  */
/******************************************************************/

typedef intptr_t ISC_STATUS;

#define ISC_STATUS_LENGTH	20
typedef ISC_STATUS ISC_STATUS_ARRAY[ISC_STATUS_LENGTH];

/* SQL State as defined in the SQL Standard. */
#define FB_SQLSTATE_LENGTH	5
#define FB_SQLSTATE_SIZE	(FB_SQLSTATE_LENGTH + 1)
typedef char FB_SQLSTATE_STRING[FB_SQLSTATE_SIZE];

/******************************************************************/
/* Define type, export and other stuff based on c/c++ and Windows */
/******************************************************************/
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
	#define  ISC_EXPORT	__stdcall
	#define  ISC_EXPORT_VARARG	__cdecl
#else
	#define  ISC_EXPORT
	#define  ISC_EXPORT_VARARG
#endif

/*
 * It is difficult to detect 64-bit long from the redistributable header
 * thus we may use plain "int" which is 32-bit on all platforms we support
 *
 * We'll move to this definition in future API releases.
 *
 */

#if defined(_LP64) || defined(__LP64__) || defined(__arch64__)
typedef	int				ISC_LONG;
typedef	unsigned int	ISC_ULONG;
#else
typedef	signed long		ISC_LONG;
typedef	unsigned long	ISC_ULONG;
#endif

typedef	signed short	ISC_SHORT;
typedef	unsigned short	ISC_USHORT;

typedef	unsigned char	ISC_UCHAR;
typedef char			ISC_SCHAR;

typedef ISC_UCHAR		FB_BOOLEAN;
#define	FB_FALSE		'\0'
#define	FB_TRUE			'\1'

/*******************************************************************/
/* 64 bit Integers                                                 */
/*******************************************************************/

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__)) && !defined(__GNUC__)
typedef __int64				ISC_INT64;
typedef unsigned __int64	ISC_UINT64;
#else
typedef long long int			ISC_INT64;
typedef unsigned long long int	ISC_UINT64;
#endif

/*******************************************************************/
/* Time & Date support                                             */
/*******************************************************************/

#ifndef ISC_TIMESTAMP_DEFINED
typedef int			ISC_DATE;
typedef unsigned int	ISC_TIME;
typedef struct
{
	ISC_DATE timestamp_date;
	ISC_TIME timestamp_time;
} ISC_TIMESTAMP;
#define ISC_TIMESTAMP_DEFINED
#endif	/* ISC_TIMESTAMP_DEFINED */

/*******************************************************************/
/* Blob Id support                                                 */
/*******************************************************************/

struct GDS_QUAD_t {
	ISC_LONG gds_quad_high;
	ISC_ULONG gds_quad_low;
};

typedef struct GDS_QUAD_t GDS_QUAD;
typedef struct GDS_QUAD_t ISC_QUAD;

#define	isc_quad_high	gds_quad_high
#define	isc_quad_low	gds_quad_low

typedef int (*FB_SHUTDOWN_CALLBACK)(const int reason, const int mask, void* arg);

struct FB_DEC16_t {
	ISC_UINT64 fb_data[1];
};

struct FB_DEC34_t {
	ISC_UINT64 fb_data[2];
};

typedef struct FB_DEC16_t FB_DEC16;
typedef struct FB_DEC34_t FB_DEC34;

#endif /* INCLUDE_TYPES_PUB_H */
