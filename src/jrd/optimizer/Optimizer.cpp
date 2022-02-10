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
 * 2002.10.12: Nickolay Samofatov: Fixed problems with wrong results produced by
 *            outer joins
 * 2001.07.28: John Bellardo: Added code to handle rse_skip nodes.
 * 2001.07.17 Claudio Valderrama: Stop crash with indices and recursive calls
 *            of OPT_compile: indicator csb_indices set to zero after used memory is
 *            returned to the free pool.
 * 2001.02.15: Claudio Valderrama: Don't obfuscate the plan output if a selectable
 *             stored procedure doesn't access tables, views or other procedures directly.
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 * 2002.10.30: Arno Brinkman: Changes made to gen_retrieval, OPT_compile and make_inversion.
 *             Procedure sort_indices added. The changes in gen_retrieval are that now
 *             an index with high field-count has priority to build an index from.
 *             Procedure make_inversion is changed so that it not pick every index
 *             that comes away, this was slow performance with bad selectivity indices
 *             which most are foreign_keys with a reference to a few records.
 * 2002.11.01: Arno Brinkman: Added match_indices for better support of OR handling
 *             in INNER JOIN (gen_join) statements.
 * 2002.12.15: Arno Brinkman: Added find_used_streams, so that inside opt_compile all the
 *             streams are marked active. This causes that more indices can be used for
 *             a retrieval. With this change BUG SF #219525 is solved too.
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/align.h"
#include "../jrd/val.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/lls.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/sort.h"
#include "../jrd/ini.h"
#include "../jrd/intl.h"
#include "../jrd/Collation.h"
#include "../common/gdsassert.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/cvt2_proto.h"
#include "../jrd/dpm_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/DataTypeUtil.h"
#include "../jrd/KeywordsTable.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/VirtualTable.h"
#include "../jrd/Monitoring.h"
#include "../jrd/TimeZone.h"
#include "../jrd/UserManagement.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/os/os_utils.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/recsrc/Cursor.h"
#include "../jrd/Mapping.h"
#include "../jrd/DbCreators.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/ConfigTable.h"

#include "../jrd/optimizer/Optimizer.h"

using namespace Jrd;
using namespace Firebird;

#ifdef OPT_DEBUG_RETRIEVAL
#define OPT_DEBUG
#endif

#ifdef OPT_DEBUG
#define OPTIMIZER_DEBUG_FILE "opt_debug.out"
#endif


namespace
{
	inline void SET_DEP_BIT(ULONG* array, const SLONG bit)
	{
		array[bit / BITS_PER_LONG] |= (1L << (bit % BITS_PER_LONG));
	}

	inline bool TEST_DEP_BIT(const ULONG* array, const ULONG bit)
	{
		return (array[bit / BITS_PER_LONG] & (1L << (bit % BITS_PER_LONG))) != 0;
	}

	const int CACHE_PAGES_PER_STREAM			= 15;

	// enumeration of sort datatypes

	static const UCHAR sort_dtypes[] =
	{
		0,							// dtype_unknown
		SKD_text,					// dtype_text
		SKD_cstring,				// dtype_cstring
		SKD_varying,				// dtype_varying
		0,
		0,
		0,							// dtype_packed
		0,							// dtype_byte
		SKD_short,					// dtype_short
		SKD_long,					// dtype_long
		SKD_quad,					// dtype_quad
		SKD_float,					// dtype_real
		SKD_double,					// dtype_double
		SKD_double,					// dtype_d_float
		SKD_sql_date,				// dtype_sql_date
		SKD_sql_time,				// dtype_sql_time
		SKD_timestamp,				// dtype_timestamp
		SKD_quad,					// dtype_blob
		0,							// dtype_array
		SKD_int64,					// dtype_int64
		SKD_text,					// dtype_dbkey - use text sort for backward compatibility
		SKD_bytes,					// dtype_boolean
		SKD_dec64,					// dtype_dec64
		SKD_dec128,					// dtype_dec128
		SKD_int128,					// dtype_int128
		SKD_sql_time_tz,			// dtype_sql_time_tz
		SKD_timestamp_tz			// dtype_timestamp_tz
	};

	struct SortField
	{
		SortField() : stream(INVALID_STREAM), id(0), desc(nullptr)
		{}

		SortField(StreamType _stream, ULONG _id, const dsc* _desc)
			: stream(_stream), id(_id), desc(_desc)
		{}

		StreamType stream;
		ULONG id;
		const dsc* desc;
	};

	class CrossJoin : public River
	{
	public:
		CrossJoin(CompilerScratch* csb, RiverList& rivers)
			: River(csb, nullptr, rivers)
		{
			// Save states of the underlying streams and restore them afterwards

			StreamStateHolder stateHolder(csb, m_streams);

			// Generate record source objects

			const FB_SIZE_T riverCount = rivers.getCount();

			if (riverCount == 1)
			{
				River* const sub_river = rivers.pop();
				m_rsb = sub_river->getRecordSource();
			}
			else
			{
				HalfStaticArray<RecordSource*, OPT_STATIC_ITEMS> rsbs(riverCount);

				// Reorder input rivers according to their possible inter-dependencies

				while (rivers.hasData())
				{
					const auto orgCount = rsbs.getCount();

					for (auto& subRiver : rivers)
					{
						const auto subRsb = subRiver->getRecordSource();
						fb_assert(!rsbs.exist(subRsb));

						subRiver->activate(csb);

						if (subRiver->isComputable(csb))
						{
							rsbs.add(subRsb);
							rivers.remove(&subRiver);
							break;
						}

						subRiver->deactivate(csb);
					}

					if (rsbs.getCount() == orgCount)
						break;
				}

				if (rivers.hasData())
				{
					// Ideally, we should never get here. But just in case it happened, handle it.

					for (auto& subRiver : rivers)
					{
						const auto subRsb = subRiver->getRecordSource();
						fb_assert(!rsbs.exist(subRsb));

						const auto pos = &subRiver - rivers.begin();
						rsbs.insert(pos, subRsb);
					}

					rivers.clear();
				}

				m_rsb = FB_NEW_POOL(csb->csb_pool) NestedLoopJoin(csb, rsbs.getCount(), rsbs.begin());
			}
		}
	};

	inline void compose(MemoryPool& pool, BoolExprNode** node1, BoolExprNode* node2)
	{
		if (node2)
			*node1 = (*node1) ? FB_NEW_POOL(pool) BinaryBoolNode(pool, blr_and, *node1, node2) : node2;
	}

	void classMask(unsigned count, ValueExprNode** eq_class, ULONG* mask)
	{
		// Given an sort/merge join equivalence class (vector of node pointers
		// of representative values for rivers), return a bit mask of rivers with values

		if (count > MAX_CONJUNCTS)
		{
			ERR_post(Arg::Gds(isc_optimizer_blk_exc));
			// Msg442: size of optimizer block exceeded
		}

		for (unsigned i = 0; i < OPT_STREAM_BITS; i++)
			mask[i] = 0;

		for (unsigned i = 0; i < count; i++, eq_class++)
		{
			if (*eq_class)
			{
				SET_DEP_BIT(mask, i);
				DEV_BLKCHK(*eq_class, type_nod);
			}
		}
	}


	unsigned getRiverCount(unsigned count, const ValueExprNode* const* eq_class)
	{
		// Given an sort/merge join equivalence class (vector of node pointers
		// of representative values for rivers), return the count of rivers with values

		unsigned cnt = 0;

		for (unsigned i = 0; i < count; i++)
		{
			if (*eq_class++)
				cnt++;
		}

		return cnt;
	}

	bool fieldEqual(const ValueExprNode* node1, const ValueExprNode* node2)
	{
		if (!node1 || !node2)
			return false;

		if (node1->getType() != node2->getType())
			return false;

		if (node1 == node2)
			return true;

		const FieldNode* fieldNode1 = nodeAs<FieldNode>(node1);
		const FieldNode* fieldNode2 = nodeAs<FieldNode>(node2);

		if (fieldNode1 && fieldNode2)
		{
			return fieldNode1->fieldStream == fieldNode2->fieldStream &&
				fieldNode1->fieldId == fieldNode2->fieldId;
		}

		return false;
	}

	bool fieldEqual(const BoolExprNode* node1, const BoolExprNode* node2)
	{
		if (!node1 || !node2)
			return false;

		if (node1->getType() != node2->getType())
			return false;

		if (node1 == node2)
			return true;

		const auto cmpNode = nodeAs<ComparativeBoolNode>(node1);
		const auto cmpNode2 = nodeAs<ComparativeBoolNode>(node2);

		if (cmpNode && cmpNode2 && cmpNode->blrOp == cmpNode2->blrOp &&
			(cmpNode->blrOp == blr_eql || cmpNode->blrOp == blr_equiv))
		{
			if (fieldEqual(cmpNode->arg1, cmpNode2->arg1) &&
				fieldEqual(cmpNode->arg2, cmpNode2->arg2))
			{
				return true;
			}

			if (fieldEqual(cmpNode->arg1, cmpNode2->arg2) &&
				fieldEqual(cmpNode->arg2, cmpNode2->arg1))
			{
				return true;
			}
		}

		return false;
	}


	bool augmentStack(ValueExprNode* node, ValueExprNodeStack& stack)
	{
		for (ValueExprNodeStack::const_iterator temp(stack); temp.hasData(); ++temp)
		{
			if (fieldEqual(node, temp.object()))
				return false;
		}

		stack.push(node);
		return true;
	}

	bool augmentStack(BoolExprNode* node, BoolExprNodeStack& stack)
	{
		for (BoolExprNodeStack::const_iterator temp(stack); temp.hasData(); ++temp)
		{
			if (fieldEqual(node, temp.object()))
				return false;
		}

		stack.push(node);
		return true;
	}

	bool searchStack(const ValueExprNode* node, const ValueExprNodeStack& stack)
	{
		for (ValueExprNodeStack::const_iterator iter(stack); iter.hasData(); ++iter)
		{
			if (fieldEqual(node, iter.object()))
				return true;
		}

		return false;
	}

	double getCardinality(thread_db* tdbb, jrd_rel* relation, const Format* format)
	{
		// Return the estimated cardinality for the given relation

		if (relation->isVirtual())
			return 100.0; // Just a dumb estimation

		if (relation->rel_file)
			return EXT_cardinality(tdbb, relation);

		MET_post_existence(tdbb, relation);
		const double cardinality = DPM_cardinality(tdbb, relation, format);
		MET_release_existence(tdbb, relation);

		return cardinality;
	}

	void markIndices(CompilerScratch::csb_repeat* tail, USHORT relationId)
	{
		// Mark indices that were not included in the user-specified access plan

		const auto plan = tail->csb_plan;
		fb_assert(plan);

		if (plan->type != PlanNode::TYPE_RETRIEVE)
			return;

		// Go through each of the indices and mark it unusable
		// for indexed retrieval unless it was specifically mentioned
		// in the plan; also mark indices for navigational access.

		// If there were none indices, this is a sequential retrieval.

		const auto relation = tail->csb_relation;
		fb_assert(relation);

		if (!tail->csb_idx)
			return;

		for (auto& idx : *tail->csb_idx)
		{
			if (!plan->accessType)
			{
				idx.idx_runtime_flags |= idx_plan_dont_use;
				continue;
			}

			bool first = true, found = false;
			for (const auto& arg : plan->accessType->items)
			{
				if (relationId != arg.relationId)
				{
					// index %s cannot be used in the specified plan
					ERR_post(Arg::Gds(isc_index_unused) << arg.indexName);
				}

				if (idx.idx_id == arg.indexId)
				{
					if (plan->accessType->type == PlanNode::AccessType::TYPE_NAVIGATIONAL && first)
					{
						// dimitr:	navigational access can use only one index,
						//			hence the extra check added (see the line above)
						idx.idx_runtime_flags |= idx_plan_navigate;
					}
					else
					{
						// nod_indices
						found = true;
						break;
					}
				}

				first = false;
			}

			if (!found)
				idx.idx_runtime_flags |= idx_plan_dont_use;
		}
	}

	bool mapEqual(const ValueExprNode* field1, const ValueExprNode* field2, const MapNode* map)
	{
		// Test to see if two fields are equal, where the fields are in two different streams
		// possibly mapped to each other. Order of the input fields is important.
		const auto fieldNode1 = nodeAs<FieldNode>(field1);
		const auto fieldNode2 = nodeAs<FieldNode>(field2);

		if (!fieldNode1 || !fieldNode2)
			return false;

		// look through the mapping and see if we can find an equivalence.
		auto sourcePtr = map->sourceList.begin();
		auto targetPtr = map->targetList.begin();

		for (const auto sourceEnd = map->sourceList.end();
			 sourcePtr != sourceEnd;
			 ++sourcePtr, ++targetPtr)
		{
			const auto mapFrom = nodeAs<FieldNode>(*sourcePtr);
			const auto mapTo = nodeAs<FieldNode>(*targetPtr);

			if (!mapFrom || !mapTo)
				continue;

			if (fieldNode1->fieldStream != mapFrom->fieldStream ||
				fieldNode1->fieldId != mapFrom->fieldId)
			{
				continue;
			}

			if (fieldNode2->fieldStream != mapTo->fieldStream ||
				fieldNode2->fieldId != mapTo->fieldId)
			{
				continue;
			}

			return true;
		}

		return false;
	}

	void setDirection(SortNode* fromClause, SortNode* toClause)
	{
		// Update the direction of a GROUP BY, DISTINCT, or ORDER BY
		// clause to the same direction as another clause. Do the same
		// for the nulls placement flag.

		const auto fromCount = fromClause->expressions.getCount();

		fb_assert(fromCount <= toClause->expressions.getCount());
		fb_assert(fromCount == fromClause->direction.getCount() &&
			fromCount == fromClause->nullOrder.getCount());
		fb_assert(toClause->expressions.getCount() == toClause->direction.getCount() &&
			toClause->expressions.getCount() == toClause->nullOrder.getCount());

		for (FB_SIZE_T i = 0; i < fromCount; ++i)
		{
			toClause->direction[i] = fromClause->direction[i];
			toClause->nullOrder[i] = fromClause->nullOrder[i];
		}
	}

	void setPosition(const SortNode* from_clause, SortNode* to_clause, const MapNode* map)
	{
		// Update the fields in a GROUP BY, DISTINCT, or ORDER BY clause to the same position
		// as another clause, possibly using a mapping between the streams.

		// Track the position in the from list with "to_swap", and find the corresponding
		// field in the from list with "to_ptr", then swap the two fields.  By the time
		// we get to the end of the from list, all fields in the to list will be reordered.

		auto to_swap = to_clause->expressions.begin();

		// We need to process no more than the number of nodes in the "from" clause

		const auto count = from_clause->expressions.getCount();
		fb_assert(count <= to_clause->expressions.getCount());

		auto from_ptr = from_clause->expressions.begin();
		for (const auto from_end = from_ptr + count; from_ptr != from_end; ++from_ptr)
		{
			NestConst<ValueExprNode>* to_ptr = to_clause->expressions.begin();
			for (const auto to_end = to_ptr + count; to_ptr != to_end; ++to_ptr)
			{
				const auto fromField = nodeAs<FieldNode>(*from_ptr);
				const auto toField = nodeAs<FieldNode>(*to_ptr);

				if ((map && mapEqual(*to_ptr, *from_ptr, map)) ||
					(!map && fromField && toField &&
						fromField->fieldStream == toField->fieldStream &&
						fromField->fieldId == toField->fieldId))
				{
					ValueExprNode* swap = *to_swap;
					*to_swap = *to_ptr;
					*to_ptr = swap;
				}
			}

			++to_swap;
		}
	}

} // namespace


//
// Constructor
//

Optimizer::Optimizer(thread_db* aTdbb, CompilerScratch* aCsb, RseNode* aRse)
	: PermanentStorage(*aTdbb->getDefaultPool()),
	  tdbb(aTdbb), csb(aCsb), rse(aRse),
	  compileStreams(getPool()),
	  bedStreams(getPool()),
	  keyStreams(getPool()),
	  subStreams(getPool()),
	  outerStreams(getPool()),
	  conjuncts(getPool())
{
}


//
// Destructor
//

Optimizer::~Optimizer()
{
	// Release memory allocated for index descriptions
	for (const auto compileStream : compileStreams)
	{
		delete csb->csb_rpt[compileStream].csb_idx;
		csb->csb_rpt[compileStream].csb_idx = nullptr;
	}

	if (debugFile)
		fclose(debugFile);
}


//
// Compile and optimize a record selection expression into a set of record source blocks
//

RecordSource* Optimizer::compile(BoolExprNodeStack* parentStack)
{
	// If there is a boolean, there is some work to be done.  First,
	// decompose the boolean into conjunctions.  Then get descriptions
	// of all indices for all relations in the RseNode.  This will give
	// us the info necessary to allocate a optimizer block big
	// enough to hold this crud.

	RecordSource* rsb = nullptr;

	checkSorts();
	SortNode* sort = rse->rse_sorted;
	SortNode* project = rse->rse_projection;
	SortNode* aggregate = rse->rse_aggregate;

	BoolExprNodeStack conjunct_stack;
	unsigned conjunct_count = 0;

	// put any additional booleans on the conjunct stack, and see if we
	// can generate additional booleans by associativity--this will help
	// to utilize indices that we might not have noticed
	if (rse->rse_boolean)
		conjunct_count = decompose(rse->rse_boolean, conjunct_stack);

	conjunct_count += distributeEqualities(conjunct_stack, conjunct_count);

	// AB: If we have limit our retrieval with FIRST / SKIP syntax then
	// we may not deliver above conditions (from higher rse's) to this
	// rse, because the results should be consistent.
	if (rse->rse_skip || rse->rse_first)
		parentStack = nullptr;

	// Set base-point before the parent/distributed nodes begin.
	const unsigned base_count = conjunct_count;
	baseConjuncts = base_count;

	// AB: Add parent conjunctions to conjunct_stack, keep in mind
	// the outer-streams! For outer streams put missing (IS NULL)
	// conjunctions in the missing_stack.
	//
	// opt_rpt[0..opt_base_conjuncts-1] = defined conjunctions to this stream
	// opt_rpt[0..opt_base_parent_conjuncts-1] = defined conjunctions to this
	//   stream and allowed distributed conjunctions (with parent)
	// opt_rpt[0..opt_base_missing_conjuncts-1] = defined conjunctions to this
	//   stream and allowed distributed conjunctions and allowed parent
	// opt_rpt[0..opt_conjuncts_count-1] = all conjunctions
	//
	// allowed = booleans that can never evaluate to NULL/Unknown or turn
	//   NULL/Unknown into a True or False.

	unsigned parent_count = 0, distributed_count = 0;
	BoolExprNodeStack missing_stack;

	if (parentStack)
	{
		for (BoolExprNodeStack::iterator iter(*parentStack);
			 iter.hasData() && conjunct_count < MAX_CONJUNCTS; ++iter)
		{
			BoolExprNode* const node = iter.object();

			if (!isInnerJoin() && node->possiblyUnknown())
			{
				// parent missing conjunctions shouldn't be
				// distributed to FULL OUTER JOIN streams at all
				if (!isFullJoin())
					missing_stack.push(node);
			}
			else
			{
				conjunct_stack.push(node);
				conjunct_count++;
				parent_count++;
			}
		}

		// We've now merged parent, try again to make more conjunctions.
		distributed_count = distributeEqualities(conjunct_stack, conjunct_count);
		conjunct_count += distributed_count;
	}

	// The newly created conjunctions belong to the base conjunctions.
	// After them are starting the parent conjunctions.
	baseParentConjuncts = baseConjuncts + distributed_count;

	// Set base-point before the parent IS NULL nodes begin
	baseMissingConjuncts = conjunct_count;

	// Check if size of optimizer block exceeded.
	if (conjunct_count > MAX_CONJUNCTS)
	{
		ERR_post(Arg::Gds(isc_optimizer_blk_exc));
		// Msg442: size of optimizer block exceeded
	}

	// Put conjunctions in opt structure.
	// Note that it's a stack and we get the nodes in reversed order from the stack.

	conjuncts.grow(conjunct_count);
	int nodeBase = -1, j = -1;

	for (unsigned i = conjunct_count; i > 0; i--, j--)
	{
		BoolExprNode* const node = conjunct_stack.pop();

		if (i == base_count)
		{
			// The base conjunctions
			j = base_count - 1;
			nodeBase = 0;
		}
		else if (i == conjunct_count - distributed_count)
		{
			// The parent conjunctions
			j = parent_count - 1;
			nodeBase = baseParentConjuncts;
		}
		else if (i == conjunct_count)
		{
			// The new conjunctions created by "distribution" from the stack
			j = distributed_count - 1;
			nodeBase = baseConjuncts;
		}

		fb_assert(nodeBase >= 0 && j >= 0);
		conjuncts[nodeBase + j].node = node;
	}

	// Put the parent missing nodes on the stack
	for (BoolExprNodeStack::iterator iter(missing_stack);
		 iter.hasData() && conjunct_count < MAX_CONJUNCTS; ++iter)
	{
		BoolExprNode* const node = iter.object();

		conjuncts.grow(conjunct_count + 1);
		conjuncts[conjunct_count].node = node;
		conjunct_count++;
	}

	// Clear the csb_active flag of all streams in the RseNode
	StreamList rseStreams;
	rse->computeRseStreams(rseStreams);

	for (StreamList::iterator i = rseStreams.begin(); i != rseStreams.end(); ++i)
		csb->csb_rpt[*i].deactivate();

	// Go through the record selection expression generating
	// record source blocks for all streams

	RiverList rivers;

	bool innerSubStream = false;
	for (auto node : rse->rse_relations)
	{
		fb_assert(sort == rse->rse_sorted);
		fb_assert(aggregate == rse->rse_aggregate);

		// Find the stream number and place it at the end of the bedStreams array
		// (if this is really a stream and not another RseNode)

		node->computeRseStreams(bedStreams);
		node->computeDbKeyStreams(keyStreams);

		// Compile the node
		rsb = node->compile(tdbb, this, innerSubStream);

		// If an rsb has been generated, we have a non-relation;
		// so it forms a river of its own since it is separately
		// optimized from the streams in this rsb

		if (rsb)
		{
			StreamList localStreams;
			rsb->findUsedStreams(localStreams);

			// AB: Save all outer-part streams
			if (isInnerJoin() || (isLeftJoin() && !innerSubStream))
			{
				subStreams.join(localStreams);
				outerStreams.join(localStreams);
			}

			const auto river = FB_NEW_POOL(getPool()) River(csb, rsb, node, localStreams);
			river->deactivate(csb);
			rivers.add(river);
		}
		else
		{
			// We have a relation, just add its stream
			fb_assert(bedStreams.hasData());
			outerStreams.add(bedStreams.back());
		}

		innerSubStream = true;
	}

	// This is an attempt to make sure we have a large enough cache to
	// efficiently retrieve this query; make sure the cache has a minimum
	// number of pages for each stream in the RseNode (the number is just a guess)
	if (compileStreams.getCount() > 5)
		CCH_expand(tdbb, (ULONG) (compileStreams.getCount() * CACHE_PAGES_PER_STREAM));

	// At this point we are ready to start optimizing.
	// We will use the opt block to hold information of
	// a global nature, meaning that it needs to stick
	// around for the rest of the optimization process.

	// Attempt to optimize aggregates via an index, if possible
	if (aggregate && !sort)
		sort = aggregate;
	else
		rse->rse_aggregate = aggregate = nullptr;

	// AB: Mark the previous used streams (sub-RseNode's) as active
	for (const auto subStream : subStreams)
		csb->csb_rpt[subStream].activate();

	bool sortCanBeUsed = true;
	SortNode* const orgSortNode = sort;

	// When DISTINCT and ORDER BY are done on different fields,
	// and ORDER BY can be mapped to an index, then the records
	// are returned in the wrong order because DISTINCT sort is
	// performed after the navigational walk of the index.
	// For that reason, we need to de-optimize this case so that
	// ORDER BY does not use an index.
	if (sort && project)
	{
		sort = nullptr;
		sortCanBeUsed = false;
	}

	// Outer joins are processed their own way
	if (!isInnerJoin())
		rsb = generateOuterJoin(rivers, &sort);
	else
	{
		// AB: If previous rsb's are already on the stack we can't use
		// a navigational-retrieval for an ORDER BY because the next
		// streams are JOINed to the previous ones
		if (rivers.hasData())
		{
			sort = nullptr;
			sortCanBeUsed = false;

			// AB: We could already have multiple rivers at this
			// point so try to do some hashing or sort/merging now.
			while (generateEquiJoin(rivers))
				;
		}

		StreamList joinStreams(compileStreams);

		fb_assert(joinStreams.getCount() != 1 || csb->csb_rpt[joinStreams[0]].csb_relation);

		while (true)
		{
			// AB: Determine which streams have an index relationship
			// with the currently active rivers. This is needed so that
			// no merge is made between a new cross river and the
			// currently active rivers. Where in the new cross river
			// a stream depends (index) on the active rivers.
			StreamList dependent_streams, free_streams;
			findDependentStreams(joinStreams, dependent_streams, free_streams);

			// If we have dependent and free streams then we can't rely on
			// the sort node to be used for index navigation
			if (dependent_streams.hasData() && free_streams.hasData())
			{
				sort = nullptr;
				sortCanBeUsed = false;
			}

			if (dependent_streams.hasData())
			{
				// Copy free streams
				joinStreams.assign(free_streams);

				// Make rivers from the dependent streams
				generateInnerJoin(dependent_streams, rivers, &sort, rse->rse_plan);

				// Generate one river which holds a cross join rsb between
				// all currently available rivers

				rivers.add(FB_NEW_POOL(getPool()) CrossJoin(csb, rivers));
				rivers.back()->activate(csb);
			}
			else
			{
				if (free_streams.hasData())
				{
					// Deactivate streams from rivers on stack, because
					// the remaining streams don't have any indexed relationship with them
					for (const auto river : rivers)
						river->deactivate(csb);
				}

				break;
			}
		}

		// attempt to form joins in decreasing order of desirability
		generateInnerJoin(joinStreams, rivers, &sort, rse->rse_plan);

		// If there are multiple rivers, try some hashing or sort/merging
		while (generateEquiJoin(rivers))
			;

		rsb = CrossJoin(csb, rivers).getRecordSource();

		// Pick up any residual boolean that may have fallen thru the cracks
		rsb = generateResidualBoolean(rsb);
	}

	// Assign the sort node back if it wasn't used by the index navigation
	if (orgSortNode && !sortCanBeUsed)
		sort = orgSortNode;

	// If the aggregate was not optimized via an index, get rid of the
	// sort and flag the fact to the calling routine
	if (aggregate && sort)
	{
		rse->rse_aggregate = nullptr;
		sort = nullptr;
	}

	// Check index usage in all the base streams to ensure
	// that any user-specified access plan is followed

	checkIndices();

	if (project || sort)
	{
		// Eliminate any duplicate dbkey streams
		for (const auto stream: bedStreams)
		{
			FB_SIZE_T pos;
			if (keyStreams.find(stream, pos))
				keyStreams.remove(pos);
		}

		// Handle project clause, if present
		if (project)
			rsb = generateSort(bedStreams, &keyStreams, rsb, project, favorFirstRows(), true);

		// Handle sort clause if present
		if (sort)
			rsb = generateSort(bedStreams, &keyStreams, rsb, sort, favorFirstRows(), false);
	}

    // Handle first and/or skip.  The skip MUST (if present)
    // appear in the rsb list AFTER the first.  Since the gen_first and gen_skip
    // functions add their nodes at the beginning of the rsb list we MUST call
    // gen_skip before gen_first.

    if (rse->rse_skip)
		rsb = FB_NEW_POOL(getPool()) SkipRowsStream(csb, rsb, rse->rse_skip);

	if (rse->rse_first)
		rsb = FB_NEW_POOL(getPool()) FirstRowsStream(csb, rsb, rse->rse_first);

	if (rse->flags & RseNode::FLAG_SINGULAR)
		rsb = FB_NEW_POOL(*tdbb->getDefaultPool()) SingularStream(csb, rsb);

	if (rse->flags & RseNode::FLAG_WRITELOCK)
	{
		for (const auto compileStream : compileStreams)
		{
			const auto tail = &csb->csb_rpt[compileStream];
			tail->csb_flags |= csb_update;

			fb_assert(tail->csb_relation);

			CMP_post_access(tdbb, csb, tail->csb_relation->rel_security_name,
				tail->csb_view ? tail->csb_view->rel_id : 0,
				SCL_update, obj_relations, tail->csb_relation->rel_name);
		}

		rsb = FB_NEW_POOL(*tdbb->getDefaultPool()) LockedStream(csb, rsb);
	}

	if (rse->flags & RseNode::FLAG_SCROLLABLE)
		rsb = FB_NEW_POOL(*tdbb->getDefaultPool()) BufferedStream(csb, rsb);

	return rsb;
}


//
// Prepare relation and its indices for optimization
//

void Optimizer::compileRelation(StreamType stream)
{
	// We have found a base relation; record its stream number in the streams array
	// as a candidate for merging into a river

	compileStreams.add(stream);

	// If we have any booleans or sort fields, we may be able to
	// use an index to optimize them; retrieve the current format of
	// all indices at this time so we can determine if it's possible

	const bool needIndices = conjuncts.hasData() || (rse->rse_sorted || rse->rse_aggregate);

	const auto tail = &csb->csb_rpt[stream];

	const auto relation = tail->csb_relation;
	fb_assert(relation);

	tail->csb_idx = nullptr;

	if (needIndices && !relation->rel_file && !relation->isVirtual())
	{
		const auto relPages = relation->getPages(tdbb);
		IndexDescList idxList;
		BTR_all(tdbb, relation, idxList, relPages);

		if (idxList.hasData())
			tail->csb_idx = FB_NEW_POOL(getPool()) IndexDescList(getPool(), idxList);

		if (tail->csb_plan)
			markIndices(tail, relation->rel_id);
	}

	const auto format = CMP_format(tdbb, csb, stream);
	tail->csb_cardinality = getCardinality(tdbb, relation, format);
}


//
// Generate a separate AggregateSort (Aggregate SortedStream Block) for each distinct operation.
// Note that this should be optimized to use indices if possible.
//

void Optimizer::generateAggregateDistincts(MapNode* map)
{
	dsc descriptor;
	dsc* desc = &descriptor;

	for (auto from : map->sourceList)
	{
		auto aggNode = nodeAs<AggNode>(from);

		if (aggNode && aggNode->distinct)
		{
			// Build the sort key definition. Turn cstrings into varying text.
			aggNode->arg->getDesc(tdbb, csb, desc);

			if (desc->dsc_dtype == dtype_cstring)
			{
				desc->dsc_dtype = dtype_varying;
				desc->dsc_length++;
			}

			const auto asb = FB_NEW_POOL(getPool()) AggregateSort(getPool());

			asb->intl = desc->isText() && desc->getTextType() != ttype_none &&
				desc->getTextType() != ttype_binary && desc->getTextType() != ttype_ascii;

			sort_key_def* sort_key = asb->keyItems.getBuffer(asb->intl ? 2 : 1);
			sort_key->setSkdOffset();

			if (asb->intl)
			{
				const USHORT key_length = ROUNDUP(INTL_key_length(tdbb,
					INTL_TEXT_TO_INDEX(desc->getTextType()), desc->getStringLength()), sizeof(SINT64));

				sort_key->setSkdLength(SKD_bytes, key_length);
				sort_key->skd_flags = SKD_ascending;
				sort_key->skd_vary_offset = 0;

				++sort_key;
				sort_key->setSkdOffset(&sort_key[-1]);
				asb->length = sort_key->getSkdOffset();
			}

			fb_assert(desc->dsc_dtype < FB_NELEM(sort_dtypes));
			sort_key->setSkdLength(sort_dtypes[desc->dsc_dtype], desc->dsc_length);

			if (!sort_key->skd_dtype)
				ERR_post(Arg::Gds(isc_invalid_sort_datatype) << Arg::Str(DSC_dtype_tostring(desc->dsc_dtype)));

			if (desc->dsc_dtype == dtype_varying)
			{
				// allocate space to store varying length
				sort_key->skd_vary_offset = sort_key->getSkdOffset() + ROUNDUP(desc->dsc_length, sizeof(SLONG));
				asb->length = sort_key->skd_vary_offset + sizeof(USHORT);
			}
			else
				asb->length += sort_key->getSkdLength();

			asb->length = ROUNDUP(asb->length, sizeof(SLONG));
			// dimitr:	allocate an extra longword for the purely artificial counter,
			// 			see AggNode::aggPass() for details; the length remains rounded properly
			asb->length += sizeof(ULONG);

			sort_key->skd_flags = SKD_ascending;
			asb->impure = csb->allocImpure<impure_agg_sort>();
			asb->desc = *desc;

			aggNode->asb = asb;
		}
	}
}


//
// Generate a record source block to handle either a sort or a project.
// The two case are virtual identical -- the only difference is that
// project eliminates duplicates.  However, since duplicates are
// recognized and handled by sort, the JRD processing is identical.
//

SortedStream* Optimizer::generateSort(const StreamList& streams,
									  const StreamList* dbkeyStreams,
									  RecordSource* rsb,
									  SortNode* sort,
									  bool refetchFlag,
									  bool projectFlag)
{
	/* We already know the number of keys, but we also need to compute the
	total number of fields, keys and non-keys, to be pumped thru sort.  Starting
	with the number of keys, count the other field referenced.  Since a field
	is often a key, check for overlap to keep the length of the sort record
	down. */

	/* Along with the record number, the transaction id of the
	 * record will also be stored in the sort file.  This will
	 * be used to detect update conflict in read committed
	 * transactions. */

	ULONG items = sort->expressions.getCount() +
		3 * streams.getCount() + 2 * (dbkeyStreams ? dbkeyStreams->getCount() : 0);
	const NestConst<ValueExprNode>* const end_node = sort->expressions.end();

	// Collect all fields involved into the sort

	HalfStaticArray<SortField, OPT_STATIC_ITEMS> fields;
	ULONG totalLength = 0;

	for (const auto stream : streams)
	{
		UInt32Bitmap::Accessor accessor(csb->csb_rpt[stream].csb_fields);

		if (accessor.getFirst())
		{
			do
			{
				const auto id = accessor.current();

				const auto format = CMP_format(tdbb, csb, stream);
				const auto desc = &format->fmt_desc[id];

				if (id >= format->fmt_count || desc->isUnknown())
					IBERROR(157);		// msg 157 cannot sort on a field that does not exist

				fields.push(SortField(stream, id, desc));
				totalLength += desc->dsc_length;

				// If the field has already been mentioned as a sort key, don't bother to repeat it.
				// Unless this key is computed/volatile and thus cannot be restored after sorting.

				for (const auto expr : sort->expressions)
				{
					const auto fieldNode = nodeAs<FieldNode>(expr);

					if (fieldNode && fieldNode->fieldStream == stream && fieldNode->fieldId == id)
					{
						if (!SortedStream::hasVolatileKey(desc))
						{
							totalLength -= desc->dsc_length;
							fields.pop();
						}

						break;
					}
				}

			} while (accessor.getNext());
		}
	}

	auto fieldCount = fields.getCount();

	// Unless refetching is requested explicitly (e.g. FIRST ROWS optimization mode),
	// validate the sort record length against the configured threshold for inline storage

	if (!refetchFlag)
	{
		const auto dbb = tdbb->getDatabase();
		const auto threshold = dbb->dbb_config->getInlineSortThreshold();

		refetchFlag = (totalLength > threshold);
	}

	// Check for persistent fields to be excluded from the sort.
	// If nothing is excluded, there's no point in the refetch mode.

	if (refetchFlag)
	{
		for (auto& item : fields)
		{
			const auto relation = csb->csb_rpt[item.stream].csb_relation;

			if (relation &&
				!relation->rel_file &&
				!relation->rel_view_rse &&
				!relation->isVirtual())
			{
				item.desc = nullptr;
				--fieldCount;
			}
		}

		refetchFlag = (fieldCount != fields.getCount());
	}

	items += fieldCount;

	// Now that we know the number of items, allocate a sort map block.
	const auto map = FB_NEW_POOL(getPool()) SortedStream::SortMap(getPool());

	if (projectFlag)
		map->flags |= SortedStream::FLAG_PROJECT;

	if (refetchFlag)
		map->flags |= SortedStream::FLAG_REFETCH;

	if (sort->unique)
		map->flags |= SortedStream::FLAG_UNIQUE;

    sort_key_def* prev_key = nullptr;

	// Loop thru sort keys building sort keys.  Actually, to handle null values
	// correctly, two sort keys are made for each field, one for the null flag
	// and one for field itself.

	dsc descriptor;

	SortedStream::SortMap::Item* map_item = map->items.getBuffer(items);
	sort_key_def* sort_key = map->keyItems.getBuffer(2 * sort->expressions.getCount());
	const SortDirection* direction = sort->direction.begin();
	const NullsPlacement* nullOrder = sort->nullOrder.begin();

	for (NestConst<ValueExprNode>* node_ptr = sort->expressions.begin();
		 node_ptr != end_node;
		 ++node_ptr, ++nullOrder, ++direction, ++map_item)
	{
		// Pick up sort key expression.

		NestConst<ValueExprNode> node = *node_ptr;
		dsc* desc = &descriptor;
		node->getDesc(tdbb, csb, desc);

		// Allow for "key" forms of International text to grow
		if (IS_INTL_DATA(desc))
		{
			// Turn varying text and cstrings into text.

			if (desc->dsc_dtype == dtype_varying)
			{
				desc->dsc_dtype = dtype_text;
				desc->dsc_length -= sizeof(USHORT);
			}
			else if (desc->dsc_dtype == dtype_cstring)
			{
				desc->dsc_dtype = dtype_text;
				desc->dsc_length--;
			}

			desc->dsc_length = INTL_key_length(tdbb, INTL_INDEX_TYPE(desc), desc->dsc_length);
		}

		// Make key for null flag
		sort_key->setSkdLength(SKD_text, 1);
		sort_key->setSkdOffset(prev_key);

		// Handle nulls placement
		sort_key->skd_flags = SKD_ascending;

		// Have SQL-compliant nulls ordering for ODS11+
		if ((*nullOrder == NULLS_DEFAULT && *direction != ORDER_DESC) || *nullOrder == NULLS_FIRST)
			sort_key->skd_flags |= SKD_descending;

		prev_key = sort_key++;

		// Make key for sort key proper
		fb_assert(desc->dsc_dtype < FB_NELEM(sort_dtypes));
		sort_key->setSkdLength(sort_dtypes[desc->dsc_dtype], desc->dsc_length);
		sort_key->setSkdOffset(&sort_key[-1], desc);
		sort_key->skd_flags = SKD_ascending;
		if (*direction == ORDER_DESC)
			sort_key->skd_flags |= SKD_descending;

		if (!sort_key->skd_dtype)
			ERR_post(Arg::Gds(isc_invalid_sort_datatype) << Arg::Str(DSC_dtype_tostring(desc->dsc_dtype)));

		if (sort_key->skd_dtype == SKD_varying || sort_key->skd_dtype == SKD_cstring)
		{
			if (desc->dsc_ttype() == ttype_binary)
				sort_key->skd_flags |= SKD_binary;
		}

		if (SortedStream::hasVolatileKey(desc) && !refetchFlag)
			sort_key->skd_flags |= SKD_separate_data;

		map_item->clear();
		map_item->node = node;
		map_item->flagOffset = prev_key->getSkdOffset();
		map_item->desc = *desc;
		map_item->desc.dsc_address = (UCHAR*)(IPTR) sort_key->getSkdOffset();

		prev_key = sort_key++;

		FieldNode* fieldNode;

		if ( (fieldNode = nodeAs<FieldNode>(node)) )
		{
			map_item->stream = fieldNode->fieldStream;
			map_item->fieldId = fieldNode->fieldId;
		}
	}

	fb_assert(prev_key);
	ULONG map_length = prev_key ? ROUNDUP(prev_key->getSkdOffset() + prev_key->getSkdLength(), sizeof(SLONG)) : 0;
	map->keyLength = map_length;
	ULONG flag_offset = map_length;
	map_length += fieldCount;

	// Now go back and process all to fields involved with the sort

	for (const auto& item : fields)
	{
		if (!item.desc)
			continue;

		if (item.desc->dsc_dtype >= dtype_aligned)
			map_length = FB_ALIGN(map_length, type_alignments[item.desc->dsc_dtype]);

		map_item->clear();
		map_item->fieldId = (SSHORT) item.id;
		map_item->stream = item.stream;
		map_item->flagOffset = flag_offset++;
		map_item->desc = *item.desc;
		map_item->desc.dsc_address = (UCHAR*)(IPTR) map_length;
		map_length += item.desc->dsc_length;
		map_item++;
	}

	// Make fields for record numbers and transaction ids for all streams

	map_length = ROUNDUP(map_length, sizeof(SINT64));
	for (const auto stream : streams)
	{
		map_item->clear();
		map_item->fieldId = SortedStream::ID_DBKEY;
		map_item->stream = stream;
		dsc* desc = &map_item->desc;
		desc->dsc_dtype = dtype_int64;
		desc->dsc_length = sizeof(SINT64);
		desc->dsc_address = (UCHAR*)(IPTR) map_length;
		map_length += desc->dsc_length;
		map_item++;

		map_item->clear();
		map_item->fieldId = SortedStream::ID_TRANS;
		map_item->stream = stream;
		desc = &map_item->desc;
		desc->dsc_dtype = dtype_int64;
		desc->dsc_length = sizeof(SINT64);
		desc->dsc_address = (UCHAR*)(IPTR) map_length;
		map_length += desc->dsc_length;
		map_item++;
	}

	if (dbkeyStreams && dbkeyStreams->hasData())
	{
		map_length = ROUNDUP(map_length, sizeof(SINT64));

		for (const auto stream : *dbkeyStreams)
		{
			map_item->clear();
			map_item->fieldId = SortedStream::ID_DBKEY;
			map_item->stream = stream;
			dsc* desc = &map_item->desc;
			desc->dsc_dtype = dtype_int64;
			desc->dsc_length = sizeof(SINT64);
			desc->dsc_address = (UCHAR*)(IPTR) map_length;
			map_length += desc->dsc_length;
			map_item++;
		}

		for (const auto stream : *dbkeyStreams)
		{
			map_item->clear();
			map_item->fieldId = SortedStream::ID_DBKEY_VALID;
			map_item->stream = stream;
			dsc* desc = &map_item->desc;
			desc->dsc_dtype = dtype_text;
			desc->dsc_ttype() = CS_BINARY;
			desc->dsc_length = 1;
			desc->dsc_address = (UCHAR*)(IPTR) map_length;
			map_length += desc->dsc_length;
			map_item++;
		}
	}

	for (const auto stream : streams)
	{
		map_item->clear();
		map_item->fieldId = SortedStream::ID_DBKEY_VALID;
		map_item->stream = stream;
		dsc* desc = &map_item->desc;
		desc->dsc_dtype = dtype_text;
		desc->dsc_ttype() = CS_BINARY;
		desc->dsc_length = 1;
		desc->dsc_address = (UCHAR*)(IPTR) map_length;
		map_length += desc->dsc_length;
		map_item++;
	}

	fb_assert(map_item == map->items.end());
	fb_assert(sort_key == map->keyItems.end());

	map_length = ROUNDUP(map_length, sizeof(SLONG));

	// Make fields to store varying and cstring length

	for (auto& sortKey : map->keyItems)
	{
		fb_assert(sortKey.skd_dtype != 0);

		if (sortKey.skd_dtype == SKD_varying || sortKey.skd_dtype == SKD_cstring)
		{
			sortKey.skd_vary_offset = map_length;
			map_length += sizeof(USHORT);
			map->flags |= SortedStream::FLAG_KEY_VARY;
		}
	}

	if (map_length > MAX_SORT_RECORD)
	{
		ERR_post(Arg::Gds(isc_sort_rec_size_err) << Arg::Num(map_length));
		// Msg438: sort record size of %ld bytes is too big
	}

	map->length = map_length;

	// That was most unpleasant.  Never the less, it's done (except for the debugging).
	// All that remains is to build the record source block for the sort.
	return FB_NEW_POOL(getPool()) SortedStream(csb, rsb, map);
}


//
// Find conjuncts local to the given river and compose an appropriate filter
//

RecordSource* Optimizer::applyLocalBoolean(const River* river)
{
	StreamStateHolder stateHolder(csb);
	stateHolder.deactivate();

	river->activate(csb);

	BoolExprNode* boolean = nullptr;

	for (auto iter = getBaseConjuncts(); iter.hasData(); ++iter)
	{
		if (!(iter & CONJUNCT_USED) &&
			!(iter->nodFlags & ExprNode::FLAG_RESIDUAL) &&
			iter->computable(csb, INVALID_STREAM, false))
		{
			compose(getPool(), &boolean, iter);
			iter |= CONJUNCT_USED;
		}
	}

	const auto rsb = river->getRecordSource();
	return boolean ? FB_NEW_POOL(getPool()) FilteredStream(csb, rsb, boolean) : rsb;
}


//
// Check to make sure that the user-specified indices were actually utilized by the optimizer
//

void Optimizer::checkIndices()
{
	for (const auto compileStream : compileStreams)
	{
		const auto tail = &csb->csb_rpt[compileStream];

		const auto plan = tail->csb_plan;
		if (!plan)
			continue;

		if (plan->type != PlanNode::TYPE_RETRIEVE)
			continue;

		const auto relation = tail->csb_relation;

		// If there were no indices fetched at all but the user specified some,
		// error out using the first index specified

		if (!tail->csb_idx && plan->accessType && !tdbb->getAttachment()->isGbak())
		{
			// index %s cannot be used in the specified plan
			ERR_post(Arg::Gds(isc_index_unused) << plan->accessType->items[0].indexName);
		}

		if (!tail->csb_idx)
			return;

		// Check to make sure that all indices are either used or marked not to be used,
		// and that there are no unused navigational indices
		MetaName index_name;

		for (const auto& idx : *tail->csb_idx)
		{
			if (!(idx.idx_runtime_flags & (idx_plan_dont_use | idx_used)) ||
				((idx.idx_runtime_flags & idx_plan_navigate) && !(idx.idx_runtime_flags & idx_navigate)))
			{
				if (relation)
					MET_lookup_index(tdbb, index_name, relation->rel_name, (USHORT) (idx.idx_id + 1));
				else
					index_name = "";

				// index %s cannot be used in the specified plan
				ERR_post(Arg::Gds(isc_index_unused) << Arg::Str(index_name));
			}
		}
	}
}


//
// Try to optimize out unnecessary sorting
//

void Optimizer::checkSorts()
{
	SortNode* sort = rse->rse_sorted;
	const auto sortCount = sort ? sort->expressions.getCount() : 0;

	SortNode* project = rse->rse_projection;
	const auto projectCount = project ? project->expressions.getCount() : 0;

	// Check if a GROUP BY exists using the same fields as the project or sort:
	// if so, the projection can be eliminated; if no projection exists, then
	// the sort can be eliminated

	RecordSourceNode* subRse;
	AggregateSourceNode* aggregate;
	SortNode* group;

	if ((project || sort) &&
		rse->rse_relations.getCount() == 1 &&
		(subRse = rse->rse_relations[0]) &&
		(aggregate = nodeAs<AggregateSourceNode>(subRse)) &&
		(group = aggregate->group))
	{
		const auto map = aggregate->map;
		const auto groupCount = group->expressions.getCount();

		// If all the fields of the project are the same as all the fields
		// of the group by, get rid of the project

		if (project && projectCount == groupCount)
		{
			bool equal = true;
			for (unsigned i = 0; i < groupCount; i++)
			{
				const auto groupNode = group->expressions[i];
				const auto projectNode = project->expressions[i];

				if (!mapEqual(groupNode, projectNode, map))
				{
					equal = false;
					break;
				}
			}

			// We can now ignore the project, but in case the project is being done
			// in descending order because of an order by, do the group by the same way.
			if (equal)
			{
				setDirection(project, group);
				project = rse->rse_projection = nullptr;
			}
		}

		// If there is no projection, then we can make a similar optimization
		// for sort, except that sort may have fewer fields than group by

		if (!project && sort && sortCount <= groupCount)
		{
			bool equal = true;
			for (unsigned i = 0; i < sortCount; i++)
			{
				const auto groupNode = group->expressions[i];
				const auto sortNode = sort->expressions[i];

				if (!mapEqual(groupNode, sortNode, map))
				{
					equal = false;
					break;
				}
			}

			// If all the fields in the sort list match the first n fields in the
			// project list, we can ignore the sort, but update the sort order
			// (ascending/descending) to match that in the sort list

			if (equal)
			{
				setDirection(sort, group);
				setPosition(sort, group, map);
				sort = rse->rse_sorted = nullptr;
			}
		}

	}

	// Examine the ORDER BY and DISTINCT clauses; if all the fields in the
	// ORDER BY match the first n fields in the DISTINCT in any order, the
	// ORDER BY can be removed, changing the fields in the DISTINCT to match
	// the ordering of fields in the ORDER BY

	if (sort && project && sortCount <= projectCount)
	{
		bool equal = true;
		for (unsigned i = 0; i < sortCount; i++)
		{
			const auto sortField = nodeAs<FieldNode>(sort->expressions[i]);
			const auto projectField = nodeAs<FieldNode>(project->expressions[i]);

			if (!sortField || !projectField ||
				sortField->fieldStream != projectField->fieldStream ||
				sortField->fieldId != projectField->fieldId)
			{
				equal = false;
				break;
			}
		}

		// If all the fields in the sort list match the first n fields
		// in the project list, we can ignore the sort, but update
		// the project to match the sort
		if (equal)
		{
			setDirection(sort, project);
			setPosition(sort, project, nullptr);
			sort = rse->rse_sorted = nullptr;
		}
	}

	// RP: optimize sort with OUTER JOIN
	// if all the fields in the sort list are from one stream, check the stream is
	// the most outer stream, if true update rse and ignore the sort
	if (sort && !project)
	{
		StreamType sortStream = 0;
		bool usableSort = true;

		for (unsigned i = 0; i < sortCount; i++)
		{
			const auto sortNode = sort->expressions[i];
			const auto sortField = nodeAs<FieldNode>(sortNode);

			if (sortField)
			{
				// Get stream for this field at this position.
				const StreamType currentStream = sortField->fieldStream;

				// If this is the first position node, save this stream
				if (i == 0)
					sortStream = currentStream;
				else if (currentStream != sortStream)
				{
					// If the current stream is different then the previous stream
					// then we can't use this sort for an indexed order retrieval
					usableSort = false;
					break;
				}
			}
			else
			{
				// If this is not the first position node, reject this sort.
				// Two expressions cannot be mapped to a single index.
				if (i > 0)
				{
					usableSort = false;
					break;
				}

				// This position doesn't use a simple field, thus we should
				// check the expression internals
				SortedStreamList streams;
				sortNode->collectStreams(streams);

				// We can use this sort only if there's a single stream
				// referenced by the expression
				if (streams.getCount() == 1)
					sortStream = streams[0];
				else
				{
					usableSort = false;
					break;
				}
			}
		}

		if (usableSort)
		{
			RecordSourceNode* node = rse;
			RseNode* newRse = nullptr;

			while (node)
			{
				if (nodeIs<RseNode>(node))
				{
					newRse = static_cast<RseNode*>(node);

					// AB: Don't distribute the sort when a FIRST/SKIP is supplied,
					// because that will affect the behaviour from the deeper RSE.
					// dimitr: the same rule applies to explicit/implicit user-defined sorts.
					if (newRse != rse &&
						(newRse->rse_first || newRse->rse_skip ||
						 newRse->rse_sorted || newRse->rse_projection))
					{
						node = nullptr;
						break;
					}

					// Walk trough the relations of the RSE and see if a
					// matching stream can be found.
					if (newRse->rse_jointype == blr_inner)
					{
						if (newRse->rse_relations.getCount() == 1)
							node = newRse->rse_relations[0];
						else
						{
							bool sortStreamFound = false;
							for (const auto subRse : newRse->rse_relations)
							{
								if ((nodeIs<RelationSourceNode>(subRse) || nodeIs<LocalTableSourceNode>(subRse)) &&
									subRse->getStream() == sortStream &&
									newRse != rse)
								{
									// We have found the correct stream
									sortStreamFound = true;
									break;
								}
							}

							if (sortStreamFound)
							{
								// Set the sort to the found stream and clear the original sort
								newRse->rse_sorted = sort;
								sort = rse->rse_sorted = nullptr;
							}

							node = nullptr;
						}
					}
					else if (newRse->rse_jointype == blr_left)
						node = newRse->rse_relations[0];
					else
						node = nullptr;
				}
				else
				{
					if ((nodeIs<RelationSourceNode>(node) || nodeIs<LocalTableSourceNode>(node)) &&
						node->getStream() == sortStream &&
						newRse && newRse != rse)
					{
						// We have found the correct stream, thus apply the sort here
						newRse->rse_sorted = sort;
						sort = rse->rse_sorted = nullptr;
					}

					node = nullptr;
				}
			}
		}
	}
}


//
// Decompose a boolean into a stack of conjuctions.
//

unsigned Optimizer::decompose(BoolExprNode* boolNode, BoolExprNodeStack& stack)
{
	if (const auto binaryNode = nodeAs<BinaryBoolNode>(boolNode))
	{
		if (binaryNode->blrOp == blr_and)
		{
			auto count = decompose(binaryNode->arg1, stack);
			count += decompose(binaryNode->arg2, stack);
			return count;
		}
		else if (binaryNode->blrOp == blr_or)
		{
			BoolExprNodeStack or_stack;

			if (decompose(binaryNode->arg1, or_stack) >= 2)
			{
				binaryNode->arg1 = or_stack.pop();

				while (or_stack.hasData())
				{
					const auto newBoolNode =
						FB_NEW_POOL(getPool()) BinaryBoolNode(getPool(), blr_and);
					newBoolNode->arg1 = or_stack.pop();
					newBoolNode->arg2 = binaryNode->arg1;

					binaryNode->arg1 = newBoolNode;
				}
			}

			or_stack.clear();

			if (decompose(binaryNode->arg2, or_stack) >= 2)
			{
				binaryNode->arg2 = or_stack.pop();

				while (or_stack.hasData())
				{
					const auto newBoolNode =
						FB_NEW_POOL(getPool()) BinaryBoolNode(getPool(), blr_and);
					newBoolNode->arg1 = or_stack.pop();
					newBoolNode->arg2 = binaryNode->arg2;

					binaryNode->arg2 = newBoolNode;
				}
			}
		}
	}
	else if (const auto cmpNode = nodeAs<ComparativeBoolNode>(boolNode))
	{
		// turn a between into (a greater than or equal) AND (a less than  or equal)

		if (cmpNode->blrOp == blr_between)
		{
			auto newCmpNode = FB_NEW_POOL(getPool()) ComparativeBoolNode(getPool(), blr_geq);
			newCmpNode->arg1 = cmpNode->arg1;
			newCmpNode->arg2 = cmpNode->arg2;

			stack.push(newCmpNode);

			newCmpNode = FB_NEW_POOL(getPool()) ComparativeBoolNode(getPool(), blr_leq);
			newCmpNode->arg1 = CMP_clone_node_opt(tdbb, csb, cmpNode->arg1);
			newCmpNode->arg2 = cmpNode->arg3;

			stack.push(newCmpNode);

			return 2;
		}

		// turn a LIKE/SIMILAR into a LIKE/SIMILAR and a STARTING WITH, if it starts
		// with anything other than a pattern-matching character

		ValueExprNode* arg;

		if ((cmpNode->blrOp == blr_like || cmpNode->blrOp == blr_similar) &&
			(arg = optimizeLikeSimilar(cmpNode)))
		{
			const auto newCmpNode =
				FB_NEW_POOL(getPool()) ComparativeBoolNode(getPool(), blr_starting);
			newCmpNode->arg1 = cmpNode->arg1;
			newCmpNode->arg2 = arg;

			stack.push(newCmpNode);
			stack.push(boolNode);

			return 2;
		}
	}

	stack.push(boolNode);

	return 1;
}


//
// Given a stack of conjunctions, generate some simple inferences.
// In general, find classes of equalities, then find operations based on members of those classes.
// If we find any, generate additional conjunctions. In short:
//
// if (a == b) and (a $ c) --> (b $ c) for any operation '$'.
//

unsigned Optimizer::distributeEqualities(BoolExprNodeStack& orgStack, unsigned baseCount)
{
	// dimitr:	Dumb protection against too many injected conjuncts (see CORE-5381).
	//			Don't produce more additional conjuncts than we originally had
	//			(i.e. this routine should never more than double the number of conjuncts).
	//			Ideally, we need two separate limits here:
	//				1) number of injected conjuncts (affects required impure size)
	//				2) number of input conjuncts (affects search time inside this routine)

	if (baseCount * 2 > MAX_CONJUNCTS)
		return 0;

	ObjectsArray<ValueExprNodeStack> classes;
	ObjectsArray<ValueExprNodeStack>::iterator eq_class;

	// Zip thru stack of booleans looking for field equalities

	for (BoolExprNodeStack::iterator iter(orgStack); iter.hasData(); ++iter)
	{
		const auto boolean = iter.object();

		if (boolean->nodFlags & ExprNode::FLAG_DEOPTIMIZE)
			continue;

		const auto cmpNode = nodeAs<ComparativeBoolNode>(boolean);

		if (!cmpNode || cmpNode->blrOp != blr_eql)
			continue;

		auto node1 = cmpNode->arg1;
		if (!nodeIs<FieldNode>(node1))
			continue;

		auto node2 = cmpNode->arg2;
		if (!nodeIs<FieldNode>(node2))
			continue;

		for (eq_class = classes.begin(); eq_class != classes.end(); ++eq_class)
		{
			if (searchStack(node1, *eq_class))
			{
				augmentStack(node2, *eq_class);
				break;
			}
			else if (searchStack(node2, *eq_class))
			{
				eq_class->push(node1);
				break;
			}
		}

		if (eq_class == classes.end())
		{
			ValueExprNodeStack& s = classes.add();
			s.push(node1);
			s.push(node2);
			eq_class = classes.back();
		}
	}

	if (classes.isEmpty())
		return 0;

	// Make another pass looking for any equality relationships that may have crept
	// in between classes (this could result from the sequence (A = B, C = D, B = C)

	for (eq_class = classes.begin(); eq_class != classes.end(); ++eq_class)
	{
		for (ValueExprNodeStack::const_iterator iter(*eq_class); iter.hasData(); ++iter)
		{
			for (ObjectsArray<ValueExprNodeStack>::iterator eq_class2(eq_class);
				 ++eq_class2 != classes.end();)
			{
				if (searchStack(iter.object(), *eq_class2))
				{
					while (eq_class2->hasData())
						augmentStack(eq_class2->pop(), *eq_class);
				}
			}
		}
	}

	unsigned count = 0;

	// Start by making a pass distributing field equalities

	for (eq_class = classes.begin(); eq_class != classes.end(); ++eq_class)
	{
		if (eq_class->hasMore(2))
		{
			for (ValueExprNodeStack::iterator outer(*eq_class); outer.hasData(); ++outer)
			{
				for (ValueExprNodeStack::iterator inner(outer); (++inner).hasData(); )
				{
					if (count < baseCount)
					{
						AutoPtr<ComparativeBoolNode> cmpNode(FB_NEW_POOL(getPool())
							ComparativeBoolNode(getPool(), blr_eql));
						cmpNode->arg1 = outer.object();
						cmpNode->arg2 = inner.object();

						if (augmentStack(cmpNode, orgStack))
						{
							count++;
							cmpNode.release();
						}
					}
				}
			}
		}
	}

	// Now make a second pass looking for non-field equalities

	for (BoolExprNodeStack::iterator iter(orgStack); iter.hasData(); ++iter)
	{
		const auto boolean = iter.object();
		const auto cmpNode = nodeAs<ComparativeBoolNode>(boolean);
		ValueExprNode* node1;
		ValueExprNode* node2;

		if (cmpNode &&
			(cmpNode->blrOp == blr_eql ||
			 cmpNode->blrOp == blr_gtr || cmpNode->blrOp == blr_geq ||
			 cmpNode->blrOp == blr_leq || cmpNode->blrOp == blr_lss ||
			 cmpNode->blrOp == blr_matching || cmpNode->blrOp == blr_containing ||
			 cmpNode->blrOp == blr_like || cmpNode->blrOp == blr_similar))
		{
			node1 = cmpNode->arg1;
			node2 = cmpNode->arg2;
		}
		else
			continue;

		bool reverse = false;

		if (!nodeIs<FieldNode>(node1))
		{
			ValueExprNode* swap_node = node1;
			node1 = node2;
			node2 = swap_node;
			reverse = true;
		}

		if (!nodeIs<FieldNode>(node1))
			continue;

		if (!nodeIs<LiteralNode>(node2) && !nodeIs<ParameterNode>(node2) && !nodeIs<VariableNode>(node2))
			continue;

		for (eq_class = classes.begin(); eq_class != classes.end(); ++eq_class)
		{
			if (searchStack(node1, *eq_class))
			{
				for (ValueExprNodeStack::iterator temp(*eq_class); temp.hasData(); ++temp)
				{
					if (!fieldEqual(node1, temp.object()) && count < baseCount)
					{
						ValueExprNode* arg1;
						ValueExprNode* arg2;

						if (reverse)
						{
							arg1 = cmpNode->arg1;
							arg2 = temp.object();
						}
						else
						{
							arg1 = temp.object();
							arg2 = cmpNode->arg2;
						}

						// From the conjuncts X(A,B) and A=C, infer the conjunct X(C,B)
						AutoPtr<BoolExprNode> newNode(makeInferenceNode(boolean, arg1, arg2));

						if (augmentStack(newNode, orgStack))
						{
							++count;
							newNode.release();
						}
					}
				}

				break;
			}
		}
	}

	return count;
}


//
// Find the streams that can use an index with the currently active streams
//

void Optimizer::findDependentStreams(const StreamList& streams,
									 StreamList& dependent_streams,
									 StreamList& free_streams)
{
#ifdef OPT_DEBUG_RETRIEVAL
	if (streams.hasData())
		printf("Detecting dependent streams:\n");
#endif

	for (const auto stream : streams)
	{
		const auto tail = &csb->csb_rpt[stream];

		// Set temporary active flag for this stream
		tail->activate();

		bool indexed_relationship = false;

		if (conjuncts.hasData())
		{
			// Calculate the inversion for this stream.
			// The returning candidate contains the streams that will be used for
			// index retrieval. This meant that if some stream is used this stream
			// depends on already active streams and can not be used in a separate
			// SORT/MERGE.

			Retrieval retrieval(tdbb, this, stream, false, false, nullptr, true);
			const auto candidate = retrieval.getInversion();

			if (candidate->dependentFromStreams.hasData())
				indexed_relationship = true;
		}

		if (indexed_relationship)
			dependent_streams.add(stream);
		else
			free_streams.add(stream);

		// Reset active flag
		tail->deactivate();
	}
}


//
// Form streams into rivers (combinations of streams)
//

bool Optimizer::formRiver(unsigned streamCount,
						  StreamList& streams,
						  const StreamList& joinedStreams,
						  RiverList& rivers,
						  SortNode** sortClause)
{
	const auto count = joinedStreams.getCount();
	fb_assert(count);

	if (count != streamCount)
		sortClause = nullptr;

	HalfStaticArray<RecordSource*, OPT_STATIC_ITEMS> rsbs(count);

	for (const auto stream : joinedStreams)
	{
		rsbs.add(generateRetrieval(stream, sortClause, false, false, nullptr));
		sortClause = nullptr;

		// Remove already consumed streams from remainingStreams
		FB_SIZE_T pos;
		if (streams.find(stream, pos))
			streams.remove(pos);
		else
			fb_assert(false);
	}

	const auto rsb = (count == 1) ? rsbs[0] :
		FB_NEW_POOL(getPool()) NestedLoopJoin(csb, count, rsbs.begin());

	// Allocate a river block and move the best order into it
	const auto river = FB_NEW_POOL(getPool()) River(csb, rsb, nullptr, joinedStreams);
	river->deactivate(csb);
	rivers.push(river);

	return streams.hasData();
}


//
// Form streams into rivers according to the user-specified plan
//

void Optimizer::formRivers(const StreamList& streams,
						   RiverList& rivers,
						   SortNode** sortClause,
						   const PlanNode* planClause)
{
	StreamList tempStreams;

	// This must be a join or a merge node, so go through
	// the substreams and place them into the temp vector
	// for formation into a river

	for (const auto planNode : planClause->subNodes)
	{
		if (planNode->type == PlanNode::TYPE_JOIN)
		{
			formRivers(streams, rivers, sortClause, planNode);
			continue;
		}

		// At this point we must have a retrieval node, so put
		// the stream into the river
		fb_assert(planNode->type == PlanNode::TYPE_RETRIEVE);

		const StreamType stream = planNode->relationNode->getStream();

		// dimitr:	the plan may contain more retrievals than the "streams"
		//			array (some streams could already be joined to the active
		//			rivers), so we populate the "temp" array only with the
		//			streams that appear in both the plan and the "streams"
		//			array.

		if (streams.exist(stream))
			tempStreams.add(stream);
	}

	// Just because the user specified a join does not mean that
	// we are able to form a river;  thus form as many rivers out
	// of the join are as necessary to exhaust the streams.
	// AB: Only form rivers when any retrieval node is seen, for
	// example a MERGE on two JOINs will come with no retrievals
	// at this point.
	// CVC: Notice "plan_node" is pointing to the last element in the loop above.
	// If the loop didn't execute, we had garbage in "planNode".

	if (tempStreams.hasData())
	{
		const auto count = tempStreams.getCount();

		InnerJoin innerJoin(tdbb, this, tempStreams,
						 	(sortClause ? *sortClause : nullptr),
							(planClause != nullptr));

		StreamList joinedStreams;
		while (innerJoin.findJoinOrder(joinedStreams))
		{
			if (!formRiver(count, tempStreams, joinedStreams, rivers, sortClause))
				break;
		}
	}
}


//
// We've got a set of rivers that may or may not be amenable to
// a hash join or a sort/merge join, and it's time to find out.
// If there are, build an appropriate join RecordSource,
// push it on the rsb stack, and update rivers accordingly.
// If two or more rivers were successfully joined, return true.
// If the whole things is a moby no-op, return false.
//

bool Optimizer::generateEquiJoin(RiverList& org_rivers)
{
	ULONG selected_rivers[OPT_STREAM_BITS], selected_rivers2[OPT_STREAM_BITS];
	ValueExprNode** eq_class;

	// Count the number of "rivers" involved in the operation, then allocate
	// a scratch block large enough to hold values to compute equality
	// classes.

	const unsigned cnt = (unsigned) org_rivers.getCount();

	if (cnt < 2)
		return false;

	HalfStaticArray<ValueExprNode*, OPT_STATIC_ITEMS> scratch;
	scratch.grow(baseConjuncts * cnt);
	ValueExprNode** classes = scratch.begin();

	// Compute equivalence classes among streams. This involves finding groups
	// of streams joined by field equalities.

	ValueExprNode** last_class = classes;

	for (auto iter = getBaseConjuncts(); iter.hasData(); ++iter)
	{
		if (iter & CONJUNCT_USED)
			continue;

		const auto cmpNode = nodeAs<ComparativeBoolNode>(*iter);

		if (!cmpNode || (cmpNode->blrOp != blr_eql && cmpNode->blrOp != blr_equiv))
			continue;

		ValueExprNode* node1 = cmpNode->arg1;
		ValueExprNode* node2 = cmpNode->arg2;

		dsc result, desc1, desc2;
		node1->getDesc(tdbb, csb, &desc1);
		node2->getDesc(tdbb, csb, &desc2);

		// Ensure that arguments can be compared in the binary form
		if (!CVT2_get_binary_comparable_desc(&result, &desc1, &desc2))
			continue;

		// Cast the arguments, if required
		if (!DSC_EQUIV(&result, &desc1, true) || !DSC_EQUIV(&result, &desc2, true))
		{
			if (!DSC_EQUIV(&result, &desc1, true))
			{
				const auto cast = FB_NEW_POOL(getPool()) CastNode(getPool());
				cast->source = node1;
				cast->castDesc = result;
				cast->impureOffset = csb->allocImpure<impure_value>();
				node1 = cast;
			}

			if (!DSC_EQUIV(&result, &desc2, true))
			{
				const auto cast = FB_NEW_POOL(getPool()) CastNode(getPool());
				cast->source = node2;
				cast->castDesc = result;
				cast->impureOffset = csb->allocImpure<impure_value>();
				node2 = cast;
			}
		}

		for (unsigned i = 0; i < org_rivers.getCount(); i++)
		{
			const auto river1 = org_rivers[i];

			if (!river1->isReferenced(node1))
			{
				if (!river1->isReferenced(node2))
					continue;

				ValueExprNode* const temp = node1;
				node1 = node2;
				node2 = temp;
			}

			for (unsigned j = i + 1; j < org_rivers.getCount(); j++)
			{
				const auto river2 = org_rivers[j];

				if (river2->isReferenced(node2))
				{
					for (eq_class = classes; eq_class < last_class; eq_class += cnt)
					{
						if (fieldEqual(node1, classes[i]) ||
							fieldEqual(node2, classes[j]))
						{
							break;
						}
					}

					eq_class[i] = node1;
					eq_class[j] = node2;

					if (eq_class == last_class)
						last_class += cnt;
				}
			}
		}
	}

	// Pick both a set of classes and a set of rivers on which to join.
	// Obviously, if the set of classes is empty, return false
	// to indicate that nothing could be done.

	unsigned river_cnt = 0;
	HalfStaticArray<ValueExprNode**, OPT_STATIC_ITEMS> selected_classes(cnt);

	for (eq_class = classes; eq_class < last_class; eq_class += cnt)
	{
		unsigned i = getRiverCount(cnt, eq_class);

		if (i > river_cnt)
		{
			river_cnt = i;
			selected_classes.shrink(0);
			selected_classes.add(eq_class);
			classMask(cnt, eq_class, selected_rivers);
		}
		else
		{
			classMask(cnt, eq_class, selected_rivers2);

			for (i = 0; i < OPT_STREAM_BITS; i++)
			{
				if ((selected_rivers[i] & selected_rivers2[i]) != selected_rivers[i])
					break;
			}

			if (i == OPT_STREAM_BITS)
				selected_classes.add(eq_class);
		}
	}

	if (!river_cnt)
		return false;

	// AB: Deactivate currently all streams from every river, because we
	// need to know which nodes are computable between the rivers used
	// for the merge.

	StreamStateHolder stateHolder(csb);
	stateHolder.deactivate();

	HalfStaticArray<RecordSource*, OPT_STATIC_ITEMS> rsbs;
	HalfStaticArray<NestValueArray*, OPT_STATIC_ITEMS> keys;

	// Unconditionally disable merge joins in favor of hash joins.
	// This is a temporary debugging measure.
	bool prefer_merge_over_hash = false;

	// AB: Get the lowest river position from the rivers that are merged

	RiverList rivers_to_merge;
	unsigned lowest_river_position = MAX_ULONG;
	unsigned number = 0;

	for (River** iter = org_rivers.begin(); iter < org_rivers.end(); number++)
	{
		River* const river = *iter;

		if (!(TEST_DEP_BIT(selected_rivers, number)))
		{
			iter++;
			continue;
		}

		if (number < lowest_river_position)
			lowest_river_position = number;

		rivers_to_merge.add(river);
		org_rivers.remove(iter);

		// Apply local river booleans, if any

		auto rsb = applyLocalBoolean(river);

		// Collect RSBs and keys to join

		const auto key = FB_NEW_POOL(getPool()) SortNode(getPool());

		if (prefer_merge_over_hash)
		{
			ValueExprNode*** selected_class;

			for (selected_class = selected_classes.begin();
				 selected_class != selected_classes.end(); ++selected_class)
			{
				key->direction.add(ORDER_ASC);	// Ascending sort
				key->nullOrder.add(NULLS_DEFAULT);	// Default nulls placement
				key->expressions.add((*selected_class)[number]);
			}

			StreamList streams;
			streams.assign(river->getStreams());
			rsb = generateSort(streams, nullptr, rsb, key, favorFirstRows(), false);
		}
		else
		{
			ValueExprNode*** selected_class;

			for (selected_class = selected_classes.begin();
				 selected_class != selected_classes.end(); ++selected_class)
			{
				key->expressions.add((*selected_class)[number]);
			}
		}

		// It seems that rivers are already sorted by their cardinality.
		// For a hash join, we need to choose the smallest ones as inner sub-streams,
		// hence we reverse the order when storing them in the temporary arrays.

		if (prefer_merge_over_hash)
		{
			rsbs.add(rsb);
			keys.add(&key->expressions);
		}
		else
		{
			rsbs.insert(0, rsb);
			keys.insert(0, &key->expressions);
		}
	}

	fb_assert(rsbs.getCount() == keys.getCount());

	// Build a join stream

	RecordSource* rsb = nullptr;

	if (prefer_merge_over_hash)
	{
		rsb = FB_NEW_POOL(getPool())
			MergeJoin(csb, rsbs.getCount(), (SortedStream**) rsbs.begin(), keys.begin());
	}
	else
	{
		rsb = FB_NEW_POOL(getPool())
			HashJoin(tdbb, csb, rsbs.getCount(), rsbs.begin(), keys.begin());
	}

	// Activate streams of all the rivers being merged

	for (const auto river : rivers_to_merge)
		river->activate(csb);

	// Pick up any boolean that may apply

	BoolExprNode* boolean = nullptr;

	for (auto iter = getBaseConjuncts(); iter.hasData(); ++iter)
	{
		if (!(iter & CONJUNCT_USED) &&
			!(iter->nodFlags & ExprNode::FLAG_RESIDUAL) &&
			iter->computable(csb, INVALID_STREAM, false))
		{
			compose(getPool(), &boolean, iter);
			iter |= CONJUNCT_USED;
		}
	}

	if (boolean)
		rsb = FB_NEW_POOL(getPool()) FilteredStream(csb, rsb, boolean);

	const auto merged_river = FB_NEW_POOL(getPool()) River(csb, rsb, rivers_to_merge);

	org_rivers.insert(lowest_river_position, merged_river);

	return true;
}


//
//	Find all indexed relationships between streams,
//	then form streams into rivers (combinations of streams)
//

void Optimizer::generateInnerJoin(StreamList& streams,
								  RiverList& rivers,
								  SortNode** sortClause,
								  const PlanNode* planClause)
{
	if (streams.isEmpty())
		return;

	if (planClause && streams.getCount() > 1)
	{
		// this routine expects a join/merge
		formRivers(streams, rivers, sortClause, planClause);
		return;
	}

	const auto count = streams.getCount();

	InnerJoin innerJoin(tdbb, this, streams,
						(sortClause ? *sortClause : nullptr),
						(planClause != nullptr));

	StreamList joinedStreams;
	while (innerJoin.findJoinOrder(joinedStreams))
	{
		if (!formRiver(count, streams, joinedStreams, rivers, sortClause))
			break;
	}
}


//
// Generate a top level outer join. The "outer" and "inner" sub-streams must be
// handled differently from each other. The inner is like other streams.
// The outer one isn't because conjuncts may not eliminate records from the stream.
// They only determine if a join with an inner stream record is to be attempted.
//

RecordSource* Optimizer::generateOuterJoin(RiverList& rivers,
										   SortNode** sortClause)
{
	struct {
		RecordSource* stream_rsb;
		StreamType stream_num;
	} stream_o, stream_i, *stream_ptr[2];

	// Determine which stream should be outer and which is inner.
	// In the case of a left join, the syntactically left stream is the
	// outer, and the right stream is the inner.  For all others, swap
	// the sense of inner and outer, though for a full join it doesn't
	// matter and we should probably try both orders to see which is
	// more efficient.
	if (rse->rse_jointype != blr_left)
	{
		stream_ptr[1] = &stream_o;
		stream_ptr[0] = &stream_i;
	}
	else
	{
		stream_ptr[0] = &stream_o;
		stream_ptr[1] = &stream_i;
	}

	// Loop through the outer join sub-streams in
	// reverse order because rivers may have been PUSHed
	for (int i = 1; i >= 0; i--)
	{
		const auto node = rse->rse_relations[i];

		if (nodeIs<RelationSourceNode>(node) || nodeIs<LocalTableSourceNode>(node))
		{
			stream_ptr[i]->stream_rsb = nullptr;
			stream_ptr[i]->stream_num = node->getStream();
		}
		else
		{
			River* const river = rivers.pop();
			stream_ptr[i]->stream_rsb = river->getRecordSource();
		}
	}

	if (!isFullJoin())
	{
		// Generate rsbs for the sub-streams.
		// For the left sub-stream we also will get a boolean back.
		BoolExprNode* boolean = nullptr;

		if (!stream_o.stream_rsb)
		{
			stream_o.stream_rsb =
				generateRetrieval(stream_o.stream_num, sortClause, true, false, &boolean);
		}

		if (!stream_i.stream_rsb)
		{
			// AB: the sort clause for the inner stream of an OUTER JOIN
			//	   should never be used for the index retrieval
			stream_i.stream_rsb =
				generateRetrieval(stream_i.stream_num, nullptr, false, true, nullptr);
		}

		// generate a parent boolean rsb for any remaining booleans that
		// were not satisfied via an index lookup
		stream_i.stream_rsb = generateResidualBoolean(stream_i.stream_rsb);

		// Allocate and fill in the rsb
		return FB_NEW_POOL(getPool())
			NestedLoopJoin(csb, stream_o.stream_rsb, stream_i.stream_rsb,
						   boolean, OUTER_JOIN);
	}

	bool hasOuterRsb = true, hasInnerRsb = true;
	BoolExprNode* boolean = nullptr;

	if (!stream_o.stream_rsb)
	{
		hasOuterRsb = false;
		stream_o.stream_rsb =
			generateRetrieval(stream_o.stream_num, nullptr, true, false, &boolean);
	}

	if (!stream_i.stream_rsb)
	{
		hasInnerRsb = false;
		stream_i.stream_rsb =
			generateRetrieval(stream_i.stream_num, nullptr, false, true, nullptr);
	}

	const auto innerRsb = generateResidualBoolean(stream_i.stream_rsb);

	const auto rsb1 = FB_NEW_POOL(getPool())
		NestedLoopJoin(csb, stream_o.stream_rsb, innerRsb, boolean, OUTER_JOIN);

	for (auto iter = getConjuncts(); iter.hasData(); ++iter)
	{
		if (iter & CONJUNCT_USED)
			iter.reset(CMP_clone_node_opt(tdbb, csb, iter));
	}

	if (!hasInnerRsb)
		csb->csb_rpt[stream_i.stream_num].deactivate();

	if (!hasOuterRsb)
		csb->csb_rpt[stream_o.stream_num].deactivate();

	boolean = nullptr;

	if (!hasInnerRsb)
	{
		stream_i.stream_rsb =
			generateRetrieval(stream_i.stream_num, nullptr, true, false, &boolean);
	}

	if (!hasOuterRsb)
	{
		stream_o.stream_rsb =
			generateRetrieval(stream_o.stream_num, nullptr, false, false, nullptr);
	}

	const auto outerRsb = generateResidualBoolean(stream_o.stream_rsb);

	const auto rsb2 = FB_NEW_POOL(getPool())
		NestedLoopJoin(csb, stream_i.stream_rsb, outerRsb, boolean, ANTI_JOIN);

	return FB_NEW_POOL(getPool()) FullOuterJoin(csb, rsb1, rsb2);
}


//
// Pick up any residual boolean remaining, meaning those that have not been used
// as part of some join. These booleans must still be applied to the result stream.
//

RecordSource* Optimizer::generateResidualBoolean(RecordSource* rsb)
{
	BoolExprNode* boolean = nullptr;

	for (auto iter = getBaseConjuncts(); iter.hasData(); ++iter)
	{
		if (!(iter & CONJUNCT_USED))
		{
			compose(getPool(), &boolean, iter);
			iter |= CONJUNCT_USED;
		}
	}

	return boolean ? FB_NEW_POOL(getPool()) FilteredStream(csb, rsb, boolean) : rsb;
}


//
// Compile a record retrieval source
//

RecordSource* Optimizer::generateRetrieval(StreamType stream,
										   SortNode** sortClause,
										   bool outerFlag,
										   bool innerFlag,
										   BoolExprNode** returnBoolean)
{
	const auto tail = &csb->csb_rpt[stream];
	const auto relation = tail->csb_relation;
	fb_assert(relation);

	const string alias = makeAlias(stream);
	tail->activate();

	// Time to find inversions. For each index on the relation
	// match all unused booleans against the index looking for upper
	// and lower bounds that can be computed by the index. When
	// all unused conjunctions are exhausted, see if there is enough
	// information for an index retrieval. If so, build up an
	// inversion component of the boolean.

	RecordSource* rsb = nullptr;
	InversionNode* inversion = nullptr;
	BoolExprNode* condition = nullptr;
	Array<DbKeyRangeNode*> dbkeyRanges;

	if (relation->rel_file)
	{
		// External table
		rsb = FB_NEW_POOL(getPool()) ExternalTableScan(csb, alias, stream, relation);
	}
	else if (relation->isVirtual())
	{
		// Virtual table: monitoring or security
		switch (relation->rel_id)
		{
		case rel_global_auth_mapping:
			rsb = FB_NEW_POOL(getPool()) GlobalMappingScan(csb, alias, stream, relation);
			break;

		case rel_sec_users:
		case rel_sec_user_attributes:
			rsb = FB_NEW_POOL(getPool()) UsersTableScan(csb, alias, stream, relation);
			break;

		case rel_sec_db_creators:
			rsb = FB_NEW_POOL(getPool()) DbCreatorsScan(csb, alias, stream, relation);
			break;

		case rel_time_zones:
			rsb = FB_NEW_POOL(getPool()) TimeZonesTableScan(csb, alias, stream, relation);
			break;

		case rel_config:
			rsb = FB_NEW_POOL(getPool()) ConfigTableScan(csb, alias, stream, relation);
			break;

		case rel_keywords:
			rsb = FB_NEW_POOL(getPool()) KeywordsTableScan(csb, alias, stream, relation);
			break;

		default:
			rsb = FB_NEW_POOL(getPool()) MonitoringTableScan(csb, alias, stream, relation);
			break;
		}
	}
	else
	{
		// Persistent table
		Retrieval retrieval(tdbb, this, stream, outerFlag, innerFlag,
							(sortClause ? *sortClause : nullptr), false);
		const auto candidate = retrieval.getInversion();

		if (candidate)
		{
			inversion = candidate->inversion;
			condition = candidate->condition;
			dbkeyRanges.assign(candidate->dbkeyRanges);

			// Just for safety sake, this condition must be already checked
			// inside OptimizerRetrieval::matchOnIndexes()

			if (inversion && condition &&
				!condition->computable(csb, stream, false))
			{
				fb_assert(false);
				inversion = nullptr;
				condition = nullptr;
				dbkeyRanges.clear();
			}
		}

		const auto navigation = retrieval.getNavigation();

		if (navigation)
		{
			if (sortClause)
				*sortClause = nullptr;

			navigation->setInversion(inversion, condition);

			rsb = navigation;
		}
	}

	if (outerFlag)
	{
		fb_assert(returnBoolean);
		*returnBoolean = nullptr;

		// Now make another pass thru the outer conjuncts only, finding unused,
		// computable booleans. When one is found, roll it into a final
		// boolean and mark it used.
		for (auto iter = getBaseConjuncts(); iter.hasData(); ++iter)
		{
			if (!(iter & CONJUNCT_USED) &&
				!(iter->nodFlags & ExprNode::FLAG_RESIDUAL) &&
				iter->computable(csb, INVALID_STREAM, false))
			{
				compose(getPool(), returnBoolean, iter);
				iter |= CONJUNCT_USED;
			}
		}
	}

	// Now make another pass thru the conjuncts finding unused, computable
	// booleans.  When one is found, roll it into a final boolean and mark
	// it used. If a computable boolean didn't match against an index then
	// mark the stream to denote unmatched booleans.
	BoolExprNode* boolean = nullptr;

	for (auto iter = getConjuncts(innerFlag, outerFlag); iter.hasData(); ++iter)
	{
		if (!(iter & CONJUNCT_USED) &&
			!(iter->nodFlags & ExprNode::FLAG_RESIDUAL) &&
			iter->computable(csb, INVALID_STREAM, false))
		{
			// If inversion is available, utilize all conjuncts that refer to
			// the stream being retrieved. Otherwise, utilize only conjuncts
			// that are local to this stream. The remaining ones are left in piece
			// as possible candidates for a merge/hash join.

			if ((inversion && iter->containsStream(stream)) ||
				(!inversion && iter->computable(csb, stream, true)))
			{
				compose(getPool(), &boolean, iter);
				iter |= CONJUNCT_USED;

				if (!outerFlag && !(iter & CONJUNCT_MATCHED))
					tail->csb_flags |= csb_unmatched;
			}
		}
	}

	if (!rsb)
	{
		if (inversion && condition)
		{
			RecordSource* const rsb1 =
				FB_NEW_POOL(getPool()) FullTableScan(csb, alias, stream, relation, dbkeyRanges);
			RecordSource* const rsb2 =
				FB_NEW_POOL(getPool()) BitmapTableScan(csb, alias, stream, relation, inversion);

			rsb = FB_NEW_POOL(getPool()) ConditionalStream(csb, rsb1, rsb2, condition);
		}
		else if (inversion)
		{
			rsb = FB_NEW_POOL(getPool()) BitmapTableScan(csb, alias, stream, relation, inversion);
		}
		else
		{
			rsb = FB_NEW_POOL(getPool()) FullTableScan(csb, alias, stream, relation, dbkeyRanges);

			if (boolean)
				csb->csb_rpt[stream].csb_flags |= csb_unmatched;
		}
	}

	return boolean ? FB_NEW_POOL(getPool()) FilteredStream(csb, rsb, boolean) : rsb;
}


//
// Make an alias string suitable for printing as part of the plan.
// For views, this means multiple aliases to distinguish the base table.
//

string Optimizer::makeAlias(StreamType stream)
{
	string alias;

	const CompilerScratch::csb_repeat* csb_tail = &csb->csb_rpt[stream];

	if (csb_tail->csb_view || csb_tail->csb_alias)
	{
		ObjectsArray<string> alias_list;

		while (csb_tail)
		{
			if (csb_tail->csb_alias)
				alias_list.push(*csb_tail->csb_alias);
			else if (csb_tail->csb_relation)
				alias_list.push(csb_tail->csb_relation->rel_name.c_str());

			if (!csb_tail->csb_view)
				break;

			csb_tail = &csb->csb_rpt[csb_tail->csb_view_stream];
		}

		while (alias_list.hasData())
		{
			alias += alias_list.pop();

			if (alias_list.hasData())
				alias += ' ';
		}
	}
	else if (csb_tail->csb_relation)
		alias = csb_tail->csb_relation->rel_name.c_str();
	else if (csb_tail->csb_procedure)
		alias = csb_tail->csb_procedure->getName().toString();
	//// TODO: LocalTableSourceNode
	else
		fb_assert(false);

	return alias;
}


//
// From the predicate, boolean, and infer a new predicate using arg1 & arg2 as the first two
// parameters to the predicate.
//
// This is used when the engine knows A<B and A=C, and creates a new node to represent
// the infered knowledge C<B.
//
// Note that this may be sometimes incorrect with 3-value logic
// (per Chris Date's Object & Relations seminar).
// Later stages of query evaluation evaluate exactly the originally specified query,
// so 3-value issues are caught there. Making this inference might cause us to
// examine more records than needed, but would not result in incorrect results.
//
// Note that some predicates have more than two parameters for a boolean operation
// (LIKE has an optional 3rd parameter for the ESCAPE character option of SQL).
//

BoolExprNode* Optimizer::makeInferenceNode(BoolExprNode* boolean,
										   ValueExprNode* arg1,
										   ValueExprNode* arg2)
{
	const auto cmpNode = nodeAs<ComparativeBoolNode>(boolean);
	fb_assert(cmpNode);	// see our caller

	// Clone the input predicate
	const auto newCmpNode =
		FB_NEW_POOL(getPool()) ComparativeBoolNode(getPool(), cmpNode->blrOp);

	// We may safely copy invariantness flag because
	// (1) we only distribute field equalities
	// (2) invariantness of second argument of STARTING WITH or LIKE is solely
	//    determined by its dependency on any of the fields
	// If provisions above change the line below will have to be modified
	newCmpNode->nodFlags = cmpNode->nodFlags;

	// Share impure area for cached invariant value used to hold pre-compiled
	// pattern for new LIKE and CONTAINING algorithms.
	// Proper cloning of impure area for this node would require careful accounting
	// of new invariant dependencies - we avoid such hassles via using single
	// cached pattern value for all node clones. This is faster too.
	if (newCmpNode->nodFlags & ExprNode::FLAG_INVARIANT)
		newCmpNode->impureOffset = cmpNode->impureOffset;

	// But substitute new values for some of the predicate arguments
	newCmpNode->arg1 = CMP_clone_node_opt(tdbb, csb, arg1);
	newCmpNode->arg2 = CMP_clone_node_opt(tdbb, csb, arg2);

	// Arguments after the first two are just cloned (eg: LIKE ESCAPE clause)
	if (cmpNode->arg3)
		newCmpNode->arg3 = CMP_clone_node_opt(tdbb, csb, cmpNode->arg3);

	return newCmpNode;
}


//
// Optimize a LIKE/SIMILAR expression, if possible, into a "STARTING WITH" AND a "LIKE/SIMILAR".
// This will allow us to use the index for the starting with, and the LIKE/SIMILAR can just tag
// along for the ride. But on the ride it does useful work, consider match LIKE/SIMILAR "ab%c".
// This is optimized by adding AND STARTING WITH "ab", but the LIKE/SIMILAR clause is still needed.
//

ValueExprNode* Optimizer::optimizeLikeSimilar(ComparativeBoolNode* cmpNode)
{
	ValueExprNode* matchNode = cmpNode->arg1;
	ValueExprNode* patternNode = cmpNode->arg2;
	ValueExprNode* escapeNode = cmpNode->arg3;

	// if the pattern string or the escape string can't be
	// evaluated at compile time, forget it
	if (!nodeIs<LiteralNode>(patternNode) || (escapeNode && !nodeIs<LiteralNode>(escapeNode)))
		return nullptr;

	dsc matchDesc;
	matchNode->getDesc(tdbb, csb, &matchDesc);

	dsc* patternDesc = &nodeAs<LiteralNode>(patternNode)->litDesc;
	dsc* escapeDesc = nullptr;

	if (escapeNode)
		escapeDesc = &nodeAs<LiteralNode>(escapeNode)->litDesc;

	// if either is not a character expression, forget it
	if ((matchDesc.dsc_dtype > dtype_any_text) ||
		(patternDesc->dsc_dtype > dtype_any_text) ||
		(escapeNode && escapeDesc->dsc_dtype > dtype_any_text))
	{
		return nullptr;
	}

	TextType* matchTextType = INTL_texttype_lookup(tdbb, INTL_TTYPE(&matchDesc));
	CharSet* matchCharset = matchTextType->getCharSet();
	TextType* patternTextType = INTL_texttype_lookup(tdbb, INTL_TTYPE(patternDesc));
	CharSet* patternCharset = patternTextType->getCharSet();

	if (cmpNode->blrOp == blr_like)
	{
		UCHAR escape_canonic[sizeof(ULONG)];
		UCHAR first_ch[sizeof(ULONG)];
		ULONG first_len;
		UCHAR* p;
		USHORT p_count;
		MoveBuffer escapeBuffer;

		// Get the escape character, if any
		if (escapeNode)
		{
			// Ensure escape string is same character set as match string
			p_count = MOV_make_string2(tdbb, escapeDesc, INTL_TTYPE(&matchDesc), &p, escapeBuffer);

			first_len = matchCharset->substring(p_count, p, sizeof(first_ch), first_ch, 0, 1);
			matchTextType->canonical(first_len, p, sizeof(escape_canonic), escape_canonic);
		}

		MoveBuffer patternBuffer;
		p_count = MOV_make_string2(tdbb, patternDesc, INTL_TTYPE(&matchDesc), &p, patternBuffer);

		first_len = matchCharset->substring(p_count, p, sizeof(first_ch), first_ch, 0, 1);

		UCHAR first_canonic[sizeof(ULONG)];
		matchTextType->canonical(first_len, p, sizeof(first_canonic), first_canonic);

		const BYTE canWidth = matchTextType->getCanonicalWidth();

		const UCHAR* matchOneChar = matchCharset->getSqlMatchOneLength() != 0 ?
			matchTextType->getCanonicalChar(TextType::CHAR_SQL_MATCH_ONE) : nullptr;
		const UCHAR* matchAnyChar = matchCharset->getSqlMatchAnyLength() != 0 ?
			matchTextType->getCanonicalChar(TextType::CHAR_SQL_MATCH_ANY) : nullptr;

		// If the first character is a wildcard char, forget it.
		if ((!escapeNode || memcmp(first_canonic, escape_canonic, canWidth) != 0) &&
			((matchOneChar && memcmp(first_canonic, matchOneChar, canWidth) == 0) ||
			(matchAnyChar && memcmp(first_canonic, matchAnyChar, canWidth) == 0)))
		{
			return nullptr;
		}

		// allocate a literal node to store the starting with string;
		// assume it will be shorter than the pattern string

		const auto literal = FB_NEW_POOL(getPool()) LiteralNode(getPool());
		literal->litDesc = *patternDesc;
		UCHAR* q = literal->litDesc.dsc_address = FB_NEW_POOL(getPool()) UCHAR[literal->litDesc.dsc_length];

		// Set the string length to point till the first wildcard character.

		HalfStaticArray<UCHAR, BUFFER_SMALL> patternCanonical;
		ULONG patternCanonicalLen = p_count / matchCharset->minBytesPerChar() * canWidth;

		patternCanonicalLen = matchTextType->canonical(p_count, p,
			patternCanonicalLen, patternCanonical.getBuffer(patternCanonicalLen));

		for (const UCHAR* patternPtr = patternCanonical.begin(); patternPtr < patternCanonical.end(); )
		{
			// if there are escape characters, skip past them and don't treat the next char as a wildcard
			const UCHAR* patternPtrStart = patternPtr;
			patternPtr += canWidth;

			if (escapeNode && (memcmp(patternPtrStart, escape_canonic, canWidth) == 0))
			{
				// Check for Escape character at end of string
				if (!(patternPtr < patternCanonical.end()))
					break;

				patternPtrStart = patternPtr;
				patternPtr += canWidth;
			}
			else if ((matchOneChar && memcmp(patternPtrStart, matchOneChar, canWidth) == 0) ||
					(matchAnyChar && memcmp(patternPtrStart, matchAnyChar, canWidth) == 0))
			{
				break;
			}

			q += patternCharset->substring(patternDesc->dsc_length,
					patternDesc->dsc_address,
					literal->litDesc.dsc_length - (q - literal->litDesc.dsc_address), q,
					(patternPtrStart - patternCanonical.begin()) / canWidth, 1);
		}

		literal->litDesc.dsc_length = q - literal->litDesc.dsc_address;

		return literal;
	}
	else
	{
		fb_assert(cmpNode->blrOp == blr_similar);

		MoveBuffer escapeBuffer;
		UCHAR* escapeStart = nullptr;
		ULONG escapeLen = 0;

		// Get the escape character, if any
		if (escapeNode)
		{
			// Ensure escape string is same character set as match string
			escapeLen = MOV_make_string2(tdbb, escapeDesc, INTL_TTYPE(&matchDesc), &escapeStart, escapeBuffer);
		}

		MoveBuffer patternBuffer;
		UCHAR* patternStart;
		ULONG patternLen = MOV_make_string2(tdbb, patternDesc, INTL_TTYPE(&matchDesc), &patternStart, patternBuffer);
		const auto patternEnd = patternStart + patternLen;
		const UCHAR* patternPtr = patternStart;

		MoveBuffer prefixBuffer;
		ULONG charLen = 0;

		while (IntlUtil::readOneChar(matchCharset, &patternPtr, patternEnd, &charLen))
		{
			if (escapeNode && charLen == escapeLen && memcmp(patternPtr, escapeStart, escapeLen) == 0)
			{
				if (!IntlUtil::readOneChar(matchCharset, &patternPtr, patternEnd, &charLen) ||
					!((charLen == escapeLen && memcmp(patternPtr, escapeStart, escapeLen) == 0) ||
					  (charLen == 1 && SimilarToRegex::isSpecialChar(*patternPtr))))
				{
					// Invalid escape.
					return nullptr;
				}
			}
			else if (charLen == 1 && SimilarToRegex::isSpecialChar(*patternPtr))
				break;

			prefixBuffer.push(patternPtr, charLen);
		}

		if (prefixBuffer.isEmpty())
			return nullptr;

		// Allocate a literal node to store the starting with string.
		// Use the match text type as the pattern string is converted to it.

		const auto literal = FB_NEW_POOL(getPool()) LiteralNode(getPool());
		literal->litDesc.makeText(prefixBuffer.getCount(), INTL_TTYPE(&matchDesc),
			FB_NEW_POOL(getPool()) UCHAR[prefixBuffer.getCount()]);
		memcpy(literal->litDesc.dsc_address, prefixBuffer.begin(), prefixBuffer.getCount());

		return literal;
	}
}

void Optimizer::printf(const char* format, ...)
{
#ifdef OPT_DEBUG
	if (!debugFile)
		debugFile = os_utils::fopen(OPTIMIZER_DEBUG_FILE, "a");

	fb_assert(debugFile);

	va_list arglist;
	va_start(arglist, format);
	Firebird::string str;
	str.vprintf(format, arglist);
	va_end(arglist);

	fprintf(debugFile, str.c_str());
#endif
}
