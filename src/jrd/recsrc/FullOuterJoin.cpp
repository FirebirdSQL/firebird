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
#include "../jrd/evl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/vio_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ----------------------------
// Data access: full outer join
// ----------------------------

FullOuterJoin::FullOuterJoin(CompilerScratch* csb, RecordSource* arg1, RecordSource* arg2)
	: RecordSource(csb),
	  m_arg1(arg1),
	  m_arg2(arg2)
{
	fb_assert(m_arg1 && m_arg2);

	m_impure = csb->allocImpure<Impure>();
}

void FullOuterJoin::internalOpen(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open | irsb_first;

	m_arg1->open(tdbb);
}

void FullOuterJoin::close(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		if (impure->irsb_flags & irsb_first)
			m_arg1->close(tdbb);
		else
			m_arg2->close(tdbb);
	}
}

bool FullOuterJoin::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	if (impure->irsb_flags & irsb_first)
	{
		if (m_arg1->getRecord(tdbb))
			return true;

		impure->irsb_flags &= ~irsb_first;
		m_arg1->close(tdbb);
		m_arg2->open(tdbb);
	}

	return m_arg2->getRecord(tdbb);
}

bool FullOuterJoin::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

bool FullOuterJoin::lockRecord(thread_db* tdbb) const
{
	SET_TDBB(tdbb);

	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
	return false; // compiler silencer
}

void FullOuterJoin::getChildren(Array<const RecordSource*>& children) const
{
	children.add(m_arg1);
	children.add(m_arg2);
}

void FullOuterJoin::print(thread_db* tdbb, string& plan, bool detailed, unsigned level, bool recurse) const
{
	if (detailed)
	{
		plan += printIndent(++level) + "Full Outer Join";

		if (recurse)
		{
			m_arg1->print(tdbb, plan, true, level, recurse);
			m_arg2->print(tdbb, plan, true, level, recurse);
		}
	}
	else
	{
		level++;
		plan += "JOIN (";
		m_arg1->print(tdbb, plan, false, level, recurse);
		plan += ", ";
		m_arg2->print(tdbb, plan, false, level, recurse);
		plan += ")";
	}
}

void FullOuterJoin::markRecursive()
{
	m_arg1->markRecursive();
	m_arg2->markRecursive();
}

void FullOuterJoin::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_arg1->findUsedStreams(streams, expandAll);
	m_arg2->findUsedStreams(streams, expandAll);
}

void FullOuterJoin::invalidateRecords(jrd_req* request) const
{
	m_arg1->invalidateRecords(request);
	m_arg2->invalidateRecords(request);
}

void FullOuterJoin::nullRecords(thread_db* tdbb) const
{
	m_arg1->nullRecords(tdbb);
	m_arg2->nullRecords(tdbb);
}
