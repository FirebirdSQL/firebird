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
 *  Copyright (c) 2018 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_SYSTEM_PACKAGES_H
#define JRD_SYSTEM_PACKAGES_H

#include "firebird.h"
#include "../common/status.h"
#include "../common/classes/init.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../jrd/constants.h"
#include "../jrd/ini.h"
#include "firebird/Interface.h"
#include <initializer_list>
#include <functional>

namespace Jrd
{
	struct SystemProcedureParameter
	{
		SystemProcedureParameter(
			const char* aName,
			USHORT aFieldId,
			bool aNullable,
			const char* aDefaultText = nullptr,
			std::initializer_list<UCHAR> aDefaultBlr = {}
		)
			: name(aName),
			  fieldId(aFieldId),
			  nullable(aNullable),
			  defaultText(aDefaultText),
			  defaultBlr(*getDefaultMemoryPool(), aDefaultBlr)
		{
		}

		SystemProcedureParameter(Firebird::MemoryPool& pool, const SystemProcedureParameter& other)
			: defaultBlr(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT fieldId;
		bool nullable;
		const char* defaultText = nullptr;
		Firebird::Array<UCHAR> defaultBlr;
	};

	struct SystemProcedure
	{
		typedef std::function<Firebird::IExternalProcedure* (
				Firebird::ThrowStatusExceptionWrapper*,
				Firebird::IExternalContext*,
				Firebird::IRoutineMetadata*,
				Firebird::IMetadataBuilder*,
				Firebird::IMetadataBuilder*
			)> Factory;

		SystemProcedure(
			Firebird::MemoryPool& pool,
			const char* aName,
			Factory aFactory,
			prc_t aType,
			std::initializer_list<SystemProcedureParameter> aInputParameters,
			std::initializer_list<SystemProcedureParameter> aOutputParameters
		)
			: name(aName),
			  factory(aFactory),
			  type(aType),
			  inputParameters(pool, aInputParameters),
			  outputParameters(pool, aOutputParameters)
		{
		}

		SystemProcedure(Firebird::MemoryPool& pool, const SystemProcedure& other)
			: inputParameters(pool),
			  outputParameters(pool)
		{
			*this = other;
		}

		const char* name;
		Factory factory;
		prc_t type;
		Firebird::ObjectsArray<SystemProcedureParameter> inputParameters;
		Firebird::ObjectsArray<SystemProcedureParameter> outputParameters;
	};

	struct SystemFunctionParameter
	{
		SystemFunctionParameter(
			const char* aName,
			USHORT aFieldId,
			bool aNullable,
			const char* aDefaultText = nullptr,
			std::initializer_list<UCHAR> aDefaultBlr = {}
		)
			: name(aName),
			  fieldId(aFieldId),
			  nullable(aNullable),
			  defaultText(aDefaultText),
			  defaultBlr(*getDefaultMemoryPool(), aDefaultBlr)
		{
		}

		SystemFunctionParameter(Firebird::MemoryPool& pool, const SystemFunctionParameter& other)
			: defaultBlr(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT fieldId;
		bool nullable;
		const char* defaultText = nullptr;
		Firebird::Array<UCHAR> defaultBlr;
	};

	struct SystemFunctionReturnType
	{
		USHORT fieldId;
		bool nullable;
	};

	struct SystemFunction
	{
		typedef std::function<Firebird::IExternalFunction* (
				Firebird::ThrowStatusExceptionWrapper*,
				Firebird::IExternalContext*,
				Firebird::IRoutineMetadata*,
				Firebird::IMetadataBuilder*,
				Firebird::IMetadataBuilder*
			)> Factory;

		SystemFunction(
			Firebird::MemoryPool& pool,
			const char* aName,
			Factory aFactory,
			std::initializer_list<SystemFunctionParameter> aParameters,
			SystemFunctionReturnType aReturnType
		)
			: name(aName),
			  factory(aFactory),
			  parameters(pool, aParameters),
			  returnType(aReturnType)
		{
		}

		SystemFunction(Firebird::MemoryPool& pool, const SystemFunction& other)
			: parameters(pool)
		{
			*this = other;
		}

		const char* name;
		Factory factory;
		Firebird::ObjectsArray<SystemFunctionParameter> parameters;
		SystemFunctionReturnType returnType;
	};

	struct SystemPackage
	{
		SystemPackage(
			Firebird::MemoryPool& pool,
			const char* aName,
			USHORT aOdsVersion,
			std::initializer_list<SystemProcedure> aProcedures,
			std::initializer_list<SystemFunction> aFunctions
		)
			: name(aName),
			  odsVersion(aOdsVersion),
			  procedures(pool, aProcedures),
			  functions(pool, aFunctions)
		{
		}

		SystemPackage(Firebird::MemoryPool& pool, const SystemPackage& other)
			: procedures(pool),
			  functions(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT odsVersion;
		Firebird::ObjectsArray<SystemProcedure> procedures;
		Firebird::ObjectsArray<SystemFunction> functions;

		static Firebird::ObjectsArray<SystemPackage>& get();

	private:
		SystemPackage(const SystemPackage&) = delete;
		SystemPackage& operator=(SystemPackage const&) = default;
	};

	class VoidMessage
	{
	public:
		typedef void Type;

	public:
		static void setup(Firebird::ThrowStatusExceptionWrapper*, Firebird::IMetadataBuilder*)
		{
		}
	};

	template <
		typename Input,
		typename Output,
		Firebird::IExternalResultSet* (*OpenFunction)(
			Firebird::ThrowStatusExceptionWrapper*,
			Firebird::IExternalContext*,
			const typename Input::Type*,
			typename Output::Type*
		)
	>
	struct SystemProcedureFactory
	{
		class SystemProcedureImpl :
			public
				Firebird::DisposeIface<
					Firebird::IExternalProcedureImpl<
						SystemProcedureImpl,
						Firebird::ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemProcedureImpl(Firebird::ThrowStatusExceptionWrapper* status,
				Firebird::IMetadataBuilder* inBuilder, Firebird::IMetadataBuilder* outBuilder)
			{
				Input::setup(status, inBuilder);
				Output::setup(status, outBuilder);
			}

		public:
			void dispose() override
			{
				delete this;
			}

		public:
			void getCharSet(Firebird::ThrowStatusExceptionWrapper* status, Firebird::IExternalContext* context,
				char* name, unsigned nameSize) override
			{
				strncpy(name, "UTF8", nameSize);
			}

			Firebird::IExternalResultSet* open(Firebird::ThrowStatusExceptionWrapper* status,
				Firebird::IExternalContext* context, void* inMsg, void* outMsg) override
			{
				return OpenFunction(status, context,
					static_cast<typename Input::Type*>(inMsg),
					static_cast<typename Output::Type*>(outMsg));
			}
		};

		SystemProcedureImpl* operator()(
			Firebird::ThrowStatusExceptionWrapper* status,
			Firebird::IExternalContext* /*context*/,
			Firebird::IRoutineMetadata* /*metadata*/,
			Firebird::IMetadataBuilder* inBuilder,
			Firebird::IMetadataBuilder* outBuilder)
		{
			return FB_NEW SystemProcedureImpl(status, inBuilder, outBuilder);
		}
	};

	template <
		typename Input,
		typename Output,
		void (*ExecFunction)(
			Firebird::ThrowStatusExceptionWrapper*,
			Firebird::IExternalContext*,
			const typename Input::Type*,
			typename Output::Type*
		)
	>
	struct SystemFunctionFactory
	{
		class SystemFunctionImpl :
			public
				Firebird::DisposeIface<
					Firebird::IExternalFunctionImpl<
						SystemFunctionImpl,
						Firebird::ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemFunctionImpl(Firebird::ThrowStatusExceptionWrapper* status,
				Firebird::IMetadataBuilder* inBuilder, Firebird::IMetadataBuilder* outBuilder)
			{
				Input::setup(status, inBuilder);
				Output::setup(status, outBuilder);
			}

		public:
			void getCharSet(Firebird::ThrowStatusExceptionWrapper* status, Firebird::IExternalContext* context,
				char* name, unsigned nameSize) override
			{
				strncpy(name, "UTF8", nameSize);
			}

			void execute(Firebird::ThrowStatusExceptionWrapper* status,
				Firebird::IExternalContext* context, void* inMsg, void* outMsg) override
			{
				ExecFunction(status, context,
					static_cast<typename Input::Type*>(inMsg),
					static_cast<typename Output::Type*>(outMsg));
			}
		};

		SystemFunctionImpl* operator()(
			Firebird::ThrowStatusExceptionWrapper* status,
			Firebird::IExternalContext* /*context*/,
			Firebird::IRoutineMetadata* /*metadata*/,
			Firebird::IMetadataBuilder* inBuilder,
			Firebird::IMetadataBuilder* outBuilder)
		{
			return FB_NEW SystemFunctionImpl(status, inBuilder, outBuilder);
		}
	};
}	// namespace Jrd

#endif	// JRD_SYSTEM_PACKAGES_H
