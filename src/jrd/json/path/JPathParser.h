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
 *  The Original Code was created by Artyom Abakumov
 *  <artyom.abakumov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2023 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "firebird.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/PodOptional.h"

#include "../jrd/json/JsonRuntime.h"
#include "../jrd/json/path/JsonPath.h"

namespace FBJSON {

// Define before jparse.h
struct JsonPathContext
{
	FBJSON::JsonPath* main = nullptr;
	PathNode* current = nullptr;

	PathVariable* variable = nullptr;
	bool dropMain = false;

	JsonPathContext()
	{ }

	JsonPathContext(JsonPathContext&&) noexcept = delete;
	JsonPathContext(const JsonPathContext&) = delete;

	JsonPathContext& operator=(JsonPathContext&&) noexcept = delete;
	JsonPathContext& operator=(const JsonPathContext&) = delete;

	~JsonPathContext()
	{
		if (dropMain)
			delete main;
	}

	inline void addRange(const RangeSize up)
	{
		current->ranges.add().init(up);
	}

	inline void addRange(const RangeSize up, const RangeSize bot)
	{
		current->ranges.add().init(up, bot);
	}

	void clearRange()
	{
		current->ranges.clear();
	}

	inline JsonPath* extract() noexcept
	{
		dropMain = false;
		return main;
	}

	inline  bool isEmpty() const
	{
		return main->isZeroPath() && !current->filterNode.hasData();
	}
};

} // namespace

#include "../gen/jparse.h"

using PassingKeys = Firebird::LeftPooledMap<FBJSON::SmallString, bool>;

namespace FBJSON {


class PathParser : public Firebird::PermanentStorage
{
public:
	PathParser(MemoryPool& pool, const std::string_view jsonPath);

	PathParser(PathParser&&) noexcept = delete;
	PathParser(const PathParser&) = delete;

	PathParser& operator=(PathParser&&) noexcept = delete;
	PathParser& operator=(const PathParser&) = delete;

	~PathParser();

	JsonPathExpr* parse(const PassingKeys* keys = nullptr);

	// Allow only one root expression for functions like JSON_EXISTS
	inline void setMultipleRoots(const bool allow) noexcept
	{
		m_allowPathExtensions = allow;
	}

private:
	// User-defined text position type.
	struct Position
	{
		ULONG firstLine;
		ULONG firstColumn;
		ULONG lastLine;
		ULONG lastColumn;
		const char* firstPos;
		const char* lastPos;
		const char* leadingFirstPos;
		const char* trailingLastPos;
	};

	typedef Position YYPOSN;
	typedef int Yshort;

	struct yyparsestate
	{
	  yyparsestate* save;		// Previously saved parser state
	  int state;
	  int errflag;
	  Yshort* ssp;				// state stack pointer
	  YYSTYPE* vsp;				// value stack pointer
	  YYPOSN* psp;				// position stack pointer
	  YYSTYPE val;				// value as returned by actions
	  YYPOSN pos;				// position as returned by universal action
	  Yshort* ss;				// state stack base
	  YYSTYPE* vs;				// values stack base
	  YYPOSN* ps;				// position stack base
	  int lexeme;				// index of the conflict lexeme in the lexical queue
	  unsigned int stacksize;	// current maximum stack size
	  Yshort ctry;				// index in yyctable[] for this conflict
	};

	struct LexerState
	{
		// This is, in fact, parser state. Not used in lexer itself
		int dsql_debug;

		// Actual lexer state begins from here

		const TEXT* leadingPtr;
		const TEXT* ptr;
		const TEXT* end;
		const TEXT* last_token;
		const TEXT* start;
		const TEXT* line_start;
		const TEXT* last_token_bk;
		const TEXT* line_start_bk;
		int prev_keyword;
	};

	[[maybe_unused]]
	int yydebug = 0;

	JsonPathContext* createContext()
	{
		JsonPathContext* context = FB_NEW_POOL(*m_tempPool) JsonPathContext();
		context->main = FB_NEW_POOL(getPool()) JsonPath(getPool());
		context->dropMain = true;
		context->current = context->main->getRootNode();

		return context;
	}

	JsonPathContext* createMainContext()
	{
		if (m_mainRootCreated)
		{
			if (!m_parseInFilter && !m_allowPathExtensions)
			{
				json_syntax_exception::raise(
					JsonStatusMsg(isc_jpath_common) <<
					JsonStatusMsg(isc_jpath_multiple_roots));
			}

			return createVariableContext(PathVariable::Type::JSON);
		}
		else
		{
			m_mainRootCreated = true;

			// Main path
			JsonPathContext* context = FB_NEW_POOL(*m_tempPool) JsonPathContext();
			context->dropMain = false;
			context->main = m_output->getJsonPath();
			context->current = context->main->getRootNode();
			context->variable = FB_NEW_POOL(getPool()) PathVariable(getPool(), PathVariable::Type::ROOT);

			return context;
		}
	}

	JsonPathContext* createVariableContext(const PathVariable::Type type)
	{
		if (!m_parseInFilter)
		{
			if (type == PathVariable::Type::ITEM)
			{
				json_syntax_exception::raise(
					JsonStatusMsg(isc_jpath_common) <<
					JsonStatusMsg(isc_jpath_illegal_item_variable));
			}

			if (type == PathVariable::Type::PASSING)
			{
				m_mainRootCreated = true; // Allow passing instead of root path expression ($.<something>) 
			}
		}

		PathVariable* var = FB_NEW_POOL(getPool()) PathVariable(getPool(), type);
		return createVariableContext(*var);
	}

	JsonPathContext* createVariableContext(PathVariable& var)
	{
		JsonPathContext* context = FB_NEW_POOL(*m_tempPool) JsonPathContext();
		var.path.reset(FB_NEW_POOL(getPool()) JsonPath(getPool()));
		if (!m_isLaxMode)
			var.path->setStrictFlag();

		context->dropMain = false;
		context->main = var.path.get();
		context->current = var.path->getRootNode();
		context->variable = &var;

		return context;
	}

	JsonPathContext* createPassingContext(const SmallString& name)
	{
		checkPassing(name);
		JsonPathContext* context = createVariableContext(PathVariable::Type::PASSING);
		context->variable->name = name;
		return context;
	}

	PathInjection& addPathInjection(JsonPathContext* main, const SmallString& name)
	{
		checkPassing(name);
		PathVariable* passing = FB_NEW_POOL(getPool()) PathVariable(getPool(), PathVariable::Type::PASSING);
		passing->name = name;

		return addPathInjection(main, FB_NEW_POOL(getPool()) JsonExprNode(getPool(), passing));
	}

	PathInjection& addPathInjection(JsonPathContext* context, JsonExprNode* node)
	{
		auto& injection = context->main->addInjection();
		injection.expr = node;
		injection.depth = context->current->depth + 1;

		return injection;
	}

	void unwrapPrimary(JsonExprNode* nodeToUnwrap)
	{
		if (!nodeToUnwrap->isVariable())
			return;

		if (nodeToUnwrap->getVariable()->type == PathVariable::Type::ROOT)
		{
			m_output->getJsonPath()->unwrap();
		}
		else
			nodeToUnwrap->unwrap();
	}

private:
	Firebird::string* mkTempString(const Firebird::string& s)
	{
		return FB_NEW_POOL(*m_tempPool) Firebird::string(*m_tempPool, s);
	}

	Firebird::string* mkTempString(const char* s)
	{
		return FB_NEW_POOL(*m_tempPool) Firebird::string(*m_tempPool, s);
	}

	template<JsonExprNode::Type T>
	JsonExprNode* mkNode()
	{
		return JsonExprNode::make<T>(getPool());
	}

	JsonExprNode* mkVariableNode(PathVariable* var)
	{
		return FB_NEW_POOL(getPool()) JsonExprNode(getPool(), var);
	}

	JsonExprNode* mkMethodNode(const PathMethod method, JsonExprNode* arg = nullptr)
	{
		auto* node = FB_NEW_POOL(getPool()) JsonExprNode(getPool(), method);

		if (arg)
			node->addChild(arg);

		return node;
	}

	JsonExprNode* addOptTailNode(JsonExprNode* head, JsonExprNode* newTail)
	{
		if (newTail == nullptr)
			return head;

		bool needUnwrap = m_isLaxMode && JsonPath::isUnwrapMethod(newTail->getPathMethod());

		// Inject head into expression
		if (!head->canHasTail())
		{
			if (needUnwrap)
				unwrapPrimary(head);

			newTail->setHead(head);
			return newTail;
		}

		// Add new tail
		if (head->hasTail())
		{
			JsonExprNode* tailEnd = head->getTailNode();
			addOptTailNode(tailEnd, newTail); // tail after tail
			return head;
		}

		// Always inject root into expression
		const bool isRootVariable = head->isVariable() && head->getVariable()->type == PathVariable::Type::ROOT;
		if (isRootVariable)
		{
			if (needUnwrap)
				unwrapPrimary(head);

			newTail->setHead(head);
			return newTail;
		}

		if (needUnwrap && head->testType(JsonExprNode::VARIABLE_NODE))
		{
			head->unwrap(); // Unwrap variable
			needUnwrap = false;
		}

		if (!newTail->testType(JsonExprNode::FILTER_NODE))
			newTail->makeHeadNode(needUnwrap);

		// Add the next method as the second "tail" path
		head->addChild(newTail);

		return head;
	}

	void setPrimary(JsonExprNode* arithmetic)
	{
		if (!m_mainRootCreated)
		{
			json_syntax_exception::raise(
				JsonStatusMsg(isc_jpath_common) <<
				JsonStatusMsg(isc_jpath_missing_root));
		}

		fb_assert(m_output->getTail() == nullptr);
		fb_assert(m_output->getMath() == nullptr);
		m_output->resetExpr(m_pathRootTail.release(), arithmetic);
	}

	JsonExprNode* completePath(JsonExprNode* node)
	{
		JsonExprNode* rnode = node;
		while (!rnode->isVariable()) // While method or a filter - get the source argument
		{
			if (rnode->getChildrenCount() == 0)
				return node;

			rnode = rnode->getFirstChild();
		}

		if (!m_pathRootTail.hasData() && rnode->getVariable()->type == PathVariable::Type::ROOT)
		{
			fb_assert(!node->testType(JsonExprNode::CALCULATION_NODE));
			m_pathRootTail.reset(node);

			// Return a dummy header node
			return mkVariableNode(FB_NEW_POOL(getPool()) PathVariable(getPool(), PathVariable::Type::ROOT));
		}
		else
		{
			return node;
		}
	}

	void yyReducePosn(YYPOSN& ret, YYPOSN* termPosns, YYSTYPE* termVals,
		int termNo, int stkPos, int yychar, YYPOSN& yyposn, void*);

	int yylex();
	bool yylexSkipSpaces();
	bool yylexSkipEol();	// returns true if EOL is detected and skipped
	int yylexAux();

	void yyerror_detailed(const TEXT* error_string, int yychar, YYSTYPE&, YYPOSN&);

private:
	void checkPassing(const SmallString& pass) const;
	void raiseFullErrorVector(const JsonStatusMsg& error, const YYPOSN& posn);


// start - defined in btyacc_json.ske
private:
	static void yySCopy(YYSTYPE* to, YYSTYPE* from, int size);
	static void yyPCopy(YYPOSN* to, YYPOSN* from, int size);
	static void yyFreeState(yyparsestate* p);

	void yyMoreStack(yyparsestate* yyps);
	yyparsestate* yyNewState(int size);


private:
	int parseAux();
	int yylex1();
	int yyexpand();
// end - defined in btyacc_json.ske

private:
	USHORT parser_version;

	Firebird::string transformedString;

	// These value/posn are taken from the lexer
	YYSTYPE yylval;
	YYPOSN yyposn;

	// These value/posn of the root non-terminal are returned to the caller
	YYSTYPE yyretlval;
	Position yyretposn;

	int yynerrs;
	int yym;	// ASF: moved from local variable of Parser::parseAux()

	// Current parser state
	yyparsestate* yyps;
	// yypath!=NULL: do the full parse, starting at *yypath parser state.
	yyparsestate* yypath;
	// Base of the lexical value queue
	YYSTYPE* yylvals;
	// Current posistion at lexical value queue
	YYSTYPE* yylvp;
	// End position of lexical value queue
	YYSTYPE* yylve;
	// The last allocated position at the lexical value queue
	YYSTYPE* yylvlim;
	// Base of the lexical position queue
	Position* yylpsns;
	// Current posistion at lexical position queue
	Position* yylpp;
	// End position of lexical position queue
	Position* yylpe;
	// The last allocated position at the lexical position queue
	Position* yylplim;
	// Current position at lexical token queue
	Yshort* yylexp;
	Yshort* yylexemes;

	LexerState lex;

private:
	Firebird::AutoMemoryPool m_tempPool;
	Firebird::AutoPtr<JsonPathExpr> m_output;
	Firebird::AutoPtr<JsonExprNode> m_pathRootTail;

	const PassingKeys* m_passingKeys = nullptr;

	SLONG m_filterBracketCounter = 0;
	bool m_isLaxMode = true; // Global state
	bool m_lexInFilter = false; // For error printing
	bool m_parseInFilter = false; // For context check
	bool m_mainRootCreated = false;

	bool m_allowPathExtensions = true;
};

}

#endif	// DSQL_PARSER_H
