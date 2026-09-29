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
 *  The Original Code was created by Adriano dos Santos Fernandes.
 *
 *  Copyright (c) 2014 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include <stdint.h>
#include "CalcCppApi.h"
#include <stdio.h>
#include <assert.h>

#ifdef WIN32
#include <windows.h>
#define DLL_EXPORT __declspec(dllexport)
#else
#include <dlfcn.h>
#define DLL_EXPORT __attribute__((visibility("default")))
#endif


//--------------------------------------

// CalcException


class CalcException
{
public:
	CalcException(calc::IStatus* status)
		: code(status ? status->getCode() : -1)
	{
	}

	CalcException(int code)
		: code(code)
	{
	}

public:
	int code;
};


//--------------------------------------

// StatusWrapper


class StatusWrapper : public calc::IStatusImpl<StatusWrapper, StatusWrapper>
{
public:
	StatusWrapper(calc::IStatus* delegate)
		: delegate(delegate),
		  code(0)
	{
	}

	virtual void dispose()
	{
		delete this;
	}

	virtual int getCode() const
	{
		return code;
	}

	virtual void setCode(int code)
	{
		this->code = code;
		delegate->setCode(code);
	}

	static void clearException(StatusWrapper* status)
	{
		if (status->code != 0)
			status->setCode(0);
	}

	static void checkException(StatusWrapper* status)
	{
		if (status->code != 0)
			throw CalcException(status);
	}

	static void catchException(StatusWrapper* status)
	{
		try
		{
			throw;
		}
		catch (const CalcException& e)
		{
			assert(status);
			status->setCode(e.code);
		}
		catch (...)
		{
			assert(false);
		}
	}

	static void setVersionError(StatusWrapper* status, const char* /*interfaceName*/,
		unsigned /*currentVersion*/, unsigned /*expectedVersion*/)
	{
		status->setCode(calc::IStatus::ERROR_1);
	}

private:
	calc::IStatus* delegate;
	int code;
};


//--------------------------------------

// StatusImpl


class StatusImpl : public calc::IStatusImpl<StatusImpl, StatusWrapper>
{
public:
	StatusImpl()
		: code(0)
	{
	}

	virtual void dispose()
	{
		delete this;
	}

	virtual int getCode() const
	{
		return code;
	}

	virtual void setCode(int code)
	{
		this->code = code;
	}

private:
	int code;
};


//--------------------------------------

// CalculatorImpl


class CalculatorImpl : public calc::ICalculatorImpl<CalculatorImpl, StatusWrapper>
{
public:
	CalculatorImpl()
		: memory(0)
	{
	}

	virtual void dispose()
	{
		delete this;
	}

	virtual int sum(StatusWrapper* status, int n1, int n2) const
	{
		if (n1 + n2 > 1000)
			throw CalcException(calc::IStatus::ERROR_1);
		else
			return n1 + n2;
	}

	virtual int getMemory() const
	{
		return memory;
	}

	virtual void setMemory(int n)
	{
		memory = n;
	}

	virtual void sumAndStore(StatusWrapper* status, int n1, int n2)
	{
		setMemory(sum(status, n1, n2));
	}

private:
	int memory;
};


//--------------------------------------

// Calculator2Impl


class Calculator2Impl : public calc::ICalculator2Impl<Calculator2Impl, StatusWrapper>
{
public:
	Calculator2Impl()
		: memory(0)
	{
	}

	virtual void dispose()
	{
		delete this;
	}

	virtual int sum(StatusWrapper* status, int n1, int n2) const
	{
		if (n1 + n2 > 1000)
			throw CalcException(calc::IStatus::ERROR_1);
		else
			return n1 + n2;
	}

	virtual int getMemory() const
	{
		return memory;
	}

	virtual void setMemory(int n)
	{
		memory = n;
	}

	virtual void sumAndStore(StatusWrapper* status, int n1, int n2)
	{
		setMemory(sum(status, n1, n2));
	}

	virtual int multiply(StatusWrapper* status, int n1, int n2) const
	{
		return n1 * n2;
	}

	virtual void copyMemory(const calc::ICalculator* calculator)
	{
		setMemory(calculator->getMemory());
	}

	virtual void copyMemory2(const int* address)
	{
		setMemory(*address);
	}

private:
	int memory;
};


//--------------------------------------

// BrokenCalculatorImpl


class BrokenCalculatorImpl :
	public calc::ICalculatorBaseImpl<BrokenCalculatorImpl, StatusWrapper, CalculatorImpl>
{
public:
	virtual int sum(StatusWrapper* status, int n1, int n2) const
	{
		return CalculatorImpl::sum(status, n1, n2) + 1;
	}
};


//--------------------------------------

// FactoryImpl


class FactoryImpl : public calc::IFactoryImpl<FactoryImpl, StatusWrapper>
{
public:
	virtual void dispose()
	{
		delete this;
	}

	virtual calc::IStatus* createStatus()
	{
		return statusFactory ? statusFactory->createStatus() : new StatusImpl();
	}

	virtual calc::ICalculator* createCalculator(StatusWrapper* /*status*/)
	{
		return new CalculatorImpl();
	}

	virtual calc::ICalculator2* createCalculator2(StatusWrapper* /*status*/)
	{
		return new Calculator2Impl();
	}

	virtual calc::ICalculator* createBrokenCalculator(StatusWrapper* /*status*/)
	{
		return new BrokenCalculatorImpl();
	}

	virtual void setStatusFactory(calc::IStatusFactory* statusFactory)
	{
		this->statusFactory = statusFactory;
	}

private:
	calc::IStatusFactory* statusFactory;
};


//--------------------------------------

// Library entry point


extern "C" DLL_EXPORT calc::IFactory* createFactory()
{
	return new FactoryImpl();
}


//--------------------------------------


static void test(calc::IFactory* (*createFactory)())
{
	calc::IFactory* factory = createFactory();
	StatusImpl statusImpl;
	StatusWrapper status(&statusImpl);

	calc::ICalculator* calculator = factory->createCalculator(&status);

	calculator->sumAndStore(&status, 1, 22);
	printf("%d\n", calculator->getMemory());	// 23

	calculator->setMemory(calculator->sum(&status, 2, 33));
	printf("%d\n", calculator->getMemory());	// 35

	calc::ICalculator2* calculator2 = factory->createCalculator2(&status);

	calculator2->copyMemory(calculator);
	printf("%d\n", calculator2->getMemory());	// 35

	int address = 40;
	calculator2->copyMemory2(&address);
	printf("%d\n", calculator2->getMemory());	// 40

	calculator->dispose();
	calculator = calculator2;

	calculator->sumAndStore(&status, 1, 22);
	printf("%d\n", calculator->getMemory());	// 23

	calculator->setMemory(calculator->sum(&status, 2, 33));
	printf("%d\n", calculator->getMemory());	// 35

	calculator2->sumAndStore(&status, 1, 22);
	printf("%d\n", calculator2->getMemory());	// 23

	calculator2->setMemory(calculator2->multiply(&status, 2, 33));
	printf("%d\n", calculator2->getMemory());	// 66

	calculator->dispose();

	calculator = factory->createBrokenCalculator(&status);

	calculator->sumAndStore(&status, 1, 22);
	printf("%d\n", calculator->getMemory());	// 24

	calculator->setMemory(calculator->sum(&status, 2, 33));
	printf("%d\n", calculator->getMemory());	// 36

	try
	{
		printf("%d\n", calculator->sum(&status, 600, 600));
	}
	catch (const CalcException& e)
	{
		printf("exception %d\n", e.code);	// exception 1
	}

	calculator->dispose();
	factory->dispose();

	printf("\n");
}

#ifdef WIN32
template <typename T>
static void loadSymbol(HMODULE library, const char* name, T& symbol)
{
	symbol = (T) GetProcAddress(library, name);
}
#else
template <typename T>
static void loadSymbol(void* library, const char* name, T& symbol)
{
	symbol = (T) dlsym(library, name);
}
#endif

int main(int argc, char* argv[])
{
	calc::IFactory* (*createFactory)();

#ifdef WIN32
	HMODULE library = LoadLibrary(argv[1]);
#else
	void* library = dlopen(argv[1], RTLD_LAZY);
#endif

	loadSymbol(library, "createFactory", createFactory);
	test(createFactory);

#ifdef WIN32
	FreeLibrary(library);
#else
	dlclose(library);
#endif

	return 0;
}
