/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		gen_proto.h
 *	DESCRIPTION:	Prototype Header file for gen.cpp
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

#ifndef DSQL_GEN_PROTO_H
#define DSQL_GEN_PROTO_H

namespace Firebird::Jrd {
	class DmlNode;
	class DsqlCompilerScratch;
	class ExprNode;
	class RseNode;
	class dsql_ctx;
	class dsql_msg;
	class dsql_par;
	class ValueListNode;
}

namespace Firebird::Jrd {

void GEN_arg(DsqlCompilerScratch*, ExprNode*);
void GEN_descriptor(DsqlCompilerScratch* dsqlScratch, const dsc* desc, bool texttype);
void GEN_expr(DsqlCompilerScratch*, ExprNode*);
void GEN_hidden_variables(DsqlCompilerScratch* dsqlScratch);
void GEN_parameter(DsqlCompilerScratch*, const dsql_par*);
void GEN_port(DsqlCompilerScratch*, dsql_msg*);
void GEN_statement(DsqlCompilerScratch*, DmlNode*);
void GEN_rse(DsqlCompilerScratch*, RseNode*);
void GEN_sort(DsqlCompilerScratch*, UCHAR, ValueListNode*);
void GEN_stuff_context(DsqlCompilerScratch*, const dsql_ctx*);
void GEN_stuff_context_number(DsqlCompilerScratch*, USHORT);

} // namespace Firebird::Jrd

#endif //  DSQL_GEN_PROTO_H
