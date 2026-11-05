/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		evl_proto.h
 *	DESCRIPTION:	Prototype header file for evl.cpp
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

#ifndef JRD_EVL_PROTO_H
#define JRD_EVL_PROTO_H

#include "../jrd/intl_classes.h"
#include "../jrd/req.h"

namespace Firebird::Jrd
{
	class DbKeyRangeNode;
	class InversionNode;
	struct Item;
	class ItemInfo;

	dsc*		EVL_assign_to(thread_db* tdbb, const ValueExprNode*);
	RecordBitmap**	EVL_bitmap(thread_db* tdbb, const InversionNode*, RecordBitmap*);
	void		EVL_dbkey_bounds(thread_db* tdbb, const Array<DbKeyRangeNode*>&,
								jrd_rel*, RecordNumber&, RecordNumber&);
	bool		EVL_field(jrd_rel*, Record*, USHORT, dsc*);
	void		EVL_make_value(thread_db* tdbb, const dsc*, impure_value*, MemoryPool* pool = NULL);
	void		EVL_validate(thread_db*, const Item&, const ItemInfo*, dsc*, bool);

	// Evaluate a value expression.
	inline dsc* EVL_expr(thread_db* tdbb, Request* request, const ValueExprNode* node)
	{
		if (!node)
			BUGCHECK(303);	// msg 303 Invalid expression for evaluation

		SET_TDBB(tdbb);

		JRD_reschedule(tdbb);

		return node->execute(tdbb, request);
	}
}	// namespace Firebird::Jrd

#endif // JRD_EVL_PROTO_H
