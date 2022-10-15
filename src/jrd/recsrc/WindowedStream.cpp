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
 *  Copyright (c) 2009 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../dsql/Nodes.h"
#include "../jrd/mov_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/optimizer/Optimizer.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ------------------------------
// Data access: window expression
// ------------------------------

namespace
{
	// This stream makes possible to reuse a BufferedStream, so each usage maintains a different
	// cursor position.
	class BufferedStreamWindow : public BaseBufferedStream
	{
		struct Impure : public RecordSource::Impure
		{
			FB_UINT64 irsb_position;
		};

	public:
		BufferedStreamWindow(CompilerScratch* csb, BufferedStream* next);

		void internalOpen(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool internalGetRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		WriteLockResult lockRecord(thread_db* tdbb, bool skipLocked) const override;

		void getChildren(Firebird::Array<const RecordSource*>& children) const override;

		void print(thread_db* tdbb, Firebird::string& plan, bool detailed, unsigned level, bool recurse) const override;

		void markRecursive() override;
		void invalidateRecords(Request* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll) const override;
		void nullRecords(thread_db* tdbb) const override;

		void locate(thread_db* tdbb, FB_UINT64 position) const override
		{
			Request* const request = tdbb->getRequest();
			Impure* const impure = request->getImpure<Impure>(m_impure);
			impure->irsb_position = position;
		}

		FB_UINT64 getCount(thread_db* tdbb) const override
		{
			return m_next->getCount(tdbb);
		}

		FB_UINT64 getPosition(Request* request) const override
		{
			Impure* const impure = request->getImpure<Impure>(m_impure);
			return impure->irsb_position;
		}

	public:
		NestConst<BufferedStream> m_next;
	};

	// BufferedStreamWindow implementation

	BufferedStreamWindow::BufferedStreamWindow(CompilerScratch* csb, BufferedStream* next)
		: BaseBufferedStream(csb),
		  m_next(next)
	{
		m_impure = csb->allocImpure<Impure>();
	}

	void BufferedStreamWindow::internalOpen(thread_db* tdbb) const
	{
		Request* const request = tdbb->getRequest();
		Impure* const impure = request->getImpure<Impure>(m_impure);

		impure->irsb_flags = irsb_open;
		impure->irsb_position = 0;
	}

	void BufferedStreamWindow::close(thread_db* tdbb) const
	{
		Request* const request = tdbb->getRequest();

		invalidateRecords(request);

		Impure* const impure = request->getImpure<Impure>(m_impure);

		if (impure->irsb_flags & irsb_open)
			impure->irsb_flags &= ~irsb_open;
	}

	bool BufferedStreamWindow::internalGetRecord(thread_db* tdbb) const
	{
		Request* const request = tdbb->getRequest();
		Impure* const impure = request->getImpure<Impure>(m_impure);

		if (!(impure->irsb_flags & irsb_open))
			return false;

		m_next->locate(tdbb, impure->irsb_position);
		if (!m_next->getRecord(tdbb))
			return false;

		++impure->irsb_position;
		return true;
	}

	bool BufferedStreamWindow::refetchRecord(thread_db* tdbb) const
	{
		return m_next->refetchRecord(tdbb);
	}

	WriteLockResult BufferedStreamWindow::lockRecord(thread_db* tdbb, bool skipLocked) const
	{
		return m_next->lockRecord(tdbb, skipLocked);
	}

	void BufferedStreamWindow::getChildren(Array<const RecordSource*>& children) const
	{
		children.add(m_next);
	}

	void BufferedStreamWindow::print(thread_db* tdbb, string& plan, bool detailed, unsigned level, bool recurse) const
	{
		if (recurse)
			m_next->print(tdbb, plan, detailed, level, recurse);
	}

	void BufferedStreamWindow::markRecursive()
	{
		m_next->markRecursive();
	}

	void BufferedStreamWindow::findUsedStreams(StreamList& streams, bool expandAll) const
	{
		m_next->findUsedStreams(streams, expandAll);
	}

	void BufferedStreamWindow::invalidateRecords(Request* request) const
	{
		m_next->invalidateRecords(request);
	}

	void BufferedStreamWindow::nullRecords(thread_db* tdbb) const
	{
		m_next->nullRecords(tdbb);
	}

	// ------------------------------

	SLONG zero = 0;

	struct InitDsc : public dsc
	{
		InitDsc()
		{
			makeLong(0, &zero);
		}
	} zeroDsc;
}	// namespace

// ------------------------------

WindowedStream::WindowedStream(thread_db* tdbb, Optimizer* opt,
			ObjectsArray<WindowSourceNode::Window>& windows, RecordSource* next)
	: RecordSource(opt->getCompilerScratch()),
	  m_joinedStream(nullptr)
{
	const auto csb = opt->getCompilerScratch();

	m_next = FB_NEW_POOL(csb->csb_pool) BufferedStream(csb, next);
	m_impure = csb->allocImpure<Impure>();
	m_cardinality = next->getCardinality();

	// Process the unpartioned and unordered map, if existent.

	for (auto& window : windows)
	{
		// While here, verify not supported functions/clauses.

		if (window.order || window.frameExtent->unit == FrameExtent::Unit::ROWS)
		{
			for (const auto& source : window.map->sourceList)
			{
				if (const auto aggNode = nodeAs<AggNode>(source))
				{
					const char* arg = nullptr;

					if (aggNode->distinct)
						arg = "DISTINCT";
					else if (!(aggNode->getCapabilities() & AggNode::CAP_SUPPORTS_WINDOW_FRAME))
						arg = aggNode->aggInfo.name;

					if (arg)
					{
						string msg;
						msg.printf("%s is not supported in windows with ORDER BY or frame by ROWS clauses", arg);

						status_exception::raise(
							Arg::Gds(isc_wish_list) <<
							Arg::Gds(isc_random) << msg);
					}
				}
			}
		}

		if (!window.group && !window.order)
		{
			if (!m_joinedStream)
			{
				m_joinedStream = FB_NEW_POOL(csb->csb_pool) WindowStream(tdbb, csb, window.stream,
					nullptr, FB_NEW_POOL(csb->csb_pool) BufferedStreamWindow(csb, m_next),
					nullptr, window.map, window.frameExtent, window.exclusion);
			}
			else
			{
				m_joinedStream = FB_NEW_POOL(csb->csb_pool) WindowStream(tdbb, csb, window.stream,
					nullptr, FB_NEW_POOL(csb->csb_pool) BufferedStream(csb, m_joinedStream),
					nullptr, window.map, window.frameExtent, window.exclusion);
			}

			opt->generateAggregateDistincts(window.map);
		}
	}

	if (!m_joinedStream)
		m_joinedStream = FB_NEW_POOL(csb->csb_pool) BufferedStreamWindow(csb, m_next);

	// Process ordered windows.

	StreamList streams;

	for (auto& window : windows)
	{
#if 0	//// FIXME: This causes problems, for example with FIRST_VALUE.
		//// I think it can be fixed with the help of SlidingWindow.

		// Invert bounds and order if necessary for faster execution.

		// between !{<n> following || unbounded preceding} and unbounded following
		if (window->frameExtent &&
			window->frameExtent->frame2->bound == Frame::Bound::FOLLOWING &&
			!window->frameExtent->frame2->value &&
			!(window->frameExtent->frame1->bound == Frame::Bound::FOLLOWING ||
			  (window->frameExtent->frame1->bound == Frame::Bound::PRECEDING &&
			   !window->frameExtent->frame1->value)))
		{
			if (window->order)
			{
				Array<NullsPlacement>::iterator nullIt = window->order->nullOrder.begin();

				for (Array<SortDirection>::iterator descIt = window->order->direction.begin();
					 descIt != window->order->direction.end();
					 ++descIt, ++nullIt)
				{
					*descIt = !*descIt;

					if (*nullIt == NULLS_FIRST)
						*nullIt = NULLS_LAST;
					else if (*nullIt == NULLS_LAST)
						*nullIt = NULLS_FIRST;
				}
			}

			Frame* temp = window->frameExtent->frame1;
			window->frameExtent->frame1 = window->frameExtent->frame2;
			window->frameExtent->frame2 = temp;

			window->frameExtent->frame1->bound = Frame::Bound::PRECEDING;

			if (window->frameExtent->frame2->bound == Frame::Bound::PRECEDING)
				window->frameExtent->frame2->bound = Frame::Bound::FOLLOWING;
		}
#endif

		// Build the sort key. It's the order items following the window items.

		SortNode* windowOrder;

		if (window.group)
		{
			windowOrder = FB_NEW_POOL(csb->csb_pool) SortNode(csb->csb_pool);
			windowOrder->expressions.join(window.group->expressions);
			windowOrder->direction.join(window.group->direction);
			windowOrder->nullOrder.join(window.group->nullOrder);

			if (window.order)
			{
				windowOrder->expressions.join(window.order->expressions);
				windowOrder->direction.join(window.order->direction);
				windowOrder->nullOrder.join(window.order->nullOrder);
			}
		}
		else
			windowOrder = window.order;

		if (windowOrder)
		{
			// Refresh the stream list based on the last m_joinedStream.
			streams.clear();
			m_joinedStream->findUsedStreams(streams);

			const auto sortedStream =
				opt->generateSort(streams, nullptr, m_joinedStream, windowOrder, false, false);

			m_joinedStream = FB_NEW_POOL(csb->csb_pool) WindowStream(tdbb, csb, window.stream,
				(window.group ? &window.group->expressions : nullptr),
				FB_NEW_POOL(csb->csb_pool) BufferedStream(csb, sortedStream),
				window.order, window.map, window.frameExtent, window.exclusion);

			opt->generateAggregateDistincts(window.map);
		}
	}
}

void WindowedStream::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	m_next->open(tdbb);
	m_joinedStream->open(tdbb);
}

void WindowedStream::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;
		m_joinedStream->close(tdbb);
		m_next->close(tdbb);
	}
}

bool WindowedStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	if (!m_joinedStream->getRecord(tdbb))
		return false;

	return true;
}

bool WindowedStream::refetchRecord(thread_db* tdbb) const
{
	return m_joinedStream->refetchRecord(tdbb);
}

WriteLockResult WindowedStream::lockRecord(thread_db* /*tdbb*/, bool /*skipLocked*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void WindowedStream::getChildren(Array<const RecordSource*>& children) const
{
	children.add(m_joinedStream);
}

void WindowedStream::print(thread_db* tdbb, string& plan, bool detailed, unsigned level, bool recurse) const
{
	if (recurse)
		m_joinedStream->print(tdbb, plan, detailed, level, recurse);
}

void WindowedStream::markRecursive()
{
	m_joinedStream->markRecursive();
}

void WindowedStream::invalidateRecords(Request* request) const
{
	m_joinedStream->invalidateRecords(request);
}

void WindowedStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_joinedStream->findUsedStreams(streams, expandAll);
}

void WindowedStream::nullRecords(thread_db* tdbb) const
{
	m_joinedStream->nullRecords(tdbb);
}

// ------------------------------

// Note that we can have NULL order here, in case of window function with shouldCallWinPass
// returning true, with partition, and without order. Example: ROW_NUMBER() OVER (PARTITION BY N).
WindowedStream::WindowStream::WindowStream(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
			const NestValueArray* group, BaseBufferedStream* next,
			SortNode* order, MapNode* windowMap,
			FrameExtent* frameExtent,
			Exclusion exclusion)
	: BaseAggWinStream(tdbb, csb, stream, group, NULL, false, next),
	  m_order(order),
	  m_windowMap(windowMap),
	  m_frameExtent(frameExtent),
	  m_arithNodes(csb->csb_pool),
	  m_aggSources(csb->csb_pool),
	  m_aggTargets(csb->csb_pool),
	  m_winPassSources(csb->csb_pool),
	  m_winPassTargets(csb->csb_pool),
	  m_exclusion(exclusion),
	  m_invariantOffsets(0)
{
	// Separate nodes that requires the winPass call.

	const NestConst<ValueExprNode>* const sourceEnd = m_windowMap->sourceList.end();

	for (const NestConst<ValueExprNode>* source = m_windowMap->sourceList.begin(),
			*target = m_windowMap->targetList.begin();
		 source != sourceEnd;
		 ++source, ++target)
	{
		const AggNode* aggNode = nodeAs<AggNode>(*source);

		if (aggNode)
		{
			unsigned capabilities = aggNode->getCapabilities();

			if (capabilities & AggNode::CAP_WANTS_AGG_CALLS)
			{
				m_aggSources.add(*source);
				m_aggTargets.add(*target);
			}

			if (capabilities & AggNode::CAP_WANTS_WIN_PASS_CALL)
			{
				m_winPassSources.add(*source);
				m_winPassTargets.add(*target);
			}
		}
	}

	m_arithNodes.resize(2);

	if (m_order)
	{
		dsc dummyDesc;

		for (unsigned i = 0; i < 2; ++i)
		{
			Frame* frame = i == 0 ?
				m_frameExtent->frame1 : m_frameExtent->frame2;

			if (m_frameExtent->unit == FrameExtent::Unit::RANGE && frame->value)
			{
				int direction = frame->bound == Frame::Bound::FOLLOWING ? 1 : -1;

				if (m_order->direction[0] == ORDER_DESC)
					direction *= -1;

				m_arithNodes[i] = FB_NEW_POOL(csb->csb_pool) ArithmeticNode(csb->csb_pool,
					(direction == 1 ? blr_add : blr_subtract),
					(csb->blrVersion == 4),
					m_order->expressions[0],
					frame->value);

				// Set parameters as nodFlags and nodScale
				m_arithNodes[i]->getDesc(tdbb, csb, &dummyDesc);
			}

			//// TODO: Better check for invariants.

			if (frame->value &&
				(nodeIs<LiteralNode>(frame->value) ||
				 nodeIs<VariableNode>(frame->value) ||
				 nodeIs<ParameterNode>(frame->value)))
			{
				m_invariantOffsets |= i == 0 ? 0x1 : 0x2;
			}
		}
	}

	(void) m_exclusion;	// avoid warning
}

void WindowedStream::WindowStream::internalOpen(thread_db* tdbb) const
{
	BaseAggWinStream::internalOpen(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = getImpure(request);

	impure->partitionBlock.startPosition = impure->partitionBlock.endPosition =
		impure->partitionPending = impure->rangePending = 0;
	impure->windowBlock.invalidate();

	unsigned impureCount = m_order ? m_order->expressions.getCount() : 0;

	if (!impure->orderValues && impureCount > 0)
	{
		impure->orderValues = FB_NEW_POOL(*tdbb->getDefaultPool()) impure_value[impureCount];
		memset(impure->orderValues, 0, sizeof(impure_value) * impureCount);
	}

	if (m_invariantOffsets & 0x1)
		getFrameValue(tdbb, request, m_frameExtent->frame1, &impure->startOffset);

	if (m_invariantOffsets & 0x2)
		getFrameValue(tdbb, request, m_frameExtent->frame2, &impure->endOffset);
}

void WindowedStream::WindowStream::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
		aggFinish(tdbb, request, m_windowMap);

	BaseAggWinStream::close(tdbb);
}

bool WindowedStream::WindowStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = getImpure(request);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	const SINT64 position = (SINT64) m_next->getPosition(request);

	if (impure->partitionPending == 0)
	{
		if (m_group)
		{
			if (!evaluateGroup(tdbb))
			{
				rpb->rpb_number.setValid(false);
				return false;
			}
		}
		else
		{
			FB_UINT64 count = m_next->getCount(tdbb);

			if (position != 0 || count == 0)
			{
				rpb->rpb_number.setValid(false);
				return false;
			}

			m_next->locate(tdbb, count);
			impure->state = STATE_EOF;
		}

		impure->partitionBlock.startPosition = position;
		impure->partitionBlock.endPosition = m_next->getPosition(request) - 1 -
			(impure->state == STATE_FETCHED ? 1 : 0);
		impure->partitionPending =
			impure->partitionBlock.endPosition - impure->partitionBlock.startPosition + 1;

		fb_assert(impure->partitionPending > 0);

		m_next->locate(tdbb, position);
		impure->state = STATE_GROUPING;
	}

	if (!m_next->getRecord(tdbb))
		fb_assert(false);

	if (impure->rangePending > 0)
		--impure->rangePending;
	else
	{
		Block lastWindow = impure->windowBlock;

		// Find the window start.

		if (m_frameExtent->frame1->value && !(m_invariantOffsets & 0x1))
			getFrameValue(tdbb, request, m_frameExtent->frame1, &impure->startOffset);

		// {range | rows} between unbounded preceding and ...
		// (no order by) range
		if ((m_frameExtent->frame1->bound == Frame::Bound::PRECEDING && !m_frameExtent->frame1->value) ||
			(!m_order && m_frameExtent->unit == FrameExtent::Unit::RANGE))
		{
			impure->windowBlock.startPosition = impure->partitionBlock.startPosition;
		}
		// rows between current row and ...
		else if (m_frameExtent->unit == FrameExtent::Unit::ROWS &&
			m_frameExtent->frame1->bound == Frame::Bound::CURRENT_ROW)
		{
			impure->windowBlock.startPosition = position;
		}
		// rows between <n> {preceding | following} and ...
		else if (m_frameExtent->unit == FrameExtent::Unit::ROWS &&
			m_frameExtent->frame1->value)
		{
			impure->windowBlock.startPosition = position + impure->startOffset.vlux_count;
		}
		// range between current row and ...
		else if (m_frameExtent->unit == FrameExtent::Unit::RANGE &&
			m_frameExtent->frame1->bound == Frame::Bound::CURRENT_ROW)
		{
			impure->windowBlock.startPosition = position;
		}
		// range between <n> {preceding | following} and ...
		else if (m_frameExtent->unit == FrameExtent::Unit::RANGE &&
			m_frameExtent->frame1->value)
		{
			impure->windowBlock.startPosition = locateFrameRange(tdbb, request, impure,
				m_frameExtent->frame1, &impure->startOffset.vlu_desc, position);
		}
		else
		{
			fb_assert(false);
			return false;
		}

		// Find the window end.

		if (m_frameExtent->frame2->value && !(m_invariantOffsets & 0x2))
			getFrameValue(tdbb, request, m_frameExtent->frame2, &impure->endOffset);

		// {range | rows} between ... and unbounded following
		// (no order by) range
		if ((m_frameExtent->frame2->bound == Frame::Bound::FOLLOWING && !m_frameExtent->frame2->value) ||
			(!m_order && m_frameExtent->unit == FrameExtent::Unit::RANGE))
		{
			impure->windowBlock.endPosition = impure->partitionBlock.endPosition;
		}
		// rows between ... and current row
		else if (m_frameExtent->unit == FrameExtent::Unit::ROWS &&
			m_frameExtent->frame2->bound == Frame::Bound::CURRENT_ROW)
		{
			impure->windowBlock.endPosition = position;
		}
		// rows between ... and <n> {preceding | following}
		else if (m_frameExtent->unit == FrameExtent::Unit::ROWS &&
			m_frameExtent->frame2->value)
		{
			impure->windowBlock.endPosition = position + impure->endOffset.vlux_count;
		}
		// range between ... and current row
		else if (m_frameExtent->unit == FrameExtent::Unit::RANGE &&
			m_frameExtent->frame2->bound == Frame::Bound::CURRENT_ROW)
		{
			SINT64 rangePos = position;
			cacheValues(tdbb, request, &m_order->expressions, impure->orderValues,
				DummyAdjustFunctor());

			while (++rangePos <= impure->partitionBlock.endPosition)
			{
				if (!m_next->getRecord(tdbb))
					fb_assert(false);

				if (lookForChange(tdbb, request, &m_order->expressions, m_order,
						impure->orderValues))
				{
					break;
				}
			}

			impure->windowBlock.endPosition = rangePos - 1;

			m_next->locate(tdbb, position);

			if (!m_next->getRecord(tdbb))
				fb_assert(false);
		}
		// range between ... and <n> {preceding | following}
		else if (m_frameExtent->unit == FrameExtent::Unit::RANGE &&
			m_frameExtent->frame2->value)
		{
			impure->windowBlock.endPosition = locateFrameRange(tdbb, request, impure,
				m_frameExtent->frame2, &impure->endOffset.vlu_desc, position);
		}
		else
		{
			fb_assert(false);
			return false;
		}

		if ((m_frameExtent->frame1->bound == Frame::Bound::PRECEDING && !m_frameExtent->frame1->value &&
			 m_frameExtent->frame2->bound == Frame::Bound::FOLLOWING && !m_frameExtent->frame2->value) ||
		    (m_frameExtent->unit == FrameExtent::Unit::RANGE && !m_order))
		{
			impure->rangePending = MAX(0, impure->windowBlock.endPosition - position);
		}
		else if (m_frameExtent->unit == FrameExtent::Unit::RANGE)
		{
			SINT64 rangePos = position;
			cacheValues(tdbb, request, &m_order->expressions, impure->orderValues,
				DummyAdjustFunctor());

			while (++rangePos <= impure->partitionBlock.endPosition)
			{
				if (!m_next->getRecord(tdbb))
					fb_assert(false);

				if (lookForChange(tdbb, request, &m_order->expressions, m_order,
						impure->orderValues))
				{
					break;
				}
			}

			impure->rangePending = rangePos - position - 1;
		}

		m_next->locate(tdbb, position);

		if (!m_next->getRecord(tdbb))
			fb_assert(false);

		//// TODO: There is no need to pass record by record when m_aggSources.isEmpty()

		if (!impure->windowBlock.isValid() ||
			impure->windowBlock.endPosition < impure->windowBlock.startPosition ||
			impure->windowBlock.startPosition > impure->partitionBlock.endPosition ||
			impure->windowBlock.endPosition < impure->partitionBlock.startPosition)
		{
			if (position == 0 || impure->windowBlock.isValid())
			{
				impure->windowBlock.invalidate();
				aggInit(tdbb, request, m_windowMap);
				aggExecute(tdbb, request, m_aggSources, m_aggTargets);
			}
		}
		else
		{
			impure->windowBlock.startPosition =
				MAX(impure->windowBlock.startPosition, impure->partitionBlock.startPosition);
			impure->windowBlock.endPosition =
				MIN(impure->windowBlock.endPosition, impure->partitionBlock.endPosition);

			// If possible, reuse the last window aggregation.
			//
			// This may be incompatible with some function like LIST, but currently LIST cannot
			// be used in ordered windows anyway.

			if (!lastWindow.isValid() ||
				impure->windowBlock.startPosition > lastWindow.startPosition ||
				impure->windowBlock.endPosition < lastWindow.endPosition)
			{
				aggInit(tdbb, request, m_windowMap);
				m_next->locate(tdbb, impure->windowBlock.startPosition);
			}
			else
			{
				if (impure->windowBlock.startPosition < lastWindow.startPosition)
				{
					m_next->locate(tdbb, impure->windowBlock.startPosition);
					SINT64 pending = lastWindow.startPosition - impure->windowBlock.startPosition;

					while (pending-- > 0)
					{
						if (!m_next->getRecord(tdbb))
							fb_assert(false);

						aggPass(tdbb, request, m_aggSources, m_aggTargets);
					}
				}

				m_next->locate(tdbb, lastWindow.endPosition + 1);
			}

			SINT64 aggPos = (SINT64) m_next->getPosition(request);

			while (aggPos++ <= impure->windowBlock.endPosition)
			{
				if (!m_next->getRecord(tdbb))
					fb_assert(false);

				aggPass(tdbb, request, m_aggSources, m_aggTargets);
			}

			aggExecute(tdbb, request, m_aggSources, m_aggTargets);

			m_next->locate(tdbb, position);

			if (!m_next->getRecord(tdbb))
				fb_assert(false);
		}
	}

	--impure->partitionPending;

	if (m_winPassSources.hasData())
	{
		SlidingWindow window(tdbb, m_next, request,
			impure->partitionBlock.startPosition, impure->partitionBlock.endPosition,
			impure->windowBlock.startPosition, impure->windowBlock.endPosition);
		dsc* desc;

		const NestConst<ValueExprNode>* const sourceEnd = m_winPassSources.end();

		for (const NestConst<ValueExprNode>* source = m_winPassSources.begin(),
				*target = m_winPassTargets.begin();
			 source != sourceEnd;
			 ++source, ++target)
		{
			const AggNode* aggNode = nodeAs<AggNode>(*source);

			const FieldNode* field = nodeAs<FieldNode>(*target);
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

			window.moveWithinPartition(0);
		}
	}

	// If there is no partition, we should reassign the map items.
	if (!m_group)
	{
		const NestConst<ValueExprNode>* const sourceEnd = m_windowMap->sourceList.end();

		for (const NestConst<ValueExprNode>* source = m_windowMap->sourceList.begin(),
				*target = m_windowMap->targetList.begin();
			 source != sourceEnd;
			 ++source, ++target)
		{
			const AggNode* aggNode = nodeAs<AggNode>(*source);

			if (!aggNode)
				EXE_assignment(tdbb, *source, *target);
		}
	}

	rpb->rpb_number.setValid(true);
	return true;
}

void WindowedStream::WindowStream::getChildren(Array<const RecordSource*>& children) const
{
	children.add(m_next);
}

void WindowedStream::WindowStream::print(thread_db* tdbb, string& plan, bool detailed,
	unsigned level, bool recurse) const
{
	if (detailed)
	{
		plan += printIndent(++level) + "Window";
		printOptInfo(plan);
	}

	if (recurse)
		m_next->print(tdbb, plan, detailed, level, recurse);
}

void WindowedStream::WindowStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	BaseAggWinStream::findUsedStreams(streams);

	m_next->findUsedStreams(streams, expandAll);
}

void WindowedStream::WindowStream::nullRecords(thread_db* tdbb) const
{
	BaseAggWinStream::nullRecords(tdbb);

	m_next->nullRecords(tdbb);
}

const void WindowedStream::WindowStream::getFrameValue(thread_db* tdbb, Request* request,
	const Frame* frame, impure_value_ex* impureValue) const
{
	dsc* desc = EVL_expr(tdbb, request, frame->value);
	bool error = false;

	if (request->req_flags & req_null)
		error = true;
	else
	{
		if (m_frameExtent->unit == FrameExtent::Unit::ROWS)
		{
			// Purposedly used 32-bit here. So long distance will complicate things for no gain.
			impureValue->vlux_count = MOV_get_long(tdbb, desc, 0);

			if (impureValue->vlux_count < 0)
				error = true;

			if (frame->bound == Frame::Bound::PRECEDING)
				impureValue->vlux_count = -impureValue->vlux_count;
		}
		else if (MOV_compare(tdbb, desc, &zeroDsc) < 0)
			error = true;

		if (!error)
			EVL_make_value(tdbb, desc, impureValue);
	}

	if (error)
	{
		status_exception::raise(
			Arg::Gds(isc_window_frame_value_invalid));
	}
}

SINT64 WindowedStream::WindowStream::locateFrameRange(thread_db* tdbb, Request* request, Impure* impure,
	const Frame* frame, const dsc* offsetDesc, SINT64 position) const
{
	if (m_order->expressions.getCount() != 1)
	{
		fb_assert(false);
		return false;
	}

	SINT64 rangePos = position;

	if (offsetDesc)
	{
		cacheValues(tdbb, request, &m_order->expressions, impure->orderValues,
			AdjustFunctor(m_arithNodes[frame == m_frameExtent->frame1 ? 0 : 1], offsetDesc));
	}
	else
	{
		cacheValues(tdbb, request, &m_order->expressions, impure->orderValues,
			DummyAdjustFunctor());
	}

	// We found a NULL...
	if (!impure->orderValues[0].vlu_desc.dsc_address)
	{
		if (frame == m_frameExtent->frame2)
		{
			while (++rangePos <= impure->partitionBlock.endPosition)
			{
				if (!m_next->getRecord(tdbb))
					fb_assert(false);

				if (lookForChange(tdbb, request, &m_order->expressions, m_order,
						impure->orderValues))
				{
					break;
				}
			}

			--rangePos;
		}
	}
	else if (frame->bound == Frame::Bound::FOLLOWING)
	{
		const int bound = frame == m_frameExtent->frame1 ? 0 : 1;

		do
		{
			if (lookForChange(tdbb, request, &m_order->expressions, m_order, impure->orderValues) >=
					bound ||
				++rangePos > impure->partitionBlock.endPosition)
			{
				break;
			}

			if (!m_next->getRecord(tdbb))
				fb_assert(false);
		} while (true);

		if (frame == m_frameExtent->frame2)
			--rangePos;
	}
	else
	{
		const int bound = frame == m_frameExtent->frame1 ? -1 : 0;

		do
		{
			if (lookForChange(tdbb, request, &m_order->expressions, m_order, impure->orderValues) <=
					bound ||
				--rangePos < impure->partitionBlock.startPosition)
			{
				break;
			}

			//// FIXME: Going backward may be slow...

			m_next->locate(tdbb, rangePos);

			if (!m_next->getRecord(tdbb))
				fb_assert(false);
		} while (true);

		if (frame == m_frameExtent->frame1)
			++rangePos;
		else if (rangePos >= impure->partitionBlock.startPosition)
		{
			// This should be necessary for the case where offsetDesc is 0.

			while (++rangePos <= impure->partitionBlock.endPosition)
			{
				if (!m_next->getRecord(tdbb))
					fb_assert(false);

				if (lookForChange(tdbb, request, &m_order->expressions, m_order,
						impure->orderValues))
				{
					break;
				}
			}

			--rangePos;
		}
	}

	m_next->locate(tdbb, position);

	if (!m_next->getRecord(tdbb))
		fb_assert(false);

	return rangePos;
}

// ------------------------------

SlidingWindow::SlidingWindow(thread_db* aTdbb, const BaseBufferedStream* aStream,
			Request* request,
			FB_UINT64 aPartitionStart, FB_UINT64 aPartitionEnd,
			FB_UINT64 aFrameStart, FB_UINT64 aFrameEnd)
	: tdbb(aTdbb),	// Note: instantiate the class only as local variable
	  stream(aStream),
	  partitionStart(aPartitionStart),
	  partitionEnd(aPartitionEnd),
	  frameStart(aFrameStart),
	  frameEnd(aFrameEnd),
	  moved(false)
{
	savedPosition = stream->getPosition(request) - 1;
}

SlidingWindow::~SlidingWindow()
{
	if (!moved)
		return;

	// Position the stream where we received it.
	moveWithinPartition(0);
}

// Move in the window without pass partition boundaries.
bool SlidingWindow::moveWithinPartition(SINT64 delta)
{
	const auto newPosition = savedPosition + delta;

	if (newPosition < partitionStart || newPosition > partitionEnd)
		return false;

	moved = true;

	stream->locate(tdbb, newPosition);

	if (!stream->getRecord(tdbb))
	{
		fb_assert(false);
		return false;
	}

	return true;
}

// Move in the window without pass frame boundaries.
bool SlidingWindow::moveWithinFrame(SINT64 delta)
{
	const auto newPosition = savedPosition + delta;

	if (newPosition < frameStart || newPosition > frameEnd)
		return false;

	return moveWithinPartition(delta);
}
