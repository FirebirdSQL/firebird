# JSON

Functional to parse, generate, query and store JSON

## JSON Path

The JSON Path is uses to query data from JSON.
For example, the path `$[*].name` with JSON `[{"name":"John", "score":10}, {"name":"Sam", "score":50}]` will produce the sequence `["John","name"]`.
To filter elements, a filer can be used: `$[*] ? (@.score > 15)`. Child filter is also supported:  `$[*] ? (@.score > 15).name`.

The JSON Path has 2 modes: lax (default) and strict. The first one allows to flex path and JSON content matching.
Missing fields and incorrect array ranges are ignored. The strict one produces an error. The full rules:
1) Invalid array range: lax - allowed, strict - error
2) Index of array range: lax - allowed, strict - error
3) Missing field: lax - allowed, strict - error
4) Error inside a JSON Path Filter: lax - hides (the), strict - throw error
5) Unwrapping. lax: for `$.name` with `[{"name":"John"},{"name":"Sam"}]`, the array will be unwrapped. A path will be consider as follow: `$[*].name`. Strict - error
6) Path unwrapping before methods (except `size()` and `type()`) in lax mode
7) Path unwrapping before filters in lax mode
8) Path unwrapping before unary `-` and `+`
9) Path unwrapping for arithmetic operands`
10) Wrapping. lax: for `$[*].name` with `{"name":"John"}`, the object will be wrapped. A path will be consider as follow: `$.name`. Strict - error

The JSON PATH is used in JSON_VALUE, JSON_QUERY, JSON_EXISTS and JSON_TABLE
The full syntax:

```sql
<JSON path expression> ::=
	<JSON path mode> <JSON path expr>

<JSON path mode> ::= lax | strict

<JSON path> ::=
	'$' [<JSON path accessors>]
	| '@' [<JSON path accessors>]
	| <scalar>
	| <Passing variable> [<JSON path accessors>]
	| '(' <JSON path expr> ')'

<Passing variable> ::= '$' <identifier>

<JSON path expr> ::=
	[<JSON unary>] <JSON path>
	| <JSON path expr> <arithmetic expression> <JSON path expr>
	| <JSON path expr> <JSON item method>
	| <JSON path expr> <JSON filter expression>

<JSON path accessors> ::= <JSON accessor op> [<JSON accessor op> ...]

<JSON accessor op> ::=
	<JSON member accessor>
	| <JSON wildcard member accessor>
	| <JSON array accessor>
	| <JSON filter expression>
	| <JSON item method>

<JSON member accessor> ::=
	'.' <quoted string literal>
	| '.' <string literal>

<JSON wildcard member accessor> ::=
	'.' '*'

<JSON array accessor> ::=
	'[' <JSON subscript list> ']'

<JSON subscript list> ::=
	<JSON subscript> [ { <comma> <JSON subscript> }... ]

<JSON subscript> ::=
	<integer>
	| 'last'
	| 'last' - <integer>
	| 'last' + <integer>
	| <integer> to <integer>
	| '*'
	| <JSON path expr>


<JSON unary> ::=
	'+'
	| '-'

<arithmetic expression> ::=
	'+'
	| '-'
	| '*'
	| '/'
	| '%'


<JSON filter expression> ::=
	'?' '(' <JSON comparison predicate> ')'

<JSON item method> ::=
	type '(' ')'
	| size '(' ')'
	| double '(' ')'
	| ceiling '(' ')'
	| floor '(' ')'
	| abs '(' ')'
	| datetime '(' [ <JSON datetime template> ] ')'
	| keyvalue '(' ')' [<JSON path accessors>]
	| bigint '(' ')'
	| boolean '(' ')'
	| date '(' ')'
	| integer '(' ')'
	| number '(' ')'
	| string '(' ')'
	| time '(' ')'
	| time_tz '('')'
	| timestamp '('')'
	| timestamp_tz '('')'

<JSON datetime template> ::=
	<string literal>


<JSON comparison predicate> ::=
	<JSON delimited predicate>
	| <JSON non-delimited predicate>

<JSON delimited predicate> ::=
	<JSON exists path predicate>
	| '(' <JSON boolean disjunction> ')'

<JSON exists path predicate> ::=
	exists '(' <JSON path expr> ')'


<JSON non-delimited predicate> ::=
	<JSON comparison predicate>
	| <JSON like_regex predicate>
	| <JSON starts with predicate>
	| <JSON unknown predicate>
	| <JSON path expr>

<JSON comparison predicate> ::=
	<JSON boolean negation> <JSON comp op> <JSON boolean negation>

<JSON comp op> ::=
	'=='
	| '!='
	| '<>'
	| '<'
	| '>'
	| '<='
	| '>='

<JSON like_regex predicate> ::=
	<JSON path expr> like_regex <JSON like_regex pattern> [ flag <JSON like_regex flags> ]

<JSON like_regex pattern> ::=
	<string literal> [. <JSON item method> ]
	| <JSON path>

<JSON like_regex flags> ::=	<string literal>

<JSON unary expression> ::=
	<JSON accessor expression>

<JSON starts with predicate> ::=
	<JSON path expr> starts with <JSON starts with initial>

<JSON starts with initial> ::=
	<string literal> [. <JSON item method> ]
	| <JSON path>

<JSON unknown predicate> ::=
	'(' <JSON boolean disjunction> ')' is unknown

<JSON boolean negation> ::=
	<JSON comparison predicate>
	| '!' <JSON delimited predicate>


<JSON boolean disjunction> ::=
	<JSON boolean conjunction>
	| <JSON boolean disjunction> '||' <JSON boolean conjunction>

<JSON boolean conjunction> ::=
	<JSON boolean negation>
	| <JSON boolean conjunction> '&&' <JSON boolean negation>
```

### Nodes
JSON Path should be a compile time constant

### Examples:
```sql
SELECT JSON_VALUE('[1,2,3]', '$[2]') FROM RDB$DATABASE;
SELECT JSON_QUERY('[1,2,3]', '$ ? (@ > 1)' WITH WRAPPER) FROM RDB$DATABASE;
SELECT JSON_QUERY('[1,2,3]', 'strict $[*] ? (@ > 1)' WITH WRAPPER) FROM RDB$DATABASE;
SELECT JSON_QUERY('[1,-2,3]', 'strict $[*].abs()' WITH WRAPPER) FROM RDB$DATABASE;

SELECT JSON_QUERY('[{"name":"John", "score":10}, {"name":"Sam", "score":50}]', '$ ? (@.score > 15).name' WITH WRAPPER) FROM RDB$DATABASE;
SELECT JSON_QUERY('{"items":[{"name":"John"}, {"name":"Sam", "score":50}]}', '$.items ? (exists(@.score) && @.score < 15).name' WITH WRAPPER) FROM RDB$DATABASE;

```
