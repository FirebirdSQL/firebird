/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonPath.h
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

#ifndef JSON_PATH_H
#define JSON_PATH_H

#include "../classes/JsonTypes.h"

#include "../common/classes/init.h"

namespace FBJSON {

inline constexpr PathArrayIndex JPATH_ARRAY_LAST_INDEX = -1;


struct PathNode;
class JsonExprNode;
class JsonPath;
struct PathVariable;
struct PathInjection;


class JsonPath : Firebird::PermanentStorage
{
	enum Flags : UCHAR
	{
		FLAG_STRICT = 1, // lax by default
		FLAG_HAS_INDEX = 2,
		FLAG_HAS_FIELD = 4,
		FLAG_COMPLEX_RANGE = 8,
	};

public:
	struct Range
	{
	public:
		PathArrayIndex down = 0;
		PathArrayIndex up = 0;

		constexpr void init(const PathArrayIndex singleIndex = 0) noexcept
		{
			down = singleIndex;
			up = singleIndex;
		}

		constexpr void init(const PathArrayIndex down, const PathArrayIndex up) noexcept
		{
			this->down = down;
			this->up = up;
		}

		// Support negative indexes
		// Return signed because the index can be negative even after array size adding
		constexpr JsonArrayIndex getPreparedUp(const ArraySize size) const noexcept
		{
			return up >= 0 ? up : PathArrayIndex(size) + up;
		}

		// Return signed because the index can be negative even after array size adding
		constexpr JsonArrayIndex getPreparedDown(const ArraySize size) const noexcept
		{
			return down >= 0 ? down : down + static_cast<PathArrayIndex>(size);
		}

		// Simple range
		constexpr bool enters(const PathArrayIndex index) const
		{
			fb_assert(!isComplex());
			fb_assert(down >= 0);
			fb_assert(up >= JPATH_ARRAY_LAST_INDEX);
			return index >= down && (up == JPATH_ARRAY_LAST_INDEX || index <= up);
		}

		// When a range can get first element, it is possible a wrapper
		// Example: the JSON '{"data":42}' with 'lex $[0].a' is equals to  [{"data":42}]
		constexpr bool canWrap() const noexcept
		{
			// Only -1 is a valid negative value so array size does not matter here
			return down == 0 || up == JPATH_ARRAY_LAST_INDEX || up == 0;
		}

		inline bool canGetFirst(const ArraySize arraySize) const noexcept
		{
			return down == 0 || getPreparedUp(arraySize) == 0;
		}

		constexpr bool isComplex() const noexcept
		{
			// Allow wildcard (*), simple ranges (1 to 2, 0 to last) and non negative indexes (2, 39, 0)
			// `last` token and all the negative values are consider as complex
			if (isWildcard())
				return false;

			return down < 0 || up < 0;
		}

		constexpr bool isWildcard() const noexcept
		{
			return down == 0 && up == JPATH_ARRAY_LAST_INDEX;
		}
	};

	JsonPath(MemoryPool& pool, const bool createPath = true);

	JsonPath(JsonPath&&) noexcept = delete;
	JsonPath(const JsonPath&) = delete;

	JsonPath& operator=(JsonPath&&) noexcept = delete;
	JsonPath& operator=(const JsonPath&) = delete;

	~JsonPath();

	void unwrap();

	PathNode* resetRootNode();
	PathInjection& addInjection();

	const PathNode* getRootNode() const noexcept
	{
		return m_pathRootNode.get();
	}

	PathNode* getRootNode() noexcept
	{
		return m_pathRootNode.get();
	}

public:
	inline bool hasFields() const noexcept
	{
		return flags & FLAG_HAS_FIELD;
	}

	inline bool hasIndexes() const noexcept
	{
		return flags & FLAG_HAS_INDEX;
	}

	inline bool hasComplexRange() const noexcept
	{
		return flags & FLAG_COMPLEX_RANGE;
	}

	inline bool isLax() const noexcept
	{
		return (flags & FLAG_STRICT) == 0;
	}

	inline bool isStrict() const noexcept
	{
		return flags & FLAG_STRICT;
	}

	bool isZeroPath() const;
	bool isWrappingZeroPath() const;
	bool isEmpty(const UCHAR flagsToCheck) const;

	inline void setIndexesFlag()
	{
		flags |= FLAG_HAS_INDEX;
	}

	inline void setComplexRangeFlag()
	{
		flags |= FLAG_COMPLEX_RANGE;
	}

	inline void setFieldsFlag()
	{
		flags |= FLAG_HAS_FIELD;
	}

	inline void setStrictFlag()
	{
		flags |= FLAG_STRICT;
	}

public:
	static bool isAggregateMethod(const PathMethod method);
	static bool isUnwrapMethod(const PathMethod method);

protected:
	UCHAR flags = 0;

private:
	Firebird::AutoPtr<PathNode> m_pathRootNode;

	// Instead of a field or an index, an expression can be used.
	// But in 99% of cases, this is not the case. So use simple scalars in the PathNode's and
	// inject calculated value if needed before the processing
	Firebird::ObjectsArray<PathInjection> m_injections;
};


// Store parsed JSON Path in a 3 components
// <primary path> <tail> <arithmetic>
// <primary path> is the main root path.
// <tail> are the nodes used to modify output value of its path
// <arithmetic> are the nodes used to apply arithmetical expressions to the node
// $.value.keyvalue().value + $.mod + 4
// <-----> <--------------> <--------->
// primary       tail        arithmetic

// In case of sequence, arithmetic applies to each sequence element

class JsonPathExpr : public Firebird::PermanentStorage
{
public:
	JsonPathExpr(MemoryPool& pool);

	JsonPathExpr(JsonPathExpr&&) noexcept = delete;
	JsonPathExpr(const JsonPathExpr&) = delete;

	JsonPathExpr& operator=(JsonPathExpr&&) noexcept = delete;
	JsonPathExpr& operator=(const JsonPathExpr&) = delete;

	~JsonPathExpr();

	const JsonExprNode* getMath() const;
	const JsonExprNode* getTail() const;
	inline const JsonPath* getJsonPath() const noexcept
	{
		return m_jpath.get();
	}

	inline JsonPath* getJsonPath() noexcept
	{
		return m_jpath.get();
	}

	// arithmetic(<jpath>.<tail>)
	void resetExpr(JsonExprNode* tail, JsonExprNode* arithmetic);
	inline bool hasMath() const
	{
		return getMath() != nullptr;
	}

private:
	Firebird::AutoPtr<JsonPath> m_jpath;
	Firebird::AutoPtr<JsonExprNode> m_tail; // Accepts to each element
	Firebird::AutoPtr<JsonExprNode> m_math; // Accepts to output expression
};

using RangesArray = Firebird::HalfStaticArray<JsonPath::Range, 4>;


enum class PathNodeState : UCHAR
{
	NORMAL = 0,
	TEMPORARY,
	DISABLED
};

// Each part of a json path will be converted into a PathNode.
// We could use just an array but for lax mode it may be necessary to insert a node
// For example:
// ($.price.value) => node<root> -> node('price') -> node('value');
// ($[0].value) => node<root>(ranges:[0]) -> node('value');
// When price is an object the path wont change but for an array we will insert an extra node:
// node('price')->node('value')   ==>   node('price') -> node([*]) -> node('value')
struct PathNode : Firebird::PermanentStorage
{
public:
	PathNode(MemoryPool& pool, PathNode* prev = nullptr);

	PathNode(PathNode&&) noexcept = delete;
	PathNode(const PathNode&) = delete;

	PathNode& operator=(PathNode&&) = delete;
	PathNode& operator=(const PathNode&) = delete;
	~PathNode();

public: // Node modification methods
	inline void enable() noexcept
	{
		if (state != PathNodeState::DISABLED)
			return;

		state = PathNodeState::NORMAL;
		if (next == nullptr)
			return;

		PathNode* it = next;
		it->prev = this;
		while (it)
		{
			++it->depth;
			it = it->next;
		}
	}

	inline void disable() noexcept
	{
		if (isDisabled())
			return;

		state = PathNodeState::DISABLED;

		if (next == nullptr)
			return;

		PathNode* it = next;
		next->prev = prev;

		while (it)
		{
			--it->depth;
			it = it->next;
		}
	}

	inline void selectAll()
	{
		type = ItemType::ARRAY_ELEMENT;
		ranges.add().init(0, JPATH_ARRAY_LAST_INDEX);
	}

	PathNode* update();

	inline void updateLaxPatterns()
	{
		for (FB_SIZE_T i = 0; i < ranges.getCount(); i++)
		{
			if (ranges[i].canWrap())
			{
				flags |= PathNode::FLAG_WRAP;
				break;
			}
		}
	}

	inline PathNode* add()
	{
		fb_assert(next == nullptr);
		return next = FB_NEW_POOL(getPool()) PathNode(getPool(), this);
	}

	PathNode* insertUnwrapNode();

	inline PathNode* insert()
	{
		if (next == nullptr)
			return add();
		else
		{
			PathNode* oldNext = next;
			// this<-->oldNext
			next = nullptr;
			PathNode* newNext = FB_NEW_POOL(getPool()) PathNode(getPool(), this);
			newNext->next = oldNext;
			oldNext->prev = newNext;
			// this<-->newNext<-->oldNext

			while (oldNext)
			{
				++oldNext->depth;
				oldNext = oldNext->next;
			}

			next = newNext;
			return newNext;
		}
	}

	void removeThis() noexcept;

	inline PathNode* moveTo(const SSHORT newDepth) noexcept
	{
		if (depth == newDepth)
		{
			if (isDisabled())
			{
				return prev;
			}
			else
				return this;
		}
		else if (depth > newDepth)
		{
			// Move Down

			PathNode* temp = this;
			while (temp && temp->depth > newDepth)
			{
				temp->matched = false;
				temp = temp->prev;
			}

			if (temp && temp->state != PathNodeState::TEMPORARY)
				temp->matched = false;

			return temp;
		}
		else
		{
			// Move Up

			// Don't check for update because the state adds in a previous level to the next one
			PathNode* temp = this;
			while (temp && temp->depth < newDepth)
			{
				temp = temp->next;
			}

			while (temp && temp->isDisabled())
			{
				temp = temp->next;
			}

			return temp;
		}
	}

public: // Checkers
	// Check the range to match the array size
	// Throw an error on fail
	void validateRange(const ArraySize arraySize) const
	{
		for (USHORT i = 0; i < ranges.getCount(); ++i)
		{
			// Current jsonLevel is an element level. Get size from parent (array) level
			const PathArrayIndex rangeA = ranges[i].getPreparedDown(arraySize);
			const PathArrayIndex rangeB = ranges[i].getPreparedUp(arraySize);

			// [4,2] for example
			if (rangeA > rangeB)
			{
				json_strict_exception::raise(JsonStatusMsg(isc_jstrict_common) <<
					JsonStatusMsg(isc_jstrict_invalid_range) << SINT64(rangeA) << SINT64(rangeB));
			}

			// [-20,-10] for example
			if (rangeA < 0 || rangeA >= PathArrayIndex(arraySize) || rangeB >= PathArrayIndex(arraySize))
			{
				const SINT64 printSize = arraySize > 0 ? (arraySize - 1) : 0;

				json_strict_exception::raise(JsonStatusMsg(isc_jstrict_common) <<
					JsonStatusMsg(isc_jstrict_out_of_range) <<
					SINT64(rangeA) <<  SINT64(rangeB) <<JsonStatusMsgIntArg(printSize));
			}
		}
	}

	inline bool getRangeIndex(const ArraySize arraySize, const ArraySize indexInArray, JsonArrayIndex& indexInRange) const
	{
		for (JsonArrayIndex i = indexInRange; i < ranges.getCount(); ++i)
		{
			if (ranges[i].enters(indexInArray))
			{
				indexInRange = i;
				return true;
			}
		}

		return false;
	}

	inline bool matchFieldName(const SmallString& name) const
	{
		if (name.isEmpty())
			return false;

		if (this->field == Tokens::ANY_FIELD)
			return true;

		if (this->field == name)
			return true;

		return false;
	}

	inline bool isInSimpleRange(const JsonArrayIndex index) const
	{
		if (ranges.isEmpty())
			return false;

		for (ULONG i = 0; i < ranges.getCount(); i++)
		{
			if (ranges[i].enters(index))
			{
				return true;
			}
		}

		return false;
	}

	inline bool isNextEmpty() const noexcept
	{
		// Two diabled in a row is impossible but check it anyway
		return next == nullptr || (next->isDisabled() && next->isNextEmpty());
	}

	// No complex ranges should be in the node
	// Complex ranges should be checked via JSON BINARY to avoid extra parsing to calculate array size
	bool equalsNoComplex(const JsonLevelNode* const jsonLevel);

	bool equalsZeroDepth(const JsonLevelNode* jsonLevel) const noexcept
	{
		return depth == 0 && jsonLevel->depth == 0;
	}

	bool canWrap(const JsonLevelNode* jsonLevel) const noexcept
	{
		// We do not add (wrap) json level, instead modify the path.
		// Example: for '{"data":{"a":1}}' we can convert '$.data[0].a to $.data.a
		if (isNextEmpty())
			return false;

		return matched && next->matchWrapPattern() && jsonLevel->depth == depth;
	}

	inline bool canWrapEnd(const JsonLevelNode* jsonLevel) const
	{
		// We do not add (wrap) json level, instead modify the path.
		// Example: for '{"data":1}' we can convert '$.data[0].a to $.data
		return canWrap(jsonLevel) && next->isNextEmpty();
	}

	inline bool canUnwrapEnd() noexcept
	{
		return matched && next == nullptr && matchUnwrapPattern();
	}

	inline bool canUnwrapMiddle(const JsonLevelNode* jsonLevel) const noexcept
	{
		// Can insert [*] to path
		return matched && next && jsonLevel->isArray() && matchUnwrapPattern();
	}

	inline bool isTemporary() const noexcept
	{
		return state == PathNodeState::TEMPORARY;
	}

	inline bool isDisabled() const noexcept
	{
		return state == PathNodeState::DISABLED;
	}

	inline bool isEmpty(const USHORT flagsToCheck) const
	{
		return next == nullptr && ((flags & flagsToCheck) == 0) && !filterNode.hasData();
	}

	inline bool matchWrapPattern() const noexcept
	{
		return flags & FLAG_WRAP;
	}

	inline bool matchUnwrapPattern() const noexcept
	{
		return flags & FLAG_UNWRAP;
	}

public: // Getters
	inline PathNode* getEnabledPrev() const
	{
		return (prev && prev->isDisabled() ? prev->getEnabledPrev() : prev);
	}

private:
	friend class JsonPath;

public:
	enum LexPattern
	{
		FLAG_NONE = 0,
		FLAG_WRAP = 1,
		FLAG_UNWRAP = 2,
		FLAG_PROCESSED = 4,
		FLAG_ALL_LEX = FLAG_WRAP | FLAG_UNWRAP
	};

	// Common data
	ItemType type = ItemType::FIELD;
	SSHORT depth = 0;
	UCHAR flags = 0;
	Firebird::AutoPtr<JsonExprNode> filterNode;

	// Filed
	SmallString field;

	// Range
	RangesArray ranges;

	// Common
	PathNode* next = nullptr;
	PathNode* prev = nullptr;

	// Mutable fields
	PathNodeState state = PathNodeState::NORMAL;
	bool matched = false;
};


struct PathVariable : private Firebird::PermanentStorage
{
	enum class Type
	{
		JSON = 0, // The source json
		ITEM, // Root in a filter
		PASSING,
		ROOT, // Calculated primary root variable to pass to the path tail expr
		HEAD
	};

	SmallString name;
	Firebird::AutoPtr<JsonPath> path;
	Type type = Type::ITEM;

	PathVariable(MemoryPool& pool, const Type type = Type::ITEM)
		: Firebird::PermanentStorage(pool),
		name(pool),
		type(type)
	{ }

	PathVariable(PathVariable&&) noexcept = delete;
	PathVariable(const PathVariable&) = delete;

	PathVariable& operator=(PathVariable&&) noexcept = delete;
	PathVariable& operator=(const PathVariable&) = delete;

	~PathVariable() = default;

	inline bool isItem() const noexcept
	{
		return type == Type::ITEM;
	}

	inline bool isPassing() const noexcept
	{
		return type == Type::PASSING;
	}

	bool hasPath(const UCHAR flagsMask) const;
};


// Use it to calculate and inject result into JSON Path
struct PathInjection : private Firebird::PermanentStorage
{
	enum class Position
	{
		FIELD = 0,
		RANGE_UP,
		RANGE_DOWN,
		RANGE_INDEX
	};

	// Where to inject
	Position position = Position::FIELD;
	SSHORT rangeIndex = -1;
	SSHORT depth = -1;

	// What to inject
	Firebird::AutoPtr<JsonExprNode> expr;

	PathInjection(MemoryPool& pool);
	PathInjection(PathInjection&&) noexcept = delete;
	PathInjection(const PathInjection&) = delete;

	PathInjection& operator=(PathInjection&&) noexcept = delete;
	PathInjection& operator=(const PathInjection&) = delete;

	~PathInjection() = default;
};

}

#endif // JSON_PATH_H
