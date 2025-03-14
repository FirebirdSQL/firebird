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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_PARSER_H
#define DSQL_PARSER_H

#include <optional>
#include "../dsql/dsql.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/AggNodes.h"
#include "../dsql/WinNodes.h"
#include "../dsql/PackageNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/RecordSourceNodes.h"
#include "../common/classes/TriState.h"
#include "../common/classes/stack.h"

#include "gen/parse.h"

namespace Firebird {
class CharSet;

namespace Arg {
	class StatusVector;
} // namespace
} // namespace

namespace Jrd {

class Parser : public Firebird::PermanentStorage
{
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
		SSHORT charSetId;
		SLONG lines, lines_bk;
		int prev_keyword;
		USHORT param_number;
	};

	struct StrMark
	{
		StrMark()
			: introduced(false),
			  pos(0),
			  length(0),
			  str(NULL)
		{
		}

		bool operator >(const StrMark& o) const
		{
			return pos > o.pos;
		}

		bool introduced;
		unsigned pos;
		unsigned length;
		IntlString* str;
	};

public:
	static const int MAX_TOKEN_LEN = 256;

public:
	Parser(thread_db* tdbb, MemoryPool& pool, MemoryPool* aStatementPool, DsqlCompilerScratch* aScratch,
		USHORT aClientDialect, USHORT aDbDialect, bool aRequireSemicolon,
		const TEXT* string, size_t length, SSHORT charSetId);
	~Parser();

public:
	DsqlStatement* parse();

	const Firebird::string& getTransformedString() const
	{
		return transformedString;
	}

	bool isStmtAmbiguous() const
	{
		return stmt_ambiguous;
	}

	Firebird::string* newString(const Firebird::string& s)
	{
		return FB_NEW_POOL(getPool()) Firebird::string(getPool(), s);
	}

	Lim64String* newLim64String(const Firebird::string& s, int scale)
	{
		return FB_NEW_POOL(getPool()) Lim64String(getPool(), s, scale);
	}

	IntlString* newIntlString(const Firebird::string& s, const char* charSet = NULL)
	{
		return FB_NEW_POOL(getPool()) IntlString(getPool(), s, charSet);
	}

	template <typename T, typename... Args>
	T* newNode(Args&&... args)
	{
		return setupNode<T>(FB_NEW_POOL(getPool()) T(getPool(), std::forward<Args>(args)...));
	}

private:
	template <typename T> T* setupNode(Node* node)
	{
		setNodeLineColumn(node);
		return static_cast<T*>(node);
	}

	// Overload to not make LineColumnNode data wrong after its construction.
	template <typename T> T* setupNode(LineColumnNode* node)
	{
		return node;
	}

	// Overload for non-Node classes constructed with newNode.
	template <typename T> T* setupNode(void* node)
	{
		return static_cast<T*>(node);
	}

	BoolExprNode* valueToBool(ValueExprNode* value)
	{
		BoolAsValueNode* node = nodeAs<BoolAsValueNode>(value);
		if (node)
			return node->boolean;

		ComparativeBoolNode* cmpNode = newNode<ComparativeBoolNode>(
			blr_eql, value, MAKE_constant("1", CONSTANT_BOOLEAN));
		cmpNode->dsqlCheckBoolean = true;

		return cmpNode;
	}

	void yyReducePosn(YYPOSN& ret, YYPOSN* termPosns, YYSTYPE* termVals,
		int termNo, int stkPos, int yychar, YYPOSN& yyposn, void*);

	int yylex();
	bool yylexSkipSpaces();
	bool yylexSkipEol();	// returns true if EOL is detected and skipped
	int yylexAux();

	void yyerror(const TEXT* error_string);
	void yyerror_detailed(const TEXT* error_string, int yychar, YYSTYPE&, YYPOSN&);
	void yyerrorIncompleteCmd(const YYPOSN& pos);

	void check_bound(const char* const to, const char* const string);
	void check_copy_incr(char*& to, const char ch, const char* const string);

	void yyabandon(const Position& position, SLONG, ISC_STATUS);
	void yyabandon(const Position& position, SLONG, const Firebird::Arg::StatusVector& status);

	MetaName optName(MetaName* name)
	{
		return (name ? *name : MetaName());
	}

	QualifiedName optName(QualifiedName* name)
	{
		return (name ? *name : QualifiedName());
	}

	void transformString(const char* start, unsigned length, Firebird::string& dest);
	Firebird::string makeParseStr(const Position& p1, const Position& p2);
	ParameterNode* make_parameter();

	// Set the value of a clause, checking if it was already specified.

	template <typename T>
	void setClause(T& clause, const char* duplicateMsg, const T& value)
	{
		checkDuplicateClause(clause, duplicateMsg);
		clause = value;
	}

	template <typename T, template <typename C> class Delete>
	void setClause(Firebird::AutoPtr<T, Delete>& clause, const char* duplicateMsg, T* value)
	{
		checkDuplicateClause(clause, duplicateMsg);
		clause = value;
	}

	template <typename T>
	void setClause(std::optional<T>& clause, const char* duplicateMsg, const T& value)
	{
		checkDuplicateClause(clause, duplicateMsg);
		clause = value;
	}

	template <typename T>
	void setClause(std::optional<T>& clause, const char* duplicateMsg, const std::optional<T>& value)
	{
		if (value.has_value())
		{
			checkDuplicateClause(clause, duplicateMsg);
			clause = value.value();
		}
	}

	void setClause(Firebird::TriState& clause, const char* duplicateMsg, bool value)
	{
		checkDuplicateClause(clause, duplicateMsg);
		clause = value;
	}

	void setClause(Firebird::TriState& clause, const char* duplicateMsg, const Firebird::TriState& value)
	{
		if (value.isAssigned())
		{
			checkDuplicateClause(clause, duplicateMsg);
			clause = value;
		}
	}

	template <typename T1, typename T2>
	void setClause(NestConst<T1>& clause, const char* duplicateMsg, const T2& value)
	{
		setClause(*clause.getAddress(), duplicateMsg, value);
	}

	void setClause(bool& clause, const char* duplicateMsg)
	{
		setClause(clause, duplicateMsg, true);
	}

	void setClauseFlag(unsigned& clause, const unsigned flag, const char* duplicateMsg)
	{
		using namespace Firebird;
		if (clause & flag)
		{
			status_exception::raise(
				Arg::Gds(isc_sqlerr) << Arg::Num(-637) <<
				Arg::Gds(isc_dsql_duplicate_spec) << duplicateMsg);
		}
		clause |= flag;
	}

	template <typename T>
	void checkDuplicateClause(const T& clause, const char* duplicateMsg)
	{
		using namespace Firebird;
		if (isDuplicateClause(clause))
		{
			status_exception::raise(
				Arg::Gds(isc_sqlerr) << Arg::Num(-637) <<
				Arg::Gds(isc_dsql_duplicate_spec) << duplicateMsg);
		}
	}

	template <typename T>
	bool isDuplicateClause(const T& clause)
	{
		return clause != 0;
	}

	bool isDuplicateClause(const MetaName& clause)
	{
		return clause.hasData();
	}

	bool isDuplicateClause(const QualifiedName& clause)
	{
		return clause.object.hasData();
	}

	bool isDuplicateClause(const Firebird::TriState& clause)
	{
		return clause.isAssigned();
	}

	template <typename T>
	bool isDuplicateClause(const std::optional<T>& clause)
	{
		return clause.has_value();
	}

	template <typename T>
	bool isDuplicateClause(const Firebird::Array<T>& clause)
	{
		return clause.hasData();
	}

	void setCollate(TypeClause* fld, QualifiedName* name)
	{
		if (name)
			setClause(fld->collate, "COLLATE", *name);
	}
	void checkTimeDialect();

// start - defined in btyacc_fb.ske
private:
	static void yySCopy(YYSTYPE* to, YYSTYPE* from, int size);
	static void yyPCopy(YYPOSN* to, YYPOSN* from, int size);
	static void yyFreeState(yyparsestate* p);

	void yyMoreStack(yyparsestate* yyps);
	yyparsestate* yyNewState(int size);

	void setNodeLineColumn(Node* node);

private:
	int parseAux();
	int yylex1();
	int yyexpand();
// end - defined in btyacc_fb.ske

private:
	MemoryPool* statementPool;
	DsqlCompilerScratch* scratch;
	USHORT client_dialect;
	USHORT db_dialect;
	const bool requireSemicolon;
	USHORT parser_version;
	Firebird::CharSet* charSet;

	Firebird::CharSet* metadataCharSet;
	Firebird::string transformedString;
	Firebird::GenericMap<Firebird::NonPooled<IntlString*, StrMark> > strMarks;
	bool stmt_ambiguous;
	DsqlStatement* parsedStatement;

	// Parser feedback for lexer
	QualifiedName* introducerCharSetName = nullptr;

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

public:
	LexerState lex;
};

} // namespace

#endif	// DSQL_PARSER_H
