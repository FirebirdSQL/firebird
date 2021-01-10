/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		05.user_metadata.cpp
 *	DESCRIPTION:	A sample of user-implemented IMessageMetadata.
 *					Prints firebird user name (SYSDBA by default).
 *
 *					Example for the following interfaces:
 *
 *					IOffsetsCallback - callback for IUtil::setOffsets()
 *					IMessageMetadata - how to implement it yourself
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2016 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "ifaceExamples.h"

static IMaster* master = fb_get_master_interface();

/*
 * Trivial sample of IMessageMetadata implementation.
 * Metadata is created for a fixed output format with single char field.
 * Therefore index parameter in all functions is ignored.
 * Because the only possible error is index out of bounds status parameter is ignored too.
 * Atomic operation is not used in IReferenceCounted cause we do not plan MT support.
 * Non-array vars used to represent offset and nullOffset of that single field.
 */

class MyMetadata : public IMessageMetadataImpl<MyMetadata, ThrowStatusWrapper>
{
private:
	class Callback : public IOffsetsCallbackImpl<Callback, ThrowStatusWrapper>
	{
	private:
		MyMetadata* metadata;

	public:
		Callback(MyMetadata* pmeta)
			: metadata(pmeta)
		{ }

		//IOffsetsCallback implementation
		void setOffset(ThrowStatusWrapper* status, unsigned /*index*/, unsigned offset, unsigned nullOffset)
		{
			// Typically setOffset() function should save passed offsets
			// in your implementation of message metadata.
			metadata->offset = offset;
			metadata->nullOffset = nullOffset;
		}
	};

	FbSampleAtomic referenceCounter;

	// we are using only single field (varchar) in a sample, therefore no strong alignment requirements.
	// In general message alignment is the maximum field alignment in that message.
	static const unsigned messageAlignment = sizeof(ISC_SHORT);

public:
	unsigned offset, nullOffset, length;

	MyMetadata()
		: referenceCounter(0)
	{
		IUtil* utl = master->getUtilInterface();
		ThrowStatusWrapper s(master->getStatus());
		try
		{
			Callback cb(this);
			length = utl->setOffsets(&s, this, &cb);
		}
		catch(...)
		{
			s.dispose();
			throw;
		}
		s.dispose();
	}

	void addRef()
	{
		++referenceCounter;
	}

	int release()
	{
		int rc = --referenceCounter;
		if (!rc)
			delete this;
		return rc;
	}

	// IMessageMetadata implementation
	unsigned getCount(ThrowStatusWrapper* /*status*/)
	{
		return 1;
	}

	const char* getField(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return NULL;
	}

	const char* getRelation(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return NULL;
	}

	const char* getOwner(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return NULL;
	}

	const char* getAlias(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return NULL;
	}

	unsigned getType(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return SQL_VARYING;
	}

	FB_BOOLEAN isNullable(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return false;
	}

	int getSubType(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return 0;
	}

	unsigned getLength(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return 20;		// Want to make it fit
	}

	int getScale(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return 0;
	}

	unsigned getCharSet(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return 0;
	}

	unsigned getOffset(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return offset;
	}

	unsigned getNullOffset(ThrowStatusWrapper* /*status*/, unsigned /*index*/)
	{
		return nullOffset;
	}

	IMetadataBuilder* getBuilder(ThrowStatusWrapper* status)
	{
		ISC_STATUS err[] = {isc_arg_gds, isc_wish_list, isc_arg_end};
		status->setErrors(err);
		return NULL;
	}

	unsigned getMessageLength(ThrowStatusWrapper* /*status*/)
	{
		return length;
	}

	unsigned getAlignment(ThrowStatusWrapper* /*status*/)
	{
		return messageAlignment;
	}

	unsigned getAlignedLength(ThrowStatusWrapper* /*status*/)
	{
		return ((length / messageAlignment) + (length % messageAlignment ? 1 : 0)) * messageAlignment;
	}
};

template <typename T>
T to(const unsigned char* b, unsigned o)
{
	return *((T*) (b + o));
}

int main()
{
	int rc = 0;
	unsigned char* buffer = NULL;

	// set default password if none specified in environment
	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);

	// status vector and main dispatcher
	ThrowStatusWrapper status(master->getStatus());
	IProvider* prov = master->getDispatcher();

	// declare pointers to required interfaces
	IAttachment* att = NULL;
	ITransaction* tra = NULL;
	IResultSet* curs = NULL;
	MyMetadata* meta = NULL;

	try
	{
		// Instance of our metadata
		meta = new MyMetadata;
		meta->addRef();

		// allocate output buffer
		buffer = new unsigned char[meta->length];

		// attach employee db
		att = prov->attachDatabase(&status, "employee", 0, NULL);

		// start default transaction
		tra = att->startTransaction(&status, 0, NULL);

		// open cursor
		curs = att->openCursor(&status, tra, 0, "select current_user from rdb$database",
			SAMPLES_DIALECT, NULL, NULL, meta, NULL, 0);

		// fetch record from cursor and print it
		curs->fetchNext(&status, buffer);
		ISC_SHORT l = to<ISC_SHORT>(buffer, meta->offset);
		printf("<%*.*s>\n", l, l, buffer + meta->offset + sizeof(ISC_SHORT));

		// close interfaces
		curs->close(&status);
		curs = NULL;

		tra->commit(&status);
		tra = NULL;

		att->detach(&status);
		att = NULL;
	}
	catch (const FbException& error)
	{
		// handle error
		rc = 1;

		char buf[256];
		master->getUtilInterface()->formatStatus(buf, sizeof(buf), error.getStatus());
		fprintf(stderr, "%s\n", buf);
	}

	// release interfaces after error caught
	if (curs)
		curs->release();
	if (tra)
		tra->release();
	if (att)
		att->release();

	// generic cleanup
	if (meta)
		meta->release();
	prov->release();
	status.dispose();
	delete[] buffer;

	return rc;
}
