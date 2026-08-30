/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonPath.cpp
 *	DESCRIPTION:	JSON Path parser.
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
 *  The Original Code was created by Denis Logashov
 *   <denis.logashov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2017 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "JsonPath.h"
#include "../JsonUtils.h"
#include "../JsonConsts.h"
#include "../JsonRuntime.h"

#include "JPathParser.h"

using namespace Firebird;
using namespace FBJSON;

JsonPath::JsonPath(MemoryPool& pool, const bool createPath)
	: Firebird::PermanentStorage(pool),
	m_injections(pool)
{
	if (createPath)
		resetRootNode();
}

JsonPath::~JsonPath()
{ }

PathNode* JsonPath::resetRootNode()
{
	m_pathRootNode.reset(FB_NEW_POOL(getPool()) PathNode(getPool()));
	return m_pathRootNode.get();
}

PathInjection& JsonPath::addInjection()
{
	return m_injections.add();
}

void JsonPath::unwrap()
{
	fb_assert(m_pathRootNode != nullptr);
	PathNode* current = m_pathRootNode;
	while (current->next)
	{
		current = current->next;
	}
	current->flags |= PathNode::FLAG_UNWRAP;
}

bool JsonPath::isZeroPath() const
{
	fb_assert(m_pathRootNode != nullptr);
	return m_pathRootNode->next == nullptr;
}

bool JsonPath::isWrappingZeroPath() const
{
	fb_assert(m_pathRootNode != nullptr);
	return m_pathRootNode->next != nullptr &&
		m_pathRootNode->next->next == nullptr &&
		m_pathRootNode->next->matchWrapPattern();
}

bool JsonPath::isEmpty(const UCHAR flagsToCheck) const
{
	return m_pathRootNode == nullptr || m_pathRootNode->isEmpty(flagsToCheck);
}

bool JsonPath::isAggregateMethod(const PathMethod method)
{
	switch (method)
	{
		case PathMethod::SIZE:
		case PathMethod::TYPE:
			return true;
		default:
			return false;
	}
}

bool JsonPath::isUnwrapMethod(const PathMethod method)
{
	switch (method)
	{
		case PathMethod::NONE:
		case PathMethod::SIZE:
		case PathMethod::TYPE:
			return false;
		default:
			return true;
	}
}


JsonPathExpr::JsonPathExpr(MemoryPool& pool) :
	Firebird::PermanentStorage(pool),
	m_jpath(FB_NEW_POOL(pool) JsonPath(pool))
{ }

JsonPathExpr::~JsonPathExpr()
{ }

void JsonPathExpr::resetExpr(JsonExprNode* tail, JsonExprNode* arithmetic)
{
	// Omit empty arithmetic path (it contains only a arithmetic variable)
	// tail is null when the input is a passing variable. Example: json_value('"test"', '$passed_timestamp')
	if (tail != nullptr && ((tail->isVariable() && !tail->hasTail()) || tail->isRootEmpty()))
	{
		delete tail;
		tail = nullptr;
	}
	m_tail = tail;

	fb_assert(arithmetic);
	if ((arithmetic->isVariable() && arithmetic->getVariable()->type == PathVariable::Type::ROOT) || arithmetic->isRootEmpty())
	{
		delete arithmetic;
		arithmetic = nullptr;
	}
	m_math = arithmetic;
}

const JsonExprNode* JsonPathExpr::getMath() const
{
	return m_math;
}

const JsonExprNode* JsonPathExpr::getTail() const
{
	return m_tail;
}

// PathNode

PathNode::PathNode(MemoryPool& pool, PathNode* prev)
	: Firebird::PermanentStorage(pool),
	field(pool), ranges(pool), prev(prev)
{
	if (prev)
	{
		fb_assert(prev->next == nullptr);

		prev->next = this;
		depth = prev->depth + 1;
	}
}

PathNode::~PathNode()
{
	PathNode* cur = next;
	PathNode* nnext;
	while (cur)
	{
		nnext = cur->next;
		cur->next = nullptr;
		delete cur;
		cur = nnext;
	}
}

PathNode* PathNode::update()
{
	if (isTemporary())
	{
		PathNode* newCurrent = next ? next : prev;

		fb_assert(prev);
		prev->filterNode.reset(filterNode.release());
		removeThis();
		return newCurrent;
	}
	else
		enable();

	return this;
}

PathNode* PathNode::insertUnwrapNode()
{
	insert();
	next->state = PathNodeState::TEMPORARY;
	next->matched = false;
	next->filterNode = std::move(filterNode);
	next->selectAll();

	return next;
}

void PathNode::removeThis() noexcept
{
	if (next)
	{
		next->prev = prev;
		PathNode* temp = next;
		while (temp)
		{
			--temp->depth;
			temp = temp->next;
		}
	}

	if (prev)
	{
		if (isTemporary())
			prev->filterNode = std::move(filterNode);

		prev->next = next;
	}

	next = nullptr;
	prev = nullptr;
	delete this;
}

bool PathNode::equalsNoComplex(const JsonLevelNode* const jsonLevel)
{
	// Previous level should be matched
	PathNode* enabledPrev = getEnabledPrev();
	if (enabledPrev == nullptr || !enabledPrev->matched || jsonLevel->depth > depth)
		return false;

	matched = false;
	switch (getSuperType<ItemType>(type, jsonLevel->itemType))
	{
		case getSuperType(ItemType::FIELD, ItemType::FIELD):
			if (!matchFieldName(jsonLevel->field))
				return false;

			// Check the previous state because the current one can be skipped (when an unwrapping node has been inserted)
			fb_assert(enabledPrev->filterNode == nullptr);
			matched = true;
			break;
		case getSuperType(ItemType::FIELD, ItemType::ARRAY_ELEMENT):
			if (enabledPrev->matchUnwrapPattern())
			{
				// $.field
				// [{"filed":42}]
				// Expected object but encoutred an array

				// Modify path
				// $.field   =>   $[*].field
				enabledPrev->insertUnwrapNode();
				enabledPrev->next->matched = true;
			}
			return false;
		case getSuperType(ItemType::ARRAY_ELEMENT, ItemType::ARRAY_ELEMENT):
			if (!isInSimpleRange(jsonLevel->indexInArray))
				return false;

			fb_assert(enabledPrev->filterNode == nullptr);
			matched = true;
			break;
		case getSuperType(ItemType::ARRAY_ELEMENT, ItemType::FIELD):
			if (next && matchWrapPattern())
			{
				fb_assert(!next->isDisabled());

				// $[0].a
				// {"a":42}
				matched = true;
				return next->equalsNoComplex(jsonLevel);
			}
			return false;

		default:
			fb_assert(false);
	}

	return isNextEmpty() && jsonLevel->depth == depth; // reached end
}


bool PathVariable::hasPath(const UCHAR flagsMask) const
{
	return path.hasData() && !path->isEmpty(flagsMask);
}


PathInjection::PathInjection(MemoryPool& pool)
	: Firebird::PermanentStorage(pool)
{ }
