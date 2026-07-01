/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		pass1.cpp
 *	DESCRIPTION:	First-pass compiler for statement trees.
 *
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
 *
 * 2001.5.26: Claudio Valderrama: field names should be skimmed from trailing
 *
 * 2001.5.26: Claudio Valderrama: COMPUTED fields will be skipped if a dummy
 *       "insert into tbl values(...)" sentence is issued.
 *
 * 2001.5.26: Claudio Valderrama: field names should be skimmed from trailing
 *		blanks to allow reliable comparisons in pass1_field. Same for table and
 *		and index names in plans.
 *
 * 2001.5.29: Claudio Valderrama: handle DROP VIEW case in pass1_statement().
 *
 * 2001.6.12: Claudio Valderrama: add basic BREAK capability to procedures.
 *
 * 2001.6.27: Claudio Valderrama: pass1_variable() now gives the name of the
 * variable it can't find in the error message.
 *
 * 2001.6.30: Claudio Valderrama: Enhanced again to provide (line, col), see node.h.
 *
 * 2001.7.28: John Bellardo: added code to handle nod_limit and associated fields.
 *
 * 2001.08.14 Claudio Valderrama: fixed crash with trigger and CURRENT OF <cursor> syntax.
 *
 * 2001.09.10 John Bellardo: fixed gen_rse to attribute skip/first nodes to the parent_rse
 *   if present instead of the child rse.  BUG #451798
 *
 * 2001.09.26 Claudio Valderrama: ambiguous field names are rejected from now.
 *
 * 2001.10.01 Claudio Valderrama: check constraints are allowed to have ambiguous field
 *   names because they use OLD and NEW as aliases of the same table. However, if the
 *   check constraint has an embedded ambiguous SELECT statement, it won't be detected.
 *   The code should be revisited if check constraints' before delete triggers are used
 *   for whatever reason. Currently they are never generated. The code can be improved
 *   to not report errors for fields between NEW and OLD contexts but complain otherwise.
 *
 * 2001.10.05 Neil McCalden: validate udf and parameters when comparing select list and
 *   group by list, to detect invalid SQL statements when grouping by UDFs.
 *
 * 2001.10.23 Ann Harrison:  allow reasonable checking of ambiguous names in unions.
 *   Remembering, later, that LLS_PUSH expects an object, not an LLS block.  Also
 *   stuck in the code for handling variables in pass1 - it apparently doesn't happen
 *   because the code returned an uninitialized pointer.
 *
 * 2001.11.17 Neil McCalden: Add aggregate_in_list procedure to handle cases
 *   where select statement has aggregate as a parameter to a udf which does
 *   not have to be in a group by clause.
 *
 * 2001.11.21 Claudio Valderrama: don't try to detect ambiguity in pass1_field()
 *   if the field or output procedure parameter has been fully qualified!!!
 *
 * 2001.11.27 Ann Harrison:  Redo the amiguity checking so as to give better
 *   error messages, return warnings for dialect 1, and simplify.
 *
 * 2001.11.28 Claudio Valderrama: allow udf arguments to be query parameters.
 *   Honor the code in the parser that already accepts those parameters.
 *   This closes SF Bug# 409769.
 *
 * 2001.11.29 Claudio Valderrama: make the nice new ambiguity checking code do the
 *   right thing instead of crashing the engine and restore fix from 2001.11.21.
 *
 * 2001.12.21 Claudio Valderrama: Fix SF Bug #494832 - pass1_variable() should work
 *   with def_proc, mod_proc, redef_proc, def_trig and mod_trig node types.
 *
 * 2002.07.30 Arno Brinkman: Added pass1_coalesce, pass1_simple_case, pass1_searched_case
 *   and PASS1_put_args_on_stack
 *
 * 2002.08.04 Arno Brinkman: Added ignore_cast as parameter to PASS1_node_match,
 *   Changed invalid_reference procedure for allow EXTRACT, SUBSTRING, CASE,
 *   COALESCE and NULLIF functions in GROUP BY and as select_items.
 *   Removed aggregate_in_list procedure.
 *
 * 2002.08.07 Dmitry Yemanov: Disabled BREAK statement in triggers
 *
 * 2002.08.10 Dmitry Yemanov: ALTER VIEW
 *
 * 2002.09.28 Dmitry Yemanov: Reworked internal_info stuff, enhanced
 *                            exception handling in SPs/triggers,
 *                            implemented ROWS_AFFECTED system variable
 *
 * 2002.09.29 Arno Brinkman: Adding more checking for aggregate functions
 *   and adding support for 'linking' from sub-selects to aggregate functions
 *   which are in an lower level.
 *   Modified functions pass1_field, pass1_rse, copy_field, PASS1_sort.
 *   Functions pass1_found_aggregate and pass1_found_field added.
 *
 * 2002.10.21 Nickolay Samofatov: Added support for explicit pessimistic locks
 *
 * 2002.10.25 Dmitry Yemanov: Re-allowed plans in triggers
 *
 * 2002.10.29 Nickolay Samofatov: Added support for savepoints
 *
 * 2002.12.03 Dmitry Yemanov: Implemented ORDER BY clause in subqueries
 *
 * 2002.12.18 Dmitry Yemanov: Fixed bug with BREAK and partially implemented
 *							  SQL-compliant labels and LEAVE statement
 *
 * 2003.01.11 Arno Brinkman: Reworked a lot of functions for bringing back backwards compatibilty
 *							 with sub-selects and aggregates.
 *
 * 2003.01.14 Dmitry Yemanov: Fixed bug with cursors in triggers
 *
 * 2003.01.15 Dmitry Yemanov: Added support for parametrized events
 *
 * 2003.04.05 Dmitry Yemanov: Changed logic of ORDER BY with collations
 *							  (because of the parser change)
 *
 * 2003.08.14 Arno Brinkman: Added derived table support.
 *
 * 2003.08.16 Arno Brinkman: Changed ambiguous column name checking.
 *
 * 2003.10.05 Dmitry Yemanov: Added support for explicit cursors in PSQL.
 *
 * 2004.01.16 Vlad Horsun: added support for default parameters and
 *   EXECUTE BLOCK statement
 *
 * 2026.05.17 Karol Bieniaszewski: added grouping sets
 *
 * Adriano dos Santos Fernandes
 *
 */

#include "firebird.h"
#include <numeric>
#include <string.h>
#include <stdio.h>
#include "ibase.h"
#include "../dsql/dsql.h"
#include "../dsql/Nodes.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/intl.h"
#include "firebird/impl/blr.h"
#include "../jrd/jrd.h"
#include "../jrd/constants.h"
#include "../jrd/intl_classes.h"
#include "../jrd/RecordSourceNodes.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/StmtNodes.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/pass1_proto.h"
#include "../dsql/utld_proto.h"
#include "../dsql/DSqlDataTypeUtil.h"
#include "../common/dsc_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/met_proto.h"
#include "../yvalve/why_proto.h"
#include "../jrd/SysFunction.h"
#include "../common/classes/array.h"
#include "../common/classes/auto.h"
#include "../common/utils_proto.h"
#include "../common/config/config.h"
#include "../common/StatusArg.h"

using namespace Jrd;
using namespace Firebird;


static ValueListNode* pass1_group_by_list(DsqlCompilerScratch*, ValueListNode*, ValueListNode*);
static GroupingSpec* pass1_grouping_spec_raw(DsqlCompilerScratch*, GroupingClause*, ValueListNode*);
static RseNode* pass1_lower_grouping_sets(DsqlCompilerScratch*, RseNode*, ValueListNode*, RowsClause*,
	bool, bool, USHORT);
static ValueExprNode* pass1_make_derived_field(thread_db*, DsqlCompilerScratch*, ValueExprNode*);
static RseNode* pass1_rse(DsqlCompilerScratch*, RecordSourceNode*, ValueListNode*, RowsClause*, bool, bool, USHORT);
static RseNode* pass1_rse_impl(DsqlCompilerScratch*, RecordSourceNode*, ValueListNode*, RowsClause*, bool, bool, USHORT);
static ValueListNode* pass1_sel_list(DsqlCompilerScratch*, ValueListNode*);
static RseNode* pass1_union(DsqlCompilerScratch*, UnionSourceNode*, ValueListNode*, RowsClause*, bool, bool, USHORT);
static void pass1_union_auto_cast(DsqlCompilerScratch*, ExprNode*, const dsc&, FB_SIZE_T position);
static void remap_streams_to_parent_context(ExprNode*, dsql_ctx*);


AggregateFinder::AggregateFinder(MemoryPool& pool, DsqlCompilerScratch* aDsqlScratch, bool aWindow)
	: PermanentStorage(pool),
	  dsqlScratch(aDsqlScratch),
	  window(aWindow),
	  currentLevel(dsqlScratch->scopeLevel),
	  deepestLevel(0),
	  ignoreSubSelects(false)
{
}

bool AggregateFinder::find(MemoryPool& pool, DsqlCompilerScratch* dsqlScratch, bool window, ExprNode* node)
{
	AggregateFinder visitor(pool, dsqlScratch, window);
	return visitor.visit(node);
}

bool AggregateFinder::visit(ExprNode* node)
{
	return node && node->dsqlAggregateFinder(*this);
}


Aggregate2Finder::Aggregate2Finder(MemoryPool& pool, USHORT aCheckScopeLevel,
		FieldMatchType aMatchType, bool aWindowOnly)
	: PermanentStorage(pool),
	  checkScopeLevel(aCheckScopeLevel),
	  matchType(aMatchType),
	  windowOnly(aWindowOnly),
	  currentScopeLevelEqual(true)
{
}

bool Aggregate2Finder::find(MemoryPool& pool, USHORT checkScopeLevel,
	FieldMatchType matchType, bool windowOnly, ExprNode* node)
{
	Aggregate2Finder visitor(pool, checkScopeLevel, matchType, windowOnly);
	return visitor.visit(node);
}

bool Aggregate2Finder::visit(ExprNode* node)
{
	return node && node->dsqlAggregate2Finder(*this);
}


FieldFinder::FieldFinder(MemoryPool& pool, USHORT aCheckScopeLevel, FieldMatchType aMatchType)
	: PermanentStorage(pool),
	  checkScopeLevel(aCheckScopeLevel),
	  matchType(aMatchType),
	  field(false)
{
}

bool FieldFinder::find(MemoryPool& pool, USHORT checkScopeLevel, FieldMatchType matchType, ExprNode* node)
{
	FieldFinder visitor(pool, checkScopeLevel, matchType);
	return visitor.visit(node);
}

bool FieldFinder::visit(ExprNode* node)
{
	return node && node->dsqlFieldFinder(*this);
}


InvalidReferenceFinder::InvalidReferenceFinder(DsqlCompilerScratch* aDsqlScratch,
		const dsql_ctx* aContext, const ValueListNode* aList)
	: dsqlScratch(aDsqlScratch),
	  context(aContext),
	  list(aList),
	  insideOwnMap(false),
	  insideHigherMap(false)
{
	DEV_BLKCHK(list, dsql_type_nod);
}

bool InvalidReferenceFinder::find(DsqlCompilerScratch* dsqlScratch, const dsql_ctx* context,
	const ValueListNode* list, ExprNode* node)
{
	InvalidReferenceFinder visitor(dsqlScratch, context, list);
	return visitor.visit(node);
}

bool InvalidReferenceFinder::visit(ExprNode* node)
{
	DEV_BLKCHK(node, dsql_type_nod);

	if (!node)
		return false;

	// ASF: What we do in this function is the verification of all fields/dbkeys (or any parent
	// expression involving them) are present in the passed node list.
	// That makes valid:
	//   select n + 0 from table group by n			=> The n item is present in the list
	//   select n + 0 from table group by n + 0		=> The n + 0 item is present in the list
	// And makes invalid:
	//   select n + 1 from table group by n + 0		=> The n + 1 item is not present in the list

	if (list)
	{
		// Check if this node (with ignoring of CASTs) appears also
		// in the list of group by. If yes then it's allowed
		const NestConst<ValueExprNode>* ptr = list->items.begin();
		for (const NestConst<ValueExprNode>* const end = list->items.end(); ptr != end; ++ptr)
		{
			if (PASS1_node_match(dsqlScratch, node, *ptr, true))
				return false;
		}
	}

	return node->dsqlInvalidReferenceFinder(*this);
}


FieldRemapper::FieldRemapper(MemoryPool& pool, DsqlCompilerScratch* aDsqlScratch, dsql_ctx* aContext, bool aWindow,
			WindowClause* aWindowNode)
	: PermanentStorage(pool),
	  dsqlScratch(aDsqlScratch),
	  context(aContext),
	  window(aWindow),
	  windowNode(aWindowNode),
	  currentLevel(dsqlScratch->scopeLevel)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);
	DEV_BLKCHK(context, dsql_type_ctx);
}

ExprNode* FieldRemapper::visit(ExprNode* node)
{
	return node ? node->dsqlFieldRemapper(*this) : NULL;
}


bool SubSelectFinder::visit(ExprNode* node)
{
	return node && node->dsqlSubSelectFinder(*this);
}


/**

 	PASS1_make_context

    @brief	Generate a context for a dsqlScratch.


    @param dsqlScratch
    @param relationNode

 **/
dsql_ctx* PASS1_make_context(DsqlCompilerScratch* dsqlScratch, RecordSourceNode* relationNode)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);

	thread_db* const tdbb = JRD_get_thread_data();

	// figure out whether this is a relation or a procedure
	// and give an error if it is neither

	QualifiedName name;
	ProcedureSourceNode* procNode = NULL;
	RelationSourceNode* relNode = NULL;
	SelectExprNode* selNode = NULL;
	TableValueFunctionSourceNode* tableValueFunctionNode = nullptr;

	if ((procNode = nodeAs<ProcedureSourceNode>(relationNode)))
		name = procNode->dsqlName;
	else if ((relNode = nodeAs<RelationSourceNode>(relationNode)))
		name = relNode->dsqlName;
	else if ((tableValueFunctionNode = nodeAs<TableValueFunctionSourceNode>(relationNode)))
		name.object = tableValueFunctionNode->alias;
	//// TODO: LocalTableSourceNode
	else if ((selNode = nodeAs<SelectExprNode>(relationNode)))
		name.object = selNode->alias;

	SelectExprNode* cte = NULL;
	dsql_rel* relation = NULL;
	dsql_prc* procedure = NULL;
	dsql_tab_func* tableValueFunctionContext = nullptr;

	if (selNode)
	{
		// No processing needed here for derived tables.
	}
	else if (tableValueFunctionNode)
	{
		tableValueFunctionContext = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_tab_func(*tdbb->getDefaultPool());
		tableValueFunctionContext->funName = tableValueFunctionNode->dsqlName;
	}
	else if (!((procNode && procNode->inputSources) || name.schema.hasData() || name.package.hasData()) &&
		(cte = dsqlScratch->findCTE(name.object)))
	{
		relationNode = cte;
	}
	else
	{
		const auto resolvedObject = dsqlScratch->resolveRoutineOrRelation(name,
			(((procNode && procNode->inputSources)) ?
				std::initializer_list<ObjectType>{obj_procedure} :
				std::initializer_list<ObjectType>{obj_procedure, obj_relation}));

		if (const auto resolvedProcedure = std::get_if<dsql_prc*>(&resolvedObject))
			procedure = *resolvedProcedure;
		else if (const auto resolvedRelation = std::get_if<dsql_rel*>(&resolvedObject))
			relation = *resolvedRelation;

		if (!procedure && !relation)
		{
			const auto errorCode = name.package.hasData() || (procNode && procNode->inputSources) ?
				isc_dsql_procedure_err : isc_dsql_relation_err;

			ERRD_post(
				Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
				Arg::Gds(errorCode) <<
				Arg::Gds(isc_random) << name.toQuotedString() <<
				Arg::Gds(isc_dsql_line_col_error) << Arg::Num(relationNode->line) << Arg::Num(relationNode->column));
		}

		if (procedure)
		{
			if (procedure->prc_private && name.getSchemaAndPackage() != dsqlScratch->package)
			{
				status_exception::raise(
					Arg::Gds(isc_private_procedure) <<
					name.object.toQuotedString() <<
					name.getSchemaAndPackage().toQuotedString());
			}

			if (!procedure->prc_out_count)
			{
				ERRD_post(
					Arg::Gds(isc_sqlerr) << Arg::Num(-84) <<
					Arg::Gds(isc_dsql_procedure_use_err) << name.toQuotedString() <<
					Arg::Gds(isc_dsql_line_col_error) <<
						Arg::Num(relationNode->line) << Arg::Num(relationNode->column));
			}

			procNode->dsqlName = name;
		}
		else if (relation)
		{
			if (relation->rel_private && name.getSchemaAndPackage() != dsqlScratch->package)
			{
				status_exception::raise(
					Arg::Gds(isc_private_table) <<
					name.object.toQuotedString() <<
					name.getSchemaAndPackage().toQuotedString());
			}

			if (relNode)
				relNode->dsqlName = name;
		}
	}

	// Set up context block.
	const auto context = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_ctx(*tdbb->getDefaultPool());
	context->ctx_relation = relation;
	context->ctx_procedure = procedure;
	context->ctx_table_value_fun = tableValueFunctionContext;

	if (selNode)
	{
		context->ctx_context = USHORT(MAX_UCHAR) + 1 + dsqlScratch->derivedContextNumber++;

		if (selNode->dsqlFlags & RecordSourceNode::DFLAG_CURSOR)
			context->ctx_flags |= CTX_cursor;

		if (selNode->dsqlFlags & RecordSourceNode::DFLAG_LATERAL)
			context->ctx_flags |= CTX_lateral;
	}
	else
		context->ctx_context = dsqlScratch->contextNumber++;

	context->ctx_scope_level = dsqlScratch->scopeLevel;

	// When we're in a outer-join part mark context for it.
	if (dsqlScratch->inOuterJoin)
		context->ctx_flags |= CTX_outer_join;

	context->ctx_in_outer_join = dsqlScratch->inOuterJoin;

	// find the context alias name, if it exists.
	string str;

	if ((procNode = nodeAs<ProcedureSourceNode>(relationNode)))
		str = procNode->alias;
	else if ((relNode = nodeAs<RelationSourceNode>(relationNode)))
		str = relNode->alias;
	else if ((selNode = nodeAs<SelectExprNode>(relationNode)))
	{
		str = selNode->alias;
		// ASF: In the case of a UNION contained in a CTE, selNode->querySpec will be a
		// UnionSourceNode instead of a RseNode. Since ctx_rse is a RseNode and is always accessed
		// as one, I'll leave this assignment here. It will be set in PASS1_derived_table anyway.
		///context->ctx_rse = selNode->querySpec;
	}
	else if ((tableValueFunctionNode = nodeAs<TableValueFunctionSourceNode>(relationNode)))
		str = tableValueFunctionNode->alias.c_str();

	if (str.hasData())
		context->ctx_internal_alias = QualifiedName(str);

	if (dsqlScratch->aliasRelationPrefix.hasData() && !selNode)
	{
		context->ctx_alias = dsqlScratch->aliasRelationPrefix;

		if (str.isEmpty() && name.object.hasData())
			context->ctx_alias.push(name);
	}

	if (str.hasData())
		context->ctx_alias.push(QualifiedName(str));

	if (context->ctx_alias.hasData())
	{
		// check to make sure the context is not already used at this same
		// query level (if there are no subqueries, this checks that the
		// alias is not used twice in the dsqlScratch).
		for (DsqlContextStack::iterator stack(*dsqlScratch->context); stack.hasData(); ++stack)
		{
			const dsql_ctx* conflict = stack.object();

			if (conflict->ctx_scope_level != context->ctx_scope_level)
				continue;

			ObjectsArray<QualifiedName> conflictNames;
			ISC_STATUS error_code;

			if (conflict->ctx_alias.hasData())
			{
				conflictNames = conflict->ctx_alias;
				error_code = isc_alias_conflict_err;
				// alias %s conflicts with an alias in the same dsqlScratch.
			}
			else if (conflict->ctx_procedure)
			{
				conflictNames.add(conflict->ctx_procedure->prc_name);
				error_code = isc_procedure_conflict_error;
				// alias %s conflicts with a procedure in the same dsqlScratch.
			}
			else if (conflict->ctx_relation)
			{
				conflictNames.add(conflict->ctx_relation->rel_name);
				error_code = isc_relation_conflict_err;
				// alias %s conflicts with a relation in the same dsqlScratch.
			}
			else
				continue;

			if (PASS1_compare_alias(context->ctx_alias, conflictNames))
			{
				fb_assert(conflictNames.hasData());

				const auto conflictStr = std::accumulate(
					std::next(conflictNames.begin()),
					conflictNames.end(),
					conflictNames[0].toQuotedString(),
					[](const auto& a, const auto& b) {
						return a + " " + b.toQuotedString();
					}
				);

				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
						  Arg::Gds(error_code) << conflictStr);
			}
		}
	}

	if (procedure)
	{
		USHORT count = 0;

		if (procNode && procNode->inputSources)
		{
			context->ctx_proc_inputs = Node::doDsqlPass(dsqlScratch, procNode->inputSources, false);
			count = context->ctx_proc_inputs->items.getCount();
		}

		if (count > procedure->prc_in_count ||
			count < procedure->prc_in_count - procedure->prc_def_count)
		{
			ERRD_post(Arg::Gds(isc_prcmismat) << procNode->dsqlName.toQuotedString());
		}

		if (count)
		{
			auto inputList = context->ctx_proc_inputs;
			auto argIt = inputList->items.begin();
			const auto argEnd = inputList->items.end();

			const auto positionalArgCount = inputList->items.getCount() -
				(procNode->dsqlInputArgNames ? procNode->dsqlInputArgNames->getCount() : 0);
			const auto* field = procedure->prc_inputs;
			unsigned pos = 0;

			while (pos < positionalArgCount && field && argIt != argEnd)
			{
				dsc descNode;
				DsqlDescMaker::fromField(&descNode, field);

				PASS1_set_parameter_type(dsqlScratch, *argIt,
					[&] (dsc* desc) { *desc = descNode; },
					false);

				field = field->fld_next;
				++pos;
				++argIt;
			}

			if (procNode->dsqlInputArgNames)
			{
				fb_assert(procNode->dsqlInputArgNames->getCount() <= inputList->items.getCount());

				LeftPooledMap<MetaName, const dsql_fld*> argsByName;

				for (const auto* field = procedure->prc_inputs; field; field = field->fld_next)
					argsByName.put(field->fld_name, field);

				Arg::StatusVector mismatchStatus;

				for (const auto& argName : *procNode->dsqlInputArgNames)
				{
					if (const auto field = argsByName.get(argName))
					{
						dsc descNode;
						DsqlDescMaker::fromField(&descNode, *field);

						PASS1_set_parameter_type(dsqlScratch, *argIt,
							[&] (dsc* desc) { *desc = descNode; },
							false);
					}
					else
						mismatchStatus << Arg::Gds(isc_param_not_exist) << argName;

					++argIt;
				}

				if (mismatchStatus.hasData())
					status_exception::raise(Arg::Gds(isc_prcmismat) << procNode->dsqlName.toQuotedString() << mismatchStatus);
			}
		}
	}
	else if (tableValueFunctionNode)
	{
		context->ctx_flags |= CTX_blr_fields;

		fb_assert(tableValueFunctionContext);
		tableValueFunctionContext->outputField = tableValueFunctionNode->makeField(dsqlScratch);

		context->ctx_proc_inputs = tableValueFunctionNode->inputList;
	}

	// push the context onto the dsqlScratch context stack
	// for matching fields against

	dsqlScratch->context->push(context);

	return context;
}


// Compile a record selection expression, bumping up the statement scope level everytime an rse is
// seen. The scope level controls parsing of aliases.
RseNode* PASS1_rse(DsqlCompilerScratch* dsqlScratch,
				   SelectExprNode* input,
				   const SelectNode* select)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);
	DEV_BLKCHK(input, dsql_type_nod);

	const bool updateLock = select ? select->withLock : false;
	const bool skipLocked = select ? select->skipLocked : false;

	dsqlScratch->scopeLevel++;
	RseNode* node = pass1_rse(dsqlScratch, input, NULL, NULL, updateLock, skipLocked, 0);
	dsqlScratch->scopeLevel--;

	if (select)
		node->firstRows = select->optimizeForFirstRows;

	return node;
}


// Check for ambiguity in a field reference. The list with contexts where the field was found is
// checked and the necessary message is build from it.
void PASS1_ambiguity_check(DsqlCompilerScratch* dsqlScratch,
	const MetaName& name, const DsqlContextStack& ambiguous_contexts)
{
	// If there are no relations or only 1 there's no ambiguity, thus return.
	if (ambiguous_contexts.getCount() < 2)
		return;

	string buffers[2];
	string* bufferPtr = &buffers[0];
	bool printAliasHelp = false;

	for (DsqlContextStack::const_iterator stack(ambiguous_contexts); stack.hasData(); ++stack)
	{
		string& buffer = *bufferPtr;
		const dsql_ctx* context = stack.object();
		const dsql_rel* relation = context->ctx_relation;
		const dsql_prc* procedure = context->ctx_procedure;

		// if this is the second loop add "and " before relation.
		if (buffer.hasData())
			buffer += " and ";

		// Process relation when present.
		if (relation)
		{
			if (!(relation->rel_flags & REL_view))
				buffer += "table ";
			else
				buffer += "view ";

			buffer += relation->rel_name.toQuotedString();
		}
		else if (procedure)
		{
			// Process procedure when present.
			buffer += "procedure ";
			buffer += procedure->prc_name.toQuotedString();
		}
		else if (context->ctx_flags & CTX_package)
		{
			// Package constant or variable
			printAliasHelp = true;
			buffer += "package ";
			if (context->ctx_alias.hasData())
				buffer += context->getConcatenatedAlias();
		}
		else
		{
			const auto contextAliases = context->getConcatenatedAlias();

			// When there's no relation and no procedure it's a derived table.
			buffer += "derived table ";
			if (context->ctx_alias.hasData())
				buffer += contextAliases;
		}

		if (bufferPtr == &buffers[0])
			++bufferPtr;
	}

	if (dsqlScratch->clientDialect >= SQL_DIALECT_V6)
	{
		Arg::StatusVector status;
		status.assign(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
				  Arg::Gds(isc_dsql_ambiguous_field_name) << buffers[0] << buffers[1] <<
				  Arg::Gds(isc_random) << name);

		if (printAliasHelp)
			status.append(Arg::Gds(isc_package_alias_help));

		ERR_post(status);
	}

	ERRD_post_warning(Arg::Warning(isc_sqlwarn) << Arg::Num(204) <<
					  Arg::Warning(isc_dsql_ambiguous_field_name) << buffers[0] << buffers[1] <<
					  Arg::Warning(isc_random) << name);
}


bool PASS1_compare_alias(const QualifiedName& contextAlias, const QualifiedName& lookupAlias)
{
	return lookupAlias == contextAlias || (lookupAlias.schema.isEmpty() && lookupAlias.object == contextAlias.object);
}


bool PASS1_compare_alias(const ObjectsArray<QualifiedName>& contextAlias,
	const ObjectsArray<QualifiedName>& lookupAlias)
{
	if (contextAlias.getCount() != lookupAlias.getCount())
		return false;

	auto contextAliasIt = contextAlias.begin();

	for (const auto& lookupAliasIt : lookupAlias)
	{
		if (!PASS1_compare_alias(*contextAliasIt, lookupAliasIt))
			return false;

		++contextAliasIt;
	}

	return true;
}


// Compose two booleans.
BoolExprNode* PASS1_compose(BoolExprNode* expr1, BoolExprNode* expr2, UCHAR blrOp)
{
	thread_db* tdbb = JRD_get_thread_data();

	fb_assert(blrOp == blr_and || blrOp == blr_or);

	if (!expr1)
		return expr2;

	if (!expr2)
		return expr1;

	return FB_NEW_POOL(*tdbb->getDefaultPool()) BinaryBoolNode(
		*tdbb->getDefaultPool(), blrOp, expr1, expr2);
}


// Report a field parsing recognition error.
void PASS1_field_unknown(const TEXT* qualifier_name, const TEXT* field_name,
	const ExprNode* flawed_node)
{
	string buffer;

	if (qualifier_name)
	{
		buffer.printf("%s.%s", qualifier_name, (field_name ? field_name : "*"));
		field_name = buffer.c_str();
	}

	if (flawed_node)
	{
		if (field_name)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-206) <<
					  Arg::Gds(isc_dsql_field_err) <<
					  Arg::Gds(isc_random) << Arg::Str(field_name) <<
					  Arg::Gds(isc_dsql_line_col_error) << Arg::Num(flawed_node->line) <<
					  									   Arg::Num(flawed_node->column));
		}
		else
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-206) <<
					  Arg::Gds(isc_dsql_field_err) <<
					  Arg::Gds(isc_dsql_line_col_error) << Arg::Num(flawed_node->line) <<
					  									   Arg::Num(flawed_node->column));
		}
	}
	else
	{
		if (field_name)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-206) <<
					  Arg::Gds(isc_dsql_field_err) <<
					  Arg::Gds(isc_random) << Arg::Str(field_name) <<
					  Arg::Gds(isc_dsql_unknown_pos));
		}
		else
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-206) <<
					  Arg::Gds(isc_dsql_field_err) <<
					  Arg::Gds(isc_dsql_unknown_pos));
		}
	}
}




namespace
{
	bool pass1_raw_field_matches_resolved_field(DsqlCompilerScratch* dsqlScratch,
		const FieldNode* rawField, const FieldNode* resolvedField);
}


/**

	PASS1_node_match

    @brief	Compare two nodes for equality of value.

   [2002-08-04]--- Arno Brinkman
 	If ignoreMapCast is true and the node1 is
   type nod_cast or nod_map then PASS1_node_match is
   calling itselfs again with the node1
   CASTs source or map->node.
   This is for allow CAST to other datatypes
   without complaining that it's an unknown
   column reference. (Aggregate functions)


    @param node1
    @param node2
    @param ignoreMapCast

 **/
bool PASS1_node_match(DsqlCompilerScratch* dsqlScratch, const ExprNode* node1, const ExprNode* node2,
	bool ignoreMapCast)
{
	thread_db* tdbb = JRD_get_thread_data();

	DEV_BLKCHK(node1, dsql_type_nod);
	DEV_BLKCHK(node2, dsql_type_nod);

	JRD_reschedule(tdbb);

	if (!node1 && !node2)
		return true;

	if (!node1 || !node2)
		return false;

	const CastNode* castNode1 = nodeAs<CastNode>(node1);

	if (ignoreMapCast && castNode1)
	{
		const CastNode* castNode2 = nodeAs<CastNode>(node2);

		// If node2 is also cast and same type continue with both sources.
		if (castNode2 &&
			castNode1->castDesc.dsc_dtype == castNode2->castDesc.dsc_dtype &&
			castNode1->castDesc.dsc_scale == castNode2->castDesc.dsc_scale &&
			castNode1->castDesc.dsc_length == castNode2->castDesc.dsc_length &&
			castNode1->castDesc.dsc_sub_type == castNode2->castDesc.dsc_sub_type)
		{
			return PASS1_node_match(dsqlScratch, castNode1->source, castNode2->source, ignoreMapCast);
		}

		return PASS1_node_match(dsqlScratch, castNode1->source, node2, ignoreMapCast);
	}

	const DsqlMapNode* mapNode1 = nodeAs<DsqlMapNode>(node1);

	if (ignoreMapCast && mapNode1)
	{
		const DsqlMapNode* mapNode2 = nodeAs<DsqlMapNode>(node2);

		if (mapNode2)
		{
			if (mapNode1->context != mapNode2->context)
				return false;

			return PASS1_node_match(dsqlScratch, mapNode1->map->map_node, mapNode2->map->map_node, ignoreMapCast);
		}

		return PASS1_node_match(dsqlScratch, mapNode1->map->map_node, node2, ignoreMapCast);
	}

	const DsqlAliasNode* aliasNode1 = nodeAs<DsqlAliasNode>(node1);
	const DsqlAliasNode* aliasNode2 = nodeAs<DsqlAliasNode>(node2);

	// We don't care about the alias itself but only about its field.
	if (aliasNode1 || aliasNode2)
	{
		if (aliasNode1 && aliasNode2)
			return PASS1_node_match(dsqlScratch, aliasNode1->value, aliasNode2->value, ignoreMapCast);

		if (aliasNode1)
			return PASS1_node_match(dsqlScratch, aliasNode1->value, node2, ignoreMapCast);

		if (aliasNode2)
			return PASS1_node_match(dsqlScratch, node1, aliasNode2->value, ignoreMapCast);
	}

	// Handle derived fields.

	const DerivedFieldNode* derivedField1 = nodeAs<DerivedFieldNode>(node1);
	const DerivedFieldNode* derivedField2 = nodeAs<DerivedFieldNode>(node2);

	if (derivedField1 || derivedField2)
	{
		if (derivedField1 && derivedField2)
		{
			if (derivedField1->context->ctx_context != derivedField2->context->ctx_context ||
				derivedField1->name != derivedField2->name)
			{
				return false;
			}

			return PASS1_node_match(dsqlScratch, derivedField1->value, derivedField2->value, ignoreMapCast);
		}

		if (derivedField1)
			return PASS1_node_match(dsqlScratch, derivedField1->value, node2, ignoreMapCast);

		if (derivedField2)
			return PASS1_node_match(dsqlScratch, node1, derivedField2->value, ignoreMapCast);
	}

	if (ignoreMapCast &&
		(pass1_raw_field_matches_resolved_field(dsqlScratch,
				nodeAs<FieldNode>(node1), nodeAs<FieldNode>(node2)) ||
		 pass1_raw_field_matches_resolved_field(dsqlScratch,
			 nodeAs<FieldNode>(node2), nodeAs<FieldNode>(node1))))
	{
		return true;
	}

	return node1->getType() == node2->getType() && node1->dsqlMatch(dsqlScratch, node2, ignoreMapCast);
}


/**

 	PASS1_cursor_name

    @brief	Find a cursor.


    @param dsqlScratch
    @param name
	@param mask
	@param existence_flag

 **/
DeclareCursorNode* PASS1_cursor_name(DsqlCompilerScratch* dsqlScratch, const MetaName& name,
	USHORT mask, bool existence_flag)
{
	DeclareCursorNode* cursor = NULL;

	if (name.isEmpty())
	{
		if (existence_flag)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
					  Arg::Gds(isc_dsql_cursor_err) <<
					  Arg::Gds(isc_dsql_cursor_invalid));
		}
		else
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
					  Arg::Gds(isc_dsql_decl_err) <<
					  Arg::Gds(isc_dsql_cursor_invalid));
		}
	}

	for (Array<DeclareCursorNode*>::iterator itr = dsqlScratch->cursors.begin();
		 itr != dsqlScratch->cursors.end();
		 ++itr)
	{
		cursor = *itr;
		if (cursor->dsqlName == name && (cursor->dsqlCursorType & mask))
			break;
		cursor = NULL;
	}

	if (!cursor && existence_flag)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-504) <<
				  Arg::Gds(isc_dsql_cursor_err) <<
				  Arg::Gds(isc_dsql_cursor_not_found) << name.toQuotedString());
	}
	else if (cursor && !existence_flag)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-502) <<
				  Arg::Gds(isc_dsql_decl_err) <<
				  Arg::Gds(isc_dsql_cursor_exists) << name.toQuotedString());
	}

	return cursor;
}


// Extract relation and procedure context and expand derived child contexts.
void PASS1_expand_contexts(DsqlContextStack& contexts, dsql_ctx* context)
{
	//// TODO: LocalTableSourceNode
	if (context->ctx_relation || context->ctx_procedure ||
		context->ctx_map || context->ctx_win_maps.hasData() || context->ctx_table_value_fun)
	{
		if (context->ctx_parent)
			context = context->ctx_parent;

		contexts.push(context);
	}
	else
	{
		for (DsqlContextStack::iterator i(context->ctx_childs_derived_table); i.hasData(); ++i)
			PASS1_expand_contexts(contexts, i.object());
	}
}


// Process derived table which is part of a from clause.
RseNode* PASS1_derived_table(DsqlCompilerScratch* dsqlScratch, SelectExprNode* input,
	const char* cte_alias, const SelectNode* select)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);

	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	const auto& alias = input->alias;

	// Create the context now, because we need to know it for the tables inside.
	dsql_ctx* const context = PASS1_make_context(dsqlScratch, input);

	// Save some values to restore after rse process.
	DsqlContextStack* const req_base = dsqlScratch->context;
	const auto aliasRelationPrefix = dsqlScratch->aliasRelationPrefix;

	// Change context, because the derived table cannot reference other streams
	// at the same scope_level (unless this is a lateral derived table).
	DsqlContextStack temp;
	// Put special contexts (NEW/OLD) also on the stack
	for (DsqlContextStack::iterator stack(*dsqlScratch->context); stack.hasData(); ++stack)
	{
		dsql_ctx* local_context = stack.object();

		if ((context->ctx_flags & CTX_lateral) ||
			(local_context->ctx_scope_level < dsqlScratch->scopeLevel) ||
			(local_context->ctx_flags & CTX_system))
		{
			temp.push(local_context);
		}
	}

	dsql_ctx* baseContext = NULL;

	if (temp.hasData())
		baseContext = temp.object();

	dsqlScratch->context = &temp;

	if (alias.hasData())
		dsqlScratch->aliasRelationPrefix.add(QualifiedName(alias));

	RecordSourceNode* query = input->querySpec;
	UnionSourceNode* unionQuery = nodeAs<UnionSourceNode>(query);
	RseNode* rse = NULL;
	const bool isRecursive = unionQuery && unionQuery->recursive;
	USHORT recursive_map_ctx = 0;

	if (isRecursive)
	{
		// Create dummy, non-recursive select dsqlScratch by doing a union of
		// one, non-recursive member. The dummy will be replaced at the end
		// of this function.

		RecordSourceNode* save = unionQuery->dsqlClauses->items.pop();
		unionQuery->recursive = false;

		dsql_ctx* baseUnionCtx = dsqlScratch->unionContext.hasData() ?
			dsqlScratch->unionContext.object() : NULL;

		// reserve extra context number for map's secondary context
		recursive_map_ctx = dsqlScratch->contextNumber++;

		dsqlScratch->recursiveCtxId = dsqlScratch->contextNumber;
		rse = pass1_union(dsqlScratch, unionQuery, NULL, NULL, false, false, 0);
		dsqlScratch->contextNumber = dsqlScratch->recursiveCtxId + 1;

		// recursive union always has exactly 2 members
		unionQuery->dsqlClauses->items.push(save);
		unionQuery->recursive = true;

		while (dsqlScratch->unionContext.hasData() &&
			   dsqlScratch->unionContext.object() != baseUnionCtx)
		{
			dsqlScratch->unionContext.pop();
		}
	}
	else
	{
		// AB: 2005-01-06
		// If our derived table contains a single query with a sub-select buried
		// inside the select items then we need a special handling, because we don't
		// want creating a new sub-select for every reference outside the derived
		// table to that sub-select.
		// To handle this we simple create a UNION ALL with derived table inside it.
		// Due this mappings are created and we simple reference to these mappings.
		// Optimizer effects:
		//   Good thing is that only 1 recordstream is made for the sub-select, but
		//   the worse thing is that a UNION currently can't be used in
		//   deciding the JOIN order.
		bool foundSubSelect = false;
		if (auto queryNode = nodeAs<RseNode>(query))
			foundSubSelect = SubSelectFinder::find(dsqlScratch->getPool(), queryNode->dsqlSelectList);

		if (foundSubSelect)
		{
			UnionSourceNode* unionExpr = FB_NEW_POOL(pool) UnionSourceNode(pool);
			unionExpr->dsqlClauses = FB_NEW_POOL(pool) RecSourceListNode(pool, 1);
			unionExpr->dsqlClauses->items[0] = input;
			unionExpr->dsqlAll = true;
			rse = pass1_union(dsqlScratch, unionExpr, NULL, NULL, false, false, 0);
		}
		else
			rse = PASS1_rse(dsqlScratch, input, select);

		// Finish off by cleaning up contexts and put them into derivedContext
		// so create view (ddl) can deal with it.
		// Also add the used contexts into the childs stack.
		while (temp.hasData() && (temp.object() != baseContext))
		{
			dsql_ctx* childCtx = temp.pop();

			dsqlScratch->derivedContext.push(childCtx);
			context->ctx_childs_derived_table.push(childCtx);

			// Collect contexts that will be used for blr_derived_expr generation.
			PASS1_expand_contexts(context->ctx_main_derived_contexts, childCtx);
		}

		while (temp.hasData())
			temp.pop();
	}

	context->ctx_rse = rse;

	// CVC: prepare a truncated alias for the derived table here
	// because we need it several times.
	string aliasname;

	if (alias.hasData())
		aliasname = alias;
	else
		aliasname = "<unnamed>";

	// If an alias-list is specified, process it.

	const bool ignoreColumnChecks =
		(input->dsqlFlags & RecordSourceNode::DFLAG_DT_IGNORE_COLUMN_CHECK);

	if (input->columns && input->columns->hasData())
	{
		// Do both lists have the same number of items?
		if (input->columns->getCount() != rse->dsqlSelectList->items.getCount())
		{
			// Column list by derived table %s [alias-name] has %s [more/fewer] columns
			// than the number of items.

			int errcode = isc_dsql_derived_table_less_columns;
			if (input->columns->getCount() > rse->dsqlSelectList->items.getCount())
				errcode = isc_dsql_derived_table_more_columns;

			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(errcode) << Arg::Str(aliasname));
		}

		// Generate derived fields and assign alias-name to them.
		for (FB_SIZE_T count = 0; count < input->columns->getCount(); ++count)
		{
			ValueExprNode* select_item = rse->dsqlSelectList->items[count];
			DsqlDescMaker::fromNode(dsqlScratch, select_item);

			// Make new derived field node.

			DerivedFieldNode* derivedField = FB_NEW_POOL(pool) DerivedFieldNode(pool,
				(*input->columns)[count], dsqlScratch->scopeLevel, select_item);
			derivedField->setDsqlDesc(select_item->getDsqlDesc());
			rse->dsqlSelectList->items[count] = derivedField;
		}
	}
	else
	{
		// For those select-items where no alias is specified try
		// to generate one from the field_name.
		for (FB_SIZE_T count = 0; count < rse->dsqlSelectList->items.getCount(); ++count)
		{
			ValueExprNode* select_item = pass1_make_derived_field(tdbb, dsqlScratch,
				rse->dsqlSelectList->items[count]);

			// Auto-create dummy column name for pass1_any()
			if (ignoreColumnChecks && !nodeIs<DerivedFieldNode>(select_item))
			{
				DsqlDescMaker::fromNode(dsqlScratch, select_item);

				// Construct dummy fieldname
				char fieldname[25];
				snprintf(fieldname, sizeof(fieldname), "f%u", static_cast<unsigned>(count));

				// Make new derived field node.

				DerivedFieldNode* derivedField = FB_NEW_POOL(pool) DerivedFieldNode(pool,
					fieldname, dsqlScratch->scopeLevel, select_item);
				derivedField->setDsqlDesc(select_item->getDsqlDesc());
				select_item = derivedField;
			}

			rse->dsqlSelectList->items[count] = select_item;
		}
	}

	FB_SIZE_T count;

	// Check if all root select-items have a derived field else show a message.
	for (count = 0; count < rse->dsqlSelectList->items.getCount(); ++count)
	{
		ValueExprNode* select_item = rse->dsqlSelectList->items[count];
		DerivedFieldNode* derivedField;

		if ((derivedField = nodeAs<DerivedFieldNode>(select_item)))
			derivedField->context = context;
		else
		{
			// no column name specified for column number %d in derived table %s

			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_dsql_derived_field_unnamed) << Arg::Num(count + 1) <<
																  Arg::Str(aliasname));
		}
	}

	// Check for ambiguous column names inside this derived table.
	for (count = 0; count < rse->dsqlSelectList->items.getCount(); ++count)
	{
		const DerivedFieldNode* selectItem1 = nodeAs<DerivedFieldNode>(rse->dsqlSelectList->items[count]);

		for (FB_SIZE_T count2 = (count + 1); count2 < rse->dsqlSelectList->items.getCount(); ++count2)
		{
			const DerivedFieldNode* selectItem2 = nodeAs<DerivedFieldNode>(rse->dsqlSelectList->items[count2]);

			if (selectItem1->name == selectItem2->name)
			{
				// column %s was specified multiple times for derived table %s
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_command_err) <<
						  Arg::Gds(isc_dsql_derived_field_dup_name) << selectItem1->name <<
								aliasname);
			}
		}
	}

	// If we used a dummy rse before, replace it with the real one now.
	// We cannot do this earlier, because recursive processing needs a fully
	// developed context block.
	if (isRecursive)
	{
		dsql_ctx* const saveRecursiveCtx = dsqlScratch->recursiveCtx;
		dsqlScratch->recursiveCtx = context;
		dsqlScratch->context = &temp;

		const string* const saveCteAlias =
			dsqlScratch->currCteAlias ? *dsqlScratch->currCteAlias : NULL;
		dsqlScratch->resetCTEAlias(alias.c_str());

		rse = PASS1_rse(dsqlScratch, input, select);

		if (saveCteAlias)
			dsqlScratch->resetCTEAlias(*saveCteAlias);
		dsqlScratch->recursiveCtx = saveRecursiveCtx;

		// Finish off by cleaning up contexts and put them into derivedContext
		// so create view (ddl) can deal with it.
		// Also add the used contexts into the childs stack.
		while (temp.hasData() && (temp.object() != baseContext))
		{
			dsqlScratch->derivedContext.push(temp.object());
			context->ctx_childs_derived_table.push(temp.pop());
		}

		temp.clear();

		rse->dsqlSelectList = context->ctx_rse->dsqlSelectList;

		context->ctx_rse = rse;

		if (cte_alias)
		{
			context->ctx_alias.clear();
			context->ctx_alias.push(QualifiedName(cte_alias));
		}

		dsqlScratch->context = req_base;

		// Mark union's map context as recursive and assign secondary context number to it.
		ValueListNode* items = rse->dsqlSelectList;
		ValueExprNode* map_item = items->items[0];
		DerivedFieldNode* derivedField;

		if ((derivedField = nodeAs<DerivedFieldNode>(map_item)))
			map_item = derivedField->value;

		dsql_ctx* map_context = nodeAs<DsqlMapNode>(map_item)->context;

		map_context->ctx_flags |= CTX_recursive;
		map_context->ctx_recursive = recursive_map_ctx;
	}

	// Restore our original values.
	dsqlScratch->context = req_base;
	dsqlScratch->aliasRelationPrefix = aliasRelationPrefix;

	rse->dsqlContext = context;

	return rse;
}


/**

	PASS1_expand_select_list

	@brief	Expand asterisk nodes into fields.

	@param dsqlScratch
	@param list
	@param streams

 **/
ValueListNode* PASS1_expand_select_list(DsqlCompilerScratch* dsqlScratch, ValueListNode* list,
	RecSourceListNode* streams)
{
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	ValueListNode* retList = FB_NEW_POOL(pool) ValueListNode(pool, 0u);

	if (list)
	{
		for (NestConst<ValueExprNode>* ptr = list->items.begin(); ptr != list->items.end(); ++ptr)
			PASS1_expand_select_node(dsqlScratch, *ptr, retList, true);
	}
	else
	{
		for (NestConst<RecordSourceNode>* ptr = streams->items.begin(); ptr != streams->items.end(); ++ptr)
			PASS1_expand_select_node(dsqlScratch, *ptr, retList, true);
	}

	return retList;
}

// Expand a select item node.
void PASS1_expand_select_node(DsqlCompilerScratch* dsqlScratch, ExprNode* node, ValueListNode* list,
	bool hide_using)
{
	FieldNode* fieldNode;

	//// TODO: LocalTableSourceNode
	if (auto rseNode = nodeAs<RseNode>(node))
	{
		ValueListNode* sub_items = rseNode->dsqlSelectList;

		if (sub_items)	// AB: Derived table support
		{
			NestConst<ValueExprNode>* ptr = sub_items->items.begin();

			for (const NestConst<ValueExprNode>* const end = sub_items->items.end(); ptr != end; ++ptr)
			{
				// Create a new alias else mappings would be mangled.
				NestConst<ValueExprNode> select_item = *ptr;

				// select-item should always be a derived field!

				DerivedFieldNode* derivedField;

				if (!(derivedField = nodeAs<DerivedFieldNode>(select_item)))
				{
					// Internal dsql error: alias type expected by PASS1_expand_select_node
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  Arg::Gds(isc_dsql_command_err) <<
							  Arg::Gds(isc_dsql_derived_alias_select));
				}

				dsql_ctx* context = derivedField->context;
				DEV_BLKCHK(context, dsql_type_ctx);

				if (!hide_using || context->getImplicitJoinField(derivedField->name, select_item))
					list->add(select_item);
			}
		}
		else	// joins
		{
			RecSourceListNode* streamList = rseNode->dsqlStreams;

			for (NestConst<RecordSourceNode>* ptr = streamList->items.begin();
				 ptr != streamList->items.end();
				 ++ptr)
			{
				PASS1_expand_select_node(dsqlScratch, *ptr, list, true);
			}
		}
	}
	else if (auto procNode = nodeAs<ProcedureSourceNode>(node))
	{
		dsql_ctx* context = procNode->dsqlContext;

		if (context->ctx_procedure)
		{
			for (dsql_fld* field = context->ctx_procedure->prc_outputs; field; field = field->fld_next)
			{
				DEV_BLKCHK(field, dsql_type_fld);

				NestConst<ValueExprNode> select_item = NULL;
				if (!hide_using || context->getImplicitJoinField(field->fld_name, select_item))
				{
					if (!select_item)
						select_item = MAKE_field(context, field, NULL);
					list->add(select_item);
				}
			}
		}
	}
	else if (auto relNode = nodeAs<RelationSourceNode>(node))
	{
		dsql_ctx* context = relNode->dsqlContext;

		if (context->ctx_relation)
		{
			for (dsql_fld* field = context->ctx_relation->rel_fields; field; field = field->fld_next)
			{
				DEV_BLKCHK(field, dsql_type_fld);

				NestConst<ValueExprNode> select_item = NULL;

				if (!hide_using || context->getImplicitJoinField(field->fld_name, select_item))
				{
					if (!select_item)
					{
						if (context->ctx_flags & CTX_null)
							select_item = NullNode::instance();
						else
							select_item = MAKE_field(context, field, NULL);
					}

					list->add(select_item);
				}
			}
		}
	}
	else if ((fieldNode = nodeAs<FieldNode>(node)))
	{
		RecordSourceNode* recSource = NULL;
		ValueExprNode* value = fieldNode->internalDsqlPass(dsqlScratch, &recSource);

		if (recSource)
			PASS1_expand_select_node(dsqlScratch, recSource, list, false);
		else
			list->add(value);
	}
	else if (auto tableValueFunctionNode = nodeAs<TableValueFunctionSourceNode>(node))
	{
		auto context = tableValueFunctionNode->dsqlContext;

		if (const auto tableValueFunctionContext = context->ctx_table_value_fun)
		{
			tableValueFunctionNode->setDefaultNameField(dsqlScratch);

			for (dsql_fld* field = tableValueFunctionContext->outputField; field; field = field->fld_next)
			{
				DEV_BLKCHK(field, dsql_type_fld);
				NestConst<ValueExprNode> select_item = NULL;
				if (!hide_using || context->getImplicitJoinField(field->fld_name, select_item))
				{
					if (!select_item)
						select_item = MAKE_field(context, field, NULL);
					list->add(select_item);
				}
			}
		}
	}
	else
	{
		fb_assert(node->getKind() == DmlNode::KIND_VALUE);
		list->add(static_cast<ValueExprNode*>(node));
	}
}


/**

 	pass1_group_by_list

    @brief	Process GROUP BY list, which may contain
			an ordinal or alias which references the
			select list.

    @param dsqlScratch
    @param input
    @param select_list

 **/
static ValueListNode* pass1_group_by_list(DsqlCompilerScratch* dsqlScratch, ValueListNode* input,
	ValueListNode* selectList)
{
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	DEV_BLKCHK(dsqlScratch, dsql_type_req);

	if (input->items.getCount() > MAX_SORT_ITEMS) // sort, group and distinct have the same limit for now
	{
		// cannot group on more than 255 items
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_command_err) <<
				  Arg::Gds(isc_dsql_max_group_items));
	}

	ValueListNode* retList = FB_NEW_POOL(pool) ValueListNode(pool, 0u);

	NestConst<ValueExprNode>* ptr = input->items.begin();
	for (const NestConst<ValueExprNode>* const end = input->items.end(); ptr != end; ++ptr)
	{
		ValueExprNode* sub = (*ptr);
		ValueExprNode* frnode = NULL;
		FieldNode* field;
		LiteralNode* literal;

		if ((field = nodeAs<FieldNode>(sub)))
		{
			// check for alias or field node
			if (selectList && field->dsqlQualifier.object.isEmpty() && field->dsqlName.hasData())
			{
				// AB: Check first against the select list for matching column.
				// When no matches at all are found we go on with our
				// normal way of field name lookup.
				frnode = PASS1_lookup_alias(dsqlScratch, field->dsqlName, selectList, true);
			}

			if (!frnode)
				frnode = field->internalDsqlPass(dsqlScratch, NULL);
		}
		else if ((literal = nodeAs<LiteralNode>(sub)) && (literal->litDesc.dsc_dtype == dtype_long))
		{
			const ULONG position = literal->getSlong();

			if (position < 1 || !selectList || position > (ULONG) selectList->items.getCount())
			{
				// Invalid column position used in the GROUP BY clause
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_column_pos_err) << Arg::Str("GROUP BY"));
			}

			frnode = Node::doDsqlPass(dsqlScratch, selectList->items[position - 1], false);
		}
		else
			frnode = Node::doDsqlPass(dsqlScratch, *ptr, false);

		retList->add(frnode);
	}

	// Finally make the complete list.
	return retList;
}


namespace
{
	constexpr unsigned MAX_GROUPING_SETS = 4096;

	class ActiveGroupingContextDisabler
	{
	public:
		explicit ActiveGroupingContextDisabler(DsqlCompilerScratch* dsqlScratch)
			: autoGroupingSpec(&dsqlScratch->activeGroupingSpec, NULL),
			  autoGroupingSet(&dsqlScratch->activeGroupingSet, NULL),
			  autoGroupingSelectList(&dsqlScratch->activeGroupingSelectList, NULL),
			  autoGroupingScopeLevel(&dsqlScratch->activeGroupingScopeLevel, 0)
		{
		}

	private:
		AutoSetRestore<GroupingSpec*> autoGroupingSpec;
		AutoSetRestore<const SortedArray<unsigned>*> autoGroupingSet;
		AutoSetRestore<ValueListNode*> autoGroupingSelectList;
		AutoSetRestore<USHORT> autoGroupingScopeLevel;
	};

	ValueExprNode* pass1_effective_grouping_item(ValueExprNode* node)
	{
		if (auto aliasNode = nodeAs<DsqlAliasNode>(node))
			return aliasNode->value;

		return node;
	}

	bool pass1_select_list_has_asterisk(ValueListNode* selectList)
	{
		if (!selectList)
			return true;

		for (auto item = selectList->items.begin(); item != selectList->items.end(); ++item)
		{
			if (auto fieldNode = nodeAs<FieldNode>(*item))
			{
				if (!fieldNode->dsqlName.hasData())
					return true;
			}
		}

		return false;
	}

	void pass1_post_ambiguous_select_list_name(const MetaName& name)
	{
		TEXT buffer1[] = "an item";
		TEXT buffer2[] = "an item in the select list with name";

		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
				  Arg::Gds(isc_dsql_ambiguous_field_name) << Arg::Str(buffer1) <<
															 Arg::Str(buffer2) <<
				  Arg::Gds(isc_random) << name);
	}

	bool pass1_raw_qualifier_matches_context(const QualifiedName& qualifier, const dsql_ctx* context);

	ValueExprNode* pass1_resolve_raw_grouping_item(ValueExprNode* node, ValueListNode* selectList,
		bool resolvePositions = true)
	{
		if (!node || !selectList)
			return node;

		if (auto fieldNode = nodeAs<FieldNode>(node))
		{
			if (fieldNode->dsqlName.hasData())
			{
				ValueExprNode* found = NULL;

				for (auto item = selectList->items.begin(); item != selectList->items.end(); ++item)
				{
					ValueExprNode* selectItem = *item;
					bool matched = false;

					if (fieldNode->dsqlQualifier.object.isEmpty())
					{
						if (auto aliasNode = nodeAs<DsqlAliasNode>(selectItem))
						{
							if (aliasNode->name == fieldNode->dsqlName)
							{
								selectItem = aliasNode->value;
								matched = true;
							}
						}
						else if (auto selectField = nodeAs<FieldNode>(selectItem))
						{
							matched = selectField->dsqlQualifier.object.isEmpty() &&
								selectField->dsqlName == fieldNode->dsqlName;
						}
					}

					if (!matched)
					{
						if (auto derivedField = nodeAs<DerivedFieldNode>(selectItem))
						{
							matched = derivedField->name == fieldNode->dsqlName &&
								pass1_raw_qualifier_matches_context(fieldNode->dsqlQualifier,
									derivedField->context);
						}
					}

					if (matched)
					{
						if (found)
							pass1_post_ambiguous_select_list_name(fieldNode->dsqlName);

						found = selectItem;
					}
				}

				if (found)
					return found;
			}
		}
		else if (resolvePositions)
		{
			if (auto literal = nodeAs<LiteralNode>(node))
			{
				if (literal->litDesc.dsc_dtype == dtype_long)
				{
					const ULONG position = literal->getSlong();

					if (position < 1 || position > (ULONG) selectList->items.getCount())
					{
						ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
								  Arg::Gds(isc_dsql_column_pos_err) << Arg::Str("GROUP BY"));
					}

					return pass1_effective_grouping_item(selectList->items[position - 1]);
				}
			}
		}

		return node;
	}

	void tooManyGroupingSets()
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_command_err) <<
				  Arg::Gds(isc_random) << Arg::Str("Too many grouping sets"));
	}

	bool groupingSetMatches(const GroupingSpec::Set& left, const GroupingSpec::Set& right)
	{
		if (left.dimensions.getCount() != right.dimensions.getCount())
			return false;

		auto l = left.dimensions.begin();

		for (auto r = right.dimensions.begin(); r != right.dimensions.end(); ++r, ++l)
		{
			if (*l != *r)
				return false;
		}

		return true;
	}

	void mergeGroupingSet(GroupingSpec::Set& target, const GroupingSpec::Set& source)
	{
		for (auto dimension = source.dimensions.begin(); dimension != source.dimensions.end(); ++dimension)
			target.dimensions.addUniq(*dimension);
	}

	GroupingSpec::Set& addScratchGroupingSet(ObjectsArray<GroupingSpec::Set>& sets,
		const GroupingSpec::Set& set)
	{
		if (sets.getCount() >= MAX_GROUPING_SETS)
			tooManyGroupingSets();

		const auto pos = sets.add(set);
		return sets[pos];
	}

	class GroupingSpecBuilder
	{
	public:
		GroupingSpecBuilder(DsqlCompilerScratch* aDsqlScratch, MemoryPool& aPool,
				ValueListNode* aSelectList)
			: dsqlScratch(aDsqlScratch),
			  pool(aPool),
			  selectList(aSelectList),
			  resolvePositions(!pass1_select_list_has_asterisk(aSelectList)),
			  spec(NULL)
		{
		}

		GroupingSpec* build(GroupingClause* clause)
		{
			fb_assert(clause && clause->hasAdvancedGrouping());

			spec = FB_NEW_POOL(pool) GroupingSpec(pool);
			spec->duplicateMode = clause->duplicateMode;

			ObjectsArray<GroupingSpec::Set> sets(pool);
			expandClause(clause, sets);

			for (const auto& set : sets)
				addResultSet(set);

			return spec;
		}

	private:
		unsigned addDimension(ValueExprNode* expr)
		{
			for (const auto& dimension : spec->dimensions)
			{
				if (PASS1_node_match(dsqlScratch, dimension.expr, expr, true))
					return dimension.index;
			}

			if (spec->dimensions.getCount() >= MAX_SORT_ITEMS)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_command_err) <<
						  Arg::Gds(isc_dsql_max_group_items));
			}

			auto& dimension = spec->dimensions.add();
			dimension.index = spec->dimensions.getCount() - 1;
			dimension.expr = expr;

			return dimension.index;
		}

		void addItems(GroupingSpec::Set& set, ValueListNode* items)
		{
			if (!items)
				return;

			for (auto item = items->items.begin(); item != items->items.end(); ++item)
				set.dimensions.addUniq(addDimension(
					pass1_resolve_raw_grouping_item(*item, selectList, resolvePositions)));
		}

		void addResultSet(const GroupingSpec::Set& set)
		{
			if (spec->duplicateMode == GroupingClause::DuplicateMode::DISTINCT)
			{
				for (const auto& existingSet : spec->sets)
				{
					if (groupingSetMatches(existingSet, set))
						return;
				}
			}

			if (spec->sets.getCount() >= MAX_GROUPING_SETS)
				tooManyGroupingSets();

			const auto pos = spec->sets.add(set);
			spec->sets[pos].ordinal = pos;
		}

		void expandClause(GroupingClause* clause, ObjectsArray<GroupingSpec::Set>& output)
		{
			GroupingSpec::Set empty(pool);
			addScratchGroupingSet(output, empty);

			for (auto& element : clause->elements)
			{
				ObjectsArray<GroupingSpec::Set> elementSets(pool);
				expandElement(element, elementSets);

				ObjectsArray<GroupingSpec::Set> combined(pool);

				for (const auto& currentSet : output)
				{
					for (const auto& elementSet : elementSets)
					{
						GroupingSpec::Set merged(pool, currentSet);
						mergeGroupingSet(merged, elementSet);
						addScratchGroupingSet(combined, merged);
					}
				}

				output.clear();

				for (const auto& set : combined)
					addScratchGroupingSet(output, set);
			}
		}

		void expandElement(GroupingClause::Element& element,
			ObjectsArray<GroupingSpec::Set>& output)
		{
			switch (element.type)
			{
				case GroupingClause::Element::Type::SIMPLE:
				{
					GroupingSpec::Set set(pool);
					addItems(set, element.items);
					addScratchGroupingSet(output, set);
					break;
				}

				case GroupingClause::Element::Type::EMPTY:
				{
					GroupingSpec::Set set(pool);
					addScratchGroupingSet(output, set);
					break;
				}

				case GroupingClause::Element::Type::ROLLUP:
					expandRollup(element.groupingSets, output);
					break;

				case GroupingClause::Element::Type::CUBE:
					expandCube(element.groupingSets, output);
					break;

				case GroupingClause::Element::Type::GROUPING_SETS:
					expandGroupingSets(element.groupingSets, output);
					break;
			}
		}

		void expandGroupingSets(GroupingClause* clause, ObjectsArray<GroupingSpec::Set>& output)
		{
			fb_assert(clause);

			for (auto& element : clause->elements)
			{
				ObjectsArray<GroupingSpec::Set> elementSets(pool);
				expandElement(element, elementSets);

				for (const auto& set : elementSets)
					addScratchGroupingSet(output, set);
			}
		}

		void expandUnitList(GroupingClause* clause, ObjectsArray<GroupingSpec::Set>& units)
		{
			fb_assert(clause);

			for (auto& element : clause->elements)
			{
				ObjectsArray<GroupingSpec::Set> elementSets(pool);
				expandElement(element, elementSets);

				for (const auto& set : elementSets)
					addScratchGroupingSet(units, set);
			}
		}

		void expandRollup(GroupingClause* clause, ObjectsArray<GroupingSpec::Set>& output)
		{
			ObjectsArray<GroupingSpec::Set> units(pool);
			expandUnitList(clause, units);

			for (FB_SIZE_T count = units.getCount();; --count)
			{
				GroupingSpec::Set set(pool);

				for (FB_SIZE_T i = 0; i < count; ++i)
					mergeGroupingSet(set, units[i]);

				addScratchGroupingSet(output, set);

				if (count == 0)
					break;
			}
		}

		void expandCube(GroupingClause* clause, ObjectsArray<GroupingSpec::Set>& output)
		{
			ObjectsArray<GroupingSpec::Set> units(pool);
			expandUnitList(clause, units);

			GroupingSpec::Set empty(pool);
			expandCube(units, 0, empty, output);
		}

		void expandCube(const ObjectsArray<GroupingSpec::Set>& units, FB_SIZE_T index,
			const GroupingSpec::Set& current, ObjectsArray<GroupingSpec::Set>& output)
		{
			if (index == units.getCount())
			{
				addScratchGroupingSet(output, current);
				return;
			}

			GroupingSpec::Set withUnit(pool, current);
			mergeGroupingSet(withUnit, units[index]);
			expandCube(units, index + 1, withUnit, output);
			expandCube(units, index + 1, current, output);
		}

	private:
		DsqlCompilerScratch* dsqlScratch;
		MemoryPool& pool;
		ValueListNode* selectList;
		bool resolvePositions;
		GroupingSpec* spec;
	};
}


static GroupingSpec* pass1_grouping_spec_raw(DsqlCompilerScratch* dsqlScratch,
	GroupingClause* groupingClause, ValueListNode* selectList)
{
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	GroupingSpecBuilder builder(dsqlScratch, pool, selectList);
	return builder.build(groupingClause);
}


namespace
{
	constexpr FB_SIZE_T MAX_GROUPING_ID_ARGS = 63;

	bool pass1_find_grouping_dimension_match(DsqlCompilerScratch* dsqlScratch, GroupingSpec* spec,
		ValueListNode* selectList, ValueExprNode* node, const SortedArray<unsigned>* set,
		unsigned& index, bool& present, bool resolveDsql, bool resolveNodePosition,
		bool resolveDimensionPosition);

	bool pass1_find_raw_grouping_dimension(DsqlCompilerScratch* dsqlScratch, GroupingSpec* spec,
		ValueListNode* selectList, ValueExprNode* node, unsigned& index, bool resolveDsql = true);

	ValueExprNode* pass1_make_grouping_id_value(DsqlCompilerScratch* dsqlScratch,
		GroupingSpec* spec, const SortedArray<unsigned>* groupingSet, ValueListNode* selectList,
		GroupingIdNode* groupingIdNode, bool resolveDsql);

	void postGroupingValidationError(const char* message)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_command_err) <<
				  Arg::Gds(isc_random) << Arg::Str(message));
	}

	void postGroupingValidationError(const string& message)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_command_err) <<
				  Arg::Gds(isc_random) << Arg::Str(message));
	}

	class GroupingExpressionValidator
	{
	public:
		GroupingExpressionValidator(DsqlCompilerScratch* aDsqlScratch, GroupingSpec* aSpec,
				const SortedArray<unsigned>* aGroupingSet, ValueListNode* aSelectList,
				const char* aMissingContextMessage)
			: dsqlScratch(aDsqlScratch),
			  spec(aSpec),
			  groupingSet(aGroupingSet),
			  selectList(aSelectList),
			  missingContextMessage(aMissingContextMessage)
		{
		}

		void visit(ExprNode* node)
		{
			visit(&node);
		}

	private:
		void visit(ExprNode** nodePtr)
		{
			if (!nodePtr || !*nodePtr)
				return;

			ExprNode* node = *nodePtr;

			if (GroupingNode* groupingNode = nodeAs<GroupingNode>(node))
			{
				validateGrouping(groupingNode);

				if (groupingSet)
				{
					unsigned dimension = 0;
					bool present = false;

					if (pass1_find_grouping_dimension_match(dsqlScratch, spec, selectList,
							groupingNode->arg, groupingSet, dimension, present, true, true, true))
					{
						*nodePtr = MAKE_const_slong(present ? 0 : 1);
						return;
					}
				}
			}
			else if (GroupingIdNode* groupingIdNode = nodeAs<GroupingIdNode>(node))
			{
				validateGroupingId(groupingIdNode);

				if (groupingSet)
				{
					*nodePtr = pass1_make_grouping_id_value(dsqlScratch, spec, groupingSet,
						selectList, groupingIdNode, true);
					return;
				}
			}

			if (nodeIs<SubQueryNode>(node))
				return;

			NodeRefsHolder holder(dsqlScratch->getPool());
			node->getChildren(holder, true);

			for (auto child : holder.refs)
				visit(child);
		}

		void validateGrouping(GroupingNode* groupingNode)
		{
			if (!spec)
			{
				postGroupingValidationError(missingContextMessage);
				return;
			}

			unsigned dimension = 0;

			if (pass1_find_raw_grouping_dimension(dsqlScratch, spec, selectList,
					groupingNode->arg, dimension))
			{
				return;
			}

			postGroupingValidationError("GROUPING argument must match a grouping expression");
		}

		void validateGroupingId(GroupingIdNode* groupingIdNode)
		{
			const char* functionName = groupingIdNode->getDsqlFunctionName();

			if (!spec)
			{
				postGroupingValidationError(missingContextMessage);
				return;
			}

			if (!groupingIdNode->args || groupingIdNode->args->items.isEmpty())
				postGroupingValidationError(string(functionName) +
					" requires at least one argument");

			if (groupingIdNode->args->items.getCount() > MAX_GROUPING_ID_ARGS)
				postGroupingValidationError(string(functionName) +
					" supports at most 63 arguments");

			for (auto arg = groupingIdNode->args->items.begin();
				 arg != groupingIdNode->args->items.end();
				 ++arg)
			{
				unsigned dimension = 0;

				if (!pass1_find_raw_grouping_dimension(dsqlScratch, spec, selectList, *arg,
						dimension))
				{
					postGroupingValidationError(string(functionName) +
						" argument must match a grouping expression");
				}
			}
		}

	private:
		DsqlCompilerScratch* dsqlScratch;
		GroupingSpec* spec;
		const SortedArray<unsigned>* groupingSet;
		ValueListNode* selectList;
		const char* missingContextMessage;
	};
}


static void pass1_validate_grouping_expressions(DsqlCompilerScratch* dsqlScratch,
	GroupingSpec* groupingSpec, const SortedArray<unsigned>* groupingSet, ValueListNode* selectList,
	ExprNode* node, const char* missingContextMessage)
{
	GroupingExpressionValidator(dsqlScratch, groupingSpec, groupingSet, selectList,
		missingContextMessage).visit(node);
}


namespace
{
	bool pass1_grouping_set_contains(const GroupingSpec::Set& set, unsigned dimension)
	{
		return set.dimensions.exist(dimension);
	}

	bool pass1_grouping_set_contains(const SortedArray<unsigned>& set, unsigned dimension)
	{
		return set.exist(dimension);
	}

	void pass1_copy_desc_to_field(dsql_fld* field, const dsc& desc)
	{
		field->dtype = desc.dsc_dtype;
		field->scale = desc.dsc_scale;
		field->subType = desc.dsc_sub_type;
		field->length = desc.dsc_length;
		field->flags = FLD_nullable;

		if (desc.isText() || desc.isBlob())
		{
			field->textType = desc.getTextType();
			field->charSetId = desc.getCharSet();
			field->collationId = desc.getCollation();
		}
	}

	ValueExprNode* pass1_make_typed_null(const dsc& desc)
	{
		thread_db* tdbb = JRD_get_thread_data();
		MemoryPool& pool = *tdbb->getDefaultPool();

		CastNode* castNode = FB_NEW_POOL(pool) CastNode(pool);
		castNode->source = NullNode::instance();
		castNode->dsqlField = FB_NEW_POOL(pool) dsql_fld(pool);
		pass1_copy_desc_to_field(castNode->dsqlField, desc);

		dsc nullDesc = desc;
		nullDesc.setNull();
		castNode->castDesc = nullDesc;
		castNode->setDsqlDesc(nullDesc);

		return castNode;
	}

	bool pass1_raw_qualifier_matches_context(const QualifiedName& qualifier, const dsql_ctx* context)
	{
		if (qualifier.object.isEmpty())
			return true;

		if (!context)
			return false;

		if (context->ctx_internal_alias.object.hasData())
			return PASS1_compare_alias(context->ctx_internal_alias, qualifier);

		if (context->ctx_relation)
			return PASS1_compare_alias(context->ctx_relation->rel_name, qualifier);

		if (context->ctx_procedure)
			return PASS1_compare_alias(context->ctx_procedure->prc_name, qualifier);

		if (context->ctx_table_value_fun)
		{
			QualifiedName functionName;
			functionName.object = context->ctx_table_value_fun->funName;
			return PASS1_compare_alias(functionName, qualifier);
		}

		return context->ctx_alias.hasData() && PASS1_compare_alias(context->ctx_alias[0], qualifier);
	}

	bool pass1_raw_field_matches_resolved_field(DsqlCompilerScratch* dsqlScratch,
		const FieldNode* rawField, const FieldNode* resolvedField)
	{
		if (!rawField || rawField->dsqlField || rawField->dsqlContext ||
			!resolvedField || !resolvedField->dsqlField ||
			!rawField->dsqlName.hasData() || rawField->dsqlName != resolvedField->dsqlField->fld_name)
		{
			return false;
		}

		if (!pass1_raw_qualifier_matches_context(rawField->dsqlQualifier, resolvedField->dsqlContext))
			return false;

		if (rawField->dsqlIndices || resolvedField->dsqlIndices)
		{
			return rawField->dsqlIndices && resolvedField->dsqlIndices &&
				PASS1_node_match(dsqlScratch, rawField->dsqlIndices,
					resolvedField->dsqlIndices, true);
		}

		return true;
	}

	ValueExprNode* pass1_resolve_grouping_dimension(GroupingSpec* spec, ValueListNode* selectList,
		unsigned index, bool resolvePositions = true)
	{
		return pass1_resolve_raw_grouping_item(
			pass1_effective_grouping_item(spec->dimensions[index].expr), selectList,
			resolvePositions);
	}

	ValueExprNode* pass1_dsql_pass_grouping_match_item(DsqlCompilerScratch* dsqlScratch,
		ValueExprNode* node)
	{
		ActiveGroupingContextDisabler disableGrouping(dsqlScratch);
		NestConst<ValueExprNode> nestNode = node;
		return Node::doDsqlPass(dsqlScratch, nestNode, false);
	}

	bool pass1_dsql_grouping_items_match(DsqlCompilerScratch* dsqlScratch,
		ValueExprNode* left, ValueExprNode* right)
	{
		ValueExprNode* dsqlLeft = pass1_dsql_pass_grouping_match_item(dsqlScratch, left);
		ValueExprNode* dsqlRight = pass1_dsql_pass_grouping_match_item(dsqlScratch, right);

		return PASS1_node_match(dsqlScratch, dsqlLeft, dsqlRight, true);
	}

	bool pass1_grouping_dimension_matches(DsqlCompilerScratch* dsqlScratch, GroupingSpec* spec,
		ValueListNode* selectList, const GroupingSpec::Dimension& dimension,
		ValueExprNode* resolvedNode, bool resolveDsql, bool resolveDimensionPosition)
	{
		ValueExprNode* dimensionExpr = pass1_resolve_grouping_dimension(spec, selectList,
			dimension.index, resolveDimensionPosition);

		if (PASS1_node_match(dsqlScratch, dimensionExpr, resolvedNode, true))
			return true;

		if (pass1_raw_field_matches_resolved_field(dsqlScratch, nodeAs<FieldNode>(dimensionExpr),
				nodeAs<FieldNode>(resolvedNode)))
		{
			return true;
		}

		return resolveDsql &&
			pass1_dsql_grouping_items_match(dsqlScratch, dimensionExpr, resolvedNode);
	}

	bool pass1_find_grouping_dimension_match(DsqlCompilerScratch* dsqlScratch, GroupingSpec* spec,
		ValueListNode* selectList, ValueExprNode* node, const SortedArray<unsigned>* set,
		unsigned& index, bool& present, bool resolveDsql, bool resolveNodePosition,
		bool resolveDimensionPosition)
	{
		ValueExprNode* resolvedNode = pass1_resolve_raw_grouping_item(
			pass1_effective_grouping_item(node), selectList, resolveNodePosition);

		bool found = false;
		present = false;

		for (auto& dimension : spec->dimensions)
		{
			if (pass1_grouping_dimension_matches(dsqlScratch, spec, selectList, dimension,
					resolvedNode, resolveDsql, resolveDimensionPosition))
			{
				if (!found)
				{
					index = dimension.index;
					found = true;
				}

				if (set && pass1_grouping_set_contains(*set, dimension.index))
				{
					index = dimension.index;
					present = true;
				}
			}
		}

		return found;
	}

	bool pass1_find_raw_grouping_dimension(DsqlCompilerScratch* dsqlScratch, GroupingSpec* spec,
		ValueListNode* selectList, ValueExprNode* node, unsigned& index, bool resolveDsql)
	{
		bool present = false;

		return pass1_find_grouping_dimension_match(dsqlScratch, spec, selectList, node, NULL,
			index, present, resolveDsql, true, true);
	}

	ValueExprNode* pass1_make_grouping_id_value(DsqlCompilerScratch* dsqlScratch,
		GroupingSpec* spec, const SortedArray<unsigned>* groupingSet, ValueListNode* selectList,
		GroupingIdNode* groupingIdNode, bool resolveDsql)
	{
		fb_assert(spec);
		fb_assert(groupingSet);

		const char* functionName = groupingIdNode->getDsqlFunctionName();

		if (!groupingIdNode->args || groupingIdNode->args->items.isEmpty())
			postGroupingValidationError(string(functionName) +
				" requires at least one argument");

		if (groupingIdNode->args->items.getCount() > MAX_GROUPING_ID_ARGS)
			postGroupingValidationError(string(functionName) +
				" supports at most 63 arguments");

		SINT64 value = 0;
		const FB_SIZE_T count = groupingIdNode->args->items.getCount();

		for (FB_SIZE_T i = 0; i < count; ++i)
		{
			unsigned dimension = 0;
			bool present = false;

			if (!pass1_find_grouping_dimension_match(dsqlScratch, spec, selectList,
					groupingIdNode->args->items[i], groupingSet, dimension, present, resolveDsql,
					true, true))
			{
				postGroupingValidationError(string(functionName) +
					" argument must match a grouping expression");
			}

			if (!present)
				value |= SINT64(1) << (count - i - 1);
		}

		return MAKE_const_sint64(value, 0);
	}

	ValueExprNode* pass1_make_grouping_value(DsqlCompilerScratch* dsqlScratch, MemoryPool& pool,
		GroupingSpec* spec, const GroupingSpec::Set& set, ValueListNode* selectList, ValueExprNode* node)
	{
		if (auto aliasNode = nodeAs<DsqlAliasNode>(node))
			return node;

		unsigned dimension = 0;
		bool present = false;

		if (auto groupingNode = nodeAs<GroupingNode>(node))
		{
			if (pass1_find_grouping_dimension_match(dsqlScratch, spec, selectList,
					groupingNode->arg, &set.dimensions, dimension, present, false, true, false))
			{
				DsqlAliasNode* aliasNode = FB_NEW_POOL(pool) DsqlAliasNode(pool, MetaName("GROUPING"),
					MAKE_const_slong(present ? 0 : 1));
				aliasNode->dsqlGroupingExpression = groupingNode;
				return aliasNode;
			}

			return node;
		}

		if (auto groupingIdNode = nodeAs<GroupingIdNode>(node))
		{
			DsqlAliasNode* aliasNode = FB_NEW_POOL(pool) DsqlAliasNode(pool,
				MetaName(groupingIdNode->getDsqlFunctionName()),
				pass1_make_grouping_id_value(dsqlScratch, spec, &set.dimensions, selectList,
					groupingIdNode, false));
			aliasNode->dsqlGroupingExpression = groupingIdNode;
			return aliasNode;
		}

		if (pass1_find_grouping_dimension_match(dsqlScratch, spec, selectList, node,
				&set.dimensions, dimension, present, false, false, false) && !present)
		{
			if (auto fieldNode = nodeAs<FieldNode>(node))
			{
				if (fieldNode->dsqlName.hasData())
					return FB_NEW_POOL(pool) DsqlAliasNode(pool, fieldNode->dsqlName, node);
			}
		}

		return node;
	}

	ValueListNode* pass1_make_grouping_select_list(DsqlCompilerScratch* dsqlScratch, MemoryPool& pool,
		GroupingSpec* spec, const GroupingSpec::Set& set, ValueListNode* selectList)
	{
		if (!selectList)
			return NULL;

		ValueListNode* branchList = FB_NEW_POOL(pool) ValueListNode(pool, 0u);

		for (auto item = selectList->items.begin(); item != selectList->items.end(); ++item)
		{
			branchList->add(pass1_make_grouping_value(dsqlScratch, pool, spec, set,
				selectList, *item));
		}

		return branchList;
	}

	ValueListNode* pass1_copy_grouping_select_list(MemoryPool& pool, ValueListNode* selectList)
	{
		if (!selectList)
			return NULL;

		ValueListNode* copy = FB_NEW_POOL(pool) ValueListNode(pool, 0u);

		for (auto item = selectList->items.begin(); item != selectList->items.end(); ++item)
			copy->add(*item);

		return copy;
	}

	ValueListNode* pass1_trim_grouping_select_list(MemoryPool& pool, ValueListNode* selectList,
		FB_SIZE_T count)
	{
		if (selectList->items.getCount() == count)
			return selectList;

		ValueListNode* trimmed = FB_NEW_POOL(pool) ValueListNode(pool, count);

		for (FB_SIZE_T i = 0; i < count; ++i)
			trimmed->items[i] = selectList->items[i];

		return trimmed;
	}

	AggregateSourceNode* pass1_find_grouping_aggregate(RseNode* rse)
	{
		if (!rse || !rse->dsqlStreams)
			return NULL;

		for (auto stream = rse->dsqlStreams->items.begin(); stream != rse->dsqlStreams->items.end();
			 ++stream)
		{
			if (auto aggregate = nodeAs<AggregateSourceNode>(*stream))
			{
				if (!aggregate->dsqlWindow)
					return aggregate;

				if (auto nested = pass1_find_grouping_aggregate(aggregate->dsqlRse.getObject()))
					return nested;
			}
			else if (auto nestedRse = nodeAs<RseNode>(*stream))
			{
				if (auto nested = pass1_find_grouping_aggregate(nestedRse))
					return nested;
			}
		}

		return NULL;
	}

	bool pass1_grouping_contexts_match(const dsql_ctx* left, const dsql_ctx* right)
	{
		if (left == right)
			return true;

		if (!left || !right || left->ctx_scope_level != right->ctx_scope_level)
			return false;

		if (left->ctx_relation != right->ctx_relation ||
			left->ctx_procedure != right->ctx_procedure ||
			left->ctx_table_value_fun != right->ctx_table_value_fun)
		{
			return false;
		}

		if (left->ctx_alias.hasData() || right->ctx_alias.hasData())
			return PASS1_compare_alias(left->ctx_alias, right->ctx_alias);

		return left->ctx_internal_alias == right->ctx_internal_alias;
	}

	bool pass1_resolved_group_fields_match(DsqlCompilerScratch* dsqlScratch,
		const FieldNode* left, const FieldNode* right)
	{
		if (!left || !right || !left->dsqlField || !right->dsqlField ||
			left->dsqlField != right->dsqlField ||
			!pass1_grouping_contexts_match(left->dsqlContext, right->dsqlContext))
		{
			return false;
		}

		if (left->dsqlIndices || right->dsqlIndices)
		{
			return left->dsqlIndices && right->dsqlIndices &&
				(PASS1_node_match(dsqlScratch, left->dsqlIndices, right->dsqlIndices, true) ||
				 PASS1_node_match(dsqlScratch, right->dsqlIndices, left->dsqlIndices, true));
		}

		return true;
	}

	void pass1_collect_grouping_match_nodes(ExprNode* node,
		HalfStaticArray<FieldNode*, 8>& fields, HalfStaticArray<DsqlMapNode*, 4>& maps)
	{
		if (!node)
			return;

		if (auto fieldNode = nodeAs<FieldNode>(node))
			fields.add(fieldNode);
		else if (auto mapNode = nodeAs<DsqlMapNode>(node))
		{
			maps.add(mapNode);
			pass1_collect_grouping_match_nodes(mapNode->map->map_node, fields, maps);
		}

		NodeRefsHolder holder;
		node->getChildren(holder, true);

		for (auto child : holder.refs)
			pass1_collect_grouping_match_nodes(*child, fields, maps);
	}

	bool pass1_find_grouping_field_match_context(const dsql_ctx* context,
		const HalfStaticArray<FieldNode*, 8>& fields, dsql_ctx*& target)
	{
		target = NULL;

		if (!context)
			return false;

		for (auto field = fields.begin(); field != fields.end(); ++field)
		{
			if (!(*field)->dsqlContext)
				continue;

			if (!pass1_grouping_contexts_match((*field)->dsqlContext, context))
				continue;

			if (!target)
				target = (*field)->dsqlContext;
			else if (target != (*field)->dsqlContext)
				return false;
		}

		return target != NULL;
	}

	bool pass1_find_grouping_map_match_context(const dsql_ctx* context,
		const HalfStaticArray<DsqlMapNode*, 4>& maps, dsql_ctx*& target)
	{
		target = NULL;

		if (!context)
			return false;

		for (auto map = maps.begin(); map != maps.end(); ++map)
		{
			if (!(*map)->context)
				continue;

			if (!pass1_grouping_contexts_match((*map)->context, context))
				continue;

			if (!target)
				target = (*map)->context;
			else if (target != (*map)->context)
				return false;
		}

		return target != NULL;
	}

	struct GroupingFieldReferenceRestore
	{
		ExprNode** ref = NULL;
		ExprNode* node = NULL;
	};

	struct GroupingMapContextRestore
	{
		DsqlMapNode* node = NULL;
		dsql_ctx* context = NULL;
	};

	class GroupingContextMatcher
	{
	public:
		GroupingContextMatcher(MemoryPool& aPool, ValueExprNode* left, ValueExprNode* right)
			: pool(aPool),
			  rightRoot(right)
		{
			pass1_collect_grouping_match_nodes(left, leftFields, leftMaps);

			HalfStaticArray<FieldNode*, 8> rightFields;
			pass1_collect_grouping_match_nodes(rightRoot, rightFields, rightMaps);

			ExprNode** rightRootRef = reinterpret_cast<ExprNode**>(rightRoot.getAddress());
			remapFieldReferences(rightRootRef);
			remapMapContexts();
		}

		~GroupingContextMatcher()
		{
			for (auto restore = fieldRestores.begin(); restore != fieldRestores.end(); ++restore)
				*restore->ref = restore->node;

			for (auto restore = mapRestores.begin(); restore != mapRestores.end(); ++restore)
				restore->node->context = restore->context;
		}

		ValueExprNode* getRightNode()
		{
			return rightRoot;
		}

	private:
		FieldNode* makeFieldNode(const FieldNode* source, dsql_ctx* context)
		{
			FieldNode* node = FB_NEW_POOL(pool) FieldNode(pool, context, source->dsqlField,
				const_cast<ValueListNode*>(source->dsqlIndices.getObject()));

			node->dsqlQualifier = source->dsqlQualifier;
			node->dsqlName = source->dsqlName;
			node->setDsqlDesc(source->getDsqlDesc());
			node->format = source->format;
			node->cursorNumber = source->cursorNumber;
			node->dsqlCursorField = source->dsqlCursorField;

			return node;
		}

		void remapFieldReference(ExprNode** ref, FieldNode* field)
		{
			dsql_ctx* target = NULL;

			if (!pass1_find_grouping_field_match_context(field->dsqlContext, leftFields, target) ||
				field->dsqlContext == target)
			{
				return;
			}

			GroupingFieldReferenceRestore restore;
			restore.ref = ref;
			restore.node = *ref;
			fieldRestores.add(restore);

			*ref = makeFieldNode(field, target);
		}

		void remapFieldReferences(ExprNode** ref)
		{
			if (!ref || !*ref)
				return;

			if (auto field = nodeAs<FieldNode>(*ref))
			{
				remapFieldReference(ref, field);
				return;
			}

			if (auto map = nodeAs<DsqlMapNode>(*ref))
			{
				ExprNode** mapRef = reinterpret_cast<ExprNode**>(map->map->map_node.getAddress());
				remapFieldReferences(mapRef);
			}

			NodeRefsHolder holder;
			(*ref)->getChildren(holder, true);

			for (auto child : holder.refs)
			{
				remapFieldReferences(child);
			}
		}

		void remapMapContexts()
		{
			for (auto map = rightMaps.begin(); map != rightMaps.end(); ++map)
			{
				dsql_ctx* target = NULL;

				if (!pass1_find_grouping_map_match_context((*map)->context, leftMaps, target) ||
					(*map)->context == target)
				{
					continue;
				}

				GroupingMapContextRestore restore;
				restore.node = *map;
				restore.context = (*map)->context;
				mapRestores.add(restore);

				(*map)->context = target;
			}
		}

	private:
		MemoryPool& pool;
		NestConst<ValueExprNode> rightRoot;
		HalfStaticArray<FieldNode*, 8> leftFields;
		HalfStaticArray<DsqlMapNode*, 4> leftMaps;
		HalfStaticArray<DsqlMapNode*, 4> rightMaps;
		HalfStaticArray<GroupingFieldReferenceRestore, 8> fieldRestores;
		HalfStaticArray<GroupingMapContextRestore, 4> mapRestores;
	};

	bool pass1_group_items_match(DsqlCompilerScratch* dsqlScratch, const ValueExprNode* left,
		const ValueExprNode* right)
	{
		ValueExprNode* leftNode = const_cast<ValueExprNode*>(left);
		ValueExprNode* rightNode = const_cast<ValueExprNode*>(right);

		if (PASS1_node_match(dsqlScratch, left, right, true) ||
			PASS1_node_match(dsqlScratch, right, left, true) ||
			pass1_resolved_group_fields_match(dsqlScratch, nodeAs<FieldNode>(left),
				nodeAs<FieldNode>(right)) ||
			pass1_dsql_grouping_items_match(dsqlScratch, leftNode, rightNode))
		{
			return true;
		}

		GroupingContextMatcher matcher(dsqlScratch->getPool(), leftNode, rightNode);
		ValueExprNode* remappedRightNode = matcher.getRightNode();

		return PASS1_node_match(dsqlScratch, left, remappedRightNode, true) ||
			PASS1_node_match(dsqlScratch, remappedRightNode, left, true);
	}

	bool pass1_group_list_contains(DsqlCompilerScratch* dsqlScratch, const ValueListNode* group,
		const ValueExprNode* expr)
	{
		if (!group)
			return false;

		for (auto item = group->items.begin(); item != group->items.end(); ++item)
		{
			if (pass1_group_items_match(dsqlScratch, *item, expr))
				return true;
		}

		return false;
	}

	FB_SIZE_T pass1_group_list_unique_count(DsqlCompilerScratch* dsqlScratch,
		const ValueListNode* group)
	{
		if (!group)
			return 0;

		FB_SIZE_T count = 0;

		for (auto item = group->items.begin(); item != group->items.end(); ++item)
		{
			bool duplicate = false;

			for (auto previous = group->items.begin(); previous != item; ++previous)
			{
				if (pass1_group_items_match(dsqlScratch, *previous, *item))
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
				++count;
		}

		return count;
	}

	bool pass1_group_lists_match(DsqlCompilerScratch* dsqlScratch, const ValueListNode* left,
		const ValueListNode* right)
	{
		if (pass1_group_list_unique_count(dsqlScratch, left) !=
			pass1_group_list_unique_count(dsqlScratch, right))
		{
			return false;
		}

		if (!left)
			return true;

		for (auto item = left->items.begin(); item != left->items.end(); ++item)
		{
			bool duplicate = false;

			for (auto previous = left->items.begin(); previous != item; ++previous)
			{
				if (pass1_group_items_match(dsqlScratch, *previous, *item))
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate && !pass1_group_list_contains(dsqlScratch, right, *item))
				return false;
		}

		return true;
	}

	bool pass1_grouping_branches_match(DsqlCompilerScratch* dsqlScratch, RseNode* left,
		RseNode* right)
	{
		AggregateSourceNode* leftAggregate = pass1_find_grouping_aggregate(left);
		AggregateSourceNode* rightAggregate = pass1_find_grouping_aggregate(right);

		return leftAggregate && rightAggregate &&
			pass1_group_lists_match(dsqlScratch, leftAggregate->dsqlGroup,
				rightAggregate->dsqlGroup);
	}

	RecSourceListNode* pass1_deduplicate_grouping_union_clauses(
		DsqlCompilerScratch* dsqlScratch, MemoryPool& pool, RecSourceListNode* clauses)
	{
		RecSourceListNode* deduplicated = FB_NEW_POOL(pool) RecSourceListNode(pool, 0u);

		for (auto clause = clauses->items.begin(); clause != clauses->items.end(); ++clause)
		{
			RseNode* candidate = nodeAs<RseNode>(*clause);
			fb_assert(candidate);

			bool duplicate = false;

			for (auto existing = deduplicated->items.begin(); existing != deduplicated->items.end();
				 ++existing)
			{
				if (pass1_grouping_branches_match(dsqlScratch, nodeAs<RseNode>(*existing),
						candidate))
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
				deduplicated->add(candidate);
		}

		return deduplicated;
	}

	ValueListNode* pass1_make_grouping_group_list(MemoryPool& pool, GroupingSpec* spec,
		const GroupingSpec::Set& set)
	{
		ValueListNode* groupList = FB_NEW_POOL(pool) ValueListNode(pool, 0u);

		for (auto dimension = set.dimensions.begin(); dimension != set.dimensions.end(); ++dimension)
			groupList->add(spec->dimensions[*dimension].expr);

		return groupList;
	}

	ValueExprNode* pass1_unmap_grouping_order_item(ValueExprNode* node)
	{
		while (auto mapNode = nodeAs<DsqlMapNode>(node))
			node = mapNode->map->map_node;

		return node;
	}

	bool pass1_raw_select_item_matches(DsqlCompilerScratch* dsqlScratch, ValueExprNode* orderValue,
		ValueExprNode* selectItem)
	{
		selectItem = pass1_unmap_grouping_order_item(selectItem);

		if (PASS1_node_match(dsqlScratch, orderValue, selectItem, true) ||
			PASS1_node_match(dsqlScratch, selectItem, orderValue, true))
		{
			return true;
		}

		if (auto aliasNode = nodeAs<DsqlAliasNode>(selectItem))
		{
			if (aliasNode->dsqlGroupingExpression &&
				(PASS1_node_match(dsqlScratch, orderValue, aliasNode->dsqlGroupingExpression, true) ||
				 PASS1_node_match(dsqlScratch, aliasNode->dsqlGroupingExpression, orderValue, true)))
			{
				return true;
			}

			return PASS1_node_match(dsqlScratch, orderValue, aliasNode->value, true) ||
				PASS1_node_match(dsqlScratch, aliasNode->value, orderValue, true);
		}

		return false;
	}

	bool pass1_node_contains_grouping(ExprNode* node)
	{
		if (!node)
			return false;

		if (nodeIs<GroupingNode>(node) || nodeIs<GroupingIdNode>(node))
			return true;

		if (nodeIs<SubQueryNode>(node))
			return false;

		NodeRefsHolder holder;
		node->getChildren(holder, true);

		for (auto child : holder.refs)
		{
			if (pass1_node_contains_grouping(*child))
				return true;
		}

		return false;
	}

	ValueExprNode* pass1_make_hidden_order_select_item(MemoryPool& pool, ValueExprNode* orderValue)
	{
		if (!pass1_node_contains_grouping(orderValue))
			return orderValue;

		DsqlAliasNode* aliasNode = FB_NEW_POOL(pool) DsqlAliasNode(pool, MetaName(), orderValue);
		aliasNode->dsqlGroupingExpression = orderValue;
		return aliasNode;
	}

	bool pass1_grouping_expression_matches(DsqlCompilerScratch* dsqlScratch, ValueExprNode* left,
		ValueExprNode* right)
	{
		return PASS1_node_match(dsqlScratch, left, right, true) ||
			PASS1_node_match(dsqlScratch, right, left, true);
	}

	bool pass1_find_grouping_dependency_position(DsqlCompilerScratch* dsqlScratch,
		ValueExprNode* value, ValueListNode* selectList, SLONG& position)
	{
		if (!pass1_node_contains_grouping(value) || !selectList)
			return false;

		for (FB_SIZE_T i = selectList->items.getCount(); i > 0; --i)
		{
			ValueExprNode* selectItem = pass1_unmap_grouping_order_item(selectList->items[i - 1]);

			if (auto aliasNode = nodeAs<DsqlAliasNode>(selectItem))
			{
				if (aliasNode->dsqlGroupingExpression &&
					pass1_grouping_expression_matches(dsqlScratch, value,
						aliasNode->dsqlGroupingExpression))
				{
					position = static_cast<SLONG>(i);
					return true;
				}
			}
		}

		return false;
	}

	bool pass1_find_raw_order_position(DsqlCompilerScratch* dsqlScratch, ValueExprNode* orderValue,
		ValueListNode* selectList, FB_SIZE_T selectItems, SLONG& position)
	{
		if (auto fieldNode = nodeAs<FieldNode>(orderValue))
		{
			if (fieldNode->dsqlName.hasData())
			{
				bool found = false;

				for (FB_SIZE_T i = 0; i < selectItems; ++i)
				{
					ValueExprNode* selectItem = selectList->items[i];
					ValueExprNode* orderSelectItem = pass1_unmap_grouping_order_item(selectItem);
					bool matched = false;

					if (fieldNode->dsqlQualifier.object.isEmpty())
					{
						if (auto aliasNode = nodeAs<DsqlAliasNode>(orderSelectItem))
							matched = aliasNode->name == fieldNode->dsqlName;
						else if (auto selectField = nodeAs<FieldNode>(orderSelectItem))
						{
							matched = selectField->dsqlQualifier.object.isEmpty() &&
								selectField->dsqlName == fieldNode->dsqlName;
						}
					}

					if (!matched)
					{
						matched = pass1_raw_field_matches_resolved_field(dsqlScratch, fieldNode,
							nodeAs<FieldNode>(pass1_effective_grouping_item(orderSelectItem)));
					}

					if (!matched)
					{
						if (auto derivedField = nodeAs<DerivedFieldNode>(orderSelectItem))
						{
							matched = derivedField->name == fieldNode->dsqlName &&
								pass1_raw_qualifier_matches_context(fieldNode->dsqlQualifier,
									derivedField->context);
						}
					}

					if (matched)
					{
						if (found)
							pass1_post_ambiguous_select_list_name(fieldNode->dsqlName);

						position = static_cast<SLONG>(i + 1);
						found = true;
					}
				}

				if (found)
					return true;
			}
		}

		for (FB_SIZE_T i = 0; i < selectItems; ++i)
		{
			if (pass1_raw_select_item_matches(dsqlScratch, orderValue, selectList->items[i]))
			{
				position = static_cast<SLONG>(i + 1);
				return true;
			}
		}

		return false;
	}

	void pass1_append_grouping_order_select_items(DsqlCompilerScratch* dsqlScratch, MemoryPool& pool,
		ValueListNode* selectList, ValueListNode* order)
	{
		if (!order)
			return;

		for (FB_SIZE_T i = 0; i < order->items.getCount(); ++i)
		{
			const auto orderNode = nodeAs<OrderNode>(order->items[i]);
			fb_assert(orderNode);

			ValueExprNode* orderValue = orderNode->value;
			const auto collateNode = nodeAs<CollateNode>(orderValue);

			if (collateNode)
				orderValue = collateNode->arg;

			if (auto literal = nodeAs<LiteralNode>(orderValue))
			{
				if (literal->litDesc.dsc_dtype == dtype_long)
					continue;
			}

			SLONG position = 0;

			if (!pass1_find_raw_order_position(dsqlScratch, orderValue, selectList,
					selectList->items.getCount(), position))
			{
				selectList->add(pass1_make_hidden_order_select_item(pool, orderValue));
			}
		}
	}

	ValueListNode* pass1_make_grouping_union_order(DsqlCompilerScratch* dsqlScratch, MemoryPool& pool,
		ValueListNode* order, ValueListNode* selectList, FB_SIZE_T visibleSelectItems,
		bool selectListExpands)
	{
		if (!order)
			return NULL;

		if (selectListExpands)
			return order;

		fb_assert(selectList);

		ValueListNode* unionOrder = FB_NEW_POOL(pool) ValueListNode(pool, order->items.getCount());

		for (FB_SIZE_T i = 0; i < order->items.getCount(); ++i)
		{
			const auto orderNode = nodeAs<OrderNode>(order->items[i]);
			fb_assert(orderNode);

			ValueExprNode* orderValue = orderNode->value;
			const auto collateNode = nodeAs<CollateNode>(orderValue);

			if (collateNode)
				orderValue = collateNode->arg;

			SLONG position = 0;

			if (auto literal = nodeAs<LiteralNode>(orderValue))
			{
				if (literal->litDesc.dsc_dtype == dtype_long)
					position = literal->getSlong();
			}

			if (position != 0)
			{
				if (position < 1 || position > (SLONG) visibleSelectItems)
				{
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  Arg::Gds(isc_dsql_command_err) <<
							  Arg::Gds(isc_order_by_err));
				}
			}
			else if (!pass1_find_raw_order_position(dsqlScratch, orderValue, selectList,
				visibleSelectItems, position))
			{
				position = static_cast<SLONG>(selectList->items.getCount() + 1);
				selectList->add(orderValue);
			}

			ValueExprNode* unionOrderValue = MAKE_const_slong(position);

			if (collateNode)
				unionOrderValue = FB_NEW_POOL(pool) CollateNode(pool, unionOrderValue, collateNode->collation);

			OrderNode* unionOrderNode = FB_NEW_POOL(pool) OrderNode(pool, unionOrderValue);
			unionOrderNode->descending = orderNode->descending;
			unionOrderNode->nullsPlacement = orderNode->nullsPlacement;

			unionOrder->items[i] = unionOrderNode;
		}

		return unionOrder;
	}
}


ValueExprNode* DsqlCompilerScratch::makeGroupingValue(ValueExprNode* node)
{
	if (!activeGroupingSpec || !activeGroupingSet)
		return NULL;

	if (scopeLevel != activeGroupingScopeLevel)
		return NULL;

	if (auto aliasNode = nodeAs<DsqlAliasNode>(node))
	{
		if (ValueExprNode* value = makeGroupingValue(aliasNode->value))
		{
			thread_db* tdbb = JRD_get_thread_data();
			MemoryPool& pool = *tdbb->getDefaultPool();
			DsqlAliasNode* newAlias = FB_NEW_POOL(pool) DsqlAliasNode(pool, aliasNode->name, value);

			if (nodeIs<GroupingNode>(aliasNode->value) ||
				nodeIs<GroupingIdNode>(aliasNode->value))
			{
				newAlias->dsqlGroupingExpression = aliasNode->value;
			}
			else if (auto valueAlias = nodeAs<DsqlAliasNode>(value))
				newAlias->dsqlGroupingExpression = valueAlias->dsqlGroupingExpression;

			return newAlias;
		}

		return NULL;
	}

	auto groupingNode = nodeAs<GroupingNode>(node);

	unsigned dimension = 0;
	bool present = false;

	if (groupingNode)
	{
		if (inWhereClause)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_random) << Arg::Str("GROUPING is not allowed in the WHERE clause"));
		}

		if (inGroupByClause)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_random) << Arg::Str("GROUPING is not allowed in the GROUP BY clause"));
		}

		if (inAggregateFunction)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_random) << Arg::Str("GROUPING is not allowed in aggregate function arguments"));
		}

		if (!pass1_find_grouping_dimension_match(this, activeGroupingSpec,
				activeGroupingSelectList, groupingNode->arg, activeGroupingSet,
				dimension, present, true, true, true))
		{
			postGroupingValidationError("GROUPING argument must match a grouping expression");
		}

		return MAKE_const_slong(present ? 0 : 1);
	}

	if (auto groupingIdNode = nodeAs<GroupingIdNode>(node))
	{
		const char* functionName = groupingIdNode->getDsqlFunctionName();

		if (inWhereClause)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_random) <<
					  Arg::Str(string(functionName) + " is not allowed in the WHERE clause"));
		}

		if (inGroupByClause)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_random) <<
					  Arg::Str(string(functionName) + " is not allowed in the GROUP BY clause"));
		}

		if (inAggregateFunction)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_random) <<
					  Arg::Str(string(functionName) + " is not allowed in aggregate function arguments"));
		}

		return pass1_make_grouping_id_value(this, activeGroupingSpec, activeGroupingSet,
			activeGroupingSelectList, groupingIdNode, true);
	}

	if (inWhereClause || inGroupByClause || inAggregateFunction)
		return NULL;

	if (pass1_find_grouping_dimension_match(this, activeGroupingSpec,
			activeGroupingSelectList, node, activeGroupingSet, dimension, present, true, false,
			true) &&
		!present)
	{
		dsc desc;

		GroupingSpec* groupingSpec = activeGroupingSpec;
		ActiveGroupingContextDisabler disableGrouping(this);

		NestConst<ValueExprNode> dimensionNode = pass1_resolve_grouping_dimension(groupingSpec,
			activeGroupingSelectList, dimension);
		ValueExprNode* value = Node::doDsqlPass(this, dimensionNode, false);
		DsqlDescMaker::fromNode(this, &desc, value, true);

		ValueExprNode* nullValue = pass1_make_typed_null(desc);

		if (auto fieldNode = nodeAs<FieldNode>(node))
		{
			if (fieldNode->dsqlField && fieldNode->dsqlField->fld_name.hasData())
			{
				thread_db* tdbb = JRD_get_thread_data();
				MemoryPool& pool = *tdbb->getDefaultPool();
				return FB_NEW_POOL(pool) DsqlAliasNode(pool, fieldNode->dsqlField->fld_name,
					nullValue);
			}
		}

		return nullValue;
	}

	return NULL;
}

static bool pass1_contains_window_function(DsqlCompilerScratch* dsqlScratch, ExprNode* node)
{
	if (!node)
		return false;

	if (nodeIs<OverNode>(node))
		return true;

	if (nodeIs<SubQueryNode>(node))
		return false;

	NodeRefsHolder holder(dsqlScratch->getPool());
	node->getChildren(holder, true);

	for (auto child : holder.refs)
	{
		if (pass1_contains_window_function(dsqlScratch, *child))
			return true;
	}

	return false;
}

static bool pass1_contains_window_function(DsqlCompilerScratch* dsqlScratch, ValueListNode* list)
{
	if (!list)
		return false;

	for (auto item = list->items.begin(); item != list->items.end(); ++item)
	{
		if (pass1_contains_window_function(dsqlScratch, *item))
			return true;
	}

	return false;
}

static ValueExprNode* pass1_as_value_expr(ExprNode* node)
{
	return node && node->getKind() == DmlNode::KIND_VALUE ?
		static_cast<ValueExprNode*>(node) : NULL;
}

static ValueExprNode* pass1_make_grouping_window_placeholder(MemoryPool& pool, ValueExprNode* node)
{
	ValueExprNode* value = NullNode::instance();

	if (auto aliasNode = nodeAs<DsqlAliasNode>(node))
		return FB_NEW_POOL(pool) DsqlAliasNode(pool, aliasNode->name, value);

	return value;
}

static ValueListNode* pass1_make_grouping_window_select_list(DsqlCompilerScratch* dsqlScratch,
	MemoryPool& pool, ValueListNode* selectList)
{
	if (!selectList)
		return NULL;

	ValueListNode* copy = FB_NEW_POOL(pool) ValueListNode(pool, 0u);

	for (auto item = selectList->items.begin(); item != selectList->items.end(); ++item)
	{
		if (pass1_contains_window_function(dsqlScratch, *item))
			copy->add(pass1_make_grouping_window_placeholder(pool, *item));
		else
			copy->add(*item);
	}

	return copy;
}

static void pass1_append_grouping_window_dependency(DsqlCompilerScratch* dsqlScratch, MemoryPool& pool,
	ValueListNode* selectList, ValueExprNode* node)
{
	if (!selectList || !node || nodeIs<OverNode>(node) || nodeIs<SubQueryNode>(node))
		return;

	SLONG position = 0;

	if (pass1_node_contains_grouping(node))
	{
		if (!pass1_find_grouping_dependency_position(dsqlScratch, node, selectList, position))
			selectList->add(pass1_make_hidden_order_select_item(pool, node));

		return;
	}

	if (!pass1_find_raw_order_position(dsqlScratch, node, selectList,
			selectList->items.getCount(), position))
	{
		selectList->add(pass1_make_hidden_order_select_item(pool, node));
	}
}

static void pass1_append_grouping_window_order_dependencies(DsqlCompilerScratch* dsqlScratch,
	MemoryPool& pool, ValueListNode* selectList, ValueListNode* order)
{
	if (!order)
		return;

	for (auto item = order->items.begin(); item != order->items.end(); ++item)
	{
		ValueExprNode* value = *item;

		if (const auto orderNode = nodeAs<OrderNode>(value))
			value = orderNode->value;

		pass1_append_grouping_window_dependency(dsqlScratch, pool, selectList, value);
	}
}

static void pass1_append_grouping_window_clause_dependencies(DsqlCompilerScratch* dsqlScratch,
	MemoryPool& pool, ValueListNode* selectList, WindowClause* window)
{
	if (!window)
		return;

	if (window->partition)
	{
		for (auto item = window->partition->items.begin(); item != window->partition->items.end(); ++item)
			pass1_append_grouping_window_dependency(dsqlScratch, pool, selectList, *item);
	}

	pass1_append_grouping_window_order_dependencies(dsqlScratch, pool, selectList, window->order);

	if (window->extent)
	{
		NodeRefsHolder holder(dsqlScratch->getPool());
		window->extent->getChildren(holder, true);

		for (auto child : holder.refs)
		{
			if (auto value = pass1_as_value_expr(*child))
				pass1_append_grouping_window_dependency(dsqlScratch, pool, selectList, value);
		}
	}
}

static void pass1_append_grouping_window_dependencies(DsqlCompilerScratch* dsqlScratch,
	MemoryPool& pool, ValueListNode* selectList, ExprNode* node)
{
	if (!node || nodeIs<SubQueryNode>(node))
		return;

	if (auto overNode = nodeAs<OverNode>(node))
	{
		if (overNode->aggExpr)
		{
			NodeRefsHolder holder(dsqlScratch->getPool());
			overNode->aggExpr->getChildren(holder, true);

			for (auto child : holder.refs)
			{
				if (auto value = pass1_as_value_expr(*child))
					pass1_append_grouping_window_dependency(dsqlScratch, pool, selectList, value);
			}
		}

		pass1_append_grouping_window_clause_dependencies(dsqlScratch, pool, selectList,
			overNode->window);
		return;
	}

	NodeRefsHolder holder(dsqlScratch->getPool());
	node->getChildren(holder, true);

	for (auto child : holder.refs)
	{
		if (auto value = pass1_as_value_expr(*child))
		{
			if (!pass1_contains_window_function(dsqlScratch, value))
			{
				pass1_append_grouping_window_dependency(dsqlScratch, pool, selectList, value);
				continue;
			}
		}

		pass1_append_grouping_window_dependencies(dsqlScratch, pool, selectList, *child);
	}
}

static void pass1_append_grouping_window_dependencies(DsqlCompilerScratch* dsqlScratch,
	MemoryPool& pool, ValueListNode* selectList, ValueListNode* list)
{
	if (!list)
		return;

	for (auto item = list->items.begin(); item != list->items.end(); ++item)
		pass1_append_grouping_window_dependencies(dsqlScratch, pool, selectList, *item);
}

static void pass1_append_grouping_window_dependencies(DsqlCompilerScratch* dsqlScratch,
	MemoryPool& pool, ValueListNode* selectList, NamedWindowsClause* namedWindows)
{
	if (!namedWindows)
		return;

	for (NamedWindowsClause::iterator i = namedWindows->begin(); i != namedWindows->end(); ++i)
		pass1_append_grouping_window_clause_dependencies(dsqlScratch, pool, selectList, i->second);
}

static bool pass1_is_grouping_window_alias(DsqlCompilerScratch* dsqlScratch, ValueExprNode* node,
	ValueListNode* selectList)
{
	const auto fieldNode = nodeAs<FieldNode>(node);

	if (!fieldNode || !fieldNode->dsqlName.hasData() ||
		fieldNode->dsqlQualifier.object.hasData() || !selectList)
	{
		return false;
	}

	for (auto item = selectList->items.begin(); item != selectList->items.end(); ++item)
	{
		if (auto aliasNode = nodeAs<DsqlAliasNode>(*item))
		{
			if (aliasNode->name == fieldNode->dsqlName &&
				pass1_contains_window_function(dsqlScratch, aliasNode->value))
			{
				return true;
			}
		}
	}

	return false;
}

struct GroupingWindowExpressionRestore
{
	ExprNode** ref = NULL;
	ExprNode* node = NULL;
};

class GroupingWindowExpressionRemapper
{
public:
	GroupingWindowExpressionRemapper(DsqlCompilerScratch* aDsqlScratch, ValueListNode* aSourceList,
			ValueListNode* aTargetList, ValueListNode* aOuterSelectList)
		: dsqlScratch(aDsqlScratch),
		  sourceList(aSourceList),
		  targetList(aTargetList),
		  outerSelectList(aOuterSelectList)
	{
	}

	~GroupingWindowExpressionRemapper()
	{
		for (auto restore = restores.begin(); restore != restores.end(); ++restore)
			*restore->ref = restore->node;
	}

	void remapSelectList(ValueListNode* list)
	{
		remapList(list, false);
	}

	void remapOrderList(ValueListNode* list)
	{
		remapList(list, true);
	}

	void remapNamedWindows(NamedWindowsClause* namedWindows)
	{
		if (!namedWindows)
			return;

		for (NamedWindowsClause::iterator i = namedWindows->begin(); i != namedWindows->end(); ++i)
			remapChildren(i->second);
	}

private:
	void remapList(ValueListNode* list, bool preserveWindowAliases)
	{
		if (!list)
			return;

		for (auto item = list->items.begin(); item != list->items.end(); ++item)
		{
			if (preserveWindowAliases)
			{
				if (auto orderNode = nodeAs<OrderNode>(*item))
				{
					remapRef(reinterpret_cast<ExprNode**>(orderNode->value.getAddress()), true,
						preserveWindowAliases);
					continue;
				}
			}

			remapRef(reinterpret_cast<ExprNode**>(item->getAddress()), true, preserveWindowAliases);
		}
	}

	void remapChildren(ExprNode* node)
	{
		if (!node)
			return;

		NodeRefsHolder holder(dsqlScratch->getPool());
		node->getChildren(holder, true);

		for (auto child : holder.refs)
			remapRef(child, false, false);
	}

	void remapRef(ExprNode** ref, bool /*root*/, bool preserveWindowAliases)
	{
		if (!ref || !*ref || nodeIs<SubQueryNode>(*ref))
			return;

		if (auto aliasNode = nodeAs<DsqlAliasNode>(*ref))
		{
			remapRef(reinterpret_cast<ExprNode**>(aliasNode->value.getAddress()), false,
				preserveWindowAliases);
			return;
		}

		if (!nodeIs<OverNode>(*ref))
		{
			if (auto value = pass1_as_value_expr(*ref))
			{
				const bool isWindowAlias = pass1_is_grouping_window_alias(dsqlScratch, value,
					outerSelectList);

				if (sourceList && targetList && !isWindowAlias)
				{
					SLONG position = 0;

					if ((pass1_find_grouping_dependency_position(dsqlScratch, value, sourceList,
								position) ||
						 pass1_find_raw_order_position(dsqlScratch, value, sourceList,
							 sourceList->items.getCount(), position)) &&
						position >= 1 && ULONG(position) <= targetList->items.getCount())
					{
						GroupingWindowExpressionRestore restore;
						restore.ref = ref;
						restore.node = *ref;
						restores.add(restore);

						*ref = targetList->items[position - 1];
						return;
					}
				}
			}
		}

		remapChildren(*ref);
	}

private:
	DsqlCompilerScratch* dsqlScratch;
	ValueListNode* sourceList;
	ValueListNode* targetList;
	ValueListNode* outerSelectList;
	HalfStaticArray<GroupingWindowExpressionRestore, 16> restores;
};

static void pass1_process_grouping_window_named_windows(DsqlCompilerScratch* dsqlScratch,
	NamedWindowsClause* namedWindows)
{
	if (!namedWindows)
		return;

	for (NamedWindowsClause::iterator i = namedWindows->begin(); i != namedWindows->end(); ++i)
	{
		if (dsqlScratch->context->object()->ctx_named_windows.exist(i->first))
		{
			ERRD_post(
				Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
				Arg::Gds(isc_dsql_window_duplicate) << i->first);
		}

		i->second->dsqlPass(dsqlScratch);
		dsqlScratch->context->object()->ctx_named_windows.put(i->first, i->second);
	}
}

static RseNode* pass1_apply_grouping_window(DsqlCompilerScratch* dsqlScratch, RseNode* unionRse,
	ValueListNode* selectList, ValueListNode* order, RowsClause* rows, bool updateLock,
	bool skipLocked, USHORT flags)
{
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	RseNode* rse = FB_NEW_POOL(pool) RseNode(pool);
	rse->dsqlStreams = FB_NEW_POOL(pool) RecSourceListNode(pool, 1);
	rse->dsqlStreams->items[0] = unionRse;

	++dsqlScratch->inSelectList;
	rse->dsqlSelectList = pass1_sel_list(dsqlScratch, selectList);
	--dsqlScratch->inSelectList;

	if (order)
	{
		++dsqlScratch->inOrderByClause;
		rse->dsqlOrder = PASS1_sort(dsqlScratch, order, selectList);
		--dsqlScratch->inOrderByClause;
	}

	if (rows)
		PASS1_limit(dsqlScratch, rows->length, rows->skip, rse);

	if (updateLock)
		rse->flags |= RseNode::FLAG_WRITELOCK;

	if (skipLocked)
		rse->flags |= RseNode::FLAG_SKIP_LOCKED;

	dsql_ctx* parent_context = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_ctx(*tdbb->getDefaultPool());
	parent_context->ctx_scope_level = dsqlScratch->scopeLevel;

	if (dsqlScratch->inOuterJoin)
		parent_context->ctx_flags |= CTX_outer_join;
	parent_context->ctx_in_outer_join = dsqlScratch->inOuterJoin;

	AggregateSourceNode* window = FB_NEW_POOL(pool) AggregateSourceNode(pool);
	window->dsqlContext = parent_context;
	window->dsqlRse = rse;
	window->dsqlWindow = true;

	RseNode* parentRse = FB_NEW_POOL(pool) RseNode(pool);
	parentRse->dsqlStreams = FB_NEW_POOL(pool) RecSourceListNode(pool, 1);
	parentRse->dsqlStreams->items[0] = window;

	if (rse->hasWriteLock())
	{
		parentRse->flags |= RseNode::FLAG_WRITELOCK;
		rse->flags &= ~RseNode::FLAG_WRITELOCK;
	}

	if (rse->hasSkipLocked())
	{
		parentRse->flags |= RseNode::FLAG_SKIP_LOCKED;
		rse->flags &= ~RseNode::FLAG_SKIP_LOCKED;
	}

	if (rse->dsqlFirst)
	{
		parentRse->dsqlFirst = rse->dsqlFirst;
		rse->dsqlFirst = NULL;
	}

	if (rse->dsqlSkip)
	{
		parentRse->dsqlSkip = rse->dsqlSkip;
		rse->dsqlSkip = NULL;
	}

	AutoSetRestore<bool> autoProcessingWindow(&dsqlScratch->processingWindow, true);

	dsqlScratch->context->push(parent_context);
	remap_streams_to_parent_context(rse->dsqlStreams, parent_context);

	FieldRemapper remapper(dsqlScratch->getPool(), dsqlScratch, parent_context, true);

	ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlSelectList, rse->dsqlSelectList);
	rse->dsqlSelectList = NULL;

	if (order)
	{
		ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlOrder, rse->dsqlOrder);
		rse->dsqlOrder = NULL;
	}

	for (FB_SIZE_T i = 0, mapCount = parent_context->ctx_win_maps.getCount(); i < mapCount; ++i)
	{
		WindowMap* windowMap = parent_context->ctx_win_maps[i];

		if (windowMap->window && windowMap->window->partition)
		{
			windowMap->partitionRemapped = Node::doDsqlPass(dsqlScratch,
				windowMap->window->partition);

			FieldRemapper remapper2(dsqlScratch->getPool(), dsqlScratch, parent_context, true,
				windowMap->window);
			ExprNode::doDsqlFieldRemapper(remapper2, windowMap->partitionRemapped);
		}
	}

	parentRse->dsqlFlags = flags;

	return parentRse;
}


static RseNode* pass1_lower_grouping_sets(DsqlCompilerScratch* dsqlScratch, RseNode* inputRse,
	ValueListNode* order, RowsClause* rows, bool updateLock, bool skipLocked, USHORT flags)
{
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	fb_assert(inputRse->dsqlGrouping && inputRse->dsqlGrouping->hasAdvancedGrouping());

	RowsClause* unionRows = rows;

	if ((inputRse->dsqlFirst || inputRse->dsqlSkip) && rows)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_firstskip_rows));
	}
	else if (inputRse->dsqlFirst || inputRse->dsqlSkip)
	{
		unionRows = FB_NEW_POOL(pool) RowsClause(pool);
		unionRows->length = inputRse->dsqlFirst;
		unionRows->skip = inputRse->dsqlSkip;
	}

	GroupingSpec* spec = pass1_grouping_spec_raw(dsqlScratch, inputRse->dsqlGrouping,
		inputRse->dsqlSelectList);

	const bool hasWindow = pass1_contains_window_function(dsqlScratch, inputRse->dsqlSelectList) ||
		pass1_contains_window_function(dsqlScratch, order);

	ValueListNode* branchSelectList = hasWindow ?
		pass1_make_grouping_window_select_list(dsqlScratch, pool, inputRse->dsqlSelectList) :
		pass1_copy_grouping_select_list(pool, inputRse->dsqlSelectList);
	const bool selectListExpands = pass1_select_list_has_asterisk(branchSelectList);
	const FB_SIZE_T visibleSelectItems = branchSelectList ? branchSelectList->items.getCount() : 0;

	if (hasWindow)
	{
		pass1_append_grouping_window_dependencies(dsqlScratch, pool, branchSelectList,
			inputRse->dsqlSelectList);
		pass1_append_grouping_window_dependencies(dsqlScratch, pool, branchSelectList,
			inputRse->dsqlNamedWindows);
		pass1_append_grouping_window_order_dependencies(dsqlScratch, pool, branchSelectList, order);
	}

	ValueListNode* unionOrder = hasWindow ? NULL :
		pass1_make_grouping_union_order(dsqlScratch, pool, order, branchSelectList,
			visibleSelectItems, selectListExpands);

	UnionSourceNode* unionNode = FB_NEW_POOL(pool) UnionSourceNode(pool);
	unionNode->dsqlAll = true;
	unionNode->dsqlOrderBySelectList = selectListExpands;
	unionNode->dsqlDistinctGroupingSets =
		spec->duplicateMode == GroupingClause::DuplicateMode::DISTINCT;
	unionNode->dsqlClauses = FB_NEW_POOL(pool) RecSourceListNode(pool, spec->sets.getCount());

	for (FB_SIZE_T i = 0; i < spec->sets.getCount(); ++i)
	{
		const auto& set = spec->sets[i];

		RseNode* branch = FB_NEW_POOL(pool) RseNode(pool);
		branch->line = inputRse->line;
		branch->column = inputRse->column;
		branch->dsqlDistinct = NULL;
		branch->dsqlSelectList = pass1_make_grouping_select_list(dsqlScratch, pool, spec, set,
			branchSelectList);
		branch->dsqlFrom = inputRse->dsqlFrom;
		branch->dsqlWhere = inputRse->dsqlWhere;
		branch->dsqlJoinUsing = inputRse->dsqlJoinUsing;
		branch->dsqlGroup = pass1_make_grouping_group_list(pool, spec, set);
		branch->dsqlGroupingSpec = spec;
		branch->dsqlGroupingSet = &set.dimensions;
		branch->dsqlGroupingSelectList = inputRse->dsqlSelectList;
		branch->dsqlGroupingOrder = (!hasWindow && selectListExpands) ? order : NULL;
		branch->dsqlHaving = inputRse->dsqlHaving;
		branch->dsqlNamedWindows = hasWindow ? NULL : inputRse->dsqlNamedWindows;
		branch->rse_plan = inputRse->rse_plan;
		branch->dsqlFlags = inputRse->dsqlFlags;

		unionNode->dsqlClauses->items[i] = branch;
	}

	RseNode* unionRse = pass1_union(dsqlScratch, unionNode, unionOrder, hasWindow ? NULL : unionRows, updateLock,
		skipLocked, flags);

	if (!hasWindow && branchSelectList && !selectListExpands)
	{
		unionRse->dsqlSelectList = pass1_trim_grouping_select_list(pool, unionRse->dsqlSelectList,
			visibleSelectItems);
	}

	if (hasWindow)
	{
		ValueListNode* outerSelectList = pass1_copy_grouping_select_list(pool,
			inputRse->dsqlSelectList);
		ValueListNode* outerOrder = pass1_copy_grouping_select_list(pool, order);

		GroupingWindowExpressionRemapper remapper(dsqlScratch, branchSelectList,
			unionRse->dsqlSelectList, outerSelectList);

		remapper.remapSelectList(outerSelectList);
		remapper.remapOrderList(outerOrder);
		remapper.remapNamedWindows(inputRse->dsqlNamedWindows);

		pass1_process_grouping_window_named_windows(dsqlScratch, inputRse->dsqlNamedWindows);

		RowsClause* outerRows = rows;

		if (!outerRows && (inputRse->dsqlFirst || inputRse->dsqlSkip))
		{
			outerRows = FB_NEW_POOL(pool) RowsClause(pool);
			outerRows->length = inputRse->dsqlFirst;
			outerRows->skip = inputRse->dsqlSkip;
		}

		RseNode* windowRse = pass1_apply_grouping_window(dsqlScratch, unionRse, outerSelectList,
			outerOrder, outerRows, updateLock, skipLocked, flags);

		if (inputRse->dsqlDistinct)
		{
			if (updateLock)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_wlock_conflict) << Arg::Str("DISTINCT"));
			}

			if (windowRse->dsqlSelectList->items.getCount() > MAX_SORT_ITEMS)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_command_err) <<
						  Arg::Gds(isc_dsql_max_distinct_items));
			}

			windowRse->dsqlDistinct = windowRse->dsqlSelectList;
		}

		return windowRse;
	}

	if (inputRse->dsqlDistinct)
	{
		if (updateLock)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_wlock_conflict) << Arg::Str("DISTINCT"));
		}

		if (unionRse->dsqlSelectList->items.getCount() > MAX_SORT_ITEMS)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_dsql_max_distinct_items));
		}

		unionRse->dsqlDistinct = unionRse->dsqlSelectList;
	}

	return unionRse;
}


// Process the limit clause (FIRST/SKIP/ROWS)
void PASS1_limit(DsqlCompilerScratch* dsqlScratch, NestConst<ValueExprNode> firstNode,
	NestConst<ValueExprNode> skipNode, RseNode* rse)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);
	DEV_BLKCHK(firstNode, dsql_type_nod);
	DEV_BLKCHK(skipNode, dsql_type_nod);

	dsc descNode;

	if (dsqlScratch->clientDialect <= SQL_DIALECT_V5)
		descNode.makeLong(0);
	else
		descNode.makeInt64(0);

	{
		ActiveGroupingContextDisabler disableGrouping(dsqlScratch);

		rse->dsqlFirst = Node::doDsqlPass(dsqlScratch, firstNode, false);
		rse->dsqlSkip = Node::doDsqlPass(dsqlScratch, skipNode, false);
	}

	PASS1_set_parameter_type(dsqlScratch, rse->dsqlFirst,
		[&] (dsc* desc) { *desc = descNode; },
		false);

	PASS1_set_parameter_type(dsqlScratch, rse->dsqlSkip,
		[&] (dsc* desc) { *desc = descNode; },
		false);
}


// Lookup a matching item in the select list. Return node if found else return NULL.
// If more matches are found we raise ambiguity error.
ValueExprNode* PASS1_lookup_alias(DsqlCompilerScratch* dsqlScratch, const MetaName& name,
	ValueListNode* selectList, bool process)
{
	ValueExprNode* returnNode = NULL;
	NestConst<ValueExprNode>* ptr = selectList->items.begin();
	const NestConst<ValueExprNode>* const end = selectList->items.end();
	for (; ptr < end; ++ptr)
	{
		NestConst<ValueExprNode> matchingNode = NULL;
		ValueExprNode* node = *ptr;
		DsqlAliasNode* aliasNode;
		FieldNode* fieldNode;
		DerivedFieldNode* derivedField;

		if ((aliasNode = nodeAs<DsqlAliasNode>(node)))
		{
			if (aliasNode->name == name)
				matchingNode = node;
		}
		else if ((fieldNode = nodeAs<FieldNode>(node)))
		{
			if (fieldNode->dsqlField->fld_name == name.c_str())
				matchingNode = node;
		}
		else if ((derivedField = nodeAs<DerivedFieldNode>(node)))
		{
			if (derivedField->name == name)
				matchingNode = node;
		}

		if (matchingNode)
		{
			if (process)
				matchingNode = Node::doDsqlPass(dsqlScratch, matchingNode, false);

			if (returnNode)
			{
				// There was already a node matched, thus raise ambiguous field name error.
				TEXT buffer1[256];
				buffer1[0] = 0;

				if (nodeIs<DsqlAliasNode>(returnNode))
					strcat(buffer1, "an alias");
				else if (nodeIs<FieldNode>(returnNode))
					strcat(buffer1, "a field");
				else if (nodeIs<DerivedFieldNode>(returnNode))
					strcat(buffer1, "a derived field");
				else
					strcat(buffer1, "an item");

				TEXT buffer2[256];
				buffer2[0] = 0;

				if (nodeIs<DsqlAliasNode>(matchingNode))
					strcat(buffer2, "an alias");
				else if (nodeIs<FieldNode>(matchingNode))
					strcat(buffer2, "a field");
				else if (nodeIs<DerivedFieldNode>(matchingNode))
					strcat(buffer2, "a derived field");
				else
					strcat(buffer2, "an item");

				strcat(buffer2, " in the select list with name");

				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
						  Arg::Gds(isc_dsql_ambiguous_field_name) << Arg::Str(buffer1) <<
																	 Arg::Str(buffer2) <<
						  Arg::Gds(isc_random) << name);
			}

			returnNode = matchingNode;
		}
	}

	return returnNode;
}

/**

 	pass1_make_derived_field

    @brief	Create a derived field based on underlying expressions


    @param dsqlScratch
    @param tdbb
    @param select_item

 **/
static ValueExprNode* pass1_make_derived_field(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	ValueExprNode* select_item)
{
	MemoryPool& pool = *tdbb->getDefaultPool();
	DsqlAliasNode* aliasNode;
	SubQueryNode* subQueryNode;
	DsqlMapNode* mapNode;
	FieldNode* fieldNode;
	DerivedFieldNode* derivedField;

	if ((aliasNode = nodeAs<DsqlAliasNode>(select_item)))
	{
		// Create a derived field and ignore alias node.
		DerivedFieldNode* newField = FB_NEW_POOL(pool) DerivedFieldNode(pool,
			aliasNode->name, dsqlScratch->scopeLevel, aliasNode->value);
		newField->setDsqlDesc(aliasNode->value->getDsqlDesc());
		return newField;
	}
	else if ((subQueryNode = nodeAs<SubQueryNode>(select_item)))
	{
		// Try to generate derived field from sub-select
		ValueExprNode* derived_field = pass1_make_derived_field(tdbb, dsqlScratch,
			subQueryNode->value1);

		if ((derivedField = nodeAs<DerivedFieldNode>(derived_field)))
		{
			derivedField->value = select_item;
			return derived_field;
		}
	}
	else if ((mapNode = nodeAs<DsqlMapNode>(select_item)))
	{
		// Aggregate's have map on top.
		ValueExprNode* derived_field = pass1_make_derived_field(tdbb, dsqlScratch, mapNode->map->map_node);

		// If we had successfully made a derived field node change it with original map.
		if ((derivedField = nodeAs<DerivedFieldNode>(derived_field)))
		{
			derivedField->value = select_item;
			derivedField->scope = dsqlScratch->scopeLevel;
			derivedField->setDsqlDesc(select_item->getDsqlDesc());
			return derived_field;
		}
	}
	else if ((fieldNode = nodeAs<FieldNode>(select_item)))
	{
		// Create a derived field and hook in.

		DerivedFieldNode* newField = FB_NEW_POOL(pool) DerivedFieldNode(pool,
			fieldNode->dsqlField->fld_name, dsqlScratch->scopeLevel, select_item);
		newField->setDsqlDesc(fieldNode->getDsqlDesc());
		return newField;
	}
	else if ((derivedField = nodeAs<DerivedFieldNode>(select_item)))
	{
		// Create a derived field that points to a derived field.

		DerivedFieldNode* newField = FB_NEW_POOL(pool) DerivedFieldNode(pool,
			derivedField->name, dsqlScratch->scopeLevel, select_item);
		newField->setDsqlDesc(select_item->getDsqlDesc());
		return newField;
	}

	return select_item;
}


// Prepare a relation name for processing. Allocate a new relation node.
RecordSourceNode* PASS1_relation(DsqlCompilerScratch* dsqlScratch, RecordSourceNode* input)
{
	thread_db* tdbb = JRD_get_thread_data();

	DEV_BLKCHK(dsqlScratch, dsql_type_req);

	const auto context = PASS1_make_context(dsqlScratch, input);

	if (context->ctx_relation)
	{
		const auto relNode = FB_NEW_POOL(*tdbb->getDefaultPool()) RelationSourceNode(
			*tdbb->getDefaultPool(), context->ctx_relation->rel_name);
		relNode->dsqlContext = context;
		return relNode;
	}
	else if (context->ctx_procedure)
	{
		const auto procNode = FB_NEW_POOL(*tdbb->getDefaultPool()) ProcedureSourceNode(
			*tdbb->getDefaultPool(), context->ctx_procedure->prc_name);
		procNode->dsqlContext = context;
		procNode->inputSources = context->ctx_proc_inputs;
		procNode->dsqlInputArgNames = nodeAs<ProcedureSourceNode>(input)->dsqlInputArgNames;
		return procNode;
	}
	else if (context->ctx_table_value_fun)
	{
			const auto tableValueFunctionNode = FB_NEW_POOL(*tdbb->getDefaultPool())
				TableValueFunctionSourceNode(*tdbb->getDefaultPool());
			tableValueFunctionNode->dsqlContext = context;
			return tableValueFunctionNode;
	}

	fb_assert(false);
	return NULL;
}


// Wrapper for pass1_rse_impl. Substitute recursive CTE alias (if needed) and call pass1_rse_impl.
static RseNode* pass1_rse(DsqlCompilerScratch* dsqlScratch, RecordSourceNode* input,
	ValueListNode* order, RowsClause* rows, bool updateLock, bool skipLocked, USHORT flags)
{
	ObjectsArray<QualifiedName> save_alias;
	RseNode* rseNode = nodeAs<RseNode>(input);
	const bool isRecursive = rseNode && (rseNode->dsqlFlags & RecordSourceNode::DFLAG_RECURSIVE);
	AutoSetRestore<USHORT> autoScopeLevel(&dsqlScratch->scopeLevel, dsqlScratch->scopeLevel);

	if (isRecursive)
	{
		fb_assert(dsqlScratch->recursiveCtx);
		save_alias = dsqlScratch->recursiveCtx->ctx_alias;

		dsqlScratch->recursiveCtx->ctx_alias.clear();
		dsqlScratch->recursiveCtx->ctx_alias.add(QualifiedName(*dsqlScratch->getNextCTEAlias()));

		// ASF: We need to reset the scope level to the same value found in the non-recursive
		// part of the query, to verify usage of aggregate functions correctly. See CORE-4322.
		dsqlScratch->scopeLevel = dsqlScratch->recursiveCtx->ctx_scope_level;
	}

	RseNode* ret = pass1_rse_impl(dsqlScratch, input, order, rows, updateLock, skipLocked, flags);

	if (isRecursive)
		dsqlScratch->recursiveCtx->ctx_alias = save_alias;

	return ret;
}


// Compile a record selection expression. The input node may either be a "select_expression"
// or a "list" (an implicit union) or a "query specification".
static RseNode* pass1_rse_impl(DsqlCompilerScratch* dsqlScratch, RecordSourceNode* input,
	ValueListNode* order, RowsClause* rows, bool updateLock, bool skipLocked, USHORT flags)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);

	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	if (auto selNode = nodeAs<SelectExprNode>(input))
	{
		WithClause* withClause = selNode->withClause;
		try
		{
			if (withClause)
				dsqlScratch->addCTEs(withClause);

			RseNode* ret = pass1_rse(dsqlScratch, selNode->querySpec, selNode->orderClause,
				selNode->rowsClause, updateLock, skipLocked, selNode->dsqlFlags);

			if (withClause)
			{
				dsqlScratch->checkUnusedCTEs();
				dsqlScratch->clearCTEs();
			}

			return ret;
		}
		catch (const Exception&)
		{
			if (withClause)
				dsqlScratch->clearCTEs();
			throw;
		}
	}
	else if (auto unionNode = nodeAs<UnionSourceNode>(input))
	{
		fb_assert(unionNode->dsqlClauses->items.hasData());
		return pass1_union(dsqlScratch, unionNode, order, rows, updateLock, skipLocked, flags);
	}

	RseNode* inputRse = nodeAs<RseNode>(input);
	fb_assert(inputRse);

	if (inputRse->dsqlGrouping && inputRse->dsqlGrouping->hasAdvancedGrouping())
		return pass1_lower_grouping_sets(dsqlScratch, inputRse, order, rows, updateLock, skipLocked, flags);

	AutoSetRestore<GroupingSpec*> autoGroupingSpec(&dsqlScratch->activeGroupingSpec,
		inputRse->dsqlGroupingSpec.getObject());
	AutoSetRestore<const SortedArray<unsigned>*> autoGroupingSet(&dsqlScratch->activeGroupingSet,
		inputRse->dsqlGroupingSet);
	AutoSetRestore<ValueListNode*> autoGroupingSelectList(&dsqlScratch->activeGroupingSelectList,
		inputRse->dsqlGroupingSelectList.getObject());
	AutoSetRestore<USHORT> autoGroupingScopeLevel(&dsqlScratch->activeGroupingScopeLevel,
		dsqlScratch->scopeLevel);

	// Save the original base of the context stack and process relations

	RseNode* targetRse = FB_NEW_POOL(pool) RseNode(pool);
	RseNode* rse = targetRse;

	if (updateLock)
		rse->flags |= RseNode::FLAG_WRITELOCK;

	if (skipLocked)
		rse->flags |= RseNode::FLAG_SKIP_LOCKED;

	{
		ActiveGroupingContextDisabler disableGrouping(dsqlScratch);

		rse->dsqlStreams = Node::doDsqlPass(dsqlScratch, inputRse->dsqlFrom, false);
	}

	RecSourceListNode* streamList = rse->dsqlStreams;

	{ // scope block
		RelationSourceNode* relNode;
		const dsql_rel* relation;

		if (updateLock &&
			(streamList->items.getCount() != 1 ||
				!(relNode = nodeAs<RelationSourceNode>(streamList->items[0])) ||
				!(relation = relNode->dsqlContext->ctx_relation) ||
				(relation->rel_flags & (REL_view | REL_external))))
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_wlock_simple));
		}
	} // end scope block

	// Process LIMIT and/or ROWS, if any

	if ((inputRse->dsqlFirst || inputRse->dsqlSkip) && rows)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_firstskip_rows));
	}
	else if (rows)
		PASS1_limit(dsqlScratch, rows->length, rows->skip, rse);
	else if (inputRse->dsqlFirst || inputRse->dsqlSkip)
		PASS1_limit(dsqlScratch, inputRse->dsqlFirst, inputRse->dsqlSkip, rse);

	// Process boolean, if any

	if (inputRse->dsqlWhere)
	{
		++dsqlScratch->inWhereClause;
		rse->dsqlWhere = Node::doDsqlPass(dsqlScratch, inputRse->dsqlWhere, false);
		--dsqlScratch->inWhereClause;

		// AB: An aggregate pointing to it's own parent_context isn't
		// allowed, HAVING should be used instead
		if (Aggregate2Finder::find(dsqlScratch->getPool(), dsqlScratch->scopeLevel,
				FIELD_MATCH_TYPE_EQUAL, false, rse->dsqlWhere))
		{
			// Cannot use an aggregate in a WHERE clause, use HAVING instead
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_agg_where_err));
		}
	}

#ifdef DSQL_DEBUG
	if (DSQL_debug & 16)
	{
		dsql_trace("PASS1_rse input tree:");
		//// TODO: Implement Node::print correctly.
	}
#endif

	// Process select list, if any. If not, generate one
	ValueListNode* selectList = inputRse->dsqlSelectList;
	// First expand select list, this will expand nodes with asterisk.
	++dsqlScratch->inSelectList;
	selectList = PASS1_expand_select_list(dsqlScratch, selectList, rse->dsqlStreams);

	ValueListNode* groupingSelectList = selectList;
	FB_SIZE_T visibleSelectItems = 0;

	if (inputRse->dsqlGroupingOrder)
	{
		visibleSelectItems = selectList->items.getCount();
		groupingSelectList = pass1_copy_grouping_select_list(pool, selectList);
		pass1_append_grouping_order_select_items(dsqlScratch, pool, selectList,
			inputRse->dsqlGroupingOrder);
	}

	if (inputRse->dsqlGroupingSpec)
		dsqlScratch->activeGroupingSelectList = groupingSelectList;

	if ((flags & RecordSourceNode::DFLAG_VALUE) &&
		(!selectList || (visibleSelectItems ? visibleSelectItems : selectList->items.getCount()) > 1))
	{
		// More than one column (or asterisk) is specified in column_singleton
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_command_err) <<
				  Arg::Gds(isc_dsql_count_mismatch));
	}

	if (inputRse->dsqlNamedWindows)
	{
		for (NamedWindowsClause::iterator i = inputRse->dsqlNamedWindows->begin();
			 i != inputRse->dsqlNamedWindows->end();
			 ++i)
		{
			if (dsqlScratch->context->object()->ctx_named_windows.exist(i->first))
			{
				ERRD_post(
					Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					Arg::Gds(isc_dsql_window_duplicate) << i->first);
			}

			i->second->dsqlPass(dsqlScratch);
			dsqlScratch->context->object()->ctx_named_windows.put(i->first, i->second);
		}
	}

	// Pass select list
	rse->dsqlSelectList = pass1_sel_list(dsqlScratch, selectList);
	--dsqlScratch->inSelectList;

	if (inputRse->dsqlFlags & RecordSourceNode::DFLAG_RECURSIVE)
	{
		if (Aggregate2Finder::find(dsqlScratch->getPool(), dsqlScratch->scopeLevel, FIELD_MATCH_TYPE_EQUAL, false,
				rse->dsqlSelectList))
		{
			// Recursive member of CTE cannot use aggregate function
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_cte_recursive_aggregate));
		}
	}

	// Process ORDER clause, if any
	if (order)
	{
		++dsqlScratch->inOrderByClause;
		rse->dsqlOrder = PASS1_sort(dsqlScratch, order, selectList);
		--dsqlScratch->inOrderByClause;

		if (inputRse->dsqlFlags & RecordSourceNode::DFLAG_RECURSIVE)
		{
			if (Aggregate2Finder::find(dsqlScratch->getPool(), dsqlScratch->scopeLevel, FIELD_MATCH_TYPE_EQUAL, false,
					rse->dsqlOrder))
			{
				// Recursive member of CTE cannot use aggregate function
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_cte_recursive_aggregate));
			}
		}
	}

	// A GROUP BY, HAVING, or any aggregate function in the select list
	// will force an aggregate
	dsql_ctx* parent_context = NULL;
	RseNode* parentRse = NULL;
	AggregateSourceNode* aggregate = NULL;

	if (inputRse->dsqlGroup ||
		inputRse->dsqlHaving ||
		(rse->dsqlSelectList && AggregateFinder::find(dsqlScratch->getPool(), dsqlScratch, false, rse->dsqlSelectList)) ||
		(rse->dsqlOrder && AggregateFinder::find(dsqlScratch->getPool(), dsqlScratch, false, rse->dsqlOrder)))
	{
		// dimitr: don't allow WITH LOCK for aggregates
		if (updateLock)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_wlock_aggregates));
		}

		parent_context = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_ctx(*tdbb->getDefaultPool());
		parent_context->ctx_scope_level = dsqlScratch->scopeLevel;

		// When we're in a outer-join part mark context for it.
		if (dsqlScratch->inOuterJoin)
			parent_context->ctx_flags |= CTX_outer_join;
		parent_context->ctx_in_outer_join = dsqlScratch->inOuterJoin;

		aggregate = FB_NEW_POOL(pool) AggregateSourceNode(pool);
		aggregate->dsqlContext = parent_context;
		aggregate->dsqlRse = rse;
		parentRse = targetRse = FB_NEW_POOL(pool) RseNode(pool);
		parentRse->dsqlStreams = (streamList = FB_NEW_POOL(pool) RecSourceListNode(pool, 1));
		streamList->items[0] = aggregate;

		if (rse->dsqlFirst)
		{
			parentRse->dsqlFirst = rse->dsqlFirst;
			rse->dsqlFirst = NULL;
		}

		if (rse->dsqlSkip)
		{
			parentRse->dsqlSkip = rse->dsqlSkip;
			rse->dsqlSkip = NULL;
		}

		dsqlScratch->context->push(parent_context);
		// replace original contexts with parent context
		remap_streams_to_parent_context(rse->dsqlStreams, parent_context);
	}

	// Process GROUP BY clause, if any
	if (inputRse->dsqlGroup)
	{
		// if there are positions in the group by clause then replace them
		// by the (newly pass) items from the select_list
		++dsqlScratch->inGroupByClause;
		aggregate->dsqlGroup = pass1_group_by_list(dsqlScratch, inputRse->dsqlGroup,
			groupingSelectList);
		--dsqlScratch->inGroupByClause;

		// GROUP BY () still forces aggregation, but has no grouping keys.
		if (!aggregate->dsqlGroup->items.hasData())
			aggregate->dsqlGroup = NULL;

		// AB: An field pointing to another parent_context isn't
		// allowed and GROUP BY items can't contain aggregates
		if (FieldFinder::find(dsqlScratch->getPool(), dsqlScratch->scopeLevel, FIELD_MATCH_TYPE_LOWER, aggregate->dsqlGroup) ||
			Aggregate2Finder::find(dsqlScratch->getPool(), dsqlScratch->scopeLevel, FIELD_MATCH_TYPE_LOWER_EQUAL,
				false, aggregate->dsqlGroup))
		{
			// Cannot use an aggregate in a GROUP BY clause
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_agg_group_err));
		}
	}

	GroupingSpec* groupingSpec = inputRse->dsqlGroupingSpec.getObject();
	const char* missingGroupingContext =
		"GROUPING is only allowed with ROLLUP, CUBE or GROUPING SETS";

	pass1_validate_grouping_expressions(dsqlScratch, groupingSpec, inputRse->dsqlGroupingSet,
		groupingSelectList,
		rse->dsqlSelectList,
		missingGroupingContext);
	pass1_validate_grouping_expressions(dsqlScratch, groupingSpec, inputRse->dsqlGroupingSet,
		groupingSelectList, rse->dsqlOrder, missingGroupingContext);

	// Parse a user-specified access PLAN
	if (inputRse->rse_plan)
	{
		PsqlChanger changer(dsqlScratch, false);
		rse->rse_plan = inputRse->rse_plan->dsqlPass(dsqlScratch);
	}

	// AB: Pass select-items for distinct operation again, because for
	// sub-selects a new context number should be generated
	if (inputRse->dsqlDistinct)
	{
		if (updateLock)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_wlock_conflict) << Arg::Str("DISTINCT"));
		}

		++dsqlScratch->inSelectList;
		targetRse->dsqlDistinct = pass1_sel_list(dsqlScratch, selectList);
		--dsqlScratch->inSelectList;

		// sort, group and distinct have the same limit for now
		if (selectList->items.getCount() > MAX_SORT_ITEMS)
		{
			// Cannot have more than 255 items in DISTINCT / UNION DISTINCT list.
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  Arg::Gds(isc_dsql_max_distinct_items));
		}
	}

	if (parent_context)
	{
		FieldRemapper remapper(dsqlScratch->getPool(), dsqlScratch, parent_context, false);

		// Reset context of select items to point to the parent stream

		ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlSelectList, rse->dsqlSelectList);
		rse->dsqlSelectList = NULL;

		// AB: Check for invalid constructions inside selected-items list
		ValueListNode* valueList = parentRse->dsqlSelectList;

		{ // scope block
			NestConst<ValueExprNode>* ptr = valueList->items.begin();
			for (const NestConst<ValueExprNode>* const end = valueList->items.end(); ptr != end; ++ptr)
			{
				if (InvalidReferenceFinder::find(dsqlScratch, parent_context, aggregate->dsqlGroup, *ptr))
				{
					// Invalid expression in the select list
					// (not contained in either an aggregate or the GROUP BY clause)
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  Arg::Gds(isc_dsql_agg_column_err) << Arg::Str("select list"));
				}
			}
		} // end scope block

		// Reset context of order items to point to the parent stream

		if (order)
		{
			ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlOrder, rse->dsqlOrder);
			rse->dsqlOrder = NULL;

			// AB: Check for invalid constructions inside the ORDER BY clause
			ValueListNode* valueList = targetRse->dsqlOrder;
			NestConst<ValueExprNode>* ptr = valueList->items.begin();
			for (const NestConst<ValueExprNode>* const end = valueList->items.end(); ptr != end; ++ptr)
			{
				if (InvalidReferenceFinder::find(dsqlScratch, parent_context, aggregate->dsqlGroup, *ptr))
				{
					// Invalid expression in the ORDER BY clause
					// (not contained in either an aggregate or the GROUP BY clause)
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  Arg::Gds(isc_dsql_agg_column_err) << Arg::Str("ORDER BY clause"));
				}
			}
		}

		// And, of course, reduction clauses must also apply to the parent
		if (inputRse->dsqlDistinct)
			ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlDistinct);

		// Process HAVING clause, if any

		if (inputRse->dsqlHaving)
		{
			++dsqlScratch->inHavingClause;
			parentRse->dsqlWhere = Node::doDsqlPass(dsqlScratch, inputRse->dsqlHaving, false);
			--dsqlScratch->inHavingClause;

			pass1_validate_grouping_expressions(dsqlScratch, groupingSpec,
				inputRse->dsqlGroupingSet, groupingSelectList, parentRse->dsqlWhere,
				missingGroupingContext);

			ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlWhere);

			// AB: Check for invalid constructions inside the HAVING clause

			if (InvalidReferenceFinder::find(dsqlScratch, parent_context, aggregate->dsqlGroup,
					parentRse->dsqlWhere))
			{
				// Invalid expression in the HAVING clause
				// (neither an aggregate nor contained in the GROUP BY clause)
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_agg_having_err) << Arg::Str("HAVING clause"));
			}

			if (AggregateFinder::find(dsqlScratch->getPool(), dsqlScratch, true, parentRse->dsqlWhere))
			{
				// Cannot use an aggregate in a WHERE clause, use HAVING instead
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_agg_where_err));
			}
		}

		rse = parentRse;

		parent_context->ctx_context = dsqlScratch->contextNumber++;
	}

	bool isWindow = (rse->dsqlOrder && AggregateFinder::find(dsqlScratch->getPool(), dsqlScratch, true, rse->dsqlOrder)) ||
		(rse->dsqlSelectList && AggregateFinder::find(dsqlScratch->getPool(), dsqlScratch, true, rse->dsqlSelectList)) ||
		inputRse->dsqlNamedWindows;

	// WINDOW functions
	if (isWindow)
	{
		AutoSetRestore<bool> autoProcessingWindow(&dsqlScratch->processingWindow, true);

		parent_context = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_ctx(*tdbb->getDefaultPool());
		parent_context->ctx_scope_level = dsqlScratch->scopeLevel;

		// When we're in a outer-join part mark context for it.
		if (dsqlScratch->inOuterJoin)
			parent_context->ctx_flags |= CTX_outer_join;
		parent_context->ctx_in_outer_join = dsqlScratch->inOuterJoin;

		AggregateSourceNode* window = FB_NEW_POOL(pool) AggregateSourceNode(pool);
		window->dsqlContext = parent_context;
		window->dsqlRse = rse;
		window->dsqlWindow = true;

		parentRse = targetRse = FB_NEW_POOL(pool) RseNode(pool);
		parentRse->dsqlStreams = streamList = FB_NEW_POOL(pool) RecSourceListNode(pool, 1);
		streamList->items[0] = window;

		if (rse->hasWriteLock())
		{
			parentRse->flags |= RseNode::FLAG_WRITELOCK;
			rse->flags &= ~RseNode::FLAG_WRITELOCK;
		}

		if (rse->hasSkipLocked())
		{
			parentRse->flags |= RseNode::FLAG_SKIP_LOCKED;
			rse->flags &= ~RseNode::FLAG_SKIP_LOCKED;
		}

		if (rse->dsqlFirst)
		{
			parentRse->dsqlFirst = rse->dsqlFirst;
			rse->dsqlFirst = NULL;
		}

		if (rse->dsqlSkip)
		{
			parentRse->dsqlSkip = rse->dsqlSkip;
			rse->dsqlSkip = NULL;
		}

		dsqlScratch->context->push(parent_context);
		// replace original contexts with parent context
		remap_streams_to_parent_context(rse->dsqlStreams, parent_context);

		if (aggregate)
		{
			// Check for invalid constructions inside selected-items list
			ValueListNode* valueList = rse->dsqlSelectList;
			NestConst<ValueExprNode>* ptr = valueList->items.begin();
			for (const NestConst<ValueExprNode>* const end = valueList->items.end(); ptr != end; ++ptr)
			{
				if (InvalidReferenceFinder::find(dsqlScratch, parent_context, aggregate->dsqlGroup, *ptr))
				{
					// Invalid expression in the select list
					// (not contained in either an aggregate or the GROUP BY clause)
					ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
							  Arg::Gds(isc_dsql_agg_column_err) << Arg::Str("select list"));
				}
			}
		}

		FieldRemapper remapper(dsqlScratch->getPool(), dsqlScratch, parent_context, true);

		ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlSelectList, rse->dsqlSelectList);
		rse->dsqlSelectList = NULL;

		if (order)
		{
			if (aggregate)
			{
				// Check for invalid constructions inside the order-by list
				ValueListNode* valueList = rse->dsqlOrder;
				NestConst<ValueExprNode>* ptr = valueList->items.begin();
				for (const NestConst<ValueExprNode>* const end = valueList->items.end(); ptr != end; ++ptr)
				{
					if (InvalidReferenceFinder::find(dsqlScratch, parent_context, aggregate->dsqlGroup, *ptr))
					{
						// Invalid expression in the ORDER BY list
						// (not contained in either an aggregate or the GROUP BY clause)
						ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
								  Arg::Gds(isc_dsql_agg_column_err) << Arg::Str("ORDER BY list"));
					}
				}
			}

			ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlOrder, rse->dsqlOrder);
			rse->dsqlOrder = NULL;
		}

		// And, of course, reduction clauses must also apply to the parent
		if (rse->dsqlDistinct)
		{
			ExprNode::doDsqlFieldRemapper(remapper, parentRse->dsqlDistinct, rse->dsqlDistinct);
			rse->dsqlDistinct = NULL;
		}

		// Remap the nodes to the partition context
		for (FB_SIZE_T i = 0, mapCount = parent_context->ctx_win_maps.getCount(); i < mapCount; ++i)
		{
			WindowMap* windowMap = parent_context->ctx_win_maps[i];

			if (windowMap->window && windowMap->window->partition)
			{
				windowMap->partitionRemapped = Node::doDsqlPass(dsqlScratch,
					windowMap->window->partition);

				FieldRemapper remapper2(dsqlScratch->getPool(), dsqlScratch, parent_context, true, windowMap->window);
				ExprNode::doDsqlFieldRemapper(remapper2, windowMap->partitionRemapped);
			}
		}

		rse = parentRse;
	}

	if (visibleSelectItems)
		rse->dsqlVisibleSelectItems = visibleSelectItems;

	rse->dsqlFlags = flags;

	return rse;
}


/**

 	pass1_sel_list

    @brief	Compile a select list.

    @param dsqlScratch
    @param input

 **/
static ValueListNode* pass1_sel_list(DsqlCompilerScratch* dsqlScratch, ValueListNode* input)
{
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	DEV_BLKCHK(dsqlScratch, dsql_type_req);

	ValueListNode* retList = FB_NEW_POOL(pool) ValueListNode(pool, 0u);

	NestConst<ValueExprNode>* ptr = input->items.begin();
	for (const NestConst<ValueExprNode>* const end = input->items.end(); ptr != end; ++ptr)
	{
		ValueExprNode* value = Node::doDsqlPass(dsqlScratch, *ptr, false);

		if ((nodeIs<GroupingNode>(*ptr) || nodeIs<GroupingIdNode>(*ptr)) &&
			!nodeIs<DsqlAliasNode>(value))
		{
			DsqlAliasNode* aliasNode = FB_NEW_POOL(pool) DsqlAliasNode(pool,
				nodeIs<GroupingIdNode>(*ptr) ? MetaName("GROUPING_ID") : MetaName("GROUPING"),
				value);
			aliasNode->dsqlGroupingExpression = *ptr;
			value = aliasNode;
		}

		retList->add(value);
	}

	return retList;
}


// Process ORDER BY list, which may contain an ordinal or alias which references the select list.
ValueListNode* PASS1_sort(DsqlCompilerScratch* dsqlScratch, ValueListNode* input, ValueListNode* selectList)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);
	DEV_BLKCHK(input, dsql_type_nod);
	DEV_BLKCHK(selectList, dsql_type_nod);

	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	if (!input)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_command_err) <<
				  // invalid ORDER BY clause
				  Arg::Gds(isc_order_by_err));
	}

	if (input->items.getCount() > MAX_SORT_ITEMS)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_command_err) <<
				  Arg::Gds(isc_order_by_err) <<
				  // invalid ORDER BY clause, cannot sort on more than 255 items
				  Arg::Gds(isc_dsql_max_sort_items));
	}

	// Node is simply to be rebuilt -- just recurse merrily

	NestConst<ValueListNode> node = FB_NEW_POOL(pool) ValueListNode(pool, input->items.getCount());
	NestConst<ValueExprNode>* ptr2 = node->items.begin();

	for (FB_SIZE_T sortloop = 0; sortloop < input->items.getCount(); ++sortloop)
	{
		DEV_BLKCHK(input->items[sortloop], dsql_type_nod);
		NestConst<OrderNode> node1 = nodeAs<OrderNode>(input->items[sortloop]);
		if (!node1)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  // invalid ORDER BY clause
					  Arg::Gds(isc_order_by_err));
		}

		// get node of value to be ordered by
		NestConst<ValueExprNode> orderValue = node1->value;

		NestConst<CollateNode> collateNode = nodeAs<CollateNode>(orderValue);

		if (collateNode)
		{
			// substitute CollateNode with its argument (real value)
			orderValue = collateNode->arg;
		}

		FieldNode* field;
		LiteralNode* literal;

		if ((field = nodeAs<FieldNode>(orderValue)))
		{
			ValueExprNode* aliasNode = NULL;

			// check for alias or field node
			if (selectList && field->dsqlQualifier.object.isEmpty() && field->dsqlName.hasData())
			{
				// AB: Check first against the select list for matching column.
				// When no matches at all are found we go on with our
				// normal way of field name lookup.
				aliasNode = PASS1_lookup_alias(dsqlScratch, field->dsqlName, selectList, true);
			}

			orderValue = aliasNode ? aliasNode : field->internalDsqlPass(dsqlScratch, NULL);
		}
		else if ((literal = nodeAs<LiteralNode>(orderValue)) && literal->litDesc.dsc_dtype == dtype_long)
		{
			const ULONG position = literal->getSlong();

			if (position < 1 || !selectList || position > (ULONG) selectList->items.getCount())
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  // Invalid column position used in the ORDER BY clause
						  Arg::Gds(isc_dsql_column_pos_err) << Arg::Str("ORDER BY"));
			}

			// substitute ordinal with appropriate field
			orderValue = Node::doDsqlPass(dsqlScratch, selectList->items[position - 1], false);
		}
		else
			orderValue = Node::doDsqlPass(dsqlScratch, orderValue, false);

		if (collateNode)
		{
			// Finally apply collation order, if necessary.
			orderValue = CollateNode::pass1Collate(dsqlScratch, orderValue, collateNode->collation);
		}

		OrderNode* node2 = FB_NEW_POOL(pool) OrderNode(pool, orderValue);
		node2->descending = node1->descending;
		node2->nullsPlacement = node1->nullsPlacement;

		// store actual value to be ordered by
		*ptr2++ = node2;
	}

	return node;
}


// Handle a UNION of substreams, generating a mapping of all the fields and adding an implicit
// PROJECT clause to ensure that all the records returned are unique.
static RseNode* pass1_union(DsqlCompilerScratch* dsqlScratch, UnionSourceNode* input,
	ValueListNode* orderList, RowsClause* rows, bool updateLock, bool skipLocked, USHORT flags)
{
	DEV_BLKCHK(dsqlScratch, dsql_type_req);

	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	if (updateLock && !input->dsqlAll)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  Arg::Gds(isc_dsql_wlock_conflict) << Arg::Str("UNION"));
	}

	// set up the rse node for the union.

	UnionSourceNode* unionSource = FB_NEW_POOL(pool) UnionSourceNode(pool);
	unionSource->dsqlAll = input->dsqlAll;
	unionSource->dsqlOrderBySelectList = input->dsqlOrderBySelectList;
	unionSource->dsqlDistinctGroupingSets = input->dsqlDistinctGroupingSets;
	unionSource->recursive = input->recursive;

	RseNode* unionRse = FB_NEW_POOL(pool) RseNode(pool);

	unionRse->dsqlStreams = FB_NEW_POOL(pool) RecSourceListNode(pool, 1);
	unionRse->dsqlStreams->items[0] = unionSource;
	unionSource->dsqlParentRse = unionRse;

	// generate a context for the union itself.
	dsql_ctx* union_context = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_ctx(*tdbb->getDefaultPool());

	if (input->recursive)
		union_context->ctx_context = dsqlScratch->recursiveCtxId;
	else
		union_context->ctx_context = dsqlScratch->contextNumber++;

	union_context->ctx_scope_level = dsqlScratch->scopeLevel;

	// When we're in a outer-join part mark context for it.
	if (dsqlScratch->inOuterJoin)
		union_context->ctx_flags |= CTX_outer_join;
	union_context->ctx_in_outer_join = dsqlScratch->inOuterJoin;

	dsqlScratch->context->push(union_context);

	unionSource->dsqlClauses = FB_NEW_POOL(pool) RecSourceListNode(pool,
		input->dsqlClauses->items.getCount());

	// process all the sub-rse's.
	{ // scope block
		NestConst<RecordSourceNode>* uptr = unionSource->dsqlClauses->items.begin();
		const DsqlContextStack::const_iterator base(*dsqlScratch->context);
		NestConst<RecordSourceNode>* ptr = input->dsqlClauses->items.begin();

		for (const NestConst<RecordSourceNode>* const end = input->dsqlClauses->items.end();
			 ptr != end;
			 ++ptr, ++uptr)
		{
			dsqlScratch->scopeLevel++;
			*uptr = pass1_rse(dsqlScratch, *ptr, NULL, NULL, updateLock, skipLocked, 0);
			dsqlScratch->scopeLevel--;

			if (updateLock)
				nodeAs<RseNode>(*uptr)->flags &= ~RseNode::FLAG_WRITELOCK;

			if (skipLocked)
				nodeAs<RseNode>(*uptr)->flags &= ~RseNode::FLAG_SKIP_LOCKED;

			while (*(dsqlScratch->context) != base)
				dsqlScratch->unionContext.push(dsqlScratch->context->pop());

			// Push recursive context after initial select has been processed.
			// Corresponding pop occurs in pass1_derived_table
			if (input->recursive && (ptr == input->dsqlClauses->items.begin()))
				dsqlScratch->context->push(dsqlScratch->recursiveCtx);
		}
	} // end scope block

	if (unionSource->dsqlDistinctGroupingSets)
		unionSource->dsqlClauses = pass1_deduplicate_grouping_union_clauses(dsqlScratch, pool,
			unionSource->dsqlClauses);

	// generate the list of fields to select.
	ValueListNode* items = nodeAs<RseNode>(unionSource->dsqlClauses->items[0])->dsqlSelectList;

	if (!input->dsqlAll && items->items.getCount() > MAX_SORT_ITEMS)
	{
		// Cannot have more than 255 items in DISTINCT / UNION DISTINCT list.
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					Arg::Gds(isc_dsql_command_err) <<
					Arg::Gds(isc_dsql_max_distinct_items));
	}

	// loop through the list nodes, checking to be sure that they have the
	// same number of items

	for (FB_SIZE_T i = 1; i < unionSource->dsqlClauses->items.getCount(); ++i)
	{
		const ValueListNode* nod1 = nodeAs<RseNode>(unionSource->dsqlClauses->items[i])->dsqlSelectList;

		if (items->items.getCount() != nod1->items.getCount())
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_command_err) <<
					  // overload of msg
					  Arg::Gds(isc_dsql_count_mismatch));
		}
	}

	FB_SIZE_T visibleSelectItems = 0;

	for (FB_SIZE_T i = 0; i < unionSource->dsqlClauses->items.getCount(); ++i)
	{
		const RseNode* clause = nodeAs<RseNode>(unionSource->dsqlClauses->items[i]);

		if (clause->dsqlVisibleSelectItems)
		{
			if (!visibleSelectItems)
				visibleSelectItems = clause->dsqlVisibleSelectItems;
			else
				fb_assert(visibleSelectItems == clause->dsqlVisibleSelectItems);
		}
	}

	// Comment below belongs to the old code (way a union was handled).

	// SQL II, section 9.3, pg 195 governs which data types
	// are considered equivalent for a UNION
	// The following restriction is in some ways more restrictive
	// (cannot UNION CHAR with VARCHAR for instance)
	// (or cannot union CHAR of different lengths)
	// and in someways less restrictive
	// (SCALE is not looked at)
	// Workaround: use a direct CAST() dsqlScratch in the SQL
	// dsqlScratch to force desired datatype.

	// loop through the list nodes and cast whenever possible.
	ValueListNode* tmp_list = FB_NEW_POOL(pool) ValueListNode(
		pool, unionSource->dsqlClauses->items.getCount());

	Array<dsc> descs(unionSource->dsqlClauses->items.getCount());

	for (FB_SIZE_T j = 0; j < items->items.getCount(); ++j)
	{
		for (FB_SIZE_T i = 0; i < unionSource->dsqlClauses->items.getCount(); ++i)
		{
			ValueListNode* nod1 = nodeAs<RseNode>(unionSource->dsqlClauses->items[i])->dsqlSelectList;
			DsqlDescMaker::fromNode(dsqlScratch, nod1->items[j]);
			tmp_list->items[i] = nod1->items[j];
		}

		dsc desc;
		DsqlDescMaker::fromList(dsqlScratch, &desc, tmp_list, "UNION");

		descs.push(desc);

		pass1_union_auto_cast(dsqlScratch, unionSource->dsqlClauses, desc, j);
	}

	items = nodeAs<RseNode>(unionSource->dsqlClauses->items[0])->dsqlSelectList;

	// Create mappings for union.

	ValueListNode* union_items = FB_NEW_POOL(pool) ValueListNode(pool, items->items.getCount());

	{ // scope block
		USHORT count = 0;
		NestConst<ValueExprNode>* uptr = items->items.begin();
		NestConst<ValueExprNode>* ptr = union_items->items.begin();

		for (const NestConst<ValueExprNode>* const end = union_items->items.end(); ptr != end; ++ptr)
		{
			// Set up the dsql_map* between the sub-rses and the union context.
			dsql_map* map = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_map;
			map->map_position = count++;
			fb_assert(count != 0); // no wrap, please!
			map->map_node = *uptr++;
			map->map_next = union_context->ctx_map;
			map->map_window = NULL;
			union_context->ctx_map = map;

			auto mapNode = FB_NEW_POOL(pool) DsqlMapNode(pool, union_context, map);

			if (descs[map->map_position].dsc_flags & DSC_nullable)
				mapNode->setNullable = true;

			if ((items->items[map->map_position]->getDsqlDesc().dsc_flags & DSC_null) &&
				!(descs[map->map_position].dsc_flags & DSC_null))
			{
				mapNode->clearNull = true;
			}

			*ptr = mapNode;
		}

		unionRse->dsqlSelectList = union_items;
	} // end scope block

	// Process ORDER clause, if any.
	if (orderList)
	{
		ValueListNode* sort = FB_NEW_POOL(pool) ValueListNode(pool, orderList->items.getCount());
		NestConst<ValueExprNode>* uptr = sort->items.begin();
		NestConst<ValueExprNode>* ptr = orderList->items.begin();

		for (const NestConst<ValueExprNode>* const end = orderList->items.end();
			 ptr != end;
			 ++ptr, ++uptr)
		{
			const auto order1 = nodeAs<OrderNode>(*ptr);
			auto position = order1->value;
			const auto collateNode = nodeAs<CollateNode>(position);

			if (collateNode)
				position = collateNode->arg;

			const LiteralNode* literal = nodeAs<LiteralNode>(position);
			SLONG number = 0;

			if (literal && literal->litDesc.dsc_dtype == dtype_long)
				number = literal->getSlong();
			else if (!input->dsqlOrderBySelectList ||
				!pass1_find_raw_order_position(dsqlScratch, position, items,
					items->items.getCount(), number))
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_command_err) <<
						  // invalid ORDER BY clause.
						  Arg::Gds(isc_order_by_err));
			}

			if (number < 1 || ULONG(number) > union_items->items.getCount())
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_command_err) <<
						  // invalid ORDER BY clause.
						  Arg::Gds(isc_order_by_err));
			}

			// make a new order node pointing at the Nth item in the select list.
			OrderNode* order2 = FB_NEW_POOL(pool) OrderNode(pool, union_items->items[number - 1]);
			*uptr = order2;
			order2->descending = order1->descending;

			if (collateNode)
			{
				order2->value = CollateNode::pass1Collate(dsqlScratch,
					order2->value, collateNode->collation);
			}

			order2->nullsPlacement = order1->nullsPlacement;
		}

		unionRse->dsqlOrder = sort;
	}

	if (rows)
		PASS1_limit(dsqlScratch, rows->length, rows->skip, unionRse);

	// PROJECT on all the select items unless UNION ALL was specified.
	if (!input->dsqlAll)
		unionRse->dsqlDistinct = union_items;

	if (updateLock)
		unionRse->flags |= RseNode::FLAG_WRITELOCK;

	if (skipLocked)
		unionRse->flags |= RseNode::FLAG_SKIP_LOCKED;

	if (visibleSelectItems && visibleSelectItems < unionRse->dsqlSelectList->items.getCount())
		unionRse->dsqlSelectList = pass1_trim_grouping_select_list(pool,
			unionRse->dsqlSelectList, visibleSelectItems);

	unionRse->dsqlFlags = flags;

	return unionRse;
}


/**

 	pass1_union_auto_cast

    @brief	Auto cast types to the same type by the rules from
	MAKE_desc_from_list. SELECT X1 FROM .. UNION SELECT X2 FROM ..
	Items X1..Xn are collected together to make the cast-descriptor, this
	was done by the caller (param desc and input is the collection).
	Then is a cast generated (or reused) for every X item if it has
	another descriptor than the param desc.
	Position tells us which column-nr we are processing.

    @param input
    @param desc
    @param position

 **/
static void pass1_union_auto_cast(DsqlCompilerScratch* dsqlScratch, ExprNode* input,
	const dsc& desc, FB_SIZE_T position)
{
	thread_db* tdbb = JRD_get_thread_data();

	RecSourceListNode* recSourceList;
	RseNode* rseNode;
	UnionSourceNode* unionNode;

	if ((recSourceList = nodeAs<RecSourceListNode>(input)))
	{
		NestConst<RecordSourceNode>* ptr = recSourceList->items.begin();
		for (const NestConst<RecordSourceNode>* const end = recSourceList->items.end(); ptr != end; ++ptr)
			pass1_union_auto_cast(dsqlScratch, *ptr, desc, position);
	}
	else if ((rseNode = nodeAs<RseNode>(input)) && !rseNode->dsqlExplicitJoin &&
		!rseNode->dsqlContext)	// not derived table
	{
		pass1_union_auto_cast(dsqlScratch, rseNode->dsqlStreams, desc, position);

		if (rseNode->dsqlStreams->items.getCount() == 1 &&
			(unionNode = nodeAs<UnionSourceNode>(rseNode->dsqlStreams->items[0])) &&
			unionNode->dsqlParentRse == rseNode)
		{
			// We're now in a UNION under a UNION (for example A UNION ALL B UNION C) so don't
			// change the existing mappings.
			// Only replace the node where the map points to, because they could be changed.
			ValueListNode* sub_rse_items =
				nodeAs<RseNode>(unionNode->dsqlClauses->items[0])->dsqlSelectList;
			auto mapNode = nodeAs<DsqlMapNode>(rseNode->dsqlSelectList->items[position]);
			dsql_map* map = mapNode->map;
			map->map_node = sub_rse_items->items[position];
			mapNode->setDsqlDesc(desc);
		}
		else
		{
			ValueListNode* list = rseNode->dsqlSelectList;

			if (position >= list->items.getCount())
			{
				// Internal dsql error: column position out of range in pass1_union_auto_cast
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  Arg::Gds(isc_dsql_command_err) <<
						  Arg::Gds(isc_dsql_auto_field_bad_pos));
			}
			else
			{
				ValueExprNode* select_item = list->items[position];
				DsqlDescMaker::fromNode(dsqlScratch, select_item);

				if (select_item->getDsqlDesc().dsc_dtype != desc.dsc_dtype ||
					select_item->getDsqlDesc().dsc_length != desc.dsc_length ||
					select_item->getDsqlDesc().dsc_scale != desc.dsc_scale ||
					select_item->getDsqlDesc().dsc_sub_type != desc.dsc_sub_type)
				{
					// Because this select item has a different descriptor then
					// our finally descriptor CAST it.
					CastNode* castNode = NULL;
					DsqlAliasNode* newAliasNode = NULL;
					DsqlAliasNode* aliasNode;
					DerivedFieldNode* derivedField;

					// Pick a existing cast if available else make a new one.
					if ((aliasNode = nodeAs<DsqlAliasNode>(select_item)) &&
						aliasNode->value &&
						(castNode = nodeAs<CastNode>(aliasNode->value)))
					{
					}
					else if ((derivedField = nodeAs<DerivedFieldNode>(select_item)) &&
						(castNode = nodeAs<CastNode>(derivedField->value)))
					{
					}
					else
						castNode = nodeAs<CastNode>(select_item);

					if (castNode && !DSC_EQUIV(&castNode->getDsqlDesc(), &desc, false))
						castNode = NULL;

					if (!castNode)
					{
						castNode = FB_NEW_POOL(*tdbb->getDefaultPool()) CastNode(
							*tdbb->getDefaultPool());

						castNode->dsqlField = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_fld(
							*tdbb->getDefaultPool());

						// We want to leave the ALIAS node on his place, because a UNION
						// uses the select_items from the first sub-rse to determine the
						// columnname.
						if ((aliasNode = nodeAs<DsqlAliasNode>(select_item)))
							castNode->source = aliasNode->value;
						else if ((derivedField = nodeAs<DerivedFieldNode>(select_item)))
							castNode->source = derivedField->value;
						else
							castNode->source = select_item;

						// When a cast is created we're losing our fieldname, thus
						// create an alias to keep it.
						ValueExprNode* name_node = select_item;
						const DsqlMapNode* mapNode;

						while ((mapNode = nodeAs<DsqlMapNode>(name_node)))
						{
							// Skip all the DsqlMapNodes.
							name_node = mapNode->map->map_node;
						}

						if (auto fieldNode = nodeAs<FieldNode>(name_node))
						{
							// Create new node for alias and copy fieldname.
							newAliasNode = FB_NEW_POOL(*tdbb->getDefaultPool()) DsqlAliasNode(
								*tdbb->getDefaultPool(), fieldNode->dsqlField->fld_name, NULL);
							// The alias value will be assigned a bit later.
						}
						else if ((derivedField = nodeAs<DerivedFieldNode>(name_node)))
						{
							// Create new node for alias and copy fieldname.
							newAliasNode = FB_NEW_POOL(*tdbb->getDefaultPool()) DsqlAliasNode(
								*tdbb->getDefaultPool(), derivedField->name, NULL);
							// The alias value will be assigned a bit later.
						}
					}

					dsql_fld* field = castNode->dsqlField;
					// Copy the descriptor to a field, because the gen_cast
					// uses a dsql field type.
					field->dtype = desc.dsc_dtype;
					field->scale = desc.dsc_scale;
					field->subType = desc.dsc_sub_type;
					field->length = desc.dsc_length;
					field->flags = (desc.dsc_flags & DSC_nullable) ? FLD_nullable : 0;

					if (desc.isText() || desc.isBlob())
					{
						field->textType = desc.getTextType();
						field->charSetId = desc.getCharSet();
						field->collationId = desc.getCollation();
					}

					// Finally copy the descriptors to the root nodes and swap
					// the necessary nodes.
					castNode->setDsqlDesc(desc);

					if ((aliasNode = nodeAs<DsqlAliasNode>(select_item)))
					{
						aliasNode->value = castNode;
						aliasNode->setDsqlDesc(desc);
					}
					else if ((derivedField = nodeAs<DerivedFieldNode>(select_item)))
					{
						derivedField->value = castNode;
						derivedField->setDsqlDesc(desc);
					}
					else
					{
						if (select_item->getDsqlDesc().dsc_flags & DSC_nullable)
						{
							dsc castDesc = castNode->getDsqlDesc();
							castDesc.dsc_flags |= DSC_nullable;
							castNode->setDsqlDesc(castDesc);
						}

						// If a new alias was created for keeping original field-name
						// make the alias the "top" node.
						if (newAliasNode)
						{
							newAliasNode->value = castNode;
							list->items[position] = newAliasNode;
						}
						else
							list->items[position] = castNode;
					}
				}
			}
		}
	}
	else if ((unionNode = nodeAs<UnionSourceNode>(input)))
	{
		recSourceList = unionNode->dsqlClauses;

		for (NestConst<RecordSourceNode>* ptr = recSourceList->items.begin();
			 ptr != recSourceList->items.end();
			 ++ptr)
		{
			pass1_union_auto_cast(dsqlScratch, *ptr, desc, position);
		}
	}
}


// Post an item to a map for a context.
DsqlMapNode* PASS1_post_map(DsqlCompilerScratch* dsqlScratch, ValueExprNode* node,
	dsql_ctx* context, WindowClause* windowNode)
{
	DEV_BLKCHK(node, dsql_type_nod);
	DEV_BLKCHK(context, dsql_type_ctx);

	thread_db* tdbb = JRD_get_thread_data();

	WindowMap* windowMap = NULL;
	dsql_map* map = NULL;

	if (dsqlScratch->processingWindow)
	{
		windowMap = context->getWindowMap(dsqlScratch, windowNode);
		map = windowMap->map;
	}
	else
		map = context->ctx_map;

	USHORT count = 0;

	while (map)
	{
		if (PASS1_node_match(dsqlScratch, node, map->map_node, false))
			break;

		++count;
		map = map->map_next;
	}

	if (!map)
	{
		dsql_map** next = windowMap ? &windowMap->map : &context->ctx_map;

		if (*next)
		{
			while (*(next = &(*next)->map_next))
				;
		}

		map = *next = FB_NEW_POOL(*tdbb->getDefaultPool()) dsql_map;
		map->map_position = count;
		map->map_node = node;
		map->map_window = windowMap;
	}

	DsqlDescMaker::fromNode(dsqlScratch, node);

	return FB_NEW_POOL(*tdbb->getDefaultPool()) DsqlMapNode(*tdbb->getDefaultPool(), context, map);
}


// For each relation in the list, flag the relation's context as having a parent context.
// Be sure to handle joins (Bug 6674).
static void remap_streams_to_parent_context(ExprNode* input, dsql_ctx* parent_context)
{
	DEV_BLKCHK(parent_context, dsql_type_ctx);

	if (auto listNode = nodeAs<RecSourceListNode>(input))
	{
		NestConst<RecordSourceNode>* ptr = listNode->items.begin();
		for (const NestConst<RecordSourceNode>* const end = listNode->items.end(); ptr != end; ++ptr)
			remap_streams_to_parent_context(*ptr, parent_context);
	}
	else if (auto procNode = nodeAs<ProcedureSourceNode>(input))
	{
		DEV_BLKCHK(procNode->dsqlContext, dsql_type_ctx);
		procNode->dsqlContext->ctx_parent = parent_context;
	}
	else if (auto relNode = nodeAs<RelationSourceNode>(input))
	{
		DEV_BLKCHK(relNode->dsqlContext, dsql_type_ctx);
		relNode->dsqlContext->ctx_parent = parent_context;
	}
	else if (auto tableValueFunctionNode = nodeAs<TableValueFunctionSourceNode>(input))
	{
		DEV_BLKCHK(tableValueFunctionNode->dsqlContext, dsql_type_ctx);
		tableValueFunctionNode->dsqlContext->ctx_parent = parent_context;
	}
	//// TODO: LocalTableSourceNode
	else if (auto rseNode = nodeAs<RseNode>(input))
		remap_streams_to_parent_context(rseNode->dsqlStreams, parent_context);
	else if (auto unionNode = nodeAs<UnionSourceNode>(input))
		remap_streams_to_parent_context(unionNode->dsqlClauses, parent_context);
	else
		fb_assert(nodeAs<AggregateSourceNode>(input));
}


// Setup the datatype of a parameter.
bool PASS1_set_parameter_type(DsqlCompilerScratch* dsqlScratch, ValueExprNode* inNode,
	std::function<void (dsc*)> makeDesc, bool forceVarchar)
{
	return inNode && inNode->setParameterType(dsqlScratch, makeDesc, forceVarchar);
}

// Setup the datatype of a parameter.
bool PASS1_set_parameter_type(DsqlCompilerScratch* dsqlScratch, ValueExprNode* inNode,
	NestConst<ValueExprNode> node, bool forceVarchar)
{
	if (!inNode)
		return false;

	auto makeDesc = [&] (dsc* desc)
		{
			DsqlDescMaker::fromNode(dsqlScratch, node);
			*desc = node->getDsqlDesc();
		};

	return inNode->setParameterType(dsqlScratch, makeDesc, forceVarchar);
}


// Returns false for hidden fields and true for non-hidden.
// For non-hidden, change "node" if the field is part of an
// implicit join.
bool dsql_ctx::getImplicitJoinField(const MetaName& name, NestConst<ValueExprNode>& node)
{
	ImplicitJoin* impJoin;
	if (ctx_imp_join.get(name, impJoin))
	{
		if (impJoin->visibleInContext == this)
		{
			node = impJoin->value;
			return true;
		}

		return false;
	}

	return true;
}

// Returns (creating, if necessary) the WindowMap of a given partition (that may be NULL).
WindowMap* dsql_ctx::getWindowMap(DsqlCompilerScratch* dsqlScratch, WindowClause* windowNode)
{
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	bool isNullWindow = windowNode == NULL;
	WindowClause nullWindow(pool, NULL, NULL, NULL, NULL, WindowClause::Exclusion::NO_OTHERS);

	if (isNullWindow)
		windowNode = &nullWindow;

	WindowMap* windowMap = NULL;

	for (Array<WindowMap*>::iterator i = ctx_win_maps.begin();
		 !windowMap && i != ctx_win_maps.end();
		 ++i)
	{
		if (PASS1_node_match(dsqlScratch, (*i)->window, windowNode, false))
		{
			windowMap = *i;
		}
	}

	if (!windowMap)
	{
		if (isNullWindow)
		{
			windowNode = FB_NEW_POOL(pool) WindowClause(pool, NULL, NULL, NULL, NULL,
				WindowClause::Exclusion::NO_OTHERS);
		}

		windowMap = FB_NEW_POOL(*tdbb->getDefaultPool()) WindowMap(windowNode);
		ctx_win_maps.add(windowMap);
		windowMap->context = dsqlScratch->contextNumber++;
	}

	return windowMap;
}
