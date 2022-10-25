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
 *  Copyright (c) 2009 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/vio_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------------------
// Data access: stream locked for write
// ------------------------------------

LockedStream::LockedStream(CompilerScratch* csb, RecordSource* next, bool skipLocked)
	: RecordSource(csb),
	  m_next(next),
	  m_skipLocked(skipLocked)
{
	fb_assert(m_next);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = next->getCardinality();
}

void LockedStream::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	m_next->open(tdbb);
}

void LockedStream::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		m_next->close(tdbb);
	}
}

bool LockedStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	while (m_next->getRecord(tdbb))
	{
		do {
			// Attempt to lock the record
			const auto lockResult = m_next->lockRecord(tdbb, m_skipLocked);

			if (lockResult == WriteLockResult::LOCKED)
				return true;	// locked

			if (lockResult == WriteLockResult::SKIPPED)
				break;	// skip locked record

			// Refetch the record and ensure it still fulfils the search condition
		} while (m_next->refetchRecord(tdbb));
	}

	return false;
}

bool LockedStream::refetchRecord(thread_db* tdbb) const
{
	return m_next->refetchRecord(tdbb);
}

WriteLockResult LockedStream::lockRecord(thread_db* tdbb, bool skipLocked) const
{
	return m_next->lockRecord(tdbb, skipLocked);
}

void LockedStream::getChildren(Array<const RecordSource*>& children) const
{
	children.add(m_next);
}

void LockedStream::print(thread_db* tdbb, string& plan, bool detailed, unsigned level, bool recurse) const
{
	if (detailed)
	{
		plan += printIndent(++level) + "Write Lock";
		printOptInfo(plan);
	}

	if (recurse)
		m_next->print(tdbb, plan, detailed, level, recurse);
}

void LockedStream::markRecursive()
{
	m_next->markRecursive();
}

void LockedStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_next->findUsedStreams(streams, expandAll);
}

void LockedStream::invalidateRecords(Request* request) const
{
	m_next->invalidateRecords(request);
}

void LockedStream::nullRecords(thread_db* tdbb) const
{
	m_next->nullRecords(tdbb);
}
