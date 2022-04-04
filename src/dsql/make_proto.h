/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		make_proto.h
 *	DESCRIPTION:	Prototype Header file for make.cpp
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
 * 2002-07-20 Arno Brinkman: Added MAKE_desc_from_list
 */

#ifndef DSQL_MAKE_PROTO_H
#define DSQL_MAKE_PROTO_H

#include "../dsql/sym.h"

namespace Jrd {
	class dsql_ctx;
	class dsql_fld;
	class TypeClause;
	class dsql_msg;
	class dsql_par;
	class DsqlRequest;
	class DsqlCompilerScratch;
	class IntlString;
	class ExprNode;
	class FieldNode;
	class LiteralNode;
	class ValueExprNode;
	class ValueListNode;
	class VariableNode;

// Parameters to MAKE_constant
	enum dsql_constant_type {
		CONSTANT_DOUBLE = 1,	// stored as a string
		CONSTANT_DECIMAL,		// stored as a string
		CONSTANT_NUM128,		// stored as a string
		CONSTANT_DATE,			// stored as a SLONG
		CONSTANT_TIME,			// stored as a ULONG
		CONSTANT_TIMESTAMP,		// stored as a QUAD
		CONSTANT_BOOLEAN,		// stored as a UCHAR
	};

	class DsqlDescMaker
	{
	public:
		static void fromElement(dsc*, const TypeClause*);
		static void fromField(dsc*, const TypeClause*);
		static void fromList(DsqlCompilerScratch*, dsc*,
			ValueListNode*, const char*, bool = false);
		static void fromNode(DsqlCompilerScratch*, dsc*,
			ValueExprNode*, bool = false);
		static void fromNode(DsqlCompilerScratch*, ValueExprNode*);

	private:
		static void composeDesc(dsc* desc,
								USHORT dtype,
								SSHORT scale,
								SSHORT subType,
								FLD_LENGTH length,
								const SSHORT charsetId,
								SSHORT collationId,
								bool nullable);
	};
}


Jrd::LiteralNode* MAKE_const_slong(SLONG);
Jrd::LiteralNode* MAKE_const_sint64(SINT64 value, SCHAR scale);
Jrd::ValueExprNode* MAKE_constant(const char*, Jrd::dsql_constant_type, SSHORT = 0);
Jrd::LiteralNode* MAKE_str_constant(const Jrd::IntlString*, SSHORT);
Jrd::FieldNode* MAKE_field(Jrd::dsql_ctx*, Jrd::dsql_fld*, Jrd::ValueListNode*);
Jrd::FieldNode* MAKE_field_name(const char*);
Jrd::dsql_par* MAKE_parameter(Jrd::dsql_msg*, bool, bool, USHORT, const Jrd::ValueExprNode*);
void MAKE_parameter_names(Jrd::dsql_par*, const Jrd::ValueExprNode*);
Jrd::LiteralNode* MAKE_system_privilege(const char*);

#endif // DSQL_MAKE_PROTO_H
