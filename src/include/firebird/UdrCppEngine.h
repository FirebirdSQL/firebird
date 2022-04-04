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

#ifndef FIREBIRD_UDR_CPP_ENGINE
#define FIREBIRD_UDR_CPP_ENGINE

#ifndef FB_UDR_STATUS_TYPE
#error FB_UDR_STATUS_TYPE must be defined with the Status class before UdrCppEngine.h inclusion.
#endif

#include "./Message.h"
#include <string.h>


// Build must export firebird_udr_plugin function.
#define FB_UDR_IMPLEMENT_ENTRY_POINT	\
	namespace Firebird	\
	{	\
		namespace Udr	\
		{	\
			RegistrationNode<IUdrFunctionFactory>* regFunctions = NULL;	\
			RegistrationNode<IUdrProcedureFactory>* regProcedures = NULL;	\
			RegistrationNode<IUdrTriggerFactory>* regTriggers = NULL;	\
		}	\
	}	\
	\
	extern "C" FB_DLL_EXPORT FB_BOOLEAN* FB_UDR_PLUGIN_ENTRY_POINT(::Firebird::IStatus* status,	\
		FB_BOOLEAN* theirUnloadFlag, ::Firebird::IUdrPlugin* udrPlugin)	\
	{	\
		::Firebird::Udr::FactoryRegistration::finish(status, udrPlugin);	\
		\
		class UnloadDetector	\
		{	\
		public:	\
			UnloadDetector(FB_BOOLEAN* aTheirUnloadFlag, ::Firebird::IUdrPlugin* aUdrPlugin)	\
				: myUnloadFlag(FB_FALSE),	\
				  theirUnloadFlag(aTheirUnloadFlag),	\
				  udrPlugin(aUdrPlugin)	\
			{	\
			}	\
			\
			~UnloadDetector()	\
			{	\
				if (!myUnloadFlag)	\
					*theirUnloadFlag = FB_TRUE;	\
			}	\
		\
			FB_BOOLEAN myUnloadFlag;	\
			FB_BOOLEAN* theirUnloadFlag;	\
			::Firebird::IUdrPlugin* udrPlugin;	\
		};	\
		\
		static UnloadDetector unloadDetector(theirUnloadFlag, udrPlugin);	\
		\
		return &unloadDetector.myUnloadFlag;	\
	}


#define FB_UDR_BEGIN_FUNCTION(name)	\
	namespace Func##name	\
	{	\
		class Impl;	\
		\
		static ::Firebird::Udr::FunctionFactoryImpl<Impl, FB_UDR_STATUS_TYPE> factory(#name);	\
		\
		class Impl : public ::Firebird::Udr::Function<Impl, FB_UDR_STATUS_TYPE>	\
		{	\
		public:	\
			FB__UDR_COMMON_IMPL

#define FB_UDR_END_FUNCTION	\
		};	\
	}

#define FB_UDR_EXECUTE_FUNCTION	\
	void execute(FB_UDR_STATUS_TYPE* status, ::Firebird::IExternalContext* context, \
		void* in, void* out)	\
	{	\
		internalExecute(status, context, (InMessage::Type*) in, (OutMessage::Type*) out);	\
	}	\
	\
	void internalExecute(FB_UDR_STATUS_TYPE* status, ::Firebird::IExternalContext* context, \
		InMessage::Type* in, OutMessage::Type* out)


#define FB_UDR_BEGIN_PROCEDURE(name)	\
	namespace Proc##name	\
	{	\
		class Impl;	\
		\
		static ::Firebird::Udr::ProcedureFactoryImpl<Impl, FB_UDR_STATUS_TYPE> factory(#name);	\
		\
		class Impl : public ::Firebird::Udr::Procedure<Impl, FB_UDR_STATUS_TYPE>	\
		{	\
		public:	\
			FB__UDR_COMMON_IMPL

#define FB_UDR_END_PROCEDURE	\
			};	\
		};	\
	}

#define FB_UDR_EXECUTE_PROCEDURE	\
	::Firebird::IExternalResultSet* open(FB_UDR_STATUS_TYPE* status, \
		::Firebird::IExternalContext* context, void* in, void* out)	\
	{	\
		return new ResultSet(status, context, this, (InMessage::Type*) in, (OutMessage::Type*) out);	\
	}	\
	\
	class ResultSet : public ::Firebird::Udr::ResultSet<ResultSet, Impl, InMessage, OutMessage, FB_UDR_STATUS_TYPE>	\
	{	\
	public:	\
		ResultSet(FB_UDR_STATUS_TYPE* status, ::Firebird::IExternalContext* context,	\
				Impl* const procedure, InMessage::Type* const in, OutMessage::Type* const out)	\
			: ::Firebird::Udr::ResultSet<ResultSet, Impl, InMessage, OutMessage, FB_UDR_STATUS_TYPE>(	\
					context, procedure, in, out)

#define FB_UDR_FETCH_PROCEDURE	\
	FB_BOOLEAN fetch(FB_UDR_STATUS_TYPE* status)	\
	{	\
		return (FB_BOOLEAN) internalFetch(status);	\
	}	\
	\
	bool internalFetch(FB_UDR_STATUS_TYPE* status)


#define FB_UDR_BEGIN_TRIGGER(name)	\
	namespace Trig##name	\
	{	\
		class Impl;	\
		\
		static ::Firebird::Udr::TriggerFactoryImpl<Impl, FB_UDR_STATUS_TYPE> factory(#name);	\
		\
		class Impl : public ::Firebird::Udr::Trigger<Impl, FB_UDR_STATUS_TYPE>	\
		{	\
		public:	\
			FB__UDR_COMMON_IMPL

#define FB_UDR_END_TRIGGER	\
		};	\
	}

#define FB_UDR_EXECUTE_TRIGGER	\
	void execute(FB_UDR_STATUS_TYPE* status, ::Firebird::IExternalContext* context,	\
		unsigned int action, void* oldFields, void* newFields)	\
	{	\
		internalExecute(status, context, action,	\
			(FieldsMessage::Type*) oldFields, (FieldsMessage::Type*) newFields);	\
	}	\
	\
	void internalExecute(FB_UDR_STATUS_TYPE* status, ::Firebird::IExternalContext* context,	\
		unsigned int action, \
		FieldsMessage::Type* oldFields, FieldsMessage::Type* newFields)


#define FB_UDR_CONSTRUCTOR	\
	Impl(FB_UDR_STATUS_TYPE* const status, ::Firebird::IExternalContext* const context,	\
			::Firebird::IRoutineMetadata* const metadata__)	\
		: master(context->getMaster()),	\
		  metadata(metadata__)

#define FB_UDR_DESTRUCTOR	\
	~Impl()


#define FB_UDR_MESSAGE(name, fields)	\
	FB_MESSAGE(name, FB_UDR_STATUS_TYPE, fields)

#define FB_UDR_TRIGGER_MESSAGE(name, fields)	\
	FB_TRIGGER_MESSAGE(name, FB_UDR_STATUS_TYPE, fields)


#define FB__UDR_COMMON_IMPL	\
	Impl(const void* const, ::Firebird::IExternalContext* const context,	\
			::Firebird::IRoutineMetadata* const aMetadata)	\
		: master(context->getMaster()),	\
		  metadata(aMetadata)	\
	{	\
	}	\
	\
	::Firebird::IMaster* master;	\
	::Firebird::IRoutineMetadata* metadata;

#define FB__UDR_COMMON_TYPE(name)	\
	struct name	\
	{	\
		typedef unsigned char Type;	\
		static void setup(FB_UDR_STATUS_TYPE*, ::Firebird::IMetadataBuilder*) {}	\
	}


namespace Firebird
{
	namespace Udr
	{
//------------------------------------------------------------------------------


template <typename T, typename StatusType> class Procedure;


template <typename This, typename Procedure, typename InMessage, typename OutMessage, typename StatusType>
class ResultSet : public IExternalResultSetImpl<This, StatusType>
{
public:
	ResultSet(IExternalContext* aContext, Procedure* aProcedure,
				typename InMessage::Type* aIn, typename OutMessage::Type* aOut)
		: context(aContext),
		  procedure(aProcedure),
		  in(aIn),
		  out(aOut)
	{
	}

public:
	void dispose()
	{
		delete static_cast<This*>(this);
	}

protected:
	IExternalContext* const context;
	Procedure* const procedure;
	typename InMessage::Type* const in;
	typename OutMessage::Type* const out;
};


template <typename This, typename StatusType>
class Function : public IExternalFunctionImpl<This, StatusType>
{
public:
	FB__UDR_COMMON_TYPE(InMessage);
	FB__UDR_COMMON_TYPE(OutMessage);

	void dispose()
	{
		delete static_cast<This*>(this);
	}

	void getCharSet(StatusType* /*status*/, IExternalContext* /*context*/,
		char* /*name*/, unsigned /*nameSize*/)
	{
	}
};


template <typename This, typename StatusType>
class Procedure : public IExternalProcedureImpl<This, StatusType>
{
public:
	FB__UDR_COMMON_TYPE(InMessage);
	FB__UDR_COMMON_TYPE(OutMessage);

	void dispose()
	{
		delete static_cast<This*>(this);
	}

	void getCharSet(StatusType* /*status*/, IExternalContext* /*context*/,
		char* /*name*/, unsigned /*nameSize*/)
	{
	}
};


template <typename This, typename StatusType>
class Trigger : public IExternalTriggerImpl<This, StatusType>
{
public:
	FB__UDR_COMMON_TYPE(FieldsMessage);

	void dispose()
	{
		delete static_cast<This*>(this);
	}

	void getCharSet(StatusType* /*status*/, IExternalContext* /*context*/,
		char* /*name*/, unsigned /*nameSize*/)
	{
	}
};


template <typename T> struct RegistrationNode
{
	const char* name;
	T* factory;
	RegistrationNode<T>* next;
};

extern RegistrationNode<IUdrFunctionFactory>* regFunctions;
extern RegistrationNode<IUdrProcedureFactory>* regProcedures;
extern RegistrationNode<IUdrTriggerFactory>* regTriggers;

class FactoryRegistration
{
public:
	template <typename T> static void schedule(const char* name, T* factory,
		RegistrationNode<T>** list)
	{
		RegistrationNode<T>* node = new RegistrationNode<T>();
		node->name = name;
		node->factory = factory;
		node->next = *list;

		*list = node;
	}

	static void finish(IStatus* status, IUdrPlugin* plugin)
	{
		CheckStatusWrapper statusWrapper(status);

		if (!run<IUdrFunctionFactory>(&statusWrapper, plugin, &IUdrPlugin::registerFunction, regFunctions))
			return;

		if (!run<IUdrProcedureFactory>(&statusWrapper, plugin, &IUdrPlugin::registerProcedure, regProcedures))
			return;

		if (!run<IUdrTriggerFactory>(&statusWrapper, plugin, &IUdrPlugin::registerTrigger, regTriggers))
			return;
	}

private:
	template <typename T>
	static bool run(CheckStatusWrapper* statusWrapper, IUdrPlugin* plugin,
		void (IUdrPlugin::*routine)(CheckStatusWrapper* status, const char* name, T* factory),
		RegistrationNode<T>* list)
	{
		for (RegistrationNode<T>* node = list; node; node = node->next)
		{
			(plugin->*routine)(statusWrapper, node->name, node->factory);

			if (statusWrapper->getState() & IStatus::STATE_ERRORS)
				return false;
		}

		return true;
	}
};


template <typename T, typename StatusType> class FunctionFactoryImpl :
	public IUdrFunctionFactoryImpl<FunctionFactoryImpl<T, StatusType>, StatusType>
{
public:
	explicit FunctionFactoryImpl(const char* name)
	{
		FactoryRegistration::schedule<IUdrFunctionFactory>(name, this, &regFunctions);
	}

	void dispose()
	{
		// Do not delete this. The instances are statically allocated.
	}

	void setup(StatusType* status, IExternalContext* /*context*/,
		IRoutineMetadata* /*metadata*/, IMetadataBuilder* in, IMetadataBuilder* out)
	{
		T::InMessage::setup(status, in);
		T::OutMessage::setup(status, out);
	}

	IExternalFunction* newItem(StatusType* status, IExternalContext* context,
		IRoutineMetadata* metadata)
	{
		return new T(status, context, metadata);
	}
};


template <typename T, typename StatusType> class ProcedureFactoryImpl :
	public IUdrProcedureFactoryImpl<ProcedureFactoryImpl<T, StatusType>, StatusType>
{
public:
	explicit ProcedureFactoryImpl(const char* name)
	{
		FactoryRegistration::schedule<IUdrProcedureFactory>(name, this, &regProcedures);
	}

	void dispose()
	{
		// Do not delete this. The instances are statically allocated.
	}

	void setup(StatusType* status, IExternalContext* /*context*/,
		IRoutineMetadata* /*metadata*/, IMetadataBuilder* in, IMetadataBuilder* out)
	{
		T::InMessage::setup(status, in);
		T::OutMessage::setup(status, out);
	}

	IExternalProcedure* newItem(StatusType* status, IExternalContext* context,
		IRoutineMetadata* metadata)
	{
		return new T(status, context, metadata);
	}
};


template <typename T, typename StatusType> class TriggerFactoryImpl :
	public IUdrTriggerFactoryImpl<TriggerFactoryImpl<T, StatusType>, StatusType>
{
public:
	explicit TriggerFactoryImpl(const char* name)
	{
		FactoryRegistration::schedule<IUdrTriggerFactory>(name, this, &regTriggers);
	}

	void dispose()
	{
		// Do not delete this. The instances are statically allocated.
	}

	void setup(StatusType* status, IExternalContext* /*context*/,
		IRoutineMetadata* /*metadata*/, IMetadataBuilder* fields)
	{
		T::FieldsMessage::setup(status, fields);
	}

	IExternalTrigger* newItem(StatusType* status, IExternalContext* context,
		IRoutineMetadata* metadata)
	{
		return new T(status, context, metadata);
	}
};


//------------------------------------------------------------------------------
	}	// namespace Udr
}	// namespace Firebird

#endif	// FIREBIRD_UDR_CPP_ENGINE
