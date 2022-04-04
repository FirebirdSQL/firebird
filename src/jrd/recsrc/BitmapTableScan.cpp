/*
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

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/rlck_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

// ---------------------------------------------
// Data access: Bitmap (DBKEY) driven table scan
// ---------------------------------------------

BitmapTableScan::BitmapTableScan(CompilerScratch* csb, const string& alias,
								 StreamType stream, jrd_rel* relation,
								 InversionNode* inversion)
	: RecordStream(csb, stream),
	  m_alias(csb->csb_pool, alias), m_relation(relation), m_inversion(inversion)
{
	fb_assert(m_inversion);

	m_impure = csb->allocImpure<Impure>();
}

void BitmapTableScan::open(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;
	impure->irsb_bitmap = EVL_bitmap(tdbb, m_inversion, NULL);

	record_param* const rpb = &request->req_rpb[m_stream];
	RLCK_reserve_relation(tdbb, request->req_transaction, m_relation, false);

	rpb->rpb_number.setValue(BOF_NUMBER);
}

void BitmapTableScan::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		if (m_recursive && impure->irsb_bitmap)
		{
			delete *impure->irsb_bitmap;
			*impure->irsb_bitmap = NULL;
		}
	}
}

bool BitmapTableScan::getRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	RecordBitmap** pbitmap = impure->irsb_bitmap;
	RecordBitmap* bitmap;

	if (!pbitmap || !(bitmap = *pbitmap))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	if (rpb->rpb_number.isBof() ? bitmap->getFirst() : bitmap->getNext())
	{
		do
		{
			rpb->rpb_number.setValue(bitmap->current());

			if (VIO_get(tdbb, rpb, request->req_transaction, request->req_pool))
			{
				rpb->rpb_number.setValid(true);
				return true;
			}
		} while (bitmap->getNext());
	}

	rpb->rpb_number.setValid(false);
	return false;
}

void BitmapTableScan::print(thread_db* tdbb, string& plan,
							bool detailed, unsigned level) const
{
	if (detailed)
	{
		plan += printIndent(++level) + "Table " +
			printName(tdbb, m_relation->rel_name.c_str(), m_alias) + " Access By ID";

		printInversion(tdbb, m_inversion, plan, true, level);
	}
	else
	{
		if (!level)
			plan += "(";

		plan += printName(tdbb, m_alias, false) + " INDEX (";
		string indices;
		printInversion(tdbb, m_inversion, indices, false, level);
		plan += indices + ")";

		if (!level)
			plan += ")";
	}
}
