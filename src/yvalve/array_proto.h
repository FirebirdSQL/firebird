/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		array_proto.h
 *	DESCRIPTION:	Prototype Header file for array.cpp
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

#ifndef DSQL_ARRAY_PROTO_H
#define DSQL_ARRAY_PROTO_H

namespace Why {
	class YAttachment;
	class YTransaction;
}

#ifdef __cplusplus
extern "C" {
#endif

ISC_STATUS API_ROUTINE isc_array_gen_sdl(ISC_STATUS*, const ISC_ARRAY_DESC*,
						SSHORT*, UCHAR*, SSHORT*);

ISC_STATUS API_ROUTINE isc_array_get_slice(ISC_STATUS*, FB_API_HANDLE*, FB_API_HANDLE*,
						ISC_QUAD*, const ISC_ARRAY_DESC*, void*, SLONG*);

ISC_STATUS API_ROUTINE isc_array_lookup_bounds(ISC_STATUS*, FB_API_HANDLE*, FB_API_HANDLE*,
						const SCHAR*, const SCHAR*, ISC_ARRAY_DESC*);

ISC_STATUS API_ROUTINE isc_array_lookup_desc(ISC_STATUS*, FB_API_HANDLE*, FB_API_HANDLE*,
						const SCHAR*, const SCHAR*, ISC_ARRAY_DESC*);

ISC_STATUS API_ROUTINE isc_array_put_slice(ISC_STATUS*, FB_API_HANDLE*, FB_API_HANDLE*,
    					   	ISC_QUAD*, const ISC_ARRAY_DESC*, void*, SLONG*);

ISC_STATUS API_ROUTINE isc_array_set_desc(ISC_STATUS*, const SCHAR*, const SCHAR*,
 						const SSHORT*, const SSHORT*, const SSHORT*, ISC_ARRAY_DESC*);
#ifdef __cplusplus
}   /* extern "C"  */
#endif

void iscArrayLookupBoundsImpl(Why::YAttachment* attachment, Why::YTransaction* transaction,
	const SCHAR* relationName, const SCHAR* fieldName, ISC_ARRAY_DESC* desc);

void iscArrayLookupDescImpl(Why::YAttachment* attachment, Why::YTransaction* transaction,
	const SCHAR* relationName, const SCHAR* fieldName, ISC_ARRAY_DESC* desc);

#endif // DSQL_ARRAY_PROTO_H
