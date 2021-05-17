/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		11.batch.cpp
 *	DESCRIPTION:	A trivial sample of using Batch interface.
 *
 *					Example for the following interfaces:
 *					IBatch - interface to work with FB batches
 *					IBatchCompletionState - contains result of batch execution
 *
 *	c++ 11.batch.cpp -lfbclient
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
 *  Copyright (c) 2017 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "ifaceExamples.h"
#include <firebird/Message.h>

static IMaster* master = fb_get_master_interface();


// output error message to user

static void errPrint(IStatus* status)
{
	char buf[256];
	master->getUtilInterface()->formatStatus(buf, sizeof(buf), status);
	fprintf(stderr, "%s\n", buf);
}


// align target to alignment boundary

template <typename T>
static inline T align(T target, uintptr_t alignment)
{
	return (T) ((((uintptr_t) target) + alignment - 1) & ~(alignment - 1));
}


// append given message to buffer ptr

static void putMsg(unsigned char*& ptr, const void* from, unsigned size, unsigned alignment)
{
	memcpy(ptr, from, size);
	ptr += align(size, alignment);
}


// append blob header with BPB to buffer ptr
// return pointer to blob size field - prefilled with BPB size

static unsigned* putBlobHdr(unsigned char*& ptr, unsigned alignment, ISC_QUAD* id, unsigned bpbSize, const unsigned char* bpb)
{
	ptr = align(ptr, alignment);

	memcpy(ptr, id, sizeof(ISC_QUAD));
	ptr += sizeof(ISC_QUAD);

	unsigned* rc = reinterpret_cast<unsigned*>(ptr);

	memcpy(ptr, &bpbSize, sizeof(unsigned));
	ptr += sizeof(unsigned);
	memcpy(ptr, &bpbSize, sizeof(unsigned));
	ptr += sizeof(unsigned);

	memcpy(ptr, bpb, bpbSize);
	ptr += bpbSize;

	return rc;
}


// append given blob to buffer ptr

static void putBlob(unsigned char*& ptr, const void* from, unsigned size, unsigned alignment, ISC_QUAD* id)
{
	unsigned* sizePtr = putBlobHdr(ptr, alignment, id, 0, NULL);
	memcpy(ptr, from, size);
	*sizePtr += size;
	ptr += size;

	ptr = align(ptr, alignment);
}


// append given segment to buffer ptr

unsigned putSegment(unsigned char*& ptr, const char* testData)
{
	ptr = align(ptr, IBatch::BLOB_SEGHDR_ALIGN);
	unsigned short l = strlen(testData);
	memcpy(ptr, &l, sizeof l);
	ptr += sizeof l;
	memcpy(ptr, testData, l);
	ptr += l;
	return align(l + sizeof l, IBatch::BLOB_SEGHDR_ALIGN);
}

// BatchCompletionState printer - prints all what we know about completed batch

static void print_cs(ThrowStatusWrapper& status, IBatchCompletionState* cs, IUtil* utl)
{
	unsigned p = 0;
	IStatus* s2 = NULL;
	bool pr1 = false, pr2 = false;

	// 1. Print per-message state info

	unsigned upcount = cs->getSize(&status);
	unsigned unk = 0, succ = 0;
	for (p = 0; p < upcount; ++p)
	{
		int s = cs->getState(&status, p);
		switch (s)
		{
		case IBatchCompletionState::EXECUTE_FAILED:
			if (!pr1)
			{
				printf("Message Status\n", p);
				pr1 = true;
			}
			printf("%5u   Execute failed\n", p);
			break;

		case IBatchCompletionState::SUCCESS_NO_INFO:
			++unk;
			break;

		default:
			if (!pr1)
			{
				printf("Message Status\n", p);
				pr1 = true;
			}
			printf("%5u   Updated %d record(s)\n", p, s);
			++succ;
			break;
		}
	}
	printf("Summary: total=%u success=%u success(but no update info)=%u\n", upcount, succ, unk);

	// 2. Print detailed errors (if exist) for messages 

	s2 = master->getStatus();
	for(p = 0; (p = cs->findError(&status, p)) != IBatchCompletionState::NO_MORE_ERRORS; ++p)
	{
		try
		{
			cs->getStatus(&status, s2, p);

			char text[1024];
			utl->formatStatus(text, sizeof(text) - 1, s2);
			text[sizeof(text) - 1] = 0;
			if (!pr2)
			{
				printf("\nDetailed errors status:\n", p);
				pr2 = true;
			}
			printf("Message %u: %s\n", p, text);
		}
		catch (const FbException& error)
		{
			// handle error
			fprintf(stderr, "\nError describing message %u\n", p);
			errPrint(error.getStatus());
			fprintf(stderr, "\n");
		}
	}

	if (s2)
		s2->dispose();
}

int main()
{
	int rc = 0;

	// set default password if none specified in environment
	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);

	// With ThrowStatusWrapper passed as status interface FbException will be thrown on error
	ThrowStatusWrapper status(master->getStatus());

	// Declare pointers to required interfaces
	IProvider* prov = master->getDispatcher();
	IUtil* utl = master->getUtilInterface();
	IAttachment* att = NULL;
	ITransaction* tra = NULL;
	IBatch* batch = NULL;
	IBatchCompletionState* cs = NULL;
	IXpbBuilder* pb = NULL;

	unsigned char streamBuf[10240];		// big enough for demo
	unsigned char* stream = NULL;

	try
	{
		// attach employee db
		att = prov->attachDatabase(&status, "employee", 0, NULL);
		tra = att->startTransaction(&status, 0, NULL);

		// cleanup
		att->execute(&status, tra, 0, "delete from project where proj_id like 'BAT%'", SAMPLES_DIALECT,
			NULL, NULL, NULL, NULL);

		//
		printf("\nPart 1. Simple messages. Adding one by one or by groups of messages, cancel batch.\n");
		//

		// Message to store in a table
		FB_MESSAGE(Msg1, ThrowStatusWrapper,
			(FB_VARCHAR(5), id)
			(FB_VARCHAR(10), name)
		) project1(&status, master);
		project1.clear();
		IMessageMetadata* meta = project1.getMetadata();

		// sizes & alignments
		unsigned mesAlign = meta->getAlignment(&status);
		unsigned mesLength = meta->getMessageLength(&status);
		unsigned char* streamStart = align(streamBuf, mesAlign);

		// set batch parameters
		pb = utl->getXpbBuilder(&status, IXpbBuilder::BATCH, NULL, 0);
		// collect per-message statistics
		pb->insertInt(&status, IBatch::TAG_RECORD_COUNTS, 1);

		// create batch
		const char* sqlStmt1 = "insert into project(proj_id, proj_name) values(?, ?)";
		batch = att->createBatch(&status, tra, 0, sqlStmt1, SAMPLES_DIALECT, meta,
			pb->getBufferLength(&status), pb->getBuffer(&status));

		// fill batch with data record by record
		project1->id.set("BAT11");
		project1->name.set("SNGL_REC1");
		batch->add(&status, 1, project1.getData());

		project1->id.set("BAT12");
		project1->name.set("SNGL_REC2");
		batch->add(&status, 1, project1.getData());

		// execute it
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		// add a big set of same records ...
		for (int i = 0; i < 100000; ++i)
		{
			project1->id.set("BAT11");
			project1->name.set("SNGL_REC");
			batch->add(&status, 1, project1.getData());
		}

		// ... and cancel that records
		batch->cancel(&status);

		// fill batch with data using many records at once
		stream = streamStart;

		project1->id.set("BAT13");
		project1->name.set("STRM_REC_A");
		putMsg(stream, project1.getData(), mesLength, mesAlign);

		project1->id.set("BAT14");
		project1->name.set("STRM_REC_B");
		putMsg(stream, project1.getData(), mesLength, mesAlign);

		project1->id.set("BAT15");
		project1->name.set("STRM_REC_C");
		putMsg(stream, project1.getData(), mesLength, mesAlign);

		batch->add(&status, 3, streamStart);

		stream = streamStart;

		project1->id.set("BAT15");		// constraint violation
		project1->name.set("STRM_REC_D");
		putMsg(stream, project1.getData(), mesLength, mesAlign);

		project1->id.set("BAT16");		// will not be processed due to return on single error
		project1->name.set("STRM_REC_E");
		putMsg(stream, project1.getData(), mesLength, mesAlign);

		batch->add(&status, 2, streamStart);

		// execute it
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		// close batch
		batch->close(&status);
		batch = NULL;

		//
		printf("\nPart 2. Simple BLOBs. Multiple errors return.\n");
		//

		// Message to store in a table
		FB_MESSAGE(Msg2, ThrowStatusWrapper,
			(FB_VARCHAR(5), id)
			(FB_VARCHAR(10), name)
			(FB_BLOB, desc)
		) project2(&status, master);
		project2.clear();
		meta = project2.getMetadata();

		mesAlign = meta->getAlignment(&status);
		mesLength = meta->getMessageLength(&status);
		streamStart = align(streamBuf, mesAlign);

		// set batch parameters
		pb->clear(&status);
		// continue batch processing in case of errors in some messages
		pb->insertInt(&status, IBatch::TAG_MULTIERROR, 1);
		// enable blobs processing - IDs generated by firebird engine
		pb->insertInt(&status, IBatch::TAG_BLOB_POLICY, IBatch::BLOB_ID_ENGINE);

		// create batch
		const char* sqlStmt2 = "insert into project(proj_id, proj_name, proj_desc) values(?, ?, ?)";
		batch = att->createBatch(&status, tra, 0, sqlStmt2, SAMPLES_DIALECT, meta,
			pb->getBufferLength(&status), pb->getBuffer(&status));

		// fill batch with data
		project2->id.set("BAT21");
		project2->name.set("SNGL_BLOB");
		batch->addBlob(&status, strlen(sqlStmt2), sqlStmt2, &project2->desc, 0, NULL);
		batch->appendBlobData(&status, 1, "\n");
		batch->appendBlobData(&status, strlen(sqlStmt1), sqlStmt1);
		batch->add(&status, 1, project2.getData());

		// execute it
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		// fill batch with data
		project2->id.set("BAT22");
		project2->name.set("SNGL_REC1");
		batch->addBlob(&status, strlen(sqlStmt2), sqlStmt2, &project2->desc, 0, NULL);
		batch->add(&status, 1, project2.getData());

		project2->id.set("BAT22");
		project2->name.set("SNGL_REC2");	// constraint violation
		batch->addBlob(&status, 2, "r2", &project2->desc, 0, NULL);
		batch->add(&status, 1, project2.getData());

		project2->id.set("BAT23");
		project2->name.set("SNGL_REC3");
		batch->addBlob(&status, 2, "r3", &project2->desc, 0, NULL);
		batch->add(&status, 1, project2.getData());

		project2->id.set("BAT23");			// constraint violation
		project2->name.set("SNGL_REC4");
		batch->addBlob(&status, 2, "r4", &project2->desc, 0, NULL);
		batch->add(&status, 1, project2.getData());

		// execute it
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		// close batch
		batch->close(&status);
		batch = NULL;

		//
		printf("\nPart 3. BLOB stream, including segmented BLOB.\n");
		//

		// use Msg2/project2/sqlStmt2 to store in a table

		// set batch parameters
		pb->clear(&status);
		// enable blobs processing - blobs are placed in a stream
		pb->insertInt(&status, IBatch::TAG_BLOB_POLICY, IBatch::BLOB_STREAM);

		// create batch
		batch = att->createBatch(&status, tra, 0, sqlStmt2, SAMPLES_DIALECT, meta,
			pb->getBufferLength(&status), pb->getBuffer(&status));

		unsigned blobAlign = batch->getBlobAlignment(&status);

		// prepare blob IDs
		ISC_QUAD v1={0,1}, v2={0,2}, v3={0,3};

		// send messages to batch
		project2->id.set("BAT31");
		project2->name.set("STRM_BLB_A");
		project2->desc = v1;
		batch->add(&status, 1, project2.getData());

		project2->id.set("BAT32");
		project2->name.set("STRM_BLB_B");
		project2->desc = v2;
		batch->add(&status, 1, project2.getData());

		project2->id.set("BAT33");
		project2->name.set("STRM_BLB_C");
		project2->desc = v3;
		batch->add(&status, 1, project2.getData());

		// prepare blobs in the stream buffer

		const char* d1 = "1111111111111111111";
		const char* d2 = "22222222222222222222";
		const char* d3 = "33333333333333333333333333333333333333333333333333333";

		stream = streamStart;
		putBlob(stream, d1, strlen(d1), blobAlign, &v1);
		putBlob(stream, d2, strlen(d2), blobAlign, &v2);
		putBlob(stream, d3, strlen(d3), blobAlign, &v3);

		batch->addBlobStream(&status, stream - streamStart, streamStart);

		// Continue last blob
		stream = streamStart;
		ISC_QUAD nullId = {0,0};
		unsigned* size = putBlobHdr(stream, blobAlign, &nullId, 0, NULL);

		const char* d4 = " 444444444444444444444444";
		unsigned ld4 = strlen(d4);

		memcpy(stream, d4, ld4);
		*size += ld4;
		stream += ld4;
		stream = align(stream, blobAlign);

		stream = align(stream, blobAlign);
		batch->addBlobStream(&status, stream - streamStart, streamStart);

		// Put segmented Blob in the stream

		// add message
		ISC_QUAD vSeg={0,10};
		project2->id.set("BAT35");
		project2->name.set("STRM_B_SEG");
		project2->desc = vSeg;
		batch->add(&status, 1, project2.getData());

		// build BPB
		pb->dispose();
		pb = NULL;
		pb = utl->getXpbBuilder(&status, IXpbBuilder::BPB, NULL, 0);
		pb->insertInt(&status, isc_bpb_type, isc_bpb_type_segmented);

		// make stream
		stream = streamStart;
		size = putBlobHdr(stream, blobAlign, &vSeg, pb->getBufferLength(&status), pb->getBuffer(&status));
		*size += putSegment(stream, d1);
		*size += putSegment(stream, "\n");
		*size += putSegment(stream, d2);
		*size += putSegment(stream, "\n");
		*size += putSegment(stream, d3);

		// add stream to the batch
		stream = align(stream, blobAlign);
		batch->addBlobStream(&status, stream - streamStart, streamStart);

		// execute batch
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		//
		printf("\nPart 4. BLOB created using IBlob interface.\n");
		//

		// use Msg2/project2/sqlStmt2 to store in a table
		// registerBlob() may be called in BLOB_STREAM batch, ID should be generated by user in this case
		// also demonstrates execution of same batch multiple times

		// create blob
		ISC_QUAD realId;
		IBlob* blob = att->createBlob(&status, tra, &realId, 0, NULL);
		const char* text = "Blob created using traditional API";
		blob->putSegment(&status, strlen(text), text);
		blob->close(&status);

		// add message
		project2->id.set("BAT38");
		project2->name.set("FRGN_BLB");
		project2->desc = v1;	// after execute may reuse IDs
		batch->registerBlob(&status, &realId, &project2->desc);
		batch->add(&status, 1, project2.getData());

		// execute it
		cs = batch->execute(&status, tra);
		print_cs(status, cs, utl);

		// cleanup
		batch->close(&status);
		batch = NULL;
		tra->commit(&status);
		tra = NULL;
		att->detach(&status);
		att = NULL;
	}
	catch (const FbException& error)
	{
		// handle error
		rc = 1;
		errPrint(error.getStatus());
	}

	// release interfaces after error caught
	if (cs)
		cs->dispose();
	if (batch)
		batch->release();
	if (tra)
		tra->release();
	if (att)
		att->release();

	// cleanup
	if (pb)
		pb->dispose();
	status.dispose();
	prov->release();

	return rc;
}
