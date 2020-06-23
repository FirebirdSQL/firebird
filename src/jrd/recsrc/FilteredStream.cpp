/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../dsql/BoolNodes.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/evl_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------------------
// Data access: predicate driven filter
// ------------------------------------

FilteredStream::FilteredStream(CompilerScratch* csb, RecordSource* next, BoolExprNode* boolean)
	: m_next(next), m_boolean(boolean)
{
	fb_assert(m_next && m_boolean);

	m_impure = csb->allocImpure<Impure>();
}

void FilteredStream::open(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	m_next->open(tdbb);
}

void FilteredStream::close(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		m_next->close(tdbb);
	}
}

bool FilteredStream::getRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	while (m_next->getRecord(tdbb))
	{
		if (m_boolean->execute(tdbb, request) == true)
			return true;
	}

	invalidateRecords(request);
	return false;
}

bool FilteredStream::refetchRecord(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();

	return m_next->refetchRecord(tdbb) &&
		m_boolean->execute(tdbb, request) == true;
}

bool FilteredStream::lockRecord(thread_db* tdbb) const
{
	return m_next->lockRecord(tdbb);
}

void FilteredStream::print(thread_db* tdbb, string& plan, bool detailed, unsigned level) const
{
	if (detailed)
		plan += printIndent(++level) + "Filter";

	m_next->print(tdbb, plan, detailed, level);
}

void FilteredStream::markRecursive()
{
	m_next->markRecursive();
}

void FilteredStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_next->findUsedStreams(streams, expandAll);
}

void FilteredStream::invalidateRecords(jrd_req* request) const
{
	m_next->invalidateRecords(request);
}

void FilteredStream::nullRecords(thread_db* tdbb) const
{
	m_next->nullRecords(tdbb);
}
