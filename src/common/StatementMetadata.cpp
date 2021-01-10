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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *		Alex Peshkoff
 *
 */

#include "firebird.h"
#include "../common/StatementMetadata.h"
#include "memory_routines.h"
#include "../common/StatusHolder.h"
#include "firebird/impl/inf_pub.h"
#include "../yvalve/gds_proto.h"
#include "../common/utils_proto.h"
#include "firebird/impl/sqlda_pub.h"

namespace Firebird {


static const UCHAR DESCRIBE_VARS[] =
{
	isc_info_sql_describe_vars,
	isc_info_sql_sqlda_seq,
	isc_info_sql_type,
	isc_info_sql_sub_type,
	isc_info_sql_scale,
	isc_info_sql_length,
	isc_info_sql_field,
	isc_info_sql_relation,
	isc_info_sql_owner,
	isc_info_sql_alias,
	isc_info_sql_describe_end
};

static const unsigned INFO_BUFFER_SIZE = MemoryPool::MAX_MEDIUM_BLOCK_SIZE;


static USHORT getLen(const UCHAR** ptr, const UCHAR* bufferEnd);
static int getNumericInfo(const UCHAR** ptr, const UCHAR* bufferEnd);
static void getStringInfo(const UCHAR** ptr, const UCHAR* bufferEnd, string* str);


// Build a list of info codes based on a prepare flags bitmask.
// Return "estimated" necessary size for the result buffer.
unsigned StatementMetadata::buildInfoItems(Array<UCHAR>& items, unsigned flags)
{
	items.clear();

	if (flags & IStatement::PREPARE_PREFETCH_TYPE)
		items.add(isc_info_sql_stmt_type);

	if (flags & IStatement::PREPARE_PREFETCH_FLAGS)
		items.add(isc_info_sql_stmt_flags);

	if (flags & IStatement::PREPARE_PREFETCH_INPUT_PARAMETERS)
	{
		items.add(isc_info_sql_bind);
		items.push(DESCRIBE_VARS, sizeof(DESCRIBE_VARS));
	}

	if (flags & IStatement::PREPARE_PREFETCH_OUTPUT_PARAMETERS)
	{
		items.add(isc_info_sql_select);
		items.push(DESCRIBE_VARS, sizeof(DESCRIBE_VARS));
	}

	if (flags & IStatement::PREPARE_PREFETCH_LEGACY_PLAN)
		items.add(isc_info_sql_get_plan);

	if (flags & IStatement::PREPARE_PREFETCH_DETAILED_PLAN)
		items.add(isc_info_sql_explain_plan);

	return INFO_BUFFER_SIZE;
}

// Build a prepare flags bitmask based on a list of info codes.
unsigned StatementMetadata::buildInfoFlags(unsigned itemsLength, const UCHAR* items)
{
	unsigned flags = 0;
	const UCHAR* end = items + itemsLength;
	UCHAR c;

	while (items < end && (c = *items++) != isc_info_end)
	{
		switch (c)
		{
			case isc_info_sql_stmt_type:
				flags |= IStatement::PREPARE_PREFETCH_TYPE;
				break;

			case isc_info_sql_stmt_flags:
				flags |= IStatement::PREPARE_PREFETCH_FLAGS;
				break;

			case isc_info_sql_get_plan:
				flags |= IStatement::PREPARE_PREFETCH_LEGACY_PLAN;
				break;

			case isc_info_sql_explain_plan:
				flags |= IStatement::PREPARE_PREFETCH_DETAILED_PLAN;
				break;

			case isc_info_sql_select:
				flags |= IStatement::PREPARE_PREFETCH_OUTPUT_PARAMETERS;
				break;

			case isc_info_sql_bind:
				flags |= IStatement::PREPARE_PREFETCH_INPUT_PARAMETERS;
				break;
		}
	}

	return flags;
}

// Get statement type.
unsigned StatementMetadata::getType()
{
	if (!type.specified)
	{
		UCHAR info[] = {isc_info_sql_stmt_type};
		UCHAR result[16];

		getAndParse(sizeof(info), info, sizeof(result), result);

		fb_assert(type.specified);
	}

	return type.value;
}

unsigned StatementMetadata::getFlags()
{
	if (!flags.specified)
	{
		UCHAR info[] = {isc_info_sql_stmt_flags};
		UCHAR result[16];

		getAndParse(sizeof(info), info, sizeof(result), result);

		fb_assert(flags.specified);
	}

	return flags.value;
}

// Get statement plan.
const char* StatementMetadata::getPlan(bool detailed)
{
	string* plan = detailed ? &detailedPlan : &legacyPlan;

	if (plan->isEmpty())
	{
		UCHAR info[] = {UCHAR(detailed ? isc_info_sql_explain_plan : isc_info_sql_get_plan)};
		UCHAR result[INFO_BUFFER_SIZE];

		getAndParse(sizeof(info), info, sizeof(result), result);
	}

	return plan->nullStr();
}

// Get statement input parameters.
IMessageMetadata* StatementMetadata::getInputMetadata()
{
	if (!inputParameters->fetched)
		fetchParameters(isc_info_sql_bind, inputParameters);

	inputParameters->addRef();
	return inputParameters;
}

// Get statement output parameters.
IMessageMetadata* StatementMetadata::getOutputMetadata()
{
	if (!outputParameters->fetched)
		fetchParameters(isc_info_sql_select, outputParameters);

	outputParameters->addRef();
	return outputParameters;
}

// Get number of records affected by the statement execution.
ISC_UINT64 StatementMetadata::getAffectedRecords()
{
	UCHAR info[] = {isc_info_sql_records};
	UCHAR result[33];

	getAndParse(sizeof(info), info, sizeof(result), result);

	ISC_UINT64 count = 0;

	if (result[0] == isc_info_sql_records)
	{
		const UCHAR* p = result + 3;

		while (*p != isc_info_end)
		{
			UCHAR counter = *p++;
			const SSHORT len = gds__vax_integer(p, 2);
			p += 2;
			if (counter != isc_info_req_select_count)
				count += gds__vax_integer(p, len);
			p += len;
		}
	}

	return count;
}

// Reset the object to its initial state.
void StatementMetadata::clear()
{
	type.specified = false;
	legacyPlan = detailedPlan = "";
	inputParameters->items.clear();
	outputParameters->items.clear();
	inputParameters->fetched = outputParameters->fetched = false;
}

// Parse an info response buffer.
void StatementMetadata::parse(unsigned bufferLength, const UCHAR* buffer)
{
	const UCHAR* bufferEnd = buffer + bufferLength;
	Parameters* parameters = NULL;
	bool finish = false;
	UCHAR c;

	while (!finish && buffer < bufferEnd && (c = *buffer++) != isc_info_end)
	{
		switch (c)
		{
			case isc_info_sql_stmt_type:
				type = getNumericInfo(&buffer, bufferEnd);
				break;

			case isc_info_sql_stmt_flags:
				flags = getNumericInfo(&buffer, bufferEnd);
				break;

			case isc_info_sql_get_plan:
			case isc_info_sql_explain_plan:
			{
				string* plan = (c == isc_info_sql_explain_plan ? &detailedPlan : &legacyPlan);
				getStringInfo(&buffer, bufferEnd, plan);
				break;
			}

			case isc_info_sql_select:
				parameters = outputParameters;
				break;

			case isc_info_sql_bind:
				parameters = inputParameters;
				break;

			case isc_info_sql_num_variables:
			case isc_info_sql_describe_vars:
			{
				if (!parameters)
				{
					finish = true;
					break;
				}

				getNumericInfo(&buffer, bufferEnd);	// skip the message index

				if (c == isc_info_sql_num_variables)
					continue;

				Parameters::Item temp(*getDefaultMemoryPool());
				Parameters::Item* param = &temp;
				bool finishDescribe = false;

				// Loop over the variables being described.
				while (!finishDescribe)
				{
					fb_assert(buffer < bufferEnd);

					if (buffer >= bufferEnd)
						break;

					switch ((c = *buffer++))
					{
						case isc_info_sql_describe_end:
							param->finished = true;
							break;

						case isc_info_sql_sqlda_seq:
							if (!parameters->fetched)
							{
								unsigned num = getNumericInfo(&buffer, bufferEnd);

								while (parameters->items.getCount() < num)
									parameters->items.add();

								param = &parameters->items[num - 1];
							}
							break;

						case isc_info_sql_type:
							param->type = getNumericInfo(&buffer, bufferEnd);
							param->nullable = (param->type & 1) != 0;
							param->type &= ~1;
							break;

						case isc_info_sql_sub_type:
							param->subType = getNumericInfo(&buffer, bufferEnd);
							break;

						case isc_info_sql_length:
							param->length = getNumericInfo(&buffer, bufferEnd);
							break;

						case isc_info_sql_scale:
							param->scale = getNumericInfo(&buffer, bufferEnd);
							break;

						case isc_info_sql_field:
							getStringInfo(&buffer, bufferEnd, &param->field);
							break;

						case isc_info_sql_relation:
							getStringInfo(&buffer, bufferEnd, &param->relation);
							break;

						case isc_info_sql_owner:
							getStringInfo(&buffer, bufferEnd, &param->owner);
							break;

						case isc_info_sql_alias:
							getStringInfo(&buffer, bufferEnd, &param->alias);
							break;

						case isc_info_truncated:
							--buffer;
							finishDescribe = true;
							break;

						default:
							--buffer;
							finishDescribe = true;

							if (parameters->fetched)
								break;

							parameters->fetched = true;

							for (unsigned n = 0; n < parameters->items.getCount(); ++n)
							{
								Parameters::Item* param = &parameters->items[n];

								if (!param->finished)
								{
									parameters->fetched = false;
									break;
								}
							}

							if (parameters->fetched && parameters->makeOffsets() != ~0u)
								parameters->fetched = false;

							if (parameters->fetched)
							{
								for (unsigned n = 0; n < parameters->items.getCount(); ++n)
								{
									Parameters::Item* param = &parameters->items[n];
									switch (param->type)
									{
										case SQL_VARYING:
										case SQL_TEXT:
											param->charSet = param->subType;
											param->subType = 0;
											break;
										case SQL_BLOB:
											param->charSet = param->scale;
											param->scale = 0;
											break;
									}
								}
							}

							break;
					}
				}

				break;
			}

			default:
				finish = true;
				break;
		}
	}
}

// Get a info buffer and parse it.
void StatementMetadata::getAndParse(unsigned itemsLength, const UCHAR* items,
	unsigned bufferLength, UCHAR* buffer)
{
	LocalStatus ls;
	CheckStatusWrapper status(&ls);
	statement->getInfo(&status, itemsLength, items, bufferLength, buffer);
	ls.check();

	parse(bufferLength, buffer);
}

// Fill an output buffer from the cached data. Return true if succeeded.
bool StatementMetadata::fillFromCache(unsigned itemsLength, const UCHAR* items,
	unsigned bufferLength, UCHAR* buffer)
{
	//// TODO: Respond more things locally. isc_dsql_prepare_m will need.

	if (((itemsLength == 1 && items[0] == isc_info_sql_stmt_type) ||
			(itemsLength == 2 && items[0] == isc_info_sql_stmt_type &&
				(items[1] == isc_info_end || items[1] == 0))) &&
		type.specified)
	{
		if (bufferLength >= 8)
		{
			*buffer++ = isc_info_sql_stmt_type;
			put_vax_short(buffer, 4);
			buffer += 2;
			put_vax_long(buffer, type.value);
			buffer += 4;
			*buffer = isc_info_end;
		}
		else
			*buffer = isc_info_truncated;

		return true;
	}

	return false;
}

// Fetch input or output parameter list.
void StatementMetadata::fetchParameters(UCHAR code, Parameters* parameters)
{
	while (!parameters->fetched)
	{
		unsigned startIndex = 1;

		for (ObjectsArray<Parameters::Item>::iterator i = parameters->items.begin();
			 i != parameters->items.end();
			 ++i)
		{
			if (!i->finished)
				break;

			++startIndex;
		}

		UCHAR items[5 + sizeof(DESCRIBE_VARS)] =
		{
			isc_info_sql_sqlda_start,
			2,
			UCHAR(startIndex & 0xFF),
			UCHAR((startIndex >> 8) & 0xFF),
			code
		};
		memcpy(items + 5, DESCRIBE_VARS, sizeof(DESCRIBE_VARS));

		UCHAR buffer[INFO_BUFFER_SIZE];
		memset(buffer, 0, sizeof(buffer));
		getAndParse(sizeof(items), items, sizeof(buffer), buffer);
	}
}


//--------------------------------------


static USHORT getLen(const UCHAR** ptr, const UCHAR* bufferEnd)
{
	if (bufferEnd - *ptr < 2)
		fatal_exception::raise("Invalid info structure - no space for clumplet length");

	const USHORT len = static_cast<USHORT>(gds__vax_integer(*ptr, 2));
	*ptr += 2;

	fb_assert(bufferEnd - *ptr >= len);
	if (bufferEnd - *ptr < len)
		fatal_exception::raiseFmt("Invalid info structure - no space for clumplet data: need %d, actual %d",
			len, bufferEnd - *ptr);

	return len;
}


// Pick up a VAX format numeric info item with a 2 byte length.
static int getNumericInfo(const UCHAR** ptr, const UCHAR* bufferEnd)
{
	const USHORT len = getLen(ptr, bufferEnd);
	int item = gds__vax_integer(*ptr, len);
	*ptr += len;
	return item;
}

// Pick up a string valued info item.
static void getStringInfo(const UCHAR** ptr, const UCHAR* bufferEnd, string* str)
{
	const USHORT len = getLen(ptr, bufferEnd);
	str->assign(*ptr, len);
	*ptr += len;
}


}	// namespace Firebird
