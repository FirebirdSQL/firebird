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
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_SYS_PACKAGES_SQL_PACKAGE_H
#define JRD_SYS_PACKAGES_SQL_PACKAGE_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/array.h"
#include "../common/classes/MetaString.h"
#include "../common/classes/objects_array.h"
#include "../jrd/SystemPackages.h"

namespace Firebird::Jrd
{


class SqlPackage final : public SystemPackage
{
public:
	SqlPackage(MemoryPool& pool);

	SqlPackage(const SqlPackage&) = delete;
	SqlPackage& operator=(const SqlPackage&) = delete;

private:
	FB_MESSAGE(ExplainInput, ThrowStatusExceptionWrapper,
		(FB_BLOB, sql)
	);

	FB_MESSAGE(ExplainOutput, ThrowStatusExceptionWrapper,
		(FB_INTEGER, planLine)
		(FB_BIGINT, recordSourceId)
		(FB_BIGINT, parentRecordSourceId)
		(FB_INTEGER, level)
		(FB_SMALLINT, objectType)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), schemaName)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), packageName)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), objectName)
		(FB_INTL_VARCHAR(255 * METADATA_BYTES_PER_CHAR, CS_METADATA), alias)
		(FB_DOUBLE, cardinality)
		(FB_INTEGER, recordLength)
		(FB_INTEGER, keyLength)
		(FB_BLOB, accessPath)
	);

	class ExplainResultSet :
		public
			DisposeIface<
				IExternalResultSetImpl<
					ExplainResultSet,
					ThrowStatusExceptionWrapper
				>
			>
	{
	public:
		ExplainResultSet(ThrowStatusExceptionWrapper* status, IExternalContext* context,
			const ExplainInput::Type* in, ExplainOutput::Type* out);

	public:
		void dispose() override
		{
			delete this;
		}

	public:
		FB_BOOLEAN fetch(ThrowStatusExceptionWrapper* status) override;

	private:
		ExplainOutput::Type* out;
		Array<ExplainOutput::Type> resultEntries{*getDefaultMemoryPool()};
		Array<ExplainOutput::Type>::const_iterator resultIterator = nullptr;
	};

	static IExternalResultSet* explainProcedure(ThrowStatusExceptionWrapper* status,
		IExternalContext* context, const ExplainInput::Type* in, ExplainOutput::Type* out);

	//----------

	FB_MESSAGE(ParseUnqualifiedNamesInput, ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(MAX_VARY_COLUMN_SIZE / METADATA_BYTES_PER_CHAR * METADATA_BYTES_PER_CHAR, CS_METADATA), names)
	);

	FB_MESSAGE(ParseUnqualifiedNamesOutput, ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), name)
	);

	class ParseUnqualifiedNamesResultSet :
		public
			DisposeIface<
				IExternalResultSetImpl<
					ParseUnqualifiedNamesResultSet,
					ThrowStatusExceptionWrapper
				>
			>
	{
	public:
		ParseUnqualifiedNamesResultSet(ThrowStatusExceptionWrapper* status, IExternalContext* context,
			const ParseUnqualifiedNamesInput::Type* in, ParseUnqualifiedNamesOutput::Type* out);

	public:
		void dispose() override
		{
			delete this;
		}

	public:
		FB_BOOLEAN fetch(ThrowStatusExceptionWrapper* status) override;

	private:
		ParseUnqualifiedNamesOutput::Type* out;
		ObjectsArray<MetaString> resultEntries{*getDefaultMemoryPool()};
		ObjectsArray<MetaString>::const_iterator resultIterator;
	};

	static IExternalResultSet* parseUnqualifiedNamesProcedure(ThrowStatusExceptionWrapper* status,
		IExternalContext* context,
		const ParseUnqualifiedNamesInput::Type* in, ParseUnqualifiedNamesOutput::Type* out);
};


}	// namespace Firebird::Jrd

#endif	// JRD_SYS_PACKAGES_SQL_PACKAGE_H
