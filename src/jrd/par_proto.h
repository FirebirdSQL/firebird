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

namespace Jrd {
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
}

struct dsc;

Jrd::ValueListNode*	PAR_args(Jrd::thread_db*, Jrd::CompilerScratch*, USHORT, USHORT);
Jrd::ValueListNode*	PAR_args(Jrd::thread_db*, Jrd::CompilerScratch*);
Jrd::DmlNode* PAR_blr(Jrd::thread_db*, const Jrd::MetaName* schema, Jrd::jrd_rel*, const UCHAR*, ULONG blr_length,
	Jrd::CompilerScratch*, Jrd::CompilerScratch**, Jrd::Statement**, const bool, USHORT);
void PAR_preparsed_node(Jrd::thread_db*, Jrd::jrd_rel*, Jrd::DmlNode*,
	Jrd::CompilerScratch*, Jrd::CompilerScratch**, Jrd::Statement**, const bool, USHORT);
Jrd::BoolExprNode* PAR_validation_blr(Jrd::thread_db*, const Jrd::MetaName* schema, Jrd::jrd_rel*, const UCHAR* blr,
	ULONG blr_length, Jrd::CompilerScratch*, Jrd::CompilerScratch**, USHORT);
StreamType		PAR_context(Jrd::CompilerScratch*, SSHORT*);
StreamType		PAR_context2(Jrd::CompilerScratch*, SSHORT*);
void			PAR_dependency(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb, StreamType stream,
	SSHORT id, const Jrd::MetaName& field_name);
USHORT			PAR_datatype(Firebird::BlrReader&, dsc*);
USHORT			PAR_desc(Jrd::thread_db*, Jrd::CompilerScratch*, dsc*, Jrd::ItemInfo* = NULL);
void			PAR_error(Jrd::CompilerScratch*, const Firebird::Arg::StatusVector&, bool isSyntaxError = true);
SSHORT			PAR_find_proc_field(const Jrd::jrd_prc*, const Jrd::MetaName&);
Jrd::ValueExprNode* PAR_gen_field(Jrd::thread_db* tdbb, StreamType stream, USHORT id, bool byId = false);
Jrd::ValueExprNode* PAR_make_field(Jrd::thread_db*, Jrd::CompilerScratch*, USHORT, const Jrd::MetaName&);
Jrd::CompoundStmtNode*	PAR_make_list(Jrd::thread_db*, Jrd::StmtNodeStack&);
ULONG			PAR_marks(Jrd::CompilerScratch*);
Jrd::CompilerScratch*	PAR_parse(Jrd::thread_db*, const UCHAR* blr, ULONG blr_length,
	bool internal_flag, ULONG = 0, const UCHAR* = NULL);

Jrd::RecordSourceNode* PAR_parseRecordSource(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb);
Jrd::RseNode*	PAR_rse(Jrd::thread_db*, Jrd::CompilerScratch*, SSHORT);
Jrd::RseNode*	PAR_rse(Jrd::thread_db*, Jrd::CompilerScratch*);
Jrd::SortNode*	PAR_sort(Jrd::thread_db*, Jrd::CompilerScratch*, UCHAR, bool);
Jrd::SortNode*	PAR_sort_internal(Jrd::thread_db*, Jrd::CompilerScratch*, bool, USHORT);
SLONG			PAR_symbol_to_gdscode(const Firebird::string&);

Jrd::BoolExprNode* PAR_parse_boolean(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb);
Jrd::ValueExprNode* PAR_parse_value(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb);
Jrd::StmtNode* PAR_parse_stmt(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb);
Jrd::DmlNode* PAR_parse_node(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb);
void PAR_register(UCHAR blr, Jrd::NodeParseFunc parseFunc);
void PAR_syntax_error(Jrd::CompilerScratch* csb, const TEXT* string);
void PAR_warning(const Firebird::Arg::StatusVector& v);

#endif // JRD_PAR_PROTO_H
