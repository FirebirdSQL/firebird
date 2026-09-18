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

#ifndef BURP_FAST_PATH_RECORD_READER_H
#define BURP_FAST_PATH_RECORD_READER_H

#include "../common/common.h"
#include "../common/classes/auto.h"

struct burp_rel;

namespace Firebird {
	class IAttachment;
	class ITransaction;
}

namespace Burp {

class RestoreMessageLayout;

class FastPathRecordReader
{
private:
	struct Imp;

public:
	FastPathRecordReader();
	~FastPathRecordReader();

	FastPathRecordReader(const FastPathRecordReader&) = delete;
	FastPathRecordReader& operator=(const FastPathRecordReader&) = delete;

public:
	void init(Firebird::IAttachment* att, Firebird::ITransaction* tra,
		const burp_rel* relation, const RestoreMessageLayout& layout, bool partition);

	void start(ULONG loPP, ULONG hiPP);
	bool readRecord(UCHAR* messageBuffer, RCRD_LENGTH msgLength);
	unsigned readRecords(UCHAR* messageBuffer, RCRD_LENGTH msgLength, unsigned maxRecords);

	void clear();

private:
	Firebird::AutoPtr<Imp> imp;
};

} // namespace Burp

#endif // BURP_FAST_PATH_RECORD_READER_H
