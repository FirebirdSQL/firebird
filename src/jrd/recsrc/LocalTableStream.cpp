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
 *  Copyright (c) 2021 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/align.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/optimizer/Optimizer.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------
// Data access: local table
// ------------------------

LocalTableStream::LocalTableStream(CompilerScratch* csb, StreamType stream, const DeclareLocalTableNode* table)
	: RecordStream(csb, stream),
	  m_table(table)
{
	fb_assert(m_table);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = DEFAULT_CARDINALITY;
}

void LocalTableStream::open(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	const auto rpb = &request->req_rpb[m_stream];
	rpb->getWindow(tdbb).win_flags = 0;

	rpb->rpb_number.setValue(BOF_NUMBER);
}

void LocalTableStream::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	invalidateRecords(request);

	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
		impure->irsb_flags &= ~irsb_open;
}

bool LocalTableStream::getRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	const auto request = tdbb->getRequest();
	const auto rpb = &request->req_rpb[m_stream];
	const auto impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	if (!rpb->rpb_record)
		rpb->rpb_record = FB_NEW_POOL(*tdbb->getDefaultPool()) Record(*tdbb->getDefaultPool(), m_format);

	rpb->rpb_number.increment();

	if (!m_table->getImpure(tdbb, request)->recordBuffer->fetch(rpb->rpb_number.getValue(), rpb->rpb_record))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	return true;
}

bool LocalTableStream::refetchRecord(thread_db* tdbb) const
{
	return true;
}

bool LocalTableStream::lockRecord(thread_db* tdbb) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
	return false;	// compiler silencer
}

void LocalTableStream::print(thread_db* tdbb, string& plan, bool detailed, unsigned level) const
{
	//// TODO: Use Local Table name/alias.

	if (detailed)
	{
		plan += printIndent(++level) + "Local Table Full Scan";
		printOptInfo(plan);
	}
	else
	{
		if (!level)
			plan += "(";

		plan += "Local_Table";
		plan += " NATURAL";

		if (!level)
			plan += ")";
	}
}
