/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonRuntime.h
 *	DESCRIPTION:	JSON calculation logic.
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

#ifndef JSON_RUNTIME_H
#define JSON_RUNTIME_H

#include "firebird.h"
#include "JsonConsts.h"
#include "classes/JsonScalar.h"
#include "classes/JsonTypes.h"
#include "path/JsonPath.h"

namespace FBJSON {

struct ContextVariables;

// operations order by its priority
enum class JsonExprOperation : UCHAR
{
	// VALUE,
	NONE = 0,


	// Functions
	STARTS_WITH,
	LIKE_REGEX,
	REGEX_FLAGS,

	// Unary operations
	IS_UNKNOWN,
	EXISTS,

	// Unary
	UNARY_PLUS,
	UNARY_MINUS,

	// Logical negation
	NOT,

	// Arithmetic operations
	MULTIPLICATION,
	DIVISION,
	MODULO,

	ADDITION,
	SUBTRACTION,

	// Logical operations
	MORE,
	MORE_OE,
	LESS,
	LESS_OE,
	EQUALS,
	NOT_EQUALS,

	// Boolean operations
	AND,
	OR,

	// Keep it last
	LAST
};

class PassingList;


// * * * *
// A main class to represent and execute complex expressions with arithmetics, comparators and methods
// All logic is set within a single class. It is not worth of splitting it into several classes
// because it only leads to overcomplicated a pretty simple and limited functional

// Some node has a special storage strategy for children nodes: <head> <args> <tail>
// <head> is a primary value (usually scalar or a variable)
// <args> are used for the current node
// <tail> is a next execution node

// * * * *
class JsonExprNode : public Firebird::PermanentStorage
{
public:
	static constexpr USHORT OFFSET_START = static_cast<USHORT>(JsonExprOperation::NONE) + 1;
	static constexpr USHORT OFFSET_SIZE  = static_cast<USHORT>(JsonExprOperation::LAST) - OFFSET_START;
	static constexpr USHORT OPERATIONS_LIMIT = 128;

	using OffsetCounter = std::array<UCHAR, JsonExprNode::OFFSET_SIZE>;

	enum Type : UCHAR
	{
		SCALAR_NODE, // Just a value
		VARIABLE_NODE, // $. @. $passingVariable
		CALCULATION_NODE, // An expression
		METHOD_NODE, // Method with args
		FILTER_NODE, // Filter: $ ? (@ > 2)
		COMPOUND_NODE // A set of 2 or more nodes
	};

	enum Flags : UCHAR
	{
		FLAG_NONE = 0,
		FLAG_METHOD_ARGUMENT = 1,
		FLAG_SOLID = 2, // in brackets
	};

	template<Type T>
	requires(T != JsonExprNode::Type::METHOD_NODE && T != JsonExprNode::Type::VARIABLE_NODE)
	static JsonExprNode* make(MemoryPool& pool, const UCHAR flag = FLAG_NONE)
	{
		return FB_NEW_POOL(pool) JsonExprNode(pool, T);
	}

	// It is better to use `make` function to create a new node
	explicit JsonExprNode(MemoryPool& pool, const Type type = CALCULATION_NODE, const UCHAR flag = FLAG_NONE);

	// Variable node
	JsonExprNode(MemoryPool& pool, PathVariable* var);

	// Method node
	JsonExprNode(MemoryPool& pool, const PathMethod method);

	JsonExprNode(JsonExprNode&&) noexcept = delete;
	JsonExprNode(const JsonExprNode&) = delete;

	JsonExprNode& operator=(JsonExprNode&&) noexcept = delete;
	JsonExprNode& operator=(const JsonExprNode&) = delete;

	~JsonExprNode();

public:
	void passPassing(const PassingList* passing);

public:// Children routines
	JsonExprNode* addChild(const Type childType, const Flags flags = FLAG_NONE);

	// The incoming node cloud be modifed and deleted (the function returns false in this case)
	bool addChild(JsonExprNode* node);

	// Only for CALCULATION_NODE
	void addOperation(const JsonExprOperation operation);

	// Value methods
	inline JsonExprNode* addVariable(PathVariable* main)
	{
		JsonExprNode* currentLevel = FB_NEW_POOL(getPool()) JsonExprNode(getPool(), main);
		return addChild(currentLevel) ? currentLevel : nullptr;
	}

	// Make a new node and insert it as first element
	void makeHeadNode(const bool unwrap);

	inline void setHead(JsonExprNode* base)
	{
		fb_assert(canAccessChildren());
		fb_assert(m_data.childrenBlock->children[0] == nullptr || m_data.childrenBlock->children[0]->m_type == VARIABLE_NODE);
		m_data.childrenBlock->children[0] = base;
	}

	// Call it after all addChild, when all operators and values are added
	JsonExprNode* finish();

public: // Set content methods
	inline JsonExprNode* set(const int value) noexcept
	{
		fb_assert(m_type == JsonExprNode::SCALAR_NODE);

		m_data.scalarBlock->value.set(value);
		m_type = JsonExprNode::SCALAR_NODE;
		return this;
	}

	inline JsonExprNode* set(const SINT64 value) noexcept
	{
		fb_assert(m_type == JsonExprNode::SCALAR_NODE);

		m_data.scalarBlock->value.set(value);
		m_type = JsonExprNode::SCALAR_NODE;
		return this;
	}

	inline JsonExprNode* set(const double value) noexcept
	{
		fb_assert(m_type == JsonExprNode::SCALAR_NODE);

		m_data.scalarBlock->value.set(value);
		m_type = JsonExprNode::SCALAR_NODE;
		return this;
	}

	inline JsonExprNode* set(const bool value) noexcept
	{
		fb_assert(m_type == JsonExprNode::SCALAR_NODE);

		m_data.scalarBlock->value.set(value);
		m_type = JsonExprNode::SCALAR_NODE;
		return this;
	}

	inline JsonExprNode* set(const char* value) noexcept
	{
		fb_assert(m_type == JsonExprNode::SCALAR_NODE);

		m_data.scalarBlock->value.set(value);
		m_type = JsonExprNode::SCALAR_NODE;
		return this;
	}

	inline JsonExprNode* setNull() noexcept
	{
		fb_assert(m_type == JsonExprNode::SCALAR_NODE);

		m_data.scalarBlock->value.setToNull();
		m_type = JsonExprNode::SCALAR_NODE;
		return this;
	}

	inline JsonExprNode* set(const SmallString& str)
	{
		fb_assert(m_type == JsonExprNode::SCALAR_NODE);

		m_data.scalarBlock->value.set(str);
		m_type = JsonExprNode::SCALAR_NODE;
		return this;
	}

public: // Getters
	inline JsonExprNode* getFirstChild() const
	{
		fb_assert(canAccessChildren());
		fb_assert(m_data.childrenBlock->children.getCount() > 0);
		return m_data.childrenBlock->children[0];
	}

	inline JsonExprNode* getSecondChild() const
	{
		fb_assert(canAccessChildren());
		fb_assert(m_data.childrenBlock->children.getCount() > 1);
		return m_data.childrenBlock->children[1];
	}

	inline JsonExprNode* getLastChild() const
	{
		fb_assert(canAccessChildren());
		fb_assert(m_data.childrenBlock->children.getCount() > 0);
		return m_data.childrenBlock->children[m_data.childrenBlock->children.getCount() - 1];
	}

	inline FB_SIZE_T getChildrenCount() const noexcept
	{
		if (!canAccessChildren())
			return 0;

		return m_data.childrenBlock->children.getCount();
	}

	inline void clearChildren() noexcept
	{
		fb_assert(canAccessChildren());
		m_data.childrenBlock->children.clear();
	}

public:// Checkers
	inline bool isRootEmpty() const
	{
		switch (m_type)
		{
			case JsonExprNode::CALCULATION_NODE:
			{
				auto& children = m_data.calculationBlock->children;
				auto& operations = m_data.calculationBlock->operations;
				return (children.isEmpty() || children.getCount() == 1 && children[0]->isRootEmpty()) && operations.isEmpty();
			}
			case JsonExprNode::COMPOUND_NODE:
			case JsonExprNode::FILTER_NODE:
				return m_data.childrenBlock->children.isEmpty();
			default:
				return false;
		}
	}

	inline bool isVariable() const
	{
		return m_type == JsonExprNode::VARIABLE_NODE;
	}

	inline bool testType(const JsonExprNode::Type type) const
	{
		return this->m_type == type;
	}

	inline bool canHasTail() const
	{
		switch (m_type)
		{
			case JsonExprNode::METHOD_NODE:
			case JsonExprNode::VARIABLE_NODE:
			case JsonExprNode::FILTER_NODE:
				return true;
			default:
				return false;
		}
	}

	inline bool hasTail() const
	{
		switch (m_type)
		{
			case METHOD_NODE:
				return m_data.childrenBlock->children.getCount() >= 2 && (m_data.childrenBlock->children.back()->m_flags & FLAG_METHOD_ARGUMENT) == 0;
			case VARIABLE_NODE:
				return m_data.childrenBlock->children.getCount() == 1;
			case FILTER_NODE:
				return m_data.childrenBlock->children.getCount() == 3;
			default:
				return false;
		}
	}

	inline JsonExprNode* getTailNode() const
	{
		if (hasTail())
			return m_data.childrenBlock->children.back(); // Always the last
		else
			return nullptr;
	}

	inline PathMethod getPathMethod() const noexcept
	{
		if (!testType(METHOD_NODE))
			return PathMethod::NONE;

		return m_data.methodBlock->method;
	}

	const PathVariable* getVariable() const;

	inline bool applyUnaryOp(const JsonExprOperation op) noexcept
	{
		fb_assert(op == JsonExprOperation::UNARY_MINUS || op == JsonExprOperation::UNARY_PLUS);

		if (!testType(SCALAR_NODE))
			return false;

		if (op == JsonExprOperation::UNARY_PLUS)
			return true;

		if (m_data.scalarBlock->value.getType() == ValueType::INT)
		{
			m_data.scalarBlock->value.getValue().integer = -m_data.scalarBlock->value.getValue().integer;
			return true;
		}
		else
		{
			m_data.scalarBlock->value.getValue().doubleValue = -m_data.scalarBlock->value.getValue().doubleValue;
			return true;
		}
	}

	std::optional<RangeSize> getRangeNumber() const
	{
		if (!testType(SCALAR_NODE))
			return std::nullopt;

		static constexpr RangeSize rangeMin = std::numeric_limits<RangeSize>::min();
		static constexpr RangeSize rangeMax = std::numeric_limits<RangeSize>::max();

		auto invalidRangeOrTypeError = []()
		{
			json_fatal_exception::raise(JsonStatusMsgWrapper(isc_jpath_common) <<
				JsonStatusMsgWrapper(isc_jpath_invalid_range) << rangeMin << rangeMax);
		};

		if (!m_data.scalarBlock->value.isNumber())
			invalidRangeOrTypeError();

		if (m_data.scalarBlock->value.getType() != ValueType::INT)
			invalidRangeOrTypeError();

		const auto value =  m_data.scalarBlock->value.getValue().integer;
		if (value < rangeMin || value > rangeMax)
			invalidRangeOrTypeError();


		return static_cast<RangeSize>(value);
	}

	// A calculation node with only one child is useless in term of this node
	// So it is safe to omit such node and keep only the child
	bool canBeOmitted() const
	{
		return m_type == CALCULATION_NODE && getChildrenCount() == 1 && !m_data.calculationBlock->operations.hasData();
	}

public: // Helpers
	// Unwrap path in a variable node
	void unwrap();

	constexpr void addFlag(const Flags flag)
	{
		m_flags |= UCHAR(flag);
	}

public: // Executions
	enum class FilterResult
	{
		// Add the 'R' prefix to avoid conflicts with macros
		RTRUE,
		RFALSE,
		RUNKNOWN
	};
	//static FilterResult passFilter(const ContextVariables& context, const JsonExprNode* filter);

	// Executers
	//JsonVariant execute(const ContextVariables& context) const;

private:
	inline bool canAccessChildren() const noexcept
	{
		// Basically all except SCALAR_NODE
		return m_type == CALCULATION_NODE || m_type == FILTER_NODE || m_type == METHOD_NODE ||
			m_type == VARIABLE_NODE || m_type == COMPOUND_NODE;
	}

	//JsonVariant executeVariableNode(const ContextVariables& context) const;
	//JsonVariant executeCalculationNode(const ContextVariables& context) const;
	//JsonVariant executeMethodNode(const ContextVariables& context) const;
	//JsonVariant executeFilterNode(const ContextVariables& context) const;

private:
	// Scalar node

	struct ScalarBlock
	{
		JsonScalar value;

		ScalarBlock(MemoryPool& pool) :
			value(pool)
		{ }
	};


	struct ChildrenNodes
	{
		Firebird::HalfStaticArray<JsonExprNode*, 4> children;

		ChildrenNodes(MemoryPool& pool) :
			children(pool)
		{ }
	};

	struct VariableBlock : public ChildrenNodes
	{
		VariableBlock(MemoryPool& pool) :
			ChildrenNodes(pool)
		{ }

		Firebird::AutoPtr<PathVariable> variable; //$. or @. or $pass
		//JsonVariant* cachedVariable = nullptr;
	};

	struct CalculationBlock : public ChildrenNodes
	{
		CalculationBlock(MemoryPool& pool) :
			ChildrenNodes(pool),
			operations(pool)
		{ }

		Firebird::HalfStaticArray<JsonExprOperation, 4> operations;
		// Counter of each operation for correct execution order
		OffsetCounter offsets = {};
	};

	// Variable node
	struct MethodBlock : public ChildrenNodes
	{
		// Method node
		PathMethod method = PathMethod::NONE;
		MethodBlock(MemoryPool& pool) :
			ChildrenNodes(pool)
		{ }
	};

	struct FilterBlock : public ChildrenNodes
	{
		FilterBlock(MemoryPool& pool) :
			ChildrenNodes(pool)
		{ }
	};

	union NodeData
	{
		ScalarBlock* scalarBlock;
		VariableBlock* varBlock;
		ChildrenNodes* childrenBlock;
		CalculationBlock* calculationBlock;
		MethodBlock* methodBlock;
		FilterBlock* filterBlock;
	} m_data{};

	// Common data
	Type m_type;
	UCHAR m_flags{};
};

}

#endif // JSON_RUNTIME_H
