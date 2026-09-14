%{
/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonPathParser.y
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
 *  The Original Code was created by Artyom Abakumov
 *  <artyom.abakumov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2023 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/json/JsonRuntime.h"
#include "../jrd/json/path/JsonPath.h"
#include "../jrd/json/JsonConsts.h"

#define YYREDUCEPOSNFUNC yyReducePosn
#define YYREDUCEPOSNFUNCARG NULL


// ASF: Inherited attributes (aka rule parameters) are executed even when in trial mode, but action
// rules ({}) are executed only when in full parse mode. NOTRIAL should be used to avoid segfaults
// due to accessing invalid pointers in parameters (not yet returned from action rules).
#define NOTRIAL(x) (yytrial ? NULL : (x))


#include "../dsql/chars.h"

using namespace Firebird;
using namespace FBJSON;

%}


// token declarations

%token <int16Val> INTEGER16

%token <int32Val> INTEGER32
%token <int64Val> INTEGER64
%token <doubleVal> DOUBLE

%token JSON_NULL
%token TRUE
%token FALSE

%token <stringPtr> STRING
%token <stringPtr> QUOTED_STRING
%token <stringPtr> PASSING_NAME
%token LAX
%token STRICT
%token LAST
%token TO
%token KEYVALUE
%token DATETIME
%token DECIMAL
%token TIME
%token TIME_TZ
%token TIMESTAMP
%token TIMESTAMP_TZ

%token METHOD_DOT // Special dot that checks bracket: '.' method '('

%token <methodType> METHOD


%token STARTS
%token WITH
%token LIKE_REGEX
%token FLAGS
%token IS
%token UNKNOWN
%token EXISTS

%token AND
%token OR
%token EQ
%token NEQ
%token LEQ
%token GEQ


// precedence declarations for expression evaluation

%left	OR
%left	AND
%left	'!'
%left	EQ NEQ '<' '>' LIKE_REGEX STARTS NEQ GEQ LEQ
%left	IS // IS UNKNOWN
%left	'+' '-'
%left	'*' '/' '%'
%left	UMINUS UPLUS

%union YYSTYPE
{
	YYSTYPE()
	{}

	bool boolVal;
	SLONG int32Val;
	SINT64 int64Val;
	double doubleVal;
	Firebird::string* stringPtr;
	struct DecimalPair
	{
		FBJSON::JsonExprNode* scale;
		FBJSON::JsonExprNode* precision;
	} pairVal;

	SSHORT int16Val;
	Firebird::PodOptional<SSHORT> int16ValOpt;
	FBJSON::PathMethod methodType;
	FBJSON::JsonExprNode* exprNode;
	FBJSON::PathVariable* variable;
	FBJSON::JsonExprOperation exprOperation;
	FBJSON::PathInjection::Position passingPos;
	FBJSON::JsonPathContext* jpathContext;
}

%include jtypes.y


%%


// One possible statement
top
	: json_path
	;

json_path
	: path_mode json_path_arithmetic_finished
		{ setPrimary($2); }
	;

path_mode
	: /* nothing */ { m_isLaxMode = true; }
	| LAX          { m_isLaxMode = true; }
	| STRICT
		{
			m_isLaxMode = false;
			m_output->getJsonPath()->setStrictFlag();
		}
	;

// **********************
// Primary Path rules
// **********************

// ($ | @ | $passing) [. <body> ]
%type <jpathContext> variable_accessor
variable_accessor
	: '$'
		{ $$ = createMainContext(); }
	| '@'
		{ $$ = createVariableContext(PathVariable::Type::ITEM); }
	| PASSING_NAME
		{ $$ = createPassingContext(*$1); }
	;

// @ | $passing
%type <variable> json_variable
json_variable
	: variable_accessor variable_body_opt($<jpathContext>1)
		{ $$ = $2; }
	;


// The primary path side, before the filter
// <$ with path> | $passing | <number scalar> | <strict scalar>
%type <exprNode> path_primary
path_primary
	: number_scalar // It is easier to place it here and check instead of adding separate rule 
		{ $$ = $1; }
	| QUOTED_STRING
		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->set(*$1); }
	| json_variable
		{ $$ = mkVariableNode($1); }
	;

%type <exprNode> path_accessor
path_accessor
	: path_primary %prec UMINUS
		{
			$$ = $1;
		}
	| path_accessor path_method  // reduce/reduce conflict due to '.'
		{
			$$ = addOptTailNode($1, $2);
		}
	| path_accessor filter_expr  // reduce/reduce conflict due to '?'
		{
			if (m_isLaxMode)
				unwrapPrimary($1);

			JsonExprNode* exprNode = mkNode<JsonExprNode::FILTER_NODE>();
			exprNode->makeHeadNode(false);
			exprNode->addChild($2);

			$$ = addOptTailNode($1, exprNode);
		}
	| '(' json_path_arithmetic ')'
		{
			$$ = $2;
		}
	;


%type <exprNode> path_accessor_compiled
path_accessor_compiled
	: path_accessor
		{
			$$ = completePath($1);
		}
	;


// ['-'] <path with arithmetic, filter and methods>
%type <exprNode> json_path_expr
json_path_expr
	: path_accessor_compiled
		{
			auto* node = mkNode<JsonExprNode::CALCULATION_NODE>();
			node->addChild($1);
			$$ = node;
		}
	| arithmetic_unary path_accessor_compiled
		{
			if ($2->applyUnaryOp($1))
			{
				$$ = $2; // Just a scalar
			}
			else
			{
				if (m_isLaxMode)
					unwrapPrimary($2);

				JsonExprNode* node = mkNode<JsonExprNode::CALCULATION_NODE>();
				node->addOperation($1);
				node->addChild($2);
				$$ = node;
			}
		}
	;

%type <exprOperation> arithmetic_unary
arithmetic_unary
	: '+'  %prec UPLUS { $$ = JsonExprOperation::UNARY_PLUS; }
	| '-'  %prec UMINUS { $$ = JsonExprOperation::UNARY_MINUS; }
	;

// (<passing> | <root>) [<arithmetic operator> (<passing> | <root>)]
%type <exprNode> json_path_arithmetic
json_path_arithmetic
	: json_path_expr
		{
			$$ = $1;
		}
	| json_path_arithmetic arithmetic_operator json_path_expr
		{
			if (m_isLaxMode)
			{
				$1->unwrap();
				$3->unwrap();
			}

			$1->addOperation($2);
			$1->addChild($3);
			$$ = $1;
		}
	;

%type <exprNode> json_path_arithmetic_finished
json_path_arithmetic_finished
	: json_path_arithmetic
		{
			$$ = $1->finish();
		}

%type <exprOperation> arithmetic_operator
arithmetic_operator
	: '+' { $$ = JsonExprOperation::ADDITION; }
	| '-' { $$ = JsonExprOperation::SUBTRACTION; }
	| '*' { $$ = JsonExprOperation::MULTIPLICATION; }
	| '/' { $$ = JsonExprOperation::DIVISION; }
	| '%' { $$ = JsonExprOperation::MODULO; }
	;

// **********************
// Any path body
// **********************

%type keyvalue_path_opt(<jpathContext>)
keyvalue_path_opt($context)
	: path_body_opt($context)
	;

%type <variable> variable_body_opt(<jpathContext>)
variable_body_opt($context)
	: path_body_opt($context)
		{ $$ = $context->variable; }
	;

%type path_body_opt(<jpathContext>)
path_body_opt($context)
	: /*nothing*/
	| path_filer($context)
	| path_filer($context) path_body_accessors($context)
	| path_body_accessors($context)
	;

%type path_body_accessors(<jpathContext>)
path_body_accessors($context)
	: accessor_with_opt_filter($context) %prec UMINUS // prec to solve reduce/reduce conflicts
	| path_body_accessors accessor_with_opt_filter($context)
	;

%type accessor_with_opt_filter(<jpathContext>)
accessor_with_opt_filter($context)
	: accessor($context) path_filter_opt($context)
	;

%type path_filter_opt(<jpathContext>)
path_filter_opt($context)
	: /* Nothing */
	| path_filer($context)
	;

%type path_filer(<jpathContext>)
path_filer($context)
	: filter_expr
		{
			if (m_isLaxMode)
				$context->current->flags |= PathNode::FLAG_UNWRAP;

			$context->main->setComplexRangeFlag();
			$context->current->filterNode = $1;
		}
	;


// Range or object
%type accessor(<jpathContext>)
accessor($context)
	: '.' object($context)
		{
			// Mark prev level
			if (m_isLaxMode && $context->current->type == ItemType::FIELD)
				$context->current->flags |= PathNode::FLAG_UNWRAP;

			// Add after in case of PathMethod
			$context->current = $context->current->add();
			$context->current->type = ItemType::FIELD;

			$context->current->field = *$2;

			$context->main->setFieldsFlag();
		}
	| '[' ranges($context) ']'
		{
			$context->current = $context->current->add();
			$context->current->type = ItemType::ARRAY_ELEMENT;

			const USHORT size = $context->current->ranges.getCount();

			bool isComplex = false;
			for (USHORT i = 0; i < size; i++)
			{
				const auto& range = $context->current->ranges[i];
				if (m_isLaxMode && range.canWrap())
					$context->current->flags |= PathNode::FLAG_WRAP;

				if (range.isComplex())
					isComplex = true;
			}

			if (size > 0)
			{
				$context->main->setIndexesFlag();
			}

			if (isComplex || size > 1)
				$context->main->setComplexRangeFlag();
		}
	;


// Object types
%type <stringPtr> object(<jpathContext>)
object($context)
	: identifier		{ $$ = $1; }
	| '*'				{ $$ = mkTempString("*"); }
	| PASSING_NAME
		{
			auto& injection = addPathInjection($context, *$1);
			injection.position = PathInjection::Position::FIELD;

			$$ = mkTempString("");
		}
	| '(' json_path_arithmetic_finished ')'
		{
			auto& injection = addPathInjection($context, $2);
			injection.position = PathInjection::Position::FIELD;

			$$ = mkTempString("");
		}
	;

%type <stringPtr> identifier
identifier
	: STRING			{ $$ = $1;}
	| QUOTED_STRING		{ $$ = $1;}
	;


// Range types
%type ranges(<jpathContext>)
ranges($context)
	: {$context->clearRange(); } indexes($context)
	;

%type indexes(<jpathContext>)
indexes($context)
	: index($context)
	| indexes ',' index($context)
	;

%type index(<jpathContext>)
index($context)
	: single_index($context, PathInjection::Position::RANGE_INDEX) range_opt($context)
		{
			const auto endRange = $2.toOptional();
			if (endRange == std::nullopt)
				$context->addRange($1);
			else
				$context->addRange($1, endRange.value());
		}
	| '*'
		{ $context->addRange(0, -1); }
	;

// Use TO <value> instead of <value> TO <value> to resolve conflicts
%type <int16ValOpt> range_opt(<jpathContext>)
range_opt($context)
	: /* nothing */
		{ $$ = std::nullopt; }
	| TO single_index($context, PathInjection::Position::RANGE_UP)
		{ $$ = $2; }
	;

%type <int16Val> single_index(<jpathContext>, <passingPos>)
single_index($context, $passType)
	: LAST
		{ $$ = -1; }
	| LAST '-' INTEGER16
		{ $$ = -1 - $3; }
	| LAST '+' INTEGER16 // Allow this obvious out of range value in case of modification (appending element) 
		{ $$ = -1 + $3; }
	| json_path_arithmetic_finished // Get number or an expression
		{
			auto rangeNumber = $1->getRangeNumber();
			if (rangeNumber != std::nullopt)
			{
				$$ = rangeNumber.value();
			}
			else
			{
				auto& injection = addPathInjection($context, $1);
				injection.position = $passType;
				injection.rangeIndex = $context->current->ranges.getCount();

				$$ = 1; // Return non zero to do not match unwrap pattern
			}
		}
	;


// **********************
// Methods types
// **********************

// Any scalar type (number, string...)
%type <exprNode> scalar_method
scalar_method
	: no_arg_method { $$ = $1; }
	| datetime_methods { $$ = $1; }
	/* | decimal_method { $$ = $1; } */
	;

// All the standard types are digital-oriented
%type <exprNode> no_arg_method
no_arg_method
	: METHOD_DOT METHOD '(' ')'
		{
			// The METHOD token includes the first '('
			$$ = mkMethodNode($2);
		}
	;

// Include JSON-oriented types like Array and Object
%type <exprNode> path_method
path_method
	: scalar_method
		{ $$ = $1; }
	| METHOD_DOT KEYVALUE '(' ')' { $<jpathContext>$ = createContext(); } /* 1-5 */ keyvalue_path_opt($<jpathContext>5) //6
		{
			JsonExprNode* tail = nullptr;

			if (!$<jpathContext>5->isEmpty())
			{
				PathVariable* var = FB_NEW_POOL(getPool()) PathVariable(getPool(), PathVariable::Type::HEAD);
				var->path.reset($<jpathContext>5->extract());

				tail = mkVariableNode(var);
			}

			$$ = mkMethodNode(PathMethod::KEYVALUE, tail);
		}
	;


%type <methodType> time_json_method
time_json_method
	: TIME {$$ = PathMethod::TIME;}
	| TIME_TZ {$$ = PathMethod::TIME_TZ;}
	| TIMESTAMP {$$ = PathMethod::TIMESTAMP;}
	| TIMESTAMP_TZ {$$ = PathMethod::TIMESTAMP_TZ;}
	;

%type <exprNode> datetime_methods
datetime_methods
//	: METHOD_DOT time_json_method '(' time_precision_opt ')' // Currently, time precision is not supported in SQL
//		{ $$ = mkMethodNode($2, $4); }
	: METHOD_DOT time_json_method '(' ')'
		{ $$ = mkMethodNode($2, nullptr); }
	| METHOD_DOT DATETIME '(' datetime_template_opt ')'
		{ $$ = mkMethodNode(PathMethod::DATETIME, $4); }
	;

%type <exprNode> decimal_method
decimal_method
	: METHOD_DOT DECIMAL '(' decimal_arguments ')'
		{
			auto* methodNode = mkMethodNode(PathMethod::DECIMAL);
			if ($4.precision)
			{
				$4.precision->addFlag(JsonExprNode::FLAG_METHOD_ARGUMENT);
				methodNode->addChild($4.precision);

				if ($4.scale)
				{
					$4.scale->addFlag(JsonExprNode::FLAG_METHOD_ARGUMENT);
					methodNode->addChild($4.scale);
				}
			}

			$$ = methodNode;
		}
	;

%type <pairVal> decimal_arguments
decimal_arguments
	: //nothing
		{ $$ = {nullptr, nullptr}; }
	| json_decimal_arg // precision
		{  $$ = {$1, nullptr}; }
	| json_decimal_arg ',' json_decimal_arg // precision, scale
		{ $$ = {$1, $3}; }
	;


%type <exprNode> json_decimal_arg
json_decimal_arg
	: INTEGER32
		{
			$$ = mkNode<JsonExprNode::SCALAR_NODE>()->set((int)$1);
			$$->addFlag(JsonExprNode::FLAG_METHOD_ARGUMENT);
		}
	;

// **********************
// Filter rules
// **********************

%type <exprNode> filter_expr
filter_expr
	: '?' { m_parseInFilter = true; } '(' boolean_expression ')' { m_parseInFilter = false; $$ = $4; }
	;

// Boolean exprs

%type <exprNode> value_to_compare
value_to_compare
	: json_path_arithmetic_finished
		{ $$ = $1; }
	| boolean
		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->set($1); }
	| JSON_NULL
		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->setNull(); }
	;


%type <exprNode> boolean_expression
boolean_expression
	: unary_predicate
		{ $$ = $1; }
	| boolean_expression bvb_comparison unary_predicate // shift Conflict
		{
			auto* root = mkNode<JsonExprNode::CALCULATION_NODE>();
			root->addChild($1);
			root->addOperation($2);
			root->addChild($3);
			$$ = root->finish();
		}
	;


// bool vs bool
%type <exprOperation> bvb_comparison
bvb_comparison
	: OR  { $$ = JsonExprOperation::OR; }
	| AND { $$ = JsonExprOperation::AND; }
	;

%type <exprNode> unary_predicate
unary_predicate
	: predicate	{ $$ = $1; }
	| not_arg predicate
		{
			auto* root = mkNode<JsonExprNode::CALCULATION_NODE>();
			root->addOperation($1);
			root->addChild($2);
			$$ = root->finish();
		}
	;

%type <exprNode> predicate
predicate
	: comparison_predicate			{ $$ = $1; }
	| filter_function				{ $$ = $1; }
	| '(' boolean_expression ')'	{ $$ = $2; }
	;

%type <exprNode> comparison_predicate
comparison_predicate
	: value_to_compare comparison_operator value_to_compare
		{
			if (m_isLaxMode)
			{
				$1->unwrap();
				$3->unwrap();
			}

			JsonExprNode* root = mkNode<JsonExprNode::CALCULATION_NODE>();
			root->addChild($1);
			root->addOperation($2);
			root->addChild($3);
			$$ = root->finish();
		}
	;

%type <exprNode> filter_function
filter_function
	: function_str_arg STARTS WITH function_str_arg
		{
			if (m_isLaxMode)
				$1->unwrap();

			auto* root = mkNode<JsonExprNode::CALCULATION_NODE>();
			root->addChild($1);
			root->addOperation(JsonExprOperation::STARTS_WITH);
			root->addChild($4);
			$$ = root->finish();
		}
	| function_str_arg LIKE_REGEX function_str_arg regex_flags_opt
		{
			if (m_isLaxMode)
				$1->unwrap();

			auto* root = mkNode<JsonExprNode::CALCULATION_NODE>();
			root->addChild($1);
			root->addOperation(JsonExprOperation::LIKE_REGEX);
			root->addChild($3);
			if ($4)
			{
				root->addOperation(JsonExprOperation::REGEX_FLAGS);
				root->addChild($4);
			}

			$$ = root->finish();
		}
	| EXISTS '(' json_path_arithmetic_finished ')'
		{
			auto* root = mkNode<JsonExprNode::CALCULATION_NODE>();
			root->addOperation(JsonExprOperation::EXISTS);
			root->addChild($3);
			$$ = root->finish();
		}
	| '(' boolean_expression ')' IS UNKNOWN
		{
			auto* root = mkNode<JsonExprNode::CALCULATION_NODE>();
			root->addChild($2);
			root->addOperation(JsonExprOperation::IS_UNKNOWN);
			$$ = root->finish();
		}
	;

// Allow invalid types in case of some kind auto generated path
%type <exprNode> function_str_arg
function_str_arg
	: path_accessor
		{ $$ = $1; }
	;

%type <exprNode> datetime_template_opt
datetime_template_opt
	: /* nothing */ { $$ = nullptr; }
	| function_str_arg
		{
			$$ = $1;
			$$->addFlag(JsonExprNode::FLAG_METHOD_ARGUMENT);
		}
	;

%type <exprNode> regex_flags_opt
regex_flags_opt
	: // nothing
		{ $$ = nullptr; }
	| FLAGS QUOTED_STRING
		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->set(*$2); }
	;

%type <exprOperation> comparison_operator
comparison_operator
	: EQ  { $$ = JsonExprOperation::EQUALS; }
	| NEQ { $$ = JsonExprOperation::NOT_EQUALS; }
	| '>' { $$ = JsonExprOperation::MORE; }
	| '<' { $$ = JsonExprOperation::LESS; }
	| GEQ { $$ = JsonExprOperation::MORE_OE; }
	| LEQ { $$ = JsonExprOperation::LESS_OE; }
	;

%type <exprOperation> not_arg
not_arg
	: '!' { $$ = JsonExprOperation::NOT; }
	;


%type <exprNode> number_scalar_pos
number_scalar_pos
	: INTEGER16		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->set((int)$1); }
	| INTEGER32		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->set((int)$1); }
	| INTEGER64		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->set($1); }
	| DOUBLE		{ $$ = mkNode<JsonExprNode::SCALAR_NODE>()->set($1); }
	;

%type <exprNode> number_scalar
number_scalar
	: number_scalar_pos		{ $$ = $1; }
	| '-' number_scalar_pos { $$ = $2; $$->applyUnaryOp(JsonExprOperation::UNARY_MINUS); }
	;

%type <exprNode> time_precision_opt
time_precision_opt
	: { $$ = nullptr; }
	| time_precision { $$ = $1; }
	;

%type <exprNode> time_precision
time_precision
	: INTEGER16
	{
		$$ = mkNode<JsonExprNode::SCALAR_NODE>()->set((int)$1);
		$$->addFlag(JsonExprNode::FLAG_METHOD_ARGUMENT);
	}
	;

%type <boolVal> boolean
boolean
	: TRUE  { $$ = true; }
	| FALSE { $$ = false; }
	;

%%
