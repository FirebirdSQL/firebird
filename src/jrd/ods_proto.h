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

	bool isSupported(const header_page* hdr);

	// NS: ODS code logic should never depend on host platform pointer size.
	// this is why data type for these things is ULONG (32-bit unsigned integer)
	ULONG bytesBitPIP(ULONG page_size);
	ULONG pagesPerPIP(ULONG page_size);
	ULONG pagesPerSCN(ULONG page_size);
	ULONG maxPagesPerSCN(ULONG page_size);
	ULONG transPerTIP(ULONG page_size);
	ULONG gensPerPage(ULONG page_size);
	ULONG dataPagesPerPP(ULONG page_size);
	ULONG maxRecsPerDP(ULONG page_size);
	ULONG maxIndices(ULONG page_size);

	TraNumber getNT(const header_page* page);
	TraNumber getOIT(const header_page* page);
	TraNumber getOAT(const header_page* page);
	TraNumber getOST(const header_page* page);

	void writeNT(header_page* page, TraNumber number);
	void writeOIT(header_page* page, TraNumber number);
	void writeOAT(header_page* page, TraNumber number);
	void writeOST(header_page* page, TraNumber number);

	TraNumber getTraNum(const void* ptr);
	void writeTraNum(void* ptr, TraNumber number, FB_SIZE_T header_size);

	AttNumber getAttID(const header_page* page);
	void writeAttID(header_page* page, AttNumber number);

} // namespace

#endif //ODS_PROTO_H
