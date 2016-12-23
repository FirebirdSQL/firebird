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

#include "CalcCApi.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#define DLL_EXPORT __declspec(dllexport)
#else
#include <dlfcn.h>
#define DLL_EXPORT
#endif


//--------------------------------------

// CALC_IStatusImpl


struct CALC_IStatusImpl
{
	void* cloopDummy;
	struct CALC_IStatusVTable* vtable;
	int code;
};

static void CALC_IStatusImpl_dispose(struct CALC_IStatus* self)
{
	free(self);
}

static int CALC_IStatusImpl_getCode(const struct CALC_IStatus* self)
{
	return ((struct CALC_IStatusImpl*) self)->code;
}

static void CALC_IStatusImpl_setCode(struct CALC_IStatus* self, int code)
{
	((struct CALC_IStatusImpl*) self)->code = code;
}

struct CALC_IStatus* CALC_IStatusImpl_create()
{
	static struct CALC_IStatusVTable vtable = {
		{NULL},
		CALC_IStatus_VERSION,
		CALC_IStatusImpl_dispose,
		CALC_IStatusImpl_getCode,
		CALC_IStatusImpl_setCode
	};

	struct CALC_IStatusImpl* impl = malloc(sizeof(struct CALC_IStatusImpl));
	impl->vtable = &vtable;
	impl->code = 0;

	return (struct CALC_IStatus*) impl;
}


//--------------------------------------

// CALC_ICalculatorImpl


struct CALC_ICalculatorImpl
{
	void* cloopDummy;
	struct CALC_ICalculatorVTable* vtable;
	int memory;
};

static void CALC_ICalculatorImpl_dispose(struct CALC_ICalculator* self)
{
	free(self);
}

static int CALC_ICalculatorImpl_sum(const struct CALC_ICalculator* self,
	struct CALC_IStatus* status, int n1, int n2)
{
	if (n1 + n2 > 1000)
	{
		CALC_IStatus_setCode(status, CALC_IStatus_ERROR_1);
		return 0;
	}
	else
		return n1 + n2;
}

static int CALC_ICalculatorImpl_getMemory(const struct CALC_ICalculator* self)
{
	return ((struct CALC_ICalculatorImpl*) self)->memory;
}

static void CALC_ICalculatorImpl_setMemory(struct CALC_ICalculator* self, int n)
{
	((struct CALC_ICalculatorImpl*) self)->memory = n;
}

static void CALC_ICalculatorImpl_sumAndStore(struct CALC_ICalculator* self,
	struct CALC_IStatus* status, int n1, int n2)
{
	CALC_ICalculator_setMemory(self, CALC_ICalculator_sum(self, status, n1, n2));
}

struct CALC_ICalculator* CALC_ICalculatorImpl_create()
{
	static struct CALC_ICalculatorVTable vtable = {
		{NULL},
		CALC_ICalculator_VERSION,
		CALC_ICalculatorImpl_dispose,
		CALC_ICalculatorImpl_sum,
		CALC_ICalculatorImpl_getMemory,
		CALC_ICalculatorImpl_setMemory,
		CALC_ICalculatorImpl_sumAndStore
	};

	struct CALC_ICalculatorImpl* impl = malloc(sizeof(struct CALC_ICalculatorImpl));
	impl->vtable = &vtable;
	impl->memory = 0;

	return (struct CALC_ICalculator*) impl;
}


//--------------------------------------

// CALC_ICalculator2Impl


struct CALC_ICalculator2Impl
{
	void* cloopDummy;
	struct CALC_ICalculator2VTable* vtable;
	int memory;
};

static void CALC_ICalculator2Impl_dispose(struct CALC_ICalculator2* self)
{
	free(self);
}

static int CALC_ICalculator2Impl_sum(const struct CALC_ICalculator2* self,
	struct CALC_IStatus* status, int n1, int n2)
{
	if (n1 + n2 > 1000)
	{
		CALC_IStatus_setCode(status, CALC_IStatus_ERROR_1);
		return 0;
	}
	else
		return n1 + n2;
}

static int CALC_ICalculator2Impl_getMemory(const struct CALC_ICalculator2* self)
{
	return ((struct CALC_ICalculator2Impl*) self)->memory;
}

static void CALC_ICalculator2Impl_setMemory(struct CALC_ICalculator2* self, int n)
{
	((struct CALC_ICalculator2Impl*) self)->memory = n;
}

static void CALC_ICalculator2Impl_sumAndStore(struct CALC_ICalculator2* self,
	struct CALC_IStatus* status, int n1, int n2)
{
	CALC_ICalculator2_setMemory(self, CALC_ICalculator2_sum(self, status, n1, n2));
}

static int CALC_ICalculator2Impl_multiply(const struct CALC_ICalculator2* self,
	struct CALC_IStatus* status, int n1, int n2)
{
	return n1 * n2;
}

static void CALC_ICalculator2Impl_copyMemory(struct CALC_ICalculator2* self,
	const struct CALC_ICalculator* calculator)
{
	CALC_ICalculator2_setMemory(self, CALC_ICalculator_getMemory(calculator));
}

static void CALC_ICalculator2Impl_copyMemory2(struct CALC_ICalculator2* self, const int* address)
{
	CALC_ICalculator2_setMemory(self, *address);
}

struct CALC_ICalculator2* CALC_ICalculator2Impl_create()
{
	static struct CALC_ICalculator2VTable vtable = {
		{NULL},
		CALC_ICalculator2_VERSION,
		CALC_ICalculator2Impl_dispose,
		CALC_ICalculator2Impl_sum,
		CALC_ICalculator2Impl_getMemory,
		CALC_ICalculator2Impl_setMemory,
		CALC_ICalculator2Impl_sumAndStore,
		CALC_ICalculator2Impl_multiply,
		CALC_ICalculator2Impl_copyMemory,
		CALC_ICalculator2Impl_copyMemory2
	};

	struct CALC_ICalculator2Impl* impl = malloc(sizeof(struct CALC_ICalculator2Impl));
	impl->vtable = &vtable;
	impl->memory = 0;

	return (struct CALC_ICalculator2*) impl;
}


//--------------------------------------

// CALC_IBrokenCalculatorImpl


static int CALC_IBrokenCalculatorImpl_sum(const struct CALC_ICalculator* self,
	struct CALC_IStatus* status, int n1, int n2)
{
	return CALC_ICalculatorImpl_sum(self, status, n1, n2) + 1;
}

struct CALC_ICalculator* CALC_IBrokenCalculatorImpl_create()
{
	static struct CALC_ICalculatorVTable vtable = {
		{NULL},
		CALC_ICalculator_VERSION,
		CALC_ICalculatorImpl_dispose,
		CALC_IBrokenCalculatorImpl_sum,
		CALC_ICalculatorImpl_getMemory,
		CALC_ICalculatorImpl_setMemory,
		CALC_ICalculatorImpl_sumAndStore
	};

	struct CALC_ICalculatorImpl* impl = malloc(sizeof(struct CALC_ICalculatorImpl));
	impl->vtable = &vtable;
	impl->memory = 0;

	return (struct CALC_ICalculator*) impl;
}


//--------------------------------------

// CALC_IFactoryImpl


static void CALC_IFactoryImpl_dispose(struct CALC_IFactory* self)
{
	free(self);
}

static struct CALC_IStatus* CALC_IFactoryImpl_createStatus(struct CALC_IFactory* self)
{
	return CALC_IStatusImpl_create();
}

static struct CALC_ICalculator* CALC_IFactoryImpl_createCalculator(struct CALC_IFactory* self,
	struct CALC_IStatus* status)
{
	return CALC_ICalculatorImpl_create();
}

static struct CALC_ICalculator2* CALC_IFactoryImpl_createCalculator2(struct CALC_IFactory* self,
	struct CALC_IStatus* status)
{
	return CALC_ICalculator2Impl_create();
}

static struct CALC_ICalculator* CALC_IFactoryImpl_createBrokenCalculator(struct CALC_IFactory* self,
	struct CALC_IStatus* status)
{
	return CALC_IBrokenCalculatorImpl_create();
}

struct CALC_IFactory* CALC_IFactoryImpl_create()
{
	static struct CALC_IFactoryVTable vtable = {
		{NULL},
		CALC_IFactory_VERSION,
		CALC_IFactoryImpl_dispose,
		CALC_IFactoryImpl_createStatus,
		CALC_IFactoryImpl_createCalculator,
		CALC_IFactoryImpl_createCalculator2,
		CALC_IFactoryImpl_createBrokenCalculator
	};

	struct CALC_IFactory* impl = malloc(sizeof(struct CALC_IFactory));
	impl->vtable = &vtable;

	return impl;
}


//--------------------------------------

// Library entry point


DLL_EXPORT struct CALC_IFactory* createFactory()
{
	return CALC_IFactoryImpl_create();
}


//--------------------------------------


static void test(struct CALC_IFactory* (*createFactory)())
{
	struct CALC_IFactory* factory = createFactory();
	struct CALC_IStatus* status = (struct CALC_IStatus*) CALC_IStatusImpl_create();
	struct CALC_ICalculator* calculator;
	struct CALC_ICalculator2* calculator2;
	int sum, code, address;

	calculator = CALC_IFactory_createCalculator(factory, status);

	address = 40;

	CALC_ICalculator_sumAndStore(calculator, status, 1, 22);
	printf("%d\n", CALC_ICalculator_getMemory(calculator));	// 23

	CALC_ICalculator_setMemory(calculator, CALC_ICalculator_sum(calculator, status, 2, 33));
	printf("%d\n", CALC_ICalculator_getMemory(calculator));	// 35

	calculator2 = CALC_IFactory_createCalculator2(factory, status);

	CALC_ICalculator2_copyMemory(calculator2, calculator);
	printf("%d\n", CALC_ICalculator2_getMemory(calculator2));	// 35

	CALC_ICalculator2_copyMemory2(calculator2, &address);
	printf("%d\n", CALC_ICalculator2_getMemory(calculator2));	// 40

	CALC_ICalculator_dispose(calculator);
	calculator = (struct CALC_ICalculator*) calculator2;

	CALC_ICalculator_sumAndStore(calculator, status, 1, 22);
	printf("%d\n", CALC_ICalculator_getMemory(calculator));	// 23

	CALC_ICalculator_setMemory(calculator, CALC_ICalculator_sum(calculator, status, 2, 33));
	printf("%d\n", CALC_ICalculator_getMemory(calculator));	// 35

	CALC_ICalculator2_sumAndStore(calculator2, status, 1, 22);
	printf("%d\n", CALC_ICalculator2_getMemory(calculator2));	// 23

	CALC_ICalculator2_setMemory(calculator2, CALC_ICalculator2_multiply(calculator2, status, 2, 33));
	printf("%d\n", CALC_ICalculator2_getMemory(calculator2));	// 66

	CALC_ICalculator_dispose(calculator);

	calculator = CALC_IFactory_createBrokenCalculator(factory, status);

	CALC_ICalculator_sumAndStore(calculator, status, 1, 22);
	printf("%d\n", CALC_ICalculator_getMemory(calculator));	// 24

	CALC_ICalculator_setMemory(calculator, CALC_ICalculator_sum(calculator, status, 2, 33));
	printf("%d\n", CALC_ICalculator_getMemory(calculator));	// 36

	sum = CALC_ICalculator_sum(calculator, status, 600, 600);
	code = CALC_IStatus_getCode(status);
	if (code != 0)
		printf("exception %d\n", code);	// exception 1
	else
		printf("%d\n", sum);

	CALC_ICalculator_dispose(calculator);

	CALC_IStatus_dispose(status);
	CALC_IFactory_dispose(factory);

	printf("\n");
}

int main(int argc, char* argv[])
{
#ifdef WIN32
	HMODULE library = LoadLibrary(argv[1]);
#else
	void* library = dlopen(argv[1], RTLD_LAZY);
#endif

	struct CALC_IFactory* (*createFactory)();

#ifdef WIN32
	createFactory = (struct Factory* (*)()) GetProcAddress(library, "createFactory");
#else
	createFactory = dlsym(library, "createFactory");
#endif

	test(createFactory);

#ifdef WIN32
	FreeLibrary(library);
#else
	dlclose(library);
#endif

	return 0;
}
