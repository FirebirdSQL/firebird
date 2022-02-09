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
 *  Copyright (c) 2015 Dmitry Yemanov <dimitrf@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_CURSOR_H
#define DSQL_CURSOR_H

#include "../jrd/TempSpace.h"

namespace Jrd {

class DsqlDmlRequest;
class JResultSet;

class DsqlCursor
{
	enum State { BOS, POSITIONED, EOS };

public:
	DsqlCursor(DsqlDmlRequest* req, ULONG flags);
	~DsqlCursor();

	jrd_tra* getTransaction() const;
	Attachment* getAttachment() const;
	void setInterfacePtr(JResultSet* interfacePtr) throw();

	static void close(thread_db* tdbb, DsqlCursor* cursor);

	int fetchNext(thread_db* tdbb, UCHAR* buffer);
	int fetchPrior(thread_db* tdbb, UCHAR* buffer);
	int fetchFirst(thread_db* tdbb, UCHAR* buffer);
	int fetchLast(thread_db* tdbb, UCHAR* buffer);
	int fetchAbsolute(thread_db* tdbb, UCHAR* buffer, SLONG position);
	int fetchRelative(thread_db* tdbb, UCHAR* buffer, SLONG offset);

	bool isBof() const
	{
		return (m_state == BOS);
	}

	bool isEof() const
	{
		return (m_state == EOS);
	}

private:
	int fetchFromCache(thread_db* tdbb, UCHAR* buffer, FB_UINT64 position);
	bool cacheInput(thread_db* tdbb, FB_UINT64 position = MAX_UINT64);

	DsqlDmlRequest* const m_request;
	const dsql_msg* const m_message;
	JResultSet* m_resultSet;
	const ULONG m_flags;
	TempSpace m_space;
	State m_state;
	bool m_eof;
	FB_UINT64 m_position;
	FB_UINT64 m_cachedCount;
};

} // namespace

#endif // DSQL_CURSOR_H
