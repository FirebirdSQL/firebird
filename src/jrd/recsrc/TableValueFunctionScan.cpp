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
#include "../jrd/jrd.h"
#include "../jrd/mov_proto.h"
#include "../jrd/optimizer/Optimizer.h"
#include "../jrd/req.h"
#include "../jrd/vio_proto.h"

#include "RecordSource.h"
#include <string_view>

using namespace Firebird;
using namespace Jrd;

TableValueFunctionScan::TableValueFunctionScan(CompilerScratch* csb, StreamType stream,
											   const string& alias)
	: RecordStream(csb, stream), m_alias(csb->csb_pool, alias)
{
	const auto tabFunc = csb->csb_rpt[stream].csb_table_value_fun;
	fb_assert(tabFunc);
	m_name = tabFunc->name;

	m_cardinality = DEFAULT_CARDINALITY;
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

	do
	{
		if (impure->m_recordBuffer->fetch(rpb->rpb_number.getValue(), rpb->rpb_record))
		{
			rpb->rpb_number.setValid(true);
			return true;
		}
	} while (nextBuffer(tdbb));

	rpb->rpb_number.setValid(false);
	return false;
}

bool TableValueFunctionScan::refetchRecord(thread_db* /*tdbb*/) const
{
	return true;
}

WriteLockResult TableValueFunctionScan::lockRecord(thread_db* /*tdbb*/) const
{
	status_exception::raise(Arg::Gds(isc_record_lock_not_supp));
}

void TableValueFunctionScan::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (!level)
		plan += "(";

	plan += printName(tdbb, m_alias) + " NATURAL";

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

//--------------------

UnlistFunctionScan::UnlistFunctionScan(CompilerScratch* csb, StreamType stream, const string& alias,
									   ValueListNode* list)
	: TableValueFunctionScan(csb, stream, alias), m_inputList(list)
{
	m_impure = csb->allocImpure<Impure>();
}

void UnlistFunctionScan::close(thread_db* tdbb) const
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

		if (impure->m_blob)
		{
			impure->m_blob->BLB_close(tdbb);
			impure->m_blob = nullptr;
		}

		if (impure->m_separatorStr)
		{
			delete impure->m_separatorStr;
			impure->m_separatorStr = nullptr;
		}

		if (impure->m_resultStr)
		{
			delete impure->m_resultStr;
			impure->m_resultStr = nullptr;
		}
	}
}

void UnlistFunctionScan::internalOpen(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto rpb = &request->req_rpb[m_stream];
	MemoryPool& pool = *tdbb->getDefaultPool();

	rpb->rpb_number.setValue(BOF_NUMBER);

	fb_assert(m_inputList->items.getCount() >= UNLIST_INDEX_LAST);

	auto valueItem = m_inputList->items[UNLIST_INDEX_STRING];
	const auto valueDesc = EVL_expr(tdbb, request, valueItem);
	if (valueDesc == nullptr)
	{
		rpb->rpb_number.setValid(false);
		return;
	}

	auto separatorItem = m_inputList->items[UNLIST_INDEX_SEPARATOR];
	const auto separatorDesc = EVL_expr(tdbb, request, separatorItem);
	if (separatorDesc == nullptr)
	{
		rpb->rpb_number.setValid(false);
		return;
	}

	const auto impure = request->getImpure<Impure>(m_impure);
	impure->irsb_flags |= irsb_open;
	impure->m_recordBuffer = FB_NEW_POOL(pool) RecordBuffer(pool, m_format);
	impure->m_blob = nullptr;
	impure->m_resultStr = nullptr;

	Record* const record = VIO_record(tdbb, rpb, m_format, &pool);

	auto toDesc = m_format->fmt_desc.begin();
	fb_assert(toDesc);
	const auto textType = toDesc->getTextType();

	impure->m_separatorStr = FB_NEW_POOL(pool)
							 string(pool, MOV_make_string2(tdbb, separatorDesc, textType, true));

	if (impure->m_separatorStr->isEmpty())
	{
		const string valueStr(MOV_make_string2(tdbb, valueDesc, textType, false));
		dsc fromDesc;
		fromDesc.makeText(valueStr.size(), textType, (UCHAR*)(IPTR)(valueStr.c_str()));
		assignParameter(tdbb, &fromDesc, toDesc, 0, record);
		impure->m_recordBuffer->store(record);
		return;
	}

	if (valueDesc->isBlob())
	{
		impure->m_blob = blb::open(tdbb, request->req_transaction,
								   reinterpret_cast<bid*>(valueDesc->dsc_address));
		impure->m_resultStr = FB_NEW_POOL(pool) string(pool);
	}
	else
	{
		const std::string_view separatorView(impure->m_separatorStr->data(),
											 impure->m_separatorStr->length());
		string valueStr(MOV_make_string2(tdbb, valueDesc, textType, true));
		std::string_view valueView(valueStr.data(), valueStr.length());
		auto end = std::string_view::npos;
		do
		{
			auto size = end = valueView.find(separatorView);
			if (end == std::string_view::npos)
			{
				if (valueView.empty())
					break;

				size = valueView.length();
			}

			if (size > 0)
			{
				dsc fromDesc;
				fromDesc.makeText(static_cast<USHORT>(size), textType,
					(UCHAR*)(IPTR) valueView.data());
				assignParameter(tdbb, &fromDesc, toDesc, 0, record);
				impure->m_recordBuffer->store(record);
			}

			if (end != std::string_view::npos)
				size += separatorView.length();

			valueView.remove_prefix(size);

		} while (end != std::string_view::npos);
	}
}

void UnlistFunctionScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned /*level*/,
										 bool /*recurse*/) const
{
	planEntry.className = "FunctionScan";

	planEntry.lines.add().text = "Function " +
		printName(tdbb, m_name.toQuotedString(), m_alias) + " Scan";

	printOptInfo(planEntry.lines);

	if (m_alias.hasData())
		planEntry.alias = m_alias;
}

bool UnlistFunctionScan::nextBuffer(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->m_blob)
	{
		auto setStringToRecord = [&](const string& str)
		{
			Record* const record = request->req_rpb[m_stream].rpb_record;

			auto toDesc = m_format->fmt_desc.begin();
			const auto textType = toDesc->getTextType();

			dsc fromDesc;
			fromDesc.makeText(str.length(), textType, (UCHAR*)(IPTR)(str.c_str()));
			assignParameter(tdbb, &fromDesc, toDesc, 0, record);
			impure->m_recordBuffer->store(record);
		};

		string bufferString;

		if (impure->m_resultStr->hasData())
			bufferString = *impure->m_resultStr;

		const auto resultLength = impure->m_resultStr->length();
		bufferString.reserve(MAX_COLUMN_SIZE + resultLength);
		const auto address = bufferString.begin() + resultLength;

		const auto blobLength = impure->m_blob->BLB_get_data(tdbb, reinterpret_cast<UCHAR*>(address),
														 MAX_COLUMN_SIZE, false);
		if (blobLength)
		{
			const std::string_view separatorView(impure->m_separatorStr->data(),
												 impure->m_separatorStr->length());
			std::string_view valueView(bufferString.data(), blobLength + resultLength);
			auto end = std::string_view::npos;
			impure->m_resultStr->erase();

			do
			{
				const auto size = end = valueView.find(separatorView);
				if (end == std::string_view::npos)
				{
					if (!valueView.empty())
					{
						impure->m_resultStr->append(valueView.data(),
							static_cast<string::size_type>(valueView.length()));
					}

					break;
				}

				if (size > 0)
				{
					impure->m_resultStr->append(valueView.data(),
						static_cast<string::size_type>(size));
				}

				valueView.remove_prefix(size + separatorView.length());

				if (impure->m_resultStr->hasData())
				{
					setStringToRecord(*impure->m_resultStr);
					impure->m_resultStr->erase();
				}
			} while (end != std::string_view::npos);

			return true;
		}

		if (impure->m_blob->blb_flags & BLB_eof)
		{
			if (impure->m_resultStr->hasData())
			{
				setStringToRecord(*impure->m_resultStr);
				impure->m_resultStr->erase();
				return true;
			}
		}
	}

	return false;
}

//--------------------

GenSeriesFunctionScan::GenSeriesFunctionScan(CompilerScratch* csb, StreamType stream, const string& alias,
									   ValueListNode* list)
	: TableValueFunctionScan(csb, stream, alias), m_inputList(list)
{
	m_impure = csb->allocImpure<Impure>();
}

void GenSeriesFunctionScan::close(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();

	invalidateRecords(request);

	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
		impure->irsb_flags &= ~irsb_open;
}


void GenSeriesFunctionScan::internalOpen(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto rpb = &request->req_rpb[m_stream];
	MemoryPool& pool = *tdbb->getDefaultPool();

	rpb->rpb_number.setValue(BOF_NUMBER);

	fb_assert(m_inputList->items.getCount() >= GEN_SERIES_INDEX_LAST);

	auto startItem = m_inputList->items[GEN_SERIES_INDEX_START];
	const auto startDesc = EVL_expr(tdbb, request, startItem);
	if (startDesc == nullptr)
		return;

	auto finishItem = m_inputList->items[GEN_SERIES_INDEX_FINISH];
	const auto finishDesc = EVL_expr(tdbb, request, finishItem);
	if (finishDesc == nullptr)
		return;

	auto stepItem = m_inputList->items[GEN_SERIES_INDEX_STEP];
	const auto stepDesc = EVL_expr(tdbb, request, stepItem);
	if (stepDesc == nullptr)
		return;

	const auto impure = request->getImpure<Impure>(m_impure);
	impure->m_recordBuffer = nullptr;

	// common scale
	impure->m_scale = MIN(MIN(startDesc->dsc_scale, finishDesc->dsc_scale), stepDesc->dsc_scale);
	// common type
	impure->m_dtype = MAX(MAX(startDesc->dsc_dtype, finishDesc->dsc_dtype), stepDesc->dsc_dtype);

	if (impure->m_dtype != dtype_int128)
	{
		const auto start = MOV_get_int64(tdbb, startDesc, impure->m_scale);
		const auto finish = MOV_get_int64(tdbb, finishDesc, impure->m_scale);
		const auto step = MOV_get_int64(tdbb, stepDesc, impure->m_scale);

		if (step == 0)
			status_exception::raise(Arg::Gds(isc_genseq_stepmustbe_nonzero) << Arg::Str(m_name));

		// validate parameter value
		if (((step > 0) && (start > finish)) ||
			((step < 0) && (start < finish)))
		{
			return;
		}

		impure->m_start.vlu_int64 = start;
		impure->m_finish.vlu_int64 = finish;
		impure->m_step.vlu_int64 = step;
		impure->m_result.vlu_int64 = start;
	}
	else
	{
		const auto start = MOV_get_int128(tdbb, startDesc, impure->m_scale);
		const auto finish = MOV_get_int128(tdbb, finishDesc, impure->m_scale);
		const auto step = MOV_get_int128(tdbb, stepDesc, impure->m_scale);

		if (step.sign() == 0)
			status_exception::raise(Arg::Gds(isc_genseq_stepmustbe_nonzero) << Arg::Str(m_name));

		// validate parameter value
		if (((step.sign() > 0) && (start.compare(finish) > 0)) ||
			((step.sign() < 0) && (start.compare(finish) < 0)))
		{
			return;
		}

		impure->m_start.vlu_int128 = start;
		impure->m_finish.vlu_int128 = finish;
		impure->m_step.vlu_int128 = step;
		impure->m_result.vlu_int128 = start;
	}

	impure->irsb_flags |= irsb_open;

	VIO_record(tdbb, rpb, m_format, &pool);
}

void GenSeriesFunctionScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned /*level*/,
										 bool /*recurse*/) const
{
	planEntry.className = "FunctionScan";

	planEntry.lines.add().text = "Function " +
		printName(tdbb, m_name.toQuotedString(), m_alias) + " Scan";

	printOptInfo(planEntry.lines);

	if (m_alias.hasData())
		planEntry.alias = m_alias;
}

bool GenSeriesFunctionScan::internalGetRecord(thread_db* tdbb) const
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

	if (nextBuffer(tdbb))
	{
		rpb->rpb_number.setValid(true);
		return true;
	}

	rpb->rpb_number.setValid(false);
	return false;
}

bool GenSeriesFunctionScan::nextBuffer(thread_db* tdbb) const
{
	const auto request = tdbb->getRequest();
	const auto impure = request->getImpure<Impure>(m_impure);

	if (impure->m_dtype != dtype_int128)
	{
		auto result = impure->m_result.vlu_int64;
		const auto finish = impure->m_finish.vlu_int64;
		const auto step = impure->m_step.vlu_int64;

		if (((step > 0) && (result <= finish)) ||
			((step < 0) && (result >= finish)))
		{
			Record* const record = request->req_rpb[m_stream].rpb_record;

			auto toDesc = m_format->fmt_desc.begin();

			dsc fromDesc;
			fromDesc.makeInt64(impure->m_scale, &result);
			assignParameter(tdbb, &fromDesc, toDesc, 0, record);

			// Prevents overflows for SINT64 type at boundary values
			bool reachedBoundary = false;
			if (step > 0)
			{
				if (result > MAX_SINT64 - step)
					reachedBoundary = true;
				else
					reachedBoundary = (result + step > finish);
			}
			else if (step < 0)
			{
				if (result < MIN_SINT64 - step)
					reachedBoundary = true;
				else
					reachedBoundary = (result + step < finish);
			}

			if ((result == finish) || reachedBoundary)
				impure->m_step.vlu_int64 = 0;
			else
			    result += step;

			impure->m_result.vlu_int64 = result;

			return true;
		}
	}
	else
	{
		auto result = impure->m_result.vlu_int128;
		const auto finish = impure->m_finish.vlu_int128;
		const auto step = impure->m_step.vlu_int128;

		if (((step.sign() > 0) && (result.compare(finish) <= 0)) ||
			((step.sign() < 0) && (result.compare(finish) >= 0)))
		{
			Record* const record = request->req_rpb[m_stream].rpb_record;

			auto toDesc = m_format->fmt_desc.begin();

			dsc fromDesc;
			fromDesc.makeInt128(impure->m_scale, &result);
			assignParameter(tdbb, &fromDesc, toDesc, 0, record);

			// Prevents overflows for INT128 type at boundary values
			bool reachedBoundary = false;
			if (step.sign() > 0)
			{
				if (result.compare(MAX_Int128.sub(step)) > 0)
					reachedBoundary = true;
				else
					reachedBoundary = (result.add(step).compare(finish) > 0);
			}
			else if (step.sign() < 0)
			{
				if (result.compare(MIN_Int128.sub(step)) < 0)
					reachedBoundary = true;
				else
					reachedBoundary = (result.add(step).compare(finish) < 0);
			}

			if (reachedBoundary || (result.compare(finish) == 0))
				impure->m_step.vlu_int128.set(0, 0);
			else
				result = result.add(step);

			impure->m_result.vlu_int128 = result;

			return true;
		}
	}

	return false;
}
