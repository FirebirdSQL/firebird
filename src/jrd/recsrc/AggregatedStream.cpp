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

// Note that we can have NULL order here, in case of window function with shouldCallWinPass
// returning true, with partition, and without order. Example: ROW_NUMBER() OVER (PARTITION BY N).
AggregatedStream::AggregatedStream(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
			const NestValueArray* group, MapNode* map, BaseBufferedStream* next,
			const NestValueArray* order)
	: RecordStream(csb, stream),
	  m_bufferedStream(next),
	  m_next(m_bufferedStream),
	  m_group(group),
	  m_map(map),
	  m_order(order),
	  m_winPassSources(csb->csb_pool),
	  m_winPassTargets(csb->csb_pool)
{
	init(tdbb, csb);
}

AggregatedStream::AggregatedStream(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
			const NestValueArray* group, MapNode* map, RecordSource* next)
	: RecordStream(csb, stream),
	  m_bufferedStream(NULL),
	  m_next(next),
	  m_group(group),
	  m_map(map),
	  m_order(NULL),
	  m_winPassSources(csb->csb_pool),
	  m_winPassTargets(csb->csb_pool)
{
	init(tdbb, csb);
}

void AggregatedStream::open(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	impure->state = STATE_GROUPING;
	impure->lastGroup = false;
	impure->partitionBlock.startPosition = impure->partitionBlock.endPosition =
		impure->partitionBlock.pending = 0;
	impure->orderBlock = impure->partitionBlock;

	VIO_record(tdbb, &request->req_rpb[m_stream], m_format, tdbb->getDefaultPool());

	unsigned impureCount = m_group ? m_group->getCount() : 0;
	impureCount += m_order ? m_order->getCount() : 0;

	if (!impure->impureValues && impureCount > 0)
	{
		impure->impureValues = FB_NEW_POOL(*tdbb->getDefaultPool()) impure_value[impureCount];
		memset(impure->impureValues, 0, sizeof(impure_value) * impureCount);
	}

	m_next->open(tdbb);
}

void AggregatedStream::close(thread_db* tdbb) const
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

bool AggregatedStream::getRecord(thread_db* tdbb) const
{
	if (--tdbb->tdbb_quantum < 0)
		JRD_reschedule(tdbb, 0, true);

	jrd_req* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	if (m_bufferedStream)	// Is that a window stream?
	{
		const FB_UINT64 position = m_bufferedStream->getPosition(request);

		if (impure->orderBlock.pending == 0)
		{
			if (impure->partitionBlock.pending == 0)
			{
				if (impure->lastGroup || !evaluateGroup(tdbb, AGG_TYPE_GROUP, MAX_UINT64))
				{
					rpb->rpb_number.setValid(false);
					return false;
				}

				impure->partitionBlock.startPosition = position;
				impure->partitionBlock.endPosition = m_bufferedStream->getPosition(request) - 1 -
					(impure->state == STATE_FETCHED ? 1 : 0);
				impure->partitionBlock.pending =
					impure->partitionBlock.endPosition - impure->partitionBlock.startPosition + 1;

				fb_assert(impure->partitionBlock.pending > 0);

				m_bufferedStream->locate(tdbb, position);
				impure->state = STATE_GROUPING;

				impure->lastGroup = impure->state == STATE_EOF;
			}

			// Check if we need to re-aggregate by the ORDER BY clause.
			if (!m_order && m_winPassSources.isEmpty())
				impure->orderBlock = impure->partitionBlock;
			else
			{
				if (!evaluateGroup(tdbb, AGG_TYPE_ORDER, impure->partitionBlock.pending))
					fb_assert(false);

				impure->orderBlock.startPosition = position;
				impure->orderBlock.endPosition = m_bufferedStream->getPosition(request) - 1 -
					(impure->state == STATE_FETCHED ? 1 : 0);
				impure->orderBlock.pending =
					impure->orderBlock.endPosition - impure->orderBlock.startPosition + 1;

				fb_assert(impure->orderBlock.pending > 0);
			}

			m_bufferedStream->locate(tdbb, position);
			impure->state = STATE_GROUPING;
		}

		fb_assert(impure->orderBlock.pending > 0 && impure->partitionBlock.pending > 0);

		--impure->orderBlock.pending;
		--impure->partitionBlock.pending;

		if (m_winPassSources.hasData())
		{
			SlidingWindow window(tdbb, m_bufferedStream, m_group, request,
				impure->partitionBlock.startPosition, impure->partitionBlock.endPosition,
				impure->orderBlock.startPosition, impure->orderBlock.endPosition);
			dsc* desc;

			const NestConst<ValueExprNode>* const sourceEnd = m_winPassSources.end();

			for (const NestConst<ValueExprNode>* source = m_winPassSources.begin(),
					*target = m_winPassTargets.begin();
				 source != sourceEnd;
				 ++source, ++target)
			{
				const AggNode* aggNode = (*source)->as<AggNode>();

				const FieldNode* field = (*target)->as<FieldNode>();
				const USHORT id = field->fieldId;
				Record* record = request->req_rpb[field->fieldStream].rpb_record;

				desc = aggNode->winPass(tdbb, request, &window);

				if (!desc)
					record->setNull(id);
				else
				{
					MOV_move(tdbb, desc, EVL_assign_to(tdbb, *target));
					record->clearNull(id);
				}
			}
		}

		if (!m_bufferedStream->getRecord(tdbb))
			fb_assert(false);

		// If there is no group, we should reassign the map items.
		if (!m_group)
		{
			const NestConst<ValueExprNode>* const sourceEnd = m_map->sourceList.end();

			for (const NestConst<ValueExprNode>* source = m_map->sourceList.begin(),
					*target = m_map->targetList.begin();
				 source != sourceEnd;
				 ++source, ++target)
			{
				const AggNode* aggNode = (*source)->as<AggNode>();

				if (!aggNode)
					EXE_assignment(tdbb, *source, *target);
			}
		}
	}
	else
	{
		if (!evaluateGroup(tdbb, AGG_TYPE_GROUP, MAX_UINT64))
		{
			rpb->rpb_number.setValid(false);
			return false;
		}
	}

	rpb->rpb_number.setValid(true);
	return true;
}

bool AggregatedStream::refetchRecord(thread_db* tdbb) const
{
	return m_next->refetchRecord(tdbb);
}

bool AggregatedStream::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
	return false; // compiler silencer
}

void AggregatedStream::print(thread_db* tdbb, string& plan,
							 bool detailed, unsigned level) const
{
	if (detailed)
		plan += printIndent(++level) + (m_bufferedStream ? "Window" : "Aggregate");

	m_next->print(tdbb, plan, detailed, level);
}

void AggregatedStream::markRecursive()
{
	m_next->markRecursive();
}

void AggregatedStream::invalidateRecords(jrd_req* request) const
{
	m_next->invalidateRecords(request);
}

void AggregatedStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	RecordStream::findUsedStreams(streams);

	if (expandAll)
		m_next->findUsedStreams(streams, true);

	if (m_bufferedStream)
		m_bufferedStream->findUsedStreams(streams, expandAll);
}

void AggregatedStream::nullRecords(thread_db* tdbb) const
{
	RecordStream::nullRecords(tdbb);

	if (m_bufferedStream)
		m_bufferedStream->nullRecords(tdbb);
}

void AggregatedStream::init(thread_db* tdbb, CompilerScratch* csb)
{
	fb_assert(m_map && m_next);
	m_impure = CMP_impure(csb, sizeof(Impure));

	// Separate nodes that requires the winPass call.

	NestConst<ValueExprNode>* const sourceEnd = m_map->sourceList.end();

	for (NestConst<ValueExprNode>* source = m_map->sourceList.begin(),
			*target = m_map->targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		AggNode* aggNode = (*source)->as<AggNode>();

		if (aggNode)
		{
			aggNode->ordered = m_order != NULL;

			bool wantWinPass = false;
			aggNode->aggSetup(wantWinPass);

			if (wantWinPass)
			{
				m_winPassSources.add(*source);
				m_winPassTargets.add(*target);
			}
		}
	}
}

// Compute the next aggregated record of a value group.
bool AggregatedStream::evaluateGroup(thread_db* tdbb, AggType aggType, FB_UINT64 limit) const
{
	const NestValueArray* const group = aggType == AGG_TYPE_GROUP ? m_group : m_order;
	unsigned groupOffset = aggType == AGG_TYPE_GROUP || !m_group ? 0 : m_group->getCount();
	jrd_req* const request = tdbb->getRequest();

	if (--tdbb->tdbb_quantum < 0)
		JRD_reschedule(tdbb, 0, true);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	// if we found the last record last time, we're all done
	if (impure->state == STATE_EOF)
		return false;

	try
	{
		if (aggType == AGG_TYPE_GROUP || group != NULL)
			aggInit(tdbb, request, aggType);

		// If there isn't a record pending, open the stream and get one

		if (!getNextRecord(tdbb, request, limit))
		{
			impure->state = STATE_EOF;

			if (group || m_bufferedStream)
			{
				finiDistinct(tdbb, request);
				return false;
			}
		}
		else
			cacheValues(tdbb, request, group, groupOffset);

		// Loop thru records until either a value change or EOF

		while (impure->state == STATE_GROUPING)
		{
			if ((aggType == AGG_TYPE_GROUP || group != NULL) && !aggPass(tdbb, request))
				impure->state = STATE_EOF;
			else if (getNextRecord(tdbb, request, limit))
			{
				// In the case of a group by, look for a change in value of any of
				// the columns; if we find one, stop aggregating and return what we have.

				if (lookForChange(tdbb, request, group, groupOffset))
					impure->state = STATE_FETCHED;
			}
			else
				impure->state = STATE_EOF;
		}

		if (aggType == AGG_TYPE_GROUP || group != NULL)
			aggExecute(tdbb, request);
	}
	catch (const Exception&)
	{
		finiDistinct(tdbb, request);
		throw;
	}

	return true;
}

// Initialize the aggregate record
void AggregatedStream::aggInit(thread_db* tdbb, jrd_req* request, AggType aggType) const
{
	const NestConst<ValueExprNode>* const sourceEnd = m_map->sourceList.end();

	for (const NestConst<ValueExprNode>* source = m_map->sourceList.begin(),
			*target = m_map->targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		const AggNode* aggNode = (*source)->as<AggNode>();

		if (aggNode)
			aggNode->aggInit(tdbb, request, aggType);
		else if ((*source)->is<LiteralNode>())
			EXE_assignment(tdbb, *source, *target);
	}
}

// Go through and compute all the aggregates on this record
bool AggregatedStream::aggPass(thread_db* tdbb, jrd_req* request) const
{
	bool ret = true;
	const NestConst<ValueExprNode>* const sourceEnd = m_map->sourceList.end();

	for (const NestConst<ValueExprNode>* source = m_map->sourceList.begin(),
			*target = m_map->targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		const AggNode* aggNode = (*source)->as<AggNode>();

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

void AggregatedStream::aggExecute(thread_db* tdbb, jrd_req* request) const
{
	const NestConst<ValueExprNode>* const sourceEnd = m_map->sourceList.end();

	for (const NestConst<ValueExprNode>* source = m_map->sourceList.begin(),
			*target = m_map->targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		const AggNode* aggNode = (*source)->as<AggNode>();

		if (aggNode)
		{
			const FieldNode* field = (*target)->as<FieldNode>();
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

bool AggregatedStream::getNextRecord(thread_db* tdbb, jrd_req* request, FB_UINT64& limit) const
{
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (limit == 0)
		return false;
	else if (impure->state == STATE_FETCHED)
	{
		impure->state = STATE_GROUPING;
		return true;
	}
	else if (m_next->getRecord(tdbb))
	{
		--limit;
		return true;
	}
	else
		return false;
}

// Cache the values of a group/order in the impure.
inline void AggregatedStream::cacheValues(thread_db* tdbb, jrd_req* request,
	const NestValueArray* group, unsigned impureOffset) const
{
	if (!group)
		return;

	Impure* const impure = request->getImpure<Impure>(m_impure);

	for (const NestConst<ValueExprNode>* ptrValue = group->begin(), *endValue = group->end();
		 ptrValue != endValue;
		 ++ptrValue, ++impureOffset)
	{
		const ValueExprNode* from = *ptrValue;
		impure_value* target = &impure->impureValues[impureOffset];

		dsc* desc = EVL_expr(tdbb, request, from);

		if (request->req_flags & req_null)
			target->vlu_desc.dsc_address = NULL;
		else
			EVL_make_value(tdbb, desc, target);
	}
}

// Look for change in the values of a group/order.
inline bool AggregatedStream::lookForChange(thread_db* tdbb, jrd_req* request,
	const NestValueArray* group, unsigned impureOffset) const
{
	if (!group)
		return false;

	Impure* const impure = request->getImpure<Impure>(m_impure);

	for (const NestConst<ValueExprNode>* ptrValue = group->begin(), *endValue = group->end();
		 ptrValue != endValue;
		 ++ptrValue, ++impureOffset)
	{
		const ValueExprNode* from = *ptrValue;
		impure_value* vtemp = &impure->impureValues[impureOffset];

		dsc* desc = EVL_expr(tdbb, request, from);

		if (request->req_flags & req_null)
		{
			if (vtemp->vlu_desc.dsc_address)
				return true;
		}
		else if (!vtemp->vlu_desc.dsc_address || MOV_compare(&vtemp->vlu_desc, desc) != 0)
			return true;
	}

	return false;
}

// Finalize a sort for distinct aggregate
void AggregatedStream::finiDistinct(thread_db* tdbb, jrd_req* request) const
{
	const NestConst<ValueExprNode>* const sourceEnd = m_map->sourceList.end();

	for (const NestConst<ValueExprNode>* source = m_map->sourceList.begin();
		 source != sourceEnd;
		 ++source)
	{
		const AggNode* aggNode = (*source)->as<AggNode>();

		if (aggNode)
			aggNode->aggFinish(tdbb, request);
	}
}


SlidingWindow::SlidingWindow(thread_db* aTdbb, const BaseBufferedStream* aStream,
			const NestValueArray* aGroup, jrd_req* aRequest,
			FB_UINT64 aPartitionStart, FB_UINT64 aPartitionEnd,
			FB_UINT64 aOrderStart, FB_UINT64 aOrderEnd)
	: tdbb(aTdbb),	// Note: instanciate the class only as local variable
	  stream(aStream),
	  group(aGroup),
	  request(aRequest),
	  partitionStart(aPartitionStart),
	  partitionEnd(aPartitionEnd),
	  orderStart(aOrderStart),
	  orderEnd(aOrderEnd),
	  moved(false)
{
	savedPosition = stream->getPosition(request);
}

SlidingWindow::~SlidingWindow()
{
	if (!moved)
		return;

	for (impure_value* impure = partitionKeys.begin(); impure != partitionKeys.end(); ++impure)
		delete impure->vlu_string;

	// Position the stream where we received it.
	stream->locate(tdbb, savedPosition);
}

// Move in the window without pass partition boundaries.
bool SlidingWindow::move(SINT64 delta)
{
	const SINT64 newPosition = SINT64(savedPosition) + delta;

	// If we try to go out of bounds, no need to check the partition.
	if (newPosition < 0 || newPosition >= (SINT64) stream->getCount(tdbb))
		return false;

	if (!group)
	{
		// No partition, we may go everywhere.

		moved = true;

		stream->locate(tdbb, newPosition);

		if (!stream->getRecord(tdbb))
		{
			fb_assert(false);
			return false;
		}

		return true;
	}

	if (!moved)
	{
		// This is our first move. We should cache the partition values, so subsequente moves didn't
		// need to evaluate them again.

		if (!stream->getRecord(tdbb))
		{
			fb_assert(false);
			return false;
		}

		try
		{
			impure_value* impure = partitionKeys.getBuffer(group->getCount());
			memset(impure, 0, sizeof(impure_value) * group->getCount());

			const NestConst<ValueExprNode>* const end = group->end();
			dsc* desc;

			for (const NestConst<ValueExprNode>* ptr = group->begin(); ptr < end; ++ptr, ++impure)
			{
				const ValueExprNode* from = *ptr;
				desc = EVL_expr(tdbb, request, from);

				if (request->req_flags & req_null)
					impure->vlu_desc.dsc_address = NULL;
				else
					EVL_make_value(tdbb, desc, impure);
			}
		}
		catch (const Exception&)
		{
			stream->locate(tdbb, savedPosition);	// Reposition for a new try.
			throw;
		}

		moved = true;
	}

	stream->locate(tdbb, newPosition);

	if (!stream->getRecord(tdbb))
	{
		fb_assert(false);
		return false;
	}

	// Verify if we're still inside the same partition.

	impure_value* impure = partitionKeys.begin();
	dsc* desc;
	const NestConst<ValueExprNode>* const end = group->end();

	for (const NestConst<ValueExprNode>* ptr = group->begin(); ptr != end; ++ptr, ++impure)
	{
		const ValueExprNode* from = *ptr;
		desc = EVL_expr(tdbb, request, from);

		if (request->req_flags & req_null)
		{
			if (impure->vlu_desc.dsc_address)
				return false;
		}
		else
		{
			if (!impure->vlu_desc.dsc_address || MOV_compare(&impure->vlu_desc, desc) != 0)
				return false;
		}
	}

	return true;
}
