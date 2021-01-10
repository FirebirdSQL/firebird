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
#include "../dsql/Nodes.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/Attachment.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------
// Data access: aggregation
// ------------------------

template <typename ThisType, typename NextType>
BaseAggWinStream<ThisType, NextType>::BaseAggWinStream(thread_db* tdbb, CompilerScratch* csb,
			StreamType stream, const NestValueArray* group, MapNode* groupMap,
			bool oneRowWhenEmpty, NextType* next)
	: RecordStream(csb, stream),
	  m_next(next),
	  m_group(group),
	  m_groupMap(groupMap),
	  m_oneRowWhenEmpty(oneRowWhenEmpty)
{
	fb_assert(m_next);
	m_impure = csb->allocImpure<typename ThisType::Impure>();
}

template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::open(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = getImpure(request);

	impure->irsb_flags = irsb_open;

	impure->state = STATE_GROUPING;

	VIO_record(tdbb, &request->req_rpb[m_stream], m_format, tdbb->getDefaultPool());

	unsigned impureCount = m_group ? m_group->getCount() : 0;

	if (!impure->groupValues && impureCount > 0)
	{
		impure->groupValues = FB_NEW_POOL(*tdbb->getDefaultPool()) impure_value[impureCount];
		memset(impure->groupValues, 0, sizeof(impure_value) * impureCount);
	}

	m_next->open(tdbb);
}

template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::close(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = getImpure(request);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		m_next->close(tdbb);
	}
}

template <typename ThisType, typename NextType>
bool BaseAggWinStream<ThisType, NextType>::refetchRecord(thread_db* tdbb) const
{
	return m_next->refetchRecord(tdbb);
}

template <typename ThisType, typename NextType>
bool BaseAggWinStream<ThisType, NextType>::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
	return false; // compiler silencer
}

template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::markRecursive()
{
	m_next->markRecursive();
}

template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::invalidateRecords(jrd_req* request) const
{
	m_next->invalidateRecords(request);
}

template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::findUsedStreams(StreamList& streams,
	bool expandAll) const
{
	RecordStream::findUsedStreams(streams);

	if (expandAll)
		m_next->findUsedStreams(streams, true);
}

// Compute the next aggregated record of a value group.
template <typename ThisType, typename NextType>
bool BaseAggWinStream<ThisType, NextType>::evaluateGroup(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();

	JRD_reschedule(tdbb);

	Impure* const impure = getImpure(request);

	// if we found the last record last time, we're all done
	if (impure->state == STATE_EOF)
		return false;

	try
	{
		if (m_groupMap)
			aggInit(tdbb, request, m_groupMap);

		// If there isn't a record pending, open the stream and get one

		if (!getNextRecord(tdbb, request))
		{
			impure->state = STATE_EOF;

			if (!m_oneRowWhenEmpty)
			{
				if (m_groupMap)
					aggFinish(tdbb, request, m_groupMap);
				return false;
			}
		}
		else
			cacheValues(tdbb, request, m_group, impure->groupValues, DummyAdjustFunctor());

		// Loop thru records until either a value change or EOF

		while (impure->state == STATE_GROUPING)
		{
			if (m_groupMap && !aggPass(tdbb, request, m_groupMap->sourceList, m_groupMap->targetList))
				impure->state = STATE_EOF;
			else if (getNextRecord(tdbb, request))
			{
				// In the case of a group by, look for a change in value of any of
				// the columns; if we find one, stop aggregating and return what we have.

				if (lookForChange(tdbb, request, m_group, NULL, impure->groupValues))
					impure->state = STATE_FETCHED;
			}
			else
				impure->state = STATE_EOF;
		}

		if (m_groupMap)
			aggExecute(tdbb, request, m_groupMap->sourceList, m_groupMap->targetList);
	}
	catch (const Exception&)
	{
		if (m_groupMap)
			aggFinish(tdbb, request, m_groupMap);
		throw;
	}

	return true;
}

// Initialize the aggregate record
template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::aggInit(thread_db* tdbb, jrd_req* request,
	const MapNode* map) const
{
	const NestConst<ValueExprNode>* const sourceEnd = map->sourceList.end();

	for (const NestConst<ValueExprNode>* source = map->sourceList.begin(),
			*target = map->targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		const AggNode* aggNode = nodeAs<AggNode>(*source);

		if (aggNode)
			aggNode->aggInit(tdbb, request);
		else if (nodeIs<LiteralNode>(*source))
			EXE_assignment(tdbb, *source, *target);
	}
}

// Go through and compute all the aggregates on this record
template <typename ThisType, typename NextType>
bool BaseAggWinStream<ThisType, NextType>::aggPass(thread_db* tdbb, jrd_req* request,
	const NestValueArray& sourceList, const NestValueArray& targetList) const
{
	bool ret = true;
	const NestConst<ValueExprNode>* const sourceEnd = sourceList.end();

	for (const NestConst<ValueExprNode>* source = sourceList.begin(),
			*target = targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		const AggNode* aggNode = nodeAs<AggNode>(*source);

		if (aggNode)
		{
			if (aggNode->aggPass(tdbb, request))
			{
				// If a max or min has been mapped to an index, then the first record is the EOF.
				if (aggNode->indexed)
					ret = false;
			}
		}
		else
			EXE_assignment(tdbb, *source, *target);
	}

	return ret;
}

template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::aggExecute(thread_db* tdbb, jrd_req* request,
	const NestValueArray& sourceList, const NestValueArray& targetList) const
{
	const NestConst<ValueExprNode>* const sourceEnd = sourceList.end();

	for (const NestConst<ValueExprNode>* source = sourceList.begin(),
			*target = targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		const AggNode* aggNode = nodeAs<AggNode>(*source);

		if (aggNode)
		{
			const FieldNode* field = nodeAs<FieldNode>(*target);
			const USHORT id = field->fieldId;
			Record* record = request->req_rpb[field->fieldStream].rpb_record;

			dsc* desc = aggNode->execute(tdbb, request);
			if (!desc || !desc->dsc_dtype)
				record->setNull(id);
			else
			{
				MOV_move(tdbb, desc, EVL_assign_to(tdbb, *target));
				record->clearNull(id);
			}
		}
	}
}

// Finalize a sort for distinct aggregate
template <typename ThisType, typename NextType>
void BaseAggWinStream<ThisType, NextType>::aggFinish(thread_db* tdbb, jrd_req* request,
	const MapNode* map) const
{
	const NestConst<ValueExprNode>* const sourceEnd = map->sourceList.end();

	for (const NestConst<ValueExprNode>* source = map->sourceList.begin();
		 source != sourceEnd;
		 ++source)
	{
		const AggNode* aggNode = nodeAs<AggNode>(*source);

		if (aggNode)
			aggNode->aggFinish(tdbb, request);
	}
}

// Look for change in the values of a group/order.
template <typename ThisType, typename NextType>
int BaseAggWinStream<ThisType, NextType>::lookForChange(thread_db* tdbb, jrd_req* request,
	const NestValueArray* group, const SortNode* sort, impure_value* values) const
{
	if (!group)
		return false;

	Impure* const impure = getImpure(request);

	for (const NestConst<ValueExprNode>* ptrValue = group->begin(), *endValue = group->end();
		 ptrValue != endValue;
		 ++ptrValue)
	{
		int sortDirection = 1;
		int nullsPlacement = 1;

		if (sort)
		{
			unsigned index = ptrValue - group->begin();

			if (sort->direction[index] == ORDER_DESC)
				sortDirection = -1;

			if (sort->getEffectiveNullOrder(index) == NULLS_LAST)
				nullsPlacement = -1;
		}

		const ValueExprNode* from = *ptrValue;
		impure_value* vtemp = &values[ptrValue - group->begin()];

		dsc* desc = EVL_expr(tdbb, request, from);
		int n;

		if (request->req_flags & req_null)
		{
			if (vtemp->vlu_desc.dsc_address)
				return -1 * nullsPlacement;
		}
		else if (!vtemp->vlu_desc.dsc_address)
			return 1 * nullsPlacement;
		else if ((n = MOV_compare(tdbb, desc, &vtemp->vlu_desc)) != 0)
			return n * sortDirection;
	}

	return 0;
}

template <typename ThisType, typename NextType>
bool BaseAggWinStream<ThisType, NextType>::getNextRecord(thread_db* tdbb, jrd_req* request) const
{
	Impure* const impure = getImpure(request);

	if (impure->state == STATE_FETCHED)
	{
		impure->state = STATE_GROUPING;
		return true;
	}
	else
		return m_next->getRecord(tdbb);
}

// Export the template for WindowedStream::WindowStream.
template class Jrd::BaseAggWinStream<WindowedStream::WindowStream, BaseBufferedStream>;

// ------------------------------

AggregatedStream::AggregatedStream(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
			const NestValueArray* group, MapNode* map, RecordSource* next)
	: BaseAggWinStream(tdbb, csb, stream, group, map, !group, next)
{
	fb_assert(map);
}

void AggregatedStream::print(thread_db* tdbb, string& plan, bool detailed, unsigned level) const
{
	if (detailed)
		plan += printIndent(++level) + "Aggregate";

	m_next->print(tdbb, plan, detailed, level);
}

bool AggregatedStream::getRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	jrd_req* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = getImpure(request);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	if (!evaluateGroup(tdbb))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	rpb->rpb_number.setValid(true);
	return true;
}
