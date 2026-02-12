/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		Optimizer.h
 *	DESCRIPTION:	Optimizer
 *
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
 *  The Original Code was created by Arno Brinkman
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Arno Brinkman <firebird@abvisie.nl>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

//#define OPT_DEBUG
//#define OPT_DEBUG_RETRIEVAL
//#define OPT_DEBUG_SYS_REQUESTS

#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/exe.h"
#include "../jrd/recsrc/RecordSource.h"

namespace Jrd {

// AB: 2005-11-05
// Constants below needs some discussions and ideas
const double REDUCE_SELECTIVITY_FACTOR_EQUALITY = 0.001;
const double REDUCE_SELECTIVITY_FACTOR_BETWEEN = 0.0025;
const double REDUCE_SELECTIVITY_FACTOR_LESS = 0.05;
const double REDUCE_SELECTIVITY_FACTOR_GREATER = 0.05;
const double REDUCE_SELECTIVITY_FACTOR_STARTING = 0.01;
const double REDUCE_SELECTIVITY_FACTOR_OTHER = 0.01;

// Cost of simple (CPU bound) operations is less than the page access cost
const double COST_FACTOR_MEMCOPY = 0.5;
const double COST_FACTOR_HASHING = 0.5;

const double MAXIMUM_SELECTIVITY = 1.0;
const double DEFAULT_SELECTIVITY = 0.1;

const double MINIMUM_CARDINALITY = 1.0;
const double THRESHOLD_CARDINALITY = 5.0;
const double DEFAULT_CARDINALITY = 1000.0;

// Default depth of an index tree (including one leaf page),
// also representing the minimal cost of the index scan.
// We assume that the root page would be always cached,
// so it's not included here.
const double DEFAULT_INDEX_COST = 3.0;


struct index_desc;
class jrd_rel;
class IndexTableScan;
class ComparativeBoolNode;
class InversionNode;
class PlanNode;
class SortNode;
class River;
class SortedStream;


//
// StreamStateHolder
//

class StreamStateHolder
{
public:
	explicit StreamStateHolder(CompilerScratch* csb)
		: m_csb(csb), m_streams(csb->csb_pool), m_flags(csb->csb_pool)
	{
		for (StreamType stream = 0; stream < csb->csb_n_stream; stream++)
			m_streams.add(stream);

		init();
	}

	StreamStateHolder(CompilerScratch* csb, const StreamList& streams)
		: m_csb(csb), m_streams(csb->csb_pool), m_flags(csb->csb_pool)
	{
		m_streams.assign(streams);

		init();
	}

	~StreamStateHolder()
	{
		for (FB_SIZE_T i = 0; i < m_streams.getCount(); i++)
		{
			const StreamType stream = m_streams[i];

			if (m_flags[i >> 3] & (1 << (i & 7)))
				m_csb->csb_rpt[stream].activate();
			else
				m_csb->csb_rpt[stream].deactivate();
		}
	}

	void activate()
	{
		for (const auto stream : m_streams)
			m_csb->csb_rpt[stream].activate();
	}

	void deactivate()
	{
		for (const auto stream : m_streams)
			m_csb->csb_rpt[stream].deactivate();
	}

private:
	void init()
	{
		m_flags.resize(FLAG_BYTES(m_streams.getCount()));

		for (FB_SIZE_T i = 0; i < m_streams.getCount(); i++)
		{
			const StreamType stream = m_streams[i];

			if (m_csb->csb_rpt[stream].csb_flags & csb_active)
				m_flags[i >> 3] |= (1 << (i & 7));
		}
	}

	CompilerScratch* const m_csb;
	StreamList m_streams;
	Firebird::HalfStaticArray<UCHAR, sizeof(SLONG)> m_flags;
};


//
// River
//

typedef Firebird::HalfStaticArray<River*, OPT_STATIC_ITEMS> RiverList;

class River
{
public:
	River(CompilerScratch* csb, RecordSource* rsb, RecordSourceNode* node, const StreamList& streams)
		: m_rsb(rsb), m_nodes(csb->csb_pool), m_streams(csb->csb_pool, streams)
	{
		if (node)
			m_nodes.add(node);
	}

	River(CompilerScratch* csb, RecordSource* rsb, RiverList& rivers)
		: m_rsb(rsb), m_nodes(csb->csb_pool), m_streams(csb->csb_pool)
	{
		for (const auto subRiver : rivers)
		{
			m_nodes.join(subRiver->m_nodes);
			m_streams.join(subRiver->m_streams);
		}
	}

	RecordSource* getRecordSource() const
	{
		return m_rsb;
	}

	const StreamList& getStreams() const
	{
		return m_streams;
	}

	void activate(CompilerScratch* csb) const
	{
		for (const auto stream : m_streams)
			csb->csb_rpt[stream].activate();
	}

	void deactivate(CompilerScratch* csb) const
	{
		for (const auto stream : m_streams)
			csb->csb_rpt[stream].deactivate();
	}

	bool isReferenced(const ExprNode* node) const
	{
		SortedStreamList nodeStreams;
		node->collectStreams(nodeStreams);

		if (!nodeStreams.hasData())
			return false;

		for (const auto stream : nodeStreams)
		{
			if (!m_streams.exist(stream))
				return false;
		}

		return true;
	}

	bool isComputable(CompilerScratch* csb) const
	{
		for (const auto node : m_nodes)
		{
			if (!node->computable(csb, INVALID_STREAM, false))
				return false;
		}

		return true;
	}

protected:
	RecordSource* m_rsb;
	Firebird::HalfStaticArray<RecordSourceNode*, OPT_STATIC_ITEMS> m_nodes;
	StreamList m_streams;
};


//
// Optimizer
//

class Optimizer : public Firebird::PermanentStorage
{
public:
	struct Conjunct
	{
		// Conjunctions and their options
		BoolExprNode* node;
		unsigned flags;
	};

	static const unsigned CONJUNCT_USED		= 1;	// conjunct is used
	static const unsigned CONJUNCT_MATCHED	= 2;	// conjunct matches an index segment
	static const unsigned CONJUNCT_JOINED	= 4;	// conjunct used for equi-join

	typedef Firebird::HalfStaticArray<Conjunct, OPT_STATIC_ITEMS> ConjunctList;

	class ConjunctIterator
	{
	public:
		ConjunctIterator(Conjunct* _begin, const Conjunct* _end)
			: begin(_begin), end(_end)
		{
			rewind();
		}

		ConjunctIterator(const ConjunctIterator& other)
			: begin(other.begin), end(other.end), iter(other.iter)
		{}

		operator BoolExprNode*() const
		{
			return iter->node;
		}

		BoolExprNode* operator->() const
		{
			return iter->node;
		}

		BoolExprNode* operator*() const
		{
			return iter->node;
		}

		unsigned operator&(unsigned flags) const
		{
			return (iter->flags & flags);
		}

		void operator|=(unsigned flags)
		{
			iter->flags |= flags;
		}

		void operator++()
		{
			iter++;
		}

		bool hasData() const
		{
			return (iter < end);
		}

		unsigned getFlags() const
		{
			return iter->flags;
		}

		void rewind()
		{
			iter = begin;
		}

		void reset(BoolExprNode* node)
		{
			iter->node = node;
			iter->flags = 0;
		}

	private:
		Conjunct* const begin;
		const Conjunct* const end;
		Conjunct* iter;
	};

	ConjunctIterator getBaseConjuncts()
	{
		const auto begin = conjuncts.begin();
		const auto end = begin + baseConjuncts;

		return ConjunctIterator(begin, end);
	}

	ConjunctIterator getParentConjuncts()
	{
		const auto begin = conjuncts.begin() + baseParentConjuncts;
		const auto end = conjuncts.end();

		return ConjunctIterator(begin, end);
	}

	ConjunctIterator getConjuncts(bool outer = false, bool inner = false)
	{
		const auto begin = conjuncts.begin() + (outer ? baseParentConjuncts : 0);
		const auto end = inner ? begin + baseMissingConjuncts : conjuncts.end();

		return ConjunctIterator(begin, end);
	}

	static Firebird::string getPlan(thread_db* tdbb, const Statement* statement, bool detailed)
	{
		return statement ? statement->getPlan(tdbb, detailed) : "";
	}

	static double getSelectivity(const BoolExprNode* node)
	{
		auto factor = REDUCE_SELECTIVITY_FACTOR_OTHER;

		if (const auto binaryNode = nodeAs<BinaryBoolNode>(node))
		{
			if (binaryNode->blrOp == blr_and)
				factor = getSelectivity(binaryNode->arg1) * getSelectivity(binaryNode->arg2);
			else if (binaryNode->blrOp == blr_or)
				factor = getSelectivity(binaryNode->arg1) + getSelectivity(binaryNode->arg2);
			else
				fb_assert(false);
		}
		else if (const auto listNode = nodeAs<InListBoolNode>(node))
		{
			factor = REDUCE_SELECTIVITY_FACTOR_EQUALITY * listNode->list->items.getCount();
		}
		else if (nodeIs<MissingBoolNode>(node))
		{
			factor = REDUCE_SELECTIVITY_FACTOR_EQUALITY;
		}
		else if (const auto cmpNode = nodeAs<ComparativeBoolNode>(node))
		{
			switch (cmpNode->blrOp)
			{
			case blr_eql:
			case blr_equiv:
				factor = REDUCE_SELECTIVITY_FACTOR_EQUALITY;
				break;

			case blr_gtr:
			case blr_geq:
				factor = REDUCE_SELECTIVITY_FACTOR_GREATER;
				break;

			case blr_lss:
			case blr_leq:
				factor = REDUCE_SELECTIVITY_FACTOR_LESS;
				break;

			case blr_between:
				factor = REDUCE_SELECTIVITY_FACTOR_BETWEEN;
				break;

			case blr_starting:
				factor = REDUCE_SELECTIVITY_FACTOR_STARTING;
				break;

			default:
				break;
			}
		}

		// dimitr:
		//
		// Adjust to values similar to those used when the index selectivity is missing.
		// The final value will be in the range [0.1 .. 0.5] that also matches the v3/v4 logic.
		// This estimation is quite pessimistic but it seems to work better in practice,
		// especially when multiple unmatchable booleans are used.

		const auto adjustment = DEFAULT_SELECTIVITY / REDUCE_SELECTIVITY_FACTOR_EQUALITY;
		const auto selectivity = factor * adjustment;

		return MIN(selectivity, MAXIMUM_SELECTIVITY / 2);
	}

	static void adjustSelectivity(double& selectivity, double factor, double cardinality)
	{
		if (!cardinality)
			cardinality = DEFAULT_CARDINALITY;

		const auto minSelectivity = MAXIMUM_SELECTIVITY / cardinality;
		const auto diffSelectivity = selectivity > minSelectivity ?
			selectivity - minSelectivity : 0;
		selectivity = minSelectivity + diffSelectivity * factor;
	}

	static RecordSource* compile(thread_db* tdbb, CompilerScratch* csb, RseNode* rse)
	{
		bool firstRows = false;

		// System requests should not be affected by user-specified settings
		if (!(csb->csb_g_flags & csb_internal))
		{
			const auto dbb = tdbb->getDatabase();
			const auto defaultFirstRows = dbb->dbb_config->getOptimizeForFirstRows();

			const auto attachment = tdbb->getAttachment();
			firstRows = attachment->att_opt_first_rows.orElse(defaultFirstRows);
		}

		return Optimizer(tdbb, csb, rse, firstRows).compile(nullptr);
	}

	~Optimizer();

	RecordSource* compile(RseNode* subRse, BoolExprNodeStack* parentStack);
	void compileRelation(StreamType stream);
	unsigned decomposeBoolean(BoolExprNode* boolNode, BoolExprNodeStack& stack);
	void generateAggregateDistincts(MapNode* map);
	RecordSource* generateRetrieval(StreamType stream,
									SortNode** sortClause,
									bool outerFlag,
									bool innerFlag,
									BoolExprNode** returnBoolean = nullptr);
	SortedStream* generateSort(const StreamList& streams,
							   const StreamList* dbkeyStreams,
							   RecordSource* rsb, SortNode* sort,
							   bool refetchFlag, bool projectFlag);

	CompilerScratch* getCompilerScratch() const
	{
		return csb;
	}

	bool isInnerJoin() const
	{
		return (rse->rse_jointype == blr_inner);
	}

	bool isLeftJoin() const
	{
		return (rse->rse_jointype == blr_left);
	}

	bool isFullJoin() const
	{
		return (rse->rse_jointype == blr_full);
	}

	const StreamList& getOuterStreams() const
	{
		return outerStreams;
	}

	bool favorFirstRows() const
	{
		return firstRows;
	}

	bool isSemiJoined() const
	{
		return (rse->flags & RseNode::FLAG_SEMI_JOINED) != 0;
	}

	RecordSource* applyBoolean(RecordSource* rsb, ConjunctIterator& iter);
	RecordSource* applyLocalBoolean(RecordSource* rsb,
									const StreamList& streams,
									ConjunctIterator& iter);

	bool checkEquiJoin(BoolExprNode* boolean);
	bool getEquiJoinKeys(BoolExprNode* boolean,
						 NestConst<ValueExprNode>* node1,
						 NestConst<ValueExprNode>* node2);

	Firebird::string getStreamName(StreamType stream);
	Firebird::string makeAlias(StreamType stream);
	void printf(const char* format, ...);

private:
	Optimizer(thread_db* aTdbb, CompilerScratch* aCsb, RseNode* aRse, bool parentFirstRows);

	RecordSource* compile(BoolExprNodeStack* parentStack);

	void checkIndices();
	void checkSorts();
	unsigned distributeEqualities(BoolExprNodeStack& orgStack, unsigned baseCount);
	void findDependentStreams(const StreamList& streams,
							  StreamList& dependent_streams,
							  StreamList& free_streams);
	void formRivers(const StreamList& streams,
					RiverList& rivers,
					SortNode** sortClause,
					const PlanNode* planClause);
	bool generateEquiJoin(RiverList& rivers, JoinType joinType = INNER_JOIN);
	void generateInnerJoin(StreamList& streams,
						   RiverList& rivers,
						   SortNode** sortClause,
						   const PlanNode* planClause);
	RecordSource* generateOuterJoin(RiverList& rivers,
								    SortNode** sortClause);
	RecordSource* generateResidualBoolean(RecordSource* rsb);
	bool getEquiJoinKeys(NestConst<ValueExprNode>& node1,
						 NestConst<ValueExprNode>& node2,
						 bool needCast);
	BoolExprNode* makeInferenceNode(BoolExprNode* boolean,
									ValueExprNode* arg1,
									ValueExprNode* arg2);
	BoolExprNode* makeInferenceNode(BoolExprNode* boolean,
									ValueExprNode* arg,
									ValueListNode* list);

	ValueExprNode* optimizeLikeSimilar(ComparativeBoolNode* cmpNode);

	thread_db* const tdbb;
	CompilerScratch* const csb;
	RseNode* const rse;

	bool firstRows = false;					// optimize for first rows

	FILE* debugFile = nullptr;
	unsigned baseConjuncts = 0;				// number of conjuncts in our rse, next conjuncts are distributed parent
	unsigned baseParentConjuncts = 0;		// number of conjuncts in our rse + distributed with parent, next are parent
	unsigned baseMissingConjuncts = 0;		// number of conjuncts in our and parent rse, but without missing

	StreamList compileStreams, bedStreams, keyStreams, outerStreams;
	ConjunctList conjuncts;
};


//
// IndexScratch
//

enum segmentScanType {
	segmentScanNone,
	segmentScanGreater,
	segmentScanLess,
	segmentScanBetween,
	segmentScanEqual,
	segmentScanEquivalent,
	segmentScanMissing,
	segmentScanStarting,
	segmentScanList
};

typedef Firebird::HalfStaticArray<BoolExprNode*, OPT_STATIC_ITEMS> MatchedBooleanList;

struct IndexScratchSegment
{
	explicit IndexScratchSegment(MemoryPool& p)
		: matches(p)
	{}

	explicit IndexScratchSegment(MemoryPool& p, const IndexScratchSegment& other)
		: lowerValue(other.lowerValue),
		  upperValue(other.upperValue),
		  valueList(other.valueList),
		  excludeLower(other.excludeLower),
		  excludeUpper(other.excludeUpper),
		  scope(other.scope),
		  scanType(other.scanType),
		  matches(p, other.matches)
	{}

	ValueExprNode* lowerValue = nullptr;		// lower bound on index value
	ValueExprNode* upperValue = nullptr;		// upper bound on index value
	LookupValueList* valueList = nullptr;		// values to match
	bool excludeLower = false;					// exclude lower bound value from scan
	bool excludeUpper = false;					// exclude upper bound value from scan
	unsigned scope = 0;							// highest scope level
	segmentScanType scanType = segmentScanNone;	// scan type
	SSHORT scale = 0;							// scale for SINT64/Int128-based segment of index

	MatchedBooleanList matches;					// matched booleans
};

struct IndexScratch
{
	IndexScratch(MemoryPool& p, index_desc* idx);
	IndexScratch(MemoryPool& p, const IndexScratch& other);

	index_desc* index = nullptr;				// index descriptor
	double cardinality = 0;						// estimated cardinality of the whole index
	double selectivity = MAXIMUM_SELECTIVITY;	// calculated selectivity for this index
	bool candidate = false;						// used when deciding which indices to use
	bool scopeCandidate = false;				// used when making inversion based on scope
	unsigned lowerCount = 0;
	unsigned upperCount = 0;
	unsigned nonFullMatchedSegments = 0;
	bool usePartialKey = false;					// Use INTL_KEY_PARTIAL
	bool useMultiStartingKeys = false;			// Use INTL_KEY_MULTI_STARTING
	bool useRootListScan = false;

	Firebird::ObjectsArray<IndexScratchSegment> segments;
	MatchedBooleanList matches;					// matched booleans (partial indices only)
};

typedef Firebird::ObjectsArray<IndexScratch> IndexScratchList;

//
// InversionCandidate
//

struct InversionCandidate
{
	explicit InversionCandidate(MemoryPool& p)
		: matches(p), dbkeyRanges(p), dependentFromStreams(p)
	{}

	double selectivity = MAXIMUM_SELECTIVITY;
	double cost = 0;
	unsigned nonFullMatchedSegments = MAX_INDEX_SEGMENTS + 1;
	unsigned matchedSegments = 0;
	unsigned indexes = 0;
	unsigned dependencies = 0;
	BoolExprNode* boolean = nullptr;
	BoolExprNode* condition = nullptr;
	InversionNode* inversion = nullptr;
	IndexScratch* scratch = nullptr;
	bool used = false;
	bool unique = false;
	bool navigated = false;

	MatchedBooleanList matches;
	Firebird::Array<DbKeyRangeNode*> dbkeyRanges;
	SortedStreamList dependentFromStreams;
};

typedef Firebird::HalfStaticArray<InversionCandidate*, OPT_STATIC_ITEMS> InversionCandidateList;


//
// Retrieval
//

class Retrieval : private Firebird::PermanentStorage
{
public:
	Retrieval(thread_db* tdbb, Optimizer* opt, StreamType streamNumber,
			  bool outer, bool inner, SortNode* sortNode, bool costOnly);

	~Retrieval()
	{
		for (auto candidate : inversionCandidates)
			delete candidate;
	}

	InversionCandidate* getInversion();
	IndexTableScan* getNavigation();

protected:
	void analyzeNavigation(const InversionCandidateList& inversions);
	bool betterInversion(const InversionCandidate* inv1, const InversionCandidate* inv2,
						 bool navigation) const;
	bool checkIndexCondition(index_desc& idx, MatchedBooleanList& matches) const;
	bool checkIndexExpression(const index_desc* idx, ValueExprNode* node) const;
	InversionNode* composeInversion(InversionNode* node1, InversionNode* node2,
		InversionNode::Type node_type) const;
	const Firebird::string& getAlias();
	void getInversionCandidates(InversionCandidateList& inversions,
		IndexScratchList& indexScratches, unsigned scope) const;
	InversionNode* makeIndexScanNode(IndexScratch* indexScratch) const;
	InversionCandidate* makeInversion(InversionCandidateList& inversions) const;
	bool matchBoolean(IndexScratch* indexScratch, BoolExprNode* boolean, unsigned scope) const;
	InversionCandidate* matchDbKey(BoolExprNode* boolean) const;
	InversionCandidate* matchOnIndexes(IndexScratchList& indexScratches,
		BoolExprNode* boolean, unsigned scope) const;
	ValueExprNode* findDbKey(ValueExprNode* dbkey, SLONG* position) const;
	bool validateStarts(IndexScratch* indexScratch, ComparativeBoolNode* cmpNode,
		unsigned segment) const;

#ifdef OPT_DEBUG_RETRIEVAL
	void printCandidate(const InversionCandidate* candidate) const;
	void printCandidates(const InversionCandidateList& inversions) const;
	void printFinalCandidate(const InversionCandidate* candidate) const;
#endif

private:
	thread_db* const tdbb;
	Optimizer* const optimizer;
	CompilerScratch* const csb;
	const StreamType stream;
	const bool innerFlag;
	const bool outerFlag;
	SortNode* const sort;
	jrd_rel* relation;
	const bool createIndexScanNodes;
	const bool setConjunctionsMatched;
	Firebird::string alias;
	IndexScratchList indexScratches;
	InversionCandidateList inversionCandidates;
	Firebird::AutoPtr<InversionCandidate> finalCandidate;
	Firebird::AutoPtr<InversionCandidate> navigationCandidate;
};


//
// InnerJoin
//

class InnerJoin : private Firebird::PermanentStorage
{
	struct IndexRelationship
	{
		static const unsigned MAX_DEP_STREAMS = 8;

		static bool cheaperThan(const IndexRelationship& item1, const IndexRelationship& item2)
		{
			if (item1.cost == 0)
				return true;

			if (item2.cost == 0)
				return false;

			const double compare = item1.cost / item2.cost;
			if (compare >= 0.98 && compare <= 1.02)
			{
				// cost is nearly the same, now check uniqueness and cardinality

				if (item1.unique == item2.unique)
				{
					if (item1.cardinality < item2.cardinality)
						return true;
				}
				else if (item1.unique)
					return true;
				else if (item2.unique)
					return false;
			}
			else if (item1.cost < item2.cost)
				return true;

			return false;
		}

		// Needed for SortedArray
		bool operator>(const IndexRelationship& other) const
		{
			return !cheaperThan(*this, other);
		}

		StreamType stream = 0;
		bool unique = false;
		double cost = 0;
		double cardinality = 0;
		Firebird::Vector<StreamType, MAX_DEP_STREAMS> depStreams;
	};

	typedef Firebird::SortedArray<IndexRelationship> IndexedRelationships;

	class StreamInfo
	{
	public:
		StreamInfo(MemoryPool& p, StreamType num)
			: number(num), baseMatches(p), baseDependentFromStreams(p),
			  indexedRelationships(p)
		{}

		bool isIndependent() const
		{
			// Return true if this stream can't be used by other streams
			// and it can't use index retrieval based on other streams

			return (indexedRelationships.isEmpty() && !previousExpectedStreams);
		}

		bool isFiltered() const
		{
			return (baseIndexes || baseSelectivity < MAXIMUM_SELECTIVITY);
		}

		static bool cheaperThan(const StreamInfo* item1, const StreamInfo* item2)
		{
			// First those streams which cannot be used by other streams
			// or cannot depend on a stream
			if (item1->isIndependent() && !item2->isIndependent())
				return true;

			// Next those with the lowest previous expected streams
			const int compare = item1->previousExpectedStreams -
				item2->previousExpectedStreams;

			if (compare < 0)
				return true;

			// Next those with the cheapest base cost
			if (item1->baseCost < item2->baseCost)
				return true;

			return false;
		}

		const StreamType number;

		bool baseUnique = false;
		double baseCost = 0;
		double baseSelectivity = 0;
		unsigned baseIndexes = 0;
		bool baseNavigated = false;
		bool used = false;
		unsigned previousExpectedStreams = 0;

		MatchedBooleanList baseMatches;
		SortedStreamList baseDependentFromStreams;

		IndexedRelationships indexedRelationships;
	};

	typedef Firebird::HalfStaticArray<StreamInfo*, OPT_STATIC_ITEMS> StreamInfoList;

	struct JoinedStreamInfo
	{
		static const unsigned MAX_EQUI_MATCHES = 8;

		void reset (StreamType num)
		{
			number = num;
			selectivity = 0.0;
			equiMatches.clear();
		}

		StreamType number;			// stream in position of join order
		double selectivity = 0.0;	// position selectivity
		Firebird::Vector<BoolExprNode*, MAX_EQUI_MATCHES> equiMatches;
	};

	typedef Firebird::HalfStaticArray<JoinedStreamInfo, OPT_STATIC_ITEMS> JoinedStreamList;

public:
	InnerJoin(thread_db* tdbb, Optimizer* opt,
			  const StreamList& streams,
			  SortNode** sortClause, bool hasPlan);

	~InnerJoin()
	{
		for (auto innerStream : innerStreams)
			delete innerStream;
	}

	bool findJoinOrder();
	River* formRiver();

protected:
	void calculateStreamInfo();
	void estimateCost(unsigned position, const StreamInfo* stream, double& cost, double& cardinality);
	void findBestOrder(unsigned position, StreamInfo* stream,
		IndexedRelationships& processList, double cost, double cardinality);
	void getIndexedRelationships(StreamInfo* testStream);
	StreamInfo* getStreamInfo(StreamType stream);
#ifdef OPT_DEBUG
	void printBestOrder() const;
	void printFoundOrder(StreamType position, double positionCost,
		double positionCardinality, double cost, double cardinality) const;
	void printProcessList(const IndexedRelationships& processList, StreamType stream) const;
	void printStartOrder() const;
#endif

private:
	thread_db* const tdbb;
	Optimizer* const optimizer;
	CompilerScratch* const csb;
	SortNode** sortPtr;
	const bool plan;

	unsigned remainingStreams = 0;
	unsigned bestCount = 0;	// longest length of indexable streams
	double bestCost = 0;	// cost of best join order

	StreamInfoList innerStreams;
	JoinedStreamList joinedStreams;
	JoinedStreamList bestStreams;
};

} // namespace Jrd

#endif // OPTIMIZER_H
