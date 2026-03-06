/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		pass1_proto.h
 *	DESCRIPTION:	Prototype Header file for pass1.cpp
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

#ifndef DSQL_PASS1_PROTO_H
#define DSQL_PASS1_PROTO_H

namespace Firebird::Jrd
{
	class CompoundStmtNode;
	class DeclareCursorNode;
	class DsqlMapNode;
	class ExprNode;
	class RecordSourceNode;
	class RseNode;
	class SelectExprNode;
	class SelectNode;
	class ValueExprNode;
	class ValueListNode;

	void PASS1_ambiguity_check(DsqlCompilerScratch*, const MetaName&, const DsqlContextStack&);
	bool PASS1_compare_alias(const QualifiedName& contextAlias, const QualifiedName& lookupAlias);
	bool PASS1_compare_alias(const ObjectsArray<QualifiedName>& contextAlias,
		const ObjectsArray<QualifiedName>& lookupAlias);
	BoolExprNode* PASS1_compose(BoolExprNode*, BoolExprNode*, UCHAR);
	DeclareCursorNode* PASS1_cursor_name(DsqlCompilerScratch*, const MetaName&, USHORT, bool);
	RseNode* PASS1_derived_table(DsqlCompilerScratch*, SelectExprNode*, const char*,
		const SelectNode* = nullptr);
	void PASS1_expand_contexts(DsqlContextStack& contexts, dsql_ctx* context);
	ValueListNode* PASS1_expand_select_list(DsqlCompilerScratch*, ValueListNode*, RecSourceListNode*);
	void PASS1_expand_select_node(DsqlCompilerScratch*, ExprNode*, ValueListNode*, bool);
	void PASS1_field_unknown(const TEXT*, const TEXT*, const ExprNode*);
	void PASS1_limit(DsqlCompilerScratch*, NestConst<ValueExprNode>,
		NestConst<ValueExprNode>, RseNode*);
	ValueExprNode* PASS1_lookup_alias(DsqlCompilerScratch*, const MetaName&,
		ValueListNode*, bool);
	dsql_ctx* PASS1_make_context(DsqlCompilerScratch* statement, RecordSourceNode* relationNode);
	bool PASS1_node_match(DsqlCompilerScratch*, const ExprNode*, const ExprNode*, bool);
	DsqlMapNode* PASS1_post_map(DsqlCompilerScratch*, ValueExprNode*, dsql_ctx*,
		WindowClause*);
	RecordSourceNode* PASS1_relation(DsqlCompilerScratch*, RecordSourceNode*);
	RseNode* PASS1_rse(DsqlCompilerScratch*, SelectExprNode*, const SelectNode* = nullptr);
	bool PASS1_set_parameter_type(DsqlCompilerScratch*, ValueExprNode*, std::function<void (dsc*)>, bool);
	bool PASS1_set_parameter_type(DsqlCompilerScratch*, ValueExprNode*, NestConst<ValueExprNode>, bool);
	ValueListNode* PASS1_sort(DsqlCompilerScratch*, ValueListNode*, ValueListNode*);
} // namespace Firebird::Jrd


#endif // DSQL_PASS1_PROTO_H
