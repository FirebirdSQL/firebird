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
 * Adriano dos Santos Fernandes - refactored from pass1.cpp, gen.cpp, cmp.cpp, par.cpp and evl.cpp
 */

#include "firebird.h"
#include "../common/classes/VaryStr.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/align.h"
#include "firebird/impl/blr.h"
#include "../jrd/tra.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/recsrc/Cursor.h"
#include "../jrd/Optimizer.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/Collation.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/pass1_proto.h"

using namespace Firebird;
using namespace Jrd;

namespace Jrd {


// Maximum members in "IN" list. For eg. SELECT * FROM T WHERE F IN (1, 2, 3, ...)
// Bug 10061, bsriram - 19-Apr-1999
static const int MAX_MEMBER_LIST = 1500;


//--------------------


BoolExprNode* BoolExprNode::pass2(thread_db* tdbb, CompilerScratch* csb)
{
	pass2Boolean1(tdbb, csb);
	ExprNode::pass2(tdbb, csb);
	pass2Boolean2(tdbb, csb);

	if (nodFlags & FLAG_INVARIANT)
	{
		// Bind values of invariant nodes to top-level RSE (if present)

		if (csb->csb_current_nodes.hasData())
		{
			RseNode* topRseNode = nodeAs<RseNode>(csb->csb_current_nodes[0]);
			fb_assert(topRseNode);

			if (!topRseNode->rse_invariants)
			{
				topRseNode->rse_invariants =
					FB_NEW_POOL(*tdbb->getDefaultPool()) VarInvariantArray(*tdbb->getDefaultPool());
			}

			topRseNode->rse_invariants->add(impureOffset);
		}
	}

	return this;
}


//--------------------


static RegisterBoolNode<BinaryBoolNode> regBinaryBoolNode({blr_and, blr_or});

BinaryBoolNode::BinaryBoolNode(MemoryPool& pool, UCHAR aBlrOp, BoolExprNode* aArg1,
			BoolExprNode* aArg2)
	: TypedNode<BoolExprNode, ExprNode::TYPE_BINARY_BOOL>(pool),
	  blrOp(aBlrOp),
	  arg1(aArg1),
	  arg2(aArg2)
{
}

DmlNode* BinaryBoolNode::parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp)
{
	BinaryBoolNode* node = FB_NEW_POOL(pool) BinaryBoolNode(pool, blrOp);
	node->arg1 = PAR_parse_boolean(tdbb, csb);
	node->arg2 = PAR_parse_boolean(tdbb, csb);
	return node;
}

string BinaryBoolNode::internalPrint(NodePrinter& printer) const
{
	BoolExprNode::internalPrint(printer);

	NODE_PRINT(printer, blrOp);
	NODE_PRINT(printer, arg1);
	NODE_PRINT(printer, arg2);

	return "BinaryBoolNode";
}

BoolExprNode* BinaryBoolNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	return FB_NEW_POOL(dsqlScratch->getPool()) BinaryBoolNode(dsqlScratch->getPool(), blrOp,
		doDsqlPass(dsqlScratch, arg1), doDsqlPass(dsqlScratch, arg2));
}

void BinaryBoolNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->appendUChar(blrOp);
	GEN_expr(dsqlScratch, arg1);
	GEN_expr(dsqlScratch, arg2);
}

bool BinaryBoolNode::dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const
{
	if (!BoolExprNode::dsqlMatch(dsqlScratch, other, ignoreMapCast))
		return false;

	const BinaryBoolNode* o = nodeAs<BinaryBoolNode>(other);
	fb_assert(o);

	return blrOp == o->blrOp;
}

bool BinaryBoolNode::sameAs(CompilerScratch* csb, const ExprNode* other, bool ignoreStreams) const
{
	const BinaryBoolNode* const otherNode = nodeAs<BinaryBoolNode>(other);

	if (!otherNode || blrOp != otherNode->blrOp)
		return false;

	if (arg1->sameAs(csb, otherNode->arg1, ignoreStreams) &&
		arg2->sameAs(csb, otherNode->arg2, ignoreStreams))
	{
		return true;
	}

	// A AND B is equivalent to B AND A, ditto for A OR B and B OR A.
	return arg1->sameAs(csb, otherNode->arg2, ignoreStreams) &&
		arg2->sameAs(csb, otherNode->arg1, ignoreStreams);
}

BoolExprNode* BinaryBoolNode::copy(thread_db* tdbb, NodeCopier& copier) const
{
	BinaryBoolNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) BinaryBoolNode(*tdbb->getDefaultPool(),
		blrOp);
	node->nodFlags = nodFlags;
	node->arg1 = copier.copy(tdbb, arg1);
	node->arg2 = copier.copy(tdbb, arg2);
	return node;
}

TriState BinaryBoolNode::execute(thread_db* tdbb, jrd_req* request) const
{
	switch (blrOp)
	{
		case blr_and:
			return executeAnd(tdbb, request);

		case blr_or:
			return executeOr(tdbb, request);
	}

	fb_assert(false);
	return TriState(false);
}

TriState BinaryBoolNode::executeAnd(thread_db* tdbb, jrd_req* request) const
{
	// If either operand is false, then the result is false;
	// If both are true, the result is true;
	// Otherwise, the result is NULL.
	//
	// op 1            op 2            result
	// ----            ----            ------
	// F               F                F
	// F               T                F
	// F               N                F
	// T               F                F
	// T               T                T
	// T               N                N
	// N               F                F
	// N               T                N
	// N               N                N

	const TriState value1 = arg1->execute(tdbb, request);

	if (value1 == false)
	{
		// First term is false, why the whole expression is false.
		return TriState(false);
	}

	const TriState value2 = arg2->execute(tdbb, request);

	if (value2 == false)
		return TriState(false);	// at least one operand was false

	if (value1 == true && value2 == true)
		return TriState(true);	// both true

	return TriState();	// otherwise, return null
}

TriState BinaryBoolNode::executeOr(thread_db* tdbb, jrd_req* request) const
{
	// If either operand is true, then the result is true;
	// If both are false, the result is false;
	// Otherwise, the result is NULL.
	//
	// op 1            op 2            result
	// ----            ----            ------
	// F               F                F
	// F               T                T
	// F               N                N
	// T               F                T
	// T               T                T
	// T               N                T
	// N               F                N
	// N               T                T
	// N               N                N

	const TriState value1 = arg1->execute(tdbb, request);

	if (value1 == true)
	{
		// First term is true, why the whole expression is true.
		return TriState(true);
	}

	const TriState value2 = arg2->execute(tdbb, request);

	return value2 == true ? TriState(true) :
		(value1.isUnknown() || value2.isUnknown()) ? TriState() :
		TriState(false);
}


//--------------------


static RegisterBoolNode<ComparativeBoolNode> regComparativeBoolNode({
	blr_eql,
	blr_geq,
	blr_gtr,
	blr_leq,
	blr_lss,
	blr_neq,
	blr_equiv,
	blr_between,
	blr_like,
	blr_ansi_like,
	blr_containing,
	blr_starting,
	blr_similar,
	blr_matching,
	blr_matching2
});

ComparativeBoolNode::ComparativeBoolNode(MemoryPool& pool, UCHAR aBlrOp,
			ValueExprNode* aArg1, ValueExprNode* aArg2, ValueExprNode* aArg3)
	: TypedNode<BoolExprNode, ExprNode::TYPE_COMPARATIVE_BOOL>(pool),
	  blrOp(aBlrOp),
	  dsqlCheckBoolean(false),
	  dsqlFlag(DFLAG_NONE),
	  arg1(aArg1),
	  arg2(aArg2),
	  arg3(aArg3),
	  dsqlSpecialArg(NULL)
{
}

DmlNode* ComparativeBoolNode::parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp)
{
	ComparativeBoolNode* node = FB_NEW_POOL(pool) ComparativeBoolNode(pool, blrOp);

	node->arg1 = PAR_parse_value(tdbb, csb);
	node->arg2 = PAR_parse_value(tdbb, csb);

	if (blrOp == blr_between || blrOp == blr_ansi_like || blrOp == blr_matching2)
	{
		if (blrOp == blr_ansi_like)
			node->blrOp = blr_like;

		node->arg3 = PAR_parse_value(tdbb, csb);
	}
	else if (blrOp == blr_similar)
	{
		if (csb->csb_blr_reader.getByte() != 0)
			node->arg3 = PAR_parse_value(tdbb, csb);	// escape
	}

	return node;
}

string ComparativeBoolNode::internalPrint(NodePrinter& printer) const
{
	BoolExprNode::internalPrint(printer);

	NODE_PRINT(printer, blrOp);
	NODE_PRINT(printer, dsqlFlag);
	NODE_PRINT(printer, arg1);
	NODE_PRINT(printer, arg2);
	NODE_PRINT(printer, arg3);
	NODE_PRINT(printer, dsqlSpecialArg);

	return "ComparativeBoolNode";
}

BoolExprNode* ComparativeBoolNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	NestConst<ValueExprNode> procArg1 = arg1;
	NestConst<ValueExprNode> procArg2 = arg2;
	NestConst<ValueExprNode> procArg3 = arg3;

	if (dsqlSpecialArg)
	{
		ValueListNode* listNode = nodeAs<ValueListNode>(dsqlSpecialArg);
		if (listNode)
		{
			int listItemCount = 0;
			BoolExprNode* resultNode = NULL;
			NestConst<ValueExprNode>* ptr = listNode->items.begin();

			for (const NestConst<ValueExprNode>* const end = listNode->items.end();
				 ptr != end;
				 ++listItemCount, ++ptr)
			{
				if (listItemCount >= MAX_MEMBER_LIST)
				{
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
							  Arg::Gds(isc_imp_exc) <<
							  Arg::Gds(isc_dsql_too_many_values) << Arg::Num(MAX_MEMBER_LIST));
				}

				ComparativeBoolNode* temp = FB_NEW_POOL(dsqlScratch->getPool()) ComparativeBoolNode(
					dsqlScratch->getPool(), blrOp, procArg1, *ptr);
				resultNode = PASS1_compose(resultNode, temp, blr_or);
			}

			return resultNode->dsqlPass(dsqlScratch);
		}

		SelectExprNode* selNode = nodeAs<SelectExprNode>(dsqlSpecialArg);
		if (selNode)
		{
			fb_assert(!(selNode->dsqlFlags & RecordSourceNode::DFLAG_SINGLETON));
			UCHAR newBlrOp = blr_any;

			if (dsqlFlag == DFLAG_ANSI_ANY)
				newBlrOp = blr_ansi_any;
			else if (dsqlFlag == DFLAG_ANSI_ALL)
				newBlrOp = blr_ansi_all;

			return createRseNode(dsqlScratch, newBlrOp);
		}

		fb_assert(false);
	}

	procArg2 = doDsqlPass(dsqlScratch, procArg2);

	ComparativeBoolNode* node = FB_NEW_POOL(dsqlScratch->getPool()) ComparativeBoolNode(dsqlScratch->getPool(), blrOp,
		doDsqlPass(dsqlScratch, procArg1),
		procArg2,
		doDsqlPass(dsqlScratch, procArg3));

	if (dsqlCheckBoolean)
	{
		dsc desc;
		DsqlDescMaker::fromNode(dsqlScratch, &desc, node->arg1);

		if (desc.dsc_dtype != dtype_boolean && desc.dsc_dtype != dtype_unknown && !desc.isNull())
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				Arg::Gds(isc_invalid_boolean_usage));
		}
	}

	switch (blrOp)
	{
		case blr_eql:
		case blr_neq:
		case blr_gtr:
		case blr_geq:
		case blr_lss:
		case blr_leq:
		case blr_equiv:
		case blr_between:
		{
			// Try to force arg1 to be same type as arg2 eg: ? = FIELD case
			PASS1_set_parameter_type(dsqlScratch, node->arg1, procArg2, false);

			// Try to force arg2 to be same type as arg1 eg: FIELD = ? case
			// Try even when the above call succeeded, because "arg2" may
			// have arg-expressions that should be resolved.
			PASS1_set_parameter_type(dsqlScratch, procArg2, node->arg1, false);

			// X BETWEEN Y AND ? case
			if (!PASS1_set_parameter_type(dsqlScratch, node->arg3, node->arg1, false))
			{
				// ? BETWEEN Y AND ? case
				PASS1_set_parameter_type(dsqlScratch, node->arg3, procArg2, false);
			}

			break;
		}

		case blr_containing:
		case blr_like:
		case blr_similar:
		case blr_starting:
			// Try to force arg1 to be same type as arg2 eg: ? LIKE FIELD case
			PASS1_set_parameter_type(dsqlScratch, node->arg1, procArg2, true);

			// Try to force arg2 same type as arg 1 eg: FIELD LIKE ? case
			// Try even when the above call succeeded, because "arg2" may
			// have arg-expressions that should be resolved.
			PASS1_set_parameter_type(dsqlScratch, procArg2, node->arg1, true);

			// X LIKE Y ESCAPE ? case
			PASS1_set_parameter_type(dsqlScratch, node->arg3, procArg2, true);
	}

	return node;
}

void ComparativeBoolNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->appendUChar(blrOp == blr_like && arg3 ? blr_ansi_like : blrOp);

	GEN_expr(dsqlScratch, arg1);
	GEN_expr(dsqlScratch, arg2);

	if (blrOp == blr_similar)
		dsqlScratch->appendUChar(arg3 ? 1 : 0);

	if (arg3)
		GEN_expr(dsqlScratch, arg3);
}

bool ComparativeBoolNode::dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const
{
	if (!BoolExprNode::dsqlMatch(dsqlScratch, other, ignoreMapCast))
		return false;

	const ComparativeBoolNode* o = nodeAs<ComparativeBoolNode>(other);
	fb_assert(o);

	return dsqlFlag == o->dsqlFlag && blrOp == o->blrOp;
}

bool ComparativeBoolNode::sameAs(CompilerScratch* csb, const ExprNode* other, bool ignoreStreams) const
{
	const ComparativeBoolNode* const otherNode = nodeAs<ComparativeBoolNode>(other);

	if (!otherNode || blrOp != otherNode->blrOp)
		return false;

	bool matching = arg1->sameAs(csb, otherNode->arg1, ignoreStreams) &&
		arg2->sameAs(csb, otherNode->arg2, ignoreStreams);

	if (matching)
	{
		matching = (!arg3 == !otherNode->arg3) &&
			(!arg3 || arg3->sameAs(csb, otherNode->arg3, ignoreStreams));

		if (matching)
			return true;
	}

	// TODO match A > B to B <= A, etc

	if (blrOp == blr_eql || blrOp == blr_equiv || blrOp == blr_neq)
	{
		// A = B is equivalent to B = A, etc.
		if (arg1->sameAs(csb, otherNode->arg2, ignoreStreams) &&
			arg2->sameAs(csb, otherNode->arg1, ignoreStreams))
		{
			return true;
		}
	}

	return false;
}

BoolExprNode* ComparativeBoolNode::copy(thread_db* tdbb, NodeCopier& copier) const
{
	ComparativeBoolNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) ComparativeBoolNode(
		*tdbb->getDefaultPool(), blrOp);
	node->nodFlags = nodFlags;
	node->arg1 = copier.copy(tdbb, arg1);
	node->arg2 = copier.copy(tdbb, arg2);

	if (arg3)
		node->arg3 = copier.copy(tdbb, arg3);

	return node;
}

BoolExprNode* ComparativeBoolNode::pass1(thread_db* tdbb, CompilerScratch* csb)
{
	bool invariantCheck = false;

	switch (blrOp)
	{
		case blr_like:
		case blr_similar:
		case blr_containing:
		case blr_starting:
			invariantCheck = true;
			break;
	}

	doPass1(tdbb, csb, arg1.getAddress());

	if (invariantCheck)
	{
		// We need to take care of invariantness expressions to be able to pre-compile the pattern.
		nodFlags |= FLAG_INVARIANT;
		csb->csb_current_nodes.push(this);
	}

	doPass1(tdbb, csb, arg2.getAddress());
	doPass1(tdbb, csb, arg3.getAddress());

	if (invariantCheck)
	{
		csb->csb_current_nodes.pop();

		// If there is no top-level RSE present and patterns are not constant, unmark node as invariant
		// because it may be dependent on data or variables.
		if ((nodFlags & FLAG_INVARIANT) &&
			(!nodeIs<LiteralNode>(arg2) || (arg3 && !nodeIs<LiteralNode>(arg3))))
		{
			ExprNode* const* ctx_node;
			ExprNode* const* end;

			for (ctx_node = csb->csb_current_nodes.begin(), end = csb->csb_current_nodes.end();
				 ctx_node != end; ++ctx_node)
			{
				if (nodeAs<RseNode>(*ctx_node))
					break;
			}

			if (ctx_node >= end)
				nodFlags &= ~FLAG_INVARIANT;
		}
	}

	return this;
}

void ComparativeBoolNode::pass2Boolean1(thread_db* /*tdbb*/, CompilerScratch* csb)
{
	if (nodFlags & FLAG_INVARIANT)
		csb->csb_invariants.push(&impureOffset);
}

void ComparativeBoolNode::pass2Boolean2(thread_db* tdbb, CompilerScratch* csb)
{
	RecordKeyNode* keyNode;

	if (arg3)
	{
		if ((keyNode = nodeAs<RecordKeyNode>(arg3)) && keyNode->aggregate)
			ERR_post(Arg::Gds(isc_bad_dbkey));

		dsc descriptor_c;
		arg1->getDesc(tdbb, csb, &descriptor_c);

		if (DTYPE_IS_DATE(descriptor_c.dsc_dtype))
		{
			arg1->nodFlags |= FLAG_DATE;
			arg2->nodFlags |= FLAG_DATE;
		}
	}

	if (((keyNode = nodeAs<RecordKeyNode>(arg1)) && keyNode->aggregate) ||
		((keyNode = nodeAs<RecordKeyNode>(arg2)) && keyNode->aggregate))
	{
		ERR_post(Arg::Gds(isc_bad_dbkey));
	}

	dsc descriptor_a, descriptor_b;
	arg1->getDesc(tdbb, csb, &descriptor_a);
	arg2->getDesc(tdbb, csb, &descriptor_b);

	if (DTYPE_IS_DATE(descriptor_a.dsc_dtype))
		arg2->nodFlags |= FLAG_DATE;
	else if (DTYPE_IS_DATE(descriptor_b.dsc_dtype))
		arg1->nodFlags |= FLAG_DATE;

	if (nodFlags & FLAG_INVARIANT)
	{
		// This may currently happen for nod_like, nod_contains and nod_similar
		impureOffset = csb->allocImpure<impure_value>();
	}
}

TriState ComparativeBoolNode::execute(thread_db* tdbb, jrd_req* request) const
{
	dsc* desc[2] = {NULL, NULL};
	bool computed_invariant = false;

	request->req_flags &= ~req_same_tx_upd;

	// Evaluate arguments.  If either is null, result is null, but in
	// any case, evaluate both, since some expressions may later depend
	// on mappings which are developed here

	desc[0] = EVL_expr(tdbb, request, arg1);

	const ULONG flags = request->req_flags;
	bool force_equal = (request->req_flags & req_same_tx_upd) != 0;

	// Currently only blr_like, blr_containing, blr_starting and blr_similar may be marked invariant
	if (nodFlags & FLAG_INVARIANT)
	{
		impure_value* impure = request->getImpure<impure_value>(impureOffset);

		// Check that data type of operand is still the same.
		// It may change due to multiple formats present in stream
		// System tables are the good example of such streams -
		// data coming from ini.epp has ASCII ttype, user data is UNICODE_FSS
		//
		// Note that value descriptor may be NULL pointer if value is SQL NULL
		if ((impure->vlu_flags & VLU_computed) && desc[0] &&
			(impure->vlu_desc.dsc_dtype != desc[0]->dsc_dtype ||
			 impure->vlu_desc.dsc_sub_type != desc[0]->dsc_sub_type ||
			 impure->vlu_desc.dsc_scale != desc[0]->dsc_scale))
		{
			impure->vlu_flags &= ~VLU_computed;
		}

		if (impure->vlu_flags & VLU_computed)
		{
			if (!(impure->vlu_flags & VLU_null))
				computed_invariant = true;
		}
		else
		{
			desc[1] = EVL_expr(tdbb, request, arg2);

			if (!desc[1])
				impure->vlu_flags |= VLU_computed | VLU_null;
			else
			{
				impure->vlu_flags &= ~VLU_null;

				// Search object depends on operand data type.
				// Thus save data type which we use to compute invariant
				if (desc[0])
				{
					impure->vlu_desc.dsc_dtype = desc[0]->dsc_dtype;
					impure->vlu_desc.dsc_sub_type = desc[0]->dsc_sub_type;
					impure->vlu_desc.dsc_scale = desc[0]->dsc_scale;
				}
				else
				{
					// Indicate we do not know type of expression.
					// This code will force pattern recompile for the next non-null value
					impure->vlu_desc.dsc_dtype = 0;
					impure->vlu_desc.dsc_sub_type = 0;
					impure->vlu_desc.dsc_scale = 0;
				}
			}
		}
	}
	else
		desc[1] = EVL_expr(tdbb, request, arg2);

	// An equivalence operator evaluates to true when both operands
	// are NULL and behaves like an equality operator otherwise.
	// Note that this operator never returns a SQL null.

	if (blrOp == blr_equiv)
	{
		if (!desc[0] && !desc[1])
			return TriState(true);

		if (!desc[0] || !desc[1])
			return TriState(false);
	}

	// If either of expressions above returned NULL, return NULL.

	if (!desc[0] || (!computed_invariant && !desc[1]))
		return TriState();

	force_equal |= (request->req_flags & req_same_tx_upd) != 0;
	int comparison; // while the two switch() below are in sync, no need to initialize

	switch (blrOp)
	{
		case blr_eql:
		case blr_equiv:
		case blr_gtr:
		case blr_geq:
		case blr_lss:
		case blr_leq:
		case blr_neq:
		case blr_between:
			comparison = MOV_compare(tdbb, desc[0], desc[1]);
	}

	// If we are checking equality of record_version
	// and same transaction updated the record, force equality.

	const RecordKeyNode* recVersionNode = nodeAs<RecordKeyNode>(arg1);

	if (recVersionNode && recVersionNode->blrOp == blr_record_version && force_equal)
		comparison = 0;

	request->req_flags &= ~(req_same_tx_upd);

	switch (blrOp)
	{
		case blr_eql:
		case blr_equiv:
			return TriState(comparison == 0);

		case blr_gtr:
			return TriState(comparison > 0);

		case blr_geq:
			return TriState(comparison >= 0);

		case blr_lss:
			return TriState(comparison < 0);

		case blr_leq:
			return TriState(comparison <= 0);

		case blr_neq:
			return TriState(comparison != 0);

		case blr_between:
			desc[1] = EVL_expr(tdbb, request, arg3);
			if (!desc[1])
				return TriState();
			return TriState(comparison >= 0 && MOV_compare(tdbb, desc[0], desc[1]) <= 0);

		case blr_containing:
		case blr_starting:
		case blr_matching:
		case blr_like:
		case blr_similar:
			return stringBoolean(tdbb, request, desc[0], desc[1], computed_invariant);

		case blr_matching2:
			return sleuth(tdbb, request, desc[0], desc[1]);
	}

	return TriState(false);
}

// Perform one of the complex string functions CONTAINING, MATCHES, or STARTS WITH.
TriState ComparativeBoolNode::stringBoolean(thread_db* tdbb, jrd_req* request, dsc* desc1,
	dsc* desc2, bool computed_invariant) const
{
	UCHAR* p1 = NULL;
	UCHAR* p2 = NULL;
	SLONG l2 = 0;
	USHORT type1;
	MoveBuffer match_str;

	SET_TDBB(tdbb);

	if (!desc1->isBlob())
	{
		// Source is not a blob, do a simple search

		// Get text type of data string

		type1 = INTL_TEXT_TYPE(*desc1);

		// Get address and length of search string - convert to datatype of data

		if (!computed_invariant)
			l2 = MOV_make_string2(tdbb, desc2, type1, &p2, match_str, false);

		VaryStr<256> temp1;
		USHORT xtype1;
		const USHORT l1 = MOV_get_string_ptr(tdbb, desc1, &xtype1, &p1, &temp1, sizeof(temp1));

		fb_assert(xtype1 == type1);

		return stringFunction(tdbb, request, l1, p1, l2, p2, type1, computed_invariant);
	}

	// Source string is a blob, things get interesting

	HalfStaticArray<UCHAR, BUFFER_SMALL> buffer;

	if (desc1->dsc_sub_type == isc_blob_text)
		type1 = desc1->dsc_blob_ttype();	// pick up character set and collation of blob
	else
		type1 = ttype_none;	// Do byte matching

	Collation* obj = INTL_texttype_lookup(tdbb, type1);
	CharSet* charset = obj->getCharSet();

	// Get address and length of search string - make it string if necessary
	// but don't transliterate character set if the source blob is binary
	if (!computed_invariant)
	{
		l2 = MOV_make_string2(tdbb, desc2, type1, &p2, match_str, false);
	}

	blb* blob =	blb::open(tdbb, request->req_transaction, reinterpret_cast<bid*>(desc1->dsc_address));

	if (charset->isMultiByte() &&
		(blrOp != blr_starting || !(obj->getFlags() & TEXTTYPE_DIRECT_MATCH)))
	{
		buffer.getBuffer(blob->blb_length);		// alloc space to put entire blob in memory
	}

	// Performs the string_function on each segment of the blob until
	// a positive result is obtained

	TriState ret_val;

	switch (blrOp)
	{
		case blr_like:
		case blr_similar:
		{
			VaryStr<TEMP_STR_LENGTH> temp3;
			const UCHAR* escape_str = NULL;
			USHORT escape_length = 0;

			// ensure 3rd argument (escape char) is in operation text type
			if (arg3 && !computed_invariant)
			{
				// Convert ESCAPE to operation character set
				dsc* desc = EVL_expr(tdbb, request, arg3);

				if (!desc)
				{
					if (nodFlags & FLAG_INVARIANT)
					{
						impure_value* impure = request->getImpure<impure_value>(impureOffset);
						impure->vlu_flags |= VLU_computed;
						impure->vlu_flags |= VLU_null;
					}
					break;
				}

				escape_length = MOV_make_string(tdbb, desc, type1,
					reinterpret_cast<const char**>(&escape_str), &temp3, sizeof(temp3));

				if (!escape_length || charset->length(escape_length, escape_str, true) != 1)
				{
					// If characters left, or null byte character, return error
					blob->BLB_close(tdbb);
					ERR_post(Arg::Gds(isc_escape_invalid));
				}

				USHORT escape[2] = {0, 0};

				charset->getConvToUnicode().convert(escape_length, escape_str, sizeof(escape), escape);
				if (!escape[0])
				{
					// If or null byte character, return error
					blob->BLB_close(tdbb);
					ERR_post(Arg::Gds(isc_escape_invalid));
				}
			}

			PatternMatcher* evaluator;

			if (nodFlags & FLAG_INVARIANT)
			{
				impure_value* impure = request->getImpure<impure_value>(impureOffset);

				if (!(impure->vlu_flags & VLU_computed))
				{
					delete impure->vlu_misc.vlu_invariant;
					impure->vlu_flags |= VLU_computed;

					if (blrOp == blr_like)
					{
						impure->vlu_misc.vlu_invariant = evaluator = obj->createLikeMatcher(
							*tdbb->getDefaultPool(), p2, l2, escape_str, escape_length);
					}
					else	// nod_similar
					{
						impure->vlu_misc.vlu_invariant = evaluator = obj->createSimilarToMatcher(
							tdbb, *tdbb->getDefaultPool(), p2, l2, escape_str, escape_length);
					}
				}
				else
				{
					evaluator = impure->vlu_misc.vlu_invariant;
					evaluator->reset();
				}
			}
			else if (blrOp == blr_like)
			{
				evaluator = obj->createLikeMatcher(*tdbb->getDefaultPool(),
					p2, l2, escape_str, escape_length);
			}
			else	// nod_similar
			{
				evaluator = obj->createSimilarToMatcher(tdbb, *tdbb->getDefaultPool(),
					p2, l2, escape_str, escape_length);
			}

			while (!(blob->blb_flags & BLB_eof))
			{
				const SLONG l1 = blob->BLB_get_data(tdbb, buffer.begin(), buffer.getCapacity(), false);
				if (!evaluator->process(buffer.begin(), l1))
					break;
			}

			ret_val = evaluator->result();

			if (!(nodFlags & FLAG_INVARIANT))
				delete evaluator;

			break;
		}

		case blr_containing:
		case blr_starting:
		{
			PatternMatcher* evaluator;

			if (nodFlags & FLAG_INVARIANT)
			{
				impure_value* impure = request->getImpure<impure_value>(impureOffset);
				if (!(impure->vlu_flags & VLU_computed))
				{
					delete impure->vlu_misc.vlu_invariant;

					if (blrOp == blr_containing)
					{
						impure->vlu_misc.vlu_invariant = evaluator =
							obj->createContainsMatcher(*tdbb->getDefaultPool(), p2, l2);
					}
					else	// nod_starts
					{
						impure->vlu_misc.vlu_invariant = evaluator =
							obj->createStartsMatcher(*tdbb->getDefaultPool(), p2, l2);
					}

					impure->vlu_flags |= VLU_computed;
				}
				else
				{
					evaluator = impure->vlu_misc.vlu_invariant;
					evaluator->reset();
				}
			}
			else
			{
				if (blrOp == blr_containing)
					evaluator = obj->createContainsMatcher(*tdbb->getDefaultPool(), p2, l2);
				else	// nod_starts
					evaluator = obj->createStartsMatcher(*tdbb->getDefaultPool(), p2, l2);
			}

			while (!(blob->blb_flags & BLB_eof))
			{
				const SLONG l1 = blob->BLB_get_data(tdbb, buffer.begin(), buffer.getCapacity(), false);
				if (!evaluator->process(buffer.begin(), l1))
					break;
			}

			ret_val = evaluator->result();

			if (!(nodFlags & FLAG_INVARIANT))
				delete evaluator;

			break;
		}
	}

	blob->BLB_close(tdbb);

	return ret_val;
}

// Perform one of the pattern matching string functions.
TriState ComparativeBoolNode::stringFunction(thread_db* tdbb, jrd_req* request,
	SLONG l1, const UCHAR* p1, SLONG l2, const UCHAR* p2, USHORT ttype,
	bool computed_invariant) const
{
	SET_TDBB(tdbb);

	Collation* obj = INTL_texttype_lookup(tdbb, ttype);
	CharSet* charset = obj->getCharSet();

	// Handle contains and starts
	if (blrOp == blr_containing || blrOp == blr_starting)
	{
		if (nodFlags & FLAG_INVARIANT)
		{
			impure_value* impure = request->getImpure<impure_value>(impureOffset);
			PatternMatcher* evaluator;
			if (!(impure->vlu_flags & VLU_computed))
			{
				delete impure->vlu_misc.vlu_invariant;

				if (blrOp == blr_containing)
				{
					impure->vlu_misc.vlu_invariant = evaluator =
						obj->createContainsMatcher(*tdbb->getDefaultPool(), p2, l2);
				}
				else
				{
					// nod_starts
					impure->vlu_misc.vlu_invariant = evaluator =
						obj->createStartsMatcher(*tdbb->getDefaultPool(), p2, l2);
				}

				impure->vlu_flags |= VLU_computed;
			}
			else
			{
				evaluator = impure->vlu_misc.vlu_invariant;
				evaluator->reset();
			}

			evaluator->process(p1, l1);
			return TriState(evaluator->result());
		}

		if (blrOp == blr_containing)
			return TriState(obj->contains(*tdbb->getDefaultPool(), p1, l1, p2, l2));

		// nod_starts
		return TriState(obj->starts(*tdbb->getDefaultPool(), p1, l1, p2, l2));
	}

	// Handle LIKE and SIMILAR
	if (blrOp == blr_like || blrOp == blr_similar)
	{
		VaryStr<TEMP_STR_LENGTH> temp3;
		const UCHAR* escape_str = NULL;
		USHORT escape_length = 0;
		// ensure 3rd argument (escape char) is in operation text type
		if (arg3 && !computed_invariant)
		{
			// Convert ESCAPE to operation character set
			dsc* desc = EVL_expr(tdbb, request, arg3);
			if (!desc)
			{
				if (nodFlags & FLAG_INVARIANT)
				{
					impure_value* impure = request->getImpure<impure_value>(impureOffset);
					impure->vlu_flags |= VLU_computed;
					impure->vlu_flags |= VLU_null;
				}
				return TriState();
			}

			escape_length = MOV_make_string(tdbb, desc, ttype,
				reinterpret_cast<const char**>(&escape_str), &temp3, sizeof(temp3));

			if (!escape_length || charset->length(escape_length, escape_str, true) != 1)
			{
				// If characters left, or null byte character, return error
				ERR_post(Arg::Gds(isc_escape_invalid));
			}

			USHORT escape[2] = {0, 0};

			charset->getConvToUnicode().convert(escape_length, escape_str, sizeof(escape), escape);

			if (!escape[0])
			{
				// If or null byte character, return error
				ERR_post(Arg::Gds(isc_escape_invalid));
			}
		}

		if (nodFlags & FLAG_INVARIANT)
		{
			impure_value* impure = request->getImpure<impure_value>(impureOffset);
			PatternMatcher* evaluator;

			if (!(impure->vlu_flags & VLU_computed))
			{
				delete impure->vlu_misc.vlu_invariant;
				impure->vlu_flags |= VLU_computed;

				if (blrOp == blr_like)
				{
					impure->vlu_misc.vlu_invariant = evaluator = obj->createLikeMatcher(
						*tdbb->getDefaultPool(), p2, l2, escape_str, escape_length);
				}
				else	// nod_similar
				{
					impure->vlu_misc.vlu_invariant = evaluator = obj->createSimilarToMatcher(
						tdbb, *tdbb->getDefaultPool(), p2, l2, escape_str, escape_length);
				}
			}
			else
			{
				evaluator = impure->vlu_misc.vlu_invariant;
				evaluator->reset();
			}

			evaluator->process(p1, l1);

			return TriState(evaluator->result());
		}

		if (blrOp == blr_like)
			return TriState(obj->like(*tdbb->getDefaultPool(), p1, l1, p2, l2, escape_str, escape_length));

		// nod_similar
		return TriState(obj->similarTo(tdbb, *tdbb->getDefaultPool(), p1, l1, p2, l2, escape_str, escape_length));
	}

	// Handle MATCHES
	return TriState(obj->matches(*tdbb->getDefaultPool(), p1, l1, p2, l2));
}

// Execute SLEUTH operator.
TriState ComparativeBoolNode::sleuth(thread_db* tdbb, jrd_req* request, const dsc* desc1,
	const dsc* desc2) const
{
	SET_TDBB(tdbb);

	// Choose interpretation for the operation

 	USHORT ttype;
	if (desc1->isBlob())
	{
		if (desc1->dsc_sub_type == isc_blob_text)
			ttype = desc1->dsc_blob_ttype();	// Load blob character set and collation
		else
			ttype = INTL_TTYPE(desc2);
	}
	else
		ttype = INTL_TTYPE(desc1);

	Collation* obj = INTL_texttype_lookup(tdbb, ttype);

	// Get operator definition string (control string)

	dsc* desc3 = EVL_expr(tdbb, request, arg3);
	if (!desc3)
		return TriState();

	UCHAR* p1;
	MoveBuffer sleuth_str;
	USHORT l1 = MOV_make_string2(tdbb, desc3, ttype, &p1, sleuth_str);
	// Get address and length of search string
	UCHAR* p2;
	MoveBuffer match_str;
	USHORT l2 = MOV_make_string2(tdbb, desc2, ttype, &p2, match_str);

	// Merge search and control strings
	UCHAR control[BUFFER_SMALL];
	const SLONG control_length = obj->sleuthMerge(*tdbb->getDefaultPool(), p2, l2, p1, l1, control); //, BUFFER_SMALL);

	// Note: resulting string from sleuthMerge is either USHORT or UCHAR
	// and never Multibyte (see note in EVL_mb_sleuthCheck)
	bool ret_val;
	MoveBuffer data_str;
	if (!desc1->isBlob())
	{
		// Source is not a blob, do a simple search

		l1 = MOV_make_string2(tdbb, desc1, ttype, &p1, data_str);
		ret_val = obj->sleuthCheck(*tdbb->getDefaultPool(), 0, p1, l1, control, control_length);
	}
	else
	{
		// Source string is a blob, things get interesting

		blb* blob = blb::open(tdbb, request->req_transaction,
			reinterpret_cast<bid*>(desc1->dsc_address));

		UCHAR buffer[BUFFER_LARGE];
		ret_val = false;

		while (!(blob->blb_flags & BLB_eof))
		{
			l1 = blob->BLB_get_segment(tdbb, buffer, sizeof(buffer));
			if (obj->sleuthCheck(*tdbb->getDefaultPool(), 0, buffer, l1, control, control_length))
			{
				ret_val = true;
				break;
			}
		}

		blob->BLB_close(tdbb);
	}

	return TriState(ret_val);
}

BoolExprNode* ComparativeBoolNode::createRseNode(DsqlCompilerScratch* dsqlScratch, UCHAR rseBlrOp)
{
	MemoryPool& pool = dsqlScratch->getPool();

	// Create a derived table representing our subquery.
	SelectExprNode* dt = FB_NEW_POOL(pool) SelectExprNode(pool);
	// Ignore validation for column names that must exist for "user" derived tables.
	dt->dsqlFlags = RecordSourceNode::DFLAG_DT_IGNORE_COLUMN_CHECK | RecordSourceNode::DFLAG_DERIVED;
	dt->querySpec = static_cast<RecordSourceNode*>(dsqlSpecialArg.getObject());

	RseNode* querySpec = FB_NEW_POOL(pool) RseNode(pool);
	querySpec->dsqlFrom = FB_NEW_POOL(pool) RecSourceListNode(pool, 1);
	querySpec->dsqlFrom->items[0] = dt;

	SelectExprNode* select_expr = FB_NEW_POOL(pool) SelectExprNode(pool);
	select_expr->querySpec = querySpec;

	const DsqlContextStack::iterator base(*dsqlScratch->context);
	const DsqlContextStack::iterator baseDT(dsqlScratch->derivedContext);
	const DsqlContextStack::iterator baseUnion(dsqlScratch->unionContext);

	RseNode* rse = PASS1_rse(dsqlScratch, select_expr, false);
	rse->flags |= RseNode::FLAG_DSQL_COMPARATIVE;

	// Create a conjunct to be injected.

	ComparativeBoolNode* cmpNode = FB_NEW_POOL(pool) ComparativeBoolNode(pool, blrOp,
		doDsqlPass(dsqlScratch, arg1, false), rse->dsqlSelectList->items[0]);

	PASS1_set_parameter_type(dsqlScratch, cmpNode->arg1, cmpNode->arg2, false);

	rse->dsqlWhere = cmpNode;

	// Create output node.
	RseBoolNode* rseBoolNode = FB_NEW_POOL(pool) RseBoolNode(pool, rseBlrOp, rse);

	// Finish off by cleaning up contexts
	dsqlScratch->unionContext.clear(baseUnion);
	dsqlScratch->derivedContext.clear(baseDT);
	dsqlScratch->context->clear(base);

	return rseBoolNode;
}


//--------------------


static RegisterBoolNode<MissingBoolNode> regMissingBoolNode({blr_missing});

MissingBoolNode::MissingBoolNode(MemoryPool& pool, ValueExprNode* aArg, bool aDsqlUnknown)
	: TypedNode<BoolExprNode, ExprNode::TYPE_MISSING_BOOL>(pool),
	  dsqlUnknown(aDsqlUnknown),
	  arg(aArg)
{
}

DmlNode* MissingBoolNode::parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR /*blrOp*/)
{
	MissingBoolNode* node = FB_NEW_POOL(pool) MissingBoolNode(pool);
	node->arg = PAR_parse_value(tdbb, csb);
	return node;
}

string MissingBoolNode::internalPrint(NodePrinter& printer) const
{
	BoolExprNode::internalPrint(printer);

	NODE_PRINT(printer, dsqlUnknown);
	NODE_PRINT(printer, arg);

	return "MissingBoolNode";
}

BoolExprNode* MissingBoolNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	MissingBoolNode* node = FB_NEW_POOL(dsqlScratch->getPool()) MissingBoolNode(dsqlScratch->getPool(),
		doDsqlPass(dsqlScratch, arg));

	// dimitr:	MSVC12 has a known bug with default function constructor. MSVC13 seems to have it fixed,
	//			but I keep the explicit empty-object initializer here.
	PASS1_set_parameter_type(dsqlScratch, node->arg, std::function<void (dsc*)>(nullptr), false);

	dsc desc;
	DsqlDescMaker::fromNode(dsqlScratch, &desc, node->arg);

	if (dsqlUnknown && desc.dsc_dtype != dtype_boolean && !desc.isNull())
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			Arg::Gds(isc_invalid_boolean_usage));
	}

	return node;
}

void MissingBoolNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->appendUChar(blr_missing);
	GEN_expr(dsqlScratch, arg);
}

BoolExprNode* MissingBoolNode::copy(thread_db* tdbb, NodeCopier& copier) const
{
	MissingBoolNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) MissingBoolNode(
		*tdbb->getDefaultPool());
	node->nodFlags = nodFlags;
	node->arg = copier.copy(tdbb, arg);
	return node;
}

BoolExprNode* MissingBoolNode::pass1(thread_db* tdbb, CompilerScratch* csb)
{
	return BoolExprNode::pass1(tdbb, csb);
}

void MissingBoolNode::pass2Boolean2(thread_db* tdbb, CompilerScratch* csb)
{
	RecordKeyNode* keyNode = nodeAs<RecordKeyNode>(arg);

	if (keyNode && keyNode->aggregate)
		ERR_post(Arg::Gds(isc_bad_dbkey));

	// check for syntax errors in the calculation
	dsc descriptor_a;
	arg->getDesc(tdbb, csb, &descriptor_a);
}

TriState MissingBoolNode::execute(thread_db* tdbb, jrd_req* request) const
{
	return TriState(EVL_expr(tdbb, request, arg) == nullptr);
}


//--------------------


static RegisterBoolNode<NotBoolNode> regNotBoolNode({blr_not});

NotBoolNode::NotBoolNode(MemoryPool& pool, BoolExprNode* aArg)
	: TypedNode<BoolExprNode, ExprNode::TYPE_NOT_BOOL>(pool),
	  arg(aArg)
{
}

DmlNode* NotBoolNode::parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR /*blrOp*/)
{
	NotBoolNode* node = FB_NEW_POOL(pool) NotBoolNode(pool);
	node->arg = PAR_parse_boolean(tdbb, csb);
	return node;
}

string NotBoolNode::internalPrint(NodePrinter& printer) const
{
	BoolExprNode::internalPrint(printer);

	NODE_PRINT(printer, arg);

	return "NotBoolNode";
}

BoolExprNode* NotBoolNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	return process(dsqlScratch, true);
}

void NotBoolNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->appendUChar(blr_not);
	GEN_expr(dsqlScratch, arg);
}

BoolExprNode* NotBoolNode::copy(thread_db* tdbb, NodeCopier& copier) const
{
	NotBoolNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) NotBoolNode(*tdbb->getDefaultPool());
	node->nodFlags = nodFlags;
	node->arg = copier.copy(tdbb, arg);
	return node;
}

TriState NotBoolNode::execute(thread_db* tdbb, jrd_req* request) const
{
	const TriState value = arg->execute(tdbb, request);
	return value.isUnknown() ? value : TriState(!value.value);
}

// Replace NOT with an appropriately inverted condition, if possible.
// Get rid of redundant nested NOT predicates.
BoolExprNode* NotBoolNode::process(DsqlCompilerScratch* dsqlScratch, bool invert)
{
	MemoryPool& pool = dsqlScratch->getPool();
	NotBoolNode* notArg = nodeAs<NotBoolNode>(arg);

	if (notArg)
	{
		// Recurse until different node is found (every even call means no inversion required).
		return notArg->process(dsqlScratch, !invert);
	}

	if (!invert)
		return arg->dsqlPass(dsqlScratch);

	ComparativeBoolNode* cmpArg = nodeAs<ComparativeBoolNode>(arg);
	BinaryBoolNode* binArg = nodeAs<BinaryBoolNode>(arg);

	// Do not handle special case: <value> NOT IN <list>

	if (cmpArg && (!cmpArg->dsqlSpecialArg || !nodeIs<ValueListNode>(cmpArg->dsqlSpecialArg)))
	{
		// Invert the given boolean.
		switch (cmpArg->blrOp)
		{
			case blr_eql:
			case blr_neq:
			case blr_lss:
			case blr_gtr:
			case blr_leq:
			case blr_geq:
			{
				UCHAR newBlrOp;

				switch (cmpArg->blrOp)
				{
					case blr_eql:
						newBlrOp = blr_neq;
						break;
					case blr_neq:
						newBlrOp = blr_eql;
						break;
					case blr_lss:
						newBlrOp = blr_geq;
						break;
					case blr_gtr:
						newBlrOp = blr_leq;
						break;
					case blr_leq:
						newBlrOp = blr_gtr;
						break;
					case blr_geq:
						newBlrOp = blr_lss;
						break;
					default:
						fb_assert(false);
						return NULL;
				}

				ComparativeBoolNode* node = FB_NEW_POOL(pool) ComparativeBoolNode(
					pool, newBlrOp, cmpArg->arg1, cmpArg->arg2);
				node->dsqlSpecialArg = cmpArg->dsqlSpecialArg;
				node->dsqlCheckBoolean = cmpArg->dsqlCheckBoolean;

				if (cmpArg->dsqlFlag == ComparativeBoolNode::DFLAG_ANSI_ANY)
					node->dsqlFlag = ComparativeBoolNode::DFLAG_ANSI_ALL;
				else if (cmpArg->dsqlFlag == ComparativeBoolNode::DFLAG_ANSI_ALL)
					node->dsqlFlag = ComparativeBoolNode::DFLAG_ANSI_ANY;

				return node->dsqlPass(dsqlScratch);
			}

			case blr_between:
			{
				ComparativeBoolNode* cmpNode1 = FB_NEW_POOL(pool) ComparativeBoolNode(pool,
					blr_lss, cmpArg->arg1, cmpArg->arg2);

				ComparativeBoolNode* cmpNode2 = FB_NEW_POOL(pool) ComparativeBoolNode(pool,
					blr_gtr, cmpArg->arg1, cmpArg->arg3);

				BinaryBoolNode* node = FB_NEW_POOL(pool) BinaryBoolNode(pool, blr_or,
					cmpNode1, cmpNode2);

				return node->dsqlPass(dsqlScratch);
			}
		}
	}
	else if (binArg)
	{
		switch (binArg->blrOp)
		{
			case blr_and:
			case blr_or:
			{
				UCHAR newBlrOp = binArg->blrOp == blr_and ? blr_or : blr_and;

				NotBoolNode* notNode1 = FB_NEW_POOL(pool) NotBoolNode(pool, binArg->arg1);
				NotBoolNode* notNode2 = FB_NEW_POOL(pool) NotBoolNode(pool, binArg->arg2);

				BinaryBoolNode* node = FB_NEW_POOL(pool) BinaryBoolNode(pool, newBlrOp,
					notNode1, notNode2);

				return node->dsqlPass(dsqlScratch);
			}
		}
	}

	// No inversion is possible, so just recreate the input node
	// and return immediately to avoid infinite recursion later.

	return FB_NEW_POOL(pool) NotBoolNode(pool, doDsqlPass(dsqlScratch, arg));
}


//--------------------


// ASF: Where is blr_exists handled?
static RegisterBoolNode<RseBoolNode> regRseBoolNode({blr_any, blr_unique, blr_ansi_any, blr_ansi_all, blr_exists});

RseBoolNode::RseBoolNode(MemoryPool& pool, UCHAR aBlrOp, RecordSourceNode* aDsqlRse)
	: TypedNode<BoolExprNode, ExprNode::TYPE_RSE_BOOL>(pool),
	  blrOp(aBlrOp),
	  ownSavepoint(true),
	  dsqlRse(aDsqlRse),
	  rse(NULL),
	  subQuery(NULL)
{
}

DmlNode* RseBoolNode::parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp)
{
	RseBoolNode* node = FB_NEW_POOL(pool) RseBoolNode(pool, blrOp);
	node->rse = PAR_rse(tdbb, csb);

	if (blrOp == blr_ansi_any || blrOp == blr_ansi_all)
	{
		// We treat ANY and ALL in the same manner in execution - as ANY.
		// For that, we invert ALL's operator to its negated complement and prefix RseBoolNode with NOT.

		// This code must be in sync with RseBoolNode::execute.

		BoolExprNode* rseBoolNode = nullptr;
		BinaryBoolNode* binNode = nullptr;
		ComparativeBoolNode* cmpNode = nullptr;

		if (node->rse->rse_boolean && (binNode = nodeAs<BinaryBoolNode>(node->rse->rse_boolean)) &&
			binNode->blrOp == blr_and)
		{
			rseBoolNode = binNode->arg1;
			cmpNode = nodeAs<ComparativeBoolNode>(binNode->arg2);
		}

		if (!cmpNode && node->rse->rse_boolean)
			cmpNode = nodeAs<ComparativeBoolNode>(node->rse->rse_boolean);

		if (!cmpNode)
			PAR_syntax_error(csb, "blr_ansi condition");

		auto valueNode = cmpNode->arg1;
		auto columnNode = cmpNode->arg2;

		SubExprNodeCopier copier(csb->csb_pool, csb);

		const auto injectedBoolean =
			FB_NEW_POOL(pool) BinaryBoolNode(pool, blr_or,
				FB_NEW_POOL(pool) BinaryBoolNode(pool, blr_or,
					cmpNode,
					FB_NEW_POOL(pool) MissingBoolNode(pool, copier.copy(tdbb, columnNode))
				),
				FB_NEW_POOL(pool) MissingBoolNode(pool, copier.copy(tdbb, valueNode))
			);

		node->rse->rse_boolean = rseBoolNode ?
			FB_NEW_POOL(pool) BinaryBoolNode(pool, blr_and, rseBoolNode, injectedBoolean) :
			injectedBoolean;

		if (blrOp == blr_ansi_all)
		{
			switch (cmpNode->blrOp)
			{
				case blr_eql:
					// = to <>
					cmpNode->blrOp = blr_neq;
					break;

				case blr_neq:
					// <> to =
					cmpNode->blrOp = blr_eql;
					break;

				case blr_gtr:
					// > to <=
					cmpNode->blrOp = blr_leq;
					break;

				case blr_geq:
					// >= to <
					cmpNode->blrOp = blr_lss;
					break;

				case blr_lss:
					// < to >=
					cmpNode->blrOp = blr_geq;
					break;

				case blr_leq:
					// <= to >
					cmpNode->blrOp = blr_gtr;
					break;

				default:
					fb_assert(false);
					PAR_syntax_error(csb, "blr_ansi operator");
					break;
			}
		}
	}

	if (blrOp == blr_any || blrOp == blr_exists) // maybe for blr_unique as well?
		node->rse->flags |= RseNode::FLAG_OPT_FIRST_ROWS;

	if (csb->csb_currentForNode && csb->csb_currentForNode->parBlrBeginCnt <= 1)
		node->ownSavepoint = false;

	if (csb->csb_currentDMLNode)
		node->ownSavepoint = false;

	if (blrOp == blr_ansi_all)
		return FB_NEW_POOL(pool) NotBoolNode(pool, node);

	return node;
}

string RseBoolNode::internalPrint(NodePrinter& printer) const
{
	BoolExprNode::internalPrint(printer);

	NODE_PRINT(printer, blrOp);
	NODE_PRINT(printer, ownSavepoint);
	NODE_PRINT(printer, dsqlRse);
	NODE_PRINT(printer, rse);
	NODE_PRINT(printer, subQuery);

	return "RseBoolNode";
}

BoolExprNode* RseBoolNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	if (dsqlScratch->flags & DsqlCompilerScratch::FLAG_VIEW_WITH_CHECK)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
				  Arg::Gds(isc_subquery_err));
	}

	const DsqlContextStack::iterator base(*dsqlScratch->context);

	RseBoolNode* node = FB_NEW_POOL(dsqlScratch->getPool()) RseBoolNode(dsqlScratch->getPool(), blrOp,
		PASS1_rse(dsqlScratch, nodeAs<SelectExprNode>(dsqlRse), false));

	// Finish off by cleaning up contexts
	dsqlScratch->context->clear(base);

	return node;
}

void RseBoolNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->appendUChar(blrOp);
	GEN_rse(dsqlScratch, nodeAs<RseNode>(dsqlRse));
}

bool RseBoolNode::dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const
{
	if (!BoolExprNode::dsqlMatch(dsqlScratch, other, ignoreMapCast))
		return false;

	const RseBoolNode* o = nodeAs<RseBoolNode>(other);
	fb_assert(o);

	return blrOp == o->blrOp;
}

bool RseBoolNode::sameAs(CompilerScratch* csb, const ExprNode* other, bool ignoreStreams) const
{
	if (!BoolExprNode::sameAs(csb, other, ignoreStreams))
		return false;

	const RseBoolNode* const otherNode = nodeAs<RseBoolNode>(other);
	fb_assert(otherNode);

	return blrOp == otherNode->blrOp;
}

BoolExprNode* RseBoolNode::copy(thread_db* tdbb, NodeCopier& copier) const
{
	RseBoolNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) RseBoolNode(
		*tdbb->getDefaultPool(), blrOp);
	node->nodFlags = nodFlags;
	node->ownSavepoint = this->ownSavepoint;
	node->rse = copier.copy(tdbb, rse);

	return node;
}

void RseBoolNode::pass2Boolean1(thread_db* tdbb, CompilerScratch* csb)
{
	if (!(rse->flags & RseNode::FLAG_VARIANT))
	{
		nodFlags |= FLAG_INVARIANT;
		csb->csb_invariants.push(&impureOffset);
	}

	rse->pass2Rse(tdbb, csb);
}

void RseBoolNode::pass2Boolean2(thread_db* tdbb, CompilerScratch* csb)
{
	if (nodFlags & FLAG_INVARIANT)
		impureOffset = csb->allocImpure<impure_value>();

	RecordSource* const rsb = CMP_post_rse(tdbb, csb, rse);

	csb->csb_fors.add(rsb);

	subQuery = FB_NEW_POOL(*tdbb->getDefaultPool()) SubQuery(rsb, rse->rse_invariants);
}

TriState RseBoolNode::execute(thread_db* tdbb, jrd_req* request) const
{
	USHORT* invariant_flags;
	impure_value* impure;

	if (nodFlags & FLAG_INVARIANT)
	{
		impure = request->getImpure<impure_value>(impureOffset);
		invariant_flags = &impure->vlu_flags;

		if (*invariant_flags & VLU_computed)
		{
			// An invariant node has already been computed.

			if ((blrOp == blr_ansi_any || blrOp == blr_ansi_all) && (*invariant_flags & VLU_null))
			{
				// ASF: Check suspicious code when req_null was there.
				fb_assert(impure->vlu_misc.vlu_short == 0);
				return TriState();
			}
			else
				return TriState(impure->vlu_misc.vlu_short != 0);
		}
	}

	StableCursorSavePoint savePoint(tdbb, request->req_transaction, ownSavepoint);

	subQuery->open(tdbb);
	TriState value(subQuery->fetch(tdbb));

	switch (blrOp)
	{
		case blr_unique:
			if (value == true)
				value = !subQuery->fetch(tdbb);
			break;

		case blr_ansi_all:
		case blr_ansi_any:
			// Check if we do have at least a record. ANY (empty) is FALSE.
			if (value == true)
			{
				// This code must be in sync with RseBoolNode::parse.

				auto boolNode1 = nodeAs<BinaryBoolNode>(rse->rse_boolean);
				fb_assert(boolNode1);

				if (boolNode1->blrOp == blr_and)	// User's RSE has a WHERE?
				{
					boolNode1 = nodeAs<BinaryBoolNode>(boolNode1->arg2);
					fb_assert(boolNode1);
				}

				fb_assert(boolNode1->blrOp == blr_or);

				auto boolNode2 = nodeAs<BinaryBoolNode>(boolNode1->arg1);
				fb_assert(boolNode2 && boolNode2->blrOp == blr_or);

				const auto missingValueNode = nodeAs<MissingBoolNode>(boolNode1->arg2);
				const auto valueNode = missingValueNode->arg;
				const auto cmpNode = boolNode2->arg1;

				fb_assert(missingValueNode && nodeIs<MissingBoolNode>(boolNode2->arg2));  // equivalent MissingBoolNode
				fb_assert(valueNode && cmpNode);

				// Test our value. NULL op ANY (non empty) is UNKNOWN.
				if (!EVL_expr(tdbb, request, valueNode))
					value.invalidate();
				else
				{
					bool hasNull = false;

					// If we found a record, immediately return TRUE.
					// Otherwise we should check all records.
					// If we had a NULL, return UNKNOWN.
					// If we had not a NULL, return FALSE.
					do
					{
						value = cmpNode->execute(tdbb, request);

						if (value == true)
							break;
						else if (value.isUnknown())
							hasNull = true;
					} while (subQuery->fetch(tdbb));

					if (value == false && hasNull)
						value.invalidate();
				}
			}
			break;
	}

	subQuery->close(tdbb);

	// If this is an invariant node, save the return value.

	if (nodFlags & FLAG_INVARIANT)
	{
		*invariant_flags |= VLU_computed;

		if ((blrOp == blr_ansi_any || blrOp == blr_ansi_all) && value.isUnknown())
			*invariant_flags |= VLU_null;

		impure->vlu_misc.vlu_short = value == true ? TRUE : FALSE;
	}

	return value;
}


}	// namespace Jrd
