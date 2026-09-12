/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		cmp_proto.h
 *	DESCRIPTION:	Prototype header file for cmp.cpp
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

#ifndef JRD_CMP_PROTO_H
#define JRD_CMP_PROTO_H

#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/scl.h"
#include "../jrd/Resources.h"

namespace Firebird::Jrd
{
	StreamType* CMP_alloc_map(thread_db*, CompilerScratch*, StreamType stream);
	ValueExprNode* CMP_clone_node_opt(thread_db*, CompilerScratch*, ValueExprNode*);
	BoolExprNode* CMP_clone_node_opt(thread_db*, CompilerScratch*, BoolExprNode*);
	ValueExprNode* CMP_clone_node(thread_db*, CompilerScratch*, ValueExprNode*);
	Statement* CMP_compile(thread_db* tdbb, const UCHAR* blr, ULONG blrLength, bool internalFlag,
		ULONG dbginfoLength, const UCHAR* dbginfo);
	Request* CMP_compile_request(thread_db*, const UCHAR* blr, ULONG blrLength, bool internalFlag);
	CompilerScratch::csb_repeat* CMP_csb_element(CompilerScratch*, StreamType element);
	const Format* CMP_format(thread_db*, CompilerScratch*, StreamType);
	Request* CMP_make_request(thread_db*, CompilerScratch*, bool);
	ItemInfo* CMP_pass2_validation(thread_db*, CompilerScratch*, const Item&);

	bool CMP_procedure_arguments(
		thread_db* tdbb,
		CompilerScratch* csb,
		Routine* routine,
		bool isInput,
		USHORT argCount,
		ObjectsArray<MetaName>* argNames,
		NestConst<ValueListNode>& sources,
		NestConst<ValueListNode>& targets,
		NestConst<MessageNode>& message,
		Arg::StatusVector& mismatchStatus);

	void CMP_post_access(thread_db*, CompilerScratch*, const MetaName&, SLONG ssRelationId,
						SecurityClass::flags_t, ObjectType obj_type,
						const QualifiedName&,
						const MetaName& = {});

	void CMP_post_procedure_access(thread_db*, CompilerScratch*, Cached::Procedure*);
	RecordSource* CMP_post_rse(thread_db*, CompilerScratch*, RseNode*);
	void CMP_release(thread_db*, Request*);
}	// namespace Firebird::Jrd

#endif // JRD_CMP_PROTO_H
