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
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include "../jrd/Statement.h"
#include "../jrd/Attachment.h"
#include "../jrd/intl_classes.h"
#include "../jrd/acl.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "../jrd/val.h"
#include "../jrd/align.h"
#include "../dsql/Nodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/Function.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/lck.h"
#include "../jrd/exe_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/Collation.h"
#include "../jrd/met.h"
#include "../jrd/recsrc/Cursor.h"
#include "../common/classes/auto.h"

using namespace Firebird;
using namespace Jrd;


template <typename T> static void makeSubRoutines(thread_db* tdbb, Statement* statement,
	CompilerScratch* csb, T& subs);


ULONG CompilerScratch::allocImpure(ULONG align, ULONG size)
{
	const ULONG offset = FB_ALIGN(csb_impure, align);

	if (offset + size > Statement::MAX_REQUEST_SIZE)
		IBERROR(226);	// msg 226: request size limit exceeded

	csb_impure = offset + size;

	return offset;
}


// Start to turn a parsed scratch into a statement. This is completed by makeStatement.
Statement::Statement(thread_db* tdbb, MemoryPool* p, CompilerScratch* csb)
	: pool(p),
	  rpbsSetup(*p),
	  requests(),
	  externalList(*p),
	  accessList(*p),
	  triggerName(*p),
	  triggerInvoker(NULL),
	  parentStatement(NULL),
	  subStatements(*p),
	  fors(*p),
	  localTables(*p),
	  invariants(*p),
	  blr(*p),
	  mapFieldInfo(*p),
	  resources(nullptr),
	  messages(*p, 2) // Most statements have two messages, preallocate space for them
{
	if (csb->csb_resources)
	{
		fb_assert(csb->csb_resources->getPool() == pool);
		resources = csb->csb_resources;
	}

	try
	{
		makeSubRoutines(tdbb, this, csb, csb->subProcedures);
		makeSubRoutines(tdbb, this, csb, csb->subFunctions);

		topNode = (csb->csb_node && csb->csb_node->getKind() == DmlNode::KIND_STATEMENT) ?
			static_cast<StmtNode*>(csb->csb_node) : NULL;

		accessList = csb->csb_access;
		csb->csb_access.clear();

		externalList = csb->csb_external;
		csb->csb_external.clear();

		mapFieldInfo.takeOwnership(csb->csb_map_field_info);

		impureSize = csb->csb_impure;

		//if (csb->csb_g_flags & csb_blr_version4)
		//	flags |= FLAG_VERSION4;
		blrVersion = csb->blrVersion;

		// make a vector of all used RSEs
		fors = csb->csb_fors;
		csb->csb_fors.clear();

		localTables = csb->csb_localTables;
		csb->csb_localTables.clear();

		// make a vector of all invariant-type nodes, so that we will
		// be able to easily reinitialize them when we restart the request
		invariants.join(csb->csb_invariants);
		csb->csb_invariants.clear();

		rpbsSetup.grow(csb->csb_n_stream);

		auto tail = csb->csb_rpt.begin();
		const auto* const streams_end = tail + csb->csb_n_stream;

		for (auto rpb = rpbsSetup.begin(); tail < streams_end; ++rpb, ++tail)
		{
			// fetch input stream for update if all booleans matched against indices
			if ((tail->csb_flags & csb_update) && !(tail->csb_flags & csb_unmatched))
				 rpb->rpb_stream_flags |= RPB_s_update;

			// if no fields are referenced and this stream is not intended for update,
			// mark the stream as not requiring record's data
			if (!tail->csb_fields && !(tail->csb_flags & csb_update))
				 rpb->rpb_stream_flags |= RPB_s_no_data;

			if (tail->csb_flags & csb_unstable)
				rpb->rpb_stream_flags |= RPB_s_unstable;

			if (tail->csb_flags & csb_skip_locked)
				rpb->rpb_stream_flags |= RPB_s_skipLocked;

			rpb->rpb_relation = tail->csb_relation;
			if (rpb->rpb_relation())
				fb_assert(resources->relations.knownResource(rpb->rpb_relation()));
//				rpb->rpb_relation = resources->relations.registerResource(rpb->rpb_relation());

			delete tail->csb_fields;
			tail->csb_fields = NULL;
		}

		// versioned metadata support
		if (csb->csb_g_flags & csb_internal)
			flags |= FLAG_INTERNAL;
		loadResources(tdbb, nullptr, false);

		messages.grow(csb->csb_rpt.getCount());
		for (decltype(messages)::size_type i = 0; i < csb->csb_rpt.getCount(); ++i)
		{
			if (auto message = csb->csb_rpt[i].csb_message)
			{
				// When outer messages are mapped to inner just pointers are assigned so they keep original numbers inside.
				// That's why this assert is commented out.
				//fb_assert(i == message->messageNumber);
				if (messages.getCount() <= i)
				{
					messages.grow(i + 1);
				}
				messages[i] = message;
			}
		}

		if (csb->csb_variables)
			csb->csb_variables->clear();

		csb->csb_current_nodes.free();
		csb->csb_current_for_nodes.free();
		csb->csb_computing_fields.free();
		csb->csb_variables_used_in_subroutines.free();
		csb->csb_dbg_info.reset();
		csb->csb_map_item_info.clear();
		csb->csb_message_pad.clear();
		csb->subFunctions.clear();
		csb->subProcedures.clear();
		csb->outerMessagesMap.clear();
		csb->outerVarsMap.clear();
		csb->csb_rpt.free();
		csb->csb_resources = nullptr;
	}
	catch (Exception&)
	{
		for (Statement** subStatement = subStatements.begin();
			 subStatement != subStatements.end();
			 ++subStatement)
		{
			(*subStatement)->release(tdbb);
		}

		throw;
	}
}

void Statement::loadResources(thread_db* tdbb, Request* req, bool withLock)
{
	auto* mdc = MetadataCache::get(tdbb);
	const MdcVersion frontVersion = mdc->getFrontVersion();
	auto* tra = tdbb->getTransaction();
	bool ddl = tra && tra->isDdl();
	if (ddl)
		withLock = false;

	if (ddl || (!latestVer) || (latestVer->version != frontVersion))
	{
		MutexEnsureUnlock guard(lvMutex, FB_FUNCTION);

		if (withLock)
		{
			fb_assert(req);
			guard.enter();
		}

		if (ddl || (!latestVer) || (latestVer->version != frontVersion))
		{
			const FB_SIZE_T resourceCount = latestVer ? latestVer->getCapacity() :
				resources->charSets.getCount() + resources->relations.getCount() + resources->procedures.getCount() +
				resources->functions.getCount() + resources->triggers.getCount();
			AutoPtr<VersionedObjects> newVer = FB_NEW_RPT(*pool, resourceCount) VersionedObjects(resourceCount);

			MetadataCache::Version ver(mdc);
			do
			{
				resources->transfer(tdbb, newVer, flags & FLAG_INTERNAL);
			} while (!ver.isStable());
			newVer->version = ver.get();

			if (ddl)
			{
				if (req)
					req->setResources(newVer.release(), rpbsSetup);
				return;
			}
			latestVer = newVer.release();
		}
	}

	if (req && req->getResources() != latestVer)
		req->setResources(latestVer, rpbsSetup);
}

void Request::setResources(VersionedObjects* r, RecordParameters& rpbsSetup)
{
	req_resources = r;

	// setup correct jrd_rel pointers in rpbs
	req_rpb.grow(rpbsSetup.getCount());
	fb_assert(req_rpb.getCount() == rpbsSetup.getCount());
	for (FB_SIZE_T n = 0; n < rpbsSetup.getCount(); ++n)
	{
		req_rpb[n] = rpbsSetup[n];
		req_rpb[n].rpb_relation = rpbsSetup[n].rpb_relation(r);
		//fb_assert(req_rpb[n].rpb_relation);
	}
}


// Turn a parsed scratch into a statement.
Statement* Statement::makeStatement(thread_db* tdbb, CompilerScratch* csb, bool internalFlag,
	std::function<void ()> beforeCsbRelease)
{
	DEV_BLKCHK(csb, type_csb);
	SET_TDBB(tdbb);

	const auto dbb = tdbb->getDatabase();
	fb_assert(dbb);

#if defined(DEV_BUILD) && defined(DEBUG_LOST_POOLS)
	MemoryPool* defPool = tdbb->getDefaultPool();
	{ // scope
		Firebird::SyncLockGuard guard(&dbb->dbb_pools_sync, Firebird::SYNC_SHARED, "Statement::makeStatement");
		for (FB_SIZE_T i = 1; i < dbb->dbb_pools.getCount(); ++i)
		{
			if (dbb->dbb_pools[i] == defPool)
				goto found;
		}
	}
	fb_assert(!"wrong pool in makeStatement");
found:
#endif

	const auto attachment = tdbb->getAttachment();

	const auto old_request = tdbb->getRequest();
	tdbb->setRequest(nullptr);

	Statement* statement = nullptr;

	try
	{
		// Once any expansion required has been done, make a pass to assign offsets
		// into the impure area and throw away any unnecessary crude. Execution
		// optimizations can be performed here.

		DmlNode::doPass1(tdbb, csb, &csb->csb_node);

		// CVC: I'm going to preallocate the map before the loop to avoid alloc/dealloc calls.
		StreamMap localMap;
		StreamType* const map = localMap.getBuffer(STREAM_MAP_LENGTH);

		// Copy and compile (pass1) domains DEFAULT and constraints.
		MapFieldInfo::Accessor accessor(&csb->csb_map_field_info);

		for (bool found = accessor.getFirst(); found; found = accessor.getNext())
		{
			FieldInfo& fieldInfo = accessor.current()->second;

			AutoSetRestore<USHORT> autoRemapVariable(&csb->csb_remap_variable,
				(csb->csb_variables ? csb->csb_variables->count() : 0) + 1);

			fieldInfo.defaultValue = NodeCopier::copy(tdbb, csb, fieldInfo.defaultValue, map);

			csb->csb_remap_variable = (csb->csb_variables ? csb->csb_variables->count() : 0) + 1;

			if (fieldInfo.validationExpr)
			{
				NodeCopier copier(csb->csb_pool, csb, map);
				fieldInfo.validationExpr = copier.copy(tdbb, fieldInfo.validationExpr);
			}

			DmlNode::doPass1(tdbb, csb, fieldInfo.defaultValue.getAddress());
			DmlNode::doPass1(tdbb, csb, fieldInfo.validationExpr.getAddress());
		}

		if (csb->csb_node)
		{
			if (csb->csb_node->getKind() == DmlNode::KIND_STATEMENT)
				StmtNode::doPass2(tdbb, csb, reinterpret_cast<StmtNode**>(&csb->csb_node), NULL);
			else
				ExprNode::doPass2(tdbb, csb, &csb->csb_node);
		}

		// Compile (pass2) domains DEFAULT and constraints
		for (bool found = accessor.getFirst(); found; found = accessor.getNext())
		{
			FieldInfo& fieldInfo = accessor.current()->second;
			ExprNode::doPass2(tdbb, csb, fieldInfo.defaultValue.getAddress());
			ExprNode::doPass2(tdbb, csb, fieldInfo.validationExpr.getAddress());
		}

		/*** Print nodes for debugging purposes.
		NodePrinter printer;
		csb->csb_node->print(printer);
		printf("\n%s\n\n\n", printer.getText().c_str());
		***/

		if (csb->csb_impure > MAX_REQUEST_SIZE)
			IBERROR(226);			// msg 226 request size limit exceeded

		if (beforeCsbRelease)
			beforeCsbRelease();

		// Build the statement and the final request block.
		const auto pool = tdbb->getDefaultPool();
		statement = FB_NEW_POOL(*pool) Statement(tdbb, pool, csb);

		tdbb->setRequest(old_request);
	} // try
	catch (const Exception& ex)
	{
		if (statement)
		{
			// Release sub statements.
			for (auto subStatement : statement->subStatements)
				subStatement->release(tdbb);
		}

		ex.stuffException(tdbb->tdbb_status_vector);
		tdbb->setRequest(old_request);
		ERR_punt();
	}

	if (internalFlag)
	{
		statement->flags |= FLAG_INTERNAL;
		statement->charSetId = CS_METADATA;
	}
	else
		statement->charSetId = attachment->att_charset;

	return statement;
}

Statement* Statement::makeBoolExpression(thread_db* tdbb, BoolExprNode*& node,
	CompilerScratch* csb, bool internalFlag)
{
	fb_assert(csb->csb_node->getKind() == DmlNode::KIND_BOOLEAN);

	return makeStatement(tdbb, csb, internalFlag,
		[&]
		{
			node = static_cast<BoolExprNode*>(csb->csb_node);
		});
}

Statement* Statement::makeValueExpression(thread_db* tdbb, ValueExprNode*& node, dsc& desc,
	CompilerScratch* csb, bool internalFlag)
{
	fb_assert(csb->csb_node->getKind() == DmlNode::KIND_VALUE);

	return makeStatement(tdbb, csb, internalFlag,
		[&]
		{
			node = static_cast<ValueExprNode*>(csb->csb_node);
			node->getDesc(tdbb, csb, &desc);
		});
}

// Turn a parsed scratch into an executable request.
Request* Statement::makeRequest(thread_db* tdbb, CompilerScratch* csb, bool internalFlag)
{
	Statement* statement = makeStatement(tdbb, csb, internalFlag);

	Request* req = statement->getRequest(tdbb, statement->requests.readAccessor(), 0);
	statement->loadResources(tdbb, req, false);

	return req;
}

// Returns function or procedure routine.
const Routine* Statement::getRoutine() const
{
	fb_assert(!(procedure && function));

	if (procedure)
		return procedure;

	return function;
}

void Statement::restartRequests(thread_db* tdbb, jrd_tra* trans)
{
	auto g = requests.readAccessor();
	for (FB_SIZE_T n = 0; n < g->getCount(); ++n)
	{
		Request* request = g->value(n);

		if (request && request->req_transaction)
		{
			fb_assert(request->req_attachment == tdbb->getAttachment());

			EXE_unwind(tdbb, request);
			EXE_start(tdbb, request, trans);
		}
	}
}

Request* Statement::findRequest(thread_db* tdbb, bool unique)
{
	SET_TDBB(tdbb);
	Attachment* const attachment = tdbb->getAttachment();

	const Statement* const thisPointer = this;	// avoid warning
	if (!thisPointer)
		BUGCHECK(167);	/* msg 167 invalid SEND request */

	// Search clones for one request used whenever by this attachment.
	// If not found, return first inactive request.
	Request* clone = NULL;

	do
	{
		USHORT count = 0;
		auto g = requests.readAccessor();
		const USHORT clones = g->getCount();
		USHORT n;

		for (n = 0; n < clones; ++n)
		{
			Request* next = getRequest(tdbb, g, n);

			if (next->req_attachment == attachment)
			{
				if (!next->isUsed())
				{
					clone = next;
					break;
				}

				if (unique)
					return NULL;

				++count;
			}
			else if (!(next->isUsed()) && !clone)
				clone = next;
		}

		if (count > MAX_CLONES)
			ERR_post(Arg::Gds(isc_req_max_clones_exceeded));

		if (!clone)
			clone = getRequest(tdbb, g, n);

	} while (!clone->setUsed());

	clone->setAttachment(attachment);
	clone->req_stats.reset();
	clone->req_base_stats.reset();

	try
	{
		loadResources(tdbb, clone, true);
		return clone;
	}
	catch(const Exception&)
	{
		clone->setUnused();
		throw;
	}
}

Request* Statement::getRequest(thread_db* tdbb, const Requests::ReadAccessor& g, USHORT level)
{
	SET_TDBB(tdbb);

	Database* const dbb = tdbb->getDatabase();
	fb_assert(dbb);

	if (level < g->getCount() && g->value(level))
		return g->value(level);

	// Create the request.
	AutoMemoryPool reqPool(MemoryPool::createPool(pool));
#ifdef DEBUG_LOST_POOLS
	fprintf(stderr, "%p %s %s\n", reqPool->mp(), sqlText ? sqlText->c_str() : "<nullptr>",
		procedure ? procedure->getName().toQuotedString().c_str() :
		function ? function->getName().toQuotedString().c_str() : "<not>");
#endif
	auto request = FB_NEW_POOL(*reqPool) Request(reqPool, dbb, this);
	try
	{
		loadResources(tdbb, request, true);
	}
	catch(const Exception&)
	{
		MemoryPool::deletePool(request->req_pool);
		throw;
	}

	Request* arrivedRq;
	{ // mutex scope
		MutexLockGuard guard(requestsGrow, FB_FUNCTION);

		auto g = requests.writeAccessor();

		if (level >= g->getCount() || !g->value(level))
		{
			requests.grow(level + 1, true);

			g = requests.writeAccessor();
			g->value(level) = request;
			return request;
		}

		arrivedRq = g->value(level);
	}

	MemoryPool::deletePool(request->req_pool);
	return arrivedRq;
}

// Invoke request obtained earlier using compileRequest() API call
Request* Statement::getUserRequest(thread_db* tdbb, USHORT level)
{
	return getRequest(tdbb, requests.readAccessor(), level);
}

// Check that we have enough rights to access all resources this request touches including
// resources it used indirectly via procedures or triggers.
void Statement::verifyAccess(thread_db* tdbb)
{
	if (flags & FLAG_INTERNAL)
		return;

	SET_TDBB(tdbb);

	ExternalAccessList external;
	const MetaName defaultUser;
	buildExternalAccess(tdbb, external, defaultUser);

	for (ExternalAccess* item = external.begin(); item != external.end(); ++item)
	{
		Routine* routine = nullptr;
		int aclType;

		if (item->exa_action == ExternalAccess::exa_procedure)
		{
			routine = MetadataCache::getVersioned<Cached::Procedure>(tdbb, item->exa_prc_id, 0);
			if (!routine)
			{
				string name;
				name.printf("id %d", item->exa_prc_id);
				ERR_post(Arg::Gds(isc_prcnotdef) << name);
			}
			aclType = id_procedure;
		}
		else if (item->exa_action == ExternalAccess::exa_function)
		{
			routine = Function::lookup(tdbb, item->exa_fun_id, 0);

			if (!routine)
			{
				string name;
				name.printf("id %d", item->exa_fun_id);
				ERR_post(Arg::Gds(isc_funnotdef) << name);
			}

			aclType = id_function;
		}
		else
		{
			jrd_rel* relation = MetadataCache::getVersioned<Cached::Relation>(tdbb, item->exa_rel_id, CacheFlag::AUTOCREATE);

			if (!relation)
				continue;

			MetaName userName = item->user;
			if (item->exa_view_id)
			{
				auto view = MetadataCache::getPerm<Cached::Relation>(tdbb, item->exa_view_id, CacheFlag::AUTOCREATE);
				if (view && (view->getId() >= USER_DEF_REL_INIT_ID))
					userName = view->rel_owner_name;
			}

			switch (item->exa_action)
			{
				case ExternalAccess::exa_insert:
					verifyTriggerAccess(tdbb, relation, relation->rel_triggers[TRIGGER_PRE_STORE], userName);
					verifyTriggerAccess(tdbb, relation, relation->rel_triggers[TRIGGER_POST_STORE], userName);
					break;
				case ExternalAccess::exa_update:
					verifyTriggerAccess(tdbb, relation, relation->rel_triggers[TRIGGER_PRE_MODIFY], userName);
					verifyTriggerAccess(tdbb, relation, relation->rel_triggers[TRIGGER_POST_MODIFY], userName);
					break;
				case ExternalAccess::exa_delete:
					verifyTriggerAccess(tdbb, relation, relation->rel_triggers[TRIGGER_PRE_ERASE], userName);
					verifyTriggerAccess(tdbb, relation, relation->rel_triggers[TRIGGER_POST_ERASE], userName);
					break;
				default:
					fb_assert(false);
			}

			continue;
		}

		fb_assert(routine);
		if (!routine->getStatement())
			continue;

		for (const auto& access : routine->getStatement()->accessList)
		{
			MetaName userName = item->user;

			if (access.acc_ss_rel_id)
			{
				auto view = MetadataCache::getPerm<Cached::Relation>(tdbb, access.acc_ss_rel_id, CacheFlag::AUTOCREATE);
				if (view && (view->getId() >= USER_DEF_REL_INIT_ID))
					userName = view->rel_owner_name;
			}

			Attachment* attachment = tdbb->getAttachment();
			UserId* effectiveUser = userName.hasData() ? attachment->getUserId(userName) : attachment->att_ss_user;
			AutoSetRestore<UserId*> userIdHolder(&attachment->att_ss_user, effectiveUser);

			const SecurityClass* sec_class = SCL_get_class(tdbb, access.acc_security_name);

			if (routine->getName().package.isEmpty())
			{
				SCL_check_access(tdbb, sec_class, aclType, routine->getName(),
							access.acc_mask, access.acc_type, true, access.acc_name, access.acc_col_name);
			}
			else
			{
				SCL_check_access(tdbb, sec_class, id_package, routine->getName().getSchemaAndPackage(),
							access.acc_mask, access.acc_type, true, access.acc_name, access.acc_col_name);
			}
		}
	}

	// Inherit privileges of caller stored procedure or trigger if and only if
	// this request is called immediately by caller (check for empty req_caller).
	// Currently (in v2.5) this rule will work for EXECUTE STATEMENT only, as
	// tra_callback_count incremented only by it.
	// In v3.0, this rule also works for external procedures and triggers.
	jrd_tra* transaction = tdbb->getTransaction();
	const bool useCallerPrivs = transaction && transaction->tra_callback_count;

	for (const AccessItem* access = accessList.begin(); access != accessList.end(); ++access)
	{
		QualifiedName objName;
		SLONG objType = 0;

		MetaName userName;

		if (useCallerPrivs)
		{
			switch (transaction->tra_caller_name.type)
			{
				case obj_trigger:
					objType = id_trigger;
					break;
				case obj_procedure:
					objType = id_procedure;
					break;
				case obj_udf:
					objType = id_function;
					break;
				case obj_package_header:
					objType = id_package;
					break;
				case obj_type_MAX:	// CallerName() constructor
					fb_assert(transaction->tra_caller_name.name.object.isEmpty());
					break;
				default:
					fb_assert(false);
			}

			objName = transaction->tra_caller_name.name;
			userName = transaction->tra_caller_name.userName;
		}

		if (access->acc_ss_rel_id)
		{
			auto view = MetadataCache::getPerm<Cached::Relation>(tdbb, access->acc_ss_rel_id, CacheFlag::AUTOCREATE);
			if (view && (view->getId() >= USER_DEF_REL_INIT_ID))
				userName = view->rel_owner_name;
		}

		Attachment* attachment = tdbb->getAttachment();
		UserId* effectiveUser = userName.hasData() ? attachment->getUserId(userName) : attachment->att_ss_user;
		AutoSetRestore<UserId*> userIdHolder(&attachment->att_ss_user, effectiveUser);

		const SecurityClass* sec_class = SCL_get_class(tdbb, access->acc_security_name);

		SCL_check_access(tdbb, sec_class, objType, objName,
			access->acc_mask, access->acc_type, true, access->acc_name, access->acc_col_name);
	}
}

// Release a statement.
void Statement::release(thread_db* tdbb)
{
	SET_TDBB(tdbb);

	// Release sub statements.
	for (Statement** subStatement = subStatements.begin();
		 subStatement != subStatements.end();
		 ++subStatement)
	{
		(*subStatement)->release(tdbb);
	}

	// ok to use write accessor w/o lock - we are in a kind of "dtor"
	auto g = requests.writeAccessor();
	for (Request** instance = g->begin(); instance != g->end(); ++instance)
	{
		if (*instance)
		{
			fb_assert(!((*instance)->isUsed()));
			EXE_release(tdbb, *instance);
			MemoryPool::deletePool((*instance)->req_pool);
			*instance = nullptr;
		}
	}

	sqlText = NULL;

	// ~Statement is never called :-(
	requests.~Requests();

	// Sub statement pool is the same of the main statement, so don't delete it.
	if (!parentStatement)
	{
		Database* dbb = tdbb->getDatabase();
		dbb->deletePool(pool);
	}
}

// Returns a formatted textual plan for all RseNode's in the specified request
string Statement::getPlan(thread_db* tdbb, bool detailed) const
{
	string plan;

	for (const auto select : fors)
		select->printPlan(tdbb, plan, detailed);

	return plan;
}

void Statement::getPlan(thread_db* tdbb, PlanEntry& planEntry) const
{
	planEntry.className = "Statement";
	planEntry.level = 0;

	for (const auto select : fors)
		select->getPlan(tdbb, planEntry.children.add(), 0, true);
}

// Check that we have enough rights to access all resources this list of triggers touches.
void Statement::verifyTriggerAccess(thread_db* tdbb, const jrd_rel* ownerRelation,
	const Triggers& triggers, MetaName userName)
{
	if (!triggers)
		return;

	SET_TDBB(tdbb);

	for (auto t : triggers)
	{
		t->compile(tdbb);
		if (!t->statement)
			continue;

		for (const AccessItem* access = t->statement->accessList.begin();
			 access != t->statement->accessList.end(); ++access)
		{
			// If this is not a system relation, we don't post access check if:
			//
			// - The table being checked is the owner of the trigger that's accessing it.
			// - The field being checked is owned by the same table than the trigger
			//   that's accessing the field.
			// - Since the trigger name comes in the triggers vector of the table and each
			//   trigger can be owned by only one table for now, we know for sure that
			//   it's a trigger defined on our target table.

			if (!ownerRelation->isSystem())
			{
				if (access->acc_type == obj_relations &&
					(ownerRelation->getName() == access->acc_name))
				{
					continue;
				}
				if (access->acc_type == obj_column &&
					(ownerRelation->getName() == access->acc_name))
				{
					continue;
				}
			}

			// a direct access to an object from this trigger
			if (access->acc_ss_rel_id)
			{
				auto view = MetadataCache::getPerm<Cached::Relation>(tdbb, access->acc_ss_rel_id, CacheFlag::AUTOCREATE);
				if (view && (view->getId() >= USER_DEF_REL_INIT_ID))
					userName = view->rel_owner_name;
			}
			else if (t->ssDefiner.asBool())
				userName = t->owner;

			Attachment* attachment = tdbb->getAttachment();
			UserId* effectiveUser = userName.hasData() ? attachment->getUserId(userName) : attachment->att_ss_user;
			AutoSetRestore<UserId*> userIdHolder(&attachment->att_ss_user, effectiveUser);

			const SecurityClass* sec_class = SCL_get_class(tdbb, access->acc_security_name);

			SCL_check_access(tdbb, sec_class, id_trigger, t->statement->triggerName, access->acc_mask,
				access->acc_type, true, access->acc_name, access->acc_col_name);
		}
	}
}

// Invoke buildExternalAccess for triggers in vector
inline void Statement::triggersExternalAccess(thread_db* tdbb, ExternalAccessList& list,
	const Triggers& tvec, const MetaName& user)
{
	if (!tvec)
		return;

	for (auto t : tvec)
	{
		t->compile(tdbb);
		if (t->statement)
		{
			const MetaName& userName = t->ssDefiner.asBool() ? t->owner : user;
			t->statement->buildExternalAccess(tdbb, list, userName);
		}
	}
}

// Recursively walk external dependencies (procedures, triggers) for request to assemble full
// list of requests it depends on.
void Statement::buildExternalAccess(thread_db* tdbb, ExternalAccessList& list, const MetaName &user)
{
	for (ExternalAccess* item = externalList.begin(); item != externalList.end(); ++item)
	{
		FB_SIZE_T i;

		// Add externals recursively
		if (item->exa_action == ExternalAccess::exa_procedure)
		{
			auto procedure = MetadataCache::getVersioned<Cached::Procedure>(tdbb, item->exa_prc_id, 0);
			if (procedure && procedure->getStatement())
			{
				item->user = procedure->invoker ? MetaName(procedure->invoker->getUserName()) : user;
				if (list.find(*item, i))
					continue;
				list.insert(i, *item);
				procedure->getStatement()->buildExternalAccess(tdbb, list, item->user);
			}
		}
		else if (item->exa_action == ExternalAccess::exa_function)
		{
			auto function = Function::lookup(tdbb, item->exa_fun_id, 0);
			if (function && function->getStatement())
			{
				item->user = function->invoker ? MetaName(function->invoker->getUserName()) : user;
				if (list.find(*item, i))
					continue;
				list.insert(i, *item);
				function->getStatement()->buildExternalAccess(tdbb, list, item->user);
			}
		}
		else
		{
			jrd_rel* relation = MetadataCache::getVersioned<Cached::Relation>(tdbb, item->exa_rel_id, CacheFlag::AUTOCREATE);
			if (!relation)
				continue;

			Triggers *vec1, *vec2;
			switch (item->exa_action)
			{
				case ExternalAccess::exa_insert:
					vec1 = &relation->rel_triggers[TRIGGER_PRE_STORE];
					vec2 = &relation->rel_triggers[TRIGGER_POST_STORE];
					break;
				case ExternalAccess::exa_update:
					vec1 = &relation->rel_triggers[TRIGGER_PRE_MODIFY];
					vec2 = &relation->rel_triggers[TRIGGER_POST_MODIFY];
					break;
				case ExternalAccess::exa_delete:
					vec1 = &relation->rel_triggers[TRIGGER_PRE_ERASE];
					vec2 = &relation->rel_triggers[TRIGGER_POST_ERASE];
					break;
				default:
					fb_assert(false);
					continue; // should never happen, silence the compiler
			}

			item->user = relation->rel_ss_definer.asBool() ? getPermanent(relation)->rel_owner_name : user;
			if (list.find(*item, i))
				continue;
			list.insert(i, *item);
			triggersExternalAccess(tdbb, list, *vec1, item->user);
			triggersExternalAccess(tdbb, list, *vec2, item->user);
		}
	}
}

MessageNode* Statement::getMessage(USHORT messageNumber) const
{
	if (messageNumber >= messages.getCount())
	{
		status_exception::raise(Arg::Gds(isc_badmsgnum));
	}
	MessageNode* result = messages[messageNumber];
	if (result == nullptr)
	{
		status_exception::raise(Arg::Gds(isc_badmsgnum));
	}
	return result;
}


// verify_request_synchronization
//
// @brief Finds the sub-request at the given level. If that specific
// sub-request is not found, throw the dreaded "request synchronization error".
// This function replaced a chunk of code repeated four times.
//
// @param level The level of the sub-request we need to find.
Request* Statement::verifyRequestSynchronization(USHORT level)
{
	auto g = requests.readAccessor();
	fb_assert(g->getCount() > 0);
	if (level && (level >= g->getCount() || !g->value(level)))
		ERR_post(Arg::Gds(isc_req_sync));

	return g->value(level);
}


// Make sub routines.
template <typename T> static void makeSubRoutines(thread_db* tdbb, Statement* statement,
	CompilerScratch* csb, T& subs)
{
	typename T::Accessor subAccessor(&subs);

	for (auto& sub : subs)
	{
		auto subNode = sub.second;
		auto subRoutine = subNode->routine;
		auto& subCsb = subNode->subCsb;

		auto subStatement = Statement::makeStatement(tdbb, subCsb, false);
		subStatement->parentStatement = statement;
		subRoutine->setStatement(subStatement);

		// Dependencies should be added directly to the main routine while parsing.
		fb_assert(subCsb->csb_dependencies.isEmpty());

		// Move permissions from the sub routine to the parent.

		for (auto& access : subStatement->externalList)
		{
			FB_SIZE_T i;
			if (!csb->csb_external.find(access, i))
				csb->csb_external.insert(i, access);
		}

		for (auto& access : subStatement->accessList)
		{
			FB_SIZE_T i;
			if (!csb->csb_access.find(access, i))
				csb->csb_access.insert(i, access);
		}

		delete subCsb;
		subCsb = NULL;

		statement->subStatements.add(subStatement);
	}
}

bool Request::isRoot() const
{
	return this == statement->rootRequest();
}

StmtNumber Request::getRequestId() const
{
	if (!req_id)
	{
		req_id = isRoot() ?
			statement->getStatementId() :
			JRD_get_thread_data()->getDatabase()->generateStatementId();
	}

	return req_id;
}

Request::Request(Firebird::AutoMemoryPool& pool, Database* dbb, /*const*/ Statement* aStatement)
	: statement(aStatement),
	  req_inUse(false),
	  req_pool(pool),
	  req_memory_stats(&aStatement->pool->getStatsGroup()),
	  req_blobs(*req_pool),
	  req_stats(*req_pool),
	  req_base_stats(*req_pool),
	  req_ext_stmt(NULL),
	  req_cursors(*req_pool),
	  req_ext_resultset(NULL),
	  req_timeout(0),
	  req_domain_validation(NULL),
	  req_auto_trans(*req_pool),
	  req_sorts(*req_pool, dbb),
	  req_rpb(*req_pool),
	  impureArea(*req_pool)
{
	fb_assert(statement);
	req_rpb = statement->rpbsSetup;
	impureArea.grow(statement->impureSize);

	pool->setStatsGroup(req_memory_stats);
	pool.release();
}

bool Request::setUsed() noexcept
{
	bool old = isUsed();
	if (old)
		return false;
	return req_inUse.compare_exchange_strong(old, true);
}

void Request::setUnused() noexcept
{
	fb_assert(isUsed());
	req_inUse.store(false, std::memory_order_release);
}

bool Request::isUsed() const noexcept
{
	return req_inUse.load(std::memory_order_relaxed);
}

bool Request::hasInternalStatement() const noexcept
{
	return statement->flags & Statement::FLAG_INTERNAL;
}

bool Request::hasPowerfulStatement() const noexcept
{
	return statement->flags & Statement::FLAG_POWERFUL;
}

#ifdef DEV_BUILD
// Function is designed to be called from debugger to print subtree of current execution node

const int devNodePrint(DmlNode* node)
{
	NodePrinter printer;
	node->print(printer);
	printf("\n%s\n\n\n", printer.getText().c_str());
	fflush(stdout);
	return 0;
}
#endif

#ifdef DEBUG_SHARED_VECTOR
namespace Jrd {

struct Acc
{
	void* mem;
	int order;

	Acc(void* mem, int order)
		: mem(mem), order(order)
	{ }
};

int order = 0;
GlobalPtr<Mutex> mtx;

class Member : public Array<Acc>
{
public:
	Member(Firebird::MemoryPool& p)
		: Array<Acc>(p)
	{ }

	~Member()
	{
		for (const auto x : *this)
		{
			printf("%d %p\n", x.order, x.mem);
		}
	}
};

GlobalPtr<Member> acc;

void srvAcc(void* mem)
{
	MutexLockGuard g(mtx, FB_FUNCTION);
	acc->add(Acc(mem, order++));
}

void srvDis(void* mem)
{
	MutexLockGuard g(mtx, FB_FUNCTION);
	Member::size_type pos;
	if (acc->find([mem](const Acc& item) { return item.mem == mem ? 0 : -1; }, pos))
		acc->remove(pos);
}

}
#endif // DEBUG_SHARED_VECTOR
