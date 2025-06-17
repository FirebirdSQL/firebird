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
#include "../jrd/btr.h"
#include "../jrd/intl.h"
#include "../jrd/req.h"
#include "../jrd/ProfilerManager.h"
#include "../jrd/tra.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/DataTypeUtil.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// Disabled so far, should be uncommented for debugging/testing
//#define PRINT_OPT_INFO	// print optimizer info (cardinality, cost) in plans


// AccessPath class
// -------------------

AccessPath::AccessPath(CompilerScratch* csb)
	: m_cursorId(csb->csb_currentCursorId),
	  m_recSourceId(csb->csb_nextRecSourceId++)
{
}

void AccessPath::getPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.accessPath = this;
	planEntry.level = level;

	internalGetPlan(tdbb, planEntry, level, recurse);
}


// PlanEntry class
// -------------------

void PlanEntry::getDescriptionAsString(string& str, bool initialIndentation) const
{
	const auto indentLevel = initialIndentation ? level : 0;
	bool firstLine = true;

	for (const auto& line : lines)
	{
		const string indent((indentLevel + line.level) * 4, ' ');

		if (initialIndentation || !firstLine)
			str += "\n" + indent;

		if (level)
			str += "-> ";

		str += line.text;

		firstLine = false;
	}
}

void PlanEntry::asFlatList(Array<NonPooledPair<const PlanEntry*, const PlanEntry*>>& list) const
{
	list.clear();
	list.add({this, nullptr});

	for (unsigned pos = 0; pos < list.getCount(); ++pos)
	{
		const auto thisEntry = list[pos].first;
		unsigned childPos = pos;

		for (const auto& child : thisEntry->children)
			list.insert(++childPos, {&child, thisEntry});
	}
}

void PlanEntry::asString(string& str) const
{
	Array<NonPooledPair<const PlanEntry*, const PlanEntry*>> list;
	asFlatList(list);

	for (const auto& pair : list)
		pair.first->getDescriptionAsString(str, true);
}


// Record source class
// -------------------

RecordSource::RecordSource(CompilerScratch* csb)
	: AccessPath(csb)
{
}

void RecordSource::open(thread_db* tdbb) const
{
	ProfilerManager::RecordSourceStopWatcher profilerRecordSourceStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::OPEN);

	internalOpen(tdbb);
}

bool RecordSource::getRecord(thread_db* tdbb) const
{
	ProfilerManager::RecordSourceStopWatcher profilerRecordSourceStopWatcher(tdbb, this,
		ProfilerManager::RecordSourceStopWatcher::Event::GET_RECORD);

	return internalGetRecord(tdbb);
}

string RecordSource::printName(thread_db* tdbb, const string& name, const string& alias)
{
	if (alias.isEmpty() || name == alias)
		return name;

	string result;
	result.printf("%s as %s", name.c_str(), alias.c_str());
	return result;
}

void RecordSource::printInversion(thread_db* tdbb, const InversionNode* inversion,
	ObjectsArray<PlanEntry::Line>& planLines, bool detailed, unsigned level, bool navigation)
{
	auto plan = &planLines.add();
	plan->level = level;

	switch (inversion->type)
	{
	case InversionNode::TYPE_AND:
		if (detailed)
			plan->text = "Bitmap And";
		printInversion(tdbb, inversion->node1, planLines, detailed, level + 1, false);
		printInversion(tdbb, inversion->node2, planLines, detailed, level + 1, false);
		break;

	case InversionNode::TYPE_OR:
	case InversionNode::TYPE_IN:
		if (detailed)
			plan->text = "Bitmap Or";
		printInversion(tdbb, inversion->node1, planLines, detailed, level + 1, false);
		printInversion(tdbb, inversion->node2, planLines, detailed, level + 1, false);
		break;

	case InversionNode::TYPE_DBKEY:
		if (detailed)
			plan->text = "DBKEY";
		break;

	case InversionNode::TYPE_INDEX:
		{
			const IndexRetrieval* const retrieval = inversion->retrieval;

			QualifiedName indexName;
			if (retrieval->irb_name && retrieval->irb_name->object.hasData())
				indexName = *retrieval->irb_name;
			else
				indexName.object.printf("<index id %d>", retrieval->irb_index + 1);

			if (detailed)
			{
				if (!navigation)
				{
					plan->text = "Bitmap";
					plan = &planLines.add();
					plan->level = level + 1;
				}

				const index_desc& idx = retrieval->irb_desc;
				const USHORT segCount = idx.idx_count;

				const USHORT minSegs = MIN(retrieval->irb_lower_count, retrieval->irb_upper_count);
				const USHORT maxSegs = MAX(retrieval->irb_lower_count, retrieval->irb_upper_count);

				const bool equality = (retrieval->irb_generic & irb_equality);
				const bool unique = (retrieval->irb_generic & irb_unique);
				const bool partial = (retrieval->irb_generic & irb_partial);

				const bool fullscan = (maxSegs == 0);
				const bool list = (retrieval->irb_list != nullptr);

				string bounds;
				if (!unique && !fullscan)
				{
					if (retrieval->irb_lower_count && retrieval->irb_upper_count)
					{
						if (equality)
						{
							if (partial)
								bounds.printf(" (partial match: %d/%d)", maxSegs, segCount);
							else
								bounds.printf(" (full match)");
						}
						else
						{
							bounds.printf(" (lower bound: %d/%d, upper bound: %d/%d)",
										  retrieval->irb_lower_count, segCount,
										  retrieval->irb_upper_count, segCount);
						}
					}
					else if (retrieval->irb_lower_count)
					{
						bounds.printf(" (lower bound: %d/%d)",
									  retrieval->irb_lower_count, segCount);
					}
					else if (retrieval->irb_upper_count)
					{
						bounds.printf(" (upper bound: %d/%d)",
									  retrieval->irb_upper_count, segCount);
					}
				}

				plan->text = "Index " + printName(tdbb, indexName.toQuotedString()) +
					(fullscan ? " Full" : unique ? " Unique" : list ? " List" : " Range") + " Scan" + bounds;
			}
			else
				plan->text = printName(tdbb, indexName.toQuotedString());
		}
		break;

	default:
		fb_assert(false);
	}
}

void RecordSource::printLegacyInversion(thread_db* tdbb, const InversionNode* inversion, string& plan)
{
	ObjectsArray<PlanEntry::Line> planLines;
	printInversion(tdbb, inversion, planLines, false, 0, false);

	for (const auto& line : planLines)
	{
		if (plan.hasData())
			plan += ", ";

		plan += line.text;
	}
}

void RecordSource::printOptInfo(ObjectsArray<PlanEntry::Line>& planLines) const
{
#ifdef PRINT_OPT_INFO
	fb_assert(planLines.hasData());
	string info;
	// Add 0.5 to convert double->int truncation into rounding
	info.printf(" [rows: %" UQUADFORMAT "]", (FB_UINT64) (m_cardinality + 0.5));
	planLines.back().text += info;
#endif
}


// RecordStream class
// ------------------

RecordStream::RecordStream(CompilerScratch* csb, StreamType stream, const Format* format)
	: RecordSource(csb),
	  m_stream(stream),
	  m_format(format ? format : csb->csb_rpt[stream].csb_format)
{
	fb_assert(m_format);
}

bool RecordStream::refetchRecord(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	jrd_tra* const transaction = request->req_transaction;

	record_param* const rpb = &request->req_rpb[m_stream];

	if (rpb->rpb_runtime_flags & RPB_refetch)
	{
		if (VIO_refetch_record(tdbb, rpb, transaction, true, false))
		{
			rpb->rpb_runtime_flags &= ~RPB_refetch;
			return true;
		}
	}

	return false;
}

WriteLockResult RecordStream::lockRecord(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	jrd_tra* const transaction = request->req_transaction;

	record_param* const rpb = &request->req_rpb[m_stream];
	jrd_rel* const relation = rpb->rpb_relation;

	fb_assert(relation && !relation->rel_view_rse);

	RLCK_reserve_relation(tdbb, transaction, relation, true);

	return VIO_writelock(tdbb, rpb, transaction);
}

void RecordStream::markRecursive()
{
	m_recursive = true;
}

void RecordStream::findUsedStreams(StreamList& streams, bool /*expandAll*/) const
{
	if (!streams.exist(m_stream))
		streams.add(m_stream);
}

void RecordStream::invalidateRecords(Request* request) const
{
	record_param* const rpb = &request->req_rpb[m_stream];
	rpb->rpb_number.setValid(false);
}

void RecordStream::nullRecords(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];

	rpb->rpb_number.setValid(false);

	// Make sure a record block has been allocated

	Record* const record = VIO_record(tdbb, rpb, m_format, tdbb->getDefaultPool());

	// Mark the record to look like NULL

	record->fakeNulls();
}
