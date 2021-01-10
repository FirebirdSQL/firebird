/*
 *	PROGRAM:		JRD access method
 *	MODULE:			DbCreators.h
 *	DESCRIPTION:	GRANT CREATE DATABASE right (in security.db)
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2014 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef JRD_DB_CREATORS
#define JRD_DB_CREATORS

#include "../common/classes/alloc.h"
#include "../common/classes/fb_string.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/Monitoring.h"

namespace Jrd {

bool checkCreateDatabaseGrant(const Firebird::MetaString& userName, const Firebird::MetaString& trustedRole,
	const Firebird::MetaString& sqlRole, const char* securityDb);

class DbCreatorsScan: public VirtualTableScan
{
public:
	DbCreatorsScan(CompilerScratch* csb, const Firebird::string& alias,
					  StreamType stream, jrd_rel* relation)
		: VirtualTableScan(csb, alias, stream, relation)
	{}

protected:
	const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const override;
	bool retrieveRecord(thread_db* tdbb, jrd_rel* relation, FB_UINT64 position,
		Record* record) const override;
};

class DbCreatorsList : public SnapshotData
{
public:
	explicit DbCreatorsList(jrd_tra* tra);

	RecordBuffer* getList(thread_db* tdbb, jrd_rel* relation);

private:
	RecordBuffer* makeBuffer(thread_db* tdbb);
};

} // namespace Jrd


#endif // JRD_DB_CREATORS
