/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Chudaykin Alexey
 *   <alexey.chudaykin (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2025 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s):
 */
#include "firebird.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/align.h"
#include "../jrd/jrd.h"
#include "../jrd/mov_proto.h"
#include "../jrd/optimizer/Optimizer.h"
#include "../jrd/req.h"
#include "../jrd/vio_proto.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;

TableValueFunctionScan::TableValueFunctionScan(CompilerScratch* csb, StreamType stream,
											   const Firebird::string& alias)
	: RecordStream(csb, stream), m_alias(csb->csb_pool, alias)
{
	m_impure = csb->allocImpure<Impure>();
	m_cardinality = DEFAULT_CARDINALITY;
}

void TableValueFunctionScan::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	invalidateRecords(request);

	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		if (impure->m_recordBuffer)
		{
			delete impure->m_recordBuffer;
			impure->m_recordBuffer = nullptr;
		}
	}
}

bool TableValueFunctionScan::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);
	const auto rpb = &request->req_rpb[m_stream];

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	rpb->rpb_number.increment();

	if (!impure->m_recordBuffer->fetch(rpb->rpb_number.getValue(), rpb->rpb_record))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}
	return true;
}

bool TableValueFunctionScan::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

WriteLockResult TableValueFunctionScan::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void TableValueFunctionScan::getLegacyPlan(thread_db* tdbb, Firebird::string& plan,
										   unsigned level) const
{
	if (!level)
		plan += "(";

	plan += printName(tdbb, m_alias, false) + " NATURAL";

	if (!level)
		plan += ")";
}

void TableValueFunctionScan::assignParameter(thread_db* tdbb, dsc* fromDesc, const dsc* toDesc,
											 SSHORT toId, Record* record) const
{
	dsc toDescValue = *toDesc;
	toDescValue.dsc_address = record->getData() + (IPTR)toDesc->dsc_address;

	if (fromDesc->isNull())
	{
		record->setNull(toId);
		return;
	}
	else
		record->clearNull(toId);

	if (!DSC_EQUIV(fromDesc, &toDescValue, false))
	{
		MOV_move(tdbb, fromDesc, &toDescValue);
		return;
	}

	memcpy(toDescValue.dsc_address, fromDesc->dsc_address, fromDesc->dsc_length);
}

UnlistFunctionScan::UnlistFunctionScan(CompilerScratch* csb, StreamType stream, const string& alias,
									   ValueListNode* list)
	: TableValueFunctionScan(csb, stream, alias), m_inputList(list)
{
}

void UnlistFunctionScan::internalOpen(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_open;

	const auto rpb = &request->req_rpb[m_stream];

	MemoryPool& pool = *tdbb->getDefaultPool();
	impure->m_recordBuffer = FB_NEW_POOL(pool) RecordBuffer(pool, m_format);

	Record* const record = VIO_record(tdbb, rpb, m_format, &pool);

	rpb->rpb_number.setValue(BOF_NUMBER);

	fb_assert(m_inputList->items.getCount() >= UNLIST_INDEX_LAST);

	auto valueItem = m_inputList->items[UNLIST_INDEX_STRING];
	const auto valueDesc = EVL_expr(tdbb, request, valueItem);
	if (valueDesc == nullptr)
	{
		rpb->rpb_number.setValid(true);
		return;
	}

	auto separatorItem = m_inputList->items[UNLIST_INDEX_SEPARATOR];
	const auto separatorDesc = EVL_expr(tdbb, request, separatorItem);
	if (separatorDesc == nullptr)
	{
		rpb->rpb_number.setValid(false);
		return;
	}

	auto toDesc = m_format->fmt_desc.begin();
	fb_assert(toDesc);

	const auto textType = toDesc->getTextType();

	auto setStringToRecord = [&] (string str, USHORT length = 0)
	{
		if (length == 0)
			length = str.length();
		dsc fromDesc;
		fromDesc.makeText(length, textType, (UCHAR*)str.c_str());
		assignParameter(tdbb, &fromDesc, toDesc, 0, record);
		impure->m_recordBuffer->store(record);
	};

	string separatorStr(MOV_make_string2(tdbb, separatorDesc, textType, true));

	if (separatorStr.isEmpty())
	{
		string valueStr(MOV_make_string2(tdbb, valueDesc, textType, false));
		setStringToRecord(valueStr);
		return;
	}

	if (valueDesc->isBlob())
	{
		blb* blob = blb::open(tdbb, request->req_transaction,
						reinterpret_cast<bid*>(valueDesc->dsc_address));

		string resultStr;

		while (!(blob->blb_flags & BLB_eof))
		{
			UCHAR buffer[BUFFER_LARGE];
			const auto length = blob->BLB_get_data(tdbb, buffer, sizeof(buffer), false);
			if (length == 0)
				continue;

			string valueStr(buffer, length);

			auto end = AbstractString::npos;
			do
			{
				auto size = end = valueStr.find(separatorStr);
				if(end == AbstractString::npos)
				{
					if (valueStr.hasData())
						resultStr.append(valueStr);

					break;
				}

				if (size > 0)
					resultStr.append(valueStr.substr(0, size));

				valueStr.erase(valueStr.begin(), valueStr.begin() + end + separatorStr.length());

				if (resultStr.hasData())
				{
					setStringToRecord(resultStr);
					resultStr.erase();
				}
			} while (end != AbstractString::npos);
		}

		// Tail
		if (resultStr.hasData())
			setStringToRecord(resultStr);
	}
	else
	{
		string valueStr(MOV_make_string2(tdbb, valueDesc, textType, true));

		auto end = AbstractString::npos;
		do
		{
			auto size = end = valueStr.find(separatorStr);
			if (end == AbstractString::npos)
			{
				if (valueStr.hasData())
					size = valueStr.size();
				else
					break;
			}

			if (size > 0)
				setStringToRecord(valueStr, size);

			valueStr.erase(valueStr.begin(), valueStr.begin() + end + separatorStr.length());
		} while (end != AbstractString::npos);
	}
}

void UnlistFunctionScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned /*level*/,
										 bool /*recurse*/) const
{
	planEntry.className = "FunctionScan";

	planEntry.lines.add().text = "Functions " + printName(tdbb, "Unlist", m_alias) + " Scan";
	printOptInfo(planEntry.lines);

	if (m_alias.hasData())
		planEntry.alias = m_alias;
}
