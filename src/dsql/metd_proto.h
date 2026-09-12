/*
 *	PROGRAM:	 Dynamic SQL runtime support
 *	MODULE:		 metd_proto.h
 *	DESCRIPTION: Prototype Header file for metd.epp
 *               This is a DSQL private header file. It is not included
 *               by anything but DSQL itself.
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

#ifndef DSQL_METD_PROTO_H
#define DSQL_METD_PROTO_H

#include "../common/classes/GenericMap.h"
#include "../jrd/MetaName.h"
#include "../common/classes/fb_pair.h"

// forward declarations
namespace Firebird::Jrd {
	typedef GenericMap<MetaNamePair> MetaNamePairMap;

	class DsqlRequest;
	class DsqlCompilerScratch;
	class jrd_tra;
	class dsql_intlsym;
	class dsql_fld;
	class dsql_udf;
	class dsql_prc;
	class dsql_rel;
	class FieldNode;
	class TypeClause;

void METD_drop_charset(jrd_tra*, const QualifiedName&);
void METD_drop_collation(jrd_tra*, const QualifiedName&);
void METD_drop_relation(jrd_tra*, const QualifiedName&);

dsql_intlsym* METD_get_charset(jrd_tra*, const QualifiedName& name);
USHORT METD_get_charset_bpc(jrd_tra*, CSetId);
QualifiedName METD_get_charset_name(jrd_tra*, CSetId);
dsql_intlsym* METD_get_collation(jrd_tra*, const QualifiedName&, CSetId charset_id);
QualifiedName METD_get_database_charset(jrd_tra*);
QualifiedName METD_get_schema_charset(jrd_tra*, const MetaName&);
bool METD_get_domain(jrd_tra*, TypeClause*, const QualifiedName& name);
dsql_udf* METD_get_function(jrd_tra*, DsqlCompilerScratch*, const QualifiedName&);
void METD_get_primary_key(jrd_tra*, const QualifiedName&, Array<NestConst<FieldNode>>&);
dsql_prc* METD_get_procedure(jrd_tra*, DsqlCompilerScratch*, const QualifiedName&);
dsql_rel* METD_get_relation(jrd_tra*, DsqlCompilerScratch*, const QualifiedName&);
bool METD_get_type(jrd_tra*, const MetaName&, const char*, SSHORT*);
dsql_rel* METD_get_view_base(jrd_tra*, DsqlCompilerScratch*, const QualifiedName& viewName, MetaNamePairMap& fields);
bool METD_get_view_relation(jrd_tra*, DsqlCompilerScratch*, const QualifiedName& view_name,
	const QualifiedName& relation_or_alias, dsql_rel*& relation, dsql_prc*& procedure);

} // namespace Firebird::Jrd


#endif // DSQL_METD_PROTO_H
