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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2013 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */


#ifndef JRD_REPLICATION_PROTOCOL_H
#define JRD_REPLICATION_PROTOCOL_H

namespace Replication
{
	// Supported protocol versions
	const USHORT PROTOCOL_VERSION_1 = 1;
	const USHORT PROTOCOL_CURRENT_VERSION = PROTOCOL_VERSION_1;

	// Global (protocol neutral) flags
	const USHORT BLOCK_BEGIN_TRANS	= 0x0001;
	const USHORT BLOCK_END_TRANS	= 0x0002;

	struct Block
	{
		FB_UINT64 traNumber;
		USHORT protocol;
		USHORT flags;
		ULONG length;
	};

	static_assert(sizeof(struct Block) == 16, "struct Block size mismatch");
	static_assert(offsetof(struct Block, traNumber) == 0, "traNumber offset mismatch");
	static_assert(offsetof(struct Block, protocol) == 8, "protocol offset mismatch");
	static_assert(offsetof(struct Block, flags) == 10, "flags offset mismatch");
	static_assert(offsetof(struct Block, length) == 12, "length offset mismatch");

	enum Operation: UCHAR
	{
		opStartTransaction = 1,
		opPrepareTransaction = 2,
		opCommitTransaction = 3,
		opRollbackTransaction = 4,
		opCleanupTransaction = 5,

		opStartSavepoint = 6,
		opReleaseSavepoint = 7,
		opRollbackSavepoint = 8,

		opInsertRecord = 9,
		opUpdateRecord =  10,
		opDeleteRecord = 11,
		opStoreBlob = 12,
		opExecuteSql = 13,
		opSetSequence = 14,
		opExecuteSqlIntl = 15,

		opDefineAtom = 16
	};

} // namespace

#endif // JRD_REPLICATION_PROTOCOL_H

