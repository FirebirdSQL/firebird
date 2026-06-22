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

#ifndef JRD_FUNCTION_H
#define JRD_FUNCTION_H

#include "../jrd/Routine.h"
#include "../common/classes/array.h"
#include "../common/dsc.h"
#include "../common/classes/NestConst.h"
#include "../jrd/QualifiedName.h"
#include "../jrd/val.h"
#include "../dsql/Nodes.h"
#include "../jrd/CacheVector.h"
#include "../jrd/lck.h"

namespace Jrd
{
	class ValueListNode;

	class Function final : public Routine
	{
		static const char* const EXCEPTION_MESSAGE;

	public:
		static const enum lck_t LOCKTYPE = LCK_fun_rescan;

		static Function* lookup(thread_db* tdbb, MetaId id, ObjectBase::Flag flags);
		static Function* lookup(thread_db* tdbb, const QualifiedName& name, ObjectBase::Flag flags);

	private:
		explicit Function(Cached::Function* perm)
			: Routine(perm->getPool()),
			  cachedFunction(perm),
			  fun_exception_message(perm->getPool())
		{
		}

	public:
		Function(MemoryPool& p)
			: Routine(p),
			  cachedFunction(FB_NEW_POOL(p) Cached::Function(p)),
			  fun_exception_message(p)
		{
		}

		static Function* create(thread_db* tdbb, MemoryPool& pool, Cached::Function* perm);
		ScanResult scan(thread_db* tdbb, ObjectBase::Flag flags);
		static std::optional<MetaId> getIdByName(thread_db* tdbb, const QualifiedName& name);
		void checkReload(thread_db* tdbb) const override;

		static const char* objectFamily(void*)
		{
			return "function";
		}

	public:
		int getObjectType() const noexcept override
		{
			return objectType();
		}

		SLONG getSclType() const noexcept override
		{
			return obj_functions;
		}

		static int objectType();

	private:
		~Function() override
		{
			delete fun_external;
			delete fun_external_aggregate;
		}

	public:
		void releaseExternal() override
		{
			delete fun_external;
			fun_external = nullptr;
			delete fun_external_aggregate;
			fun_external_aggregate = nullptr;
		}

	public:
		Cached::Function* cachedFunction;		// entry in the cache
		int (*fun_entrypoint)() = nullptr;		// function entrypoint
		USHORT fun_inputs = 0;					// input arguments
		USHORT fun_return_arg = 0;				// return argument
		ULONG fun_temp_length = 0;				// temporary space required

		Firebird::string fun_exception_message;	// message containing the exception error message

		bool fun_private = false;
		bool fun_deterministic = false;
		bool fun_aggregate = false;
		const ExtEngineManager::Function* fun_external = nullptr;
		const ExtEngineManager::AggregateFunction* fun_external_aggregate = nullptr;

		Cached::Function* getPermanent() const noexcept override
		{
			return cachedFunction;
		}

		ScanResult reload(thread_db* tdbb, ObjectBase::Flag fl);
	};
}

#endif // JRD_FUNCTION_H
