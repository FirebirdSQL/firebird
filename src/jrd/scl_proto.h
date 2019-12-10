/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		scl_proto.h
 *	DESCRIPTION:	Prototype header file for scl.epp
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

#ifndef JRD_SCL_PROTO_H
#define JRD_SCL_PROTO_H

#include "../jrd/scl.h"
#include "../common/classes/array.h"

//namespace Jrd {
//	class SecurityClass;
//}

struct dsc;

void SCL_check_access(Jrd::thread_db*, const Jrd::SecurityClass*,
					  SLONG, const Firebird::MetaName&,
					  Jrd::SecurityClass::flags_t, SLONG type, bool recursive, const Firebird::MetaName&,
					  const Firebird::MetaName& = "");
void SCL_check_create_access(Jrd::thread_db*, int type);
void SCL_check_charset(Jrd::thread_db* tdbb, const Firebird::MetaName&, Jrd::SecurityClass::flags_t);
void SCL_check_collation(Jrd::thread_db* tdbb, const Firebird::MetaName&, Jrd::SecurityClass::flags_t);
void SCL_check_database(Jrd::thread_db* tdbb, Jrd::SecurityClass::flags_t mask);
void SCL_check_domain(Jrd::thread_db* tdbb, const Firebird::MetaName&, Jrd::SecurityClass::flags_t);
bool SCL_check_exception(Jrd::thread_db* tdbb, const Firebird::MetaName&, Jrd::SecurityClass::flags_t);
bool SCL_check_generator(Jrd::thread_db* tdbb, const Firebird::MetaName&, Jrd::SecurityClass::flags_t);
void SCL_check_index(Jrd::thread_db*, const Firebird::MetaName&, UCHAR, Jrd::SecurityClass::flags_t);
bool SCL_check_package(Jrd::thread_db* tdbb, const dsc*, Jrd::SecurityClass::flags_t);
bool SCL_check_procedure(Jrd::thread_db* tdbb, const dsc*, Jrd::SecurityClass::flags_t);
bool SCL_check_function(Jrd::thread_db* tdbb, const dsc*, Jrd::SecurityClass::flags_t);
void SCL_check_filter(Jrd::thread_db* tdbb, const Firebird::MetaName &name, Jrd::SecurityClass::flags_t);
void SCL_check_relation(Jrd::thread_db* tdbb, const dsc*, Jrd::SecurityClass::flags_t, bool protectSys = true);
bool SCL_check_view(Jrd::thread_db* tdbb, const dsc*, Jrd::SecurityClass::flags_t);
void SCL_check_role(Jrd::thread_db* tdbb, const Firebird::MetaName&, Jrd::SecurityClass::flags_t);
Jrd::SecurityClass* SCL_get_class(Jrd::thread_db*, const TEXT*);
Jrd::SecurityClass::flags_t SCL_get_mask(Jrd::thread_db* tdbb, const TEXT*, const TEXT*);
void SCL_clear_classes(Jrd::thread_db*, const TEXT*);
void SCL_release_all(Jrd::SecurityClassList*&);
bool SCL_role_granted(Jrd::thread_db* tdbb, const Jrd::UserId& usr, const TEXT* sql_role);
Jrd::SecurityClass::flags_t SCL_get_object_mask(const int object_type);
ULONG SCL_get_number(const UCHAR*);
USHORT SCL_convert_privilege(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction, const Firebird::string& priv);

namespace Jrd {
typedef Firebird::Array<UCHAR> Acl;
}
bool SCL_move_priv(Jrd::SecurityClass::flags_t, Jrd::Acl&);


#endif // JRD_SCL_PROTO_H
