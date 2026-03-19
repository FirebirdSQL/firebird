/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		dsql.h
 *	DESCRIPTION:	General Definitions for V4 DSQL module
 *
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
 * 2001.11.26 Claudio Valderrama: include udf_arguments and udf_flags
 *   in the udf struct, so we can load the arguments and check for
 *   collisions between dropping and redefining the udf concurrently.
 *   This closes SF Bug# 409769.
 * 2002.10.29 Nickolay Samofatov: Added support for savepoints
 * 2004.01.16 Vlad Horsun: added support for default parameters and
 *   EXECUTE BLOCK statement
 * Adriano dos Santos Fernandes
 */

#ifndef DSQL_DSQL_H
#define DSQL_DSQL_H

#include <numeric>
#include "../common/classes/array.h"
#include "../common/classes/fb_atomic.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/MetaName.h"
#include "../common/classes/stack.h"
#include "../common/classes/auto.h"
#include "../common/classes/NestConst.h"
#include "../jrd/EngineInterface.h"
#include "../jrd/RuntimeStatistics.h"
#include "../jrd/ntrace.h"
#include "../jrd/val.h"  // Get rid of duplicated FUN_T enum.
#include "../jrd/Attachment.h"
#include "../dsql/BlrDebugWriter.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/DsqlCursor.h"


#ifdef DEV_BUILD
// This macro enables DSQL tracing code
#define DSQL_DEBUG
#endif

#ifdef DSQL_DEBUG
DEFINE_TRACE_ROUTINE(dsql_trace);
#endif

// generic block used as header to all allocated structures
#include "../include/fb_blk.h"

#include "../dsql/sym.h"

// Context aliases used in triggers
inline constexpr const char* OLD_CONTEXT_NAME = "OLD";
inline constexpr const char* NEW_CONTEXT_NAME = "NEW";

inline constexpr int OLD_CONTEXT_VALUE = 0;
inline constexpr int NEW_CONTEXT_VALUE = 1;

namespace Jrd
{
	class Attachment;
	class Database;
	class DsqlCompilerScratch;
	class DsqlStatement;
	class DsqlStatementCache;
	class RseNode;
	class ValueExprNode;
	class ValueListNode;
	class WindowClause;
	class jrd_tra;
	class Request;
	class blb;
	struct bid;

	class dsql_ctx;
	class dsql_par;
	class dsql_map;
	class dsql_intlsym;
	class dsql_fld;
	class TimeoutTimer;
	class MetaName;

	typedef Firebird::Stack<dsql_ctx*> DsqlContextStack;
	typedef Firebird::Pair<Firebird::Left<MetaName, NestConst<Jrd::WindowClause>>> NamedWindowClause;
	typedef Firebird::ObjectsArray<NamedWindowClause> NamedWindowsClause;
}

//======================================================================
// remaining node definitions for local processing
//

/// Include definition of descriptor

#include "../common/dsc.h"

namespace Jrd {

// blocks used to cache metadata

// Database Block
class dsql_dbb : public pool_alloc<dsql_type_dbb>
{
public:
	Firebird::LeftPooledMap<QualifiedName, class dsql_intlsym*> dbb_charsets;	// known charsets in database
	Firebird::LeftPooledMap<QualifiedName, class dsql_intlsym*> dbb_collations;	// known collations in database
	Firebird::NonPooledMap<SSHORT, dsql_intlsym*> dbb_charsets_by_id;		// charsets sorted by charset_id
	Firebird::LeftPooledMap<Firebird::string, DsqlDmlRequest*> dbb_cursors;	// known cursors in database
	Firebird::AutoPtr<DsqlStatementCache> dbb_statement_cache;

	MemoryPool& dbb_pool;			// The current pool for the dbb
	Attachment* dbb_attachment;
	Firebird::FullPooledMap<MetaName, QualifiedName> dbb_schemas_dfl_charset;
	QualifiedName dbb_dfl_charset;
	bool dbb_no_charset = false;

	dsql_dbb(MemoryPool& p, Attachment* attachment);
	~dsql_dbb();

	MemoryPool* createPool(ALLOC_PARAMS_NO_COMMA);
	void deletePool(MemoryPool* pool);
};

//! Relation block
class dsql_rel : public pool_alloc<dsql_type_rel>
{
public:
	explicit dsql_rel(MemoryPool& p)
		: rel_name(p),
		  rel_owner(p)
	{
	}

	dsql_rel(MemoryPool& p, class jrd_rel* jrel);
	dsql_rel(MemoryPool& p, const dsql_rel* rel);

	dsql_rel(const dsql_rel&) = delete;
	dsql_rel(dsql_rel&&) = delete;

	dsql_fld* rel_fields = nullptr;	// Field block
	QualifiedName rel_name;			// Name of relation
	MetaName rel_owner;				// Owner of relation
	USHORT rel_id = 0;				// Relation id
	USHORT rel_dbkey_length = 0;
	USHORT rel_flags = 0;
};

// rel_flags bits
enum rel_flags_vals {
	REL_new_relation	= 1, // relation exists in sys tables, not committed yet
	REL_dropped			= 2, // relation has been dropped
	REL_view			= 4, // relation is a view
	REL_external		= 8, // relation is an external table
	REL_creating		= 16,	// we are creating the bare relation in memory
	REL_ltt_created		= 32	// relation is created local temporary table
};

class TypeClause
{
public:
	TypeClause(MemoryPool& pool, const QualifiedName& aCollate)
		: fieldSource(pool),
		  typeOfTable(pool),
		  typeOfName(pool),
		  collate(pool, aCollate),
		  charSet(pool),
		  subTypeName(pool, nullptr)
	{
	}

	virtual ~TypeClause()
	{
	}

public:
	void setExactPrecision()
	{
		if (precision != 0)
			return;

		switch (dtype)
		{
			case dtype_short:
				precision = 4;
				break;

			case dtype_long:
				precision = 9;
				break;

			case dtype_int64:
				precision = 18;
				break;

			case dtype_int128:
				precision = 38;
				break;

			default:
				fb_assert(!DTYPE_IS_EXACT(dtype));
		}
	}

public:
	USHORT dtype = dtype_unknown;
	FLD_LENGTH length = 0;
	SSHORT scale = 0;
	SSHORT subType = 0;
	USHORT segLength = 0;				// Segment length for blobs
	USHORT precision = 0;				// Precision for exact numeric types
	USHORT charLength = 0;				// Length of field in characters
	std::optional<CSetId> charSetId;
	CollId collationId = CollId();
	TTypeId textType = TTypeId();
	bool fullDomain = false;			// Domain name without TYPE OF prefix
	bool notNull = false;				// NOT NULL was explicit specified
	QualifiedName fieldSource;
	QualifiedName typeOfTable;		// TYPE OF table name
	QualifiedName typeOfName;		// TYPE OF
	QualifiedName collate;
	QualifiedName charSet;			// empty means not specified
	MetaName subTypeName;				// Subtype name for later resolution
	USHORT flags = 0;
	USHORT elementDtype = 0;			// Data type of array element
	USHORT elementLength = 0;			// Length of array element
	SSHORT dimensions = 0;				// Non-zero means array
	ValueListNode* ranges = nullptr;	// ranges for multi dimension array
	bool explicitCollation = false;		// COLLATE was explicit specified
};

class dsql_fld : public TypeClause
{
public:
	explicit dsql_fld(MemoryPool& p)
		: TypeClause(p, {}),
		  fld_name(p)
	{
	}

	dsql_fld(MemoryPool& p, const dsc& desc, dsql_fld*** prev);

public:
	void resolve(DsqlCompilerScratch* dsqlScratch, bool modifying = false);

public:
	dsql_fld* fld_next = nullptr;				// Next field in relation
	class dsql_rel* fld_relation = nullptr;		// Parent relation
	class dsql_prc* fld_procedure = nullptr;	// Parent procedure
	USHORT fld_id = 0;							// Field ID in database
	USHORT fld_pos = 0;							// Field position in relation
	MetaName fld_name;
};

// values used in fld_flags

enum fld_flags_vals {
	FLD_computed	= 0x1,
	FLD_national	= 0x2, // field uses NATIONAL character set
	FLD_nullable	= 0x4,
	FLD_system		= 0x8,
	FLD_has_len		= 0x10,
	FLD_has_chset	= 0x20,
	FLD_has_scale	= 0x40,
	FLD_has_sub		= 0x80,
	FLD_legacy		= 0x100,
	FLD_native		= 0x200,
	FLD_extended	= 0x400,
	FLD_has_prec	= 0x800
};

//! Stored Procedure block
class dsql_prc : public pool_alloc<dsql_type_prc>
{
public:
	explicit dsql_prc(MemoryPool& p)
		: prc_name(p),
		  prc_owner(p)
	{
	}

	dsql_prc(MemoryPool& p, const class jrd_prc* jproc);

	dsql_prc(const dsql_prc&) = delete;
	dsql_prc(dsql_prc&&) = delete;

	dsql_fld* prc_inputs = nullptr;		// Input parameters
	dsql_fld* prc_outputs = nullptr;	// Output parameters

	QualifiedName prc_name;				// Name of procedure
	MetaName prc_owner;					// Owner of procedure
	USHORT prc_in_count = 0;
	USHORT prc_def_count = 0;			// number of inputs with default values
	USHORT prc_out_count = 0;
	SSHORT prc_id = 0;					// Procedure id
	USHORT prc_flags = 0;
	bool prc_private = false;			// Packaged private procedure

private:
	dsql_fld* cpFields(MemoryPool& p, const Firebird::Array<NestConst<Parameter>>& fields);
};

// prc_flags bits

enum prc_flags_vals {
	PRC_new_procedure	= 1,	// procedure is newly defined, not committed yet
	PRC_dropped			= 2,	// procedure has been dropped
	PRC_subproc			= 4		// Sub procedure
};

//! User defined function block
class dsql_udf : public pool_alloc<dsql_type_udf>
{
public:
	class Argument
	{
	public:
		Argument(MetaName name, const dsc& desc)
			: name(name), desc(desc)
		{ }

		Argument()
		{ }

	public:
		MetaName name;
		dsc desc;
	};

public:
	dsql_udf(MemoryPool& p, const class Function* jfun);

	explicit dsql_udf(MemoryPool& p)
		: udf_name(p), udf_arguments(p)
	{ }

	USHORT udf_dtype = 0;
	SSHORT udf_scale = 0;
	SSHORT udf_sub_type = 0;
	USHORT udf_length = 0;
	CSetId udf_character_set_id = CSetId();
	USHORT udf_flags = 0;
	QualifiedName udf_name;
	Firebird::Array<Argument> udf_arguments;
	bool udf_private = false;	// Packaged private function
	SSHORT udf_def_count = 0;	// number of inputs with default values
};

// udf_flags bits

enum udf_flags_vals {
	UDF_subfunc		= 4			// sub function
};

// Variables - input, output & local

//! Variable block
class dsql_var : public Firebird::PermanentStorage
{
public:
	enum Type
	{
		TYPE_INPUT,
		TYPE_OUTPUT,
		TYPE_LOCAL,
		TYPE_HIDDEN
	};

public:
	explicit dsql_var(MemoryPool& p)
		: PermanentStorage(p)
	{
	}

	dsql_fld* field = nullptr;	// Field on which variable is based
	Type type = TYPE_INPUT;		// Input, output, local or hidden variable
	USHORT msgNumber = 0;		// Message number containing variable
	USHORT msgItem = 0;			// Item number in message
	USHORT number = 0;			// Local variable number
	bool initialized = false;	// Is variable initialized?
	dsc desc;
};


// Symbolic names for international text types
// (either collation or character set name)

//! International symbol
class dsql_intlsym : public pool_alloc<dsql_type_intlsym>
{
public:
	explicit dsql_intlsym(MemoryPool& p)
		: intlsym_name(p)
	{
	}

	QualifiedName intlsym_name;
	USHORT intlsym_type = 0;		// what type of name
	USHORT intlsym_flags = 0;
	TTypeId intlsym_ttype = TTypeId();		// id of implementation
	CSetId intlsym_charset_id = CSetId();
	CollId intlsym_collate_id = CollId();
	USHORT intlsym_bytes_per_char = 0;
};


// Table value function
class dsql_tab_func : public pool_alloc<dsql_type_tab_func>
{
public:
	explicit dsql_tab_func(MemoryPool& p)
		: funName(p),
		  outputField(nullptr)
	{
	}

	MetaName	funName;			// Name of function
	dsql_fld*	outputField;		// Output parameters
};

// values used in intlsym_flags

enum intlsym_flags_vals {
	INTLSYM_dropped	= 1  // intlsym has been dropped
};

//! Implicit (NATURAL and USING) joins
class ImplicitJoin : public pool_alloc<dsql_type_imp_join>
{
public:
	ValueExprNode* value = nullptr;
	dsql_ctx* visibleInContext = nullptr;
};

struct WindowMap
{
	WindowMap(WindowClause* aWindow)
		: window(aWindow)
	{
	}

	NestConst<ValueListNode> partitionRemapped;
	NestConst<WindowClause> window;
	dsql_map* map = nullptr;
	USHORT context = 0;
};

//! Context block used to create an instance of a relation reference
class dsql_ctx : public pool_alloc<dsql_type_ctx>
{
public:
	explicit dsql_ctx(MemoryPool& p)
		: ctx_alias(p),
		  ctx_internal_alias(p),
		  ctx_main_derived_contexts(p),
		  ctx_childs_derived_table(p),
	      ctx_imp_join(p),
	      ctx_win_maps(p),
	      ctx_named_windows(p)
	{
	}

	dsql_rel* ctx_relation = nullptr;			// Relation for context
	dsql_prc* ctx_procedure = nullptr;			// Procedure for context
	dsql_tab_func* ctx_table_value_fun = nullptr;	// Table value function context
	NestConst<ValueListNode> ctx_proc_inputs;	// Procedure input parameters
	dsql_map* ctx_map = nullptr;				// Maps for aggregates and unions
	RseNode* ctx_rse = nullptr;					// Sub-rse for aggregates
	dsql_ctx* ctx_parent = nullptr;				// Parent context for aggregates
	USHORT ctx_context = 0;						// Context id
	USHORT ctx_recursive = 0;					// Secondary context id for recursive UNION (nobody referred to this context)
	USHORT ctx_scope_level = 0;					// Subquery level within this request
	USHORT ctx_flags = 0;						// Various flag values
	USHORT ctx_in_outer_join = 0;				// inOuterJoin when context was created
	Firebird::ObjectsArray<QualifiedName> ctx_alias;	// Context alias (can include concatenated derived table alias)
	QualifiedName ctx_internal_alias;			// Alias as specified in query
	DsqlContextStack ctx_main_derived_contexts;	// contexts used for blr_derived_expr
	DsqlContextStack ctx_childs_derived_table;	// Childs derived table context
	Firebird::LeftPooledMap<MetaName, ImplicitJoin*> ctx_imp_join;	// Map of USING fieldname to ImplicitJoin
	Firebird::Array<WindowMap*> ctx_win_maps;	// Maps for window functions
	Firebird::GenericMap<NamedWindowClause> ctx_named_windows;

	dsql_ctx& operator=(dsql_ctx& v)
	{
		ctx_relation = v.ctx_relation;
		ctx_procedure = v.ctx_procedure;
		ctx_table_value_fun = v.ctx_table_value_fun;
		ctx_proc_inputs = v.ctx_proc_inputs;
		ctx_map = v.ctx_map;
		ctx_rse = v.ctx_rse;
		ctx_parent = v.ctx_parent;
		ctx_alias = v.ctx_alias;
		ctx_context = v.ctx_context;
		ctx_recursive = v.ctx_recursive;
		ctx_scope_level = v.ctx_scope_level;
		ctx_flags = v.ctx_flags;
		ctx_in_outer_join = v.ctx_in_outer_join;
		ctx_main_derived_contexts.assign(v.ctx_main_derived_contexts);
		ctx_childs_derived_table.assign(v.ctx_childs_derived_table);
		ctx_imp_join.assign(v.ctx_imp_join);
		ctx_win_maps.assign(v.ctx_win_maps);
		ctx_named_windows.assign(v.ctx_named_windows);

		return *this;
	}

	Firebird::string getObjectName() const
	{
		if (ctx_relation)
			return ctx_relation->rel_name.toQuotedString();
		if (ctx_procedure)
			return ctx_procedure->prc_name.toQuotedString();
		return "";
	}

	Firebird::string getConcatenatedAlias() const
	{
		if (ctx_alias.hasData())
		{
			return std::accumulate(
					std::next(ctx_alias.begin()),
					ctx_alias.end(),
					ctx_alias[0].toQuotedString(),
					[](const auto& a, const auto& b) {
						return a + " " + b.toQuotedString();
					}
				);
		}

		return {};
	}

	bool getImplicitJoinField(const MetaName& name, NestConst<ValueExprNode>& node);
	WindowMap* getWindowMap(DsqlCompilerScratch* dsqlScratch, WindowClause* windowNode);
};

// Flag values for ctx_flags

inline constexpr USHORT CTX_outer_join 				= 0x01;		// reference is part of an outer join
inline constexpr USHORT CTX_system					= 0x02;		// Context generated by system (NEW/OLD in triggers, check-constraint, RETURNING)
inline constexpr USHORT CTX_null					= 0x04;		// Fields of the context should be resolved to NULL constant
inline constexpr USHORT CTX_returning				= 0x08;		// Context generated by RETURNING
inline constexpr USHORT CTX_recursive				= 0x10;		// Context has secondary number (ctx_recursive) generated for recursive UNION
inline constexpr USHORT CTX_view_with_check_store	= 0x20;		// Context of WITH CHECK OPTION view's store trigger
inline constexpr USHORT CTX_view_with_check_modify	= 0x40;		// Context of WITH CHECK OPTION view's modify trigger
inline constexpr USHORT CTX_cursor					= 0x80;		// Context is a cursor
inline constexpr USHORT CTX_lateral					= 0x100;	// Context is a lateral derived table
inline constexpr USHORT CTX_blr_fields				= 0x200;	// Fields of the context are defined inside BLR
inline constexpr USHORT CTX_package					= 0x400;	// The context is related to a package

//! Aggregate/union map block to map virtual fields to their base
//! TMN: NOTE! This datatype should definitely be renamed!
class dsql_map : public pool_alloc<dsql_type_map>
{
public:
	dsql_map* map_next = nullptr;			// Next map in item
	NestConst<ValueExprNode> map_node;		// Value for map item
	USHORT map_position = 0;				// Position in map
	NestConst<WindowMap> map_window;		// Partition
};

// Message block used in communicating with a running request
class dsql_msg : public Firebird::PermanentStorage
{
public:
	explicit dsql_msg(MemoryPool& p)
		: PermanentStorage(p),
		  msg_parameters(p)
	{
	}

	Firebird::Array<dsql_par*> msg_parameters;	// Parameter list
	USHORT msg_number = 0;			// Message number
	ULONG msg_length = 0;			// Message length
	USHORT msg_parameter = 0;		// Next parameter number
	USHORT msg_index = 0;			// Next index into SQLDA
};

// Parameter block used to describe a parameter of a message
class dsql_par : public Firebird::PermanentStorage
{
public:
	explicit dsql_par(MemoryPool& p)
		: PermanentStorage(p),
		  par_name(p),
		  par_rel_name(p),
		  par_owner_name(p),
		  par_rel_alias(p),
		  par_alias(p)
	{
	}

	dsql_msg* par_message = nullptr;	// Parent message
	dsql_par* par_null = nullptr;		// Null parameter, if used
	ValueExprNode* par_node = nullptr;	// Associated value node, if any
	MetaName par_name;					// Parameter name, if any
	QualifiedName par_rel_name;			// Relation name, if any
	MetaName par_owner_name;			// Owner name, if any
	MetaName par_rel_alias;				// Relation alias, if any
	MetaName par_alias;					// Alias, if any
	dsc par_desc;						// Field data type
	USHORT par_parameter = 0;			// BLR parameter number
	USHORT par_index = 0;				// Index into SQLDA, if appropriate
	bool par_is_text = false;			// Parameter should be dtype_text (SQL_TEXT) externaly
};

class CStrCmp
{
public:
	static int greaterThan(const char* s1, const char* s2)
	{
		return strcmp(s1, s2) > 0;
	}
};

typedef Firebird::SortedArray<const char*,
			Firebird::EmptyStorage<const char*>, const char*,
			Firebird::DefaultKeyValue<const char*>,
			CStrCmp>
		StrArray;

class IntlString
{
public:
	IntlString(Firebird::MemoryPool& p, const Firebird::string& str,
		const MetaName& cs = NULL)
		: charset(p, cs),
		  s(p, str)
	{ }

	explicit IntlString(const Firebird::string& str, const QualifiedName& cs = {})
		: charset(cs),
		  s(str)
	{ }

	IntlString(Firebird::MemoryPool& p, const IntlString& o)
		: charset(p, o.charset),
		  s(p, o.s)
	{ }

	explicit IntlString(Firebird::MemoryPool& p)
		: charset(p),
		  s(p)
	{ }

	void dsqlPass(DsqlCompilerScratch* dsqlScratch);

	Firebird::string toUtf8(jrd_tra* transaction) const;

	const QualifiedName& getCharSet() const noexcept
	{
		return charset;
	}

	void setCharSet(const QualifiedName& value)
	{
		charset = value;
	}

	const Firebird::string& getString() const noexcept
	{
		return s;
	}

	bool hasData() const
	{
		return s.hasData();
	}

	bool isEmpty() const
	{
		return s.isEmpty();
	}

private:
	QualifiedName charset;
	Firebird::string s;
};

class Lim64String : public Firebird::string
{
public:
	Lim64String(Firebird::MemoryPool& p, const Firebird::string& str, int sc)
		: Firebird::string(p, str),
		  scale(sc)
	{ }

	int getScale() noexcept
	{
		return scale;
	}

private:
	int scale;
};

struct SignatureParameter
{
	explicit SignatureParameter(MemoryPool& p)
		: name(p),
		  fieldSource(p),
		  fieldName(p),
		  relationName(p),
		  charSetName(p),
		  collationName(p),
		  subTypeName(p)
	{
	}

	SignatureParameter(MemoryPool& p, const SignatureParameter& o)
		: type(o.type),
		  number(o.number),
		  name(p, o.name),
		  fieldSource(p, o.fieldSource),
		  fieldName(p, o.fieldName),
		  relationName(p, o.relationName),
		  charSetName(p, o.charSetName),
		  collationName(p, o.collationName),
		  subTypeName(p, o.subTypeName),
		  collationId(o.collationId),
		  nullFlag(o.nullFlag),
		  mechanism(o.mechanism),
		  fieldLength(o.fieldLength),
		  fieldScale(o.fieldScale),
		  fieldType(o.fieldType),
		  fieldSubType(o.fieldSubType),
		  fieldSegmentLength(o.fieldSegmentLength),
		  fieldNullFlag(o.fieldNullFlag),
		  fieldCharLength(o.fieldCharLength),
		  fieldCollationId(o.fieldCollationId),
		  fieldCharSetId(o.fieldCharSetId),
		  fieldPrecision(o.fieldPrecision)
	{
	}

	void fromType(const TypeClause* type)
	{
		fieldType = type->dtype;
		fieldScale = type->scale;
		subTypeName = type->subTypeName;
		fieldSubType = type->subType;
		fieldLength = type->length;
		fieldCharLength = type->charLength;
		charSetName = type->charSet;
		fieldCharSetId = type->charSetId;
		collationName = type->collate;
		fieldCollationId = type->collationId;
		fieldSource = type->fieldSource;
		fieldName = type->typeOfName;
		relationName = type->typeOfTable;
		fieldSegmentLength = type->segLength;
		fieldPrecision = type->precision;
		nullFlag = (SSHORT) type->notNull;
		mechanism = (SSHORT) type->fullDomain;
	}

	SSHORT type = 0;
	SSHORT number = 0;
	MetaName name;
	QualifiedName fieldSource;
	QualifiedName fieldName;
	QualifiedName relationName;
	QualifiedName charSetName;
	QualifiedName collationName;
	MetaName subTypeName;
	std::optional<CollId> collationId;
	std::optional<SSHORT> nullFlag;
	SSHORT mechanism = 0;
	std::optional<SSHORT> fieldLength;
	std::optional<SSHORT> fieldScale;
	std::optional<SSHORT> fieldType;
	std::optional<SSHORT> fieldSubType;
	std::optional<SSHORT> fieldSegmentLength;
	std::optional<SSHORT> fieldNullFlag;
	std::optional<SSHORT> fieldCharLength;
	std::optional<SSHORT> fieldCollationId;
	std::optional<SSHORT> fieldCharSetId;
	std::optional<SSHORT> fieldPrecision;

	bool operator >(const SignatureParameter& o) const noexcept
	{
		return type > o.type || (type == o.type && number > o.number);
	}

	bool operator ==(const SignatureParameter& o) const
	{
		return type == o.type &&
			number == o.number &&
			name == o.name &&
			(fieldSource == o.fieldSource ||
				(fieldSource.schema == o.fieldSource.schema &&
					fb_utils::implicit_domain(fieldSource.object.c_str()) &&
					fb_utils::implicit_domain(o.fieldSource.object.c_str()))) &&
			fieldName == o.fieldName &&
			relationName == o.relationName &&
			collationId == o.collationId &&
			nullFlag.value_or(FALSE) == o.nullFlag.value_or(FALSE) &&
			mechanism == o.mechanism &&
			fieldLength == o.fieldLength &&
			fieldScale == o.fieldScale &&
			fieldType == o.fieldType &&
			fieldSubType.value_or(0) == o.fieldSubType.value_or(0) &&
			fieldSegmentLength == o.fieldSegmentLength &&
			fieldNullFlag.value_or(FALSE) == o.fieldNullFlag.value_or(FALSE) &&
			fieldCharLength == o.fieldCharLength &&
			charSetName == o.charSetName &&
			collationName == o.collationName &&
			subTypeName == o.subTypeName &&
			fieldCollationId.value_or(CollId()) == o.fieldCollationId.value_or(CollId()) &&
			fieldCharSetId == o.fieldCharSetId &&
			fieldPrecision == o.fieldPrecision;
	}

	bool operator !=(const SignatureParameter& o) const
	{
		return !(*this == o);
	}
};

struct Signature
{
	const static unsigned FLAG_DETERMINISTIC = 0x01;

	Signature(MemoryPool& p, const MetaName& aName)
		: name(p, aName),
		  parameters(p)
	{
	}

	explicit Signature(const MetaName& aName)
		: name(aName),
		  parameters(*getDefaultMemoryPool())
	{
	}

	explicit Signature(MemoryPool& p)
		: name(p),
		  parameters(p)
	{
	}

	Signature(MemoryPool& p, const Signature& o)
		: name(p, o.name),
		  parameters(p),
		  flags(o.flags),
		  defined(o.defined)
	{
		for (Firebird::SortedObjectsArray<SignatureParameter>::const_iterator i = o.parameters.begin();
			 i != o.parameters.end();
			 ++i)
		{
			parameters.add(*i);
		}
	}

	bool operator >(const Signature& o) const
	{
		return name > o.name;
	}

	bool operator ==(const Signature& o) const
	{
		if (name != o.name || flags != o.flags || parameters.getCount() != o.parameters.getCount())
			return false;

		for (Firebird::SortedObjectsArray<SignatureParameter>::const_iterator i = parameters.begin(),
				j = o.parameters.begin();
			i != parameters.end();
			++i, ++j)
		{
			if (*i != *j)
				return false;
		}

		return true;
	}

	bool operator !=(const Signature& o) const
	{
		return !(*this == o);
	}

	MetaName name;
	Firebird::SortedObjectsArray<SignatureParameter> parameters;
	unsigned flags = 0;
	bool defined = false;
};


} // namespace

/*! \var unsigned DSQL_debug
    \brief Debug level

    0       No output
    1       Display output tree in PASS1_statement
    2       Display input tree in PASS1_statement
    4       Display ddl BLR
    8       Display BLR
    16      Display PASS1_rse input tree
    32      Display SQL input string
    64      Display BLR in dsql/prepare
    > 256   Display yacc parser output level = DSQL_level>>8
*/

#ifdef DSQL_DEBUG
extern unsigned DSQL_debug;
#endif

#endif // DSQL_DSQL_H
