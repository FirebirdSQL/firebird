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

	class ProfilerSelectPrepare final
	{
	public:
		ProfilerSelectPrepare(thread_db* tdbb, const Select* select)
			: profilerManager(tdbb->getAttachment()->getActiveProfilerManagerForNonInternalStatement(tdbb))
		{
			const auto request = tdbb->getRequest();

			if (profilerManager)
				profilerManager->prepareCursor(tdbb, request, select);
		}

	public:
		ProfilerManager* const profilerManager;
	};

	class ProfilerSelectStopWatcher
	{
	public:
		ProfilerSelectStopWatcher(thread_db* tdbb, const Select* select, ProfilerManager::RecordSourceStopWatcher::Event event)
			: profilerSelectPrepare(tdbb, select),
			  recordSourceStopWatcher(tdbb, profilerSelectPrepare.profilerManager, select, event)
		{
		}

	private:
		ProfilerSelectPrepare profilerSelectPrepare;
		ProfilerManager::RecordSourceStopWatcher recordSourceStopWatcher;
	};
}

// ---------------------
// Select implementation
// ---------------------

Select::Select(CompilerScratch* csb, const RecordSource* source, const RseNode* rse, ULONG line, ULONG column,
			const MetaName& cursorName)
	: AccessPath(csb),
	  m_root(source),
	  m_rse(rse),
	  m_cursorName(cursorName),
	  m_line(line),
	  m_column(column)
{
}

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

void Select::getLegacyPlan(thread_db* tdbb, Firebird::string& plan, unsigned level) const
{
	if (m_line || m_column)
	{
		string pos;
		pos.printf("\n-- line %u, column %u", m_line, m_column);
		plan += pos;
	}

	plan += "\nPLAN ";
	m_root->getLegacyPlan(tdbb, plan, level);
}

void Select::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "Select";

	if (m_rse->isSubQuery())
	{
		planEntry.lines.add().text = "Sub-query";

		if (m_rse->isInvariant())
			planEntry.lines.back().text += " (invariant)";
	}
	else if (m_cursorName.hasData())
	{
		planEntry.lines.add().text = "Cursor \"" + string(m_cursorName) + "\"";

		if (m_rse->isScrollable())
			planEntry.lines.back().text += " (scrollable)";
	}
	else
		planEntry.lines.add().text = "Select Expression";

	if (m_line || m_column)
	{
		string pos;
		pos.printf(" (line %u, column %u)", m_line, m_column);
		planEntry.lines.back().text += pos;
	}

	if (recurse)
		m_root->getPlan(tdbb, planEntry.children.add(), level + 1, recurse);
}

// ---------------------
// SubQuery implementation
// ---------------------

SubQuery::SubQuery(CompilerScratch* csb, const RecordSource* rsb, const RseNode* rse)
	: Select(csb, rsb, rse, rse->line, rse->column)
{
	fb_assert(m_root);
}

void SubQuery::open(thread_db* tdbb) const
{
	ProfilerSelectStopWatcher profilerSelectStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::OPEN);

	initializeInvariants(tdbb->getRequest());
	m_root->open(tdbb);
}

void SubQuery::close(thread_db* tdbb) const
{
	m_root->close(tdbb);
}

bool SubQuery::fetch(thread_db* tdbb) const
{
	if (!validate(tdbb))
		return false;

	ProfilerSelectStopWatcher profilerSelectStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::GET_RECORD);

	return m_root->getRecord(tdbb);
}


// ---------------------
// Cursor implementation
// ---------------------

Cursor::Cursor(CompilerScratch* csb, const RecordSource* rsb, const RseNode* rse,
			   bool updateCounters, ULONG line, ULONG column, const MetaName& name)
	: Select(csb, rsb, rse, line, column, name),
	  m_updateCounters(updateCounters)
{
	fb_assert(m_root);

	m_impure = csb->allocImpure<Impure>();
}

void Cursor::open(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	ProfilerSelectStopWatcher profilerSelectStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::OPEN);

	Impure* impure = request->getImpure<Impure>(m_impure);

	impure->irsb_active = true;
	impure->irsb_state = BOS;

	initializeInvariants(request);
	m_root->open(tdbb);
}

void Cursor::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_active)
	{
		impure->irsb_active = false;
		m_root->close(tdbb);
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

	ProfilerSelectStopWatcher profilerSelectStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::GET_RECORD);

	if (!m_root->getRecord(tdbb))
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

	const auto buffer = static_cast<const BufferedStream*>(m_root);
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

	ProfilerSelectStopWatcher profilerSelectStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::GET_RECORD);

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

	const auto buffer = static_cast<const BufferedStream*>(m_root);
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

	ProfilerSelectStopWatcher profilerSelectStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::GET_RECORD);

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
			getName().toQuotedString());
	}
}
