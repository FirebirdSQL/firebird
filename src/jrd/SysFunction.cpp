/*
 *	PROGRAM:	JRD System Functions
 *	MODULE:		SysFunctions.h
 *	DESCRIPTION:	System Functions
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project, based on work done
 *  in Yaffil by Oleg Loa <loa@mail.ru> and Alexey Karyakin <aleksey.karyakin@mail.ru>
 *
 *  Copyright (c) 2007 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *    Oleg Loa <loa@mail.ru>
 *    Alexey Karyakin <aleksey.karyakin@mail.ru>
 *	  Alexander Peshkov <peshkoff@mail.ru>
 *
 */

#include "firebird.h"
#include "../common/TimeZoneUtil.h"
#include "../common/classes/VaryStr.h"
#include "../common/classes/Hash.h"
#include "../jrd/SysFunction.h"
#include "../jrd/DataTypeUtil.h"
#include "../include/fb_blk.h"
#include "../jrd/exe.h"
#include "../jrd/intl.h"
#include "../jrd/req.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cvt_proto.h"
#include "../common/cvt.h"
#include "../jrd/evl_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/tpc_proto.h"
#include "../jrd/scl_proto.h"
#include "../common/os/guid.h"
#include "../jrd/license.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/trace/TraceObjects.h"
#include "../jrd/Collation.h"
#include "../common/classes/FpeControl.h"
#include "../jrd/extds/ExtDS.h"

#include <math.h>

#ifndef WIN_NT
#define LTC_PTHREAD
#endif
#define USE_LTM
#define LTM_DESC
#include <tomcrypt.h>

using namespace Firebird;
using namespace Jrd;

namespace {

// function types handled in generic functions
enum Function
{
	funNone, // do not use
	funBinAnd,
	funBinOr,
	funBinShl,
	funBinShr,
	funBinShlRot,
	funBinShrRot,
	funBinXor,
	funBinNot,
	funFirstDay,
	funLastDay,
	funMaxValue,
	funMinValue,
	funLPad,
	funRPad,
	funLnat,
	funLog10,
	funTotalOrd,
	funCmpDec
};

enum TrigonFunction
{
	trfNone, // do not use
	trfSin,
	trfCos,
	trfTan,
	trfCot,
	trfAsin,
	trfAcos,
	trfAtan,
	trfSinh,
	trfCosh,
	trfTanh,
	trfAsinh,
	trfAcosh,
	trfAtanh
};


struct HashAlgorithmDescriptor
{
	const char* name;
	USHORT length;
	HashContext* (*create)(MemoryPool&);

	static const HashAlgorithmDescriptor* find(const char* name);
};

template <typename T>
struct HashAlgorithmDescriptorFactory
{
	static HashAlgorithmDescriptor* getInstance(const char* name, USHORT length)
	{
		desc.name = name;
		desc.length = length;
		desc.create = createContext;
		return &desc;
	}

	static HashContext* createContext(MemoryPool& pool)
	{
		return FB_NEW_POOL(pool) T(pool);
	}

	static HashAlgorithmDescriptor desc;
};

template <typename T> HashAlgorithmDescriptor HashAlgorithmDescriptorFactory<T>::desc;

static const HashAlgorithmDescriptor* hashAlgorithmDescriptors[] = {
	HashAlgorithmDescriptorFactory<Md5HashContext>::getInstance("MD5", 16),
	HashAlgorithmDescriptorFactory<Sha1HashContext>::getInstance("SHA1", 20),
	HashAlgorithmDescriptorFactory<Sha256HashContext>::getInstance("SHA256", 32),
	HashAlgorithmDescriptorFactory<Sha512HashContext>::getInstance("SHA512", 64)
};

const HashAlgorithmDescriptor* HashAlgorithmDescriptor::find(const char* name)
{
	unsigned count = FB_NELEM(hashAlgorithmDescriptors);

	for (unsigned i = 0; i < count; ++i)
	{
		if (strcmp(name, hashAlgorithmDescriptors[i]->name) == 0)
			return hashAlgorithmDescriptors[i];
	}

	status_exception::raise(Arg::Gds(isc_sysf_invalid_hash_algorithm) << name);
	return nullptr;
}


// constants
const int oneDay = 86400;

// auxiliary functions
void add10msec(ISC_TIMESTAMP* v, SINT64 msec, SINT64 multiplier);
double fbcot(double value) throw();

// generic setParams functions
void setParamsDouble(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsDblDec(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsDecFloat(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsFromList(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsInteger(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsInt64(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsSecondInteger(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);

// specific setParams functions
void setParamsAsciiVal(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsCharToUuid(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsDateAdd(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsDateDiff(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsEncrypt(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args);
void setParamsRsaEncrypt(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args);
void setParamsRsaPublic(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args);
void setParamsRsaSign(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args);
void setParamsRsaVerify(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args);
void setParamsFirstLastDay(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsGetSetContext(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsOverlay(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsPosition(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsRoundTrunc(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);
void setParamsUuidToChar(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, int argsCount, dsc** args);

// generic make functions
void makeDoubleResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeDblDecResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeDecFloatResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeFromListResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeInt64Result(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeLongResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
///void makeLongStringOrBlobResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeShortResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeBoolResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);

// specific make functions
void makeAbs(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeAsciiChar(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeBin(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeBinShift(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeCeilFloor(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeDateAdd(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeDecode64(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeEncode64(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeDecrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeEncrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeFirstLastDayResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeGetSetContext(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeGetTranCN(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeHash(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeLeftRight(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeMod(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeOverlay(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makePad(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makePi(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeReplace(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeReverse(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeRound(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeRsaDecrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeRsaEncrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeRsaPrivate(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeRsaPublic(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeRsaSign(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeTrunc(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeUuid(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);
void makeUuidToChar(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args);

// generic stdmath function
dsc* evlStdMath(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);

// specific evl functions
dsc* evlAbs(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlAsciiChar(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlAsciiVal(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlAtan2(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlBin(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlBinShift(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlCeil(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlCharToUuid(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlCrc32(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlDateAdd(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlDateDiff(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlDecode64(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlEncode64(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlDecrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlEncrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRsaDecrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRsaEncrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRsaPrivate(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRsaPublic(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRsaSign(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRsaVerify(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlExp(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlFirstLastDay(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlFloor(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlGenUuid(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlGetContext(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlSetContext(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlGetTranCN(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlHash(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlLeft(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlLnLog10(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlLog(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlMaxMinValue(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlMod(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlOverlay(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlPad(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlPi(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlPosition(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlPower(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRand(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlReplace(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlReverse(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRight(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRoleInUse(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlRound(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlSign(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlSqrt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlSystemPrivilege(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlTrunc(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);
dsc* evlUuidToChar(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure);


// System context function names
const char
	RDB_GET_CONTEXT[] = "RDB$GET_CONTEXT",
	RDB_SET_CONTEXT[] = "RDB$SET_CONTEXT";

// Context namespace names
const char
	SYSTEM_NAMESPACE[] = "SYSTEM",
	DDL_TRIGGER_NAMESPACE[] = "DDL_TRIGGER",
	USER_SESSION_NAMESPACE[] = "USER_SESSION",
	USER_TRANSACTION_NAMESPACE[] = "USER_TRANSACTION";

// System context variables names
const char
	// SYSTEM namespace: global and database wise items
	ENGINE_VERSION[] = "ENGINE_VERSION",
	DATABASE_NAME[] = "DB_NAME",
	GLOBAL_CN_NAME[] = "GLOBAL_CN",
	EXT_CONN_POOL_SIZE[] = "EXT_CONN_POOL_SIZE",
	EXT_CONN_POOL_IDLE[] = "EXT_CONN_POOL_IDLE_COUNT",
	EXT_CONN_POOL_ACTIVE[] = "EXT_CONN_POOL_ACTIVE_COUNT",
	EXT_CONN_POOL_LIFETIME[] = "EXT_CONN_POOL_LIFETIME",
	REPLICATION_SEQ_NAME[] = "REPLICATION_SEQUENCE",
	// SYSTEM namespace: connection wise items
	SESSION_ID_NAME[] = "SESSION_ID",
	NETWORK_PROTOCOL_NAME[] = "NETWORK_PROTOCOL",
	WIRE_COMPRESSED_NAME[] = "WIRE_COMPRESSED",
	WIRE_ENCRYPTED_NAME[] = "WIRE_ENCRYPTED",
	CLIENT_ADDRESS_NAME[] = "CLIENT_ADDRESS",
	CLIENT_HOST_NAME[] = "CLIENT_HOST",
	CLIENT_PID_NAME[] = "CLIENT_PID",
	CLIENT_PROCESS_NAME[] = "CLIENT_PROCESS",
	CURRENT_USER_NAME[] = "CURRENT_USER",
	CURRENT_ROLE_NAME[] = "CURRENT_ROLE",
	SESSION_IDLE_TIMEOUT[] = "SESSION_IDLE_TIMEOUT",
	STATEMENT_TIMEOUT[] = "STATEMENT_TIMEOUT",
	EFFECTIVE_USER_NAME[] = "EFFECTIVE_USER",
	// SYSTEM namespace: transaction wise items
	TRANSACTION_ID_NAME[] = "TRANSACTION_ID",
	ISOLATION_LEVEL_NAME[] = "ISOLATION_LEVEL",
	LOCK_TIMEOUT_NAME[] = "LOCK_TIMEOUT",
	READ_ONLY_NAME[] = "READ_ONLY",
	SNAPSHOT_NUMBER_NAME[] = "SNAPSHOT_NUMBER",
	// DDL_TRIGGER namespace
	DDL_EVENT_NAME[] = "DDL_EVENT",
	EVENT_TYPE_NAME[] = "EVENT_TYPE",
	OBJECT_NAME[] = "OBJECT_NAME",
	OLD_OBJECT_NAME[] = "OLD_OBJECT_NAME",
	NEW_OBJECT_NAME[] = "NEW_OBJECT_NAME",
	OBJECT_TYPE_NAME[] = "OBJECT_TYPE",
	SQL_TEXT_NAME[] = "SQL_TEXT";

// Isolation values modes
const char
	READ_COMMITTED_VALUE[] = "READ COMMITTED",
	CONSISTENCY_VALUE[] = "CONSISTENCY",
	SNAPSHOT_VALUE[] = "SNAPSHOT";

// Boolean values
static const char
	FALSE_VALUE[] = "FALSE",
	TRUE_VALUE[] = "TRUE";


void add10msec(ISC_TIMESTAMP* v, SINT64 msec, SINT64 multiplier)
{
	const SINT64 full = msec * multiplier;
	const int days = full / (oneDay * ISC_TIME_SECONDS_PRECISION);
	const int secs = full % (oneDay * ISC_TIME_SECONDS_PRECISION);

	v->timestamp_date += days;

	// Time portion is unsigned, so we avoid unsigned rolling over negative values
	// that only produce a new unsigned number with the wrong result.
	if (secs < 0 && ISC_TIME(-secs) > v->timestamp_time)
	{
		v->timestamp_date--;
		v->timestamp_time += (oneDay * ISC_TIME_SECONDS_PRECISION) + secs;
	}
	else if ((v->timestamp_time += secs) >= (oneDay * ISC_TIME_SECONDS_PRECISION))
	{
		v->timestamp_date++;
		v->timestamp_time -= (oneDay * ISC_TIME_SECONDS_PRECISION);
	}
}


double fbcot(double value) throw()
{
	return 1.0 / tan(value);
}


void tomCheck(int err, const Arg::StatusVector& secondary)
{
	if (err == CRYPT_OK)
		return;

	status_exception::raise(Arg::Gds(isc_tom_error) << error_to_string(err) << secondary);
}


bool initResult(dsc* result, int argsCount, const dsc** args, bool* isNullable)
{
	*isNullable = false;

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isNull())
		{
			result->setNull();
			return true;
		}

		if (args[i]->isNullable())
			*isNullable = true;
	}

	return false;
}


void setParamsDouble(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isUnknown())
			args[i]->makeDouble();
	}
}


template <typename DSC>
bool areParamsDouble(int argsCount, DSC** args)
{
	bool decSeen = false;

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isApprox())
			return true;
		if (args[i]->isDecFloat())
			decSeen = true;
	}

	return !decSeen;
}


bool areParamsDec64(int argsCount, dsc** args)
{
	bool f64 = false;

	for (int i = 0; i < argsCount; ++i)
	{
		switch (args[i]->dsc_dtype)
		{
		case dtype_dec64:
			f64 = true;
			break;
		case dtype_dec128:
			return false;
		}
	}

	return f64;
}


void setParamsDblDec(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	bool fDbl = areParamsDouble(argsCount, args);

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isUnknown())
		{
			if (fDbl)
				args[i]->makeDouble();
			else
				args[i]->makeDecimal128();
		}
	}
}


void setParamsDecFloat(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	bool f64 = areParamsDec64(argsCount, args);

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isUnknown())
		{
			if (f64)
				args[i]->makeDecimal64();
			else
				args[i]->makeDecimal128();
		}
	}
}


void setParamsFromList(DataTypeUtilBase* dataTypeUtil, const SysFunction* function,
	int argsCount, dsc** args)
{
	dsc desc;
	dataTypeUtil->makeFromList(&desc, function->name.c_str(), argsCount, const_cast<const dsc**>(args));

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isUnknown())
			*args[i] = desc;
	}
}


void setParamsInteger(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isUnknown())
			args[i]->makeLong(0);
	}
}


void setParamsInt64(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isUnknown())
			args[i]->makeInt64(0);
	}
}


void setParamsSecondInteger(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 2)
	{
		if (args[1]->isUnknown())
			args[1]->makeLong(0);
	}
}


void setParamsAsciiVal(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 1 && args[0]->isUnknown())
		args[0]->makeText(1, CS_ASCII);
}


void setParamsCharToUuid(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 1 && args[0]->isUnknown())
		args[0]->makeText(GUID_BODY_SIZE, ttype_ascii);
}


void setParamsDateAdd(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 1 && args[0]->isUnknown())
	{
		if (args[1]->dsc_address &&	// constant
			CVT_get_long(args[1], 0, JRD_get_thread_data()->getAttachment()->att_dec_status, ERR_post) == blr_extract_millisecond)
		{
			args[0]->makeInt64(ISC_TIME_SECONDS_PRECISION_SCALE + 3);
		}
		else
			args[0]->makeInt64(0);
	}

	if (argsCount >= 3 && args[2]->isUnknown())
		args[2]->makeTimestamp();
}


void setParamsDateDiff(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 3)
	{
		if (args[1]->isUnknown() && args[2]->isUnknown())
		{
			args[1]->makeTimestamp();
			args[2]->makeTimestamp();
		}
		else if (args[1]->isUnknown())
			*args[1] = *args[2];
		else if (args[2]->isUnknown())
			*args[2] = *args[1];
	}
}


const unsigned CRYPT_ARG_VALUE = 0;
const unsigned CRYPT_ARG_ALGORITHM = 1;
const unsigned CRYPT_ARG_MODE = 2;
const unsigned CRYPT_ARG_KEY = 3;
const unsigned CRYPT_ARG_IV = 4;
const unsigned CRYPT_ARG_CTRTYPE = 5;
const unsigned CRYPT_ARG_COUNTER = 6;
const unsigned CRYPT_ARG_MAX = 7;

void setParamsEncrypt(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	fb_assert(argsCount == CRYPT_ARG_MAX);

	if (!args[CRYPT_ARG_VALUE]->isBlob())
		args[CRYPT_ARG_VALUE]->makeVarying(args[CRYPT_ARG_VALUE]->getStringLength(), ttype_binary);

	fb_assert(args[CRYPT_ARG_ALGORITHM]->dsc_address && args[CRYPT_ARG_ALGORITHM]->isText());

	args[CRYPT_ARG_KEY]->makeVarying(args[CRYPT_ARG_KEY]->getStringLength(), ttype_binary);

	if (args[CRYPT_ARG_IV]->dsc_length)
		args[CRYPT_ARG_IV]->makeVarying(args[CRYPT_ARG_IV]->getStringLength(), ttype_binary);

	if (args[CRYPT_ARG_CTRTYPE]->dsc_length)
		args[CRYPT_ARG_CTRTYPE]->makeVarying(args[CRYPT_ARG_CTRTYPE]->getStringLength(), ttype_ascii);

	if (args[CRYPT_ARG_COUNTER]->dsc_length)
		args[CRYPT_ARG_COUNTER]->makeInt64(0);
}


const unsigned RSA_CRYPT_ARG_VALUE = 0;
const unsigned RSA_CRYPT_ARG_KEY = 1;
const unsigned RSA_CRYPT_ARG_LPARAM = 2;
const unsigned RSA_CRYPT_ARG_HASH = 3;
const unsigned RSA_CRYPT_ARG_MAX = 4;

void setParamsRsaEncrypt(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	fb_assert(argsCount == RSA_CRYPT_ARG_MAX);

	args[RSA_CRYPT_ARG_VALUE]->makeVarying(args[RSA_CRYPT_ARG_VALUE]->getStringLength(), ttype_binary);
	args[RSA_CRYPT_ARG_KEY]->makeVarying(args[RSA_CRYPT_ARG_KEY]->getStringLength(), ttype_binary);

	if (args[RSA_CRYPT_ARG_LPARAM]->dsc_length)
		args[RSA_CRYPT_ARG_LPARAM]->makeVarying(args[RSA_CRYPT_ARG_LPARAM]->getStringLength(), ttype_binary);

	if (args[RSA_CRYPT_ARG_HASH]->dsc_length)
		args[RSA_CRYPT_ARG_HASH]->makeVarying(args[RSA_CRYPT_ARG_HASH]->getStringLength(), ttype_binary);
}


const unsigned RSA_SIGN_ARG_VALUE = 0;
const unsigned RSA_SIGN_ARG_KEY = 1;
const unsigned RSA_SIGN_ARG_HASH = 2;
const unsigned RSA_SIGN_ARG_SALTLEN = 3;
const unsigned RSA_SIGN_ARG_MAX = 4;

void setParamsRsaSign(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	fb_assert(argsCount == RSA_SIGN_ARG_MAX);

	args[RSA_SIGN_ARG_VALUE]->makeVarying(args[RSA_SIGN_ARG_VALUE]->getStringLength(), ttype_binary);
	args[RSA_SIGN_ARG_KEY]->makeVarying(args[RSA_SIGN_ARG_KEY]->getStringLength(), ttype_binary);

	if (args[RSA_SIGN_ARG_HASH]->dsc_length)
		args[RSA_SIGN_ARG_HASH]->makeVarying(args[RSA_SIGN_ARG_HASH]->getStringLength(), ttype_binary);

	if (args[RSA_SIGN_ARG_SALTLEN]->dsc_length)
		args[RSA_SIGN_ARG_SALTLEN]->makeShort(0);
}


const unsigned RSA_VERIFY_ARG_VALUE = 0;
const unsigned RSA_VERIFY_ARG_SIGNATURE = 1;
const unsigned RSA_VERIFY_ARG_KEY = 2;
const unsigned RSA_VERIFY_ARG_HASH = 3;
const unsigned RSA_VERIFY_ARG_SALTLEN = 4;
const unsigned RSA_VERIFY_ARG_MAX = 5;

void setParamsRsaVerify(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	fb_assert(argsCount == RSA_VERIFY_ARG_MAX);

	args[RSA_VERIFY_ARG_VALUE]->makeVarying(args[RSA_VERIFY_ARG_VALUE]->getStringLength(), ttype_binary);
	args[RSA_VERIFY_ARG_KEY]->makeVarying(args[RSA_VERIFY_ARG_KEY]->getStringLength(), ttype_binary);
	args[RSA_VERIFY_ARG_SIGNATURE]->makeVarying(args[RSA_VERIFY_ARG_SIGNATURE]->getStringLength(), ttype_binary);

	if (args[RSA_VERIFY_ARG_HASH]->dsc_length)
		args[RSA_VERIFY_ARG_HASH]->makeVarying(args[RSA_VERIFY_ARG_HASH]->getStringLength(), ttype_binary);

	if (args[RSA_VERIFY_ARG_SALTLEN]->dsc_length)
		args[RSA_VERIFY_ARG_SALTLEN]->makeShort(0);
}


void setParamsRsaPublic(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	fb_assert(argsCount == 1);

	args[0]->makeVarying(args[0]->getStringLength(), ttype_binary);
}


void setParamsFirstLastDay(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 2)
	{
		if (args[1]->isUnknown())
			args[1]->makeTimestamp();
	}
}


void setParamsGetSetContext(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 1 && args[0]->isUnknown())
	{
		args[0]->makeVarying(80, ttype_none);
		args[0]->setNullable(true);
	}

	if (argsCount >= 2 && args[1]->isUnknown())
	{
		args[1]->makeVarying(80, ttype_none);
		args[1]->setNullable(true);
	}

	if (argsCount >= 3 && args[2]->isUnknown())
	{
		args[2]->makeVarying(255, ttype_none);
		args[2]->setNullable(true);
	}
}


void setParamsOverlay(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 3)
	{
		if (!(args[0]->isUnknown() && args[1]->isUnknown()))
		{
			if (args[1]->isUnknown())
				*args[1] = *args[0];
			else if (args[0]->isUnknown())
				*args[0] = *args[1];
		}

		if (argsCount >= 4)
		{
			if (args[2]->isUnknown() && args[3]->isUnknown())
			{
				args[2]->makeLong(0);
				args[3]->makeLong(0);
			}
			else if (args[2]->isUnknown())
				*args[2] = *args[3];
			else if (args[3]->isUnknown())
				*args[3] = *args[2];
		}

		if (args[2]->isUnknown())
			args[2]->makeLong(0);
	}
}


void setParamsPosition(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 2)
	{
		if (args[0]->isUnknown())
			*args[0] = *args[1];

		if (args[1]->isUnknown())
			*args[1] = *args[0];
	}
}


void setParamsRoundTrunc(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 1)
	{
		if (args[0]->isUnknown())
			args[0]->makeDouble();

		if (argsCount >= 2)
		{
			if (args[1]->isUnknown())
				args[1]->makeLong(0);
		}
	}
}


void setParamsUuidToChar(DataTypeUtilBase*, const SysFunction*, int argsCount, dsc** args)
{
	if (argsCount >= 1 && args[0]->isUnknown())
		args[0]->makeText(16, ttype_binary);
}


void makeDoubleResult(DataTypeUtilBase*, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	result->makeDouble();

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}


void makeDblDecResult(DataTypeUtilBase*, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	if (argsCount == 0 || areParamsDouble(argsCount, args))
		result->makeDouble();
	else
		result->makeDecimal128();

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}


void makeDecFloatResult(DataTypeUtilBase*, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	if (argsCount == 0 || args[0]->dsc_dtype == dtype_dec128)
		result->makeDecimal128();
	else
		result->makeDecimal64();

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}


void makePi(DataTypeUtilBase*, const SysFunction*, dsc* result, int, const dsc**)
{
	result->makeDouble();
	result->clearNull();
	result->setNullable(false);
}


void makeFromListResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	result->clear();
	dataTypeUtil->makeFromList(result, function->name.c_str(), argsCount, args);
}


void makeInt64Result(DataTypeUtilBase* dataTypeUtil, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	if (dataTypeUtil->getDialect() == 1)
		result->makeDouble();
	else
		result->makeInt64(0);

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}


void makeLongResult(DataTypeUtilBase*, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	result->makeLong(0);

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}

void makeBooleanResult(DataTypeUtilBase*, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	result->makeBoolean(0);

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}

/***
 * This function doesn't work yet, because makeFromListResult isn't totally prepared for blobs vs strings.
 *
void makeLongStringOrBlobResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function,
	dsc* result, int argsCount, const dsc** args)
{
	makeFromListResult(dataTypeUtil, function, result, argsCount, args);

	if (result->isText())
		result->makeVarying(dataTypeUtil->fixLength(result, MAX_STR_SIZE), result->getTextType());
}
***/


void makeShortResult(DataTypeUtilBase*, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	result->makeShort(0);

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}


void makeBoolResult(DataTypeUtilBase* dataTypeUtil, const SysFunction* function,
	dsc* result, int argsCount, const dsc** args)
{
	result->makeBoolean();
}


void makeVarBinary(dsc* result, int argsCount, const dsc** args, unsigned length)
{
	result->makeVarying(length, ttype_binary);

	bool isNullable;
	if (initResult(result, argsCount > 2 ? 2 : argsCount, args, &isNullable))
		return;

	result->setNullable(isNullable);
}


void makeRsaPrivate(DataTypeUtilBase* dataTypeUtil, const SysFunction* function,
	dsc* result, int argsCount, const dsc** args)
{
	makeVarBinary(result, argsCount, args, 16 * 1024);
}


void makeRsaPublic(DataTypeUtilBase* dataTypeUtil, const SysFunction* function,
	dsc* result, int argsCount, const dsc** args)
{
	makeVarBinary(result, argsCount, args, 8 * 1024);
}


void makeRsaSign(DataTypeUtilBase* dataTypeUtil, const SysFunction* function,
	dsc* result, int argsCount, const dsc** args)
{
	makeVarBinary(result, argsCount, args, 256);
}


void makeAbs(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	const dsc* value = args[0];

	if (value->isNull())
	{
		result->makeLong(0);
		result->setNull();
		return;
	}

	switch (value->dsc_dtype)
	{
		case dtype_short:
			result->makeLong(value->dsc_scale);
			break;

		case dtype_long:
			if (dataTypeUtil->getDialect() == 1)
				result->makeDouble();
			else
				result->makeInt64(value->dsc_scale);
			break;

		case dtype_real:
		case dtype_double:
		case dtype_int64:
		case dtype_dec64:
		case dtype_dec128:
			*result = *value;
			break;

		default:
			result->makeDouble();
			break;
	}

	result->setNullable(value->isNullable());
}


void makeAsciiChar(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	const dsc* value = args[0];

	if (value->isNull())
	{
		result->makeNullString();
		return;
	}

	result->makeText(1, ttype_none);
	result->setNullable(value->isNullable());
}


void makeBin(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	bool isNullable = false;
	bool isNull = false;
	bool first = true;

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isNullable())
			isNullable = true;

		if (args[i]->isNull())
		{
			isNull = true;
			continue;
		}

		if (!args[i]->isExact() || args[i]->dsc_scale != 0)
		{
			status_exception::raise(
				Arg::Gds(isc_expression_eval_err) <<
				Arg::Gds(isc_sysf_argmustbe_exact) << Arg::Str(function->name));
		}

		if (first)
		{
			first = false;

			result->clear();
			result->dsc_dtype = args[i]->dsc_dtype;
			result->dsc_length = args[i]->dsc_length;
		}
		else
		{
			if (args[i]->dsc_dtype == dtype_int64)
				result->makeInt64(0);
			else if (args[i]->dsc_dtype == dtype_long && result->dsc_dtype != dtype_int64)
				result->makeLong(0);
		}
	}

	if (isNull)
	{
		if (first)
			result->makeLong(0);
		result->setNull();
	}

	result->setNullable(isNullable);
}


void makeBinShift(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	result->makeInt64(0);

	bool isNullable = false;

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isNull())
		{
			result->setNull();
			return;
		}

		if (args[i]->isNullable())
			isNullable = true;

		if (!args[i]->isExact() || args[i]->dsc_scale != 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_argmustbe_exact) << Arg::Str(function->name));
		}
	}

	result->setNullable(isNullable);
}


void makeCeilFloor(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	const dsc* value = args[0];

	if (value->isNull())
	{
		result->makeLong(0);
		result->setNull();
		return;
	}

	switch (value->dsc_dtype)
	{
		case dtype_short:
			result->makeLong(0);
			break;

		case dtype_long:
		case dtype_int64:
			result->makeInt64(0);
			break;

		case dtype_dec128:
		case dtype_dec64:
			result->makeDecimal128(0);
			break;

		default:
			result->makeDouble();
			break;
	}

	result->setNullable(value->isNullable());
}


void makeDateAdd(DataTypeUtilBase*, const SysFunction*, dsc* result, int argsCount, const dsc** args)
{
	fb_assert(argsCount >= 3);

	*result = *args[2];

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	*result = *args[2];
	result->setNullable(isNullable);
}


void makeFirstLastDayResult(DataTypeUtilBase*, const SysFunction*, dsc* result,
	int argsCount, const dsc** args)
{
	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	result->makeDate();

	if (argsCount >= 2)
	{
		if (args[1]->dsc_dtype == dtype_timestamp)
			result->makeTimestamp();
		else if (args[1]->dsc_dtype == dtype_timestamp_tz)
			result->makeTimestampTz();
	}

	result->setNullable(isNullable);
}


void makeGetSetContext(DataTypeUtilBase* /*dataTypeUtil*/, const SysFunction* function, dsc* result,
	int argsCount, const dsc** /*args*/)
{
	fb_assert(argsCount == function->minArgCount);

	if (argsCount == 3)	// set_context
		result->makeLong(0);
	else
	{
		result->makeVarying(255, ttype_none);
		result->setNullable(true);
	}
}


void makeGetTranCN(DataTypeUtilBase* /*dataTypeUtil*/, const SysFunction* /*function*/, dsc* result,
	int /*argsCount*/, const dsc** /*args*/)
{
	result->makeInt64(0);
	result->setNullable(true);
}


void makeHash(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	if (argsCount == 1)
		makeInt64Result(dataTypeUtil, function, result, argsCount, args);
	else if (argsCount >= 2)
	{
		if (!args[1]->dsc_address || !args[1]->isText())	// not a constant
			status_exception::raise(Arg::Gds(isc_sysf_invalid_hash_algorithm) << "<not a string constant>");

		MetaName algorithmName;
		MOV_get_metaname(JRD_get_thread_data(), args[1], algorithmName);

		result->makeVarying(HashAlgorithmDescriptor::find(algorithmName.c_str())->length, ttype_binary);
		result->setNullable(args[0]->isNullable());
	}
}


unsigned decodeLen(unsigned len)
{
 	if (len % 4 || !len)
 		status_exception::raise(Arg::Gds(isc_tom_decode64len) << Arg::Num(len));
 	len = len / 4 * 3;
 	return len;
}


void makeDecode64(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args)
{
	 fb_assert(argsCount == 1);
	 if (args[0]->isBlob())
	 	result->makeBlob(isc_blob_untyped, ttype_binary);
	 else if (args[0]->isText())
	 	result->makeVarying(decodeLen(args[0]->getStringLength()), ttype_binary);
	 else
	 	status_exception::raise(Arg::Gds(isc_tom_strblob));

	result->setNullable(args[0]->isNullable());
}


unsigned encodeLen(unsigned len)
{
	len = (len + 2) / 3 * 4;
	return len;
}


void makeEncode64(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result, int argsCount, const dsc** args)
{
	 fb_assert(argsCount == 1);
	 if (args[0]->isBlob())
	 	result->makeBlob(isc_blob_text, ttype_ascii);
	 else if (args[0]->isText())
	 	result->makeVarying(encodeLen(args[0]->dsc_length), ttype_ascii);
	 else
	 	status_exception::raise(Arg::Gds(isc_tom_strblob));

	result->setNullable(args[0]->isNullable());
}


void makeEncrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == CRYPT_ARG_MAX);

	if (args[0]->isBlob())
		result->makeBlob(0, ttype_binary);
	else
		result->makeVarying(args[0]->getStringLength(), ttype_binary);

	result->setNullable(args[0]->isNullable());
}


void makeDecrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == CRYPT_ARG_MAX);

	if (args[0]->isBlob())
		result->makeBlob(0, ttype_none);
	else
		result->makeVarying(args[0]->getStringLength(), ttype_none);

	result->setNullable(args[0]->isNullable());
}


void makeRsaEncrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == RSA_CRYPT_ARG_MAX);

	result->makeVarying(256, ttype_binary);
	result->setNullable(args[0]->isNullable());
}


void makeRsaDecrypt(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == RSA_CRYPT_ARG_MAX);

	result->makeVarying(255, ttype_none);
	result->setNullable(args[0]->isNullable());
}


void makeLeftRight(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	const dsc* value = args[0];
	const dsc* length = args[1];

	if (value->isNull() || length->isNull())
	{
		result->makeNullString();
		return;
	}

	if (value->isBlob())
		result->makeBlob(value->getBlobSubType(), value->getTextType());
	else
	{
		result->clear();
		result->dsc_dtype = dtype_varying;
		result->setTextType(value->getTextType());
		result->setNullable(value->isNullable() || length->isNullable());

		result->dsc_length = dataTypeUtil->fixLength(result,
			dataTypeUtil->convertLength(value, result)) + static_cast<USHORT>(sizeof(USHORT));
	}
}


void makeMod(DataTypeUtilBase*,	 const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	const dsc* value1 = args[0];
	const dsc* value2 = args[1];

	if (value1->isNull() || value2->isNull())
	{
		result->makeLong(0);
		result->setNull();
		return;
	}

	switch (value1->dsc_dtype)
	{
		case dtype_short:
		case dtype_long:
		case dtype_int64:
			*result = *value1;
			result->dsc_scale = 0;
			break;

		default:
			result->makeInt64(0);
			break;
	}

	result->setNullable(value1->isNullable() || value2->isNullable());
}


void makeOverlay(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	result->makeNullString();

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	const dsc* value = args[0];
	const dsc* placing = args[1];

	if (value->isBlob())
		*result = *value;
	else if (placing->isBlob())
		*result = *placing;
	else
	{
		result->clear();
		result->dsc_dtype = dtype_varying;
	}

	result->setBlobSubType(dataTypeUtil->getResultBlobSubType(value, placing));
	result->setTextType(dataTypeUtil->getResultTextType(value, placing));

	if (!value->isBlob() && !placing->isBlob())
	{
		result->dsc_length = static_cast<USHORT>(sizeof(USHORT)) +
			dataTypeUtil->convertLength(value, result) +
			dataTypeUtil->convertLength(placing, result);
	}

	result->setNullable(isNullable);
}


void makePad(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	result->makeNullString();

	bool isNullable;
	if (initResult(result, argsCount, args, &isNullable))
		return;

	const dsc* value1 = args[0];
	const dsc* length = args[1];
	const dsc* value2 = (argsCount >= 3 ? args[2] : NULL);

	if (value1->isBlob())
		*result = *value1;
	else if (value2 && value2->isBlob())
		*result = *value2;
	else
	{
		result->clear();
		result->dsc_dtype = dtype_varying;
	}

	result->setBlobSubType(value1->getBlobSubType());
	result->setTextType(value1->getTextType());

	if (!result->isBlob())
	{
		if (length->dsc_address)	// constant
		{
			result->dsc_length = static_cast<USHORT>(sizeof(USHORT)) + dataTypeUtil->fixLength(result,
				CVT_get_long(length, 0, JRD_get_thread_data()->getAttachment()->att_dec_status, ERR_post) *
					dataTypeUtil->maxBytesPerChar(result->getCharSet()));
		}
		else
		{
			result->dsc_length = static_cast<USHORT>(sizeof(USHORT)) +
				dataTypeUtil->fixLength(result, MAX_STR_SIZE);
		}
	}

	result->setNullable(isNullable);
}


void makeReplace(DataTypeUtilBase* dataTypeUtil, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	bool isNullable = false;
	const dsc* firstBlob = NULL;

	for (int i = 0; i < argsCount; ++i)
	{
		if (args[i]->isNull())
		{
			result->makeNullString();
			return;
		}

		if (args[i]->isNullable())
			isNullable = true;

		if (!firstBlob && args[i]->isBlob())
			firstBlob = args[i];
	}

	const dsc* searched = args[0];
	const dsc* find = args[1];
	const dsc* replacement = args[2];

	if (firstBlob)
		*result = *firstBlob;
	else
	{
		result->clear();
		result->dsc_dtype = dtype_varying;
	}

	result->setBlobSubType(dataTypeUtil->getResultBlobSubType(searched, find));
	result->setBlobSubType(dataTypeUtil->getResultBlobSubType(result, replacement));

	result->setTextType(dataTypeUtil->getResultTextType(searched, find));
	result->setTextType(dataTypeUtil->getResultTextType(result, replacement));

	if (!firstBlob)
	{
		const int searchedLen = dataTypeUtil->convertLength(searched, result);
		const int findLen = dataTypeUtil->convertLength(find, result);
		const int replacementLen = dataTypeUtil->convertLength(replacement, result);

		if (findLen == 0)
			result->dsc_length = dataTypeUtil->fixLength(result, searchedLen) + static_cast<USHORT>(sizeof(USHORT));
		else
		{
			result->dsc_length = dataTypeUtil->fixLength(result,
				MAX(searchedLen, searchedLen + (searchedLen / findLen) * (replacementLen - findLen))) +
				static_cast<USHORT>(sizeof(USHORT));
		}
	}

	result->setNullable(isNullable);
}


void makeReverse(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	const dsc* value = args[0];

	if (value->isNull())
	{
		result->makeNullString();
		return;
	}

	if (value->isBlob())
		*result = *value;
	else
		result->makeVarying(value->getStringLength(), value->getTextType());

	result->setNullable(value->isNullable());
}


void makeRound(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	const dsc* value1 = args[0];

	if (value1->isNull() || (argsCount > 1 && args[1]->isNull()))
	{
		result->makeLong(0);
		result->setNull();
		return;
	}

	if (value1->isExact() || value1->dsc_dtype == dtype_real || value1->dsc_dtype == dtype_double ||
		value1->dsc_dtype == dtype_dec64 || value1->dsc_dtype == dtype_dec128)
	{
		*result = *value1;
		if (argsCount == 1)
			result->dsc_scale = 0;
	}
	else
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_exact_or_fp) << Arg::Str(function->name));

	result->setNullable(value1->isNullable() || (argsCount > 1 && args[1]->isNullable()));
}


void makeTrunc(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount >= function->minArgCount);

	const dsc* value = args[0];

	if (value->isNull() || (argsCount == 2 && args[1]->isNull()))
	{
		result->makeLong(0);
		result->setNull();
		return;
	}

	switch (value->dsc_dtype)
	{
		case dtype_short:
		case dtype_long:
		case dtype_int64:
			*result = *value;
			if (argsCount == 1)
				result->dsc_scale = 0;
			break;

		default:
			result->makeDouble();
			break;
	}

	result->setNullable(value->isNullable() || (argsCount > 1 && args[1]->isNullable()));
}


void makeUuid(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	if (argsCount > 0 && args[0]->isNull())
		result->makeNullString();
	else
		result->makeText(16, ttype_binary);

	if (argsCount > 0 && args[0]->isNullable())
		result->setNullable(true);
}


void makeUuidToChar(DataTypeUtilBase*, const SysFunction* function, dsc* result,
	int argsCount, const dsc** args)
{
	fb_assert(argsCount == function->minArgCount);

	const dsc* value = args[0];

	if (value->isNull())
	{
		result->makeNullString();
		return;
	}

	result->makeText(GUID_BODY_SIZE, ttype_ascii);
	result->setNullable(value->isNullable());
}


dsc* evlStdMath(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);
	fb_assert(function->misc != NULL);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	const double v = MOV_get_double(tdbb, value);
	double rc;

	// CVC: Apparently, gcc has built-in inverse hyperbolic functions, but since
	// VC doesn't, I'm taking the definitions from Wikipedia

	switch ((TrigonFunction)(IPTR) function->misc)
	{
	case trfSin:
		rc = sin(v);
		break;
	case trfCos:
		rc = cos(v);
		break;
	case trfTan:
		rc = tan(v);
		break;
	case trfCot:
		if (!v)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_nonzero) << Arg::Str(function->name));
		}
		rc = fbcot(v);
		break;
	case trfAsin:
		if (v < -1 || v > 1)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_range_inc1_1) << Arg::Str(function->name));
		}
		rc = asin(v);
		break;
	case trfAcos:
		if (v < -1 || v > 1)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_range_inc1_1) << Arg::Str(function->name));
		}
		rc = acos(v);
		break;
	case trfAtan:
		rc = atan(v);
		break;
	case trfSinh:
		rc = sinh(v);
		break;
	case trfCosh:
		rc = cosh(v);
		break;
	case trfTanh:
		rc = tanh(v);
		break;
	case trfAsinh:
		rc = log(v + sqrt(v * v + 1));
		break;
	case trfAcosh:
		if (v < 1)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_gteq_one) << Arg::Str(function->name));
		}
		rc = log(v + sqrt(v - 1) * sqrt (v + 1));
		break;
	case trfAtanh:
		if (v <= -1 || v >= 1)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_range_exc1_1) << Arg::Str(function->name));
		}
		rc = log((1 + v) / (1 - v)) / 2;
		break;
	default:
		fb_assert(0);
		return NULL;
	}

	if (isinf(rc))
	{
		status_exception::raise(Arg::Gds(isc_arith_except) <<
								Arg::Gds(isc_sysf_fp_overflow) << Arg::Str(function->name));
	}

	impure->vlu_misc.vlu_double = rc;
	impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);

	return &impure->vlu_desc;
}


dsc* evlAbs(thread_db* tdbb, const SysFunction*, const NestValueArray& args, impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	EVL_make_value(tdbb, value, impure);

	switch (impure->vlu_desc.dsc_dtype)
	{
		case dtype_real:
			impure->vlu_misc.vlu_float = fabs(impure->vlu_misc.vlu_float);
			break;

		case dtype_double:
			impure->vlu_misc.vlu_double = fabs(impure->vlu_misc.vlu_double);
			break;

		case dtype_dec64:
			impure->vlu_misc.vlu_dec64 = impure->vlu_misc.vlu_dec64.abs();
			break;

		case dtype_dec128:
			impure->vlu_misc.vlu_dec128 = impure->vlu_misc.vlu_dec128.abs();
			break;

		case dtype_short:
		case dtype_long:
		case dtype_int64:
			impure->vlu_misc.vlu_int64 = MOV_get_int64(tdbb, value, value->dsc_scale);

			if (impure->vlu_misc.vlu_int64 == MIN_SINT64)
				status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			else if (impure->vlu_misc.vlu_int64 < 0)
				impure->vlu_misc.vlu_int64 = -impure->vlu_misc.vlu_int64;

			impure->vlu_desc.makeInt64(value->dsc_scale, &impure->vlu_misc.vlu_int64);
			break;

		default:
			impure->vlu_misc.vlu_double = fabs(MOV_get_double(tdbb, &impure->vlu_desc));
			impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
			break;
	}

	return &impure->vlu_desc;
}


dsc* evlAsciiChar(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	const SLONG code = MOV_get_long(tdbb, value, 0);
	if (!(code >= 0 && code <= 255))
		status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

	impure->vlu_misc.vlu_uchar = (UCHAR) code;
	impure->vlu_desc.makeText(1, ttype_none, &impure->vlu_misc.vlu_uchar);

	return &impure->vlu_desc;
}


dsc* evlAsciiVal(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	const CharSet* cs = INTL_charset_lookup(tdbb, value->getCharSet());

	UCHAR* p;
	MoveBuffer temp;
	int length = MOV_make_string2(tdbb, value, value->getCharSet(), &p, temp);

	if (length == 0)
		impure->vlu_misc.vlu_short = 0;
	else
	{
		UCHAR dummy[4];

		if (cs->substring(length, p, sizeof(dummy), dummy, 0, 1) != 1)
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_transliteration_failed));

		impure->vlu_misc.vlu_short = p[0];
	}

	impure->vlu_desc.makeShort(0, &impure->vlu_misc.vlu_short);

	return &impure->vlu_desc;
}


dsc* evlAtan2(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* desc1 = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if desc1 is NULL
		return NULL;

	const dsc* desc2 = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if desc2 is NULL
		return NULL;

	double value1 = MOV_get_double(tdbb, desc1);
	double value2 = MOV_get_double(tdbb, desc2);

	if (value1 == 0 && value2 == 0)
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
								Arg::Gds(isc_sysf_argscant_both_be_zero) << Arg::Str(function->name));
	}

	impure->vlu_misc.vlu_double = atan2(value1, value2);
	impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);

	return &impure->vlu_desc;
}


dsc* evlBin(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 1);
	fb_assert(function->misc != NULL);

	jrd_req* request = tdbb->getRequest();

	for (FB_SIZE_T i = 0; i < args.getCount(); ++i)
	{
		const dsc* value = EVL_expr(tdbb, request, args[i]);
		if (request->req_flags & req_null)	// return NULL if value is NULL
			return NULL;

		if (i == 0)
		{
			if ((Function)(IPTR) function->misc == funBinNot)
				impure->vlu_misc.vlu_int64 = ~MOV_get_int64(tdbb, value, 0);
			else
				impure->vlu_misc.vlu_int64 = MOV_get_int64(tdbb, value, 0);
		}
		else
		{
			switch ((Function)(IPTR) function->misc)
			{
				case funBinAnd:
					impure->vlu_misc.vlu_int64 &= MOV_get_int64(tdbb, value, 0);
					break;

				case funBinOr:
					impure->vlu_misc.vlu_int64 |= MOV_get_int64(tdbb, value, 0);
					break;

				case funBinXor:
					impure->vlu_misc.vlu_int64 ^= MOV_get_int64(tdbb, value, 0);
					break;

				default:
					fb_assert(false);
			}
		}
	}

	impure->vlu_desc.makeInt64(0, &impure->vlu_misc.vlu_int64);

	return &impure->vlu_desc;
}


dsc* evlBinShift(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);
	fb_assert(function->misc != NULL);

	jrd_req* request = tdbb->getRequest();

	const dsc* value1 = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value1 is NULL
		return NULL;

	const dsc* value2 = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value2 is NULL
		return NULL;

	const SINT64 shift = MOV_get_int64(tdbb, value2, 0);
	if (shift < 0)
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
								Arg::Gds(isc_sysf_argmustbe_nonneg) << Arg::Str(function->name));
	}

	const SINT64 rotshift = shift % sizeof(SINT64);
	SINT64 tempbits = 0;

	const SINT64 target = MOV_get_int64(tdbb, value1, 0);

	switch ((Function)(IPTR) function->misc)
	{
		case funBinShl:
			impure->vlu_misc.vlu_int64 = target << shift;
			break;

		case funBinShr:
			impure->vlu_misc.vlu_int64 = target >> shift;
			break;

		case funBinShlRot:
			tempbits = target >> (sizeof(SINT64) - rotshift);
			impure->vlu_misc.vlu_int64 = (target << rotshift) | tempbits;
			break;

		case funBinShrRot:
			tempbits = target << (sizeof(SINT64) - rotshift);
			impure->vlu_misc.vlu_int64 = (target >> rotshift) | tempbits;
			break;

		default:
			fb_assert(false);
	}

	impure->vlu_desc.makeInt64(0, &impure->vlu_misc.vlu_int64);

	return &impure->vlu_desc;
}


dsc* evlCeil(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	EVL_make_value(tdbb, value, impure);

	switch (impure->vlu_desc.dsc_dtype)
	{
		case dtype_short:
		case dtype_long:
		case dtype_int64:
			{
				SINT64 scale = 1;

				fb_assert(impure->vlu_desc.dsc_scale <= 0);
				for (int i = -impure->vlu_desc.dsc_scale; i > 0; --i)
					scale *= 10;

				const SINT64 v1 = MOV_get_int64(tdbb, &impure->vlu_desc, impure->vlu_desc.dsc_scale);
				const SINT64 v2 = MOV_get_int64(tdbb, &impure->vlu_desc, 0) * scale;

				impure->vlu_misc.vlu_int64 = v1 / scale;

				if (v1 > 0 && v1 != v2)
					++impure->vlu_misc.vlu_int64;

				impure->vlu_desc.makeInt64(0, &impure->vlu_misc.vlu_int64);
			}
			break;

		case dtype_real:
			impure->vlu_misc.vlu_float = ceil(impure->vlu_misc.vlu_float);
			break;

		default:
			impure->vlu_misc.vlu_double = MOV_get_double(tdbb, &impure->vlu_desc);
			// fall through

		case dtype_double:
			impure->vlu_misc.vlu_double = ceil(impure->vlu_misc.vlu_double);
			impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
			break;

		case dtype_dec64:
			impure->vlu_misc.vlu_dec64 = impure->vlu_misc.vlu_dec64.ceil(tdbb->getAttachment()->att_dec_status);
			impure->vlu_desc.makeDecimal64(&impure->vlu_misc.vlu_dec64);
			break;

		case dtype_dec128:
			impure->vlu_misc.vlu_dec128 = impure->vlu_misc.vlu_dec128.ceil(tdbb->getAttachment()->att_dec_status);
			impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
			break;
	}

	return &impure->vlu_desc;
}


string showInvalidChar(const UCHAR c)
{
	string str;
	str.printf("%c (ASCII %d)", c, c);
	return str;
}


dsc* evlCharToUuid(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (!value->isText())
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argviolates_uuidtype) << Arg::Str(function->name));
	}

	UCHAR* data_temp;
	USHORT len = MOV_get_string(tdbb, value, &data_temp, NULL, 0);
	const UCHAR* data;

	if (len > GUID_BODY_SIZE)
	{
		// Verify if only spaces exists after the expected length. See CORE-5062.
		data = data_temp + GUID_BODY_SIZE;

		while (len > GUID_BODY_SIZE)
		{
			if (*data++ != ASCII_SPACE)
				break;

			--len;
		}
	}

	data = data_temp;

	// validate the UUID
	if (len != GUID_BODY_SIZE) // 36
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argviolates_uuidlen) <<
										Arg::Num(GUID_BODY_SIZE) <<
										Arg::Str(function->name));
	}

	for (int i = 0; i < GUID_BODY_SIZE; ++i)
	{
		if (i == 8 || i == 13 || i == 18 || i == 23)
		{
			if (data[i] != '-')
			{
				status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
											Arg::Gds(isc_sysf_argviolates_uuidfmt) <<
												Arg::Str(showInvalidChar(data[i])) <<
												Arg::Num(i + 1) <<
												Arg::Str(function->name));
			}
		}
		else
		{
			const UCHAR c = data[i];
			const UCHAR hex = UPPER7(c);

			if (!((hex >= 'A' && hex <= 'F') || (c >= '0' && c <= '9')))
			{
				status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
											Arg::Gds(isc_sysf_argviolates_guidigits) <<
												Arg::Str(showInvalidChar(c)) <<
												Arg::Num(i + 1) <<
												Arg::Str(function->name));
			}
		}
	}

	// convert to binary representation
	char buffer[GUID_BUFF_SIZE];
	buffer[0] = '{';
	buffer[37] = '}';
	buffer[38] = '\0';
	memcpy(buffer + 1, data, GUID_BODY_SIZE);

	USHORT bytes[16];
	sscanf(buffer, GUID_NEW_FORMAT,
		&bytes[0], &bytes[1], &bytes[2], &bytes[3],
		&bytes[4], &bytes[5], &bytes[6], &bytes[7],
		&bytes[8], &bytes[9], &bytes[10], &bytes[11],
		&bytes[12], &bytes[13], &bytes[14], &bytes[15]);

	UCHAR resultData[16];
	for (unsigned i = 0; i < 16; ++i)
		resultData[i] = (UCHAR) bytes[i];

	dsc result;
	result.makeText(16, ttype_binary, resultData);
	EVL_make_value(tdbb, &result, impure);

	return &impure->vlu_desc;
}


/* As seen in blr.h; keep this array "extractParts" in sync.
#define blr_extract_year		(unsigned char)0
#define blr_extract_month		(unsigned char)1
#define blr_extract_day			(unsigned char)2
#define blr_extract_hour		(unsigned char)3
#define blr_extract_minute		(unsigned char)4
#define blr_extract_second		(unsigned char)5
#define blr_extract_weekday		(unsigned char)6
#define blr_extract_yearday		(unsigned char)7
#define blr_extract_millisecond	(unsigned char)8
#define blr_extract_week		(unsigned char)9
*/

const char* extractParts[10] =
{
	"YEAR", "MONTH", "DAY", "HOUR", "MINUTE", "SECOND", "WEEKDAY", "YEARDAY", "MILLISECOND", "WEEK"
};

const char* getPartName(int n)
{
	if (n < 0 || n >= FB_NELEM(extractParts))
		return "Unknown";

	return extractParts[n];
}


dsc* evlDateAdd(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 3);

	jrd_req* request = tdbb->getRequest();

	const dsc* quantityDsc = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if quantityDsc is NULL
		return NULL;

	const dsc* partDsc = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if partDsc is NULL
		return NULL;

	const dsc* valueDsc = EVL_expr(tdbb, request, args[2]);
	if (request->req_flags & req_null)	// return NULL if valueDsc is NULL
		return NULL;

	const SLONG part = MOV_get_long(tdbb, partDsc, 0);

	TimeStamp timestamp;

	switch (valueDsc->dsc_dtype)
	{
		case dtype_sql_time:
		case dtype_sql_time_tz:
			timestamp.value().timestamp_time = *(GDS_TIME*) valueDsc->dsc_address;
			timestamp.value().timestamp_date =
				(TimeStamp::MAX_DATE - TimeStamp::MIN_DATE) / 2 + TimeStamp::MIN_DATE;

			if (part != blr_extract_hour &&
				part != blr_extract_minute &&
				part != blr_extract_second &&
				part != blr_extract_millisecond)
			{
				status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
											Arg::Gds(isc_sysf_invalid_addpart_time) <<
												Arg::Str(function->name));
			}
			break;

		case dtype_sql_date:
			timestamp.value().timestamp_date = *(GDS_DATE*) valueDsc->dsc_address;
			timestamp.value().timestamp_time = 0;
			/*
			if (part == blr_extract_hour ||
				part == blr_extract_minute ||
				part == blr_extract_second ||
				part == blr_extract_millisecond)
			{
				status_exception::raise(Arg::Gds(isc_expression_eval_err));
			}
			*/
			break;

		case dtype_timestamp:
		case dtype_timestamp_tz:
			timestamp.value() = *(GDS_TIMESTAMP*) valueDsc->dsc_address;
			break;

		default:
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_add_datetime) <<
											Arg::Str(function->name));
			break;
	}

	static const SSHORT milliScale = ISC_TIME_SECONDS_PRECISION_SCALE + 3;
	static const int milliPow = NoThrowTimeStamp::POW_10_TABLE[-milliScale];

	const SINT64 quantity = MOV_get_int64(tdbb, quantityDsc,
		(part == blr_extract_millisecond ? milliScale : 0));

	switch (part)
	{
		// TO DO: detect overflow in the following cases.

		case blr_extract_year:
			{
				tm times;
				timestamp.decode(&times);
				times.tm_year += quantity;
				timestamp.encode(&times);

				int day = times.tm_mday;
				timestamp.decode(&times);

				if (times.tm_mday != day)
					--timestamp.value().timestamp_date;
			}
			break;

		case blr_extract_month:
			{
				tm times;
				timestamp.decode(&times);

				int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

				const int y = quantity / 12;
				const int m = quantity % 12;

				const int ld = md[times.tm_mon] - times.tm_mday;
				const int lm = times.tm_mon;
				times.tm_year += y;

				if ((times.tm_mon += m) > 11)
				{
					times.tm_year++;
					times.tm_mon -= 12;
				}
				else if (times.tm_mon < 0)
				{
					times.tm_year--;
					times.tm_mon += 12;
				}

				const int ly = times.tm_year + 1900;

				if ((ly % 4 == 0 && ly % 100 != 0) || (ly % 400 == 0))
					md[1]++;

				if (y >= 0 && m >= 0 && times.tm_mday > md[lm])
					times.tm_mday = md[times.tm_mon] - ld;

				if (times.tm_mday > md[times.tm_mon])
					times.tm_mday = md[times.tm_mon];
				else if (times.tm_mday < 1)
					times.tm_mday = 1;

				timestamp.encode(&times);
			}
			break;

		case blr_extract_day:
			timestamp.value().timestamp_date += quantity;
			break;

		case blr_extract_week:
			timestamp.value().timestamp_date += quantity * 7;
			break;

		case blr_extract_hour:
			if (valueDsc->dsc_dtype == dtype_sql_date)
				timestamp.value().timestamp_date += quantity / 24;
			else
				add10msec(&timestamp.value(), quantity, 3600 * ISC_TIME_SECONDS_PRECISION);
			break;

		case blr_extract_minute:
			if (valueDsc->dsc_dtype == dtype_sql_date)
				timestamp.value().timestamp_date += quantity / 1440; // 1440 == 24 * 60
			else
				add10msec(&timestamp.value(), quantity, 60 * ISC_TIME_SECONDS_PRECISION);
			break;

		case blr_extract_second:
			if (valueDsc->dsc_dtype == dtype_sql_date)
				timestamp.value().timestamp_date += quantity / oneDay;
			else
				add10msec(&timestamp.value(), quantity, ISC_TIME_SECONDS_PRECISION);
			break;

		case blr_extract_millisecond:
			if (valueDsc->dsc_dtype == dtype_sql_date)
				timestamp.value().timestamp_date += quantity / milliPow / (oneDay * 1000);
			else
				add10msec(&timestamp.value(), quantity, ISC_TIME_SECONDS_PRECISION / 1000 / milliPow);
			break;

		default:
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_addpart_dtime) <<
											Arg::Str(getPartName(part)) <<
											Arg::Str(function->name));
			break;
	}

	if (!TimeStamp::isValidTimeStamp(timestamp.value()))
		status_exception::raise(Arg::Gds(isc_datetime_range_exceeded));

	EVL_make_value(tdbb, valueDsc, impure);

	switch (impure->vlu_desc.dsc_dtype)
	{
		case dtype_sql_time:
		case dtype_sql_time_tz:
			impure->vlu_misc.vlu_sql_time = timestamp.value().timestamp_time;
			break;

		case dtype_sql_date:
			impure->vlu_misc.vlu_sql_date = timestamp.value().timestamp_date;
			break;

		case dtype_timestamp:
		case dtype_timestamp_tz:
			impure->vlu_misc.vlu_timestamp = timestamp.value();
			break;

		default:
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_add_dtime_rc));
			break;
	}

	return &impure->vlu_desc;
}

// Prepare tomcrypt library

class TomcryptInitializer
{
public:
	explicit TomcryptInitializer(MemoryPool&)
	{
		ltc_mp = ltm_desc;

		registerCipher(aes_desc);
		registerCipher(anubis_desc);
		registerCipher(blowfish_desc);
		registerCipher(khazad_desc);
		registerCipher(rc5_desc);
		registerCipher(rc6_desc);
		registerCipher(saferp_desc);
		registerCipher(twofish_desc);
		registerCipher(xtea_desc);

		registerHash(md5_desc);
		registerHash(sha1_desc);
		registerHash(sha256_desc);
		registerHash(sha512_desc);
	}

private:
	template <typename T>
	void registerCipher(T& desc)
	{
		if (register_cipher(&desc) == -1)
			status_exception::raise(Arg::Gds(isc_tom_reg) << "cipher");
	}

	template <typename T>
	void registerHash(T& desc)
	{
		if (register_hash(&desc) == -1)
			status_exception::raise(Arg::Gds(isc_tom_reg) << "hash");
	}
};

InitInstance<TomcryptInitializer> tomcryptInitializer;


class PseudoRandom
{
public:
	explicit PseudoRandom(MemoryPool&)
	{
		// register yarrow
		index = register_prng(&yarrow_desc);
		if (index == -1)
			status_exception::raise(Arg::Gds(isc_random) << "Error registering PRNG yarrow");

		// setup the PRNG
		tomCheck(yarrow_start(&state), Arg::Gds(isc_tom_yarrow_start));
		tomCheck(rng_make_prng(64, index, &state, NULL),  Arg::Gds(isc_tom_yarrow_setup));
	}

	~PseudoRandom()
	{
		yarrow_done(&state);
	}

	prng_state* getState()
	{
		return &state;
	}

	int getIndex()
	{
		return index;
	}

private:
	int index;
	prng_state state;
};

InitInstance<PseudoRandom> prng;


// Data exchange between tommath and firebird

const UCHAR streamBpb[] = {isc_bpb_version1, isc_bpb_type, 1, isc_bpb_type_stream};

class DataPipe
{
public:
	DataPipe(thread_db* t, const dsc* desc, impure_value* i)
		: tdbb(t),
		  impure(i),
		  blobMode(desc->isBlob()),
		  completed(false),
		  ptr(nullptr),
		  len(0),
		  blob(nullptr),
		  newBlob(nullptr)
	{
		if (!blobMode)
		{
			if (!desc->isText())
				status_exception::raise(Arg::Gds(isc_tom_strblob));
			ptr = CVT_get_bytes(desc, len);
		}
		else
		{
			blobDesc.makeBlob(0, ttype_none);
			blobDesc.dsc_address = (UCHAR*) &impure->vlu_misc.vlu_bid;

			try
			{
				const UCHAR streamBpb[] = {isc_bpb_version1, isc_bpb_type, 1, isc_bpb_type_stream};
				newBlob = blb::create2(tdbb, tdbb->getRequest()->req_transaction, &impure->vlu_misc.vlu_bid,
					sizeof(streamBpb), streamBpb);
				blob = blb::open2(tdbb, tdbb->getRequest()->req_transaction, reinterpret_cast<bid*>(desc->dsc_address),
					sizeof(streamBpb), streamBpb);

				ptr = inBuf.getBuffer(BLOB_STEP);
				len = blob->BLB_get_data(tdbb, inBuf.begin(), inBuf.getCount(), false);
			}
			catch (...)
			{
				closeBlobs();
			}
		}
	}

	~DataPipe()
	{
		closeBlobs();

		if (!completed)
		{
			dsc result;
			result.makeText(0, ttype_none, outBuf.begin());
			EVL_make_value(tdbb, &result, impure);
			impure->vlu_desc.setNull();
		}
	}

	const UCHAR* from()
	{
		return ptr;
	}

	UCHAR* to()
	{
		return outBuf.getBuffer(length());
	}

	unsigned length()
	{
		return len;
	}

	bool hasData()
	{
		return len > 0;
	}

	void next()
	{
		if (hasData())
		{
			impure->vlu_desc.clear();

			if (!blobMode)
			{
				dsc result;
				result.makeText(outBuf.getCount(), ttype_binary, outBuf.begin());
				EVL_make_value(tdbb, &result, impure);

				len = 0;
				completed = true;
			}
			else
			{
				newBlob->BLB_put_data(tdbb, outBuf.begin(), outBuf.getCount());

				len = blob->BLB_get_data(tdbb, inBuf.begin(), inBuf.getCount(), false);
				if (!len)
				{
					closeBlobs();
					EVL_make_value(tdbb, &blobDesc, impure);
					completed = true;
				}
			}
		}
	}

private:
	const FB_SIZE_T BLOB_STEP = 1024;

	thread_db* tdbb;
	UCharBuffer inBuf, outBuf;
	impure_value* impure;
	bool blobMode, completed;
	const UCHAR* ptr;
	unsigned len;
	dsc blobDesc;
	blb* blob;
	blb* newBlob;

	void closeBlobs()
	{
		if (newBlob)
		{
			newBlob->BLB_close(tdbb);
			newBlob = nullptr;
		}

		if (blob)
		{
			blob->BLB_close(tdbb);
			blob = nullptr;
		}
	}
};


// Lists of constant parameter values

class CodeValue
{
public:
	unsigned code;
	const char* value;
};

CodeValue* find(CodeValue* array, MetaName& name)
{
	for (; array->value; ++array)
	{
		if (name == array->value)
			return array;
	}

	return nullptr;
}


dsc* evlEncryptDecrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure, bool encryptFlag)
{
	tomcryptInitializer();

	fb_assert(args.getCount() == CRYPT_ARG_MAX);

	jrd_req* request = tdbb->getRequest();

	// parse args and check correctness
	const dsc* dscs[CRYPT_ARG_MAX];
	for (unsigned i = 0; i < CRYPT_ARG_MAX; ++i)
		dscs[i] = EVL_expr(tdbb, request, args[i]);

	MetaName algorithmName, modeName, counterType;
	MOV_get_metaname(tdbb, dscs[CRYPT_ARG_ALGORITHM], algorithmName);

	const unsigned ALG_RC4 = 1;
	const unsigned ALG_CHACHA = 2;
	const unsigned ALG_SOBER = 3;
	static CodeValue algorithms[] = {
		{ ALG_RC4, "RC4" },
		{ ALG_CHACHA, "CHACHA20" },
		{ ALG_SOBER, "SOBER128" },
		{ 0, nullptr }
	};

	CodeValue* a = nullptr;
	string aName(algorithmName);
	aName.lower();
	int cipher = find_cipher(aName.c_str());
	if (cipher < 0)
	{
		a = find(algorithms, algorithmName);
		if (!a)
			status_exception::raise(Arg::Gds(isc_tom_algorithm) << algorithmName);
	}

	const unsigned MODE_ECB = 1;
	const unsigned MODE_CBC = 2;
	const unsigned MODE_CTR = 3;
	const unsigned MODE_CFB = 4;
	const unsigned MODE_OFB = 5;
	static CodeValue modes[] = {
		{ MODE_ECB, "ECB" },
		{ MODE_CBC, "CBC" },
		{ MODE_CTR, "CTR" },
		{ MODE_CFB, "CFB" },
		{ MODE_OFB, "OFB" },
		{ 0, nullptr }
	};

	CodeValue* m = nullptr;
	MOV_get_metaname(tdbb, dscs[CRYPT_ARG_MODE], modeName);
	if (cipher >= 0)
	{
		if (!modeName.hasData())
			status_exception::raise(Arg::Gds(isc_tom_mode_miss));

		m = find(modes, modeName);
		if (!m)
			status_exception::raise(Arg::Gds(isc_tom_mode_bad));
	}
	else if (modeName.hasData())
		status_exception::raise(Arg::Gds(isc_tom_no_mode));

	unsigned len;
	const void* data = CVT_get_bytes(dscs[CRYPT_ARG_KEY], len);
	UCharBuffer key;
	memcpy(key.getBuffer(len), data, len);

	UCharBuffer iv;
	data = CVT_get_bytes(dscs[CRYPT_ARG_IV], len);
	if ((m && (m->code != MODE_ECB)) || (a && (a->code != ALG_RC4)))	// all other need IV
	{
		if (!len)
			status_exception::raise(Arg::Gds(isc_tom_iv_miss));
		memcpy(iv.getBuffer(len), data, len);
	}
	else if (len)
		status_exception::raise(Arg::Gds(isc_tom_no_iv));

	const unsigned CTR_32 = 1;
	const unsigned CTR_64 = 2;
	const unsigned CTR_LITTLE_ENDIAN = 3;
	const unsigned CTR_BIG_ENDIAN = 4;

	static CodeValue counterTypes[] = {
		{ CTR_LITTLE_ENDIAN, "CTR_LITTLE_ENDIAN" },
		{ CTR_BIG_ENDIAN, "CTR_BIG_ENDIAN" },
		{ 0, nullptr }
	};

	CodeValue *c = nullptr;
	MOV_get_metaname(tdbb, dscs[CRYPT_ARG_CTRTYPE], counterType);
	if (m && (m->code == MODE_CTR))
	{
		if (counterType.hasData())
		{
			c = find(counterTypes, counterType);
			if (!c)
				status_exception::raise(Arg::Gds(isc_tom_ctrtype_bad) << counterType);
		}
		else
			c = &counterTypes[CTR_LITTLE_ENDIAN];
	}
	else if (counterType.hasData())
		status_exception::raise(Arg::Gds(isc_tom_no_ctrtype) << m->value);

	FB_UINT64 ctrVal = 0;
	if ((m && (m->code == MODE_CTR)) || (a && (a->code == ALG_CHACHA)))
	{
		if (dscs[CRYPT_ARG_COUNTER]->dsc_length)
		{
			ctrVal = MOV_get_int64(tdbb, dscs[CRYPT_ARG_COUNTER], 0);
			if (m && ctrVal > key.getCount())
				status_exception::raise(Arg::Gds(isc_tom_ctr_big) << Arg::Num(ctrVal) <<  Arg::Num(key.getCount()));
		}
	}
	else if (dscs[CRYPT_ARG_COUNTER]->dsc_length)
			status_exception::raise(Arg::Gds(isc_tom_no_ctr) << (m ? "mode" : "cipher") << (m ? m->value : a->value));

	// Run selected algorithm
	DataPipe dp(tdbb, dscs[CRYPT_ARG_VALUE], impure);
	if (m)
	{
		unsigned blockLen = cipher_descriptor[cipher].block_length;
		if (iv.hasData() && iv.getCount() != blockLen)
			status_exception::raise(Arg::Gds(isc_tom_iv_length) << Arg::Num(iv.getCount()) << Arg::Num(blockLen));

		switch (m->code)
		{
		case MODE_ECB:
			{
				symmetric_ECB ecb;
				tomCheck(ecb_start(cipher, key.begin(), key.getCount(), 0, &ecb), Arg::Gds(isc_tom_init_mode) << "ECB");

				while (dp.hasData())
				{
					if (encryptFlag)
						tomCheck(ecb_encrypt(dp.from(), dp.to(), dp.length(), &ecb), Arg::Gds(isc_tom_crypt_mode) << "ECB");
					else
						tomCheck(ecb_decrypt(dp.from(), dp.to(), dp.length(), &ecb), Arg::Gds(isc_tom_decrypt_mode) << "ECB");
					dp.next();
				}
				ecb_done(&ecb);
			}
			break;

		case MODE_CBC:
			{
				symmetric_CBC cbc;
				tomCheck(cbc_start(cipher, iv.begin(), key.begin(), key.getCount(), 0, &cbc), Arg::Gds(isc_tom_init_mode) << "CBC");

				while (dp.hasData())
				{
					if (encryptFlag)
						tomCheck(cbc_encrypt(dp.from(), dp.to(), dp.length(), &cbc), Arg::Gds(isc_tom_crypt_mode) << "CBC");
					else
						tomCheck(cbc_decrypt(dp.from(), dp.to(), dp.length(), &cbc), Arg::Gds(isc_tom_decrypt_mode) << "CBC");
					dp.next();
				}
				cbc_done(&cbc);
			}
			break;

		case MODE_CFB:
			{
				symmetric_CFB cfb;
				tomCheck(cfb_start(cipher, iv.begin(), key.begin(), key.getCount(), 0, &cfb), Arg::Gds(isc_tom_init_mode) << "CFB");

				while (dp.hasData())
				{
					if (encryptFlag)
						tomCheck(cfb_encrypt(dp.from(), dp.to(), dp.length(), &cfb), Arg::Gds(isc_tom_crypt_mode) << "CFB");
					else
						tomCheck(cfb_decrypt(dp.from(), dp.to(), dp.length(), &cfb), Arg::Gds(isc_tom_decrypt_mode) << "CFB");
					dp.next();
				}
				cfb_done(&cfb);
			}
			break;

		case MODE_OFB:
			{
				symmetric_OFB ofb;
				tomCheck(ofb_start(cipher, iv.begin(), key.begin(), key.getCount(), 0, &ofb), Arg::Gds(isc_tom_init_mode) << "OFB");

				while (dp.hasData())
				{
					if (encryptFlag)
						tomCheck(ofb_encrypt(dp.from(), dp.to(), dp.length(), &ofb), Arg::Gds(isc_tom_crypt_mode) << "OFB");
					else
						tomCheck(ofb_decrypt(dp.from(), dp.to(), dp.length(), &ofb), Arg::Gds(isc_tom_decrypt_mode) << "OFB");
					dp.next();
				}
				ofb_done(&ofb);
			}
			break;

		case MODE_CTR:
			{
				symmetric_CTR ctr;
				tomCheck(ctr_start(cipher, iv.begin(), key.begin(), key.getCount(), 0,
					(c->code == CTR_LITTLE_ENDIAN ? CTR_COUNTER_LITTLE_ENDIAN : CTR_COUNTER_BIG_ENDIAN) | ctrVal,
					&ctr), Arg::Gds(isc_tom_init_mode) << "CTR");

				while (dp.hasData())
				{
					if (encryptFlag)
						tomCheck(ctr_encrypt(dp.from(), dp.to(), dp.length(), &ctr), Arg::Gds(isc_tom_crypt_mode) << "CTR");
					else
						tomCheck(ctr_decrypt(dp.from(), dp.to(), dp.length(), &ctr), Arg::Gds(isc_tom_decrypt_mode) << "CTR");
					dp.next();
				}
				ctr_done(&ctr);
			}
			break;
		}
	}
	else
	{
		fb_assert(a);
		switch (a->code)
		{
		case ALG_RC4:
			{
				rc4_state rc4;
				tomCheck(rc4_stream_setup(&rc4, key.begin(), key.getCount()), Arg::Gds(isc_tom_init_cip) << "RC4");

				while (dp.hasData())
				{
					tomCheck(rc4_stream_crypt(&rc4, dp.from(), dp.length(), dp.to()),
						Arg::Gds(encryptFlag ? isc_tom_crypt_cip : isc_tom_decrypt_cip) << "RC4");
					dp.next();
				}
				rc4_stream_done(&rc4);
			}
			break;

		case ALG_CHACHA:
			{
				chacha_state chacha;
				switch (key.getCount())
				{
				case 16:
				case 32:
					break;
				default:
					status_exception::raise(Arg::Gds(isc_tom_chacha_key) << Arg::Num(key.getCount()));
				}
				tomCheck(chacha_setup(&chacha, key.begin(), key.getCount(), 20), Arg::Gds(isc_tom_init_cip) << "CHACHA#20");
				switch (iv.getCount())
				{
				case 12:
					tomCheck(chacha_ivctr32(&chacha, iv.begin(), iv.getCount(), ctrVal), Arg::Gds(isc_tom_setup_cip) << "CHACHA#20");
					break;
				case 8:
					tomCheck(chacha_ivctr64(&chacha, iv.begin(), iv.getCount(), ctrVal),  Arg::Gds(isc_tom_setup_cip) << "CHACHA#20");
					break;
				default:
					status_exception::raise(Arg::Gds(isc_tom_setup_chacha) << Arg::Num(iv.getCount()));
					break;
				}

				while (dp.hasData())
				{
					tomCheck(chacha_crypt(&chacha, dp.from(), dp.length(), dp.to()),
						Arg::Gds(encryptFlag ? isc_tom_crypt_cip : isc_tom_decrypt_cip) << "CHACHA#20");
					dp.next();
				}
				chacha_done(&chacha);
			}
			break;

		case ALG_SOBER:
			{
				sober128_state sober128;
				tomCheck(sober128_stream_setup(&sober128, key.begin(), key.getCount()), Arg::Gds(isc_tom_init_cip) << "SOBER-128");
				tomCheck(sober128_stream_setiv(&sober128, iv.begin(), iv.getCount()),  Arg::Gds(isc_tom_setup_cip) << "SOBER-128");

				while (dp.hasData())
				{
					tomCheck(sober128_stream_crypt(&sober128, dp.from(), dp.length(), dp.to()),
						Arg::Gds(encryptFlag ? isc_tom_crypt_cip : isc_tom_decrypt_cip) << "SOBER-128");
					dp.next();
				}
				sober128_stream_done(&sober128);
			}
			break;
		}
	}

	return &impure->vlu_desc;
}

dsc* evlEncrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	return evlEncryptDecrypt(tdbb, function, args, impure, true);
}

dsc* evlDecrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	return evlEncryptDecrypt(tdbb, function, args, impure, false);
}


dsc* evlEncodeDecode64(thread_db* tdbb, bool encodeFlag, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	const dsc* arg = EVL_expr(tdbb, tdbb->getRequest(), args[0]);
	if (!arg)	// return NULL if value is NULL
		return NULL;

	UCharBuffer in;
	if (arg->isBlob())
	{
		AutoPtr<blb> blob(blb::open2(tdbb, tdbb->getRequest()->req_transaction, reinterpret_cast<const bid*>(arg->dsc_address),
			sizeof(streamBpb), streamBpb));

		UCHAR buf[4096];
		in.clear();
		for(;;)
		{
			const unsigned l = blob->BLB_get_data(tdbb, buf, sizeof buf, false);
			if (!l)
				break;
			in.append(buf, l);
		}

		blob->BLB_close(tdbb);
		blob.release();
	}
	else
	{
		unsigned len;
		const UCHAR* ptr = CVT_get_bytes(arg, len);
		in.assign(ptr, len);
	}

	UCharBuffer out;
	unsigned long outLen = encodeFlag ? encodeLen(in.getCount()) + 1 : decodeLen(in.getCount());
	auto* func = encodeFlag ? base64_encode : base64_decode;
	tomCheck(func(in.begin(), in.getCount(), out.getBuffer(outLen), &outLen),
		Arg::Gds(encodeFlag ? isc_tom_encode : isc_tom_decode) << "BASE64");
	out.resize(outLen);

	dsc result;
	if (arg->isBlob())
	{
		AutoPtr<blb> blob(blb::create2(tdbb, tdbb->getRequest()->req_transaction, &impure->vlu_misc.vlu_bid,
			sizeof(streamBpb), streamBpb));
		blob->BLB_put_data(tdbb, out.begin(), out.getCount());
		blob->BLB_close(tdbb);
		blob.release();

		result.makeBlob(encodeFlag ? isc_blob_text : isc_blob_untyped, encodeFlag ? ttype_ascii : ttype_binary,
			(ISC_QUAD*)&impure->vlu_misc.vlu_bid);
	}
	else
		result.makeText(out.getCount(), encodeFlag ? ttype_ascii : ttype_binary, const_cast<UCHAR*>(out.begin()));

	EVL_make_value(tdbb, &result, impure);
	return &impure->vlu_desc;
}

dsc* evlDecode64(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	return evlEncodeDecode64(tdbb, false, function, args, impure);
}

dsc* evlEncode64(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	return evlEncodeDecode64(tdbb, true, function, args, impure);
}


dsc* evlCrc32(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	crc32_state ctx;
	crc32_init(&ctx);

	const dsc* arg = EVL_expr(tdbb, tdbb->getRequest(), args[0]);
	if (!arg)	// return NULL if value is NULL
		return NULL;

	if (arg->isBlob())
	{
		blb* blob = blb::open2(tdbb, tdbb->getRequest()->req_transaction, reinterpret_cast<const bid*>(arg->dsc_address),
			sizeof(streamBpb), streamBpb);

		UCHAR buf[4096];
		for(;;)
		{
			const unsigned l = blob->BLB_get_data(tdbb, buf, sizeof buf, false);
			if (!l)
				break;
			crc32_update(&ctx, buf, l);
		}

		blob->BLB_close(tdbb);
	}
	else
	{
		unsigned len;
		const UCHAR* ptr = CVT_get_bytes(arg, len);
		crc32_update(&ctx, ptr, len);
	}

	SLONG hash;
	crc32_finish(&ctx, &hash, sizeof hash);

	dsc result;
	result.makeLong(0, &hash);
	EVL_make_value(tdbb, &result, impure);

	return &impure->vlu_desc;
}


dsc* evlRsaEncryptDecrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure, bool encryptFlag)
{
	tomcryptInitializer();

	fb_assert(args.getCount() == RSA_CRYPT_ARG_MAX);

	jrd_req* request = tdbb->getRequest();

	// parse args and check correctness
	const dsc* dscs[RSA_CRYPT_ARG_MAX];
	for (unsigned i = 0; i < RSA_CRYPT_ARG_MAX; ++i)
		dscs[i] = EVL_expr(tdbb, request, args[i]);

	MetaName hashName;
	if (dscs[RSA_CRYPT_ARG_HASH])
		MOV_get_metaname(tdbb, dscs[RSA_CRYPT_ARG_HASH], hashName);
	if (!hashName.hasData())
		hashName = "SHA256";
	string aName(hashName);
	aName.lower();
	int hash = find_hash(aName.c_str());
	if (hash < 0)
		status_exception::raise(Arg::Gds(isc_tom_hash_bad) << hashName);

	unsigned len;
	const UCHAR* data = CVT_get_bytes(dscs[RSA_CRYPT_ARG_VALUE], len);
	if (!data)
		return nullptr;

	unsigned keyLen;
	const UCHAR* key = CVT_get_bytes(dscs[RSA_CRYPT_ARG_KEY], keyLen);
	if (!key)
		return nullptr;

	unsigned paramLen;
	const UCHAR* lParam = CVT_get_bytes(dscs[RSA_CRYPT_ARG_LPARAM], paramLen);
	if (!paramLen)
		lParam = nullptr;

	// Run tomcrypt functions
	rsa_key rsaKey;
	tomCheck(rsa_import(key, keyLen, &rsaKey), Arg::Gds(isc_tom_rsa_import));

	unsigned long outlen = encryptFlag ? 256 : 190;
	UCharBuffer outBuf;
	int stat = 0;
	int cryptRc = encryptFlag ? rsa_encrypt_key(data, len, outBuf.getBuffer(outlen), &outlen, lParam, paramLen,
			prng().getState(), prng().getIndex(), hash, &rsaKey) :
		rsa_decrypt_key(data, len, outBuf.getBuffer(outlen), &outlen, lParam, paramLen, hash, &stat, &rsaKey);
	rsa_free(&rsaKey);
	tomCheck(cryptRc, Arg::Gds(encryptFlag ? isc_tom_crypt_cip : isc_tom_decrypt_cip) << "RSA");
	if ((!encryptFlag) && (!stat))
		status_exception::raise(Arg::Gds(isc_tom_oaep));

	dsc result;
	result.makeText(outlen, ttype_binary, outBuf.begin());
	EVL_make_value(tdbb, &result, impure);
	return &impure->vlu_desc;
}

dsc* evlRsaDecrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	return evlRsaEncryptDecrypt(tdbb, function, args, impure, false);
}

dsc* evlRsaEncrypt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	return evlRsaEncryptDecrypt(tdbb, function, args, impure, true);
}

dsc* evlRsaPrivate(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	tomcryptInitializer();

	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	const SLONG length = MOV_get_long(tdbb, value, 0);
	if (length < 1 || length > 1024)
		status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

	rsa_key rsaKey;
	tomCheck(rsa_make_key(prng().getState(), prng().getIndex(), length, 65537, &rsaKey), Arg::Gds(isc_tom_rsa_make));

	unsigned long outlen = length * 16;
	UCharBuffer key;
	int cryptRc = rsa_export(key.getBuffer(outlen), &outlen, PK_PRIVATE, &rsaKey);
	rsa_free(&rsaKey);
	tomCheck(cryptRc, Arg::Gds(isc_tom_rsa_export) << "private");

	dsc result;
	result.makeText(outlen, ttype_binary, key.begin());
	EVL_make_value(tdbb, &result, impure);
	return &impure->vlu_desc;
}

dsc* evlRsaPublic(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	tomcryptInitializer();

	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	unsigned len;
	const UCHAR* data = CVT_get_bytes(value, len);
	rsa_key rsaKey;
	tomCheck(rsa_import(data, len, &rsaKey), Arg::Gds(isc_tom_rsa_import));

	unsigned long outlen = len;
	UCharBuffer key;
	int cryptRc = rsa_export(key.getBuffer(outlen), &outlen, PK_PUBLIC, &rsaKey);
	rsa_free(&rsaKey);
	tomCheck(cryptRc, Arg::Gds(isc_tom_rsa_export) << "public");

	dsc result;
	result.makeText(outlen, ttype_binary, key.begin());
	EVL_make_value(tdbb, &result, impure);
	return &impure->vlu_desc;
}


dsc* evlRsaSign(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	tomcryptInitializer();

	fb_assert(args.getCount() == RSA_SIGN_ARG_MAX);

	jrd_req* request = tdbb->getRequest();

	// parse args and check correctness
	const dsc* dscs[RSA_SIGN_ARG_MAX];
	for (unsigned i = 0; i < RSA_SIGN_ARG_MAX; ++i)
		dscs[i] = EVL_expr(tdbb, request, args[i]);

	MetaName hashName;
	if (dscs[RSA_SIGN_ARG_HASH])
		MOV_get_metaname(tdbb, dscs[RSA_SIGN_ARG_HASH], hashName);
	if (!hashName.hasData())
		hashName = "SHA256";
	string aName(hashName);
	aName.lower();
	int hash = find_hash(aName.c_str());
	if (hash < 0)
		status_exception::raise(Arg::Gds(isc_tom_hash_bad) << hashName);

	unsigned len;
	const UCHAR* data = CVT_get_bytes(dscs[RSA_SIGN_ARG_VALUE], len);
	if (!data)
		return nullptr;

	SLONG saltLength = 8;
	if (dscs[RSA_SIGN_ARG_SALTLEN]->dsc_length)
	{
		saltLength = MOV_get_long(tdbb, dscs[RSA_SIGN_ARG_SALTLEN], 0);
		if (saltLength < 0 || saltLength > 32)
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
	}

	unsigned keyLen;
	const UCHAR* key = CVT_get_bytes(dscs[RSA_SIGN_ARG_KEY], keyLen);
	if (!key)
		return nullptr;
	rsa_key rsaKey;
	tomCheck(rsa_import(key, keyLen, &rsaKey), Arg::Gds(isc_tom_rsa_import));

	unsigned long signLen = 1024;
	UCharBuffer sign;
	int cryptRc = rsa_sign_hash(data, len, sign.getBuffer(signLen), &signLen,
		prng().getState(), prng().getIndex(), hash, saltLength, &rsaKey);
	rsa_free(&rsaKey);
	tomCheck(cryptRc, Arg::Gds(isc_tom_rsa_sign));

	dsc result;
	result.makeText(signLen, ttype_binary, sign.begin());
	EVL_make_value(tdbb, &result, impure);
	return &impure->vlu_desc;
}


static dsc* boolResult(thread_db* tdbb, impure_value* impure, bool value)
{
	dsc result;
	FB_BOOLEAN rc = value ? FB_TRUE : FB_FALSE;
	result.makeBoolean(&rc);

	EVL_make_value(tdbb, &result, impure);
	return &impure->vlu_desc;
}


dsc* evlRsaVerify(thread_db* tdbb, const SysFunction* function, const NestValueArray& args, impure_value* impure)
{
	tomcryptInitializer();

	fb_assert(args.getCount() == RSA_VERIFY_ARG_MAX);

	jrd_req* request = tdbb->getRequest();

	// parse args and check correctness
	const dsc* dscs[RSA_VERIFY_ARG_MAX];
	for (unsigned i = 0; i < RSA_VERIFY_ARG_MAX; ++i)
		dscs[i] = EVL_expr(tdbb, request, args[i]);

	MetaName hashName;
	if (dscs[RSA_VERIFY_ARG_HASH])
		MOV_get_metaname(tdbb, dscs[RSA_VERIFY_ARG_HASH], hashName);
	if (!hashName.hasData())
		hashName = "SHA256";
	string aName(hashName);
	aName.lower();
	int hash = find_hash(aName.c_str());
	if (hash < 0)
		status_exception::raise(Arg::Gds(isc_tom_hash_bad) << hashName);

	unsigned len;
	const UCHAR* data = CVT_get_bytes(dscs[RSA_VERIFY_ARG_VALUE], len);
	if (!data)
		return nullptr;

	unsigned signLen;
	const UCHAR* sign = CVT_get_bytes(dscs[RSA_VERIFY_ARG_SIGNATURE], signLen);
	if (!sign)
		return boolResult(tdbb, impure, false);

	SLONG saltLength = 8;
	if (dscs[RSA_VERIFY_ARG_SALTLEN]->dsc_length)
	{
		saltLength = MOV_get_long(tdbb, dscs[RSA_VERIFY_ARG_SALTLEN], 0);
		if (saltLength < 0 || saltLength > 32)
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
	}

	unsigned keyLen;
	const UCHAR* key = CVT_get_bytes(dscs[RSA_VERIFY_ARG_KEY], keyLen);
	if (!key)
		return boolResult(tdbb, impure, false);
	rsa_key rsaKey;
	tomCheck(rsa_import(key, keyLen, &rsaKey), Arg::Gds(isc_tom_rsa_import));

	int state = 0;
	int cryptRc = rsa_verify_hash(sign, signLen, data, len, hash, saltLength, &state, &rsaKey);
	rsa_free(&rsaKey);
	if (cryptRc != CRYPT_INVALID_PACKET)
		tomCheck(cryptRc, Arg::Gds(isc_tom_rsa_verify));
	else
		state = 0;

	return boolResult(tdbb, impure, state);
}


dsc* evlDateDiff(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 3);

	jrd_req* request = tdbb->getRequest();

	const dsc* partDsc = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if partDsc is NULL
		return NULL;

	const dsc* value1Dsc = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value1Dsc is NULL
		return NULL;

	const dsc* value2Dsc = EVL_expr(tdbb, request, args[2]);
	if (request->req_flags & req_null)	// return NULL if value2Dsc is NULL
		return NULL;

	TimeStamp timestamp1;

	switch (value1Dsc->dsc_dtype)
	{
		case dtype_sql_time:
		case dtype_sql_time_tz:
			timestamp1.value().timestamp_time = *(GDS_TIME*) value1Dsc->dsc_address;
			timestamp1.value().timestamp_date = 0;

			if (value1Dsc->dsc_dtype == dtype_sql_time && value2Dsc->isDateTimeTz())
				TimeZoneUtil::localTimeToUtc(timestamp1.value().timestamp_time, &EngineCallbacks::instance);
			break;

		case dtype_sql_date:
			timestamp1.value().timestamp_date = *(GDS_DATE*) value1Dsc->dsc_address;
			timestamp1.value().timestamp_time = 0;
			break;

		case dtype_timestamp:
		case dtype_timestamp_tz:
			timestamp1.value() = *(GDS_TIMESTAMP*) value1Dsc->dsc_address;

			if (value1Dsc->dsc_dtype == dtype_timestamp && value2Dsc->isDateTimeTz())
				TimeZoneUtil::localTimeStampToUtc(timestamp1.value(), &EngineCallbacks::instance);
			break;

		default:
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_diff_dtime) <<
											Arg::Str(function->name));
			break;
	}

	TimeStamp timestamp2;

	switch (value2Dsc->dsc_dtype)
	{
		case dtype_sql_time:
		case dtype_sql_time_tz:
			timestamp2.value().timestamp_time = *(GDS_TIME*) value2Dsc->dsc_address;
			timestamp2.value().timestamp_date = 0;

			if (value2Dsc->dsc_dtype == dtype_sql_time && value1Dsc->isDateTimeTz())
				TimeZoneUtil::localTimeToUtc(timestamp2.value().timestamp_time, &EngineCallbacks::instance);
			break;

		case dtype_sql_date:
			timestamp2.value().timestamp_date = *(GDS_DATE*) value2Dsc->dsc_address;
			timestamp2.value().timestamp_time = 0;
			break;

		case dtype_timestamp:
		case dtype_timestamp_tz:
			timestamp2.value() = *(GDS_TIMESTAMP*) value2Dsc->dsc_address;

			if (value2Dsc->dsc_dtype == dtype_timestamp && value1Dsc->isDateTimeTz())
				TimeZoneUtil::localTimeStampToUtc(timestamp2.value(), &EngineCallbacks::instance);
			break;

		default:
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_diff_dtime) <<
											Arg::Str(function->name));
			break;
	}

	tm times1, times2;
	timestamp1.decode(&times1);
	timestamp2.decode(&times2);

	const SLONG part = MOV_get_long(tdbb, partDsc, 0);

	switch (part)
	{
		case blr_extract_hour:
			times1.tm_min = 0;
			times2.tm_min = 0;
			// fall through

		case blr_extract_minute:
			times1.tm_sec = 0;
			times2.tm_sec = 0;
			// fall through

		case blr_extract_second:
			timestamp1.encode(&times1);
			timestamp2.encode(&times2);
			break;
	}

	// ASF: throw error if at least one value is "incomplete" from the EXTRACT POV
	switch (part)
	{
		case blr_extract_year:
		case blr_extract_month:
		case blr_extract_day:
		case blr_extract_week:
			if (value1Dsc->isTime() || value2Dsc->isTime())
			{
				status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
											Arg::Gds(isc_sysf_invalid_timediff) <<
												Arg::Str(function->name));
			}
			break;

		case blr_extract_hour:
		case blr_extract_minute:
		case blr_extract_second:
		case blr_extract_millisecond:
			{
				// ASF: also throw error if one value is TIMESTAMP and the other is TIME
				// CVC: Or if one value is DATE and the other is TIME.
				if ((value1Dsc->isTimeStamp() && value2Dsc->isTime()) ||
					(value1Dsc->isTime() && value2Dsc->isTimeStamp()))
				{
					status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
												Arg::Gds(isc_sysf_invalid_tstamptimediff) <<
													Arg::Str(function->name));
				}
				if ((value1Dsc->dsc_dtype == dtype_sql_date && value2Dsc->isTime()) ||
					(value1Dsc->isTime() && value2Dsc->dsc_dtype == dtype_sql_date))
				{
					status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
												Arg::Gds(isc_sysf_invalid_datetimediff) <<
													Arg::Str(function->name));
				}
			}
			break;

		default:
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_diffpart) <<
											Arg::Str(getPartName(part)) <<
											Arg::Str(function->name));
			break;
	}

	SINT64 result = 0;

	switch (part)
	{
		case blr_extract_year:
			result = times2.tm_year - times1.tm_year;
			break;

		case blr_extract_month:
			result = 12 * (times2.tm_year - times1.tm_year);
			result += times2.tm_mon - times1.tm_mon;
			break;

		case blr_extract_day:
			result = timestamp2.value().timestamp_date - timestamp1.value().timestamp_date;
			break;

		case blr_extract_week:
			result = (timestamp2.value().timestamp_date - timestamp1.value().timestamp_date) / 7;
			break;

		// TO DO: detect overflow in the following cases.

		case blr_extract_hour:
			result = SINT64(24) * (timestamp2.value().timestamp_date - timestamp1.value().timestamp_date);
			result += ((SINT64) timestamp2.value().timestamp_time -
				(SINT64) timestamp1.value().timestamp_time) /
				ISC_TIME_SECONDS_PRECISION / 3600;
			break;

		case blr_extract_minute:
			result = SINT64(24) * 60 * (timestamp2.value().timestamp_date - timestamp1.value().timestamp_date);
			result += ((SINT64) timestamp2.value().timestamp_time -
				(SINT64) timestamp1.value().timestamp_time) /
				ISC_TIME_SECONDS_PRECISION / 60;
			break;

		case blr_extract_second:
			result = (SINT64) oneDay *
				(timestamp2.value().timestamp_date - timestamp1.value().timestamp_date);
			result += ((SINT64) timestamp2.value().timestamp_time -
				(SINT64) timestamp1.value().timestamp_time) /
				ISC_TIME_SECONDS_PRECISION;
			break;

		case blr_extract_millisecond:
			result = (SINT64) oneDay *
				(timestamp2.value().timestamp_date - timestamp1.value().timestamp_date) * 1000;
			result += ((SINT64) timestamp2.value().timestamp_time -
				(SINT64) timestamp1.value().timestamp_time) /
				(ISC_TIME_SECONDS_PRECISION / 1000);
			break;

		default:
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_diffpart) <<
											Arg::Str(getPartName(part)) <<
											Arg::Str(function->name));
			break;
	}

	impure->vlu_misc.vlu_int64 = result;
	impure->vlu_desc.makeInt64(0, &impure->vlu_misc.vlu_int64);

	return &impure->vlu_desc;
}


dsc* evlExp(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (value->isDecFloat())
	{
		DecimalStatus decSt = tdbb->getAttachment()->att_dec_status;
		impure->vlu_misc.vlu_dec128 = MOV_get_dec128(tdbb, value);

		Decimal128 e;
		e.set("2.718281828459045235360287471352662497757", decSt);
		impure->vlu_misc.vlu_dec128 = e.pow(decSt, impure->vlu_misc.vlu_dec128);
		impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
	}
	else
	{
		const double rc = exp(MOV_get_double(tdbb, value));
		if (rc == HUGE_VAL) // unlikely to trap anything
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_exception_float_overflow));
		if (isinf(rc))
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_exception_float_overflow));

		impure->vlu_misc.vlu_double = rc;
		impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
	}

	return &impure->vlu_desc;
}


dsc* evlFirstLastDay(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* partDsc = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if partDsc is NULL
		return NULL;

	const dsc* valueDsc = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if valueDsc is NULL
		return NULL;

	TimeStamp timestamp;
	tm times = {0};
	int fractions = 0;

	switch (valueDsc->dsc_dtype)
	{
		case dtype_sql_date:
			timestamp.value().timestamp_date = *(GDS_DATE*) valueDsc->dsc_address;
			timestamp.value().timestamp_time = 0;
			timestamp.decode(&times, &fractions);
			break;

		case dtype_timestamp:
			timestamp.value() = *(GDS_TIMESTAMP*) valueDsc->dsc_address;
			timestamp.decode(&times, &fractions);
			break;

		case dtype_timestamp_tz:
			TimeZoneUtil::decodeTimeStamp(*(ISC_TIMESTAMP_TZ*) valueDsc->dsc_address, &times, &fractions);
			break;

		default:
			status_exception::raise(
				Arg::Gds(isc_expression_eval_err) <<
				Arg::Gds(isc_sysf_invalid_date_timestamp) <<
				Arg::Str(function->name));
			break;
	}

	const SLONG part = MOV_get_long(tdbb, partDsc, 0);

	switch (part)
	{
		case blr_extract_year:
			times.tm_mon = 0;
			// fall through

		case blr_extract_month:
			times.tm_mday = 1;
			break;

		case blr_extract_week:
			break;

		default:
			status_exception::raise(
				Arg::Gds(isc_expression_eval_err) <<
				Arg::Gds(isc_sysf_invalid_first_last_part) <<
				Arg::Str(function->name));
			break;
	}

	const bool last = (Function)(IPTR) function->misc == funLastDay;
	int adjust = 0;

	if (last)
	{
		switch (part)
		{
			case blr_extract_year:
				++times.tm_year;
				adjust = -1;
				break;

			case blr_extract_month:
				if (++times.tm_mon == 12)
				{
					times.tm_mon = 0;
					++times.tm_year;
				}

				adjust = -1;
				break;

			case blr_extract_week:
				adjust = 6 - times.tm_wday;
				break;
		}
	}
	else if (part == blr_extract_week)
		adjust = -times.tm_wday;

	timestamp.encode(&times, fractions);
	timestamp.value().timestamp_date += adjust;

	if (!TimeStamp::isValidTimeStamp(timestamp.value()))
		status_exception::raise(Arg::Gds(isc_datetime_range_exceeded));

	EVL_make_value(tdbb, valueDsc, impure);

	switch (impure->vlu_desc.dsc_dtype)
	{
		case dtype_sql_date:
			impure->vlu_misc.vlu_sql_date = timestamp.value().timestamp_date;
			break;

		case dtype_timestamp:
			impure->vlu_misc.vlu_timestamp = timestamp.value();
			break;

		case dtype_timestamp_tz:
			impure->vlu_misc.vlu_timestamp_tz.utc_timestamp = timestamp.value();
			impure->vlu_misc.vlu_timestamp_tz.time_zone = ((ISC_TIMESTAMP_TZ*) valueDsc->dsc_address)->time_zone;
			TimeZoneUtil::localTimeStampToUtc(impure->vlu_misc.vlu_timestamp_tz);
			break;
	}

	return &impure->vlu_desc;
}


dsc* evlFloor(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	EVL_make_value(tdbb, value, impure);

	switch (impure->vlu_desc.dsc_dtype)
	{
		case dtype_short:
		case dtype_long:
		case dtype_int64:
			{
				SINT64 scale = 1;

				fb_assert(impure->vlu_desc.dsc_scale <= 0);
				for (int i = -impure->vlu_desc.dsc_scale; i > 0; --i)
					scale *= 10;

				const SINT64 v1 = MOV_get_int64(tdbb, &impure->vlu_desc, impure->vlu_desc.dsc_scale);
				const SINT64 v2 = MOV_get_int64(tdbb, &impure->vlu_desc, 0) * scale;

				impure->vlu_misc.vlu_int64 = v1 / scale;

				if (v1 < 0 && v1 != v2)
					--impure->vlu_misc.vlu_int64;

				impure->vlu_desc.makeInt64(0, &impure->vlu_misc.vlu_int64);
			}
			break;

		case dtype_real:
			impure->vlu_misc.vlu_float = floor(impure->vlu_misc.vlu_float);
			break;

		default:
			impure->vlu_misc.vlu_double = MOV_get_double(tdbb, &impure->vlu_desc);
			// fall through

		case dtype_double:
			impure->vlu_misc.vlu_double = floor(impure->vlu_misc.vlu_double);
			impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
			break;

		case dtype_dec64:
			impure->vlu_misc.vlu_dec64 = impure->vlu_misc.vlu_dec64.floor(tdbb->getAttachment()->att_dec_status);
			impure->vlu_desc.makeDecimal64(&impure->vlu_misc.vlu_dec64);
			break;

		case dtype_dec128:
			impure->vlu_misc.vlu_dec128 = impure->vlu_misc.vlu_dec128.floor(tdbb->getAttachment()->att_dec_status);
			impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
			break;
	}

	return &impure->vlu_desc;
}


dsc* evlGenUuid(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 0);

	Guid fbguid;
	fb_assert(sizeof(fbguid.data) == 16);

	GenerateGuid(&fbguid);
	Win32GUID* guid = (Win32GUID*)&fbguid;

	UCHAR data[16];
	data[0] = (guid->data1 >> 24) & 0xFF;
	data[1] = (guid->data1 >> 16) & 0xFF;
	data[2] = (guid->data1 >> 8) & 0xFF;
	data[3] = guid->data1 & 0xFF;
	data[4] = (guid->data2 >> 8) & 0xFF;
	data[5] = guid->data2 & 0xFF;
	data[6] = (guid->data3 >> 8) & 0xFF;
	data[7] = guid->data3 & 0xFF;
	data[8] = guid->data4[0];
	data[9] = guid->data4[1];
	data[10] = guid->data4[2];
	data[11] = guid->data4[3];
	data[12] = guid->data4[4];
	data[13] = guid->data4[5];
	data[14] = guid->data4[6];
	data[15] = guid->data4[7];

	dsc result;
	result.makeText(16, ttype_binary, data);
	EVL_make_value(tdbb, &result, impure);

	return &impure->vlu_desc;
}


dsc* evlGetContext(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	Jrd::Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();
	jrd_tra* transaction = tdbb->getTransaction();
	jrd_req* request = tdbb->getRequest();

	request->req_flags &= ~req_null;
	const dsc* nameSpace = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// Complain if namespace is null
		ERR_post(Arg::Gds(isc_ctx_bad_argument) << Arg::Str(RDB_GET_CONTEXT));

	const dsc* name = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// Complain if variable name is null
		ERR_post(Arg::Gds(isc_ctx_bad_argument) << Arg::Str(RDB_GET_CONTEXT));

	const string nameSpaceStr(MOV_make_string2(tdbb, nameSpace, ttype_none));
	const string nameStr(MOV_make_string2(tdbb, name, ttype_none));

	string resultStr;
	USHORT resultType = ttype_none;
	request->req_flags |= req_null;

	if (nameSpaceStr == SYSTEM_NAMESPACE)	// Handle system variables
	{
		if (nameStr == ENGINE_VERSION)
			resultStr.printf("%s.%s.%s", FB_MAJOR_VER, FB_MINOR_VER, FB_REV_NO);
		else if (nameStr == DATABASE_NAME)
			resultStr = dbb->dbb_database_name.ToString();
		else if (nameStr == SESSION_ID_NAME)
			resultStr.printf("%" SQUADFORMAT, PAG_attachment_id(tdbb));
		else if (nameStr == NETWORK_PROTOCOL_NAME)
		{
			if (attachment->att_network_protocol.isEmpty())
				return NULL;

			resultStr = attachment->att_network_protocol;
		}
		else if (nameStr == WIRE_COMPRESSED_NAME)
		{
			if (attachment->att_network_protocol.isEmpty())
				return NULL;

			resultStr = (attachment->att_remote_flags & isc_dpb_addr_flag_conn_compressed) ? TRUE_VALUE : FALSE_VALUE;
		}
		else if (nameStr == WIRE_ENCRYPTED_NAME)
		{
			if (attachment->att_network_protocol.isEmpty())
				return NULL;

			resultStr = (attachment->att_remote_flags & isc_dpb_addr_flag_conn_encrypted) ? TRUE_VALUE : FALSE_VALUE;
		}
		else if (nameStr == CLIENT_ADDRESS_NAME)
		{
			if (attachment->att_remote_address.isEmpty())
				return NULL;

			resultStr = attachment->att_remote_address;
		}
		else if (nameStr == CLIENT_HOST_NAME)
		{
			if (attachment->att_remote_host.isEmpty())
				return NULL;

			resultStr = attachment->att_remote_host;
		}
		else if (nameStr == CLIENT_PID_NAME)
		{
			if (!attachment->att_remote_pid)
				return NULL;

			resultStr.printf("%" SLONGFORMAT, attachment->att_remote_pid);
		}
		else if (nameStr == CLIENT_PROCESS_NAME)
		{
			if (attachment->att_remote_process.isEmpty())
				return NULL;

			resultStr = attachment->att_remote_process.ToString();
		}
		else if (nameStr == CURRENT_USER_NAME)
		{
			MetaName user;
			if (attachment->att_user)
				user = attachment->att_user->getUserName();

			if (user.isEmpty())
				return NULL;
			resultStr = user.c_str();
		}
		else if (nameStr == CURRENT_ROLE_NAME)
		{
			MetaName role;
			if (attachment->att_user)
				role = attachment->att_user->getSqlRole();

			if (role.isEmpty())
				return NULL;
			resultStr = role.c_str();
		}
		else if (nameStr == SESSION_IDLE_TIMEOUT)
			resultStr.printf("%" ULONGFORMAT, attachment->getIdleTimeout());
		else if (nameStr == STATEMENT_TIMEOUT)
			resultStr.printf("%" ULONGFORMAT, attachment->getStatementTimeout());
		else if (nameStr == TRANSACTION_ID_NAME)
			resultStr.printf("%" SQUADFORMAT, transaction->tra_number);
		else if (nameStr == ISOLATION_LEVEL_NAME)
		{
			if (transaction->tra_flags & TRA_read_committed)
				resultStr = READ_COMMITTED_VALUE;
			else if (transaction->tra_flags & TRA_degree3)
				resultStr = CONSISTENCY_VALUE;
			else
				resultStr = SNAPSHOT_VALUE;
		}
		else if (nameStr == LOCK_TIMEOUT_NAME)
			resultStr.printf("%" SLONGFORMAT, transaction->tra_lock_timeout);
		else if (nameStr == READ_ONLY_NAME)
			resultStr = (transaction->tra_flags & TRA_readonly) ? TRUE_VALUE : FALSE_VALUE;
		else if (nameStr == GLOBAL_CN_NAME)
			resultStr.printf("%" SQUADFORMAT, dbb->dbb_tip_cache->getGlobalCommitNumber());
		else if (nameStr == SNAPSHOT_NUMBER_NAME)
		{
			if (!(transaction->tra_flags & TRA_read_committed))
				resultStr.printf("%" SQUADFORMAT, transaction->tra_snapshot_number);
			else if ((transaction->tra_flags & TRA_read_committed) &&
				(transaction->tra_flags & TRA_read_consistency))
			{
				jrd_req* snapshot_req = request->req_snapshot.m_owner;
				if (snapshot_req)
					resultStr.printf("%" SQUADFORMAT, snapshot_req->req_snapshot.m_number);
				else
					return NULL;
			}
			else
				return NULL;
		}
		else if (nameStr == EXT_CONN_POOL_SIZE)
			resultStr.printf("%d", EDS::Manager::getConnPool()->getMaxCount());
		else if (nameStr == EXT_CONN_POOL_IDLE)
			resultStr.printf("%d", EDS::Manager::getConnPool()->getIdleCount());
		else if (nameStr == EXT_CONN_POOL_ACTIVE)
		{
			EDS::ConnectionsPool* connPool = EDS::Manager::getConnPool();
			resultStr.printf("%d", connPool->getAllCount() - connPool->getIdleCount());
		}
		else if (nameStr == EXT_CONN_POOL_LIFETIME)
			resultStr.printf("%d", EDS::Manager::getConnPool()->getLifeTime());
		else if (nameStr == REPLICATION_SEQ_NAME)
			resultStr.printf("%" UQUADFORMAT, dbb->getReplSequence(tdbb));
		else if (nameStr == EFFECTIVE_USER_NAME)
		{
			MetaName user;
			if (attachment->att_ss_user)
				user = attachment->att_ss_user->getUserName();
			else if (attachment->att_user)
				user = attachment->att_user->getUserName();

			if (user.isEmpty())
				return NULL;
			resultStr = user.c_str();
		}
		else
		{
			// "Context variable %s is not found in namespace %s"
			ERR_post(Arg::Gds(isc_ctx_var_not_found) << Arg::Str(nameStr) <<
														Arg::Str(nameSpaceStr));
		}
	}
	else if (nameSpaceStr == DDL_TRIGGER_NAMESPACE)	// Handle ddl trigger variables
	{
		if (!attachment->ddlTriggersContext.hasData())
			status_exception::raise(Arg::Gds(isc_sysf_invalid_trig_namespace));

		const DdlTriggerContext* context = Stack<DdlTriggerContext*>::const_iterator(
			attachment->ddlTriggersContext).object();

		if (nameStr == EVENT_TYPE_NAME)
			resultStr = context->eventType;
		else if (nameStr == OBJECT_TYPE_NAME)
			resultStr = context->objectType;
		else if (nameStr == DDL_EVENT_NAME)
			resultStr = context->eventType + " " + context->objectType;
		else if (nameStr == OBJECT_NAME)
		{
			resultStr = context->objectName.c_str();
			resultType = ttype_metadata;
		}
		else if (nameStr == OLD_OBJECT_NAME)
		{
			if (context->oldObjectName.isEmpty())
				return NULL;

			resultStr = context->oldObjectName.c_str();
			resultType = ttype_metadata;
		}
		else if (nameStr == NEW_OBJECT_NAME)
		{
			if (context->newObjectName.isEmpty())
				return NULL;

			resultStr = context->newObjectName.c_str();
			resultType = ttype_metadata;
		}
		else if (nameStr == SQL_TEXT_NAME)
		{
			if (context->sqlText.isEmpty())
				return NULL;

			blb* blob = blb::create(tdbb, transaction, &impure->vlu_misc.vlu_bid);
			blob->BLB_put_data(tdbb, reinterpret_cast<const UCHAR*>(context->sqlText.c_str()),
				context->sqlText.length());
			blob->BLB_close(tdbb);

			dsc result;
			result.makeBlob(isc_blob_text, ttype_metadata, (ISC_QUAD*) &impure->vlu_misc.vlu_bid);
			EVL_make_value(tdbb, &result, impure);

			request->req_flags &= ~req_null;
			return &impure->vlu_desc;
		}
		else
		{
			// "Context variable %s is not found in namespace %s"
			ERR_post(Arg::Gds(isc_ctx_var_not_found) << Arg::Str(nameStr) <<
														Arg::Str(nameStr));
		}
	}
	else if (nameSpaceStr == USER_SESSION_NAMESPACE)	// Handle user-defined session variables
	{
		if (!attachment->att_context_vars.get(nameStr, resultStr))
			return NULL;
	}
	else if (nameSpaceStr == USER_TRANSACTION_NAMESPACE)	// Handle user-defined trans. variables
	{
		if (!transaction->tra_context_vars.get(nameStr, resultStr))
			return NULL;
	}
	else
	{
		// "Invalid namespace name %s passed to %s"
		ERR_post(Arg::Gds(isc_ctx_namespace_invalid) <<
			Arg::Str(nameSpaceStr) << Arg::Str(RDB_GET_CONTEXT));
	}

	dsc result;
	result.makeText(resultStr.length(), resultType,
		(UCHAR*) const_cast<char*>(resultStr.c_str()));	// safe const_cast
	EVL_make_value(tdbb, &result, impure);

	request->req_flags &= ~req_null;
	return &impure->vlu_desc;
}


dsc* evlSetContext(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 3);

	Jrd::Attachment* attachment = tdbb->getAttachment();
	jrd_tra* transaction = tdbb->getTransaction();
	jrd_req* request = tdbb->getRequest();

	request->req_flags &= ~req_null;
	const dsc* nameSpace = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// Complain if namespace is null
		ERR_post(Arg::Gds(isc_ctx_bad_argument) << Arg::Str(RDB_GET_CONTEXT));

	const dsc* name = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// Complain if variable name is null
		ERR_post(Arg::Gds(isc_ctx_bad_argument) << Arg::Str(RDB_GET_CONTEXT));

	const dsc* value = EVL_expr(tdbb, request, args[2]);

	const string nameSpaceStr(MOV_make_string2(tdbb, nameSpace, ttype_none));
	const string nameStr(MOV_make_string2(tdbb, name, ttype_none));

	impure->vlu_desc.makeLong(0, &impure->vlu_misc.vlu_long);

	StringMap* contextVars = NULL;

	if (nameSpaceStr == USER_SESSION_NAMESPACE)
	{
		if (!attachment)
		{
			fb_assert(false);
			return 0;
		}

		contextVars = &attachment->att_context_vars;
	}
	else if (nameSpaceStr == USER_TRANSACTION_NAMESPACE)
	{
		if (!transaction)
		{
			fb_assert(false);
			return 0;
		}

		contextVars = &transaction->tra_context_vars;
	}
	else
	{
		// "Invalid namespace name %s passed to %s"
		ERR_post(Arg::Gds(isc_ctx_namespace_invalid) <<
			Arg::Str(nameStr) << Arg::Str(RDB_SET_CONTEXT));
	}

	string valueStr;

	if (!value)
		impure->vlu_misc.vlu_long = (SLONG) contextVars->remove(nameStr);
	else
	{
		valueStr = MOV_make_string2(tdbb, value, ttype_none);

		if (contextVars->count() == MAX_CONTEXT_VARS)
		{
			string* rc = contextVars->get(nameStr);
			if (rc)
			{
				*rc = valueStr;
				impure->vlu_misc.vlu_long = 1;
			}
			else
				ERR_post(Arg::Gds(isc_ctx_too_big)); // "Too many context variables"
		}
		else
		{
			if (contextVars->count() >= MAX_CONTEXT_VARS)
			{
				// "Too many context variables"
				ERR_post(Arg::Gds(isc_ctx_too_big));
			}

			impure->vlu_misc.vlu_long = (SLONG) contextVars->put(nameStr, valueStr);
		}
	}

	if (attachment->att_trace_manager->needs(ITraceFactory::TRACE_EVENT_SET_CONTEXT))
	{
		TraceConnectionImpl conn(attachment);
		TraceTransactionImpl tran(transaction);

		TraceContextVarImpl ctxvar(nameSpaceStr.c_str(), nameStr.c_str(),
			(value ? valueStr.c_str() : NULL));

		attachment->att_trace_manager->event_set_context(&conn, &tran, &ctxvar);
	}

	request->req_flags &= ~req_null;
	return &impure->vlu_desc;
}


dsc* evlGetTranCN(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	Database* dbb = tdbb->getDatabase();
	jrd_req* request = tdbb->getRequest();

	request->req_flags &= ~req_null;
	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)
		return NULL;

	TraNumber traNum = MOV_get_int64(tdbb, value, 0);
	if (traNum > dbb->dbb_next_transaction)
	{
		request->req_flags |= req_null;
		return NULL;
	}

	CommitNumber cn = dbb->dbb_tip_cache->snapshotState(tdbb, traNum);

	dsc result;
	result.makeInt64(0, (SINT64*)&cn);

	EVL_make_value(tdbb, &result, impure);

	request->req_flags &= ~req_null;
	return &impure->vlu_desc;
}


dsc* evlHash(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	AutoPtr<HashContext> hashContext;
	MemoryPool& pool = *request->req_pool;

	if (args.getCount() >= 2)
	{
		const dsc* algorithmDesc = EVL_expr(tdbb, request, args[1]);
		if (request->req_flags & req_null)	// return NULL if algorithm is NULL
			return NULL;

		if (!algorithmDesc->isText())
			status_exception::raise(Arg::Gds(isc_sysf_invalid_hash_algorithm) << "<not a string constant>");

		MetaName algorithmName;
		MOV_get_metaname(tdbb, algorithmDesc, algorithmName);
		hashContext.reset(HashAlgorithmDescriptor::find(algorithmName.c_str())->create(pool));
	}
	else
	{
		hashContext.reset(FB_NEW_POOL(pool) WeakHashContext());
		impure->vlu_misc.vlu_int64 = 0;
	}

	if (value->isBlob())
	{
		UCHAR buffer[BUFFER_LARGE];
		blb* blob = blb::open(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<bid*>(value->dsc_address));

		while (!(blob->blb_flags & BLB_eof))
		{
			const ULONG length = blob->BLB_get_data(tdbb, buffer, sizeof(buffer), false);
			hashContext->update(buffer, length);
		}

		blob->BLB_close(tdbb);
	}
	else
	{
		UCHAR* address;
		MoveBuffer buffer;
		const ULONG length = MOV_make_string2(tdbb, value, value->getTextType(), &address, buffer, false);
		hashContext->update(address, length);
	}

	HashContext::Buffer resultBuffer;
	hashContext->finish(resultBuffer);

	if (args.getCount() >= 2)
	{
		dsc result;
		result.makeText(resultBuffer.getCount(), ttype_binary, resultBuffer.begin());
		EVL_make_value(tdbb, &result, impure);
	}
	else
	{
		fb_assert(resultBuffer.getCount() == sizeof(SINT64));
		memcpy(&impure->vlu_misc.vlu_int64, resultBuffer.begin(), sizeof(SINT64));

		// make descriptor for return value
		impure->vlu_desc.makeInt64(0, &impure->vlu_misc.vlu_int64);
	}

	return &impure->vlu_desc;
}


dsc* evlLeft(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* str = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if str is NULL
		return NULL;

	const dsc* len = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if len is NULL
		return NULL;

	SLONG start = 0;
	dsc startDsc;
	startDsc.makeLong(0, &start);

	return SubstringNode::perform(tdbb, impure, str, &startDsc, len);
}


dsc* evlLnLog10(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);
	fb_assert(function->misc != NULL);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (value->isDecFloat())
	{
		DecimalStatus decSt = tdbb->getAttachment()->att_dec_status;
		Decimal128 d = MOV_get_dec128(tdbb, value);

		if (d.compare(decSt, CDecimal128(0)) <= 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_argmustbe_positive) <<
											Arg::Str(function->name));
		}

		switch ((Function)(IPTR) function->misc)
		{
		case funLnat:
			d = d.ln(decSt);
			break;
		case funLog10:
			d = d.log10(decSt);
			break;
		default:
			fb_assert(0);
			return NULL;
		}

		impure->vlu_misc.vlu_dec128 = d;
		impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
	}
	else
	{
		const double v = MOV_get_double(tdbb, value);

		if (v <= 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_argmustbe_positive) <<
											Arg::Str(function->name));
		}

		double rc;

		switch ((Function)(IPTR) function->misc)
		{
		case funLnat:
			rc = log(v);
			break;
		case funLog10:
			rc = log10(v);
			break;
		default:
			fb_assert(0);
			return NULL;
		}

		impure->vlu_misc.vlu_double = rc;
		impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
	}

	return &impure->vlu_desc;
}


dsc* evlLog(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value[2];
	value[0] = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	value[1] = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (!areParamsDouble(2, value))
	{
		DecimalStatus decSt = tdbb->getAttachment()->att_dec_status;
		Decimal128 v1 = MOV_get_dec128(tdbb, value[0]);
		Decimal128 v2 = MOV_get_dec128(tdbb, value[1]);

		if (v1.compare(decSt, CDecimal128(0)) <= 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_basemustbe_positive) <<
											Arg::Str(function->name));
		}

		if (v2.compare(decSt, CDecimal128(0)) <= 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_argmustbe_positive) <<
											Arg::Str(function->name));
		}

		impure->vlu_misc.vlu_dec128 = v2.ln(decSt).div(decSt, v1.ln(decSt));
		impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
	}
	else
	{
		const double v1 = MOV_get_double(tdbb, value[0]);
		const double v2 = MOV_get_double(tdbb, value[1]);

		if (v1 <= 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_basemustbe_positive) <<
											Arg::Str(function->name));
		}

		if (v2 <= 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_argmustbe_positive) <<
											Arg::Str(function->name));
		}

		impure->vlu_misc.vlu_double = log(v2) / log(v1);
		impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
	}

	return &impure->vlu_desc;
}


dsc* evlQuantize(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value[2];
	value[0] = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	value[1] = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	DecimalStatus decSt = tdbb->getAttachment()->att_dec_status;

	if (value[0]->dsc_dtype == dtype_dec64)
	{
		Decimal64 v1 = MOV_get_dec64(tdbb, value[0]);
		Decimal64 v2 = MOV_get_dec64(tdbb, value[1]);

		impure->vlu_misc.vlu_dec64 = v1.quantize(decSt, v2);
		impure->vlu_desc.makeDecimal64(&impure->vlu_misc.vlu_dec64);
	}
	else
	{
		Decimal128 v1 = MOV_get_dec128(tdbb, value[0]);
		Decimal128 v2 = MOV_get_dec128(tdbb, value[1]);

		impure->vlu_misc.vlu_dec128 = v1.quantize(decSt, v2);
		impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
	}

	return &impure->vlu_desc;
}


dsc* evlCompare(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value[2];
	value[0] = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	value[1] = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (value[0]->dsc_dtype == dtype_dec64)
	{
		Decimal64 v1 = MOV_get_dec64(tdbb, value[0]);
		Decimal64 v2 = MOV_get_dec64(tdbb, value[1]);

		switch ((Function)(IPTR) function->misc)
		{
		case funTotalOrd:
			impure->vlu_misc.vlu_short = v1.totalOrder(v2);
			break;
		case funCmpDec:
			impure->vlu_misc.vlu_short = v1.decCompare(v2);
			break;
		default:
			fb_assert(false);
		}
	}
	else
	{
		Decimal128 v1 = MOV_get_dec128(tdbb, value[0]);
		Decimal128 v2 = MOV_get_dec128(tdbb, value[1]);

		switch ((Function)(IPTR) function->misc)
		{
		case funTotalOrd:
			impure->vlu_misc.vlu_short = v1.totalOrder(v2);
			break;
		case funCmpDec:
			impure->vlu_misc.vlu_short = v1.decCompare(v2);
			break;
		default:
			fb_assert(false);
		}
	}

	impure->vlu_desc.makeShort(0, &impure->vlu_misc.vlu_short);
	return &impure->vlu_desc;
}


dsc* evlNormDec(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value;
	value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	DecimalStatus decSt = tdbb->getAttachment()->att_dec_status;

	if (value->dsc_dtype == dtype_dec64)
	{
		Decimal64 v = MOV_get_dec64(tdbb, value);

		impure->vlu_misc.vlu_dec64 = v.normalize(decSt);
		impure->vlu_desc.makeDecimal64(&impure->vlu_misc.vlu_dec64);
	}
	else
	{
		Decimal128 v = MOV_get_dec128(tdbb, value);

		impure->vlu_misc.vlu_dec128 = v.normalize(decSt);
		impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
	}

	return &impure->vlu_desc;
}


dsc* evlMaxMinValue(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value*)
{
	fb_assert(args.getCount() >= 1);
	fb_assert(function->misc != NULL);

	jrd_req* request = tdbb->getRequest();
	dsc* result = NULL;

	for (FB_SIZE_T i = 0; i < args.getCount(); ++i)
	{
		dsc* value = EVL_expr(tdbb, request, args[i]);
		if (request->req_flags & req_null)	// return NULL if value is NULL
			return NULL;

		if (i == 0)
			result = value;
		else
		{
			switch ((Function)(IPTR) function->misc)
			{
				case funMaxValue:
					if (MOV_compare(tdbb, value, result) > 0)
						result = value;
					break;

				case funMinValue:
					if (MOV_compare(tdbb, value, result) < 0)
						result = value;
					break;

				default:
					fb_assert(false);
			}
		}
	}

	return result;
}


dsc* evlMod(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value1 = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value1 is NULL
		return NULL;

	const dsc* value2 = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value2 is NULL
		return NULL;

	EVL_make_value(tdbb, value1, impure);
	impure->vlu_desc.dsc_scale = 0;

	const SINT64 divisor = MOV_get_int64(tdbb, value2, 0);

	if (divisor == 0)
		status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_exception_integer_divide_by_zero));

	const SINT64 result = MOV_get_int64(tdbb, value1, 0) % divisor;

	switch (impure->vlu_desc.dsc_dtype)
	{
		case dtype_short:
			impure->vlu_misc.vlu_short = (SSHORT) result;
			break;

		case dtype_long:
			impure->vlu_misc.vlu_long = (SLONG) result;
			break;

		case dtype_int64:
			impure->vlu_misc.vlu_int64 = result;
			break;

		default:
			impure->vlu_misc.vlu_int64 = result;
			impure->vlu_desc.makeInt64(0, &impure->vlu_misc.vlu_int64);
			break;
	}

	return &impure->vlu_desc;
}


dsc* evlOverlay(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 3);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	const dsc* placing = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if placing is NULL
		return NULL;

	const dsc* fromDsc = EVL_expr(tdbb, request, args[2]);
	if (request->req_flags & req_null)	// return NULL if fromDsc is NULL
		return NULL;

	const dsc* lengthDsc = NULL;
	ULONG length = 0;

	if (args.getCount() >= 4)
	{
		lengthDsc = EVL_expr(tdbb, request, args[3]);
		if (request->req_flags & req_null)	// return NULL if lengthDsc is NULL
			return NULL;

		const SLONG auxlen = MOV_get_long(tdbb, lengthDsc, 0);

		if (auxlen < 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_argnmustbe_nonneg) <<
											Arg::Num(4) <<
											Arg::Str(function->name));
		}

		length = auxlen;
	}

	SLONG from = MOV_get_long(tdbb, fromDsc, 0);

	if (from <= 0)
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argnmustbe_positive) <<
										Arg::Num(3) <<
										Arg::Str(function->name));
	}

	const USHORT resultTextType = DataTypeUtil::getResultTextType(value, placing);
	CharSet* cs = INTL_charset_lookup(tdbb, resultTextType);

	MoveBuffer temp1;
	UCHAR* str1;
	ULONG len1;

	if (value->isBlob())
	{
		UCharBuffer bpb;
		BLB_gen_bpb_from_descs(value, &impure->vlu_desc, bpb);

		blb* blob = blb::open2(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<bid*>(value->dsc_address), bpb.getCount(), bpb.begin());
		len1 =
			(blob->blb_length / INTL_charset_lookup(tdbb, value->getCharSet())->minBytesPerChar()) *
			cs->maxBytesPerChar();

		str1 = temp1.getBuffer(len1);
		len1 = blob->BLB_get_data(tdbb, str1, len1, true);
	}
	else
		len1 = MOV_make_string2(tdbb, value, resultTextType, &str1, temp1);

	MoveBuffer temp2;
	UCHAR* str2;
	ULONG len2;

	if (placing->isBlob())
	{
		UCharBuffer bpb;
		BLB_gen_bpb_from_descs(placing, &impure->vlu_desc, bpb);

		blb* blob = blb::open2(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<bid*>(placing->dsc_address), bpb.getCount(), bpb.begin());
		len2 =
			(blob->blb_length / INTL_charset_lookup(tdbb, placing->getCharSet())->minBytesPerChar()) *
			cs->maxBytesPerChar();

		str2 = temp2.getBuffer(len2);
		len2 = blob->BLB_get_data(tdbb, str2, len2, true);
	}
	else
		len2 = MOV_make_string2(tdbb, placing, resultTextType, &str2, temp2);

	from = MIN((ULONG) from, len1 + 1);

	if (lengthDsc == NULL)	// not specified
	{
		if (cs->isMultiByte())
			length = cs->length(len2, str2, true);
		else
			length = len2 / cs->maxBytesPerChar();
	}

	length = MIN(length, len1 - from + 1);

	blb* newBlob = NULL;

	if (!value->isBlob() && !placing->isBlob())
	{
		const SINT64 newlen = (SINT64) len1 - length + len2;
		if (newlen > static_cast<SINT64>(MAX_STR_SIZE))
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_imp_exc));

		dsc desc;
		desc.makeText(newlen, resultTextType);
		EVL_make_value(tdbb, &desc, impure);
	}
	else
	{
		EVL_make_value(tdbb, (value->isBlob() ? value : placing), impure);
		impure->vlu_desc.setBlobSubType(DataTypeUtil::getResultBlobSubType(value, placing));
		impure->vlu_desc.setTextType(resultTextType);
		newBlob = blb::create(tdbb, tdbb->getRequest()->req_transaction, &impure->vlu_misc.vlu_bid);
	}

	HalfStaticArray<UCHAR, BUFFER_LARGE> blobBuffer;
	int l1;

	if (newBlob)
	{
		l1 = (from - 1) * cs->maxBytesPerChar();

		if (!cs->isMultiByte())
			newBlob->BLB_put_data(tdbb, str1, l1);
		else
		{
			l1 = cs->substring(len1, str1, l1, blobBuffer.getBuffer(l1), 0, from - 1);

			newBlob->BLB_put_data(tdbb, blobBuffer.begin(), l1);
		}
	}
	else
	{
		l1 = cs->substring(len1, str1, impure->vlu_desc.dsc_length,
			impure->vlu_desc.dsc_address, 0, from - 1);
	}

	int l2;

	if (newBlob)
	{
		newBlob->BLB_put_data(tdbb, str2, len2);

		const ULONG auxlen = len1 - l1;
		if (!cs->isMultiByte())
		{
			newBlob->BLB_put_data(tdbb, str1 + l1 + length * cs->maxBytesPerChar(),
				auxlen - length * cs->maxBytesPerChar());
		}
		else
		{
			l2 = cs->substring(auxlen, str1 + l1, auxlen,
				blobBuffer.getBuffer(auxlen), length, auxlen);
			newBlob->BLB_put_data(tdbb, blobBuffer.begin(), l2);
		}

		newBlob->BLB_close(tdbb);
	}
	else
	{
		memcpy(impure->vlu_desc.dsc_address + l1, str2, len2);
		l2 = cs->substring(len1 - l1, str1 + l1, impure->vlu_desc.dsc_length - len2,
			impure->vlu_desc.dsc_address + l1 + len2, length, len1 - l1);

		impure->vlu_desc.dsc_length = (USHORT) (l1 + len2 + l2);
	}

	return &impure->vlu_desc;
}


dsc* evlPad(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value1 = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value1 is NULL
		return NULL;

	const dsc* padLenDsc = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if padLenDsc is NULL
		return NULL;

	const SLONG padLenArg = MOV_get_long(tdbb, padLenDsc, 0);
	if (padLenArg < 0)
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argnmustbe_nonneg) <<
										Arg::Num(2) <<
										Arg::Str(function->name));
	}

	ULONG padLen = static_cast<ULONG>(padLenArg);

	const dsc* value2 = NULL;
	if (args.getCount() >= 3)
	{
		value2 = EVL_expr(tdbb, request, args[2]);
		if (request->req_flags & req_null)	// return NULL if value2 is NULL
			return NULL;
	}

	const USHORT ttype = value1->getTextType();
	CharSet* cs = INTL_charset_lookup(tdbb, ttype);

	MoveBuffer buffer1;
	UCHAR* address1;
	ULONG length1 = MOV_make_string2(tdbb, value1, ttype, &address1, buffer1, false);
	ULONG charLength1 = cs->length(length1, address1, true);

	MoveBuffer buffer2;
	const UCHAR* address2;
	ULONG length2;

	if (value2 == NULL)
	{
		address2 = cs->getSpace();
		length2 = cs->getSpaceLength();
	}
	else
	{
		UCHAR* address2Temp = NULL;
		length2 = MOV_make_string2(tdbb, value2, ttype, &address2Temp, buffer2, false);
		address2 = address2Temp;
	}

	ULONG charLength2 = cs->length(length2, address2, true);

	blb* newBlob = NULL;

	if (value1->isBlob() || (value2 && value2->isBlob()))
	{
		EVL_make_value(tdbb, (value1->isBlob() ? value1 : value2), impure);
		impure->vlu_desc.setBlobSubType(value1->getBlobSubType());
		impure->vlu_desc.setTextType(ttype);
		newBlob = blb::create(tdbb, tdbb->getRequest()->req_transaction, &impure->vlu_misc.vlu_bid);
	}
	else
	{
		if (padLen * cs->maxBytesPerChar() > MAX_STR_SIZE)
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_imp_exc));

		dsc desc;
		desc.makeText(padLen * cs->maxBytesPerChar(), ttype);
		EVL_make_value(tdbb, &desc, impure);
	}

	MoveBuffer buffer;

	if (charLength1 > padLen)
	{
		if (newBlob)
		{
			buffer.getBuffer(padLen * cs->maxBytesPerChar());
			length1 = cs->substring(length1, address1, buffer.getCapacity(),
				buffer.begin(), 0, padLen);
		}
		else
		{
			length1 = cs->substring(length1, address1, impure->vlu_desc.dsc_length,
				impure->vlu_desc.dsc_address, 0, padLen);
		}
		charLength1 = padLen;
	}

	padLen -= charLength1;

	UCHAR* p = impure->vlu_desc.dsc_address;

	if ((Function)(IPTR) function->misc == funRPad)
	{
		if (newBlob)
			newBlob->BLB_put_data(tdbb, address1, length1);
		else
		{
			memcpy(p, address1, length1);
			p += length1;
		}
	}

	for (; charLength2 > 0 && padLen > 0; padLen -= charLength2)
	{
		if (charLength2 <= padLen)
		{
			if (newBlob)
				newBlob->BLB_put_data(tdbb, address2, length2);
			else
			{
				memcpy(p, address2, length2);
				p += length2;
			}
		}
		else
		{
			if (newBlob)
			{
				buffer.getBuffer(padLen * cs->maxBytesPerChar());
				SLONG len = cs->substring(length2, address2, buffer.getCapacity(),
					buffer.begin(), 0, padLen);
				newBlob->BLB_put_data(tdbb, address2, len);
			}
			else
			{
				p += cs->substring(length2, address2,
					impure->vlu_desc.dsc_length - (p - impure->vlu_desc.dsc_address), p, 0, padLen);
			}

			charLength2 = padLen;
		}
	}

	if ((Function)(IPTR) function->misc == funLPad)
	{
		if (newBlob)
			newBlob->BLB_put_data(tdbb, address1, length1);
		else
		{
			memcpy(p, address1, length1);
			p += length1;
		}
	}

	if (newBlob)
		newBlob->BLB_close(tdbb);
	else
		impure->vlu_desc.dsc_length = p - impure->vlu_desc.dsc_address;

	return &impure->vlu_desc;
}


dsc* evlPi(thread_db* /*tdbb*/, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 0);

	impure->vlu_misc.vlu_double = 3.14159265358979323846;
	impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);

	return &impure->vlu_desc;
}


dsc* evlPosition(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value1 = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value1 is NULL
		return NULL;

	const dsc* value2 = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value2 is NULL
		return NULL;

	SLONG start = 1;

	if (args.getCount() >= 3)
	{
		const dsc* value3 = EVL_expr(tdbb, request, args[2]);
		if (request->req_flags & req_null)	// return NULL if value3 is NULL
			return NULL;

		start = MOV_get_long(tdbb, value3, 0);
		if (start <= 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_argnmustbe_positive) <<
											Arg::Num(3) <<
											Arg::Str(function->name));
		}
	}

	// make descriptor for return value
	impure->vlu_desc.makeLong(0, &impure->vlu_misc.vlu_long);

	// we'll use the collation from the second string
	const USHORT ttype = value2->getTextType();
	TextType* tt = INTL_texttype_lookup(tdbb, ttype);
	CharSet* cs = tt->getCharSet();
	const UCHAR canonicalWidth = tt->getCanonicalWidth();

	MoveBuffer value1Buffer;
	UCHAR* value1Address;
	ULONG value1Length;

	if (value1->isBlob())
	{
		// value1 is a blob
		blb* blob = blb::open(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<bid*>(value1->dsc_address));

		value1Address = value1Buffer.getBuffer(blob->blb_length);
		value1Length = blob->BLB_get_data(tdbb, value1Address, blob->blb_length, true);
	}
	else
		value1Length = MOV_make_string2(tdbb, value1, ttype, &value1Address, value1Buffer);

	HalfStaticArray<UCHAR, BUFFER_SMALL> value1Canonical;
	value1Canonical.getBuffer(value1Length / cs->minBytesPerChar() * canonicalWidth);
	const SLONG value1CanonicalLen = tt->canonical(value1Length, value1Address,
		value1Canonical.getCount(), value1Canonical.begin()) * canonicalWidth;

	// If the first string is empty, we should return the start position accordingly to the SQL2003
	// standard. Using the same logic with our "start" parameter (an extension to the standard),
	// we should return it if it's >= 1 and <= (the other string length + 1). Otherwise, return 0.
	if (value1CanonicalLen == 0 && start == 1)
	{
		impure->vlu_misc.vlu_long = start;
		return &impure->vlu_desc;
	}

	MoveBuffer value2Buffer;
	UCHAR* value2Address;
	ULONG value2Length;

	if (value2->isBlob())
	{
		// value2 is a blob
		blb* blob = blb::open(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<bid*>(value2->dsc_address));

		value2Address = value2Buffer.getBuffer(blob->blb_length);
		value2Length = blob->BLB_get_data(tdbb, value2Address, blob->blb_length, true);
	}
	else
		value2Length = MOV_make_string2(tdbb, value2, ttype, &value2Address, value2Buffer);

	HalfStaticArray<UCHAR, BUFFER_SMALL> value2Canonical;
	value2Canonical.getBuffer(value2Length / cs->minBytesPerChar() * canonicalWidth);
	const SLONG value2CanonicalLen = tt->canonical(value2Length, value2Address,
		value2Canonical.getCount(), value2Canonical.begin()) * canonicalWidth;

	if (value1CanonicalLen == 0)
	{
		impure->vlu_misc.vlu_long = (start <= value2CanonicalLen / canonicalWidth + 1) ? start : 0;
		return &impure->vlu_desc;
	}

	// if the second string is empty, first one is not inside it
	if (value2CanonicalLen == 0)
	{
		impure->vlu_misc.vlu_long = 0;
		return &impure->vlu_desc;
	}

	// search if value1 is inside value2
	const UCHAR* const end = value2Canonical.begin() + value2CanonicalLen;

	for (const UCHAR* p = value2Canonical.begin() + (start - 1) * canonicalWidth;
		 p + value1CanonicalLen <= end;
		 p += canonicalWidth)
	{
		if (memcmp(p, value1Canonical.begin(), value1CanonicalLen) == 0)
		{
			impure->vlu_misc.vlu_long = ((p - value2Canonical.begin()) / canonicalWidth) + 1;
			return &impure->vlu_desc;
		}
	}

	// value1 isn't inside value2
	impure->vlu_misc.vlu_long = 0;
	return &impure->vlu_desc;
}


dsc* evlPower(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value[2];
	value[0] = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	value[1] = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (!areParamsDouble(2, value))
	{
		DecimalStatus decSt = tdbb->getAttachment()->att_dec_status;
		impure->vlu_misc.vlu_dec128 = MOV_get_dec128(tdbb, value[0]);
		Decimal128 v2 = MOV_get_dec128(tdbb, value[1]);

		impure->vlu_misc.vlu_dec128 = impure->vlu_misc.vlu_dec128.pow(decSt, v2);
		impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
	}
	else
	{
		impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);

		const double v1 = MOV_get_double(tdbb, value[0]);
		const double v2 = MOV_get_double(tdbb, value[1]);

		if (v1 == 0 && v2 < 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_zeropowneg) <<
											Arg::Str(function->name));
		}

		if (v1 < 0 &&
			(!value[1]->isExact() ||
			 MOV_get_int64(tdbb, value[1], 0) * SINT64(CVT_power_of_ten(-value[1]->dsc_scale)) !=
				MOV_get_int64(tdbb, value[1], value[1]->dsc_scale)))
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_negpowfp) <<
											Arg::Str(function->name));
		}

		const double rc = pow(v1, v2);
		if (isinf(rc))
			status_exception::raise(Arg::Gds(isc_arith_except) << Arg::Gds(isc_exception_float_overflow));

		impure->vlu_misc.vlu_double = rc;
	}

	return &impure->vlu_desc;
}


dsc* evlRand(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 0);

	SINT64 n;
	tdbb->getAttachment()->att_random_generator.getBytes(&n, sizeof(n));
	n &= QUADCONST(0x7FFFFFFFFFFFFFFF);	// remove the sign

	impure->vlu_misc.vlu_double = (double) n / MAX_SINT64;
	impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);

	return &impure->vlu_desc;
}


dsc* evlReplace(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 3);

	jrd_req* request = tdbb->getRequest();
	dsc* values[3];	// 0 = searched, 1 = find, 2 = replacement
	const dsc* firstBlob = NULL;

	for (int i = 0; i < 3; ++i)
	{
		values[i] = EVL_expr(tdbb, request, args[i]);
		if (request->req_flags & req_null)	// return NULL if values[i] is NULL
			return NULL;

		if (!firstBlob && values[i]->isBlob())
			firstBlob = values[i];
	}

	const USHORT ttype = values[0]->getTextType();
	TextType* tt = INTL_texttype_lookup(tdbb, ttype);
	CharSet* cs = tt->getCharSet();
	const UCHAR canonicalWidth = tt->getCanonicalWidth();

	MoveBuffer buffers[3];
	UCHAR* addresses[3];
	ULONG lengths[3];

	for (int i = 0; i < 3; ++i)
	{
		if (values[i]->isBlob())
		{
			// values[i] is a blob
			blb* blob = blb::open(tdbb, tdbb->getRequest()->req_transaction,
				reinterpret_cast<bid*>(values[i]->dsc_address));

			addresses[i] = buffers[i].getBuffer(blob->blb_length);
			lengths[i] = blob->BLB_get_data(tdbb, addresses[i], blob->blb_length, true);
		}
		else
			lengths[i] = MOV_make_string2(tdbb, values[i], ttype, &addresses[i], buffers[i]);
	}

	if (lengths[1] == 0)
		return values[0];

	HalfStaticArray<UCHAR, BUFFER_SMALL> canonicals[2];	// searched, find
	for (int i = 0; i < 2; ++i)
	{
		canonicals[i].getBuffer(lengths[i] / cs->minBytesPerChar() * canonicalWidth);
		canonicals[i].resize(tt->canonical(lengths[i], addresses[i],
			canonicals[i].getCount(), canonicals[i].begin()) * canonicalWidth);
	}

	blb* newBlob = NULL;

	// make descriptor for return value
	if (!firstBlob)
	{
		const unsigned int searchedLen = canonicals[0].getCount() / canonicalWidth;
		const unsigned int findLen = canonicals[1].getCount() / canonicalWidth;
		const unsigned int replacementLen = lengths[2] / cs->minBytesPerChar();

		const USHORT len = MIN(MAX_STR_SIZE, cs->maxBytesPerChar() *
			MAX(searchedLen, searchedLen + (searchedLen / findLen) * (replacementLen - findLen)));

		dsc desc;
		desc.makeText(len, ttype);
		EVL_make_value(tdbb, &desc, impure);
	}
	else
	{
		EVL_make_value(tdbb, firstBlob, impure);
		impure->vlu_desc.setBlobSubType(values[0]->getBlobSubType());
		impure->vlu_desc.setTextType(ttype);
		newBlob = blb::create(tdbb, tdbb->getRequest()->req_transaction, &impure->vlu_misc.vlu_bid);
	}

	// search 'find' in 'searched'
	bool finished = false;
	const UCHAR* const end = canonicals[0].begin() + canonicals[0].getCount();
	const UCHAR* srcPos = addresses[0];
	UCHAR* dstPos = (newBlob ? NULL : impure->vlu_desc.dsc_address);
	MoveBuffer buffer;
	const UCHAR* last;

	for (const UCHAR* p = last = canonicals[0].begin();
		 !finished || (p + canonicals[1].getCount() <= end);
		 p += canonicalWidth)
	{
		if (p + canonicals[1].getCount() > end)
		{
			finished = true;
			p = canonicals[0].end();
		}

		if (finished || memcmp(p, canonicals[1].begin(), canonicals[1].getCount()) == 0)
		{
			int len;

			if (newBlob)
			{
				len = ((p - last) / canonicalWidth) * cs->maxBytesPerChar();

				if (cs->isMultiByte())
				{
					buffer.getBuffer(len);
					len = cs->substring(addresses[0] + lengths[0] - srcPos, srcPos,
						buffer.getCapacity(), buffer.begin(), 0, (p - last) / canonicalWidth);

					newBlob->BLB_put_data(tdbb, buffer.begin(), len);
				}
				else
					newBlob->BLB_put_data(tdbb, srcPos, len);

				if (!finished)
					newBlob->BLB_put_data(tdbb, addresses[2], lengths[2]);
			}
			else
			{
				len = cs->substring(addresses[0] + lengths[0] - srcPos, srcPos,
					(impure->vlu_desc.dsc_address + impure->vlu_desc.dsc_length) - dstPos, dstPos,
					0, (p - last) / canonicalWidth);

				dstPos += len;

				if (!finished)
				{
					memcpy(dstPos, addresses[2], lengths[2]);
					dstPos += lengths[2];
				}
			}

			if (cs->isMultiByte())
			{
				buffer.getBuffer(canonicals[1].getCount() / canonicalWidth * cs->maxBytesPerChar());
				srcPos += cs->substring(addresses[0] + lengths[0] - srcPos - len, srcPos + len,
					buffer.getCapacity(), buffer.begin(),
					0, canonicals[1].getCount() / canonicalWidth);
			}
			else
				srcPos += lengths[1];

			srcPos += len;
			last = p + canonicals[1].getCount();
			p += canonicals[1].getCount() - canonicalWidth;
		}
	}

	if (newBlob)
		newBlob->BLB_close(tdbb);
	else
		impure->vlu_desc.dsc_length = dstPos - impure->vlu_desc.dsc_address;

	return &impure->vlu_desc;
}


dsc* evlReverse(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	CharSet* cs = INTL_charset_lookup(tdbb, value->getCharSet());

	if (value->isBlob())
	{
		blb* blob = blb::open(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<bid*>(value->dsc_address));

		HalfStaticArray<UCHAR, BUFFER_LARGE> buffer;
		HalfStaticArray<UCHAR, BUFFER_LARGE> buffer2;

		UCHAR* p = buffer.getBuffer(blob->blb_length);
		const SLONG len = blob->BLB_get_data(tdbb, p, blob->blb_length, true);

		if (cs->isMultiByte() || cs->minBytesPerChar() > 1)
		{
			const UCHAR* p1 = p;
			UCHAR* p2 = buffer2.getBuffer(len) + len;
			const UCHAR* const end = p1 + len;
			ULONG size = 0;

			while (p2 > buffer2.begin())
			{
#ifdef DEV_BUILD
				const bool read =
#endif
					IntlUtil::readOneChar(cs, &p1, end, &size);
				fb_assert(read == true);
				memcpy(p2 -= size, p1, size);
			}

			fb_assert(p2 == buffer2.begin());
			p = p2;
		}
		else
		{
			for (UCHAR* p2 = p + len - 1; p2 >= p; p++, p2--)
			{
				const UCHAR c = *p;
				*p = *p2;
				*p2 = c;
			}

			p = buffer.begin();
		}

		EVL_make_value(tdbb, value, impure);

		blb* newBlob = blb::create(tdbb, tdbb->getRequest()->req_transaction,
			&impure->vlu_misc.vlu_bid);
		newBlob->BLB_put_data(tdbb, p, len);
		newBlob->BLB_close(tdbb);
	}
	else
	{
		MoveBuffer temp;
		UCHAR* p;
		const int len = MOV_make_string2(tdbb, value, value->getTextType(), &p, temp);

		dsc desc;
		desc.makeText(len, value->getTextType());
		EVL_make_value(tdbb, &desc, impure);

		UCHAR* p2 = impure->vlu_desc.dsc_address + impure->vlu_desc.dsc_length;

		if (cs->isMultiByte() || cs->minBytesPerChar() > 1)
		{
			const UCHAR* p1 = p;
			const UCHAR* const end = p1 + len;
			ULONG size = 0;

			while (p2 > impure->vlu_desc.dsc_address)
			{
#ifdef DEV_BUILD
				const bool read =
#endif
					IntlUtil::readOneChar(cs, &p1, end, &size);
				fb_assert(read == true);
				memcpy(p2 -= size, p1, size);
			}
			fb_assert(p2 == impure->vlu_desc.dsc_address);
		}
		else
		{
			while (p2 > impure->vlu_desc.dsc_address)
				*--p2 = *p++;
		}
	}

	return &impure->vlu_desc;
}


dsc* evlRight(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 2);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	const dsc* len = EVL_expr(tdbb, request, args[1]);
	if (request->req_flags & req_null)	// return NULL if len is NULL
		return NULL;

	CharSet* charSet = INTL_charset_lookup(tdbb, value->getCharSet());
	SLONG start;

	if (value->isBlob())
	{
		blb* blob = blb::open(tdbb, tdbb->getRequest()->req_transaction,
			reinterpret_cast<bid*>(value->dsc_address));

		if (charSet->isMultiByte())
		{
			HalfStaticArray<UCHAR, BUFFER_LARGE> buffer;
			SLONG length = blob->BLB_get_data(tdbb, buffer.getBuffer(blob->blb_length),
				blob->blb_length, false);
			start = charSet->length(length, buffer.begin(), true);
		}
		else
			start = blob->blb_length / charSet->maxBytesPerChar();

		blob->BLB_close(tdbb);
	}
	else
	{
		MoveBuffer temp;
		UCHAR* p;
		start = MOV_make_string2(tdbb, value, value->getTextType(), &p, temp);
		start = charSet->length(start, p, true);
	}

	start -= MOV_get_long(tdbb, len, 0);
	start = MAX(0, start);

	dsc startDsc;
	startDsc.makeLong(0, &start);

	return SubstringNode::perform(tdbb, impure, value, &startDsc, len);
}


dsc* evlRound(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	SLONG scale = 0;

	if (args.getCount() > 1)
	{
		const dsc* scaleDsc = EVL_expr(tdbb, request, args[1]);
		if (request->req_flags & req_null)	// return NULL if scaleDsc is NULL
			return NULL;

		scale = -MOV_get_long(tdbb, scaleDsc, 0);
		if (!(scale >= MIN_SCHAR && scale <= MAX_SCHAR))
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_scale) <<
											Arg::Str(function->name));
		}
	}

	impure->vlu_misc.vlu_int64 = MOV_get_int64(tdbb, value, scale);
	impure->vlu_desc.makeInt64(scale, &impure->vlu_misc.vlu_int64);

	return &impure->vlu_desc;
}


dsc* evlSign(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (value->isDecFloat())
		impure->vlu_misc.vlu_short = MOV_get_dec128(tdbb, value).sign();
	else
	{
		const double val = MOV_get_double(tdbb, value);

		if (val > 0)
			impure->vlu_misc.vlu_short = 1;
		else if (val < 0)
			impure->vlu_misc.vlu_short = -1;
		else	// val == 0
			impure->vlu_misc.vlu_short = 0;
	}

	impure->vlu_desc.makeShort(0, &impure->vlu_misc.vlu_short);

	return &impure->vlu_desc;
}


dsc* evlSqrt(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (value->isDecFloat())
	{
		DecimalStatus decSt = tdbb->getAttachment()->att_dec_status;
		impure->vlu_misc.vlu_dec128 = MOV_get_dec128(tdbb, value);

		if (impure->vlu_misc.vlu_dec128.compare(decSt, CDecimal128(0)) < 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_nonneg) << Arg::Str(function->name));
		}

		impure->vlu_misc.vlu_dec128 = impure->vlu_misc.vlu_dec128.sqrt(decSt);
		impure->vlu_desc.makeDecimal128(&impure->vlu_misc.vlu_dec128);
	}
	else
	{
		impure->vlu_misc.vlu_double = MOV_get_double(tdbb, value);

		if (impure->vlu_misc.vlu_double < 0)
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_argmustbe_nonneg) << Arg::Str(function->name));
		}

		impure->vlu_misc.vlu_double = sqrt(impure->vlu_misc.vlu_double);
		impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
	}

	return &impure->vlu_desc;
}


dsc* evlTrunc(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() >= 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	SLONG resultScale = 0;
	if (args.getCount() > 1)
	{
		const dsc* scaleDsc = EVL_expr(tdbb, request, args[1]);
		if (request->req_flags & req_null)	// return NULL if scaleDsc is NULL
			return NULL;

		resultScale = -MOV_get_long(tdbb, scaleDsc, 0);
		if (!(resultScale >= MIN_SCHAR && resultScale <= MAX_SCHAR))
		{
			status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
										Arg::Gds(isc_sysf_invalid_scale) <<
											Arg::Str(function->name));
		}
	}

	if (value->isExact())
	{
		SSHORT scale = value->dsc_scale;
		impure->vlu_misc.vlu_int64 = MOV_get_int64(tdbb, value, scale);

		if (resultScale < scale)
			resultScale = scale;

		scale -= resultScale;

		if (scale < 0)
		{
			while (scale)
			{
				impure->vlu_misc.vlu_int64 /= 10;
				++scale;
			}
		}

		impure->vlu_desc.makeInt64(resultScale, &impure->vlu_misc.vlu_int64);
	}
	else
	{
		impure->vlu_misc.vlu_double = MOV_get_double(tdbb, value);

		SINT64 v = 1;

		if (resultScale > 0)
		{
			while (resultScale > 0)
			{
				v *= 10;
				--resultScale;
			}

			impure->vlu_misc.vlu_double /= v;
			modf(impure->vlu_misc.vlu_double, &impure->vlu_misc.vlu_double);
			impure->vlu_misc.vlu_double *= v;
		}
		else
		{
			double r = modf(impure->vlu_misc.vlu_double, &impure->vlu_misc.vlu_double);

			if (resultScale != 0)
			{
				for (SLONG i = 0; i > resultScale; --i)
					v *= 10;

				modf(r * v, &r);
				impure->vlu_misc.vlu_double += r / v;
			}
		}

		impure->vlu_desc.makeDouble(&impure->vlu_misc.vlu_double);
	}

	return &impure->vlu_desc;
}


dsc* evlUuidToChar(thread_db* tdbb, const SysFunction* function, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	if (!value->isText())
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_binuuid_mustbe_str) <<
										Arg::Str(function->name));
	}

	UCHAR* data;
	const USHORT len = MOV_get_string(tdbb, value, &data, NULL, 0);

	if (len != sizeof(Guid))
	{
		status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_sysf_binuuid_wrongsize) <<
										Arg::Num(sizeof(Guid)) <<
										Arg::Str(function->name));
	}

	char buffer[GUID_BUFF_SIZE];
	sprintf(buffer, GUID_NEW_FORMAT,
		USHORT(data[0]), USHORT(data[1]), USHORT(data[2]), USHORT(data[3]), USHORT(data[4]),
		USHORT(data[5]), USHORT(data[6]), USHORT(data[7]), USHORT(data[8]), USHORT(data[9]),
		USHORT(data[10]), USHORT(data[11]), USHORT(data[12]), USHORT(data[13]), USHORT(data[14]),
		USHORT(data[15]));

	dsc result;
	result.makeText(GUID_BODY_SIZE, ttype_ascii, reinterpret_cast<UCHAR*>(buffer) + 1);
	EVL_make_value(tdbb, &result, impure);

	return &impure->vlu_desc;
}

dsc* evlRoleInUse(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();
	Jrd::Attachment* attachment = tdbb->getAttachment();

	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	string roleStr(MOV_make_string2(tdbb, value, ttype_none));

	// sorry - but this breaks role names containing lower case letters
	// roles to be entered as returned by CURRENT_ROLE
	//roleStr.upper();

	impure->vlu_misc.vlu_uchar = (attachment->att_user &&
		attachment->att_user->roleInUse(tdbb, roleStr.c_str())) ? FB_TRUE : FB_FALSE;

	impure->vlu_desc.makeBoolean(&impure->vlu_misc.vlu_uchar);

	return &impure->vlu_desc;
}


dsc* evlSystemPrivilege(thread_db* tdbb, const SysFunction*, const NestValueArray& args,
	impure_value* impure)
{
	fb_assert(args.getCount() == 1);

	jrd_req* request = tdbb->getRequest();
	const dsc* value = EVL_expr(tdbb, request, args[0]);
	if (request->req_flags & req_null)	// return NULL if value is NULL
		return NULL;

	fb_assert(value->dsc_dtype == dtype_short);
	USHORT p = *((USHORT*) value->dsc_address);

	Jrd::Attachment* attachment = tdbb->getAttachment();
	impure->vlu_misc.vlu_uchar = (attachment->att_user &&
		attachment->att_user->locksmith(tdbb, p)) ? FB_TRUE : FB_FALSE;
	impure->vlu_desc.makeBoolean(&impure->vlu_misc.vlu_uchar);

	return &impure->vlu_desc;
}

} // anonymous namespace



const SysFunction SysFunction::functions[] =
	{
		{"ABS", 1, 1, setParamsDblDec, makeAbs, evlAbs, NULL},
		{"ACOS", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfAcos},
		{"ACOSH", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfAcosh},
		{"ASCII_CHAR", 1, 1, setParamsInteger, makeAsciiChar, evlAsciiChar, NULL},
		{"ASCII_VAL", 1, 1, setParamsAsciiVal, makeShortResult, evlAsciiVal, NULL},
		{"ASIN", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfAsin},
		{"ASINH", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfAsinh},
		{"ATAN", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfAtan},
		{"ATANH", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfAtanh},
		{"ATAN2", 2, 2, setParamsDouble, makeDoubleResult, evlAtan2, NULL},
		{"BASE64_DECODE", 1, 1, NULL, makeDecode64, evlDecode64, NULL},
		{"BASE64_ENCODE", 1, 1, NULL, makeEncode64, evlEncode64, NULL},
		{"CRC32", 1, 1, NULL, makeLongResult, evlCrc32, NULL},
		{"BIN_AND", 2, -1, setParamsInteger, makeBin, evlBin, (void*) funBinAnd},
		{"BIN_NOT", 1, 1, setParamsInteger, makeBin, evlBin, (void*) funBinNot},
		{"BIN_OR", 2, -1, setParamsInteger, makeBin, evlBin, (void*) funBinOr},
		{"BIN_SHL", 2, 2, setParamsInteger, makeBinShift, evlBinShift, (void*) funBinShl},
		{"BIN_SHR", 2, 2, setParamsInteger, makeBinShift, evlBinShift, (void*) funBinShr},
		{"BIN_SHL_ROT", 2, 2, setParamsInteger, makeBinShift, evlBinShift, (void*) funBinShlRot},
		{"BIN_SHR_ROT", 2, 2, setParamsInteger, makeBinShift, evlBinShift, (void*) funBinShrRot},
		{"BIN_XOR", 2, -1, setParamsInteger, makeBin, evlBin, (void*) funBinXor},
		{"CEIL", 1, 1, setParamsDblDec, makeCeilFloor, evlCeil, NULL},
		{"CEILING", 1, 1, setParamsDblDec, makeCeilFloor, evlCeil, NULL},
		{"CHAR_TO_UUID", 1, 1, setParamsCharToUuid, makeUuid, evlCharToUuid, NULL},
		{"COMPARE_DECFLOAT", 2, 2, setParamsDecFloat, makeShortResult, evlCompare, (void*) funCmpDec},
		{"COS", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfCos},
		{"COSH", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfCosh},
		{"COT", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfCot},
		{"DATEADD", 3, 3, setParamsDateAdd, makeDateAdd, evlDateAdd, NULL},
		{"DATEDIFF", 3, 3, setParamsDateDiff, makeInt64Result, evlDateDiff, NULL},
		{"DECRYPT", 7, 7, setParamsEncrypt, makeDecrypt, evlDecrypt, NULL},
		{"ENCRYPT", 7, 7, setParamsEncrypt, makeEncrypt, evlEncrypt, NULL},
		{"EXP", 1, 1, setParamsDblDec, makeDblDecResult, evlExp, NULL},
		{"FIRST_DAY", 2, 2, setParamsFirstLastDay, makeFirstLastDayResult, evlFirstLastDay, (void*) funFirstDay},
		{"FLOOR", 1, 1, setParamsDblDec, makeCeilFloor, evlFloor, NULL},
		{"GEN_UUID", 0, 0, NULL, makeUuid, evlGenUuid, NULL},
		{"HASH", 1, 2, NULL, makeHash, evlHash, NULL},
		{"LAST_DAY", 2, 2, setParamsFirstLastDay, makeFirstLastDayResult, evlFirstLastDay, (void*) funLastDay},
		{"LEFT", 2, 2, setParamsSecondInteger, makeLeftRight, evlLeft, NULL},
		{"LN", 1, 1, setParamsDblDec, makeDblDecResult, evlLnLog10, (void*) funLnat},
		{"LOG", 2, 2, setParamsDblDec, makeDblDecResult, evlLog, NULL},
		{"LOG10", 1, 1, setParamsDblDec, makeDblDecResult, evlLnLog10, (void*) funLog10},
		{"LPAD", 2, 3, setParamsSecondInteger, makePad, evlPad, (void*) funLPad},
		{"MAXVALUE", 1, -1, setParamsFromList, makeFromListResult, evlMaxMinValue, (void*) funMaxValue},
		{"MINVALUE", 1, -1, setParamsFromList, makeFromListResult, evlMaxMinValue, (void*) funMinValue},
		{"MOD", 2, 2, setParamsFromList, makeMod, evlMod, NULL},
		{"NORMALIZE_DECFLOAT", 1, 1, setParamsDecFloat, makeDecFloatResult, evlNormDec, NULL},
		{"OVERLAY", 3, 4, setParamsOverlay, makeOverlay, evlOverlay, NULL},
		{"PI", 0, 0, NULL, makePi, evlPi, NULL},
		{"POSITION", 2, 3, setParamsPosition, makeLongResult, evlPosition, NULL},
		{"POWER", 2, 2, setParamsDblDec, makeDblDecResult, evlPower, NULL},
		{"QUANTIZE", 2, 2, setParamsDecFloat, makeDecFloatResult, evlQuantize, NULL},
		{"RAND", 0, 0, NULL, makeDoubleResult, evlRand, NULL},
		{RDB_GET_CONTEXT, 2, 2, setParamsGetSetContext, makeGetSetContext, evlGetContext, NULL},
		{"RDB$GET_TRANSACTION_CN", 1, 1, setParamsInt64, makeGetTranCN, evlGetTranCN, NULL},
		{"RDB$ROLE_IN_USE", 1, 1, setParamsAsciiVal, makeBooleanResult, evlRoleInUse, NULL},
		{RDB_SET_CONTEXT, 3, 3, setParamsGetSetContext, makeGetSetContext, evlSetContext, NULL},
		{"RDB$SYSTEM_PRIVILEGE", 1, 1, NULL, makeBooleanResult, evlSystemPrivilege, NULL},
		{"REPLACE", 3, 3, setParamsFromList, makeReplace, evlReplace, NULL},
		{"REVERSE", 1, 1, NULL, makeReverse, evlReverse, NULL},
		{"RIGHT", 2, 2, setParamsSecondInteger, makeLeftRight, evlRight, NULL},
		{"ROUND", 1, 2, setParamsRoundTrunc, makeRound, evlRound, NULL},
		{"RPAD", 2, 3, setParamsSecondInteger, makePad, evlPad, (void*) funRPad},
		{"RSA_DECRYPT", 4, 4, setParamsRsaEncrypt, makeRsaDecrypt, evlRsaDecrypt, NULL},
		{"RSA_ENCRYPT", 4, 4, setParamsRsaEncrypt, makeRsaEncrypt, evlRsaEncrypt, NULL},
		{"RSA_PRIVATE", 1, 1, setParamsInteger, makeRsaPrivate, evlRsaPrivate, NULL},
		{"RSA_PUBLIC", 1, 1, setParamsRsaPublic, makeRsaPublic, evlRsaPublic, NULL},
		{"RSA_SIGN", 4, 4, setParamsRsaSign, makeRsaSign, evlRsaSign, NULL},
		{"RSA_VERIFY", 5, 5, setParamsRsaVerify, makeBoolResult, evlRsaVerify, NULL},
		{"SIGN", 1, 1, setParamsDblDec, makeShortResult, evlSign, NULL},
		{"SIN", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfSin},
		{"SINH", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfSinh},
		{"SQRT", 1, 1, setParamsDblDec, makeDblDecResult, evlSqrt, NULL},
		{"TAN", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfTan},
		{"TANH", 1, 1, setParamsDouble, makeDoubleResult, evlStdMath, (void*) trfTanh},
		{"TOTALORDER", 2, 2, setParamsDecFloat, makeShortResult, evlCompare, (void*) funTotalOrd},
		{"TRUNC", 1, 2, setParamsRoundTrunc, makeTrunc, evlTrunc, NULL},
		{"UUID_TO_CHAR", 1, 1, setParamsUuidToChar, makeUuidToChar, evlUuidToChar, NULL},
		{"", 0, 0, NULL, NULL, NULL, NULL}
	};


const SysFunction* SysFunction::lookup(const MetaName& name)
{
	for (const SysFunction* f = functions; f->name.length() > 0; ++f)
	{
		if (f->name == name)
			return f;
	}

	return NULL;
}


void SysFunction::checkArgsMismatch(int count) const
{
	if (count < minArgCount || (maxArgCount != -1 && count > maxArgCount))
	{
		status_exception::raise(Arg::Gds(isc_funmismat) << Arg::Str(name.c_str()));
	}
}
