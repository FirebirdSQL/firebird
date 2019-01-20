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
#include "firebird/Interface.h"
#include <initializer_list>
#include <functional>

namespace Jrd
{
	struct SystemProcedureParameter
	{
		const char* name;
		USHORT fieldId;
		bool nullable;
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
		Firebird::Array<SystemProcedureParameter> inputParameters;
		Firebird::Array<SystemProcedureParameter> outputParameters;
	};

	struct SystemFunctionParameter
	{
		const char* name;
		USHORT fieldId;
		bool nullable;
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
		Firebird::Array<SystemFunctionParameter> parameters;
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
	};
}	// namespace Jrd

#endif	// JRD_SYSTEM_PACKAGES_H
