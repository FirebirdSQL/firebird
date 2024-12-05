/*
 *	PROGRAM:		Firebird utilities interface
 *	MODULE:			UtilSvc.h
 *	DESCRIPTION:	Interface making it possible to use same code
 *					as both utility or service
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
 *  Copyright (c) 2007 Alex Peshkov <peshkoff at mail dot ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_UTILFACE
#define FB_UTILFACE

#include "firebird/Interface.h"

#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"

namespace MsgFormat {
	class SafeArg;
}

namespace Firebird {

const TEXT SVC_TRMNTR = '\377';	// ASCII 255

class ClumpletWriter;

class UtilSvc : public Firebird::GlobalStorage
{
public:
	typedef Firebird::HalfStaticArray<const char*, 20> ArgvType;

	// Services is rare for our code case where status vector is accessed from 2 different threads
	// in async way. To ensure it's stability appropriate protection is needed.
	class StatusAccessor
	{
	public:
		StatusAccessor(Mutex& mtx, Firebird::CheckStatusWrapper* st, UtilSvc* u)
			: mutex(&mtx), status(st), uSvc(u)
		{
			mutex->enter(FB_FUNCTION);
		}

		StatusAccessor()
			: mutex(nullptr), status(nullptr), uSvc(nullptr)
		{ }

		StatusAccessor(StatusAccessor&& sa) noexcept
			: mutex(sa.mutex), status(sa.status), uSvc(sa.uSvc)
		{
			sa.mutex = nullptr;
			sa.uSvc = nullptr;
			sa.status = nullptr;
		}

		operator const Firebird::CheckStatusWrapper*() const
		{
			return status;
		}

		const Firebird::CheckStatusWrapper* operator->() const
		{
			return status;
		}

		void init()
		{
			if (status)
				status->init();
		}

		void setServiceStatus(const ISC_STATUS* status)
		{
			if (uSvc)
				uSvc->setServiceStatus(status);
		}

		void setServiceStatus(const USHORT fac, const USHORT code, const MsgFormat::SafeArg& args)
		{
			if (uSvc)
				uSvc->setServiceStatus(fac, code, args);
		}

		~StatusAccessor()
		{
			if (mutex)
				mutex->leave();
		}

		StatusAccessor(const StatusAccessor&) = delete;
		StatusAccessor& operator=(const StatusAccessor&) = delete;

	private:
		Mutex* mutex;
		Firebird::CheckStatusWrapper* status;
		UtilSvc* uSvc;
	};

public:
	UtilSvc() : argv(getPool()), usvcDataMode(false) { }

	virtual bool isService() = 0;
	virtual void started() = 0;
	virtual void outputVerbose(const char* text) = 0;
	virtual void outputError(const char* text) = 0;
	virtual void outputData(const void* text, FB_SIZE_T size) = 0;
	virtual void printf(bool err, const SCHAR* format, ...) = 0;
	virtual void putLine(char, const char*) = 0;
	virtual void putSLong(char, SLONG) = 0;
	virtual void putSInt64(char, SINT64) = 0;
	virtual void putChar(char, char) = 0;
	virtual void putBytes(const UCHAR*, FB_SIZE_T) = 0;
	virtual ULONG getBytes(UCHAR*, ULONG) = 0;

private:
	virtual void setServiceStatus(const ISC_STATUS*) = 0;
	virtual void setServiceStatus(const USHORT, const USHORT, const MsgFormat::SafeArg&) = 0;

public:
	virtual StatusAccessor getStatusAccessor() = 0;
	virtual void checkService() = 0;
	virtual void hidePasswd(ArgvType&, int) = 0;
	virtual void fillDpb(Firebird::ClumpletWriter& dpb) = 0;
	virtual bool finished() = 0;
	virtual unsigned int getAuthBlock(const unsigned char** bytes) = 0;
	virtual bool utf8FileNames() = 0;
	virtual Firebird::ICryptKeyCallback* getCryptCallback() = 0;
	virtual int getParallelWorkers() = 0;

	void setDataMode(bool value)
	{
		usvcDataMode = value;
	}

	virtual ~UtilSvc() { }

	static UtilSvc* createStandalone(int ac, char** argv);

	static inline void addStringWithSvcTrmntr(const Firebird::string& str, Firebird::string& switches)
	{
		// All string parameters are delimited by SVC_TRMNTR.
		// This is done to ensure that paths with spaces are handled correctly
		// when creating the argc / argv parameters for the service.
		// SVC_TRMNTRs inside the string are duplicated.

		switches += SVC_TRMNTR;
		for (FB_SIZE_T i = 0; i < str.length(); ++i)
		{
			if (str[i] == SVC_TRMNTR)
			{
				switches += SVC_TRMNTR;
			}
			switches += str[i];
		}
		switches += SVC_TRMNTR;
		switches += ' ';
	}

public:
	ArgvType argv;

protected:
	bool usvcDataMode;
};

} // namespace Firebird

#endif // FB_UTILFACE
