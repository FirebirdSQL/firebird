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

#include "firebird.h"
#include "firebird/impl/consts_pub.h"
#include "iberror.h"
#include "firebird/impl/inf_pub.h"
#include "../jrd/ExtEngineManager.h"
#include "firebird/impl/sqlda_pub.h"
#include "../common/dsc.h"
#include "../jrd/align.h"
#include "../jrd/jrd.h"
#include "../jrd/exe.h"
#include "../jrd/req.h"
#include "../jrd/status.h"
#include "../jrd/tra.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/StmtNodes.h"
#include "../common/os/path_utils.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/Function.h"
#include "../jrd/TimeZone.h"
#include "../jrd/SystemPackages.h"
#include "../common/isc_proto.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/init.h"
#include "../common/classes/objects_array.h"
#include "../common/config/config.h"
#include "../common/ScanDir.h"
#include "../common/utils_proto.h"
#include "../common/classes/GetPlugins.h"

using namespace Firebird;
using namespace Jrd;


namespace
{
	// Internal message node.
	class IntMessageNode : public MessageNode
	{
	public:
		IntMessageNode(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, USHORT message,
				Array<NestConst<Parameter> >& aParameters, const Format* aFormat)
			: MessageNode(pool),
			  parameters(aParameters),
			  format(aFormat)
		{
			setup(tdbb, csb, message, format->fmt_count);
		}

		virtual USHORT setupDesc(thread_db* tdbb, CompilerScratch* csb, USHORT index,
			dsc* desc, ItemInfo* itemInfo)
		{
			*desc = format->fmt_desc[index];

			if (index % 2 == 0 && index / 2u < parameters.getCount())
			{
				const Parameter* param = parameters[index / 2];

				if (param->prm_mechanism != prm_mech_type_of &&
					!fb_utils::implicit_domain(param->prm_field_source.c_str()))
				{
					MetaNamePair namePair(param->prm_field_source, "");

					FieldInfo fieldInfo;
					bool exist = csb->csb_map_field_info.get(namePair, fieldInfo);
					MET_get_domain(tdbb, csb->csb_pool, param->prm_field_source, desc,
						(exist ? NULL : &fieldInfo));

					if (!exist)
						csb->csb_map_field_info.put(namePair, fieldInfo);

					itemInfo->field = namePair;
					itemInfo->nullable = fieldInfo.nullable;
					itemInfo->fullDomain = true;
				}

				itemInfo->name = param->prm_name;

				if (!param->prm_nullable)
					itemInfo->nullable = false;
			}

			return type_alignments[desc->dsc_dtype];
		}

	public:
		Array<NestConst<Parameter> >& parameters;
		const Format* format;
	};

	// External message node.
	class ExtMessageNode : public MessageNode
	{
	public:
		ExtMessageNode(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, USHORT message, const Format* aFormat)
			: MessageNode(pool),
			  format(aFormat)
		{
			setup(tdbb, csb, message, format->fmt_count);
		}

		virtual USHORT setupDesc(thread_db* tdbb, CompilerScratch* csb, USHORT index,
			dsc* desc, ItemInfo* itemInfo)
		{
			*desc = format->fmt_desc[index];
			return type_alignments[desc->dsc_dtype];
		}

		virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const
		{
			if (request->req_operation == jrd_req::req_evaluate)
			{
				// Clear the message. This is important for external routines.
				UCHAR* msg = request->getImpure<UCHAR>(impureOffset);
				memset(msg, 0, format->fmt_length);
			}

			return MessageNode::execute(tdbb, request, exeState);
		}

	public:
		const Format* format;
	};

	// Initialize output parameters with their domains default value or NULL.
	// Kind of blr_init_variable, but for parameters.
	class InitParameterNode : public TypedNode<StmtNode, StmtNode::TYPE_EXT_INIT_PARAMETER>
	{
	public:
		InitParameterNode(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb,
				Array<NestConst<Parameter> >& parameters, MessageNode* aMessage, USHORT aArgNumber)
			: TypedNode<StmtNode, StmtNode::TYPE_EXT_INIT_PARAMETER>(pool),
			  message(aMessage),
			  argNumber(aArgNumber)
		{
			Parameter* parameter = parameters[argNumber / 2];
			defaultValueNode = NULL;

			if (parameter->prm_mechanism != prm_mech_type_of &&
				!fb_utils::implicit_domain(parameter->prm_field_source.c_str()))
			{
				MetaNamePair namePair(parameter->prm_field_source, "");

				FieldInfo fieldInfo;
				bool exist = csb->csb_map_field_info.get(namePair, fieldInfo);

				if (exist && fieldInfo.defaultValue)
					defaultValueNode = CMP_clone_node(tdbb, csb, fieldInfo.defaultValue);
			}
		}

		string internalPrint(NodePrinter& printer) const
		{
			StmtNode::internalPrint(printer);

			NODE_PRINT(printer, message);
			NODE_PRINT(printer, argNumber);
			NODE_PRINT(printer, defaultValueNode);

			return "InitParameterNode";
		}

		void genBlr(DsqlCompilerScratch* /*dsqlScratch*/)
		{
		}

		InitParameterNode* pass1(thread_db* tdbb, CompilerScratch* csb)
		{
			doPass1(tdbb, csb, &defaultValueNode);
			return this;
		}

		InitParameterNode* pass2(thread_db* tdbb, CompilerScratch* csb)
		{
			ExprNode::doPass2(tdbb, csb, &defaultValueNode);
			return this;
		}

		const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* /*exeState*/) const
		{
			if (request->req_operation == jrd_req::req_evaluate)
			{
				dsc* defaultDesc = NULL;

				if (defaultValueNode)
				{
					defaultDesc = EVL_expr(tdbb, request, defaultValueNode);

					if (request->req_flags & req_null)
						defaultDesc = NULL;
				}

				if (defaultDesc)
				{
					// Initialize the value. The null flag is already initialized to not-null
					// by the ExtMessageNode.

					dsc desc = message->format->fmt_desc[argNumber];
					desc.dsc_address = request->getImpure<UCHAR>(
						message->impureOffset + (IPTR) desc.dsc_address);

					MOV_move(tdbb, defaultDesc, &desc);
				}
				else
				{
					SSHORT tempValue = -1;
					dsc temp;
					temp.makeShort(0, &tempValue);

					dsc desc = message->format->fmt_desc[argNumber + 1];
					desc.dsc_address = request->getImpure<UCHAR>(
						message->impureOffset + (IPTR) desc.dsc_address);

					MOV_move(tdbb, &temp, &desc);
				}

				request->req_operation = jrd_req::req_return;
			}

			return parentStmt;
		}

		private:
			MessageNode* message;
			USHORT argNumber;
			ValueExprNode* defaultValueNode;
	};

	// Output parameters initialization.
	class InitOutputNode : public CompoundStmtNode
	{
	public:
		InitOutputNode(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb,
				Array<NestConst<Parameter> >& parameters, MessageNode* message)
			: CompoundStmtNode(pool)
		{
			// Iterate over the format items, except the EOF item.
			for (USHORT i = 0; i < (message->format->fmt_count / 2) * 2; i += 2)
			{
				InitParameterNode* init = FB_NEW_POOL(pool) InitParameterNode(
					tdbb, pool, csb, parameters, message, i);
				statements.add(init);
			}
		}
	};

	// Move parameters from a message to another, validating theirs values.
	class MessageMoverNode : public CompoundStmtNode
	{
	public:
		MessageMoverNode(MemoryPool& pool, MessageNode* fromMessage, MessageNode* toMessage)
			: CompoundStmtNode(pool)
		{
			// Iterate over the format items, except the EOF item.
			for (USHORT i = 0; i < (fromMessage->format->fmt_count / 2) * 2; i += 2)
			{
				ParameterNode* flag = FB_NEW_POOL(pool) ParameterNode(pool);
				flag->message = fromMessage;
				flag->argNumber = i + 1;

				ParameterNode* param = FB_NEW_POOL(pool) ParameterNode(pool);
				param->message = fromMessage;
				param->argNumber = i;
				param->argFlag = flag;

				AssignmentNode* assign = FB_NEW_POOL(pool) AssignmentNode(pool);
				assign->asgnFrom = param;
				statements.add(assign);

				flag = FB_NEW_POOL(pool) ParameterNode(pool);
				flag->message = toMessage;
				flag->argNumber = i + 1;

				param = FB_NEW_POOL(pool) ParameterNode(pool);
				param->message = toMessage;
				param->argNumber = i;
				param->argFlag = flag;

				assign->asgnTo = param;
			}
		}
	};

	// External function node.
	class ExtFunctionNode : public SuspendNode
	{
	public:
		ExtFunctionNode(MemoryPool& pool, const MessageNode* aExtInMessageNode, const MessageNode* aExtOutMessageNode,
				const ExtEngineManager::Function* aFunction)
			: SuspendNode(pool),
			  extInMessageNode(aExtInMessageNode),
			  extOutMessageNode(aExtOutMessageNode),
			  function(aFunction)
		{
		}

		virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const
		{
			if (request->req_operation == jrd_req::req_evaluate)
			{
				UCHAR* inMsg = extInMessageNode ? request->getImpure<UCHAR>(extInMessageNode->impureOffset) : NULL;
				UCHAR* outMsg = request->getImpure<UCHAR>(extOutMessageNode->impureOffset);

				function->execute(tdbb, inMsg, outMsg);
			}

			return SuspendNode::execute(tdbb, request, exeState);
		}

	private:
		const MessageNode* extInMessageNode;
		const MessageNode* extOutMessageNode;
		const ExtEngineManager::Function* function;
	};

	// External procedure node.
	class ExtProcedureNode : public CompoundStmtNode
	{
	public:
		ExtProcedureNode(MemoryPool& pool, MessageNode* aExtInMessageNode, MessageNode* aExtOutMessageNode,
				MessageNode* aIntOutMessageNode, const ExtEngineManager::Procedure* aProcedure)
			: CompoundStmtNode(pool),
			  extInMessageNode(aExtInMessageNode),
			  extOutMessageNode(aExtOutMessageNode),
			  intOutMessageNode(aIntOutMessageNode),
			  procedure(aProcedure)
		{
			SuspendNode* suspend = FB_NEW_POOL(pool) SuspendNode(pool);
			suspend->message = intOutMessageNode;
			suspend->statement = FB_NEW_POOL(pool) MessageMoverNode(pool, extOutMessageNode, intOutMessageNode);

			statements.add(suspend);
			statements.add(FB_NEW_POOL(pool) StallNode(pool));
		}

		virtual const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* exeState) const
		{
			impure_state* const impure = request->getImpure<impure_state>(impureOffset);
			ExtEngineManager::ResultSet*& resultSet = request->req_ext_resultset;
			UCHAR* extInMsg = extInMessageNode ? request->getImpure<UCHAR>(extInMessageNode->impureOffset) : NULL;
			UCHAR* extOutMsg = extOutMessageNode ? request->getImpure<UCHAR>(extOutMessageNode->impureOffset) : NULL;
			UCHAR* intOutMsg = intOutMessageNode ? request->getImpure<UCHAR>(intOutMessageNode->impureOffset) : NULL;
			SSHORT* eof = intOutMsg ?
				(SSHORT*) (intOutMsg + (IPTR) intOutMessageNode->format->fmt_desc.back().dsc_address) : NULL;

			switch (request->req_operation)
			{
				case jrd_req::req_evaluate:
					fb_assert(!resultSet);
					resultSet = procedure->open(tdbb, extInMsg, extOutMsg);

					if (!resultSet)
					{
						*eof = 0;
						break;
					}
					else
						*eof = -1;
					// fall into

				case jrd_req::req_proceed:
				case jrd_req::req_sync:
					if (resultSet)
					{
						if (resultSet->fetch(tdbb) && (request->req_flags & req_proc_fetch))
							*eof = -1;
						else
						{
							*eof = 0;
							delete resultSet;
							resultSet = NULL;
						}
					}

					impure->sta_state = 0;	// suspend node
					request->req_operation = jrd_req::req_sync;
					break;

				case jrd_req::req_unwind:
					delete resultSet;
					resultSet = NULL;
					break;
			}

			return CompoundStmtNode::execute(tdbb, request, exeState);
		}

	private:
		MessageNode* extInMessageNode;
		MessageNode* extOutMessageNode;
		MessageNode* intOutMessageNode;
		const ExtEngineManager::Procedure* procedure;
	};

	// External trigger node.
	class ExtTriggerNode : public TypedNode<StmtNode, StmtNode::TYPE_EXT_TRIGGER>
	{
	public:
		ExtTriggerNode(MemoryPool& pool, const ExtEngineManager::Trigger* aTrigger)
			: TypedNode<StmtNode, StmtNode::TYPE_EXT_TRIGGER>(pool),
			  trigger(aTrigger)
		{
		}

		string internalPrint(NodePrinter& printer) const
		{
			StmtNode::internalPrint(printer);
			return "ExtTriggerNode";
		}

		void genBlr(DsqlCompilerScratch* /*dsqlScratch*/)
		{
		}

		ExtTriggerNode* pass1(thread_db* /*tdbb*/, CompilerScratch* /*csb*/)
		{
			return this;
		}

		ExtTriggerNode* pass2(thread_db* /*tdbb*/, CompilerScratch* /*csb*/)
		{
			return this;
		}

		const StmtNode* execute(thread_db* tdbb, jrd_req* request, ExeState* /*exeState*/) const
		{
			if (request->req_operation == jrd_req::req_evaluate)
			{
				trigger->execute(tdbb, request, request->req_trigger_action,
					getRpb(request, 0), getRpb(request, 1));

				request->req_operation = jrd_req::req_return;
			}

			return parentStmt;
		}

	private:
		static record_param* getRpb(jrd_req* request, USHORT n)
		{
			return request->req_rpb.getCount() > n && request->req_rpb[n].rpb_number.isValid() ?
				&request->req_rpb[n] : NULL;
		}


	private:
		const ExtEngineManager::Trigger* trigger;
	};
}


namespace Jrd {

template <typename T> class ExtEngineManager::ContextManager
{
public:
	ContextManager(thread_db* tdbb, EngineAttachmentInfo* aAttInfo, T* obj,
				CallerName aCallerName = CallerName())
		: attInfo(aAttInfo),
		  attachment(tdbb->getAttachment()),
		  transaction(tdbb->getTransaction()),
		  charSet(attachment->att_charset),
		  attInUse(attachment->att_in_use),
		  traInUse(transaction ? transaction->tra_in_use : false)
	{
		// !!!!!  needs async lock to be safe
		attachment->att_in_use = true;

		if (transaction)
		{
			callerName = transaction->tra_caller_name;
			transaction->tra_caller_name = aCallerName;
			++transaction->tra_callback_count;
			transaction->tra_in_use = true;
		}

		attInfo->context->setTransaction(tdbb);

		setCharSet(tdbb, attInfo, obj);
	}

	ContextManager(thread_db* tdbb, EngineAttachmentInfo* aAttInfo, USHORT aCharSet,
				CallerName aCallerName = CallerName())
		: attInfo(aAttInfo),
		  attachment(tdbb->getAttachment()),
		  transaction(tdbb->getTransaction()),
		  charSet(attachment->att_charset),
		  attInUse(attachment->att_in_use),
		  traInUse(transaction ? transaction->tra_in_use : false)
	{
		attachment->att_charset = aCharSet;
		// !!!!!  needs async lock to be safe
		attachment->att_in_use = true;

		if (transaction)
		{
			callerName = transaction->tra_caller_name;
			transaction->tra_caller_name = aCallerName;
			++transaction->tra_callback_count;
			transaction->tra_in_use = true;
		}

		attInfo->context->setTransaction(tdbb);
	}

	~ContextManager()
	{
		if (transaction)
		{
			--transaction->tra_callback_count;
			transaction->tra_in_use = traInUse;
			transaction->tra_caller_name = callerName;
		}

		// !!!!!  needs async lock to be safe
		attachment->att_in_use = attInUse;
		attachment->att_charset = charSet;
	}

private:
	void setCharSet(thread_db* tdbb, EngineAttachmentInfo* attInfo, T* obj)
	{
		attachment->att_charset = attInfo->adminCharSet;

		if (!obj)
			return;

		char charSetName[MAX_SQL_IDENTIFIER_SIZE];

		{	// scope
			EngineCheckout cout(tdbb, FB_FUNCTION);

			FbLocalStatus status;
			obj->getCharSet(&status, attInfo->context, charSetName, MAX_SQL_IDENTIFIER_LEN);
			status.check();
			charSetName[MAX_SQL_IDENTIFIER_LEN] = '\0';
		}

		USHORT charSetId;

		if (!MET_get_char_coll_subtype(tdbb, &charSetId,
				reinterpret_cast<const UCHAR*>(charSetName), static_cast<USHORT>(strlen(charSetName))))
		{
			status_exception::raise(Arg::Gds(isc_charset_not_found) << Arg::Str(charSetName));
		}

		attachment->att_charset = charSetId;
	}

private:
	EngineAttachmentInfo* attInfo;
	Jrd::Attachment* attachment;
	jrd_tra* transaction;
	// These data members are to restore the original information.
	const USHORT charSet;
	const bool attInUse;
	const bool traInUse;
	CallerName callerName;
};


//---------------------


ExtEngineManager::ExternalContextImpl::ExternalContextImpl(thread_db* tdbb,
		IExternalEngine* aEngine)
	: engine(aEngine),
	  internalAttachment(tdbb->getAttachment()),
	  internalTransaction(NULL),
	  externalAttachment(NULL),
	  externalTransaction(NULL),
	  miscInfo(*internalAttachment->att_pool)
{
	//// TODO: admin rights

	clientCharSet = INTL_charset_lookup(tdbb, internalAttachment->att_client_charset)->getName();

	internalAttachment->getStable()->addRef();

	externalAttachment = MasterInterfacePtr()->registerAttachment
		(AutoPlugin<JProvider>(JProvider::getInstance()), internalAttachment->getInterface());
}

ExtEngineManager::ExternalContextImpl::~ExternalContextImpl()
{
	releaseTransaction();

	if (externalAttachment)
	{
		externalAttachment->release();
		externalAttachment = NULL;
	}
}

void ExtEngineManager::ExternalContextImpl::releaseTransaction()
{
	if (externalTransaction)
	{
		externalTransaction->release();
		externalTransaction = NULL;
	}

	internalTransaction = NULL;
}

void ExtEngineManager::ExternalContextImpl::setTransaction(thread_db* tdbb)
{
	ITransaction* newTransaction = tdbb->getTransaction() ? tdbb->getTransaction()->getInterface(true) : NULL;

	if (newTransaction == internalTransaction)
		return;

	releaseTransaction();
	fb_assert(!externalTransaction && !internalTransaction);

	if ((internalTransaction = newTransaction))
		externalTransaction = MasterInterfacePtr()->registerTransaction(externalAttachment, internalTransaction);
}

IMaster* ExtEngineManager::ExternalContextImpl::getMaster()
{
	MasterInterfacePtr master;
	return master;
}

IExternalEngine* ExtEngineManager::ExternalContextImpl::getEngine(CheckStatusWrapper* /*status*/)
{
	return engine;
}

Firebird::IAttachment* ExtEngineManager::ExternalContextImpl::getAttachment(
	CheckStatusWrapper* /*status*/)
{
	externalAttachment->addRef();
	return externalAttachment;
}

Firebird::ITransaction* ExtEngineManager::ExternalContextImpl::getTransaction(
	CheckStatusWrapper* /*status*/)
{
	externalTransaction->addRef();
	return externalTransaction;
}

const char* ExtEngineManager::ExternalContextImpl::getUserName()
{
	return internalAttachment->att_user ? internalAttachment->att_user->getUserName().c_str() : "";
}

const char* ExtEngineManager::ExternalContextImpl::getDatabaseName()
{
	return internalAttachment->att_database->dbb_database_name.c_str();
}

const char* ExtEngineManager::ExternalContextImpl::getClientCharSet()
{
	return clientCharSet.c_str();
}

int ExtEngineManager::ExternalContextImpl::obtainInfoCode()
{
	static AtomicCounter counter;
	return ++counter;
}

void* ExtEngineManager::ExternalContextImpl::getInfo(int code)
{
	void* value = NULL;
	miscInfo.get(code, value);
	return value;
}

void* ExtEngineManager::ExternalContextImpl::setInfo(int code, void* value)
{
	void* oldValue = getInfo(code);
	miscInfo.put(code, value);
	return oldValue;
}


//---------------------


ExtEngineManager::Function::Function(thread_db* tdbb, ExtEngineManager* aExtManager,
		IExternalEngine* aEngine, RoutineMetadata* aMetadata, IExternalFunction* aFunction,
		const Jrd::Function* aUdf)
	: extManager(aExtManager),
	  engine(aEngine),
	  metadata(aMetadata),
	  function(aFunction),
	  udf(aUdf),
	  database(tdbb->getDatabase())
{
}


ExtEngineManager::Function::~Function()
{
	//Database::Checkout dcoHolder(database);
	function->dispose();
}


void ExtEngineManager::Function::execute(thread_db* tdbb, UCHAR* inMsg, UCHAR* outMsg) const
{
	EngineAttachmentInfo* attInfo = extManager->getEngineAttachment(tdbb, engine);
	const MetaString& userName = udf->invoker ? udf->invoker->getUserName() : "";
	ContextManager<IExternalFunction> ctxManager(tdbb, attInfo, function,
		(udf->getName().package.isEmpty() ?
			CallerName(obj_udf, udf->getName().identifier, userName) :
			CallerName(obj_package_header, udf->getName().package, userName)));

	EngineCheckout cout(tdbb, FB_FUNCTION);

	FbLocalStatus status;
	function->execute(&status, attInfo->context, inMsg, outMsg);
	status.check();
}


//---------------------


ExtEngineManager::Procedure::Procedure(thread_db* tdbb, ExtEngineManager* aExtManager,
	    IExternalEngine* aEngine, RoutineMetadata* aMetadata, IExternalProcedure* aProcedure,
		const jrd_prc* aPrc)
	: extManager(aExtManager),
	  engine(aEngine),
	  metadata(aMetadata),
	  procedure(aProcedure),
	  prc(aPrc),
	  database(tdbb->getDatabase())
{
}


ExtEngineManager::Procedure::~Procedure()
{
	//Database::Checkout dcoHolder(database);
	procedure->dispose();
}


ExtEngineManager::ResultSet* ExtEngineManager::Procedure::open(thread_db* tdbb,
	UCHAR* inMsg, UCHAR* outMsg) const
{
	return FB_NEW_POOL(*tdbb->getDefaultPool()) ResultSet(tdbb, inMsg, outMsg, this);
}


//---------------------


ExtEngineManager::ResultSet::ResultSet(thread_db* tdbb, UCHAR* inMsg, UCHAR* outMsg,
		const ExtEngineManager::Procedure* aProcedure)
	: procedure(aProcedure),
	  attachment(tdbb->getAttachment()),
	  firstFetch(true)
{
	attInfo = procedure->extManager->getEngineAttachment(tdbb, procedure->engine);
	const MetaString& userName = procedure->prc->invoker ? procedure->prc->invoker->getUserName() : "";
	ContextManager<IExternalProcedure> ctxManager(tdbb, attInfo, procedure->procedure,
		(procedure->prc->getName().package.isEmpty() ?
			CallerName(obj_procedure, procedure->prc->getName().identifier, userName) :
			CallerName(obj_package_header, procedure->prc->getName().package, userName)));

	charSet = attachment->att_charset;

	EngineCheckout cout(tdbb, FB_FUNCTION);

	FbLocalStatus status;
	resultSet = procedure->procedure->open(&status, attInfo->context, inMsg, outMsg);
	status.check();
}


ExtEngineManager::ResultSet::~ResultSet()
{
	if (resultSet)
	{
		EngineCheckout cout(JRD_get_thread_data(), FB_FUNCTION);
		resultSet->dispose();
	}
}


bool ExtEngineManager::ResultSet::fetch(thread_db* tdbb)
{
	bool wasFirstFetch = firstFetch;
	firstFetch = false;

	if (!resultSet)
		return wasFirstFetch;

	const MetaString& userName = procedure->prc->invoker ? procedure->prc->invoker->getUserName() : "";
	ContextManager<IExternalProcedure> ctxManager(tdbb, attInfo, charSet,
		(procedure->prc->getName().package.isEmpty() ?
			CallerName(obj_procedure, procedure->prc->getName().identifier, userName) :
			CallerName(obj_package_header, procedure->prc->getName().package, userName)));

	EngineCheckout cout(tdbb, FB_FUNCTION);

	FbLocalStatus status;
	bool ret = resultSet->fetch(&status);
	status.check();

	return ret;
}


//---------------------


ExtEngineManager::Trigger::Trigger(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb,
			ExtEngineManager* aExtManager, IExternalEngine* aEngine, RoutineMetadata* aMetadata,
			IExternalTrigger* aTrigger, const Jrd::Trigger* aTrg)
	: computedStatements(pool),
	  extManager(aExtManager),
	  engine(aEngine),
	  metadata(aMetadata),
	  trigger(aTrigger),
	  trg(aTrg),
	  fieldsPos(pool),
	  varDecls(pool),
	  database(tdbb->getDatabase()),
	  computedCount(0)
{
	jrd_rel* relation = trg->relation;

	if (relation)
	{
		GenericMap<Left<MetaName, USHORT> > fieldsMap;

		for (FB_SIZE_T i = 0; i < relation->rel_fields->count(); ++i)
		{
			jrd_fld* field = (*relation->rel_fields)[i];

			if (field)
				fieldsMap.put(field->fld_name, (USHORT) i);
		}

		format = Routine::createFormat(pool, metadata->triggerFields, false);

		FbLocalStatus status;

		for (unsigned i = 0; i < format->fmt_count / 2u; ++i)
		{
			const char* fieldName = metadata->triggerFields->getField(&status, i);
			status.check();

			USHORT pos;

			if (!fieldsMap.get(fieldName, pos))
				fb_assert(false);
			else
				fieldsPos.add(pos);
		}

		setupComputedFields(tdbb, pool, csb);
	}
}


ExtEngineManager::Trigger::~Trigger()
{
	trigger->dispose();
}


void ExtEngineManager::Trigger::execute(thread_db* tdbb, jrd_req* request, unsigned action,
	record_param* oldRpb, record_param* newRpb) const
{
	EngineAttachmentInfo* attInfo = extManager->getEngineAttachment(tdbb, engine);
	const Nullable<bool>& ssDefiner = trg->ssDefiner.specified ? trg->ssDefiner :
		(trg->relation && trg->relation->rel_ss_definer.specified ? trg->relation->rel_ss_definer : Nullable<bool>() );
	const MetaString& userName = ssDefiner.specified && ssDefiner.value ? trg->relation->rel_owner_name.c_str() : "";
	ContextManager<IExternalTrigger> ctxManager(tdbb, attInfo, trigger,
		CallerName(obj_trigger, trg->name, userName));

	// ASF: Using Array instead of HalfStaticArray to not need to do alignment hacks here.
	Array<UCHAR> oldMsg;
	Array<UCHAR> newMsg;

	if (oldRpb)
		setValues(tdbb, request, oldMsg, oldRpb);

	if (newRpb)
		setValues(tdbb, request, newMsg, newRpb);

	{	// scope
		EngineCheckout cout(tdbb, FB_FUNCTION);

		FbLocalStatus status;
		trigger->execute(&status, attInfo->context, action,
			(oldMsg.hasData() ? oldMsg.begin() : NULL), (newMsg.hasData() ? newMsg.begin() : NULL));
		status.check();
	}

	if (newRpb)
	{
		// Move data back from the message to the record.

		Record* record = newRpb->rpb_record;
		UCHAR* p = newMsg.begin();

		for (unsigned i = 0; i < format->fmt_count / 2u; ++i)
		{
			USHORT fieldPos = fieldsPos[i];

			dsc target;
			bool readonly = !EVL_field(newRpb->rpb_relation, record, fieldPos, &target) &&
				target.dsc_address && !(target.dsc_flags & DSC_null);

			if (!readonly && target.dsc_address)
			{
				SSHORT* nullSource = (SSHORT*) (p + (IPTR) format->fmt_desc[i * 2 + 1].dsc_address);

				if (*nullSource == 0)
				{
					dsc source = format->fmt_desc[i * 2];
					source.dsc_address += (IPTR) p;
					MOV_move(tdbb, &source, &target);
					record->clearNull(fieldPos);
				}
				else
					record->setNull(fieldPos);
			}
		}
	}
}


void ExtEngineManager::Trigger::setupComputedFields(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb)
{
	SET_TDBB(tdbb);

	USHORT varId = 0;

	static_assert(NEW_CONTEXT_VALUE == OLD_CONTEXT_VALUE + 1, "OLD/NEW context assumption.");

	for (unsigned context = OLD_CONTEXT_VALUE; context <= NEW_CONTEXT_VALUE; ++context)	// OLD (0), NEW (1)
	{
		for (FB_SIZE_T i = 0; i < trg->relation->rel_fields->count(); ++i)
		{
			jrd_fld* field = (*trg->relation->rel_fields)[i];
			if (!field || !field->fld_computation)
				continue;

			if (context == OLD_CONTEXT_VALUE)	// count once
				++computedCount;

			DeclareVariableNode* declareNode = FB_NEW_POOL(pool) DeclareVariableNode(pool);
			declareNode->varId = varId;

			declareNode->varDesc = trg->relation->rel_current_format->fmt_desc[i];

			// For CHAR fields, change variable type to VARCHAR to avoid manual INTL adjustments for multi-byte.
			if (declareNode->varDesc.isText())
			{
				declareNode->varDesc.dsc_dtype = dtype_varying;
				declareNode->varDesc.dsc_length += sizeof(USHORT);
			}

			varDecls.push(declareNode);

			csb->csb_variables = vec<DeclareVariableNode*>::newVector(
				*tdbb->getDefaultPool(), csb->csb_variables, varId);

			ValueExprNode* exprNode = FB_NEW_POOL(*tdbb->getDefaultPool()) FieldNode(*tdbb->getDefaultPool(), context, i, true);

			VariableNode* varNode = FB_NEW_POOL(pool) VariableNode(pool);
			varNode->varId = varId;

			AssignmentNode* assignNode = FB_NEW_POOL(pool) AssignmentNode(pool);
			assignNode->asgnFrom = exprNode;
			assignNode->asgnTo = varNode;

			// Do not run the assignment for invalid RPBs (NEW in DELETE, OLD in INSERT).

			SLONG* actionPtr = FB_NEW_POOL(pool) SLONG(INFO_TYPE_TRIGGER_ACTION);

			LiteralNode* actionLiteral = FB_NEW_POOL(pool) LiteralNode(pool);
			actionLiteral->litDesc.dsc_dtype = dtype_long;
			actionLiteral->litDesc.dsc_length = sizeof(SLONG);
			actionLiteral->litDesc.dsc_scale = 0;
			actionLiteral->litDesc.dsc_sub_type = 0;
			actionLiteral->litDesc.dsc_address = reinterpret_cast<UCHAR*>(actionPtr);

			InternalInfoNode* internalInfo = FB_NEW_POOL(pool) InternalInfoNode(pool, actionLiteral);

			SLONG* comparePtr = FB_NEW_POOL(pool) SLONG(context == OLD_CONTEXT_VALUE ? TRIGGER_INSERT : TRIGGER_DELETE);

			LiteralNode* compareLiteral = FB_NEW_POOL(pool) LiteralNode(pool);
			compareLiteral->litDesc.dsc_dtype = dtype_long;
			compareLiteral->litDesc.dsc_length = sizeof(SLONG);
			compareLiteral->litDesc.dsc_scale = 0;
			compareLiteral->litDesc.dsc_sub_type = 0;
			compareLiteral->litDesc.dsc_address = reinterpret_cast<UCHAR*>(comparePtr);

			ComparativeBoolNode* cmp = FB_NEW_POOL(pool) ComparativeBoolNode(pool,
				blr_neq, internalInfo, compareLiteral);

			IfNode* ifNode = FB_NEW_POOL(pool) IfNode(pool);
			ifNode->condition = cmp;
			ifNode->trueAction = assignNode;

			computedStatements.add(declareNode);
			computedStatements.add(ifNode);

			++varId;
		}
	}
}


void ExtEngineManager::Trigger::setValues(thread_db* tdbb, jrd_req* request, Array<UCHAR>& msgBuffer,
	record_param* rpb) const
{
	if (!rpb || !rpb->rpb_record)
		return;

	UCHAR* p = msgBuffer.getBuffer(format->fmt_length);
	memset(p, 0, format->fmt_length);

	// NEW variables comes after OLD ones.
	USHORT computedVarId =
		request->req_rpb.getCount() >= NEW_CONTEXT_VALUE && rpb == &request->req_rpb[NEW_CONTEXT_VALUE] ?
			computedCount : 0;

	for (unsigned i = 0; i < format->fmt_count / 2u; ++i)
	{
		USHORT fieldPos = fieldsPos[i];
		SSHORT* nullTarget = (SSHORT*) (p + (IPTR) format->fmt_desc[i * 2 + 1].dsc_address);

		dsc source;
		dsc target = format->fmt_desc[i * 2];
		target.dsc_address += (IPTR) p;

		const jrd_fld* field = (*rpb->rpb_relation->rel_fields)[fieldPos];

		if (field->fld_computation)
		{
			const DeclareVariableNode* varDecl = varDecls[computedVarId++];
			impure_value* varImpure = request->getImpure<impure_value>(varDecl->impureOffset);

			*nullTarget = (varImpure->vlu_desc.dsc_flags & DSC_null) != 0 ? -1 : 0;

			if (*nullTarget == 0)
				MOV_move(tdbb, &varImpure->vlu_desc, &target);
		}
		else
		{
			if (!EVL_field(rpb->rpb_relation, rpb->rpb_record, fieldPos, &source))
				source.dsc_flags |= DSC_null;

			*nullTarget = (source.dsc_flags & DSC_null) != 0 ? -1 : 0;

			if (*nullTarget == 0)
				MOV_move(tdbb, &source, &target);
		}
	}
}


//---------------------


ExtEngineManager::~ExtEngineManager()
{
	fb_assert(enginesAttachments.count() == 0);
/*
AP: Commented out this code due to later AV.

When engine is released, it does dlclose() plugin module (libudr_engine.so),
but that module is not actually unloaded - because UDR module (libudrcpp_example.so) is using
symbols from plugin module, therefore raising plugin module's reference count.
UDR module can be unloaded only from plugin module's global variable (ModuleMap modules) dtor,
which is not called as long as plugin module is not unloaded. As the result all this will be
unloaded only on program exit, causing at that moment AV if this code is active: it happens that
~ModuleMap dlcloses itself.

	PluginManagerInterfacePtr pi;

	EnginesMap::Accessor accessor(&engines);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext())
	{
		IExternalEngine* engine = accessor.current()->second;
		pi->releasePlugin(engine);
	}
 */
}


//---------------------


namespace
{
	class SystemEngine : public StdPlugin<IExternalEngineImpl<SystemEngine, ThrowStatusExceptionWrapper> >
	{
	public:
		explicit SystemEngine()
		{
		}

		int release() override
		{
			if (--refCounter == 0)
			{
				delete this;
				return 0;
			}

			return 1;
		}

	public:
		void open(ThrowStatusExceptionWrapper* status, IExternalContext* context,
			char* name, unsigned nameSize) override
		{
		}

		void openAttachment(ThrowStatusExceptionWrapper* status, IExternalContext* context) override
		{
		}

		void closeAttachment(ThrowStatusExceptionWrapper* status, IExternalContext* context) override
		{
		}

		IExternalFunction* makeFunction(ThrowStatusExceptionWrapper* status, IExternalContext* context,
			IRoutineMetadata* metadata, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder) override
		{
			const char* packageName = metadata->getPackage(status);
			const char* routineName = metadata->getName(status);

			for (auto& package : SystemPackage::get())
			{
				if (strcmp(package.name, packageName) == 0)
				{
					for (auto& routine : package.functions)
					{
						if (strcmp(routine.name, routineName) == 0)
							return routine.factory(status, context, metadata, inBuilder, outBuilder);
					}
				}
			}

			fb_assert(false);
			return nullptr;
		}

		IExternalProcedure* makeProcedure(ThrowStatusExceptionWrapper* status, IExternalContext* context,
			IRoutineMetadata* metadata, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder) override
		{
			const char* packageName = metadata->getPackage(status);
			const char* routineName = metadata->getName(status);

			for (auto& package : SystemPackage::get())
			{
				if (strcmp(package.name, packageName) == 0)
				{
					for (auto& routine : package.procedures)
					{
						if (strcmp(routine.name, routineName) == 0)
							return routine.factory(status, context, metadata, inBuilder, outBuilder);
					}
				}
			}

			fb_assert(false);
			return nullptr;
		}

		IExternalTrigger* makeTrigger(ThrowStatusExceptionWrapper* status, IExternalContext* context,
			IRoutineMetadata* metadata, IMetadataBuilder* fieldsBuilder) override
		{
			fb_assert(false);
			return nullptr;
		}

	public:
		static SystemEngine* INSTANCE;
	};

	SystemEngine* SystemEngine::INSTANCE = nullptr;
}


//---------------------


void ExtEngineManager::initialize()
{
	SystemEngine::INSTANCE = FB_NEW SystemEngine();
}


ExtEngineManager::ExtEngineManager(MemoryPool& p)
	: PermanentStorage(p),
	  engines(p),
	  enginesAttachments(p)
{
	engines.put("SYSTEM", SystemEngine::INSTANCE);
}


void ExtEngineManager::closeAttachment(thread_db* tdbb, Attachment* attachment)
{
	Array<IExternalEngine*> enginesCopy;

	{	// scope
		ReadLockGuard readGuard(enginesLock, FB_FUNCTION);

		EnginesMap::Accessor accessor(&engines);
		for (bool found = accessor.getFirst(); found; found = accessor.getNext())
			enginesCopy.add(accessor.current()->second);
	}

	RefDeb(DEB_RLS_JATT, "ExtEngineManager::closeAttachment");
	EngineCheckout cout(tdbb, FB_FUNCTION, true);

	for (Array<IExternalEngine*>::iterator i = enginesCopy.begin(); i != enginesCopy.end(); ++i)
	{
		IExternalEngine* engine = *i;
		EngineAttachmentInfo* attInfo = getEngineAttachment(tdbb, engine, true);

		if (attInfo)
		{
			{	// scope
				ContextManager<IExternalFunction> ctxManager(tdbb, attInfo, attInfo->adminCharSet);
				FbLocalStatus status;
				engine->closeAttachment(&status, attInfo->context);	//// FIXME: log status
				engine->release();
			}

			delete attInfo;
		}
	}
}


void ExtEngineManager::makeFunction(thread_db* tdbb, CompilerScratch* csb, Jrd::Function* udf,
	const MetaName& engine, const string& entryPoint, const string& body)
{
	string entryPointTrimmed = entryPoint;
	entryPointTrimmed.trim();

	EngineAttachmentInfo* attInfo = getEngineAttachment(tdbb, engine);
	const MetaString& userName = udf->invoker ? udf->invoker->getUserName() : "";
	ContextManager<IExternalFunction> ctxManager(tdbb, attInfo, attInfo->adminCharSet,
		(udf->getName().package.isEmpty() ?
			CallerName(obj_udf, udf->getName().identifier, userName) :
			CallerName(obj_package_header, udf->getName().package, userName)));

	///MemoryPool& pool = *tdbb->getDefaultPool();
	MemoryPool& pool = *getDefaultMemoryPool();

	AutoPtr<RoutineMetadata> metadata(FB_NEW_POOL(pool) RoutineMetadata(pool));
	metadata->package = udf->getName().package;
	metadata->name = udf->getName().identifier;
	metadata->entryPoint = entryPointTrimmed;
	metadata->body = body;
	metadata->inputParameters = Routine::createMetadata(udf->getInputFields(), true);
	metadata->outputParameters = Routine::createMetadata(udf->getOutputFields(), true);

	FbLocalStatus status;

	RefPtr<IMetadataBuilder> inBuilder(REF_NO_INCR, metadata->inputParameters->getBuilder(&status));
	status.check();

	RefPtr<IMetadataBuilder> outBuilder(REF_NO_INCR, metadata->outputParameters->getBuilder(&status));
	status.check();

	IExternalFunction* externalFunction;
	RefPtr<IMessageMetadata> extInputParameters, extOutputParameters;

	{	// scope
		EngineCheckout cout(tdbb, FB_FUNCTION);

		externalFunction = attInfo->engine->makeFunction(&status, attInfo->context, metadata,
			inBuilder, outBuilder);
		status.check();

		if (!externalFunction)
		{
			status_exception::raise(
				Arg::Gds(isc_eem_func_not_returned) << udf->getName().toString() << engine);
		}

		extInputParameters = inBuilder->getMetadata(&status);
		status.check();

		extOutputParameters = outBuilder->getMetadata(&status);
		status.check();
	}

	udf->setInputFormat(Routine::createFormat(pool, metadata->inputParameters, false));
	udf->setOutputFormat(Routine::createFormat(pool, metadata->outputParameters, true));

	const Format* extInputFormat = Routine::createFormat(pool, extInputParameters, false);
	const Format* extOutputFormat = Routine::createFormat(pool, extOutputParameters, true);

	try
	{
		udf->fun_external = FB_NEW_POOL(getPool()) Function(tdbb, this, attInfo->engine,
			metadata.release(), externalFunction, udf);

		CompoundStmtNode* mainNode = FB_NEW_POOL(getPool()) CompoundStmtNode(getPool());

		IntMessageNode* intInMessageNode = udf->getInputFields().hasData() ?
			FB_NEW_POOL(getPool()) IntMessageNode(tdbb, getPool(), csb, 0,
				udf->getInputFields(), udf->getInputFormat()) :
			NULL;
		ExtMessageNode* extInMessageNode = NULL;

		if (intInMessageNode)
		{
			mainNode->statements.add(intInMessageNode);

			extInMessageNode = FB_NEW_POOL(getPool()) ExtMessageNode(tdbb, getPool(), csb, 2, extInputFormat);
			mainNode->statements.add(extInMessageNode);
		}

		IntMessageNode* intOutMessageNode = FB_NEW_POOL(getPool()) IntMessageNode(tdbb, getPool(), csb, 1,
			udf->getOutputFields(), udf->getOutputFormat());
		mainNode->statements.add(intOutMessageNode);

		ExtMessageNode* extOutMessageNode = FB_NEW_POOL(getPool()) ExtMessageNode(tdbb, getPool(), csb, 3,
			extOutputFormat);
		mainNode->statements.add(extOutMessageNode);

		// Initialize the output fields into the external message.
		InitOutputNode* initOutputNode = FB_NEW_POOL(getPool()) InitOutputNode(
			tdbb, getPool(), csb, udf->getOutputFields(), extOutMessageNode);
		mainNode->statements.add(initOutputNode);

		if (intInMessageNode)
		{
			ReceiveNode* receiveNode = intInMessageNode ? FB_NEW_POOL(getPool()) ReceiveNode(getPool()) : NULL;
			receiveNode->message = intInMessageNode;
			receiveNode->statement = FB_NEW_POOL(getPool()) MessageMoverNode(
				getPool(), intInMessageNode, extInMessageNode);
			mainNode->statements.add(receiveNode);
		}

		ExtFunctionNode* extFunctionNode = FB_NEW_POOL(getPool()) ExtFunctionNode(getPool(),
			extInMessageNode, extOutMessageNode, udf->fun_external);
		mainNode->statements.add(extFunctionNode);
		extFunctionNode->message = intOutMessageNode;
		extFunctionNode->statement = FB_NEW_POOL(getPool()) MessageMoverNode(
			getPool(), extOutMessageNode, intOutMessageNode);

		JrdStatement* statement = udf->getStatement();
		PAR_preparsed_node(tdbb, NULL, mainNode, NULL, &csb, &statement, false, 0);
		udf->setStatement(statement);
	}
	catch (...)
	{
		EngineCheckout cout(tdbb, FB_FUNCTION);
		externalFunction->dispose();
		throw;
	}
}


void ExtEngineManager::makeProcedure(thread_db* tdbb, CompilerScratch* csb, jrd_prc* prc,
	const MetaName& engine, const string& entryPoint, const string& body)
{
	string entryPointTrimmed = entryPoint;
	entryPointTrimmed.trim();

	EngineAttachmentInfo* attInfo = getEngineAttachment(tdbb, engine);
	const MetaString& userName = prc->invoker ? prc->invoker->getUserName() : "";
	ContextManager<IExternalProcedure> ctxManager(tdbb, attInfo, attInfo->adminCharSet,
		(prc->getName().package.isEmpty() ?
			CallerName(obj_procedure, prc->getName().identifier, userName) :
			CallerName(obj_package_header, prc->getName().package, userName)));

	///MemoryPool& pool = *tdbb->getDefaultPool();
	MemoryPool& pool = *getDefaultMemoryPool();

	AutoPtr<RoutineMetadata> metadata(FB_NEW_POOL(pool) RoutineMetadata(pool));
	metadata->package = prc->getName().package;
	metadata->name = prc->getName().identifier;
	metadata->entryPoint = entryPointTrimmed;
	metadata->body = body;
	metadata->inputParameters = Routine::createMetadata(prc->getInputFields(), true);
	metadata->outputParameters = Routine::createMetadata(prc->getOutputFields(), true);

	FbLocalStatus status;

	RefPtr<IMetadataBuilder> inBuilder(REF_NO_INCR, metadata->inputParameters->getBuilder(&status));
	status.check();

	RefPtr<IMetadataBuilder> outBuilder(REF_NO_INCR, metadata->outputParameters->getBuilder(&status));
	status.check();

	IExternalProcedure* externalProcedure;
	RefPtr<IMessageMetadata> extInputParameters, extOutputParameters;

	{	// scope
		EngineCheckout cout(tdbb, FB_FUNCTION);

		externalProcedure = attInfo->engine->makeProcedure(&status, attInfo->context, metadata,
			inBuilder, outBuilder);
		status.check();

		if (!externalProcedure)
		{
			status_exception::raise(
				Arg::Gds(isc_eem_proc_not_returned) <<
					prc->getName().toString() << engine);
		}

		extInputParameters = inBuilder->getMetadata(&status);
		status.check();

		extOutputParameters = outBuilder->getMetadata(&status);
		status.check();
	}

	prc->setInputFormat(Routine::createFormat(pool, metadata->inputParameters, false));
	prc->setOutputFormat(Routine::createFormat(pool, metadata->outputParameters, true));

	const Format* extInputFormat = Routine::createFormat(pool, extInputParameters, false);
	const Format* extOutputFormat = Routine::createFormat(pool, extOutputParameters, true);

	try
	{
		prc->setExternal(FB_NEW_POOL(getPool()) Procedure(tdbb, this, attInfo->engine,
			metadata.release(), externalProcedure, prc));

		CompoundStmtNode* mainNode = FB_NEW_POOL(getPool()) CompoundStmtNode(getPool());

		IntMessageNode* intInMessageNode = prc->getInputFields().hasData() ?
			FB_NEW_POOL(getPool()) IntMessageNode(tdbb, getPool(), csb, 0,
				prc->getInputFields(), prc->getInputFormat()) :
			NULL;
		ExtMessageNode* extInMessageNode = NULL;

		if (intInMessageNode)
		{
			mainNode->statements.add(intInMessageNode);

			extInMessageNode = FB_NEW_POOL(getPool()) ExtMessageNode(tdbb, getPool(), csb, 2, extInputFormat);
			mainNode->statements.add(extInMessageNode);
		}

		IntMessageNode* intOutMessageNode = FB_NEW_POOL(getPool()) IntMessageNode(tdbb, getPool(), csb, 1,
			prc->getOutputFields(), prc->getOutputFormat());
		mainNode->statements.add(intOutMessageNode);

		ExtMessageNode* extOutMessageNode = FB_NEW_POOL(getPool()) ExtMessageNode(tdbb, getPool(),
			csb, 3, extOutputFormat);
		mainNode->statements.add(extOutMessageNode);

		// Initialize the output fields into the external message.
		InitOutputNode* initOutputNode = FB_NEW_POOL(getPool()) InitOutputNode(
			tdbb, getPool(), csb, prc->getOutputFields(), extOutMessageNode);
		mainNode->statements.add(initOutputNode);

		ReceiveNode* receiveNode = intInMessageNode ?
			FB_NEW_POOL(getPool()) ReceiveNode(getPool()) : NULL;

		if (intInMessageNode)
		{
			CompoundStmtNode* receiveSubStatement = FB_NEW_POOL(getPool()) CompoundStmtNode(getPool());
			receiveSubStatement->statements.add(FB_NEW_POOL(getPool()) MessageMoverNode(
				getPool(), intInMessageNode, extInMessageNode));
			receiveSubStatement->statements.add(FB_NEW_POOL(getPool()) StallNode(getPool()));

			receiveNode->statement = receiveSubStatement;
			receiveNode->message = intInMessageNode;

			mainNode->statements.add(receiveNode);
		}
		else
			mainNode->statements.add(FB_NEW_POOL(getPool()) StallNode(getPool()));

		ExtProcedureNode* extProcedureNode = FB_NEW_POOL(getPool()) ExtProcedureNode(getPool(),
			extInMessageNode, extOutMessageNode, intOutMessageNode, prc->getExternal());
		mainNode->statements.add(extProcedureNode);

		JrdStatement* statement = prc->getStatement();
		PAR_preparsed_node(tdbb, NULL, mainNode, NULL, &csb, &statement, false, 0);
		prc->setStatement(statement);
	}
	catch (...)
	{
		EngineCheckout cout(tdbb, FB_FUNCTION);
		externalProcedure->dispose();
		throw;
	}
}


void ExtEngineManager::makeTrigger(thread_db* tdbb, CompilerScratch* csb, Jrd::Trigger* trg,
	const MetaName& engine, const string& entryPoint, const string& body,
	unsigned type)
{
	string entryPointTrimmed = entryPoint;
	entryPointTrimmed.trim();

	EngineAttachmentInfo* attInfo = getEngineAttachment(tdbb, engine);
	const MetaString& userName = trg->ssDefiner.specified && trg->ssDefiner.value ? trg->owner.c_str() : "";
	ContextManager<IExternalTrigger> ctxManager(tdbb, attInfo, attInfo->adminCharSet,
		CallerName(obj_trigger, trg->name, userName));

	///MemoryPool& pool = *tdbb->getDefaultPool();
	MemoryPool& pool = *getDefaultMemoryPool();

	AutoPtr<RoutineMetadata> metadata(FB_NEW_POOL(pool) RoutineMetadata(pool));
	metadata->name = trg->name;
	metadata->entryPoint = entryPointTrimmed;
	metadata->body = body;
	metadata->triggerType = type;

	jrd_rel* relation = trg->relation;

	if (relation)
	{
		metadata->triggerTable = relation->rel_name;

		MsgMetadata* fieldsMsg = FB_NEW MsgMetadata;
		metadata->triggerFields = fieldsMsg;

		Format* relFormat = relation->rel_current_format;

		for (FB_SIZE_T i = 0; i < relation->rel_fields->count(); ++i)
		{
			jrd_fld* field = (*relation->rel_fields)[i];
			if (field)
			{
				dsc d(relFormat->fmt_desc[i]);
				fieldsMsg->addItem(field->fld_name, !field->fld_not_null, d);
			}
		}
	}

	FbLocalStatus status;

	RefPtr<IMetadataBuilder> fieldsBuilder(REF_NO_INCR, relation ?
		metadata->triggerFields->getBuilder(&status) : NULL);
	if (relation)
	{
		status.check();
	}

	IExternalTrigger* externalTrigger;

	{	// scope
		EngineCheckout cout(tdbb, FB_FUNCTION);

		FbLocalStatus status;
		externalTrigger = attInfo->engine->makeTrigger(&status, attInfo->context, metadata,
			fieldsBuilder);
		status.check();

		if (!externalTrigger)
		{
			status_exception::raise(
				Arg::Gds(isc_eem_trig_not_returned) << trg->name << engine);
		}

		if (relation)
		{
			metadata->triggerFields = fieldsBuilder->getMetadata(&status);
			status.check();
		}
	}

	try
	{
		trg->extTrigger = FB_NEW_POOL(getPool()) Trigger(tdbb, pool, csb, this, attInfo->engine,
			metadata.release(), externalTrigger, trg);

		CompoundStmtNode* mainNode = FB_NEW_POOL(getPool()) CompoundStmtNode(getPool());
		mainNode->statements.append(trg->extTrigger->computedStatements);

		ExtTriggerNode* extTriggerNode = FB_NEW_POOL(getPool()) ExtTriggerNode(getPool(),
			trg->extTrigger);
		mainNode->statements.add(extTriggerNode);

		PAR_preparsed_node(tdbb, trg->relation, mainNode, NULL, &csb, &trg->statement, true, 0);
	}
	catch (...)
	{
		EngineCheckout cout(tdbb, FB_FUNCTION);
		externalTrigger->dispose();
		throw;
	}
}


IExternalEngine* ExtEngineManager::getEngine(thread_db* tdbb, const MetaName& name)
{
	ReadLockGuard readGuard(enginesLock, FB_FUNCTION);
	IExternalEngine* engine = NULL;

	if (!engines.get(name, engine))
	{
		readGuard.release();
		WriteLockGuard writeGuard(enginesLock, FB_FUNCTION);

		if (!engines.get(name, engine))
		{
			GetPlugins<IExternalEngine> engineControl(IPluginManager::TYPE_EXTERNAL_ENGINE, name.c_str());

			if (engineControl.hasData())
			{
				EngineAttachment key(NULL, NULL);
				AutoPtr<EngineAttachmentInfo> attInfo;

				try
				{
					EngineCheckout cout(tdbb, FB_FUNCTION);

					engine = engineControl.plugin();
					if (engine)
					{
						Attachment::SyncGuard attGuard(tdbb->getAttachment(), FB_FUNCTION);

						key = EngineAttachment(engine, tdbb->getAttachment());
						attInfo = FB_NEW_POOL(getPool()) EngineAttachmentInfo();
						attInfo->engine = engine;
						attInfo->context = FB_NEW_POOL(getPool()) ExternalContextImpl(tdbb, engine);

						setupAdminCharSet(tdbb, engine, attInfo);

						ContextManager<IExternalFunction> ctxManager(tdbb, attInfo, attInfo->adminCharSet);
						FbLocalStatus status;
						engine->openAttachment(&status, attInfo->context);	//// FIXME: log status
					}
				}
				catch (...)
				{
					if (engine)
					{
						PluginManagerInterfacePtr()->releasePlugin(engine);
					}

					throw;
				}

				if (engine)
				{
					engine->addRef();
					engines.put(name, engine);
					enginesAttachments.put(key, attInfo);
					attInfo.release();
				}
			}
		}
	}

	if (!engine)
	{
		status_exception::raise(Arg::Gds(isc_eem_engine_notfound) << name);
	}

	return engine;
}


ExtEngineManager::EngineAttachmentInfo* ExtEngineManager::getEngineAttachment(
	thread_db* tdbb, const MetaName& name)
{
	IExternalEngine* engine = getEngine(tdbb, name);
	return getEngineAttachment(tdbb, engine);
}


ExtEngineManager::EngineAttachmentInfo* ExtEngineManager::getEngineAttachment(
	thread_db* tdbb, IExternalEngine* engine, bool closing)
{
	EngineAttachment key(engine, tdbb->getAttachment());
	EngineAttachmentInfo* attInfo = NULL;

	ReadLockGuard readGuard(&enginesLock, FB_FUNCTION);

	if (!enginesAttachments.get(key, attInfo) && !closing)
	{
		readGuard.release();
		WriteLockGuard writeGuard(enginesLock, FB_FUNCTION);

		if (!enginesAttachments.get(key, attInfo))
		{
			attInfo = FB_NEW_POOL(getPool()) EngineAttachmentInfo();
			attInfo->engine = engine;
			attInfo->context = FB_NEW_POOL(getPool()) ExternalContextImpl(tdbb, engine);

			setupAdminCharSet(tdbb, engine, attInfo);

			enginesAttachments.put(key, attInfo);

			ContextManager<IExternalFunction> ctxManager(tdbb, attInfo, attInfo->adminCharSet);
			EngineCheckout cout(tdbb, FB_FUNCTION);
			FbLocalStatus status;
			engine->openAttachment(&status, attInfo->context);	//// FIXME: log status
		}

		return attInfo;
	}

	if (closing && attInfo)
	{
		readGuard.release();
		WriteLockGuard writeGuard(enginesLock, FB_FUNCTION);
		enginesAttachments.remove(key);
	}

	return attInfo;
}


void ExtEngineManager::setupAdminCharSet(thread_db* tdbb, IExternalEngine* engine,
	EngineAttachmentInfo* attInfo)
{
	ContextManager<IExternalFunction> ctxManager(tdbb, attInfo, CS_UTF8);

	char charSetName[MAX_SQL_IDENTIFIER_SIZE] = "NONE";

	FbLocalStatus status;
	engine->open(&status, attInfo->context, charSetName, MAX_SQL_IDENTIFIER_LEN);
	status.check();

	charSetName[MAX_SQL_IDENTIFIER_LEN] = '\0';

	if (!MET_get_char_coll_subtype(tdbb, &attInfo->adminCharSet,
			reinterpret_cast<const UCHAR*>(charSetName),
			static_cast<USHORT>(strlen(charSetName))))
	{
		status_exception::raise(
			Arg::Gds(isc_charset_not_found) <<
			Arg::Str(charSetName));
	}
}


}	// namespace Jrd
