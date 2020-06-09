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
#include "../jrd/cmp_proto.h"
#include "../jrd/vio_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------------
// Data access: single row stream
// ------------------------------

SingularStream::SingularStream(CompilerScratch* csb, RecordSource* next)
	: m_next(next), m_streams(csb->csb_pool)
{
	fb_assert(m_next);

	m_next->findUsedStreams(m_streams);

	m_impure = CMP_impure(csb, sizeof(Impure));
}

void SingularStream::open(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	m_next->open(tdbb);
}

void SingularStream::close(thread_db* tdbb) const
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

bool SingularStream::getRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	if (impure->irsb_flags & irsb_singular_processed)
		return false;

	if (m_next->getRecord(tdbb))
	{
		doGetRecord(tdbb);
		return true;
	}

	return false;
}

void SingularStream::doGetRecord(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	const FB_SIZE_T streamCount = m_streams.getCount();
	MemoryPool& pool = *tdbb->getDefaultPool();
	HalfStaticArray<record_param, 16> rpbs(pool, streamCount);

	for (FB_SIZE_T i = 0; i < streamCount; i++)
	{
		rpbs.add(request->req_rpb[m_streams[i]]);
		record_param& rpb = rpbs.back();
		Record* const orgRecord = rpb.rpb_record;

		if (orgRecord)
			rpb.rpb_record = FB_NEW_POOL(pool) Record(pool, orgRecord);
	}

	if (m_next->getRecord(tdbb))
		status_exception::raise(Arg::Gds(isc_sing_select_err));

	for (FB_SIZE_T i = 0; i < streamCount; i++)
	{
		record_param& rpb = request->req_rpb[m_streams[i]];
		Record* orgRecord = rpb.rpb_record;
		rpb = rpbs[i];
		const AutoPtr<Record> newRecord(rpb.rpb_record);

		if (newRecord)
		{
			if (!orgRecord)
				BUGCHECK(284);	// msg 284 cannot restore singleton select data

			rpb.rpb_record = orgRecord;
			orgRecord->copyFrom(newRecord);
		}
	}

	impure->irsb_flags |= irsb_singular_processed;
}

bool SingularStream::refetchRecord(thread_db* tdbb) const
{
	return m_next->refetchRecord(tdbb);
}

bool SingularStream::lockRecord(thread_db* tdbb) const
{
	return m_next->lockRecord(tdbb);
}

void SingularStream::print(thread_db* tdbb, string& plan, bool detailed, unsigned level) const
{
	if (detailed)
		plan += printIndent(++level) + "Singularity Check";

	m_next->print(tdbb, plan, detailed, level);
}

void SingularStream::markRecursive()
{
	m_next->markRecursive();
}

void SingularStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_next->findUsedStreams(streams, expandAll);
}

void SingularStream::invalidateRecords(jrd_req* request) const
{
	m_next->invalidateRecords(request);
}

void SingularStream::nullRecords(thread_db* tdbb) const
{
	m_next->nullRecords(tdbb);
}
