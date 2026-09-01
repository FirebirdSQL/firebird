/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		flu.h
 *	DESCRIPTION:	Function lookup definitions
 *
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
 *
 * 2002.10.28 Sean Leyne - Completed removal of obsolete "HP700" port
 *
 */

#ifndef JRD_FLU_H
#define JRD_FLU_H

// External modules for UDFs/BLOB filters/y-valve loader

#include "../common/classes/objects_array.h"
#include "../common/os/mod_loader.h"
#include "../common/classes/RefCounted.h"

namespace Firebird::Jrd {
	class Database;

	class Module
	{
	private:
		class InternalModule : public RefCounted
		{
		private:
			InternalModule(const InternalModule &im);
			void operator=(const InternalModule &im);

		public:
			ModuleLoader::Module* handle;
			PathName originalName, loadName;

			void* findSymbol(const string& name)
			{
				if (! handle)
				{
					return 0;
				}
				return handle->findSymbol(NULL, name);
			}

			InternalModule(MemoryPool& p,
						   ModuleLoader::Module* h,
						   const PathName& on,
						   const PathName& ln)
				: handle(h),
				  originalName(p, on),
				  loadName(p, ln)
			{ }

			~InternalModule();

			bool operator==(const PathName &pn) const
			{
				return originalName == pn || loadName == pn;
			}

		};

		RefPtr<InternalModule> interMod;

		explicit Module(InternalModule* h)
			: interMod(h)
		{ }

		static Module lookupModule(const char*);

		static InternalModule* scanModule(const PathName& name);

	public:
		typedef Array<InternalModule*> LoadedModules;

		Module()
		{ }

		explicit Module(MemoryPool&)
		{ }

		Module(MemoryPool&, const Module& m)
			: interMod(m.interMod)
		{ }

		Module(const Module& m)
			: interMod(m.interMod)
		{ }

		~Module();

		// used for UDF/BLOB Filter
		static FPTR_INT lookup(const char*, const char*, Database*);

		bool operator>(const Module &im) const;

		void *lookupSymbol(const string& name)
		{
			if (! interMod)
			{
				return 0;
			}
			return interMod->findSymbol(name);
		}

		operator bool() const
		{
			return interMod;
		}
	};

	typedef SortedObjectsArray<Module> DatabaseModules;

} // namespace Firebird::Jrd

#endif // JRD_FLU_H
