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
#include "../common/dsc.h"
#include "../common/classes/init.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/constants.h"
#include "../jrd/ini.h"
#include "../jrd/jrd.h"
#include "firebird/Interface.h"
#include <initializer_list>
#include <functional>

namespace Firebird::Jrd
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

		SystemProcedureParameter(MemoryPool& pool, const SystemProcedureParameter& other)
			: defaultBlr(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT fieldId;
		bool nullable;
		const char* defaultText = nullptr;
		Array<UCHAR> defaultBlr;
	};

	struct SystemProcedure
	{
		typedef std::function<IExternalProcedure* (
				ThrowStatusExceptionWrapper*,
				IExternalContext*,
				IRoutineMetadata*,
				IMetadataBuilder*,
				IMetadataBuilder*
			)> Factory;

		SystemProcedure(
			MemoryPool& pool,
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

		SystemProcedure(MemoryPool& pool, const SystemProcedure& other)
			: inputParameters(pool),
			  outputParameters(pool)
		{
			*this = other;
		}

		const char* name;
		Factory factory;
		prc_t type;
		ObjectsArray<SystemProcedureParameter> inputParameters;
		ObjectsArray<SystemProcedureParameter> outputParameters;
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

		SystemFunctionParameter(MemoryPool& pool, const SystemFunctionParameter& other)
			: defaultBlr(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT fieldId;
		bool nullable;
		const char* defaultText = nullptr;
		Array<UCHAR> defaultBlr;
	};

	struct SystemFunctionReturnType
	{
		USHORT fieldId;
		bool nullable;
	};

	struct SystemFunction
	{
		typedef std::function<IExternalFunction* (
				ThrowStatusExceptionWrapper*,
				IExternalContext*,
				IRoutineMetadata*,
				IMetadataBuilder*,
				IMetadataBuilder*
			)> Factory;

		SystemFunction(
			MemoryPool& pool,
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

		SystemFunction(MemoryPool& pool, const SystemFunction& other)
			: parameters(pool)
		{
			*this = other;
		}

		const char* name;
		Factory factory;
		ObjectsArray<SystemFunctionParameter> parameters;
		SystemFunctionReturnType returnType;
	};

	struct SystemPackage
	{
		SystemPackage(
			MemoryPool& pool,
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

		SystemPackage(MemoryPool& pool, const SystemPackage& other)
			: procedures(pool),
			  functions(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT odsVersion;
		ObjectsArray<SystemProcedure> procedures;
		ObjectsArray<SystemFunction> functions;

		static ObjectsArray<SystemPackage>& get();

	private:
		SystemPackage(const SystemPackage&) = delete;
		SystemPackage& operator=(SystemPackage const&) = default;
	};

	class VoidMessage
	{
	public:
		typedef void Type;

	public:
		static void setup(ThrowStatusExceptionWrapper*, IMetadataBuilder*)
		{
		}
	};

	template <
		typename Input,
		typename Output,
		IExternalResultSet* (*OpenFunction)(
			ThrowStatusExceptionWrapper*,
			IExternalContext*,
			const typename Input::Type*,
			typename Output::Type*
		)
	>
	struct SystemProcedureFactory
	{
		class SystemResultSet :
			public
				DisposeIface<
					IExternalResultSetImpl<
						SystemResultSet,
						ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemResultSet(Attachment* aAttachment, IExternalResultSet* aResultSet)
				: attachment(aAttachment),
				  resultSet(aResultSet)
			{
			}

		public:
			void dispose() override
			{
				delete this;
			}

		public:
			FB_BOOLEAN fetch(ThrowStatusExceptionWrapper* status) override
			{
				// See comment in Attachment.h.
				// AutoSetRestore<bool> autoInSystemPackage(&attachment->att_in_system_routine, true);

				return resultSet->fetch(status);
			}

		private:
			Attachment* attachment;
			AutoDispose<IExternalResultSet> resultSet;
		};

		class SystemProcedureImpl :
			public
				DisposeIface<
					IExternalProcedureImpl<
						SystemProcedureImpl,
						ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemProcedureImpl(ThrowStatusExceptionWrapper* status,
				IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
			{
				const auto tdbb = JRD_get_thread_data();
				attachment = tdbb->getAttachment();

				Input::setup(status, inBuilder);
				Output::setup(status, outBuilder);
			}

		public:
			void dispose() override
			{
				delete this;
			}

		public:
			void getCharSet(ThrowStatusExceptionWrapper* status, IExternalContext* context,
				char* name, unsigned nameSize) override
			{
				strncpy(name, "UTF8", nameSize);
			}

			IExternalResultSet* open(ThrowStatusExceptionWrapper* status,
				IExternalContext* context, void* inMsg, void* outMsg) override
			{
				// See comment in Attachment.h.
				// AutoSetRestore<bool> autoInSystemPackage(&attachment->att_in_system_routine, true);

				const auto resultSet = OpenFunction(status, context,
					static_cast<typename Input::Type*>(inMsg),
					static_cast<typename Output::Type*>(outMsg));

				return resultSet ? FB_NEW SystemResultSet(attachment, resultSet) : nullptr;
			}

		private:
			Attachment* attachment;
		};

		SystemProcedureImpl* operator()(
			ThrowStatusExceptionWrapper* status,
			IExternalContext* /*context*/,
			IRoutineMetadata* /*metadata*/,
			IMetadataBuilder* inBuilder,
			IMetadataBuilder* outBuilder)
		{
			return FB_NEW SystemProcedureImpl(status, inBuilder, outBuilder);
		}
	};

	template <
		typename Input,
		typename Output,
		void (*ExecFunction)(
			ThrowStatusExceptionWrapper*,
			IExternalContext*,
			const typename Input::Type*,
			typename Output::Type*
		)
	>
	struct SystemFunctionFactory
	{
		class SystemFunctionImpl :
			public
				DisposeIface<
					IExternalFunctionImpl<
						SystemFunctionImpl,
						ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemFunctionImpl(ThrowStatusExceptionWrapper* status,
				IMetadataBuilder* inBuilder, IMetadataBuilder* outBuilder)
			{
				const auto tdbb = JRD_get_thread_data();
				attachment = tdbb->getAttachment();

				Input::setup(status, inBuilder);
				Output::setup(status, outBuilder);
			}

		public:
			void getCharSet(ThrowStatusExceptionWrapper* status, IExternalContext* context,
				char* name, unsigned nameSize) override
			{
				strncpy(name, "UTF8", nameSize);
			}

			void execute(ThrowStatusExceptionWrapper* status,
				IExternalContext* context, void* inMsg, void* outMsg) override
			{
				// See comment in Attachment.h.
				// AutoSetRestore<bool> autoInSystemPackage(&attachment->att_in_system_routine, true);

				ExecFunction(status, context,
					static_cast<typename Input::Type*>(inMsg),
					static_cast<typename Output::Type*>(outMsg));
			}

		private:
			Attachment* attachment;
		};

		SystemFunctionImpl* operator()(
			ThrowStatusExceptionWrapper* status,
			IExternalContext* /*context*/,
			IRoutineMetadata* /*metadata*/,
			IMetadataBuilder* inBuilder,
			IMetadataBuilder* outBuilder)
		{
			return FB_NEW SystemFunctionImpl(status, inBuilder, outBuilder);
		}
	};
}	// namespace Firebird::Jrd

#endif	// JRD_SYSTEM_PACKAGES_H
