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
#include "../jrd/rse.h"
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


// Record source class
// -------------------

string RecordSource::printName(thread_db* tdbb, const string& name, bool quote)
{
	const UCHAR* namePtr = (const UCHAR*) name.c_str();
	ULONG nameLength = (ULONG) name.length();

	MoveBuffer nameBuffer;

	const CHARSET_ID charset = tdbb->getCharSet();
	if (charset != CS_METADATA && charset != CS_NONE)
	{
		const ULONG bufferLength = INTL_convert_bytes(tdbb, charset, NULL, 0,
													  CS_METADATA, namePtr, nameLength, ERR_post);
		nameBuffer.getBuffer(bufferLength);
		nameLength = INTL_convert_bytes(tdbb, charset, nameBuffer.begin(), bufferLength,
										CS_METADATA, namePtr, nameLength, ERR_post);

		namePtr = nameBuffer.begin();
	}

	const string result(namePtr, nameLength);
	return quote ? "\"" + result + "\"" : result;
}

string RecordSource::printName(thread_db* tdbb, const string& name, const string& alias)
{
	if (name == alias || alias.isEmpty())
		return printName(tdbb, name, true);

	const string arg1 = printName(tdbb, name, true);
	const string arg2 = printName(tdbb, alias, true);

	string result;
	result.printf("%s as %s", arg1.c_str(), arg2.c_str());
	return result;
}

string RecordSource::printIndent(unsigned level)
{
	fb_assert(level);

	const string indent(level * 4, ' ');
	return string("\n" + indent + "-> ");
}

void RecordSource::printInversion(thread_db* tdbb, const InversionNode* inversion,
								  string& plan, bool detailed, unsigned level, bool navigation)
{
	if (detailed)
		plan += printIndent(++level);

	switch (inversion->type)
	{
	case InversionNode::TYPE_AND:
		if (detailed)
			plan += "Bitmap And";
		printInversion(tdbb, inversion->node1, plan, detailed, level);
		printInversion(tdbb, inversion->node2, plan, detailed, level);
		break;

	case InversionNode::TYPE_OR:
	case InversionNode::TYPE_IN:
		if (detailed)
			plan += "Bitmap Or";
		printInversion(tdbb, inversion->node1, plan, detailed, level);
		printInversion(tdbb, inversion->node2, plan, detailed, level);
		break;

	case InversionNode::TYPE_DBKEY:
		if (detailed)
			plan += "DBKEY";
		break;

	case InversionNode::TYPE_INDEX:
		{
			const IndexRetrieval* const retrieval = inversion->retrieval;

			MetaName indexName;
			if (retrieval->irb_name && retrieval->irb_name->hasData())
				indexName = *retrieval->irb_name;
			else
				indexName.printf("<index id %d>", retrieval->irb_index + 1);

			if (detailed)
			{
				if (!navigation)
					plan += "Bitmap" + printIndent(++level);

				const index_desc& idx = retrieval->irb_desc;
				const bool uniqueIdx = (idx.idx_flags & idx_unique);
				const USHORT segCount = idx.idx_count;

				const USHORT minSegs = MIN(retrieval->irb_lower_count, retrieval->irb_upper_count);
				const USHORT maxSegs = MAX(retrieval->irb_lower_count, retrieval->irb_upper_count);

				const bool equality = (retrieval->irb_generic & irb_equality);
				const bool partial = (retrieval->irb_generic & irb_partial);

				const bool fullscan = (maxSegs == 0);
				const bool unique = uniqueIdx && equality && (minSegs == segCount);

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

				plan += "Index " + printName(tdbb, indexName.c_str()) +
					(fullscan ? " Full" : unique ? " Unique" : " Range") + " Scan" + bounds;
			}
			else
			{
				plan += (plan.hasData() ? ", " : "") + printName(tdbb, indexName.c_str(), false);
			}
		}
		break;

	default:
		fb_assert(false);
	}
}

RecordSource::~RecordSource()
{
}


// RecordStream class
// ------------------

RecordStream::RecordStream(CompilerScratch* csb, StreamType stream, const Format* format)
	: m_stream(stream), m_format(format ? format : csb->csb_rpt[stream].csb_format)
{
	fb_assert(m_format);
}

bool RecordStream::refetchRecord(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
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

bool RecordStream::lockRecord(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
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

void RecordStream::invalidateRecords(jrd_req* request) const
{
	record_param* const rpb = &request->req_rpb[m_stream];
	rpb->rpb_number.setValid(false);
}

void RecordStream::nullRecords(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];

	rpb->rpb_number.setValid(false);

	// Make sure a record block has been allocated

	Record* const record = VIO_record(tdbb, rpb, m_format, tdbb->getDefaultPool());

	// Mark the record to look like NULL

	record->fakeNulls();
}
