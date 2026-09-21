/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		blob_proto.h
 *	DESCRIPTION:	Prototype Header file for blob.cpp
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

#ifndef DSQL_BLOB_PROTO_H
#define DSQL_BLOB_PROTO_H

#include "../common/classes/RefCounted.h"

namespace Firebird::Why
{
	class YAttachment;
	class YTransaction;

	void iscBlobLookupDescImpl(YAttachment* attachment, YTransaction* transaction,
		const UCHAR* relationName, const UCHAR* fieldName, ISC_BLOB_DESC* desc, UCHAR* global);

	// Only declared in ibase.h:
	//void API_ROUTINE isc_blob_default_desc(ISC_BLOB_DESC* desc,
	//									   const UCHAR*, const UCHAR*);
}

#endif // DSQL_BLOB_PROTO_H
