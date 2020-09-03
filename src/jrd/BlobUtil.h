/*
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_BLOB_UTIL_H
#define JRD_BLOB_UTIL_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/status.h"
#include "../jrd/SystemPackages.h"

namespace Jrd {


class BlobUtilPackage : public SystemPackage
{
public:
	BlobUtilPackage(Firebird::MemoryPool& pool);

private:
	FB_MESSAGE(BinaryMessage, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(MAX_VARY_COLUMN_SIZE, 0), data)
	);

	FB_MESSAGE(BlobMessage, Firebird::ThrowStatusExceptionWrapper,
		(FB_BLOB, blob)
	);

	FB_MESSAGE(HandleMessage, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, handle)
	);

	//----------

	FB_MESSAGE(AppendInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, handle)
		(FB_INTL_VARCHAR(MAX_VARY_COLUMN_SIZE, 0), data)
	);

	static Firebird::IExternalResultSet* appendProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const AppendInput::Type* in, void* out);

	//----------

	static Firebird::IExternalResultSet* cancelProcedure(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const HandleMessage::Type* in, void* out);

	//----------

	FB_MESSAGE(NewInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, segmented)
		(FB_BOOLEAN, tempStorage)
	);

	static void newFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context,
		const NewInput::Type* in, HandleMessage::Type* out);

	//----------

	static void openBlobFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context,
		const BlobMessage::Type* in, HandleMessage::Type* out);

	//----------

	FB_MESSAGE(SeekInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, handle)
		(FB_INTEGER, mode)
		(FB_INTEGER, offset)
	);

	FB_MESSAGE(SeekOutput, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, offset)
	);

	static void seekFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context, const SeekInput::Type* in, SeekOutput::Type* out);

	//----------

	FB_MESSAGE(ReadInput, Firebird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, handle)
		(FB_INTEGER, length)
	);

	static void readFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context,
		const ReadInput::Type* in, BinaryMessage::Type* out);

	//----------

	static void makeBlobFunction(Firebird::ThrowStatusExceptionWrapper* status,
		Firebird::IExternalContext* context,
		const HandleMessage::Type* in, BlobMessage::Type* out);
};


}	// namespace

#endif	// JRD_BLOB_UTIL_H
