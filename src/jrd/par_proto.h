/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		par_proto.h
 *	DESCRIPTION:	Prototype header file for par.cpp
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
 */

#ifndef JRD_PAR_PROTO_H
#define JRD_PAR_PROTO_H

struct dsc;

namespace Firebird::Jrd
{
	class CompilerScratch;
	class jrd_rel;
	class Request;
	class Statement;
	class thread_db;
	class ItemInfo;
	class BoolExprNode;
	class CompoundStmtNode;
	class DmlNode;
	class MessageNode;
	class SortNode;
	class StmtNode;
	class ValueExprNode;
	class ValueListNode;

	using NodeParseFunc = DmlNode* (*)(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	ValueListNode*	PAR_args(thread_db*, CompilerScratch*, USHORT, USHORT);
	ValueListNode*	PAR_args(thread_db*, CompilerScratch*);
	DmlNode* PAR_blr(thread_db*, const MetaName* schema, Cached::Relation*, const UCHAR*, ULONG blr_length,
		CompilerScratch*, CompilerScratch**, Statement**, const bool, USHORT);
	void PAR_preparsed_node(thread_db*, Cached::Relation*, DmlNode*,
		CompilerScratch*, CompilerScratch**, Statement**, const bool, USHORT);
	BoolExprNode* PAR_validation_blr(thread_db*, const MetaName* schema, Cached::Relation*, const UCHAR* blr,
		ULONG blr_length, CompilerScratch*, CompilerScratch**, USHORT);
	StreamType		PAR_context(CompilerScratch*, SSHORT*);
	StreamType		PAR_context2(CompilerScratch*, SSHORT*);
	void			PAR_dependency(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
		SSHORT id, const MetaName& field_name);
	USHORT			PAR_datatype(thread_db*, BlrReader&, dsc*);
	USHORT			PAR_desc(thread_db*, CompilerScratch*, dsc*, ItemInfo* = NULL);
	void			PAR_error(CompilerScratch*, const Arg::StatusVector&, bool isSyntaxError = true);
	SSHORT			PAR_find_proc_field(const jrd_prc*, const MetaName&);
	ValueExprNode* PAR_gen_field(thread_db* tdbb, StreamType stream, USHORT id, bool byId = false);
	void PAR_getBlrVersionAndFlags(CompilerScratch* csb);
	ValueExprNode* PAR_make_field(thread_db*, CompilerScratch*, USHORT, const MetaName&);
	CompoundStmtNode*	PAR_make_list(thread_db*, StmtNodeStack&);
	ULONG			PAR_marks(CompilerScratch*);
	CompilerScratch*	PAR_parse(thread_db*, const UCHAR* blr, ULONG blr_length,
		bool internal_flag, ULONG = 0, const UCHAR* = NULL);

	RecordSourceNode* PAR_parseRecordSource(thread_db* tdbb, CompilerScratch* csb);
	RseNode*	PAR_rse(thread_db*, CompilerScratch*, SSHORT);
	RseNode*	PAR_rse(thread_db*, CompilerScratch*);
	SortNode*	PAR_sort(thread_db*, CompilerScratch*, UCHAR, bool);
	SortNode*	PAR_sort_internal(thread_db*, CompilerScratch*, bool, USHORT);
	SLONG			PAR_symbol_to_gdscode(const string&);

	BoolExprNode* PAR_parse_boolean(thread_db* tdbb, CompilerScratch* csb);
	ValueExprNode* PAR_parse_value(thread_db* tdbb, CompilerScratch* csb);
	StmtNode* PAR_parse_stmt(thread_db* tdbb, CompilerScratch* csb);
	DmlNode* PAR_parse_node(thread_db* tdbb, CompilerScratch* csb);
	void PAR_register(UCHAR blr, NodeParseFunc parseFunc);
	void PAR_syntax_error(CompilerScratch* csb, const TEXT* string);
	void PAR_warning(const Arg::StatusVector& v);
}	// namespace Firebird::Jrd

#endif // JRD_PAR_PROTO_H
