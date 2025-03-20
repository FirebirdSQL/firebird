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

#ifndef JRD_EXT_ENGINE_MANAGER_H
#define JRD_EXT_ENGINE_MANAGER_H

#include "firebird/Interface.h"

#include <memory>
#include <optional>

#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/NestConst.h"
#include "../common/classes/auto.h"
#include "../common/classes/rwlock.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/StatementMetadata.h"
#include "../common/classes/GetPlugins.h"

struct dsc;

namespace Jrd {

class thread_db;
class jrd_prc;
class Request;
class jrd_tra;
class Attachment;
class CompilerScratch;
class Database;
class Format;
class Trigger;
class Function;
class DeclareVariableNode;
class StmtNode;
class ValueExprNode;
struct impure_value;
struct record_param;


class ExtEngineManager final : public Firebird::PermanentStorage
{
private:
	class AttachmentImpl;
	template <typename T> class ContextManager;
	class TransactionImpl;

	class RoutineMetadata final :
		public Firebird::VersionedIface<Firebird::IRoutineMetadataImpl<RoutineMetadata, Firebird::CheckStatusWrapper> >,
		public Firebird::PermanentStorage
	{
	public:
		explicit RoutineMetadata(MemoryPool& pool)
			: PermanentStorage(pool),
			  name(pool),
			  entryPoint(pool),
			  body(pool),
			  triggerTable(pool),
			  triggerType(0)
		{
		}

		const char* getPackage(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return name.package.nullStr();
		}

		const char* getName(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return name.object.c_str();
		}

		const char* getEntryPoint(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return entryPoint.c_str();
		}

		const char* getBody(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return body.c_str();
		}

		Firebird::IMessageMetadata* getInputMetadata(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return getMetadata(inputParameters);
		}

		Firebird::IMessageMetadata* getOutputMetadata(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return getMetadata(outputParameters);
		}

		Firebird::IMessageMetadata* getTriggerMetadata(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return getMetadata(triggerFields);
		}

		const char* getTriggerTable(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return triggerTable.object.c_str();
		}

		unsigned getTriggerType(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return triggerType;
		}

		const char* getSchema(Firebird::CheckStatusWrapper* /*status*/) const
		{
			return name.schema.c_str();
		}

	public:
		QualifiedName name;
		Firebird::string entryPoint;
		Firebird::string body;
		Firebird::RefPtr<Firebird::IMessageMetadata> inputParameters;
		Firebird::RefPtr<Firebird::IMessageMetadata> outputParameters;
		Firebird::RefPtr<Firebird::IMessageMetadata> triggerFields;
		QualifiedName triggerTable;
		unsigned triggerType;

	private:
		static Firebird::IMessageMetadata* getMetadata(const Firebird::IMessageMetadata* par)
		{
			Firebird::IMessageMetadata* rc = const_cast<Firebird::IMessageMetadata*>(par);
			rc->addRef();
			return rc;
		}
	};

	class ExternalContextImpl :
		public Firebird::VersionedIface<Firebird::IExternalContextImpl<ExternalContextImpl, Firebird::CheckStatusWrapper> >
	{
	friend class AttachmentImpl;

	public:
		ExternalContextImpl(thread_db* tdbb, Firebird::IExternalEngine* aEngine);
		virtual ~ExternalContextImpl();

		void releaseTransaction();
		void setTransaction(thread_db* tdbb);

		Firebird::IMaster* getMaster();
		Firebird::IExternalEngine* getEngine(Firebird::CheckStatusWrapper* status);
		Firebird::IAttachment* getAttachment(Firebird::CheckStatusWrapper* status);
		Firebird::ITransaction* getTransaction(Firebird::CheckStatusWrapper* status);
		const char* getUserName();
		const char* getDatabaseName();
		const char* getClientCharSet();
		int obtainInfoCode();
		void* getInfo(int code);
		void* setInfo(int code, void* value);

	private:
		Firebird::IExternalEngine* engine;
		Attachment* internalAttachment;
		Firebird::ITransaction* internalTransaction;
		Firebird::IAttachment* externalAttachment;
		Firebird::ITransaction* externalTransaction;
		Firebird::GenericMap<Firebird::NonPooled<int, void*> > miscInfo;
		MetaName clientCharSet;
	};

	struct EngineAttachment
	{
		EngineAttachment(Firebird::IExternalEngine* aEngine, Jrd::Attachment* aAttachment)
			: engine(aEngine),
			  attachment(aAttachment)
		{
		}

		static bool greaterThan(const EngineAttachment& i1, const EngineAttachment& i2)
		{
			return (i1.engine > i2.engine) ||
				(i1.engine == i2.engine && i1.attachment > i2.attachment);
		}

		Firebird::IExternalEngine* engine;
		Jrd::Attachment* attachment;
	};

	struct EngineAttachmentInfo
	{
		EngineAttachmentInfo()
			: engine(NULL),
			  context(NULL),
			  adminCharSet(0)
		{
		}

		Firebird::IExternalEngine* engine;
		Firebird::AutoPtr<ExternalContextImpl> context;
		USHORT adminCharSet;
	};

public:
	class ExtRoutine
	{
	public:
		ExtRoutine(thread_db* tdbb, ExtEngineManager* aExtManager,
			Firebird::IExternalEngine* aEngine, RoutineMetadata* aMetadata);
		virtual ~ExtRoutine() = default;

	private:
		class PluginDeleter
		{
		public:
			void operator()(Firebird::IPluginBase* ptr);
		};

	protected:
		ExtEngineManager* extManager;
		std::unique_ptr<Firebird::IExternalEngine, PluginDeleter> engine;
		Firebird::AutoPtr<RoutineMetadata> metadata;
		Database* database;
	};

	class Function final : public ExtRoutine
	{
	private:
		struct Impl;	// hack to avoid circular inclusion of headers

	public:
		Function(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, ExtEngineManager* aExtManager,
			Firebird::IExternalEngine* aEngine,
			RoutineMetadata* aMetadata,
			Firebird::IExternalFunction* aFunction,
			Firebird::RefPtr<Firebird::IMessageMetadata> extInputParameters,
			Firebird::RefPtr<Firebird::IMessageMetadata> extOutputParameters,
			const Jrd::Function* aUdf);
		~Function() override;

		void execute(thread_db* tdbb, Request* request, jrd_tra* transaction,
			unsigned inMsgLength, UCHAR* inMsg, unsigned outMsgLength, UCHAR* outMsg) const;

	private:
		void validateParameters(thread_db* tdbb, UCHAR* msg, bool input) const;

	private:
		Firebird::IExternalFunction* function;
		const Jrd::Function* udf;
		Firebird::AutoPtr<Format> extInputFormat;
		Firebird::AutoPtr<Format> extOutputFormat;
		Firebird::AutoPtr<Impl> impl;
		std::optional<ULONG> extInputImpureOffset;
		std::optional<ULONG> extOutputImpureOffset;
	};

	class ResultSet;

	class Procedure final : public ExtRoutine
	{
	friend class ResultSet;

	public:
		Procedure(thread_db* tdbb, ExtEngineManager* aExtManager,
			Firebird::IExternalEngine* aEngine,
			RoutineMetadata* aMetadata,
			Firebird::IExternalProcedure* aProcedure,
			const jrd_prc* aPrc);
		~Procedure() override;

		ResultSet* open(thread_db* tdbb, UCHAR* inMsg, UCHAR* outMsg) const;

	private:
		Firebird::IExternalProcedure* procedure;
		const jrd_prc* prc;
	};

	class ResultSet
	{
	public:
		ResultSet(thread_db* tdbb, UCHAR* inMsg, UCHAR* outMsg, const Procedure* aProcedure);
		~ResultSet();

		bool fetch(thread_db* tdbb);

	private:
		const Procedure* procedure;
		Attachment* attachment;
		bool firstFetch;
		EngineAttachmentInfo* attInfo;
		Firebird::IExternalResultSet* resultSet;
		USHORT charSet;
	};

	class Trigger final : public ExtRoutine
	{
	public:
		Trigger(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, ExtEngineManager* aExtManager,
			Firebird::IExternalEngine* aEngine, RoutineMetadata* aMetadata,
			Firebird::IExternalTrigger* aTrigger, const Jrd::Trigger* aTrg);
		~Trigger() override;

		void execute(thread_db* tdbb, Request* request, unsigned action,
			record_param* oldRpb, record_param* newRpb) const;

	private:
		void setupComputedFields(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb);
		void setValues(thread_db* tdbb, Request* request, Firebird::Array<UCHAR>& msgBuffer, record_param* rpb) const;

	public:
		Firebird::Array<NestConst<StmtNode>> computedStatements;

	private:
		Firebird::AutoPtr<Format> format;
		Firebird::IExternalTrigger* trigger;
		const Jrd::Trigger* trg;
		Firebird::Array<USHORT> fieldsPos;
		Firebird::Array<const DeclareVariableNode*> varDecls;
		USHORT computedCount;
	};

public:
	explicit ExtEngineManager(MemoryPool& p);
	~ExtEngineManager();

public:
	static void initialize();

public:
	void closeAttachment(thread_db* tdbb, Attachment* attachment);

	void makeFunction(thread_db* tdbb, CompilerScratch* csb, Jrd::Function* udf,
		const MetaName& engine, const Firebird::string& entryPoint,
		const Firebird::string& body);
	void makeProcedure(thread_db* tdbb, CompilerScratch* csb, jrd_prc* prc,
		const MetaName& engine, const Firebird::string& entryPoint,
		const Firebird::string& body);
	void makeTrigger(thread_db* tdbb, CompilerScratch* csb, Jrd::Trigger* trg,
		const MetaName& engine, const Firebird::string& entryPoint,
		const Firebird::string& body, unsigned type);

private:
	Firebird::IExternalEngine* getEngine(thread_db* tdbb,
		const MetaName& name);
	EngineAttachmentInfo* getEngineAttachment(thread_db* tdbb,
		const MetaName& name);
	EngineAttachmentInfo* getEngineAttachment(thread_db* tdbb,
		Firebird::IExternalEngine* engine, bool closing = false);
	void setupAdminCharSet(thread_db* tdbb, Firebird::IExternalEngine* engine,
		EngineAttachmentInfo* attInfo);

private:
	typedef Firebird::GenericMap<Firebird::Pair<
		Firebird::Left<MetaName, Firebird::IExternalEngine*> > > EnginesMap;
	typedef Firebird::GenericMap<Firebird::Pair<Firebird::NonPooled<
		EngineAttachment, EngineAttachmentInfo*> >, EngineAttachment> EnginesAttachmentsMap;

	Firebird::RWLock enginesLock;
	EnginesMap engines;
	EnginesAttachmentsMap enginesAttachments;
};


}	// namespace Jrd

#endif	// JRD_EXT_ENGINE_MANAGER_H
