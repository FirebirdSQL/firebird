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
#include "ibase.h"
#include "firebird/Interface.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/init.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/objects_array.h"
#include "../common/os/mod_loader.h"
#include "../common/os/path_utils.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/GetPlugins.h"
#include "../common/StatusHolder.h"


namespace Firebird
{
	namespace Udr
	{
//------------------------------------------------------------------------------


class UdrPluginImpl;


static GlobalPtr<ObjectsArray<PathName> > paths;

class Engine : public StdPlugin<IExternalEngineImpl<Engine, ThrowStatusWrapper> >
{
public:
	explicit Engine(IPluginConfig* par)
		: functions(getPool()),
		  procedures(getPool()),
		  triggers(getPool())
	{
		LocalStatus ls;
		CheckStatusWrapper s(&ls);
		RefPtr<IConfig> defaultConfig(REF_NO_INCR, par->getDefaultConfig(&s));
		check(&s);

		if (defaultConfig)
		{
			// this plugin is not ready to support different configurations
			// therefore keep legacy approach

			RefPtr<IConfigEntry> icp;

			for (int n = 0; icp.assignRefNoIncr(defaultConfig->findPos(&s, "path", n)); ++n)
			{
				check(&s);

				PathName newPath(icp->getValue());
				bool found = false;

				for (ObjectsArray<PathName>::iterator i = paths->begin(); i != paths->end(); ++i)
				{
					if (*i == newPath)
					{
						found = true;
						break;
					}
				}

				if (!found)
					paths->add(newPath);
			}
		}
	}

public:
	UdrPluginImpl* loadModule(ThrowStatusWrapper* status, IRoutineMetadata* metadata,
		PathName* moduleName, string* entryPoint);

	template <typename NodeType, typename ObjType, typename SharedObjType> ObjType* getChild(
		ThrowStatusWrapper* status,
		GenericMap<Pair<NonPooled<IExternalContext*, ObjType*> > >& children,
		SharedObjType* sharedObj, IExternalContext* context,
		SortedArray<SharedObjType*>& sharedObjs, const PathName& moduleName);

	template <typename ObjType> void deleteChildren(
		GenericMap<Pair<NonPooled<IExternalContext*, ObjType*> > >& children);

	template <typename T> T* findNode(ThrowStatusWrapper* status,
		const GenericMap<Pair<Left<string, T*> > >& nodes, const string& entryPoint);

public:
	void open(ThrowStatusWrapper* status, IExternalContext* context, char* name, unsigned nameSize);
	void openAttachment(ThrowStatusWrapper* status, IExternalContext* context);
	void closeAttachment(ThrowStatusWrapper* status, IExternalContext* context);
	IExternalFunction* makeFunction(ThrowStatusWrapper* status, IExternalContext* context,
		IRoutineMetadata* metadata, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder);
	IExternalProcedure* makeProcedure(ThrowStatusWrapper* status, IExternalContext* context,
		IRoutineMetadata* metadata, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder);
	IExternalTrigger* makeTrigger(ThrowStatusWrapper* status, IExternalContext* context,
		IRoutineMetadata* metadata, IMetadataBuilder* fieldsBuilder);

private:
	Mutex childrenMutex;

public:
	SortedArray<class SharedFunction*> functions;
	SortedArray<class SharedProcedure*> procedures;
	SortedArray<class SharedTrigger*> triggers;
};


class ModulesMap : public GenericMap<Pair<Left<PathName, UdrPluginImpl*> > >
{
public:
	explicit ModulesMap(MemoryPool& p)
		: GenericMap<Pair<Left<PathName, UdrPluginImpl*> > >(p)
	{
	}

	~ModulesMap();
};


//--------------------------------------


static GlobalPtr<Mutex> modulesMutex;
static GlobalPtr<ModulesMap> modules;


//--------------------------------------


class UdrPluginImpl : public VersionedIface<IUdrPluginImpl<UdrPluginImpl, ThrowStatusWrapper> >
{
public:
	UdrPluginImpl(const PathName& aModuleName, ModuleLoader::Module* aModule)
		: moduleName(*getDefaultMemoryPool(), aModuleName),
		  module(aModule),
		  myUnloadFlag(FB_FALSE),
		  theirUnloadFlag(NULL),
		  functionsMap(*getDefaultMemoryPool()),
		  proceduresMap(*getDefaultMemoryPool()),
		  triggersMap(*getDefaultMemoryPool())
	{
	}

	~UdrPluginImpl()
	{
		if (myUnloadFlag)
			return;

		*theirUnloadFlag = FB_TRUE;

		{
			GenericMap<Pair<Left<string, IUdrFunctionFactory*> > >::Accessor accessor(&functionsMap);
			for (bool cont = accessor.getFirst(); cont; cont = accessor.getNext())
				accessor.current()->second->dispose();
		}

		{
			GenericMap<Pair<Left<string, IUdrProcedureFactory*> > >::Accessor accessor(&proceduresMap);
			for (bool cont = accessor.getFirst(); cont; cont = accessor.getNext())
				accessor.current()->second->dispose();
		}

		{
			GenericMap<Pair<Left<string, IUdrTriggerFactory*> > >::Accessor accessor(&triggersMap);
			for (bool cont = accessor.getFirst(); cont; cont = accessor.getNext())
				accessor.current()->second->dispose();
		}
	}

public:
	IMaster* getMaster()
	{
		return MasterInterfacePtr();
	}

	void registerFunction(ThrowStatusWrapper* status, const char* name,
		IUdrFunctionFactory* factory)
	{
		if (functionsMap.exist(name))
		{
			static const ISC_STATUS statusVector[] = {
				isc_arg_gds, isc_random,
				isc_arg_string, (ISC_STATUS) "Duplicate UDR function",
				//// TODO: isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS) name,
				isc_arg_end
			};

			throw FbException(status, statusVector);
		}

		functionsMap.put(name, factory);
	}

	void registerProcedure(ThrowStatusWrapper* status, const char* name,
		IUdrProcedureFactory* factory)
	{
		if (proceduresMap.exist(name))
		{
			static const ISC_STATUS statusVector[] = {
				isc_arg_gds, isc_random,
				isc_arg_string, (ISC_STATUS) "Duplicate UDR procedure",
				//// TODO: isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS) name,
				isc_arg_end
			};

			throw FbException(status, statusVector);
		}

		proceduresMap.put(name, factory);
	}

	void registerTrigger(ThrowStatusWrapper* status, const char* name,
		IUdrTriggerFactory* factory)
	{
		if (triggersMap.exist(name))
		{
			static const ISC_STATUS statusVector[] = {
				isc_arg_gds, isc_random,
				isc_arg_string, (ISC_STATUS) "Duplicate UDR trigger",
				//// TODO: isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS) name,
				isc_arg_end
			};

			throw FbException(status, statusVector);
		}

		triggersMap.put(name, factory);
	}

private:
	PathName moduleName;
	AutoPtr<ModuleLoader::Module> module;

public:
	FB_BOOLEAN myUnloadFlag;
	FB_BOOLEAN* theirUnloadFlag;
	GenericMap<Pair<Left<string, IUdrFunctionFactory*> > > functionsMap;
	GenericMap<Pair<Left<string, IUdrProcedureFactory*> > > proceduresMap;
	GenericMap<Pair<Left<string, IUdrTriggerFactory*> > > triggersMap;
};


class SharedFunction : public DisposeIface<IExternalFunctionImpl<SharedFunction, ThrowStatusWrapper> >
{
public:
	SharedFunction(ThrowStatusWrapper* status, Engine* aEngine, IExternalContext* context,
				IRoutineMetadata* aMetadata,
				IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
		: engine(aEngine),
		  metadata(aMetadata),
		  moduleName(*getDefaultMemoryPool()),
		  entryPoint(*getDefaultMemoryPool()),
		  info(*getDefaultMemoryPool()),
		  children(*getDefaultMemoryPool())
	{
		module = engine->loadModule(status, metadata, &moduleName, &entryPoint);

		IUdrFunctionFactory* factory = engine->findNode<IUdrFunctionFactory>(
			status, module->functionsMap, entryPoint);

		factory->setup(status, context, metadata, inBuilder, outBuilder);
	}

	~SharedFunction()
	{
		engine->deleteChildren(children);
	}

public:
	void getCharSet(ThrowStatusWrapper* status, IExternalContext* context,
		char* name, unsigned nameSize)
	{
		strncpy(name, context->getClientCharSet(), nameSize);

		IExternalFunction* function = engine->getChild<IUdrFunctionFactory, IExternalFunction>(
			status, children, this, context, engine->functions, moduleName);

		if (function)
			function->getCharSet(status, context, name, nameSize);
	}

	void execute(ThrowStatusWrapper* status, IExternalContext* context, void* inMsg, void* outMsg)
	{
		IExternalFunction* function = engine->getChild<IUdrFunctionFactory, IExternalFunction>(
			status, children, this, context, engine->functions, moduleName);

		if (function)
			function->execute(status, context, inMsg, outMsg);
	}

public:
	Engine* engine;
	IRoutineMetadata* metadata;
	PathName moduleName;
	string entryPoint;
	string info;
	GenericMap<Pair<NonPooled<IExternalContext*, IExternalFunction*> > > children;
	UdrPluginImpl* module;
};


//--------------------------------------


class SharedProcedure : public DisposeIface<IExternalProcedureImpl<SharedProcedure, ThrowStatusWrapper> >
{
public:
	SharedProcedure(ThrowStatusWrapper* status, Engine* aEngine, IExternalContext* context,
				IRoutineMetadata* aMetadata,
				IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
		: engine(aEngine),
		  metadata(aMetadata),
		  moduleName(*getDefaultMemoryPool()),
		  entryPoint(*getDefaultMemoryPool()),
		  info(*getDefaultMemoryPool()),
		  children(*getDefaultMemoryPool())
	{
		module = engine->loadModule(status, metadata, &moduleName, &entryPoint);

		IUdrProcedureFactory* factory = engine->findNode<IUdrProcedureFactory>(
			status, module->proceduresMap, entryPoint);

		factory->setup(status, context, metadata, inBuilder, outBuilder);
	}

	~SharedProcedure()
	{
		engine->deleteChildren(children);
	}

public:
	void getCharSet(ThrowStatusWrapper* status, IExternalContext* context,
		char* name, unsigned nameSize)
	{
		strncpy(name, context->getClientCharSet(), nameSize);

		IExternalProcedure* procedure = engine->getChild<IUdrProcedureFactory, IExternalProcedure>(
			status, children, this, context, engine->procedures, moduleName);

		if (procedure)
			procedure->getCharSet(status, context, name, nameSize);
	}

	IExternalResultSet* open(ThrowStatusWrapper* status, IExternalContext* context,
		void* inMsg, void* outMsg)
	{
		IExternalProcedure* procedure = engine->getChild<IUdrProcedureFactory, IExternalProcedure>(
			status, children, this, context, engine->procedures, moduleName);

		return procedure ? procedure->open(status, context, inMsg, outMsg) : NULL;
	}

public:
	Engine* engine;
	IRoutineMetadata* metadata;
	PathName moduleName;
	string entryPoint;
	string info;
	GenericMap<Pair<NonPooled<IExternalContext*, IExternalProcedure*> > > children;
	UdrPluginImpl* module;
};


//--------------------------------------


class SharedTrigger : public DisposeIface<IExternalTriggerImpl<SharedTrigger, ThrowStatusWrapper> >
{
public:
	SharedTrigger(ThrowStatusWrapper* status, Engine* aEngine, IExternalContext* context,
				IRoutineMetadata* aMetadata, IMetadataBuilder* fieldsBuilder)
		: engine(aEngine),
		  metadata(aMetadata),
		  moduleName(*getDefaultMemoryPool()),
		  entryPoint(*getDefaultMemoryPool()),
		  info(*getDefaultMemoryPool()),
		  children(*getDefaultMemoryPool())
	{
		module = engine->loadModule(status, metadata, &moduleName, &entryPoint);

		IUdrTriggerFactory* factory = engine->findNode<IUdrTriggerFactory>(
			status, module->triggersMap, entryPoint);

		factory->setup(status, context, metadata, fieldsBuilder);
	}

	~SharedTrigger()
	{
		engine->deleteChildren(children);
	}

public:
	void getCharSet(ThrowStatusWrapper* status, IExternalContext* context,
		char* name, unsigned nameSize)
	{
		strncpy(name, context->getClientCharSet(), nameSize);

		IExternalTrigger* trigger = engine->getChild<IUdrTriggerFactory, IExternalTrigger>(
			status, children, this, context, engine->triggers, moduleName);

		if (trigger)
			trigger->getCharSet(status, context, name, nameSize);
	}

	void execute(ThrowStatusWrapper* status, IExternalContext* context,
		unsigned action, void* oldMsg, void* newMsg)
	{
		IExternalTrigger* trigger = engine->getChild<IUdrTriggerFactory, IExternalTrigger>(
			status, children, this, context, engine->triggers, moduleName);

		if (trigger)
			trigger->execute(status, context, action, oldMsg, newMsg);
	}

public:
	Engine* engine;
	IRoutineMetadata* metadata;
	PathName moduleName;
	string entryPoint;
	string info;
	GenericMap<Pair<NonPooled<IExternalContext*, IExternalTrigger*> > > children;
	UdrPluginImpl* module;
};


//--------------------------------------


template <typename FactoryType> GenericMap<Pair<Left<string, FactoryType*> > >& getFactoryMap(
	UdrPluginImpl* udrPlugin)
{
	fb_assert(false);
}

template <> GenericMap<Pair<Left<string, IUdrFunctionFactory*> > >& getFactoryMap(
	UdrPluginImpl* udrPlugin)
{
	return udrPlugin->functionsMap;
}

template <> GenericMap<Pair<Left<string, IUdrProcedureFactory*> > >& getFactoryMap(
	UdrPluginImpl* udrPlugin)
{
	return udrPlugin->proceduresMap;
}

template <> GenericMap<Pair<Left<string, IUdrTriggerFactory*> > >& getFactoryMap(
	UdrPluginImpl* udrPlugin)
{
	return udrPlugin->triggersMap;
}


//--------------------------------------


ModulesMap::~ModulesMap()
{
	Accessor accessor(this);
	for (bool cont = accessor.getFirst(); cont; cont = accessor.getNext())
		delete accessor.current()->second;
}


//--------------------------------------


UdrPluginImpl* Engine::loadModule(ThrowStatusWrapper* status, IRoutineMetadata* metadata,
	PathName* moduleName, string* entryPoint)
{
	const string str(metadata->getEntryPoint(status));

	const string::size_type pos = str.find('!');
	if (pos == string::npos)
	{
		static const ISC_STATUS statusVector[] = {
			isc_arg_gds, isc_random,
			isc_arg_string, (ISC_STATUS) "Invalid entry point",
			//// TODO: isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS) entryPoint.c_str(),
			isc_arg_end
		};

		throw FbException(status, statusVector);
	}

	*moduleName = PathName(str.substr(0, pos).c_str());
	// Do not allow module names with directory separators as a security measure.
	if (moduleName->find_first_of("/\\") != string::npos)
	{
		static const ISC_STATUS statusVector[] = {
			isc_arg_gds, isc_random,
			isc_arg_string, (ISC_STATUS) "Invalid module name",
			//// TODO: isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS) moduleName->c_str(),
			isc_arg_end
		};

		throw FbException(status, statusVector);
	}

	*entryPoint = str.substr(pos + 1);

	string::size_type n = entryPoint->find('!');
	*entryPoint = (n == string::npos ? *entryPoint : entryPoint->substr(0, n));

	MutexLockGuard guard(modulesMutex, FB_FUNCTION);

	UdrPluginImpl* ret;

	if (modules->get(*moduleName, ret))
		return ret;

	for (ObjectsArray<PathName>::iterator i = paths->begin(); i != paths->end(); ++i)
	{
		PathName path;
		PathUtils::concatPath(path, *i, *moduleName);

		ISC_STATUS_ARRAY statusArray = {
			isc_arg_gds, isc_random,
			isc_arg_string, (ISC_STATUS) "UDR module not loaded",
			isc_arg_end
		};
		const unsigned ARG_TEXT = 3;	// Keep both in sync
		const unsigned ARG_END = 4;		// with status initializer!

		ModuleLoader::Module* module = ModuleLoader::fixAndLoadModule(&statusArray[ARG_END], path);
		if (!module)
			throw FbException(status, statusArray);

		FB_BOOLEAN* (*entryPoint)(IStatus*, FB_BOOLEAN*, IUdrPlugin*);
		statusArray[ARG_TEXT] = (ISC_STATUS) "UDR plugin entry point not found";

		if (!module->findSymbol(&statusArray[ARG_END], STRINGIZE(FB_UDR_PLUGIN_ENTRY_POINT), entryPoint))
			throw FbException(status, statusArray);

		UdrPluginImpl* udrPlugin = FB_NEW UdrPluginImpl(*moduleName, module);
		udrPlugin->theirUnloadFlag = entryPoint(status, &udrPlugin->myUnloadFlag, udrPlugin);

		if (status->getState() & IStatus::STATE_ERRORS)
		{
			delete udrPlugin;
			ThrowStatusWrapper::checkException(status);
		}

		modules->put(*moduleName, udrPlugin);

		return udrPlugin;
	}

	static const ISC_STATUS statusVector[] = {
		isc_arg_gds, isc_random,
		isc_arg_string, (ISC_STATUS) "No UDR module path was configured",
		isc_arg_end
	};

	throw FbException(status, statusVector);
}


template <typename NodeType, typename ObjType, typename SharedObjType> ObjType* Engine::getChild(
	ThrowStatusWrapper* status,
	GenericMap<Pair<NonPooled<IExternalContext*, ObjType*> > >& children, SharedObjType* sharedObj,
	IExternalContext* context,
	SortedArray<SharedObjType*>& sharedObjs, const PathName& moduleName)
{
	MutexLockGuard guard(childrenMutex, FB_FUNCTION);

	if (!sharedObjs.exist(sharedObj))
		sharedObjs.add(sharedObj);

	ObjType* obj;
	if (!children.get(context, obj))
	{
		GenericMap<Pair<Left<string, NodeType*> > >& nodes = getFactoryMap<NodeType>(
			sharedObj->module);

		NodeType* factory = findNode<NodeType>(status, nodes, sharedObj->entryPoint);
		obj = factory->newItem(status, context, sharedObj->metadata);

		if (obj)
			children.put(context, obj);
	}

	return obj;
}


template <typename ObjType> void Engine::deleteChildren(
	GenericMap<Pair<NonPooled<IExternalContext*, ObjType*> > >& children)
{
	// No need to lock childrenMutex as if there are more threads simultaneously accessing
	// these children in this moment there will be a memory corruption anyway.

	typedef typename GenericMap<Pair<NonPooled<IExternalContext*, ObjType*> > >::Accessor ChildrenAccessor;
	ChildrenAccessor accessor(&children);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext())
		accessor.current()->second->dispose();
}


template <typename T> T* Engine::findNode(ThrowStatusWrapper* status,
	const GenericMap<Pair<Left<string, T*> > >& nodes, const string& entryPoint)
{
	T* factory;

	if (nodes.get(entryPoint, factory))
		return factory;

	static const ISC_STATUS statusVector[] = {
		isc_arg_gds, isc_random,
		isc_arg_string, (ISC_STATUS) "Entry point not found",
		//// TODO: isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS) entryPoint.c_str(),
		isc_arg_end
	};

	throw FbException(status, statusVector);

	return NULL;
}


void Engine::open(ThrowStatusWrapper* /*status*/, IExternalContext* /*context*/, char* name, unsigned nameSize)
{
	strncpy(name, "UTF-8", nameSize);
}


void Engine::openAttachment(ThrowStatusWrapper* /*status*/, IExternalContext* /*context*/)
{
}


void Engine::closeAttachment(ThrowStatusWrapper* /*status*/, IExternalContext* context)
{
	MutexLockGuard guard(childrenMutex, FB_FUNCTION);

	for (SortedArray<SharedFunction*>::iterator i = functions.begin(); i != functions.end(); ++i)
	{
		IExternalFunction* function;
		if ((*i)->children.get(context, function))
		{
			function->dispose();
			(*i)->children.remove(context);
		}
	}

	for (SortedArray<SharedProcedure*>::iterator i = procedures.begin(); i != procedures.end(); ++i)
	{
		IExternalProcedure* procedure;
		if ((*i)->children.get(context, procedure))
		{
			procedure->dispose();
			(*i)->children.remove(context);
		}
	}

	for (SortedArray<SharedTrigger*>::iterator i = triggers.begin(); i != triggers.end(); ++i)
	{
		IExternalTrigger* trigger;
		if ((*i)->children.get(context, trigger))
		{
			trigger->dispose();
			(*i)->children.remove(context);
		}
	}
}


IExternalFunction* Engine::makeFunction(ThrowStatusWrapper* status, IExternalContext* context,
	IRoutineMetadata* metadata, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
{
	return FB_NEW SharedFunction(status, this, context, metadata, inBuilder, outBuilder);
}


IExternalProcedure* Engine::makeProcedure(ThrowStatusWrapper* status, IExternalContext* context,
	IRoutineMetadata* metadata, IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
{
	return FB_NEW SharedProcedure(status, this, context, metadata, inBuilder, outBuilder);
}


IExternalTrigger* Engine::makeTrigger(ThrowStatusWrapper* status, IExternalContext* context,
	IRoutineMetadata* metadata, IMetadataBuilder* fieldsBuilder)
{
	return FB_NEW SharedTrigger(status, this, context, metadata, fieldsBuilder);
}


//--------------------------------------


class IExternalEngineFactoryImpl : public SimpleFactory<Engine>
{
} factory;

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(IMaster* master)
{
	CachedMasterInterface::set(master);

	PluginManagerInterfacePtr pi;
	pi->registerPluginFactory(IPluginManager::TYPE_EXTERNAL_ENGINE, "UDR", &factory);
	getUnloadDetector()->registerMe();
}


//------------------------------------------------------------------------------
	}	// namespace Udr
}	// namespace Firebird
