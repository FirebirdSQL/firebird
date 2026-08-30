/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonRuntime.cpp
 *	DESCRIPTION:	Json Calculation logic.
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
 *  The Original Code was created by Artyom Abakumov
 *  <artyom.abakumov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2022 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */
 
#include "JsonRuntime.h"

#include "JsonConsts.h"

#include "path/JsonPath.h"
#include "classes/JsonScalar.h"
#include "classes/JsonTypes.h"

#include <utility>


using namespace FBJSON;

// ****
// Counting sort helpers

constexpr int getOperationIndex(const JsonExprOperation op)
{
	return static_cast<int>(op) - JsonExprNode::OFFSET_START;
}

constexpr UCHAR moveOffsetByOperation(JsonExprNode::OffsetCounter& m_offsets, const JsonExprOperation op)
{
	return m_offsets[getOperationIndex(op)]++;
}
// ****

constexpr int isUnaryOperation(const JsonExprOperation op)
{
	return JsonExprOperation::REGEX_FLAGS < op && op < JsonExprOperation::MULTIPLICATION;
}


JsonExprNode::JsonExprNode(MemoryPool& pool, const Type m_type, const UCHAR flag)
	: Firebird::PermanentStorage(pool),
	m_data(),
	m_type(m_type),
	m_flags(flag)
{
	switch (m_type)
	{
		case SCALAR_NODE:
			m_data.scalarBlock = FB_NEW_POOL(pool) ScalarBlock(pool);
			break;
		case VARIABLE_NODE:
			m_data.varBlock = FB_NEW_POOL(pool) VariableBlock(pool);
			break;
		case CALCULATION_NODE:
			m_data.calculationBlock = FB_NEW_POOL(pool) CalculationBlock(pool);
			break;
		case METHOD_NODE:
			m_data.methodBlock = FB_NEW_POOL(pool) MethodBlock(pool);
			break;
		case FILTER_NODE:
			m_data.filterBlock = FB_NEW_POOL(pool) FilterBlock(pool);
			break;
		case COMPOUND_NODE:
			m_data.childrenBlock = FB_NEW_POOL(pool) ChildrenNodes(pool);
			break;
		default:
			fb_assert(false);
	}
}

// Variable node
JsonExprNode::JsonExprNode(MemoryPool& pool, PathVariable* var) :
	JsonExprNode(pool, JsonExprNode::VARIABLE_NODE)
{
	fb_assert(var);
	m_data.varBlock = FB_NEW_POOL(pool) VariableBlock(pool);
	m_data.varBlock->variable.reset(var);
}

// Method node
JsonExprNode::JsonExprNode(MemoryPool& pool, const PathMethod method):
	JsonExprNode(pool, JsonExprNode::METHOD_NODE)
{
	m_data.methodBlock = FB_NEW_POOL(pool) MethodBlock(pool);

	m_data.methodBlock->method = method;
	m_data.methodBlock->children.add(nullptr);
}


JsonExprNode::~JsonExprNode()
{
	if (canAccessChildren())
	{
		auto& block = *m_data.childrenBlock;
		for (FB_SIZE_T i = 0; i < block.children.getCount(); i++)
			delete block.children[i];

		block.children.clear();
	}

	switch (m_type)
	{
		case SCALAR_NODE:
			delete m_data.scalarBlock;
			break;
		case VARIABLE_NODE:
			delete m_data.varBlock;
			break;
		case CALCULATION_NODE:
			delete m_data.calculationBlock;
			break;
		case METHOD_NODE:
			delete m_data.methodBlock;
			break;
		case FILTER_NODE:
			delete m_data.filterBlock;
			break;
		case COMPOUND_NODE:
			delete m_data.filterBlock;
			break;
		default:
			fb_assert(false);
	}

	m_data = {};
}

JsonExprNode* JsonExprNode::addChild(const Type childType, const Flags flags)
{
	fb_assert(childType != CALCULATION_NODE || (flags & FLAG_SOLID) != 0); // It is useless to add CALCULATION_NODE with no SOLID flag
	JsonExprNode* currentLevel = FB_NEW_POOL(getPool()) JsonExprNode(getPool(), childType, flags);
	return addChild(currentLevel) ? currentLevel : nullptr;
}

bool JsonExprNode::addChild(JsonExprNode* node)
{
	fb_assert(node);

	fb_assert((m_type == CALCULATION_NODE || m_type == COMPOUND_NODE) // Any count of the m_children)
		|| (m_type == METHOD_NODE && m_data.childrenBlock->children.getCount() < 3) // 3 is max(primary, argument, afterpath)
		|| (m_type == FILTER_NODE && m_data.childrenBlock->children.getCount() < 3) // 3 is max
		|| (m_type == VARIABLE_NODE && m_data.childrenBlock->children.getCount() < 1)); // 1 is max

	// A statement is stored in a binary tree
	// For example:
	// '1 + 2 * 3' transforms to 'calculation_node(value_node, calculation_node(value_node, value_node))'
	// It To avoid extra memory usage and simplify the structure we keep only CALCULATION_NODE with brackets
	if (m_type == CALCULATION_NODE && node->m_type == CALCULATION_NODE && (node->m_flags & FLAG_SOLID) == 0)
	{
		for (FB_SIZE_T i = 0; i < node->getChildrenCount(); ++i)
		{
			auto* child = node->m_data.calculationBlock->children[i];
			m_data.childrenBlock->children.add(child);
		}

		auto& rhsOperations = node->m_data.calculationBlock->operations;
		for (FB_SIZE_T i = 0; i < rhsOperations.getCount(); ++i)
			addOperation(rhsOperations[i]);

		// Explicit clear to avoid calling `delete`
		node->m_data.calculationBlock->children.clear();
		delete node;
		return false;
	}
	else
	{
		m_data.childrenBlock->children.add(node);

		return true;
	}
}

void JsonExprNode::addOperation(const JsonExprOperation operation)
{
	fb_assert(m_type == CALCULATION_NODE);

	m_data.calculationBlock->operations.add(operation);

	if (m_data.calculationBlock->operations.getCount() > OPERATIONS_LIMIT)
	{
		json_syntax_exception::raise(JsonStatusMsg(isc_jpath_operations_limit)<<
			JsonStatusMsgIntArg(OPERATIONS_LIMIT + 1) << JsonStatusMsgIntArg(OPERATIONS_LIMIT));
	}

	moveOffsetByOperation(m_data.calculationBlock->offsets, operation);
}

void JsonExprNode::makeHeadNode(const bool unwrap)
{
	fb_assert(canAccessChildren());

	// Variable
	PathVariable* var = FB_NEW_POOL(getPool()) PathVariable(getPool());
	var->type = PathVariable::Type::HEAD;
	var->path = FB_NEW_POOL(getPool()) JsonPath(getPool());

	// Node
	JsonExprNode* headNode = FB_NEW_POOL(getPool()) JsonExprNode(getPool(), var);

	// Unwrap
	if (unwrap)
	{
		// Only for argument
		headNode->unwrap();
	}

	// Add head
	auto& children = m_data.childrenBlock->children;
	if (children.getCount() > 0 && children[0] == nullptr)
	{
		fb_assert(testType(METHOD_NODE));
		setHead(headNode);
	}
	else
		addChild(headNode);
}


JsonExprNode* JsonExprNode::finish()
{
	fb_assert(m_type == CALCULATION_NODE);
	if (canBeOmitted()) // Possible useless
	{
		auto& children = m_data.calculationBlock->children;
		auto* node = children.front();
		children.clear();
		delete this;

		return node;
	}

	// Counting sort
	auto& offsets = m_data.calculationBlock->offsets;
	int lastValue = offsets[0];
	for (USHORT i = 1; i < offsets.size(); ++i)
	{
		const UCHAR temp = offsets[i];
		offsets[i] += offsets[i - 1];
		offsets[i - 1] -= lastValue;
		lastValue = temp;
	}
	fb_assert(offsets.back() == m_data.calculationBlock->operations.getCount());
	offsets.back() -= lastValue;

	return this;
}

const PathVariable* JsonExprNode::getVariable() const
{
	if (m_type == VARIABLE_NODE)
		return  m_data.varBlock->variable.get();
	else
		return nullptr;
}

void JsonExprNode::unwrap()
{
	if (hasTail())
	{
		// Recursive call!
		m_data.calculationBlock->children.back()->unwrap();
		return;
	}

	// To unwrap a path, we need a variable node with data
	if (!isVariable())
		return;

	auto* variable = m_data.varBlock->variable.get();
	if (variable == nullptr)
		return;

	if (variable->path == nullptr)
		variable->path.reset(FB_NEW_POOL(getPool()) JsonPath(getPool()));

	variable->path->unwrap();
}
