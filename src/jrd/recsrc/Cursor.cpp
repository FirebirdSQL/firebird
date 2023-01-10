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
#include "../jrd/ProfilerManager.h"
#include "../jrd/cmp_proto.h"

#include "RecordSource.h"
#include "Cursor.h"

using namespace Firebird;
using namespace Jrd;

namespace
{
	bool validate(thread_db* tdbb)
	{
		const Request* const request = tdbb->getRequest();

		if (request->req_flags & req_abort)
			return false;

		if (!request->req_transaction)
			return false;

		return true;
	}
}

// ---------------------
// Select implementation
// ---------------------

void Select::initializeInvariants(Request* request) const
{
	// Initialize dependent invariants, if any

	if (m_rse->rse_invariants)
	{
		for (const auto offset : *m_rse->rse_invariants)
		{
			const auto invariantImpure = request->getImpure<impure_value>(offset);
			invariantImpure->vlu_flags = 0;
		}
	}
}

void Select::printPlan(thread_db* tdbb, string& plan, bool detailed) const
{
	if (detailed)
	{
		if (m_rse->isSubQuery())
		{
			plan += "\nSub-query";

			if (m_rse->isInvariant())
				plan += " (invariant)";
		}
		else if (m_cursorName.hasData())
		{
			plan += "\nCursor \"" + string(m_cursorName) + "\"";

			if (m_rse->isScrollable())
				plan += " (scrollable)";
		}
		else
			plan += "\nSelect Expression";

		if (m_line || m_column)
		{
			string pos;
			pos.printf(" (line %u, column %u)", m_line, m_column);
			plan += pos;
		}
	}
	else
	{
		if (m_line || m_column)
		{
			string pos;
			pos.printf("\n-- line %u, column %u", m_line, m_column);
			plan += pos;
		}

		plan += "\nPLAN ";
	}

	m_top->print(tdbb, plan, detailed, 0, true);
}

// ---------------------
// SubQuery implementation
// ---------------------

SubQuery::SubQuery(const RecordSource* rsb, const RseNode* rse)
	: Select(rsb, rse)
{
	fb_assert(m_top);
}

void SubQuery::open(thread_db* tdbb) const
{
	initializeInvariants(tdbb->getRequest());
	m_top->open(tdbb);
}

void SubQuery::close(thread_db* tdbb) const
{
	m_top->close(tdbb);
}

bool SubQuery::fetch(thread_db* tdbb) const
{
	if (!validate(tdbb))
		return false;

	return m_top->getRecord(tdbb);
}


// ---------------------
// Cursor implementation
// ---------------------

Cursor::Cursor(CompilerScratch* csb, const RecordSource* rsb, const RseNode* rse,
			   bool updateCounters, ULONG line, ULONG column, const MetaName& name)
	: Select(rsb, rse, line, column, name),
	  m_cursorProfileId(rsb->getCursorProfileId()),
	  m_updateCounters(updateCounters)
{
	fb_assert(m_top);

	m_impure = csb->allocImpure<Impure>();
}

void Cursor::open(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	prepareProfiler(tdbb, request);

	Impure* impure = request->getImpure<Impure>(m_impure);

	impure->irsb_active = true;
	impure->irsb_state = BOS;

	initializeInvariants(request);
	m_top->open(tdbb);
}

void Cursor::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_active)
	{
		impure->irsb_active = false;
		m_top->close(tdbb);
	}
}

bool Cursor::fetchNext(thread_db* tdbb) const
{
	if (m_rse->isScrollable())
		return fetchRelative(tdbb, 1);

	if (!validate(tdbb))
		return false;

	const auto request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!impure->irsb_active)
	{
		// error: invalid cursor state
		status_exception::raise(Arg::Gds(isc_cursor_not_open));
	}

	if (impure->irsb_state == EOS)
		return false;

	prepareProfiler(tdbb, request);

	if (!m_top->getRecord(tdbb))
	{
		impure->irsb_state = EOS;
		return false;
	}

	if (m_updateCounters)
	{
		request->req_records_selected++;
		request->req_records_affected.bumpFetched();
	}

	impure->irsb_state = POSITIONED;
	return true;
}

bool Cursor::fetchPrior(thread_db* tdbb) const
{
	if (!m_rse->isScrollable())
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("PRIOR"));
	}

	return fetchRelative(tdbb, -1);
}

bool Cursor::fetchFirst(thread_db* tdbb) const
{
	if (!m_rse->isScrollable())
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("FIRST"));
	}

	return fetchAbsolute(tdbb, 1);
}

bool Cursor::fetchLast(thread_db* tdbb) const
{
	if (!m_rse->isScrollable())
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("LAST"));
	}

	return fetchAbsolute(tdbb, -1);
}

bool Cursor::fetchAbsolute(thread_db* tdbb, SINT64 offset) const
{
	if (!m_rse->isScrollable())
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("ABSOLUTE"));
	}

	if (!validate(tdbb))
		return false;

	const auto request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!impure->irsb_active)
	{
		// error: invalid cursor state
		status_exception::raise(Arg::Gds(isc_cursor_not_open));
	}

	if (!offset)
	{
		impure->irsb_state = BOS;
		return false;
	}

	const auto buffer = static_cast<const BufferedStream*>(m_top);
	const auto count = buffer->getCount(tdbb);
	const SINT64 position = (offset > 0) ? offset - 1 : count + offset;

	if (position < 0)
	{
		impure->irsb_state = BOS;
		return false;
	}
	else if (position >= (SINT64) count)
	{
		impure->irsb_state = EOS;
		return false;
	}

	prepareProfiler(tdbb, request);

	impure->irsb_position = position;
	buffer->locate(tdbb, impure->irsb_position);

	if (!buffer->getRecord(tdbb))
	{
		fb_assert(false); // this should not happen
		impure->irsb_state = (offset > 0) ? EOS : BOS;
		return false;
	}

	if (m_updateCounters)
	{
		request->req_records_selected++;
		request->req_records_affected.bumpFetched();
	}

	impure->irsb_state = POSITIONED;
	return true;
}

bool Cursor::fetchRelative(thread_db* tdbb, SINT64 offset) const
{
	if (!m_rse->isScrollable())
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("RELATIVE"));
	}

	if (!validate(tdbb))
		return false;

	const auto request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!impure->irsb_active)
	{
		// error: invalid cursor state
		status_exception::raise(Arg::Gds(isc_cursor_not_open));
	}

	if (!offset)
		return (impure->irsb_state == POSITIONED);

	const auto buffer = static_cast<const BufferedStream*>(m_top);
	const auto count = buffer->getCount(tdbb);
	SINT64 position = impure->irsb_position;

	if (impure->irsb_state == BOS)
	{
		if (offset < 0)
			return false;

		position = offset - 1;
	}
	else if (impure->irsb_state == EOS)
	{
		if (offset > 0)
			return false;

		position = count + offset;
	}
	else
	{
		position += offset;
	}

	if (position < 0)
	{
		impure->irsb_state = BOS;
		return false;
	}
	else if (position >= (SINT64) count)
	{
		impure->irsb_state = EOS;
		return false;
	}

	prepareProfiler(tdbb, request);

	impure->irsb_position = position;
	buffer->locate(tdbb, impure->irsb_position);

	if (!buffer->getRecord(tdbb))
	{
		fb_assert(false); // this should not happen
		impure->irsb_state = (offset > 0) ? EOS : BOS;
		return false;
	}

	if (m_updateCounters)
	{
		request->req_records_selected++;
		request->req_records_affected.bumpFetched();
	}

	impure->irsb_state = POSITIONED;
	return true;
}

// Check if the cursor is in a good state for access a field.
void Cursor::checkState(Request* request) const
{
	const Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!impure->irsb_active)
	{
		// error: invalid cursor state
		status_exception::raise(Arg::Gds(isc_cursor_not_open));
	}

	if (impure->irsb_state != Cursor::POSITIONED)
	{
		status_exception::raise(
			Arg::Gds(isc_cursor_not_positioned) <<
			Arg::Str(m_cursorName));
	}
}

void Cursor::prepareProfiler(thread_db* tdbb, Request* request) const
{
	const auto attachment = tdbb->getAttachment();

	const auto profilerManager = attachment->isProfilerActive() && !request->hasInternalStatement() ?
		attachment->getProfilerManager(tdbb) :
		nullptr;

	if (profilerManager)
		profilerManager->prepareCursor(tdbb, request, this);
}
