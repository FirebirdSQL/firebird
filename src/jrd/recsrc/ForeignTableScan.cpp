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
 *  The Original Code was created by Vasiliy Yashkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 Vasiliy Yashkov <vasiliy.yashkov13@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/intl.h"
#include "../jrd/intl_proto.h"
#include "../jrd/req.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/vio_proto.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/StmtNodes.h"

#include "../jrd/ForeignServer.h"
#include "RecordSource.h"

#include "../jrd/optimizer/Optimizer.h"

using namespace Firebird;
using namespace Jrd;

ForeignTableScan::ForeignTableScan(CompilerScratch* csb, const string& alias,
									StreamType stream, jrd_rel* relation)
	: RecordStream(csb, stream),
	m_relation(relation),
	m_alias(csb->csb_pool, alias),
	m_filterNodes(csb->csb_pool),
	m_sortNode(NULL)
{
	m_impure = csb->allocImpure<Impure>();
	m_cardinality = csb->csb_rpt[stream].csb_cardinality;
}

void ForeignTableScan::internalOpen(thread_db* tdbb) const
{
	Database* const dbb = tdbb->getDatabase();
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	record_param* const rpb = &request->req_rpb[m_stream];
	rpb->getWindow(tdbb).win_flags = 0;

	// Append (using AND) priorly approved booleans into the WHERE part of the query.
	//
	// Convert FieldNode's with stream == m_stream into field names,
	// preserve expressions containing those fields "as is"
	// (transforming them into another syntax, if possible),
	// other nodes are evaluated and converted into literals.
	//
	// Examples (with m_stream == 0):
	//
	// ComparativeBoolNode (blr_geq)
	//   FieldNode(stream == 0)
	//   LiteralNode (1)
	//
	// The original condition looks like T1.ID >= 1
	// and it should be converted into:
	//   WHERE ... AND ID >= 1

	if (!impure->statement)
	{
		string filterSql;
		for (const auto boolean : m_filterNodes)
		{
			appendFilterValue(" and ", filterSql);
			decomposeBoolean(tdbb, filterSql, boolean);
		}

		string orderSql;
		if (m_sortNode)
		{
			orderSql.append(" order by ");

			const USHORT sortCnt = m_sortNode->expressions.getCount();
			for (USHORT i = 0; i < sortCnt; i++)
			{
				const ValueExprNode* node = m_sortNode->expressions[i];
				processArgument(tdbb, orderSql, node, nodeIs<NotBoolNode>(node));
				const SortDirection direction = m_sortNode->direction[i];
				const NullsPlacement nulls = m_sortNode->nullOrder[i];

				if (direction == ORDER_ASC)
					orderSql.append(" asc ");
				else if (direction == ORDER_DESC)
					orderSql.append(" desc ");

				if (nulls == NULLS_FIRST)
					orderSql.append(" nulls first ");
				else if (nulls == NULLS_LAST)
					orderSql.append(" nulls last ");

				orderSql.append(", ");
			}

			orderSql.rtrim(", ");
		}

		impure->statement = m_relation->rel_foreign_adapter->createStatement(tdbb, NULL, NULL, filterSql, orderSql);
	}

	m_relation->rel_foreign_adapter->execute(tdbb, impure->statement);

	VIO_record(tdbb, rpb, MET_current(tdbb, m_relation), request->req_pool);

	rpb->rpb_number.setValue(BOF_NUMBER);
}

void ForeignTableScan::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;
		if (impure->statement)
		{
			impure->statement->close(tdbb);
			impure->statement = nullptr;
		}
	}
}

bool ForeignTableScan::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	rpb->rpb_runtime_flags &= ~RPB_CLEAR_FLAGS;

	Record* const record = rpb->rpb_record;

	if (m_relation->rel_foreign_adapter->fetch(tdbb, impure->statement, record))
	{
		rpb->rpb_number.increment();
		rpb->rpb_number.setValid(true);

		return true;
	}

	impure->statement->close(tdbb);
	impure->statement = nullptr;

	rpb->rpb_number.setValid(false);
	return false;
}

bool ForeignTableScan::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

WriteLockResult ForeignTableScan::lockRecord(thread_db* tdbb) const
{
	SET_TDBB(tdbb);

	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void ForeignTableScan::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (!level)
		plan += "(";

	plan += printName(tdbb, m_alias) + " NATURAL";

	if (!level)
		plan += ")";
}

void ForeignTableScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "ForeignTableScan";

	planEntry.lines.add().text = "Table " + printName(tdbb, m_relation->rel_name.toQuotedString(), m_alias) + " Full Scan";
	printOptInfo(planEntry.lines);

	planEntry.objectType = m_relation->getObjectType();
	planEntry.objectName = m_relation->rel_name;

	if (m_alias.hasData() && m_relation->rel_name.toQuotedString() != m_alias)
		planEntry.alias = m_alias;
}

bool ForeignTableScan::applyBoolean(thread_db* tdbb, const BoolExprNode* boolean)
{
	// Check whether the boolean can be decoded into a condition
	// which is computable on the foreign server.
	//
	// If so, save it inside the object and return true.
	//
	// Maybe also do:
	//
	// m_cardinality *= Optimizer::getSelectivity(boolean);
	//
	// Beware:	boolean could also be BinaryBoolNode(blr_or) with either value expressions
	// 			or BinaryBoolNode(blr_or)'s or BinaryBoolNode(blr_and)'s buried inside.
	//
	// If the decision whether this boolean can be applied is deferred till the execution time,
	// return false so that the optimizer would re-check this condition after retrieval.

	if (checkBoolean(tdbb, boolean))
	{
		m_cardinality *= Optimizer::getSelectivity(boolean);
		m_filterNodes.add(boolean);

		return true;
	}

	return false;
}

bool ForeignTableScan::checkBoolean(thread_db* tdbb, const BoolExprNode* boolean)
{
	if (const auto notNode = nodeAs<NotBoolNode>(boolean))
	{
		if (checkBoolean(tdbb, notNode->arg))
			return true;
	}
	else if (const auto binaryNode = nodeAs<BinaryBoolNode>(boolean))
	{
		if (checkOperator(tdbb, binaryNode->blrOp) &&
			checkBoolean(tdbb, binaryNode->arg1) &&
			checkBoolean(tdbb, binaryNode->arg2))
		{
			return true;
		}
	}
	else if (const auto comparativeNode = nodeAs<ComparativeBoolNode>(boolean))
	{
		if (checkOperator(tdbb, comparativeNode->blrOp) &&
			checkArgument(tdbb, comparativeNode->arg1) &&
			checkArgument(tdbb, comparativeNode->arg2))
		{
			if (comparativeNode->arg3)
				return checkArgument(tdbb, comparativeNode->arg3);
			return true;
		}
	}
	else if (const auto missingBoolNode = nodeAs<MissingBoolNode>(boolean))
	{
		if (checkArgument(tdbb, missingBoolNode->arg))
			return true;
	}

	return false;
}

bool ForeignTableScan::checkArgument(thread_db* tdbb, const ValueExprNode* node)
{
	if (const auto arithmeticNone = nodeAs<ArithmeticNode>(node))
	{
		if (checkOperator(tdbb, arithmeticNone->blrOp) &&
			checkArgument(tdbb, arithmeticNone->arg1) &&
			checkArgument(tdbb, arithmeticNone->arg2))
		{
			return true;
		}
	}
	else if (const auto concatenateNode = nodeAs<ConcatenateNode>(node))
	{
		if (checkOperator(tdbb, blr_concatenate) &&
			checkArgument(tdbb, concatenateNode->arg1) &&
			checkArgument(tdbb, concatenateNode->arg2))
		{
			return true;
		}
	}
	else if (const auto negateNode = nodeAs<NegateNode>(node))
	{
		if (checkOperator(tdbb, blr_negate) && checkArgument(tdbb, negateNode->arg))
			return true;
	}
	else if (const auto strCaseNode = nodeAs<StrCaseNode>(node))
	{
		if (checkOperator(tdbb, strCaseNode->blrOp) && checkArgument(tdbb, strCaseNode->arg))
			return true;
	}
	else if (nodeIs<FieldNode>(node) || nodeIs<LiteralNode>(node) || nodeAs<NullNode>(node) ||
		nodeAs<ParameterNode> (node))
	{
		if (const auto fieldNode = nodeAs<FieldNode>(node))
		{
			// If node is a blob field, exclude it
			const dsc& desc = fieldNode->format->fmt_desc[fieldNode->fieldId];
			if (desc.isBlob())
				return false;
		}
		return true;
	}

	return false;
}

bool ForeignTableScan::checkOperator(thread_db* tdbb, const UCHAR op)
{
	switch (op)
	{
		case blr_add:
		case blr_subtract:
		case blr_multiply:
		case blr_divide:
		case blr_negate:
		case blr_eql:
		case blr_neq:
		case blr_gtr:
		case blr_geq:
		case blr_lss:
		case blr_leq:
		case blr_or:
		case blr_and:
		case blr_lowcase:
		case blr_upcase:
		case blr_between:
		case blr_like:
			return true;

		case blr_concatenate:
			return externalSqlFeatureSupport(tdbb, fb_feature_sql_op_concatenate);

		case blr_equiv:
			return externalSqlFeatureSupport(tdbb, fb_feature_sql_op_equiv);

		default:
			return false;
	}
}

void ForeignTableScan::decomposeBoolean(thread_db* tdbb, string& conjunctSql, const BoolExprNode* boolean,
	const bool isNotBoolNode) const
{
	if (const auto notNode = nodeAs<NotBoolNode>(boolean))
	{
		decomposeBoolean(tdbb, conjunctSql, notNode->arg, true);
	}
	else if (const auto binaryNode = nodeAs<BinaryBoolNode>(boolean))
	{
		decomposeBoolean(tdbb, conjunctSql, binaryNode->arg1, isNotBoolNode);
		processOperation(tdbb, conjunctSql, binaryNode->blrOp, isNotBoolNode);
		decomposeBoolean(tdbb, conjunctSql, binaryNode->arg2, isNotBoolNode);
	}
	else if (const auto comparativeNode = nodeAs<ComparativeBoolNode>(boolean))
	{
		processArgument(tdbb, conjunctSql, comparativeNode->arg1, isNotBoolNode);
		processOperation(tdbb, conjunctSql, comparativeNode->blrOp, isNotBoolNode);
		processArgument(tdbb, conjunctSql, comparativeNode->arg2, isNotBoolNode);
		if (comparativeNode->arg3)
			processOptionalArgument(tdbb, conjunctSql, comparativeNode->arg3, comparativeNode->blrOp, isNotBoolNode);
	}
	else if (const auto missingBoolNode = nodeAs<MissingBoolNode>(boolean))
	{
		processArgument(tdbb, conjunctSql, missingBoolNode->arg, isNotBoolNode);
		appendFilterValue(" is ", conjunctSql);

		if (isNotBoolNode)
			appendFilterValue(" not ", conjunctSql);

		// The specification does not make a distinction between the NULL value of BOOLEAN data type,
		// and the truth value UNKNOWN that is the result of an SQL predicate, search condition,
		// or Boolean value expression: they may be used interchangeably to mean exactly the same thing.
		appendFilterValue(" null ", conjunctSql);
	}
}

void ForeignTableScan::processArgument(thread_db* tdbb, string& conjunctSql, const ValueExprNode* node,
	const bool isNotBoolNode) const
{
	if (const auto arithmeticNone = nodeAs<ArithmeticNode>(node))
	{
		processArgument(tdbb, conjunctSql, arithmeticNone->arg1, isNotBoolNode);
		processOperation(tdbb, conjunctSql, arithmeticNone->blrOp, isNotBoolNode);
		processArgument(tdbb, conjunctSql, arithmeticNone->arg2, isNotBoolNode);
	}
	else if (const auto concatenateNode = nodeAs<ConcatenateNode>(node))
	{
		processArgument(tdbb, conjunctSql, concatenateNode->arg1, isNotBoolNode);
		appendFilterValue(" || ", conjunctSql);
		processArgument(tdbb, conjunctSql, concatenateNode->arg2, isNotBoolNode);
	}
	else if (const auto negateNode = nodeAs<NegateNode>(node))
	{
		processOperation(tdbb, conjunctSql, blr_negate, isNotBoolNode);
		processArgument(tdbb, conjunctSql, negateNode->arg, isNotBoolNode);
	}
	else if (const auto strCaseNode = nodeAs<StrCaseNode>(node))
	{
		string strCaseOpSql;
		string strCaseArgSql;

		processOperation(tdbb, strCaseOpSql, strCaseNode->blrOp, isNotBoolNode);
		processArgument(tdbb, strCaseArgSql, strCaseNode->arg, isNotBoolNode);
		appendFilterValue(strCaseOpSql, conjunctSql);
		appendFilterValue("(", conjunctSql);
		appendFilterValue(strCaseArgSql, conjunctSql);
		appendFilterValue(")", conjunctSql);
	}
	else if (const auto fieldNode = nodeAs<FieldNode>(node))
	{
		const jrd_fld* field = MET_get_field(m_relation, fieldNode->fieldId);
		string value = m_relation->rel_foreign_adapter->getOriginalTableName();
		value += ".";
		value += m_relation->rel_foreign_adapter->getOriginalFieldName(field->fld_name.c_str());
		appendFilterValue(value, conjunctSql);
	}
	else if (const auto literalNode = nodeAs<LiteralNode>(node))
	{
		const dsc* desc = &literalNode->litDesc;
		string value;

		getDescString(tdbb, desc, value, getServerCharset(tdbb));
		appendFilterValue("'", conjunctSql);
		appendFilterValue(value, conjunctSql);
		appendFilterValue("'", conjunctSql);
	}
	else if (const auto parameterNode = nodeAs<ParameterNode>(node))
	{
		Request* request = tdbb->getRequest();
		auto message = parameterNode->message;
		auto argNumber = parameterNode->argNumber;
		auto paramRequest = parameterNode->getParamRequest(request);
		dsc desc = message->getFormat(paramRequest)->fmt_desc[argNumber];

		string value;

		desc.dsc_address = message->getBuffer(paramRequest) + (IPTR) desc.dsc_address;

		getDescString(tdbb, &desc, value, getServerCharset(tdbb));
		appendFilterValue(value, conjunctSql);
	}
	else if (nodeIs<NullNode>(node))
		appendFilterValue("null", conjunctSql);
}

void ForeignTableScan::processOptionalArgument(thread_db* tdbb, Firebird::string& conjunctSql,
	const ValueExprNode* node, const UCHAR op, bool isNotBoolNode) const
{
	conjunctSql.append(" ");

	switch (op)
	{
		case blr_between:
			conjunctSql.append(" and ");
			break;
		case blr_like:
			conjunctSql.append(" escape ");
			break;
		default:
			fb_assert(false);
	}

	processArgument(tdbb, conjunctSql, node, isNotBoolNode);
	conjunctSql.append(" ");
}

void ForeignTableScan::processOperation(thread_db* /*tdbb*/, string& conjunctSql, const UCHAR op,
	const bool isNotBoolNode) const
{
	conjunctSql.append(" ");

	if (isNotBoolNode && op != blr_equiv)
		conjunctSql.append(" is not ");

	switch (op)
	{
		case blr_add:
			conjunctSql.append("+");
			break;
		case blr_multiply:
			conjunctSql.append("*");
			break;
		case blr_divide:
			conjunctSql.append("/");
			break;
		case blr_subtract:
		case blr_negate:
			conjunctSql.append("-");
			break;
		case blr_eql:
			conjunctSql.append("=");
			break;
		case blr_neq:
			conjunctSql.append("!=");
			break;
		case blr_gtr:
			conjunctSql.append(">");
			break;
		case blr_geq:
			conjunctSql.append(">=");
			break;
		case blr_lss:
			conjunctSql.append("<");
			break;
		case blr_leq:
			conjunctSql.append("<=");
			break;
		case blr_or:
			conjunctSql.append("or");
			break;
		case blr_and:
			conjunctSql.append("and");
			break;
		case blr_lowcase:
			conjunctSql.append("lower");
			break;
		case blr_upcase:
			conjunctSql.append("upper");
			break;
		case blr_equiv:
			if (isNotBoolNode)
				conjunctSql.append("is distinct from");
			else
				conjunctSql.append("is not distinct from");
			break;
		case blr_between:
			conjunctSql.append("between");
			break;
		case blr_like:
			conjunctSql.append("like");
			break;
		default:
			fb_assert(false);
	}

	conjunctSql.append(" ");
}

ULONG ForeignTableScan::getDescString(thread_db* tdbb, const dsc* desc, Firebird::string& outString,
	CHARSET_ID toCharset) const
{
	UCHAR* ptr = NULL;
	MoveBuffer temp;
	ULONG length;
	if (desc->isText() && desc->getCharSet() == toCharset)
		length = MOV_get_string(tdbb, desc, &ptr, NULL, 0);
	else
		length = MOV_make_string2(tdbb, desc, toCharset, &ptr, temp);

	const string value(ptr, length);
	appendFilterValue(value, outString);

	return length;
}

SSHORT ForeignTableScan::getServerCharset(thread_db* tdbb) const
{
	string serverCharset;
	const ForeignServer* server = m_relation->rel_foreign_adapter->getServer();
	ForeignOption option(*getDefaultMemoryPool());
	if (server->getOptions().get(MetaName(FOREIGN_SERVER_CHARSET), option))
		serverCharset = option.getActualValue();

	USHORT charsetId = CS_UTF8;
	if (serverCharset.hasData())
	{
		if (!MET_get_char_coll_subtype(tdbb, &charsetId, QualifiedName(serverCharset)))
		{
			// specified character set not found
			(Arg::Gds(isc_charset_not_found) << Arg::Str(serverCharset)).raise();
		}
	}

	return charsetId;
}

void ForeignTableScan::appendFilterValue(const string& value, string& conjunctSql) const
{
	conjunctSql.append(value);
}

void ForeignTableScan::trimLiteralSingleQuotes(Firebird::string& value) const
{
	value = value.substr(1, value.size() - 2);
}

bool ForeignTableScan::applySort(thread_db* tdbb, const SortNode* sort)
{
	const bool extNullOrdering = externalSqlFeatureSupport(tdbb, fb_feature_sql_op_null_ordering);
	const USHORT sortCnt = sort->expressions.getCount();

	fb_assert(!m_sortNode);

	for (USHORT i = 0; i < sortCnt; i++)
	{
		const NullsPlacement nulls = sort->nullOrder[i];
		if (!extNullOrdering && nulls != NULLS_DEFAULT)
			return false;

		const ValueExprNode* node = sort->expressions[i];
		if (!checkArgument(tdbb, node))
			return false;
	}

	m_sortNode = sort;

	return true;
}

const bool ForeignTableScan::externalSqlFeatureSupport(thread_db* tdbb, info_sql_features feature) const
{
	return m_relation->rel_foreign_adapter->testSqlFeature(tdbb, feature);
}
