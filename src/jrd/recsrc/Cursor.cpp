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

	// Initialize dependent invariants
	void initializeInvariants(Request* request, const VarInvariantArray* invariants)
	{
		if (invariants)
		{
			for (const ULONG* iter = invariants->begin(); iter < invariants->end(); ++iter)
			{
				impure_value* const invariantImpure = request->getImpure<impure_value>(*iter);
				invariantImpure->vlu_flags = 0;
			}
		}
	}
}

// ---------------------
// SubQuery implementation
// ---------------------

SubQuery::SubQuery(const RecordSource* rsb, const VarInvariantArray* invariants)
	: m_top(rsb), m_invariants(invariants)
{
	fb_assert(m_top);
}

void SubQuery::open(thread_db* tdbb) const
{
	initializeInvariants(tdbb->getRequest(), m_invariants);
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

Cursor::Cursor(CompilerScratch* csb, const RecordSource* rsb,
			   const VarInvariantArray* invariants, bool scrollable, bool updateCounters)
	: m_top(rsb), m_invariants(invariants), m_scrollable(scrollable), m_updateCounters(updateCounters)
{
	fb_assert(m_top);

	m_impure = csb->allocImpure<Impure>();
}

void Cursor::open(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* impure = request->getImpure<Impure>(m_impure);

	impure->irsb_active = true;
	impure->irsb_state = BOS;

	initializeInvariants(request, m_invariants);
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
	if (m_scrollable)
		return fetchRelative(tdbb, 1);

	if (!validate(tdbb))
		return false;

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!impure->irsb_active)
	{
		// error: invalid cursor state
		status_exception::raise(Arg::Gds(isc_cursor_not_open));
	}

	if (impure->irsb_state == EOS)
		return false;

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
	if (!m_scrollable)
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("PRIOR"));
	}

	return fetchRelative(tdbb, -1);
}

bool Cursor::fetchFirst(thread_db* tdbb) const
{
	if (!m_scrollable)
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("FIRST"));
	}

	return fetchAbsolute(tdbb, 1);
}

bool Cursor::fetchLast(thread_db* tdbb) const
{
	if (!m_scrollable)
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("LAST"));
	}

	return fetchAbsolute(tdbb, -1);
}

bool Cursor::fetchAbsolute(thread_db* tdbb, SINT64 offset) const
{
	if (!m_scrollable)
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("ABSOLUTE"));
	}

	if (!validate(tdbb))
		return false;

	Request* const request = tdbb->getRequest();
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
	if (!m_scrollable)
	{
		// error: invalid fetch direction
		status_exception::raise(Arg::Gds(isc_invalid_fetch_option) << Arg::Str("RELATIVE"));
	}

	if (!validate(tdbb))
		return false;

	Request* const request = tdbb->getRequest();
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
			Arg::Str(name));
	}
}
