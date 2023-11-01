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
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_PREPARED_STATEMENT_H
#define JRD_PREPARED_STATEMENT_H

#include "firebird.h"
#include <utility>
#include <optional>
#include "../common/dsc.h"
#include "../common/MsgMetadata.h"
#include "../jrd/intl.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/MetaString.h"
#include "../jrd/MetaName.h"

namespace Jrd {

class thread_db;
class jrd_tra;
class Attachment;
class DsqlRequest;
class dsql_msg;
class ResultSet;


template <typename T>
struct PreparedStatementTypeDiscriminator {};

class PreparedStatement : public Firebird::PermanentStorage
{
friend class ResultSet;

public:
	class Builder
	{
	public:
		enum Type
		{
			TYPE_SSHORT,
			TYPE_SLONG,
			TYPE_SINT64,
			TYPE_DOUBLE,
			TYPE_METANAME,
			TYPE_STRING,
			TYPE_METASTRING,
		};

		// This struct and the member outputParams are used to make the C++ undefined parameter
		// evaluation order do not interfere on the question marks / slots correspondence.
		struct OutputParam
		{
			OutputParam(const char* aChunk, FB_SIZE_T aNumber)
				: chunk(aChunk),
				  number(aNumber)
			{
			}

			const char* chunk;
			FB_SIZE_T number;
		};

		struct InputSlot
		{
			Type type;
			unsigned number;
			const void* address;
			bool isOptional;
		};

		struct OutputSlot
		{
			Type type;
			unsigned number;
			void* address;
			bool isOptional;
		};

	public:
		Builder()
			: outputParams(0)
		{
		}

	public:
		// Output variables.

		template <typename T> OutputParam operator ()(const char* chunk, std::optional<T>& param)
		{
			addOutput(PreparedStatementTypeDiscriminator<T>::TYPE, &param, true, outputSlots);
			return OutputParam(chunk, outputSlots.getCount() - 1);
		}

		template <typename T> OutputParam operator ()(const char* chunk, T& param)
		{
			addOutput(PreparedStatementTypeDiscriminator<T>::TYPE, &param, false, outputSlots);
			return OutputParam(chunk, outputSlots.getCount() - 1);
		}

		// SQL text concatenation.
		Builder& operator <<(OutputParam outputParam)
		{
			text += " ";
			text += outputParam.chunk;
			outputSlots[outputParam.number].number = ++outputParams;
			return *this;
		}

		// Input variables.

		Builder& operator <<(const char* chunk)
		{
			text += " ";
			text += chunk;
			return *this;
		}

		template <typename T> Builder& operator <<(const std::optional<T>& param)
		{
			addInput(PreparedStatementTypeDiscriminator<T>::TYPE, &param, true, inputSlots);
			text += "?";
			return *this;
		}

		template <typename T> Builder& operator <<(const T& param)
		{
			addInput(PreparedStatementTypeDiscriminator<T>::TYPE, &param, false, inputSlots);
			text += "?";
			return *this;
		}

	public:
		const Firebird::string& getText() const
		{
			return text;
		}

		void moveFromResultSet(thread_db* tdbb, ResultSet* rs) const;
		void moveToStatement(thread_db* tdbb, PreparedStatement* stmt) const;

	private:
		void addInput(Type type, const void* address, bool isOptional, Firebird::Array<InputSlot>& slots)
		{
			InputSlot slot;
			slot.type = type;
			slot.number = (unsigned) slots.getCount() + 1;
			slot.address = address;
			slot.isOptional = isOptional;
			slots.add(slot);
		}

		void addOutput(Type type, void* address, bool isOptional, Firebird::Array<OutputSlot>& slots)
		{
			OutputSlot slot;
			slot.type = type;
			slot.number = (unsigned) slots.getCount() + 1;
			slot.address = address;
			slot.isOptional = isOptional;
			slots.add(slot);
		}

	private:
		Firebird::string text;
		Firebird::Array<InputSlot> inputSlots;
		Firebird::Array<OutputSlot> outputSlots;
		unsigned outputParams;
	};

private:
	// Auxiliary class to use positional parameters with C++ variables.
	class PosBuilder
	{
	public:
		explicit PosBuilder(const Firebird::string& aText)
			: text(aText),
			  params(0)
		{
		}

		PosBuilder& operator <<(const char* chunk)
		{
			text += chunk;
			return *this;
		}

		PosBuilder& operator <<(unsigned& param)
		{
			text += "?";
			param = ++params;
			return *this;
		}

		operator const Firebird::string& ()
		{
			return text;
		}

	private:
		Firebird::string text;
		unsigned params;
	};

public:
	// Create a PreparedStatement builder to use positional parameters with C++ variables.
	static PosBuilder build(const Firebird::string& text)
	{
		return PosBuilder(text);
	}

	// Escape a metadata name accordingly to SQL rules.
	static Firebird::string escapeName(const MetaName& s)
	{
		Firebird::string ret;

		for (const char* p = s.begin(); p != s.end(); ++p)
		{
			ret += *p;
			if (*p == '\"')
				ret += '\"';
		}

		return ret;
	}

	// Escape a string accordingly to SQL rules.
	template <typename T> static Firebird::string escapeString(const T& s)
	{
		Firebird::string ret;

		for (const char* p = s.begin(); p != s.end(); ++p)
		{
			ret += *p;
			if (*p == '\'')
				ret += '\'';
		}

		return ret;
	}

public:
	PreparedStatement(thread_db* tdbb, Firebird::MemoryPool& aPool, Attachment* attachment,
		jrd_tra* transaction, const Firebird::string& text, bool isInternalRequest);
	PreparedStatement(thread_db* tdbb, Firebird::MemoryPool& aPool, Attachment* attachment,
		jrd_tra* transaction, const Builder& aBuilder, bool isInternalRequest);
	~PreparedStatement();

private:
	void init(thread_db* tdbb, Attachment* attachment, jrd_tra* transaction,
		const Firebird::string& text, bool isInternalRequest);

public:
	void setDesc(thread_db* tdbb, unsigned param, const dsc& value);

	void setNull(unsigned param)
	{
		fb_assert(param > 0);

		dsc* desc = &inValues[(param - 1) * 2 + 1];
		fb_assert(desc->dsc_dtype == dtype_short);
		*reinterpret_cast<SSHORT*>(desc->dsc_address) = -1;
	}

	void setSmallInt(thread_db* tdbb, unsigned param, SSHORT value, SCHAR scale = 0)
	{
		fb_assert(param > 0);

		dsc desc;
		desc.makeShort(scale, &value);
		setDesc(tdbb, param, desc);
	}

	void setInt(thread_db* tdbb, unsigned param, SLONG value, SCHAR scale = 0)
	{
		fb_assert(param > 0);

		dsc desc;
		desc.makeLong(scale, &value);
		setDesc(tdbb, param, desc);
	}

	void setBigInt(thread_db* tdbb, unsigned param, SINT64 value, SCHAR scale = 0)
	{
		fb_assert(param > 0);

		dsc desc;
		desc.makeInt64(scale, &value);
		setDesc(tdbb, param, desc);
	}

	void setDouble(thread_db* tdbb, unsigned param, double value)
	{
		fb_assert(param > 0);

		dsc desc;
		desc.makeDouble(&value);
		setDesc(tdbb, param, desc);
	}

	void setString(thread_db* tdbb, unsigned param, const Firebird::string& value)
	{
		fb_assert(param > 0);

		dsc desc;
		desc.makeText((USHORT) value.length(), inValues[(param - 1) * 2].getTextType(),
			(UCHAR*) value.c_str());
		setDesc(tdbb, param, desc);
	}

	void setMetaName(thread_db* tdbb, unsigned param, const MetaName& value)
	{
		fb_assert(param > 0);

		dsc desc;
		desc.makeText((USHORT) value.length(), CS_METADATA, (UCHAR*) value.c_str());
		setDesc(tdbb, param, desc);
	}

	void setMetaString(thread_db* tdbb, unsigned param, const Firebird::MetaString& value)
	{
		fb_assert(param > 0);

		dsc desc;
		desc.makeText((USHORT) value.length(), CS_METADATA, (UCHAR*) value.c_str());
		setDesc(tdbb, param, desc);
	}

	void execute(thread_db* tdbb, jrd_tra* transaction);
	void open(thread_db* tdbb, jrd_tra* transaction);
	ResultSet* executeQuery(thread_db* tdbb, jrd_tra* transaction);
	unsigned executeUpdate(thread_db* tdbb, jrd_tra* transaction);

	int getResultCount() const;

	DsqlRequest* getDsqlRequest()
	{
		return dsqlRequest;
	}

	static void parseDsqlMessage(const dsql_msg* dsqlMsg, Firebird::Array<dsc>& values,
		Firebird::MsgMetadata* msgMetadata, Firebird::UCharBuffer& msg);

private:
	const Builder* builder;
	DsqlRequest* dsqlRequest;
	Firebird::Array<dsc> inValues, outValues;
	Firebird::RefPtr<Firebird::MsgMetadata> inMetadata, outMetadata;
	Firebird::UCharBuffer inMessage, outMessage;
	ResultSet* resultSet;
};

typedef Firebird::AutoPtr<PreparedStatement> AutoPreparedStatement;

// PreparedStatementTypeDiscriminator specializations.

template <>
struct PreparedStatementTypeDiscriminator<SSHORT>
{
	static constexpr PreparedStatement::Builder::Type TYPE = PreparedStatement::Builder::TYPE_SSHORT;
};

template <>
struct PreparedStatementTypeDiscriminator<SLONG>
{
	static constexpr PreparedStatement::Builder::Type TYPE = PreparedStatement::Builder::TYPE_SLONG;
};

template <>
struct PreparedStatementTypeDiscriminator<SINT64>
{
	static constexpr PreparedStatement::Builder::Type TYPE = PreparedStatement::Builder::TYPE_SINT64;
};

template <>
struct PreparedStatementTypeDiscriminator<double>
{
	static constexpr PreparedStatement::Builder::Type TYPE = PreparedStatement::Builder::TYPE_DOUBLE;
};

template <>
struct PreparedStatementTypeDiscriminator<Firebird::string>
{
	static constexpr PreparedStatement::Builder::Type TYPE = PreparedStatement::Builder::TYPE_STRING;
};

template <>
struct PreparedStatementTypeDiscriminator<MetaName>
{
	static constexpr PreparedStatement::Builder::Type TYPE = PreparedStatement::Builder::TYPE_METANAME;
};

template <>
struct PreparedStatementTypeDiscriminator<Firebird::MetaString>
{
	static constexpr PreparedStatement::Builder::Type TYPE = PreparedStatement::Builder::TYPE_METASTRING;
};


}	// namespace

#endif	// JRD_PREPARED_STATEMENT_H
