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
#include "../jrd/CacheVector.h"
#include "../jrd/Resources.h"
#include "../common/classes/NestConst.h"
#include "../common/MsgMetadata.h"
#include "../common/classes/auto.h"
#include "../common/ThreadStart.h"
#include "../common/sha2/sha2.h"

namespace Jrd
{
	class thread_db;
	class CompilerScratch;
	class Statement;
	class Lock;
	class Format;
	class Parameter;
	class UserId;

	class RoutinePermanent : public Firebird::PermanentStorage
	{
	public:
		explicit RoutinePermanent(thread_db* tdbb, MemoryPool& p, MetaId metaId, NoData);

		explicit RoutinePermanent(MemoryPool& p)
			: PermanentStorage(p),
			  id(~0),
			  name(p),
			  securityName(p),
			  subRoutine(true)
		{ }

		MetaId getId() const
		{
			fb_assert(!subRoutine);
			return id;
		}

		static bool destroy(thread_db* tdbb, RoutinePermanent* routine);
		void releaseLock(thread_db*) { }

		const QualifiedName& getName() const noexcept { return name; }
		void setName(const QualifiedName& value) { name = value; }

		const QualifiedName& getSecurityName() const noexcept { return securityName; }
		void setSecurityName(const QualifiedName& value) { securityName = value; }

		bool hasData() const { return name.hasData(); }

		bool isSubRoutine() const { return subRoutine; }
		void setSubRoutine(bool value) { subRoutine = value; }

	public:
		MetaId id;							// routine ID
		QualifiedName name;					// routine name
		QualifiedName securityName;				// security class name
		bool subRoutine;                    // Is this a subroutine?
		MetaName owner;
	};

	class Routine : public ObjectBase
	{
	protected:
		explicit Routine(MemoryPool& p)
			: statement(NULL),
			  implemented(false),
			  defined(true),
			  defaultCount(0),
			  inputFormat(NULL),
			  outputFormat(NULL),
			  inputFields(p),
			  outputFields(p),
			  flReload(false),
			  invoker(NULL)
		{
		}

	public:
		virtual ~Routine()
		{
		}

	public:
		static Firebird::MsgMetadata* createMetadata(
			const Firebird::Array<NestConst<Parameter> >& parameters, bool isExtern);
		static Format* createFormat(MemoryPool& pool, Firebird::IMessageMetadata* params, bool addEof);

	public:
		static void destroy(thread_db* tdbb, Routine* routine);

		const QualifiedName& getName() const noexcept { return getPermanent()->getName(); }
		MetaId getId() const noexcept { return getPermanent()->getId(); }

		/*const*/ Statement* getStatement() const noexcept { return statement; }
		void setStatement(Statement* value);

		bool isImplemented() const noexcept { return implemented; }
		void setImplemented(bool value) noexcept { implemented = value; }

		bool isDefined() const noexcept { return defined; }
		void setDefined(bool value) noexcept { defined = value; }

		virtual void checkReload(thread_db* tdbb) const = 0;

		USHORT getDefaultCount() const noexcept { return defaultCount; }
		void setDefaultCount(USHORT value) noexcept { defaultCount = value; }

		const Format* getInputFormat() const noexcept { return inputFormat; }
		void setInputFormat(const Format* value) noexcept { inputFormat = value; }

		const Format* getOutputFormat() const noexcept { return outputFormat; }
		void setOutputFormat(const Format* value) noexcept { outputFormat = value; }

		bool hash(thread_db* tdbb, Firebird::sha512& digest);

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

		virtual void releaseFormat()
		{
		}

		void afterDecrement(Jrd::thread_db*);
		void releaseStatement(thread_db* tdbb);
		//void remove(thread_db* tdbb);
		virtual void releaseExternal()
		{
		}

		void sharedCheckUnlock(thread_db* tdbb);

	public:
		virtual RoutinePermanent* getPermanent() const noexcept = 0;	// Permanent part of data
		virtual int getObjectType() const noexcept = 0;
		virtual SLONG getSclType() const noexcept = 0;

	private:
		USHORT id;					// routine ID
		Statement* statement;		// compiled routine statement
		bool implemented;			// Is the packaged routine missing the body/entrypoint?
		bool defined;				// UDF has its implementation module available
		USHORT defaultCount;		// default input arguments
		const Format* inputFormat;	// input format
		const Format* outputFormat;	// output format
		Firebird::Array<NestConst<Parameter> > inputFields;		// array of field blocks
		Firebird::Array<NestConst<Parameter> > outputFields;	// array of field blocks

	public:
		bool flReload;
		bool compiling = false;		// Do not try to load routine's BLR in scan/reload during compile

	public:
		Jrd::UserId* invoker;		// Invoker ID
	};


	template <class R>
	class SubRoutine				// supports NestConst logic
	{
	public:
		SubRoutine()
			: routine(), subroutine(nullptr)
		{ }

		SubRoutine(const CachedResource<R, RoutinePermanent>& r)
			: routine(r), subroutine(nullptr)
		{ }

		SubRoutine(R* r)
			: routine(), subroutine(r)
		{ }

		SubRoutine& operator=(const CachedResource<R, RoutinePermanent>& r)
		{
			routine = r;
			subroutine = nullptr;
			return *this;
		}

		SubRoutine& operator=(R* r)
		{
			routine.clear();
			subroutine = r;
			return *this;
		}

		template <typename T>
		R* operator()(T t)
		{
			return isSubRoutine() ? subroutine : routine.isSet() ? routine(t) : nullptr;
		}

		template <typename T>
		const R* operator()(T t) const
		{
			return isSubRoutine() ? subroutine : routine.isSet() ? routine(t) : nullptr;
		}

		CacheElement<R, RoutinePermanent>* operator()()
		{
			return isSubRoutine() ? subroutine->getPermanent() : routine.isSet() ? routine() : nullptr;
		}

		const CacheElement<R, RoutinePermanent>* operator()() const
		{
			return isSubRoutine() ? subroutine->getPermanent() : routine.isSet() ? routine() : nullptr;
		}

		bool isSubRoutine() const
		{
			return subroutine != nullptr;
		}

		operator bool() const
		{
			fb_assert((routine.isSet() ? 1 : 0) + (subroutine ? 1 : 0) < 2);
			return routine.isSet() || subroutine;
		}

	private:
		CachedResource<R, RoutinePermanent> routine;
		R* subroutine;
	};

}

#endif // JRD_ROUTINE_H
