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
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../burp/FastPathRecordReader.h"
#include "../burp/burp_proto.h"

namespace Burp {


// Complete type for Firebird::AutoPtr<Imp>. The real definition lives in
// burp/engine/FastPathRecordReader.cpp, which is not linked into standalone gbak.
struct FastPathRecordReader::Imp
{
};

FastPathRecordReader::FastPathRecordReader()
	: imp(nullptr)
{
}

FastPathRecordReader::~FastPathRecordReader() = default;

void FastPathRecordReader::init(Firebird::IAttachment*, Firebird::ITransaction*, const burp_rel*,
	const RestoreMessageLayout&, bool)
{
	BURP_error(330, true, "FAST_PATH");
}

void FastPathRecordReader::start(ULONG, ULONG)
{
	fb_assert(false);
}

bool FastPathRecordReader::readRecord(UCHAR*, RCRD_LENGTH)
{
	fb_assert(false);
	return false;
}

unsigned FastPathRecordReader::readRecords(UCHAR*, RCRD_LENGTH, unsigned)
{
	fb_assert(false);
	return 0;
}

void FastPathRecordReader::clear()
{
}


} // namespace Burp
