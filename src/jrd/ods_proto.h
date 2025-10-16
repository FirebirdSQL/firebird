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
 *  Copyright (c) 2005 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef ODS_PROTO_H
#define ODS_PROTO_H

namespace Ods {

	bool isSupported(const header_page* hdr) noexcept;

	// NS: ODS code logic should never depend on host platform pointer size.
	// this is why data type for these things is ULONG (32-bit unsigned integer)
	ULONG bytesBitPIP(ULONG page_size) noexcept;
	ULONG pagesPerPIP(ULONG page_size) noexcept;
	ULONG pagesPerSCN(ULONG page_size) noexcept;
	ULONG maxPagesPerSCN(ULONG page_size) noexcept;
	ULONG transPerTIP(ULONG page_size) noexcept;
	ULONG gensPerPage(ULONG page_size) noexcept;
	ULONG dataPagesPerPP(ULONG page_size) noexcept;
	ULONG maxRecsPerDP(ULONG page_size) noexcept;
	ULONG maxIndices(ULONG page_size) noexcept;

	TraNumber getTraNum(const void* ptr) noexcept;
	void writeTraNum(void* ptr, TraNumber number, FB_SIZE_T header_size) noexcept;

} // namespace

#endif //ODS_PROTO_H
