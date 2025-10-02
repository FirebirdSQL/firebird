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
 *  The Original Code was created by Vasiliy Yashkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 Vasiliy Yashkov <vasiliy.yashkov13@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_FOREIGN_SERVER_H
#define JRD_FOREIGN_SERVER_H

#include "../common/utils_proto.h"
#include "../common/classes/ClumpletWriter.h"
#include "../jrd/jrd.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../dsql/Nodes.h"
#include "../jrd/extds/IscDS.h"

// Foreign server option constants
const char* const FOREIGN_SERVER_PROVIDER			= "PROVIDER";
const char* const FOREIGN_SERVER_CONNECTION_STRING	= "CONNECTION_STRING";
const char* const FOREIGN_SERVER_USER				= "USER";
const char* const FOREIGN_SERVER_PASSWORD			= "PASSWORD";
const char* const FOREIGN_SERVER_ROLE				= "ROLE";
const char* const FOREIGN_SERVER_CHARSET			= "CHARSET";

// Foreign table option constants
const char* const FOREIGN_TABLE_SCHEMA_NAME			= "SCHEMA_NAME";
const char* const FOREIGN_TABLE_NAME				= "TABLE_NAME";

// Foreign table column option constants
const char* const FOREIGN_TABLE_COLUMN_NAME			= "COLUMN_NAME";
const char* const FOREIGN_TABLE_COLUMN_PK			= "PRIMARY_KEY";
// Marks the field as read-only. The value of this field is computed by a foreign server.
// Insert, update and delete operations are prohibited for this field.
const char* const FOREIGN_TABLE_COLUMN_READONLY		= "READONLY";

static const char* FOREIGN_GENERAL_OPTIONS[] =
{
	FOREIGN_SERVER_CONNECTION_STRING,
	FOREIGN_SERVER_USER,
	FOREIGN_SERVER_PASSWORD,
	FOREIGN_SERVER_ROLE
};

namespace Jrd
{
	bool getExternalValue(const Firebird::string& value, const ExternalValueType type, Firebird::string& output);

	struct ForeignOption
	{
		ForeignOption(MemoryPool& p)
			: m_name(p),
			  m_value(p),
			  m_type(ExternalValueType::TYPE_STRING)
		{}

		ForeignOption(MemoryPool& p, const ForeignOption& o)
			: m_name(p, o.m_name),
			  m_value(p, o.m_value),
			  m_type(o.m_type)
		{}

		Firebird::string getActualValue() const
		{
			Firebird::string out;
			if (getExternalValue(m_value, m_type, out))
				return out;

			return m_value;
		}

		Firebird::MetaString m_name;
		Firebird::string m_value;
		ExternalValueType m_type;
	};

	typedef Firebird::FullPooledPair<Firebird::MetaString, ForeignOption> MetaStringOptionPair;

	class ForeignServer
	{
	public:
		explicit ForeignServer(MemoryPool& p, const MetaName& aName, const MetaName& aPlugin)
			: name(p, aName), plugin(p, aPlugin), options(p)
		{}

		void addOption(const MetaName& name, const Firebird::string& value, const ExternalValueType type);
		const Firebird::GenericMap<MetaStringOptionPair>& getOptions() const;

		const MetaName& getServerName() const
		{
			return name;
		}

		const MetaName& getPluginName() const
		{
			return plugin;
		}

	private:
		const MetaName name;
		const MetaName plugin;
		Firebird::GenericMap<MetaStringOptionPair> options;
	};

	class ForeignTableConnection : public EDS::IscConnection
	{
	public:
		void getProviderInfo(thread_db* tdbb);
		// Test specified SQL feature flag
		bool testSqlFeature(info_sql_features value) const { return m_sqlFeatures[value]; }

	private:
		// Set specified SQL feature flag
		void setSqlFeature(info_sql_features value) { m_sqlFeatures[value] = true; }

		bool m_sqlFeatures[fb_feature_sql_max];
	};

	class ForeignTableProvider : public EDS::IscProvider
	{
	public:
		ForeignTableConnection* createForeignConnection(thread_db* tdbb, ForeignServer* server);

	private:
		// Add extra options to database parameter buffer
		void fillOptionsDPB(const Firebird::GenericMap<MetaStringOptionPair>& options, Firebird::ClumpletWriter& dpb);
		// Test if an option is general
		bool isGenericOption(const MetaName& option);
	};

	class ForeignTableStatement : public EDS::IscStatement
	{
	public:
		void setSql(const Firebird::string& sql);
		void openInternal(thread_db* tdbb, EDS::IscTransaction* transaction);
		void executeInternal(thread_db* tdbb, EDS::IscTransaction* transaction, record_param* org_rpb,
			record_param* new_rpb, const Firebird::Array<int>* skippedOrgRpb = nullptr,
			const Firebird::Array<int>* skippedNewRpb = nullptr);
		bool fetchInternal(thread_db* tdbb, Record* record);

	private:
		void addRecordDescs(DescList& outDescs,
			Record* record,
			const Firebird::Array<int>* skippedParameters = nullptr,
			const USHORT multiplier = 1);
		void setInParamsInternal(thread_db* tdbb,
			record_param* rpb,
			const USHORT offset,
			const Firebird::Array<int>* skippedParameters = nullptr,
			const USHORT multiplier = 1);
	};

	class ForeignTableAdapter
	{
	public:
		class ForeignField
		{
		public:
			explicit ForeignField(MemoryPool& p)
				: name(p),
				options(p)
			{
			}

			MetaName name;
			Firebird::GenericMap<MetaStringOptionPair> options;
		};

		explicit ForeignTableAdapter(MemoryPool& pool, jrd_rel* relation, ForeignServer* server)
			: m_relation(relation),
			m_server(server),
			m_tableOptions(pool),
			m_foreignFields(pool),
			m_foreignPKNames(pool),
			m_readOnlyNames(pool),
			m_skippedNewRpbIdx(pool),
			m_skippedOrgRpbIdx(pool)
		{ }

		~ForeignTableAdapter()
		{
			Firebird::LeftPooledMap<Firebird::MetaString, ForeignField*>::Accessor accessor(&m_foreignFields);
			for (bool found = accessor.getFirst(); found; found = accessor.getNext())
			{
				Firebird::AutoPtr<ForeignField> field(accessor.current()->second);
			}
			m_foreignFields.clear();
		}

		EDS::Statement* createStatement(thread_db* tdbb, record_param* org_rpb = NULL, record_param* new_rpb = NULL,
			const Firebird::string& filter = "", const Firebird::string& order = "");
		void execute(thread_db* tdbb, EDS::Statement* statement, record_param* org_rpb = NULL,
			record_param* new_rpb = NULL);
		bool fetch(thread_db* tdbb, EDS::Statement* statement, Record* record);
		bool testSqlFeature(thread_db* tdbb, info_sql_features value);

		void addTableOption(const MetaName& name, const Firebird::string& value,
			const ExternalValueType type);
		void addTableField(const MetaName& fieldName, const MetaName& optionName, const Firebird::string& value,
			const ExternalValueType type);
		const Firebird::string getOriginalFieldName(const MetaName& name) const;

		const ForeignServer* getServer() { return m_server; }

	private:
		void ensureConnect(thread_db* tdbb);
		void getSql(Firebird::string& sql, const Firebird::string& filter, const Firebird::string& order,
			record_param* org_rpb, record_param* new_rpb) const;
		Firebird::string getRelationFieldsSql(bool select) const;
		const Firebird::string getWhereClauseSql() const;
		void processRecord(Firebird::Array<int>& outSkippedRpbIdx, record_param* rpb, bool isNewRecord = false);
		bool isFieldReadOnlyOrComputed(const jrd_fld* field) const;

		jrd_rel* m_relation;
		Firebird::AutoPtr<ForeignServer> m_server;

		ForeignTableProvider* m_provider = NULL;
		ForeignTableConnection* m_connection = NULL;

		Firebird::GenericMap<MetaStringOptionPair> m_tableOptions;
		Firebird::LeftPooledMap<Firebird::MetaString, ForeignField*> m_foreignFields;
		Firebird::SortedArray<MetaName> m_foreignPKNames;
		Firebird::SortedArray<MetaName> m_readOnlyNames;
		Firebird::Array<int> m_skippedNewRpbIdx;
		Firebird::Array<int> m_skippedOrgRpbIdx;
	};
}

#endif // JRD_FOREIGN_SERVER_H
