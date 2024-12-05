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
#include "../jrd/optimizer/Optimizer.h"
#include "../jrd/blb_proto.h"
#include "../jrd/btr_proto.h"
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
#include "../dsql/DSqlDataTypeUtil.h"

using namespace Firebird;
using namespace Jrd;

namespace
{
	// Maximum members in "IN" list. For eg. SELECT * FROM T WHERE F IN (1, 2, 3, ...)
	// Beware: raising the limit beyond the 16-bit boundaries would be an incompatible BLR change.
	static const unsigned MAX_MEMBER_LIST = MAX_USHORT;

	// Compare two comparisons with the boolean literal of the form:
	//   [NOT] <value> [{ = | <> } { TRUE | FALSE }]
	// and detect whether they're logically the same.
	// For example: (NOT A) == (A = FALSE) == (A <> TRUE) == NOT (A = TRUE) etc.

	bool sameBoolComparison(const ComparativeBoolNode* node1, const ComparativeBoolNode* node2, bool ignoreStreams)
	{
		fb_assert(node1 && node2);

		if (node1->blrOp != blr_eql && node1->blrOp != blr_neq)
			return false;

		if (node2->blrOp != blr_eql && node2->blrOp != blr_neq)
			return false;

		bool isTrue1 = false;
		const ValueExprNode* arg1 = nullptr;

		if (const auto literal = nodeAs<LiteralNode>(node1->arg1))
		{
			if (literal->litDesc.isBoolean())
			{
				isTrue1 = literal->getBoolean();
				arg1 = node1->arg2;
			}
		}
		else if (const auto literal = nodeAs<LiteralNode>(node1->arg2))
		{
			if (literal->litDesc.isBoolean())
			{
				isTrue1 = literal->getBoolean();
				arg1 = node1->arg1;
			}
		}

		if (!arg1)
			return false;

		if (node1->blrOp == blr_neq)
			isTrue1 = !isTrue1;

		bool isTrue2 = false;
		const ValueExprNode* arg2 = nullptr;

		if (const auto literal = nodeAs<LiteralNode>(node2->arg1))
		{
			if (literal->litDesc.isBoolean())
			{
				isTrue2 = literal->getBoolean();
				arg2 = node2->arg2;
			}
		}
		else if (const auto literal = nodeAs<LiteralNode>(node2->arg2))
		{
			if (literal->litDesc.isBoolean())
			{
				isTrue2 = literal->getBoolean();
				arg2 = node2->arg1;
			}
		}

		if (!arg2)
			return false;

		if (node2->blrOp == blr_neq)
			isTrue2 = !isTrue2;

		if (!arg1->sameAs(arg2, ignoreStreams) || isTrue1 != isTrue2)
			return false;

		return true;
	}

} // namespace


//--------------------


BoolExprNode* BoolExprNode::pass2(thread_db* tdbb, CompilerScratch* csb)
{
	pass2Boolean(tdbb, csb, [=] { ExprNode::pass2(tdbb, csb); });

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

bool BinaryBoolNode::sameAs(const ExprNode* other, bool ignoreStreams) const
{
	const BinaryBoolNode* const otherNode = nodeAs<BinaryBoolNode>(other);

	if (!otherNode || blrOp != otherNode->blrOp)
		return false;

	if (arg1->sameAs(otherNode->arg1, ignoreStreams) &&
		arg2->sameAs(otherNode->arg2, ignoreStreams))
	{
		return true;
	}

	// A AND B is equivalent to B AND A, ditto for A OR B and B OR A.
	return arg1->sameAs(otherNode->arg2, ignoreStreams) &&
		arg2->sameAs(otherNode->arg1, ignoreStreams);
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

bool BinaryBoolNode::execute(thread_db* tdbb, Request* request) const
{
	switch (blrOp)
	{
		case blr_and:
			return executeAnd(tdbb, request);

		case blr_or:
			return executeOr(tdbb, request);
	}

	fb_assert(false);
	return false;
}

bool BinaryBoolNode::executeAnd(thread_db* tdbb, Request* request) const
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

	const bool value1 = arg1->execute(tdbb, request);

	// Save null state and get other operand.
	const USHORT firstnull = request->req_flags & req_null;
	request->req_flags &= ~req_null;

	if (!value1 && !firstnull)
	{
		// First term is false, why the whole expression is false.
		// NULL flag is already turned off a few lines above.
		return false;
	}

	const bool value2 = arg2->execute(tdbb, request);
	const USHORT secondnull = request->req_flags & req_null;
	request->req_flags &= ~req_null;

	if (!value2 && !secondnull)
		return false;	// at least one operand was false

	if (value1 && value2)
		return true;	// both true

	// otherwise, return null
	request->req_flags |= req_null;
	return false;
}

bool BinaryBoolNode::executeOr(thread_db* tdbb, Request* request) const
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

	const bool value1 = arg1->execute(tdbb, request);

	const ULONG flags = request->req_flags;
	request->req_flags &= ~req_null;

	if (value1)
	{
		// First term is true, why the whole expression is true.
		// NULL flag is already turned off a few lines above.
		return true;
	}

	const bool value2 = arg2->execute(tdbb, request);

	if (value1 || value2)
	{
		request->req_flags &= ~req_null;
		return true;
	}

	// restore saved NULL state

	if (flags & req_null)
		request->req_flags |= req_null;

	return false;
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
	  dsqlSpecialArg(nullptr)
{
}

ComparativeBoolNode::ComparativeBoolNode(MemoryPool& pool, UCHAR aBlrOp,
			ValueExprNode* aArg1, DsqlFlag aDsqlFlag, ExprNode* aSpecialArg)
	: TypedNode<BoolExprNode, ExprNode::TYPE_COMPARATIVE_BOOL>(pool),
	  blrOp(aBlrOp),
	  dsqlCheckBoolean(false),
	  dsqlFlag(aDsqlFlag),
	  arg1(aArg1),
	  arg2(nullptr),
	  arg3(nullptr),
	  dsqlSpecialArg(aSpecialArg)
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
		if (const auto listNode = nodeAs<ValueListNode>(dsqlSpecialArg))
		{
			if (listNode->items.getCount() > MAX_MEMBER_LIST)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
					Arg::Gds(isc_imp_exc) <<
					Arg::Gds(isc_dsql_too_many_values) << Arg::Num(MAX_MEMBER_LIST));
			}

			if (listNode->items.getCount() == 1)
			{
				// Convert A IN (B) into A = B

				ComparativeBoolNode* const resultNode = FB_NEW_POOL(dsqlScratch->getPool())
					ComparativeBoolNode(dsqlScratch->getPool(),
						blr_eql, arg1, listNode->items.front());

				return resultNode->dsqlPass(dsqlScratch);
			}

			// Generate the IN LIST boolean

			InListBoolNode* const resultNode = FB_NEW_POOL(dsqlScratch->getPool())
				InListBoolNode(dsqlScratch->getPool(), procArg1, listNode);

			return resultNode->dsqlPass(dsqlScratch);
		}

		if (const auto selNode = nodeAs<SelectExprNode>(dsqlSpecialArg))
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

bool ComparativeBoolNode::sameAs(const ExprNode* other, bool ignoreStreams) const
{
	const ComparativeBoolNode* const otherNode = nodeAs<ComparativeBoolNode>(other);

	if (!otherNode)
		return false;

	if (sameBoolComparison(this, otherNode, ignoreStreams))
		return true;

	if (blrOp != otherNode->blrOp)
		return false;

	bool matching = arg1->sameAs(otherNode->arg1, ignoreStreams) &&
		arg2->sameAs(otherNode->arg2, ignoreStreams);

	if (matching)
	{
		matching = (!arg3 == !otherNode->arg3) &&
			(!arg3 || arg3->sameAs(otherNode->arg3, ignoreStreams));

		if (matching)
			return true;
	}

	// TODO match A > B to B <= A, etc

	if (blrOp == blr_eql || blrOp == blr_equiv || blrOp == blr_neq)
	{
		// A = B is equivalent to B = A, etc.
		if (arg1->sameAs(otherNode->arg2, ignoreStreams) &&
			arg2->sameAs(otherNode->arg1, ignoreStreams))
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
			for (const auto& ctxNode : csb->csb_current_nodes)
			{
				if (nodeIs<RseNode>(ctxNode))
					return this;
			}

			nodFlags &= ~FLAG_INVARIANT;
		}
	}

	return this;
}

void ComparativeBoolNode::pass2Boolean(thread_db* tdbb, CompilerScratch* csb, std::function<void ()> process)
{
	if (nodFlags & FLAG_INVARIANT)
		csb->csb_invariants.push(&impureOffset);

	process();

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
		impureOffset = csb->allocImpure<impure_value>();
	// Do not use FLAG_PATTERN_MATCHER_CACHE for blr_starting as it has very fast compilation.
	else if (blrOp == blr_containing || blrOp == blr_like || blrOp == blr_similar)
	{
		impureOffset = csb->allocImpure<impure_value>();
		nodFlags |= FLAG_PATTERN_MATCHER_CACHE;
	}
}

bool ComparativeBoolNode::execute(thread_db* tdbb, Request* request) const
{
	dsc* desc[2] = {NULL, NULL};
	bool computed_invariant = false;

	request->req_flags &= ~req_same_tx_upd;

	// Evaluate arguments.  If either is null, result is null, but in
	// any case, evaluate both, since some expressions may later depend
	// on mappings which are developed here

	desc[0] = EVL_expr(tdbb, request, arg1);

	// arg1 IS NULL
	const bool null1 = (request->req_flags & req_null);

	request->req_flags &= ~req_null;
	bool force_equal = (request->req_flags & req_same_tx_upd) != 0;

	// Currently only nod_like, nod_contains, nod_starts and nod_similar may be marked invariant
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
			if (impure->vlu_flags & VLU_null)
				request->req_flags |= req_null;
			else
				computed_invariant = true;
		}
		else
		{
			desc[1] = EVL_expr(tdbb, request, arg2);

			if (request->req_flags & req_null)
			{
				impure->vlu_flags |= VLU_computed;
				impure->vlu_flags |= VLU_null;
			}
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

	// arg2 IS NULL
	const bool null2 = (request->req_flags & req_null);

	// An equivalence operator evaluates to true when both operands
	// are NULL and behaves like an equality operator otherwise.
	// Note that this operator never sets req_null flag

	if (blrOp == blr_equiv)
	{
		if (null1 && null2)
		{
			request->req_flags &= ~req_null;
			return true;
		}

		if (null1 || null2)
		{
			request->req_flags &= ~req_null;
			return false;
		}
	}

	// If either of expressions above returned NULL set req_null flag
	// and return false. The exception is BETWEEN operator that could
	// return FALSE even when arg2 IS NULL, for example:
	//   1 BETWEEN NULL AND 0

	if (null1 || (null2 && (blrOp != blr_between)))
	{
		request->req_flags |= req_null;
		return false;
	}

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
			comparison = MOV_compare(tdbb, desc[0], desc[1]);
			break;

		case blr_between:
			if (!null2)
			{
				comparison = MOV_compare(tdbb, desc[0], desc[1]);
				if (comparison < 0)
					return false;
			}
			else
				comparison = -1;
			break;
	}

	// If we are checking equality of record_version
	// and same transaction updated the record, force equality.

	const RecordKeyNode* recVersionNode = nodeAs<RecordKeyNode>(arg1);

	if (recVersionNode && recVersionNode->blrOp == blr_record_version && force_equal)
		comparison = 0;

	request->req_flags &= ~(req_null | req_same_tx_upd);

	switch (blrOp)
	{
		case blr_eql:
		case blr_equiv:
			return comparison == 0;

		case blr_gtr:
			return comparison > 0;

		case blr_geq:
			return comparison >= 0;

		case blr_lss:
			return comparison < 0;

		case blr_leq:
			return comparison <= 0;

		case blr_neq:
			return comparison != 0;

		case blr_between:
			desc[1] = EVL_expr(tdbb, request, arg3);
			if (request->req_flags & req_null)
			{
				if (!null2 && comparison < 0)
					request->req_flags &= ~req_null;
				return false;
			}
			{
				// arg1 <= arg3
				const bool cmp1_3 = (MOV_compare(tdbb, desc[0], desc[1]) <= 0);
				if (null2)
				{
					if (cmp1_3)
						request->req_flags |= req_null;
					return false;
				}
				return cmp1_3;
			}

		case blr_containing:
		case blr_starting:
		case blr_matching:
		case blr_like:
		case blr_similar:
			return stringBoolean(tdbb, request, desc[0], desc[1], computed_invariant);

		case blr_matching2:
			return sleuth(tdbb, request, desc[0], desc[1]);
	}

	return false;
}

// Perform one of the complex string functions CONTAINING, MATCHES, or STARTS WITH.
bool ComparativeBoolNode::stringBoolean(thread_db* tdbb, Request* request, dsc* desc1,
	dsc* desc2, bool computedInvariant) const
{
	SET_TDBB(tdbb);

	USHORT type1;

	if (!desc1->isBlob())
		type1 = INTL_TEXT_TYPE(*desc1);
	else
	{
		// No MATCHES support for blob
		if (blrOp == blr_matching)
			return false;

		type1 = desc1->dsc_sub_type == isc_blob_text ? desc1->dsc_blob_ttype() : ttype_none;
	}

	Collation* obj = INTL_texttype_lookup(tdbb, type1);
	CharSet* charset = obj->getCharSet();

	VaryStr<TEMP_STR_LENGTH> escapeTemp;
	const UCHAR* escapeStr = nullptr;
	USHORT escapeLen = 0;

	// Handle escape for LIKE and SIMILAR
	if (blrOp == blr_like || blrOp == blr_similar)
	{
		// ensure 3rd argument (escape char) is in operation text type
		if (arg3 && !computedInvariant)
		{
			// Convert ESCAPE to operation character set
			dsc* desc = EVL_expr(tdbb, request, arg3);

			if (request->req_flags & req_null)
			{
				if (nodFlags & FLAG_INVARIANT)
				{
					impure_value* impure = request->getImpure<impure_value>(impureOffset);
					impure->vlu_flags |= VLU_computed;
					impure->vlu_flags |= VLU_null;
				}
				return false;
			}

			escapeLen = MOV_make_string(tdbb, desc, type1,
				reinterpret_cast<const char**>(&escapeStr), &escapeTemp, sizeof(escapeTemp));

			if (!escapeLen || charset->length(escapeLen, escapeStr, true) != 1)
			{
				// If characters left, or null byte character, return error
				ERR_post(Arg::Gds(isc_escape_invalid));
			}

			USHORT escape[2] = {0, 0};

			charset->getConvToUnicode().convert(escapeLen, escapeStr, sizeof(escape), escape);

			if (!escape[0])
			{
				// If or null byte character, return error
				ERR_post(Arg::Gds(isc_escape_invalid));
			}
		}
	}

	UCHAR* patternStr = nullptr;
	SLONG patternLen = 0;
	MoveBuffer patternBuffer;

	auto createMatcher = [&]()
	{
		return blrOp == blr_containing ? obj->createContainsMatcher(*tdbb->getDefaultPool(), patternStr, patternLen) :
			blrOp == blr_starting ? obj->createStartsMatcher(*tdbb->getDefaultPool(), patternStr, patternLen) :
			blrOp == blr_like ? obj->createLikeMatcher(*tdbb->getDefaultPool(),
				patternStr, patternLen, escapeStr, escapeLen) :
			blrOp == blr_similar ? obj->createSimilarToMatcher(tdbb, *tdbb->getDefaultPool(),
				patternStr, patternLen, escapeStr, escapeLen) :
			nullptr;	// blr_matching
	};

	// Get address and length of search string - convert to datatype of data
	if (!computedInvariant)
		patternLen = MOV_make_string2(tdbb, desc2, type1, &patternStr, patternBuffer, false);

	AutoPtr<PatternMatcher> autoEvaluator;	// deallocate non-invariant/non-cached evaluator
	PatternMatcher* evaluator;

	impure_value* impure = request->getImpure<impure_value>(impureOffset);

	if (nodFlags & FLAG_INVARIANT)
	{
		auto& matcher = impure->vlu_misc.vlu_invariant;

		if (!(impure->vlu_flags & VLU_computed))
		{
			delete matcher;
			matcher = nullptr;
			matcher = createMatcher();
			impure->vlu_flags |= VLU_computed;
		}
		else
			matcher->reset();

		evaluator = matcher;
	}
	else if (nodFlags & FLAG_PATTERN_MATCHER_CACHE)
	{
		auto& cache = impure->vlu_misc.vlu_patternMatcherCache;
		const bool cacheHit = cache &&
			cache->matcher &&
			cache->ttype == type1 &&
			cache->patternLen == patternLen &&
			cache->escapeLen == escapeLen &&
			memcmp(cache->key, patternStr, patternLen) == 0 &&
			memcmp(cache->key + patternLen, escapeStr, escapeLen) == 0;

		if (cacheHit)
			cache->matcher->reset();
		else
		{
			if (cache && cache->keySize < patternLen + escapeLen)
			{
				delete cache;
				cache = nullptr;
			}

			if (!cache)
			{
				cache = FB_NEW_RPT(*tdbb->getDefaultPool(), patternLen + escapeLen)
					impure_value::PatternMatcherCache(patternLen + escapeLen);
			}

			cache->ttype = type1;
			cache->patternLen = patternLen;
			cache->escapeLen = escapeLen;
			memcpy(cache->key, patternStr, patternLen);
			memcpy(cache->key + patternLen, escapeStr, escapeLen);

			cache->matcher = createMatcher();
		}

		evaluator = cache->matcher;
	}
	else
		autoEvaluator = evaluator = desc1->isBlob() ? createMatcher() : nullptr;

	if (!desc1->isBlob())
	{
		// Source is not a blob, do a simple search

		VaryStr<256> temp1;
		UCHAR* str = NULL;
		const USHORT strLen = MOV_get_string_ptr(tdbb, desc1, &type1, &str, &temp1, sizeof(temp1));

		if (evaluator)
		{
			evaluator->process(str, strLen);
			return evaluator->result();
		}
		else
		{
			if (blrOp == blr_containing)
				return obj->contains(*tdbb->getDefaultPool(), str, strLen, patternStr, patternLen);
			else if (blrOp == blr_starting)
				return obj->starts(*tdbb->getDefaultPool(), str, strLen, patternStr, patternLen);
			else if (blrOp == blr_like)
				return obj->like(*tdbb->getDefaultPool(), str, strLen, patternStr, patternLen, escapeStr, escapeLen);
			else if (blrOp == blr_similar)
			{
				return obj->similarTo(tdbb, *tdbb->getDefaultPool(),
					str, strLen, patternStr, patternLen, escapeStr, escapeLen);
			}
			else	// blr_matching
				return obj->matches(*tdbb->getDefaultPool(), str, strLen, patternStr, patternLen);
		}
	}

	fb_assert(evaluator);

	// Source string is a blob, things get interesting

	AutoBlb blob(tdbb, blb::open(tdbb, request->req_transaction, reinterpret_cast<bid*>(desc1->dsc_address)));

	HalfStaticArray<UCHAR, BUFFER_SMALL> buffer;

	if (charset->isMultiByte() &&
		(blrOp != blr_starting || !(obj->getFlags() & TEXTTYPE_DIRECT_MATCH)))
	{
		buffer.getBuffer(blob->blb_length);		// alloc space to put entire blob in memory
	}

	// Performs the string_function on each segment of the blob until
	// a positive result is obtained

	while (!(blob->blb_flags & BLB_eof))
	{
		const SLONG bufferLen = blob->BLB_get_data(tdbb, buffer.begin(), buffer.getCapacity(), false);
		if (!evaluator->process(buffer.begin(), bufferLen))
			break;
	}

	return evaluator->result();
}

// Execute SLEUTH operator.
bool ComparativeBoolNode::sleuth(thread_db* tdbb, Request* request, const dsc* desc1,
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

	return ret_val;
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

	RseNode* rse = PASS1_rse(dsqlScratch, select_expr);
	rse->flags |= RseNode::FLAG_DSQL_COMPARATIVE;

	// Create a conjunct to be injected.

	ComparativeBoolNode* cmpNode = FB_NEW_POOL(pool) ComparativeBoolNode(pool, blrOp,
		doDsqlPass(dsqlScratch, arg1, false), rse->dsqlSelectList->items[0]);

	PASS1_set_parameter_type(dsqlScratch, cmpNode->arg1, cmpNode->arg2, false);

	rse->dsqlWhere = cmpNode;

	// Create output node.
	RseBoolNode* rseBoolNode = FB_NEW_POOL(pool) RseBoolNode(pool, rseBlrOp, rse);
	rseBoolNode->line = line;
	rseBoolNode->column = column;

	// Finish off by cleaning up contexts
	dsqlScratch->unionContext.clear(baseUnion);
	dsqlScratch->derivedContext.clear(baseDT);
	dsqlScratch->context->clear(base);

	return rseBoolNode;
}


//--------------------


static RegisterBoolNode<InListBoolNode> regInListBoolNode({blr_in_list});

InListBoolNode::InListBoolNode(MemoryPool& pool, ValueExprNode* aArg, ValueListNode* aList)
	: TypedNode<BoolExprNode, ExprNode::TYPE_IN_LIST_BOOL>(pool),
	  arg(aArg),
	  list(aList)
{
}

DmlNode* InListBoolNode::parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp)
{
	const auto arg = PAR_parse_value(tdbb, csb);

	const auto count = csb->csb_blr_reader.getWord();
	const auto list = PAR_args(tdbb, csb, count, count);

	return FB_NEW_POOL(pool) InListBoolNode(pool, arg, list);
}

string InListBoolNode::internalPrint(NodePrinter& printer) const
{
	BoolExprNode::internalPrint(printer);

	NODE_PRINT(printer, blrOp);
	NODE_PRINT(printer, arg);
	NODE_PRINT(printer, list);

	return "InListBoolNode";
}

BoolExprNode* InListBoolNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	const auto procArg = doDsqlPass(dsqlScratch, arg);
	const auto procList = doDsqlPass(dsqlScratch, list);

	const auto node = FB_NEW_POOL(dsqlScratch->getPool())
		InListBoolNode(dsqlScratch->getPool(), procArg, procList);

	// Try to force arg to be same type as list eg: ? = (FIELD, ...) case
	for (auto item : procList->items)
		PASS1_set_parameter_type(dsqlScratch, node->arg, item, false);

	// Try to force list to be same type as arg eg: FIELD = (?, ...) case
	for (auto item : procList->items)
		PASS1_set_parameter_type(dsqlScratch, item, node->arg, false);

	// Derive a common data type for the list items
	dsc argDesc;
	DsqlDescMaker::fromNode(dsqlScratch, &argDesc, procArg);

	dsc listDesc;
	DsqlDescMaker::fromList(dsqlScratch, &listDesc, procList, "IN LIST");

	if (argDesc.isText() && listDesc.isText())
	{
		const dsc* descs[] = {&argDesc, &listDesc};
		dsc commonDesc;
		DSqlDataTypeUtil(dsqlScratch).makeFromList(&commonDesc, "IN LIST",
			FB_NELEM(descs), descs);

		if (IS_INTL_DATA(&argDesc) || IS_INTL_DATA(&listDesc))
		{
			const auto charset1 = argDesc.getCharSet();
			const auto charset2 = listDesc.getCharSet();

			if ((charset1 != CS_BINARY) && (charset2 != CS_BINARY) &&
				((charset1 != CS_ASCII) || (charset2 != CS_ASCII)) &&
				((charset1 != CS_NONE) || (charset2 != CS_NONE)))
			{
				const auto ttype = MAX(argDesc.getTextType(), listDesc.getTextType());
				commonDesc.setTextType(ttype);
			}
		}

		listDesc = commonDesc;
	}

	// Cast to the common data type where necessary
	for (auto& item : procList->items)
	{
		const auto desc = item->getDsqlDesc();

		if (!DSC_EQUIV(&listDesc, &desc, true))
		{
			auto field = FB_NEW_POOL(dsqlScratch->getPool())
				dsql_fld(dsqlScratch->getPool());

			field->dtype = listDesc.dsc_dtype;
			field->scale = listDesc.dsc_scale;
			field->subType = listDesc.dsc_sub_type;
			field->length = listDesc.dsc_length;
			field->flags = (listDesc.dsc_flags & DSC_nullable) ? FLD_nullable : 0;

			if (desc.isText() || desc.isBlob())
			{
				field->textType = listDesc.getTextType();
				field->charSetId = listDesc.getCharSet();
				field->collationId = listDesc.getCollation();
			}

			const auto castNode = FB_NEW_POOL(dsqlScratch->getPool())
				CastNode(dsqlScratch->getPool(), item, field);
			item = castNode;
		}
	}

	return node;
}

void InListBoolNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->appendUChar(blrOp);

	GEN_expr(dsqlScratch, arg);

	fb_assert(list->items.getCount() <= MAX_USHORT);
	dsqlScratch->appendUShort(list->items.getCount());

	for (auto item : list->items)
		GEN_expr(dsqlScratch, item);
}

bool InListBoolNode::dsqlMatch(DsqlCompilerScratch* dsqlScratch, const ExprNode* other, bool ignoreMapCast) const
{
	if (!BoolExprNode::dsqlMatch(dsqlScratch, other, ignoreMapCast))
		return false;

	return nodeIs<InListBoolNode>(other);
}

bool InListBoolNode::sameAs(const ExprNode* other, bool ignoreStreams) const
{
	const auto otherNode = nodeAs<InListBoolNode>(other);

	if (!otherNode)
		return false;

	return (arg->sameAs(otherNode->arg, ignoreStreams) &&
		list->sameAs(otherNode->list, ignoreStreams));
}

BoolExprNode* InListBoolNode::copy(thread_db* tdbb, NodeCopier& copier) const
{
	const auto newArg = copier.copy(tdbb, arg);
	const auto newList = copier.copy(tdbb, list);

	const auto node = FB_NEW_POOL(*tdbb->getDefaultPool())
		InListBoolNode(*tdbb->getDefaultPool(), newArg, newList);
	node->nodFlags = nodFlags;

	return node;
}

BoolExprNode* InListBoolNode::pass1(thread_db* tdbb, CompilerScratch* csb)
{
	doPass1(tdbb, csb, arg.getAddress());

	nodFlags |= FLAG_INVARIANT;
	csb->csb_current_nodes.push(this);

	doPass1(tdbb, csb, list.getAddress());

	csb->csb_current_nodes.pop();

	if (nodFlags & FLAG_INVARIANT)
	{
		// If there is no top-level RSE present and list items are not constant, unmark node as invariant
		// because it may be dependent on data or variables

		for (const auto& ctxNode : csb->csb_current_nodes)
		{
			if (nodeIs<RseNode>(ctxNode))
				return this;
		}

		for (auto item : list->items)
		{
			while (auto castNode = nodeAs<CastNode>(item))
				item = castNode->source;

			if (!nodeIs<LiteralNode>(item) && !nodeIs<ParameterNode>(item))
			{
				nodFlags &= ~FLAG_INVARIANT;
				break;
			}
		}
	}

	return this;
}

void InListBoolNode::pass2Boolean(thread_db* tdbb, CompilerScratch* csb, std::function<void ()> process)
{
	if (nodFlags & FLAG_INVARIANT)
		csb->csb_invariants.push(&impureOffset);

	process();

	if (const auto keyNode = nodeAs<RecordKeyNode>(arg))
	{
		if (keyNode->aggregate)
			ERR_post(Arg::Gds(isc_bad_dbkey));
	}

	dsc argDesc, listDesc;
	arg->getDesc(tdbb, csb, &argDesc);
	list->getDesc(tdbb, csb, &listDesc);

	if (argDesc.isDateTime())
		arg->nodFlags |= FLAG_DATE;
	else if (listDesc.isDateTime())
	{
		for (auto item : list->items)
			item->nodFlags |= FLAG_DATE;
	}

	// If lookup in the list is to be performed against the generic comparison rules,
	// add an extra cast to make things working properly
	if (!BTR_types_comparable(listDesc, argDesc))
	{
		for (auto& item : list->items)
		{
			const auto castNode = FB_NEW_POOL(csb->csb_pool) CastNode(csb->csb_pool);
			castNode->castDesc = argDesc;
			castNode->source = item;
			castNode->impureOffset = csb->allocImpure<impure_value>();
			item = castNode;
		}
	}

	if (nodFlags & FLAG_INVARIANT)
		impureOffset = csb->allocImpure<impure_value>();

	lookup = FB_NEW_POOL(csb->csb_pool) LookupValueList(csb->csb_pool, list, impureOffset);
}

bool InListBoolNode::execute(thread_db* tdbb, Request* request) const
{
	if (const auto argDesc = EVL_expr(tdbb, request, arg))
	{
		bool anyMatch = false, anyNull = false;

		if (nodFlags & FLAG_INVARIANT)
		{
			anyMatch = lookup->find(tdbb, request, arg, argDesc);
			anyNull = (request->req_flags & req_null);
		}
		else
		{
			for (const auto value : list->items)
			{
				if (const auto valueDesc = EVL_expr(tdbb, request, value))
				{
					if (!MOV_compare(tdbb, argDesc, valueDesc))
					{
						anyMatch = true;
						break;
					}
				}
				else
				{
					anyNull = true;
				}
			}
		}

		request->req_flags &= ~req_null;

		if (anyMatch)
			return true;

		if (anyNull)
			request->req_flags |= req_null;
	}

	return false; // for argDesc == nullptr, req_null is already set by EVL_expr()
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

void MissingBoolNode::pass2Boolean(thread_db* tdbb, CompilerScratch* csb, std::function<void ()> process)
{
	process();

	RecordKeyNode* keyNode = nodeAs<RecordKeyNode>(arg);

	if (keyNode && keyNode->aggregate)
		ERR_post(Arg::Gds(isc_bad_dbkey));

	// check for syntax errors in the calculation
	dsc descriptor_a;
	arg->getDesc(tdbb, csb, &descriptor_a);
}

bool MissingBoolNode::execute(thread_db* tdbb, Request* request) const
{
	EVL_expr(tdbb, request, arg);

	if (request->req_flags & req_null)
	{
		request->req_flags &= ~req_null;
		return true;
	}

	return false;
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

BoolExprNode* NotBoolNode::pass1(thread_db* tdbb, CompilerScratch* csb)
{
	RseBoolNode* rseBoolean = nodeAs<RseBoolNode>(arg);

	if (rseBoolean)
	{
		if (rseBoolean->blrOp == blr_ansi_any)
			rseBoolean->nodFlags |= FLAG_DEOPTIMIZE | FLAG_ANSI_NOT;
		else if (rseBoolean->blrOp == blr_ansi_all)
			rseBoolean->nodFlags |= FLAG_ANSI_NOT;
	}

	return BoolExprNode::pass1(tdbb, csb);
}

bool NotBoolNode::execute(thread_db* tdbb, Request* request) const
{
	bool value = arg->execute(tdbb, request);

	if (request->req_flags & req_null)
		return false;

	return !value;
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

	const auto cmpArg = nodeAs<ComparativeBoolNode>(arg);
	const auto binArg = nodeAs<BinaryBoolNode>(arg);

	// Do not handle special case: <value> NOT IN <list>

	if (cmpArg && (!cmpArg->dsqlSpecialArg || !nodeIs<ValueListNode>(cmpArg->dsqlSpecialArg)))
	{
		// Invert the given boolean

		// For (A = TRUE/FALSE), invert only the boolean value, not the condition itself

		if (cmpArg->blrOp == blr_eql)
		{
			auto newArg1 = cmpArg->arg1;
			auto newArg2 = cmpArg->arg2;

			if (const auto literal = nodeAs<LiteralNode>(cmpArg->arg1))
			{
				if (literal->litDesc.isBoolean())
				{
					const auto invertedVal = literal->getBoolean() ? "" : "1";
					newArg1 = MAKE_constant(invertedVal, CONSTANT_BOOLEAN);
				}
			}
			else if (const auto literal = nodeAs<LiteralNode>(cmpArg->arg2))
			{
				if (literal->litDesc.isBoolean())
				{
					const auto invertedVal = literal->getBoolean() ? "" : "1";
					newArg2 = MAKE_constant(invertedVal, CONSTANT_BOOLEAN);
				}
			}

			if (cmpArg->arg1 != newArg1 || cmpArg->arg2 != newArg2)
			{
				ComparativeBoolNode* node = FB_NEW_POOL(pool) ComparativeBoolNode(
					pool, cmpArg->blrOp, newArg1, newArg2);
				node->dsqlSpecialArg = cmpArg->dsqlSpecialArg;
				node->dsqlCheckBoolean = cmpArg->dsqlCheckBoolean;

				return node->dsqlPass(dsqlScratch);
			}
		}

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
	  dsqlRse(aDsqlRse)
{
}

DmlNode* RseBoolNode::parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp)
{
	RseBoolNode* node = FB_NEW_POOL(pool) RseBoolNode(pool, blrOp);
	node->rse = PAR_rse(tdbb, csb);

	node->rse->flags |= RseNode::FLAG_SUB_QUERY;

	if (blrOp == blr_any || blrOp == blr_exists) // maybe for blr_unique as well?
		node->rse->firstRows = true;

	if (csb->csb_currentForNode && csb->csb_currentForNode->parBlrBeginCnt <= 1)
		node->ownSavepoint = false;

	if (csb->csb_currentDMLNode)
		node->ownSavepoint = false;

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
		PASS1_rse(dsqlScratch, nodeAs<SelectExprNode>(dsqlRse)));

	// Finish off by cleaning up contexts
	dsqlScratch->context->clear(base);

	return node;
}

void RseBoolNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->appendUChar(blrOp);

	dsqlScratch->putDebugSrcInfo(line, column);
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

bool RseBoolNode::sameAs(const ExprNode* other, bool ignoreStreams) const
{
	if (!BoolExprNode::sameAs(other, ignoreStreams))
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

BoolExprNode* RseBoolNode::pass1(thread_db* tdbb, CompilerScratch* csb)
{
	switch (blrOp)
	{
		case blr_ansi_all:
		{
			BoolExprNode* newNode = convertNeqAllToNotAny(tdbb, csb);
			if (newNode)
			{
				doPass1(tdbb, csb, &newNode);
				return newNode;
			}

			nodFlags |= FLAG_DEOPTIMIZE;
		}
		// fall into

		case blr_ansi_any:
		{
			bool deoptimize = false;

			if (nodFlags & FLAG_DEOPTIMIZE)
			{
				nodFlags &= ~FLAG_DEOPTIMIZE;
				deoptimize = true;
			}

			// Mark the injected boolean as residual, this is required
			// to process quantified predicates correctly in some cases.
			//
			// If necessary, also deoptimize the injected boolean.
			// ALL predicate should not be evaluated using an index scan.
			// This fixes bug SF #543106.

			BoolExprNode* boolean = rse->rse_boolean;
			if (boolean)
			{
				BinaryBoolNode* const binaryNode = nodeAs<BinaryBoolNode>(boolean);
				if (binaryNode && binaryNode->blrOp == blr_and)
					boolean = binaryNode->arg2;

				boolean->nodFlags |= FLAG_RESIDUAL | (deoptimize ? FLAG_DEOPTIMIZE : 0);
			}
		}

		break;
	}

	return BoolExprNode::pass1(tdbb, csb);
}

void RseBoolNode::pass2Boolean(thread_db* tdbb, CompilerScratch* csb, std::function<void ()> process)
{
	if (rse->isInvariant())
	{
		nodFlags |= FLAG_INVARIANT;
		csb->csb_invariants.push(&impureOffset);
	}

	AutoSetCurrentCursorId autoSetCurrentCursorId(csb);

	rse->pass2Rse(tdbb, csb);

	process();

	if (nodFlags & FLAG_INVARIANT)
		impureOffset = csb->allocImpure<impure_value>();

	RecordSource* const rsb = CMP_post_rse(tdbb, csb, rse);

	// for ansi ANY clauses (and ALL's, which are negated ANY's)
	// the unoptimized boolean expression must be used, since the
	// processing of these clauses is order dependant (see FilteredStream.cpp)

	if (blrOp == blr_ansi_any || blrOp == blr_ansi_all)
	{
		const bool ansiAny = blrOp == blr_ansi_any;
		const bool ansiNot = nodFlags & FLAG_ANSI_NOT;
		rsb->setAnyBoolean(rse->rse_boolean, ansiAny, ansiNot);
	}

	subQuery = FB_NEW_POOL(*tdbb->getDefaultPool()) SubQuery(csb, rsb, rse);
	csb->csb_fors.add(subQuery);
}

bool RseBoolNode::execute(thread_db* tdbb, Request* request) const
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

			if (blrOp == blr_ansi_any && (*invariant_flags & VLU_null))
				request->req_flags |= req_null;
			else
				request->req_flags &= ~req_null;

			return impure->vlu_misc.vlu_short != 0;
		}
	}

	StableCursorSavePoint savePoint(tdbb, request->req_transaction, ownSavepoint);

	bool value;
	try
	{
		subQuery->open(tdbb);
		value = subQuery->fetch(tdbb);

		if (blrOp == blr_unique && value)
			value = !subQuery->fetch(tdbb);
	}
	catch (const Exception&)
	{
		try
		{
			subQuery->close(tdbb);
		}
		catch (const Exception&)
		{} // ignore any error to report the original one

		throw;
	}

	subQuery->close(tdbb);

	savePoint.release();

	if (blrOp == blr_any || blrOp == blr_unique)
		request->req_flags &= ~req_null;

	// If this is an invariant node, save the return value.

	if (nodFlags & FLAG_INVARIANT)
	{
		*invariant_flags |= VLU_computed;

		if ((blrOp == blr_ansi_any || blrOp == blr_ansi_all) && (request->req_flags & req_null))
			*invariant_flags |= VLU_null;

		impure->vlu_misc.vlu_short = value ? TRUE : FALSE;
	}

	return value;
}

// Try to convert nodes of expression:
//   select ... from <t1>
//     where <x> not in (select <y> from <t2>)
//   (and its variants that uses the same BLR: {NOT (a = ANY b)} and {a <> ALL b})
// to:
//   select ... from <t1>
//     where not ((x is null and exists (select 1 from <t2>)) or
//                exists (select <y> from <t2> where <y> = <x> or <y> is null))
//
// Because the second form can use indexes.
// Returns NULL when not converted, and a new node to be processed when converted.
BoolExprNode* RseBoolNode::convertNeqAllToNotAny(thread_db* tdbb, CompilerScratch* csb)
{
	SET_TDBB(tdbb);

	fb_assert(blrOp == blr_ansi_all);

	RseNode* outerRse = rse;	// blr_ansi_all rse
	ComparativeBoolNode* outerRseNeq;

	if (!outerRse ||
		outerRse->getType() != RseNode::TYPE ||		// Reduntant test?
		outerRse->rse_relations.getCount() != 1 ||
		!outerRse->rse_boolean ||
		!(outerRseNeq = nodeAs<ComparativeBoolNode>(outerRse->rse_boolean)) ||
		outerRseNeq->blrOp != blr_neq)
	{
		return NULL;
	}

	RseNode* innerRse = static_cast<RseNode*>(outerRse->rse_relations[0].getObject());	// user rse

	// If the rse is different than we expected, do nothing. Do nothing also if it uses FIRST or
	// SKIP, as we can't inject booleans there without changing the behavior.
	if (!innerRse || innerRse->getType() != RseNode::TYPE || innerRse->rse_first || innerRse->rse_skip)
		return NULL;

	NotBoolNode* newNode = FB_NEW_POOL(csb->csb_pool) NotBoolNode(csb->csb_pool);

	BinaryBoolNode* orNode = FB_NEW_POOL(csb->csb_pool) BinaryBoolNode(csb->csb_pool, blr_or);

	newNode->arg = orNode;

	BinaryBoolNode* andNode = FB_NEW_POOL(csb->csb_pool) BinaryBoolNode(csb->csb_pool, blr_and);

	orNode->arg1 = andNode;

	MissingBoolNode* missNode = FB_NEW_POOL(csb->csb_pool) MissingBoolNode(csb->csb_pool);
	missNode->arg = outerRseNeq->arg1;

	andNode->arg1 = missNode;

	RseBoolNode* rseBoolNode = FB_NEW_POOL(csb->csb_pool) RseBoolNode(csb->csb_pool, blr_any);
	rseBoolNode->rse = innerRse;
	rseBoolNode->ownSavepoint = this->ownSavepoint;

	andNode->arg2 = rseBoolNode;

	RseNode* newInnerRse = innerRse->clone(csb->csb_pool);

	rseBoolNode = FB_NEW_POOL(csb->csb_pool) RseBoolNode(csb->csb_pool, blr_any);
	rseBoolNode->rse = newInnerRse;
	rseBoolNode->ownSavepoint = this->ownSavepoint;

	orNode->arg2 = rseBoolNode;

	BinaryBoolNode* boolean = FB_NEW_POOL(csb->csb_pool) BinaryBoolNode(csb->csb_pool, blr_or);

	missNode = FB_NEW_POOL(csb->csb_pool) MissingBoolNode(csb->csb_pool);
	missNode->arg = outerRseNeq->arg2;

	boolean->arg1 = missNode;

	boolean->arg2 = outerRse->rse_boolean;
	outerRseNeq->blrOp = blr_eql;

	// If there was a boolean on the stream, append (AND) the new one
	if (newInnerRse->rse_boolean)
	{
		andNode = FB_NEW_POOL(csb->csb_pool) BinaryBoolNode(csb->csb_pool, blr_and);

		andNode->arg1 = newInnerRse->rse_boolean;
		andNode->arg2 = boolean;
		boolean = andNode;
	}

	newInnerRse->rse_boolean = boolean;

	SubExprNodeCopier copier(csb->csb_pool, csb);
	return copier.copy(tdbb, static_cast<BoolExprNode*>(newNode));
}
