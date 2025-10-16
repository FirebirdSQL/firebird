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

#ifndef JRD_ROUTINE_H
#define JRD_ROUTINE_H

#include "../common/classes/array.h"
#include "../common/classes/alloc.h"
#include "../common/classes/BlrReader.h"
#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/NestConst.h"
#include "../common/MsgMetadata.h"

namespace Jrd
{
	class thread_db;
	class CompilerScratch;
	class Statement;
	class Lock;
	class Format;
	class Parameter;
	class UserId;

	class Routine : public Firebird::PermanentStorage
	{
	protected:
		explicit Routine(MemoryPool& p)
			: PermanentStorage(p),
			  id(0),
			  name(p),
			  securityName(p),
			  statement(NULL),
			  subRoutine(false),
			  implemented(true),
			  defined(true),
			  defaultCount(0),
			  inputFormat(NULL),
			  outputFormat(NULL),
			  inputFields(p),
			  outputFields(p),
			  flags(0),
			  useCount(0),
			  intUseCount(0),
			  alterCount(0),
			  existenceLock(NULL),
			  invoker(NULL)
		{
		}

	public:
		virtual ~Routine()
		{
		}

	public:
		static constexpr USHORT FLAG_SCANNED			= 1;	// Field expressions scanned
		static constexpr USHORT FLAG_OBSOLETE			= 2;	// Procedure known gonzo
		static constexpr USHORT FLAG_BEING_SCANNED		= 4;	// New procedure needs dependencies during scan
		static constexpr USHORT FLAG_BEING_ALTERED		= 8;	// Procedure is getting altered
															// This flag is used to make sure that MET_remove_routine
															// does not delete and remove procedure block from cache
															// so dfw.epp:modify_procedure() can flip procedure body without
															// invalidating procedure pointers from other parts of metadata cache
		static constexpr USHORT FLAG_CHECK_EXISTENCE	= 16;	// Existence lock released
		static constexpr USHORT FLAG_RELOAD		 		= 32;	// Recompile before execution
		static constexpr USHORT FLAG_CLEARED			= 64;	// Routine cleared but not removed from cache

		static constexpr USHORT MAX_ALTER_COUNT = 64;	// Number of times an in-cache routine can be altered

		static Firebird::MsgMetadata* createMetadata(
			const Firebird::Array<NestConst<Parameter> >& parameters, bool isExtern);
		static Format* createFormat(MemoryPool& pool, Firebird::IMessageMetadata* params, bool addEof);

	public:
		USHORT getId() const noexcept
		{
			fb_assert(!subRoutine);
			return id;
		}

		void setId(USHORT value) noexcept { id = value; }

		const QualifiedName& getName() const noexcept { return name; }
		void setName(const QualifiedName& value) { name = value; }

		const QualifiedName& getSecurityName() const noexcept { return securityName; }
		void setSecurityName(const QualifiedName& value) { securityName = value; }

		/*const*/ Statement* getStatement() const noexcept { return statement; }
		void setStatement(Statement* value);

		bool isSubRoutine() const noexcept { return subRoutine; }
		void setSubRoutine(bool value) noexcept { subRoutine = value; }

		bool isImplemented() const noexcept { return implemented; }
		void setImplemented(bool value) noexcept { implemented = value; }

		bool isDefined() const noexcept { return defined; }
		void setDefined(bool value) noexcept { defined = value; }

		void checkReload(thread_db* tdbb);

		USHORT getDefaultCount() const noexcept { return defaultCount; }
		void setDefaultCount(USHORT value) noexcept { defaultCount = value; }

		const Format* getInputFormat() const noexcept { return inputFormat; }
		void setInputFormat(const Format* value) noexcept { inputFormat = value; }

		const Format* getOutputFormat() const noexcept { return outputFormat; }
		void setOutputFormat(const Format* value) noexcept { outputFormat = value; }

		const Firebird::Array<NestConst<Parameter> >& getInputFields() const noexcept
		{
			return inputFields;
		}
		Firebird::Array<NestConst<Parameter> >& getInputFields() noexcept { return inputFields; }

		const Firebird::Array<NestConst<Parameter> >& getOutputFields() const noexcept
		{
			return outputFields;
		}
		Firebird::Array<NestConst<Parameter> >& getOutputFields() noexcept { return outputFields; }

		void parseBlr(thread_db* tdbb, CompilerScratch* csb, const bid* blob_id, bid* blobDbg);
		void parseMessages(thread_db* tdbb, CompilerScratch* csb, Firebird::BlrReader blrReader);

		bool isUsed() const noexcept
		{
			return useCount != 0;
		}

		void addRef() noexcept
		{
			++useCount;
		}

		virtual void releaseFormat()
		{
		}

		void release(thread_db* tdbb);
		void releaseStatement(thread_db* tdbb);
		void remove(thread_db* tdbb);
		virtual void releaseExternal() {};

	public:
		virtual int getObjectType() const noexcept = 0;
		virtual SLONG getSclType() const noexcept = 0;
		virtual bool checkCache(thread_db* tdbb) const = 0;
		virtual void clearCache(thread_db* tdbb) = 0;

	private:
		USHORT id;							// routine ID
		QualifiedName name;				// routine name
		QualifiedName securityName;		// security class name
		Statement* statement;			// compiled routine statement
		bool subRoutine;					// Is this a subroutine?
		bool implemented;					// Is the packaged routine missing the body/entrypoint?
		bool defined;						// UDF has its implementation module available
		USHORT defaultCount;				// default input arguments
		const Format* inputFormat;			// input format
		const Format* outputFormat;			// output format
		Firebird::Array<NestConst<Parameter> > inputFields;		// array of field blocks
		Firebird::Array<NestConst<Parameter> > outputFields;	// array of field blocks

	protected:

		virtual bool reload(thread_db* tdbb) = 0;

	public:
		USHORT flags;
		USHORT useCount;		// requests compiled with routine
		SSHORT intUseCount;		// number of routines compiled with routine, set and
								// used internally in the MET_clear_cache routine
								// no code should rely on value of this field
								// (it will usually be 0)
		USHORT alterCount;		// No. of times the routine was altered
		Lock* existenceLock;	// existence lock, if any

		MetaName owner;
		Jrd::UserId* invoker;		// Invoker ID
	};
}

#endif // JRD_ROUTINE_H
