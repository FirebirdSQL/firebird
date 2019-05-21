/*
 *	PROGRAM:		Firebird interface.
 *	MODULE:			ImplementHelper.h
 *	DESCRIPTION:	Tools to help create interfaces.
 *
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2010 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_COMMON_CLASSES_IMPLEMENT_HELPER
#define FB_COMMON_CLASSES_IMPLEMENT_HELPER

#include "firebird/Interface.h"
#include "../common/classes/alloc.h"
#include "gen/iberror.h"
#include "../yvalve/gds_proto.h"
#include "../common/classes/init.h"
#include "../common/classes/auto.h"
#include "../common/classes/RefCounted.h"
#include "../common/StatusArg.h"
#include "firebird/impl/consts_pub.h"
#ifdef DEV_BUILD
#include <stdio.h>
#endif

namespace Firebird {

// Implement standard interface and plugin functions

// Helps to implement generic versioned interfaces
template <class C>
class VersionedIface : public C
{
public:
	VersionedIface() { }

private:
	VersionedIface(const VersionedIface&);
	VersionedIface& operator=(const VersionedIface&);
};

// Helps to implement versioned interfaces on stack or static
template <class C>
class AutoIface : public VersionedIface<C>
{
public:
	AutoIface() { }

	/*** TODO: Alex, why this?
	void* operator new(size_t, void* memory) throw()
	{
		return memory;
	}
	***/
};

// Helps to implement disposable interfaces
template <class C>
class DisposeIface : public VersionedIface<C>, public GlobalStorage
{
public:
	DisposeIface() { }

	//// TODO: can move dispose method to here, cause C has virtual destructor.
};

// Helps to implement standard interfaces
template <class C>
class RefCntIface : public VersionedIface<C>, public GlobalStorage
{
#ifdef DEV_BUILD

public:
	RefCntIface(const char* m = NULL)
		: refCounter(0), mark(m)
	{
		refCntDPrt('^');
	}

protected:
	~RefCntIface()
	{
		refCntDPrt('_');
		fb_assert(refCounter.value() == 0);
	}

public:

	void addRef()
	{
		refCntDPrt('+');
		++refCounter;
	}

	//// TODO: can move release method to here, cause C has virtual destructor.

protected:
	void refCntDPrt(char f)
	{
		if (mark)
			fprintf(stderr, "%s %p %c %d\n", mark, this, f, int(refCounter.value()));
	}

	AtomicCounter refCounter;
	const char* mark;

#else

public:
	RefCntIface() : refCounter(0) { }

	void addRef()
	{
		++refCounter;
	}

	//// TODO: can move release method to here, cause C has virtual destructor.

protected:
	AtomicCounter refCounter;

	void refCntDPrt(char)
	{ }

#endif
};


// Helps to implement plugins
template <class C>
class StdPlugin : public RefCntIface<C>
{
private:
	IReferenceCounted* owner;

public:
	StdPlugin(const char* m = NULL)
#ifdef DEV_BUILD
		: RefCntIface<C>(m), owner(NULL)
#else
		: owner(NULL)
#endif
	{ }

	IReferenceCounted* getOwner()
	{
		return owner;
	}

	void setOwner(IReferenceCounted* iface)
	{
		owner = iface;
	}
};


// Trivial factory
template <class P>
class SimpleFactoryBase : public AutoIface<IPluginFactoryImpl<SimpleFactoryBase<P>, CheckStatusWrapper> >
{
public:
	IPluginBase* createPlugin(CheckStatusWrapper* status, IPluginConfig* factoryParameter)
	{
		try
		{
			P* p = FB_NEW P(factoryParameter);
			p->addRef();
			return p;
		}
		catch (const Firebird::Exception& ex)
		{
			ex.stuffException(status);
		}
		return NULL;
	}
};

template <class P>
class SimpleFactory : public Static<SimpleFactoryBase<P> >
{
};


// Ensure access to cached pointer to master interface
class CachedMasterInterface
{
public:
	static void set(IMaster* master);

protected:
	static IMaster* getMasterInterface();
};

// Base for interface type independent accessors
template <typename C>
class AccessAutoInterface : public CachedMasterInterface
{
public:
	explicit AccessAutoInterface(C* aPtr)
		: ptr(aPtr)
	{ }

	operator C*()
	{
		return ptr;
	}

	C* operator->()
	{
		return ptr;
	}

private:
	C* ptr;
};

// Master interface access
class MasterInterfacePtr : public AccessAutoInterface<IMaster>
{
public:
	MasterInterfacePtr()
		: AccessAutoInterface<IMaster>(getMasterInterface())
	{ }
};


// Generic plugins interface access
class PluginManagerInterfacePtr : public AccessAutoInterface<IPluginManager>
{
public:
	PluginManagerInterfacePtr()
		: AccessAutoInterface<IPluginManager>(getMasterInterface()->getPluginManager())
	{ }
/*	explicit PluginManagerInterfacePtr(IMaster* master)
		: AccessAutoInterface<IPluginManager>(master->getPluginManager())
	{ } */
};


// Control timer interface access
class TimerInterfacePtr : public AccessAutoInterface<ITimerControl>
{
public:
	TimerInterfacePtr()
		: AccessAutoInterface<ITimerControl>(getMasterInterface()->getTimerControl())
	{ }
};


// Distributed transactions coordinator access
class DtcInterfacePtr : public AccessAutoInterface<IDtc>
{
public:
	DtcInterfacePtr()
		: AccessAutoInterface<IDtc>(getMasterInterface()->getDtc())
	{ }
};


// Dispatcher access
class DispatcherPtr : public AccessAutoInterface<IProvider>
{
public:
	DispatcherPtr()
		: AccessAutoInterface<IProvider>(getMasterInterface()->getDispatcher())
	{ }

	~DispatcherPtr()
	{
		(*this)->release();
	}
};


// Misc utl access
class UtilInterfacePtr : public AccessAutoInterface<IUtil>
{
public:
	UtilInterfacePtr()
		: AccessAutoInterface<IUtil>(getMasterInterface()->getUtilInterface())
	{ }
};


// When process exits, dynamically loaded modules (for us plugin modules)
// are unloaded first. As the result all global variables in plugin are already destroyed
// when yvalve is starting fb_shutdown(). This causes almost unavoidable segfault.
// To avoid it this class is added - it detects spontaneous (not by PluginManager)
// module unload and notifies PluginManager about this said fact.
class UnloadDetectorHelper FB_FINAL :
	public VersionedIface<IPluginModuleImpl<UnloadDetectorHelper, CheckStatusWrapper> >
{
public:
	typedef void VoidNoParam();

	explicit UnloadDetectorHelper(MemoryPool&)
		: cleanup(NULL), thdDetach(NULL), flagOsUnload(false)
	{ }

	void registerMe()
	{
		PluginManagerInterfacePtr()->registerModule(this);
		flagOsUnload = true;
	}

	~UnloadDetectorHelper()
	{
		if (flagOsUnload)
		{
			const bool dontCleanup = MasterInterfacePtr()->getProcessExiting();
			if (dontCleanup)
			{
				InstanceControl::cancelCleanup();
				return;
			}

			PluginManagerInterfacePtr()->unregisterModule(this);
			doClean();
		}
	}

	bool unloadStarted()
	{
		return !flagOsUnload;
	}

	void setCleanup(VoidNoParam* function)
	{
		cleanup = function;
	}

	void setThreadDetach(VoidNoParam* function)
	{
		thdDetach = function;
	}

	void doClean()
	{
		flagOsUnload = false;

		if (cleanup)
		{
			cleanup();
			cleanup = NULL;
		}
	}

	void threadDetach()
	{
		if (thdDetach)
			thdDetach();
	}

private:
	VoidNoParam* cleanup;
	VoidNoParam* thdDetach;
	bool flagOsUnload;
};

typedef GlobalPtr<UnloadDetectorHelper, InstanceControl::PRIORITY_DETECT_UNLOAD> UnloadDetector;
UnloadDetectorHelper* getUnloadDetector();

// Generic status checker
inline void check(IStatus* status)
{
	if (status->getState() & IStatus::STATE_ERRORS)
	{
		status_exception::raise(status);
	}
}


// debugger for reference counters

#ifdef NEVERDEF
#define FB_CAT2(a, b) a##b
#define FB_CAT1(a, b) FB_CAT2(a, b)
#define FB_RefDeb(c, p, l) Firebird::ReferenceCounterDebugger FB_CAT1(refCntDbg_, l)(c, p)
#define RefDeb(c, p) FB_RefDeb(c, p, __LINE__)

enum DebugEvent
{ DEB_AR_JATT, DEB_RLS_JATT, DEB_RLS_YATT, MaxDebugEvent };

class ReferenceCounterDebugger
{
public:
	ReferenceCounterDebugger(DebugEvent code, const char* p);
	~ReferenceCounterDebugger();
	static ReferenceCounterDebugger* get(DebugEvent code);

	const char* rcd_point;
	ReferenceCounterDebugger* rcd_prev;
	DebugEvent rcd_code;
};

extern IDebug* getImpDebug();

#else
#define RefDeb(c, p)
#endif

} // namespace Firebird

#endif // FB_COMMON_CLASSES_IMPLEMENT_HELPER
