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

namespace Jrd
{
	class DbKeyRangeNode;
	class InversionNode;
	struct Item;
	class ItemInfo;
}

dsc*		EVL_assign_to(Jrd::thread_db* tdbb, const Jrd::ValueExprNode*);
Jrd::RecordBitmap**	EVL_bitmap(Jrd::thread_db* tdbb, const Jrd::InversionNode*, Jrd::RecordBitmap*);
void		EVL_dbkey_bounds(Jrd::thread_db* tdbb, const Firebird::Array<Jrd::DbKeyRangeNode*>&,
							 Jrd::jrd_rel*, RecordNumber&, RecordNumber&);
void		EVL_make_value(Jrd::thread_db* tdbb, const dsc*, Jrd::impure_value*, MemoryPool* pool = NULL);
void		EVL_validate(Jrd::thread_db*, const Jrd::Item&, const Jrd::ItemInfo*, dsc*, bool);

namespace Jrd
{
	// Evaluate a value expression.
	inline dsc* EVL_expr(thread_db* tdbb, Request* request, const ValueExprNode* node)
	{
		if (!node)
			BUGCHECK(303);	// msg 303 Invalid expression for evaluation

		SET_TDBB(tdbb);

		JRD_reschedule(tdbb);

		return node->execute(tdbb, request);
	}

	inline dsc* EVL_put_desc(thread_db* tdbb, const dsc* desc, dsc* impure_desc, MemoryPool* pool = nullptr)
	{
		// Free old subdescriptors and copy
		// new ones inside appropriate pool
		//
		// Explicite impure dsc overload

		delete impure_desc->dsc_sub_first;

		if (!pool)
			pool = tdbb->getDefaultPool();

		impure_desc->pool = pool;
		*impure_desc = *desc;

		return impure_desc;
	}

	inline dsc* EVL_put_desc(thread_db* tdbb, const dsc* desc, impure_value* value, MemoryPool* pool = nullptr)
	{
		return EVL_put_desc(tdbb, desc, &value->vlu_desc, pool);
	}

	inline bool EVL_field(thread_db* tdbb, jrd_rel* relation, Record* record, USHORT id, dsc* desc)
	{
		if (!record)
		{
			// ASF: Usage of ERR_warning with Arg::Gds (instead of Arg::Warning) is correct here.
			// Maybe not all code paths are prepared for throwing an exception here,
			// but it will leave the engine as an error (when testing for req_warning).
			ERR_warning(Firebird::Arg::Gds(isc_no_cur_rec));
			return false;
		}

		const auto format = record->getFormat();
		fb_assert(format);

		if (id >= format->fmt_count || format->fmt_desc[id].isUnknown())
		{
			// Map a non-existent field to a default value, if available.
			// This enables automatic format upgrade for data rows.
			// Reference: Bug 10424, 10116

			if (relation)
			{
				thread_db* tdbb = JRD_get_thread_data();

				const Format* const currentFormat = relation->currentFormat(tdbb);

				while (id >= format->fmt_defaults.getCount() ||
					format->fmt_defaults[id].vlu_desc.isUnknown())
				{
					if (format->fmt_version >= currentFormat->fmt_version)
					{
						format = NULL;
						break;
					}

					format = relation->getPermanent()->getFormat(tdbb, format->fmt_version + 1);
					fb_assert(format);
				}

				if (format)
				{
					*desc = format->fmt_defaults[id].vlu_desc;

					if (record->isNull())
						desc->dsc_flags |= DSC_null;

					return !(desc->dsc_flags & DSC_null);
				}
			}

			desc->makeText(1, ttype_ascii, (UCHAR*) " ");
			return false;
		}

		// If the offset of the field is 0, the field can't possible exist

		if (!format->fmt_desc[id].dsc_address)
			return false;

		EVL_put_desc(tdbb, &format->fmt_desc[id], desc);

		desc->setAddress(record->getData() + (IPTR) desc->dsc_address);

		if (record->isNull(id))
		{
			desc->dsc_flags |= DSC_null;
			return false;
		}

		desc->dsc_flags &= ~DSC_null;
		return true;
	}

	inline bool EVL_field(thread_db* tdbb, Record* record, USHORT id, dsc* desc)
	{
		return EVL_field(tdbb, nullptr, record, id, desc);
	}
}

#endif // JRD_EVL_PROTO_H
