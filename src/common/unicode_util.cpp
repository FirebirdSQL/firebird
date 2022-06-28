/*
 *	PROGRAM:	JRD International support
 *	MODULE:		unicode_util.h
 *	DESCRIPTION:	Unicode functions
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
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../jrd/constants.h"
#include "../common/unicode_util.h"
#include "../common/isc_proto.h"
#include "../common/CharSet.h"
#include "../common/IntlUtil.h"
#include "../common/TimeZoneUtil.h"
#include "../common/gdsassert.h"
#include "../common/classes/auto.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/init.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/rwlock.h"
#include "../common/config/config.h"
#include "../common/StatusHolder.h"
#include "../common/os/path_utils.h"

#include <unicode/ustring.h>
#include <unicode/utrans.h>
#include <unicode/uchar.h>
#include <unicode/ucol.h>
#include <unicode/uversion.h>

#if U_ICU_VERSION_MAJOR_NUM >= 51
#	include <unicode/utf_old.h>
#endif


using namespace Firebird;

namespace {
#if defined(WIN_NT)
const char* const inTemplate = "icuin%s.dll";
const char* const ucTemplate = "icuuc%s.dll";
#elif defined(DARWIN)
const char* const inTemplate = "lib/libicui18n.%s.dylib";
const char* const ucTemplate = "lib/libicuuc.%s.dylib";
#elif defined(HPUX)
const char* const inTemplate = "libicui18n.sl.%s";
const char* const ucTemplate = "libicuuc.sl.%s";
#else
const char* const inTemplate = "libicui18n.so.%s";
const char* const ucTemplate = "libicuuc.so.%s";
#endif

// encapsulate ICU library
struct BaseICU
{
private:
	BaseICU(const BaseICU&);				// not implemented
	BaseICU& operator =(const BaseICU&);	// not implemented

public:
	BaseICU(int aMajorVersion, int aMinorVersion)
		: majorVersion(aMajorVersion),
		  minorVersion(aMinorVersion)
	{
	}

	void initialize(ModuleLoader::Module* module);

	template <typename T> void getEntryPoint(const char* name, ModuleLoader::Module* module, T& ptr,
		bool optional = false)
	{
		// System-wide ICU have no version number at entries names
		if (!majorVersion)
		{
			fb_assert(false);	// ASF: I don't think this code path is correct.

			if (module->findSymbol(NULL, name, ptr))
				return;
		}
		else
		{
			// ICU has several schemas for entries names
			const char* const patterns[] =
			{
				"%s_%d", "%s_%d_%d", "%s_%d%d", "%s"
			};

			string symbol;

			for (auto pattern : patterns)
			{
				symbol.printf(pattern, name, majorVersion, minorVersion);
				if (module->findSymbol(NULL, symbol, ptr))
					return;
			}
		}

		if (!optional)
			(Arg::Gds(isc_icu_entrypoint) << name).raise();
	}

	int majorVersion;
	int minorVersion;
};

void BaseICU::initialize(ModuleLoader::Module* module)
{
	void (U_EXPORT2 *uInit)(UErrorCode* status);
	void (U_EXPORT2 *uSetTimeZoneFilesDirectory)(const char* path, UErrorCode* status);
	void (U_EXPORT2 *uSetDataDirectory)(const char* directory);

	getEntryPoint("u_init", module, uInit, true);
	getEntryPoint("u_setTimeZoneFilesDirectory", module, uSetTimeZoneFilesDirectory, true);
	getEntryPoint("u_setDataDirectory", module, uSetDataDirectory, true);

#if defined(WIN_NT) || defined(DARWIN) || defined(ANDROID)
	if (uSetDataDirectory)
	{
		// call uSetDataDirectory only if .dat file is exists at same folder
		// as the loaded module

		PathName path, file, fullName;
		PathUtils::splitLastComponent(path, file, module->fileName);

#ifdef WIN_NT
		// icuucXX.dll -> icudtXX.dll
		file.replace(3, 2, "dt");

		// icudtXX.dll -> icudtXXl.dat
		const FB_SIZE_T pos = file.find_last_of('.');
		file.erase(pos);
		file.append("l.dat");
#else
		// libicuuc.so.XX -> icudtXX
		const FB_SIZE_T pos = file.find_last_of('.');
		if (pos > 0 && pos != file.npos)
		{
			file.replace(0, pos + 1, "icudt");
		}

		// icudtXX -> icudtXXl.dat
		file += "l.dat";
#endif

		PathUtils::concatPath(fullName, path, file);

		if (PathUtils::canAccess(fullName, 0))
			uSetDataDirectory(path.c_str());
	}
#endif

	if (uInit)
	{
		UErrorCode status = U_ZERO_ERROR;
		uInit(&status);
		if (status != U_ZERO_ERROR)
		{
			string diag;
			diag.printf("u_init() error %d", status);
			(Arg::Gds(isc_random) << diag).raise();
		}
	}

	// ICU's u_setTimeZoneFilesDirectory is an internal API, but we try to use
	// it because internally set ICU_TIMEZONE_FILES_DIR envvar in Windows is not
	// safe. See comments in fb_utils::setenv.
	if (uSetTimeZoneFilesDirectory && TimeZoneUtil::getTzDataPath().hasData())
	{
		UErrorCode status = U_ZERO_ERROR;
		uSetTimeZoneFilesDirectory(TimeZoneUtil::getTzDataPath().c_str(), &status);
	}
}

}

namespace Jrd {

static ModuleLoader::Module* formatAndLoad(const char* templateName,
	int& majorVersion, int& minorVersion);


// encapsulate ICU collations libraries
struct UnicodeUtil::ICU : public BaseICU
{
public:
	ICU(int aMajorVersion, int aMinorVersion)
		: BaseICU(aMajorVersion, aMinorVersion),
		  inModule(NULL),
		  ucModule(NULL),
		  ciAiTransCache(*getDefaultMemoryPool())
	{
	}

	~ICU()
	{
		while (ciAiTransCache.hasData())
			utransClose(ciAiTransCache.pop());

		delete ucModule;
		delete inModule;
	}

	UTransliterator* getCiAiTransliterator()
	{
		ciAiTransCacheMutex.enter(FB_FUNCTION);
		UTransliterator* ret;

		if (!ciAiTransCache.isEmpty())
		{
			ret = ciAiTransCache.pop();
			ciAiTransCacheMutex.leave();
		}
		else
		{
			ciAiTransCacheMutex.leave();

			// Fix for CORE-4136. Was "Any-Upper; NFD; [:Nonspacing Mark:] Remove; NFC".
			// Also see CORE-4739.
			static const auto RULE = (const UChar*)
				u"::NFD; ::[:Nonspacing Mark:] Remove; ::NFC;"
				" \\u00d0 > D;"		// LATIN CAPITAL LETTER ETH' (U+00D0), iceland
				" \\u00d8 > O;"		// LATIN CAPITAL LETTER O WITH STROKE' (U+00D8), used in danish & iceland alphabets;
				" \\u013f > L;"		// LATIN CAPITAL LETTER L WITH MIDDLE DOT' (U+013F), catalone (valencian)
				" \\u0141 > L;";	// LATIN CAPITAL LETTER L WITH STROKE' (U+0141), polish

			UErrorCode errorCode = U_ZERO_ERROR;
			ret = utransOpenU((const UChar*) u"FbNormalizer", -1, UTRANS_FORWARD, RULE, -1, NULL, &errorCode);
		}

		return ret;
	}

	void releaseCiAiTransliterator(UTransliterator* trans)
	{
		MutexLockGuard guard(ciAiTransCacheMutex, FB_FUNCTION);
		ciAiTransCache.push(trans);
	}

	ModuleLoader::Module* inModule;
	ModuleLoader::Module* ucModule;
	UVersionInfo collVersion;
	Mutex ciAiTransCacheMutex;
	Array<UTransliterator*> ciAiTransCache;

	void (U_EXPORT2 *uVersionToString)(UVersionInfo versionArray, char* versionString);

	int32_t (U_EXPORT2 *ulocCountAvailable)();
	const char* (U_EXPORT2 *ulocGetAvailable)(int32_t n);

	void (U_EXPORT2 *usetClose)(USet* set);
	int32_t (U_EXPORT2 *usetGetItem)(const USet* set, int32_t itemIndex,
		UChar32* start, UChar32* end, UChar* str, int32_t strCapacity, UErrorCode* ec);
	int32_t (U_EXPORT2 *usetGetItemCount)(const USet* set);
	USet* (U_EXPORT2 *usetOpen)(UChar32 start, UChar32 end);

	void (U_EXPORT2 *ucolClose)(UCollator* coll);
	int32_t (U_EXPORT2 *ucolGetContractionsAndExpansions)(const UCollator* coll, USet* contractions, USet* expansions,
		UBool addPrefixes, UErrorCode* status);
	const UChar* (U_EXPORT2 *ucolGetRules)(const UCollator* coll, int32_t* length);

	int32_t (U_EXPORT2 *ucolGetSortKey)(const UCollator* coll, const UChar* source,
		int32_t sourceLength, uint8_t* result, int32_t resultLength);
	UCollator* (U_EXPORT2 *ucolOpen)(const char* loc, UErrorCode* status);
	UCollator* (U_EXPORT2 *ucolOpenRules)(const UChar* rules, int32_t rulesLength, UColAttributeValue normalizationMode,
		UCollationStrength strength, UParseError* parseError, UErrorCode* status);
	void (U_EXPORT2 *ucolSetAttribute)(UCollator* coll, UColAttribute attr,
		UColAttributeValue value, UErrorCode* status);
	UCollationResult (U_EXPORT2 *ucolStrColl)(const UCollator* coll, const UChar* source,
		int32_t sourceLength, const UChar* target, int32_t targetLength);
	void (U_EXPORT2 *ucolGetVersion)(const UCollator* coll, UVersionInfo info);

	void (U_EXPORT2 *utransClose)(UTransliterator* trans);
	UTransliterator* (U_EXPORT2 *utransOpenU)(
		const UChar* id,
		int32_t idLength,
		UTransDirection dir,
		const UChar* rules,         /* may be Null */
		int32_t rulesLength,        /* -1 if null-terminated */
		UParseError* parseError,    /* may be Null */
		UErrorCode* status);
	void (U_EXPORT2 *utransTransUChars)(
		const UTransliterator* trans,
		UChar* text,
		int32_t* textLength,
		int32_t textCapacity,
		int32_t start,
		int32_t* limit,
		UErrorCode* status);
};


// encapsulate ICU conversion library
class ImplementConversionICU : public UnicodeUtil::ConversionICU, BaseICU
{
private:
	ImplementConversionICU(int aMajorVersion, int aMinorVersion)
		: BaseICU(aMajorVersion, aMinorVersion)
	{
		module = formatAndLoad(ucTemplate, this->majorVersion, this->minorVersion);
		if (!module)
			return;

		initialize(module);

		getEntryPoint("ucnv_open", module, ucnv_open);
		getEntryPoint("ucnv_close", module, ucnv_close);
		getEntryPoint("ucnv_fromUChars", module, ucnv_fromUChars);
		getEntryPoint("u_getVersion", module, u_getVersion);
		getEntryPoint("u_tolower", module, u_tolower);
		getEntryPoint("u_toupper", module, u_toupper);
		getEntryPoint("u_strCompare", module, u_strCompare);
		getEntryPoint("u_countChar32", module, u_countChar32);
		getEntryPoint("utf8_nextCharSafeBody", module, utf8_nextCharSafeBody);

		getEntryPoint("UCNV_FROM_U_CALLBACK_STOP", module, UCNV_FROM_U_CALLBACK_STOP);
		getEntryPoint("UCNV_TO_U_CALLBACK_STOP", module, UCNV_TO_U_CALLBACK_STOP);
		getEntryPoint("ucnv_fromUnicode", module, ucnv_fromUnicode);
		getEntryPoint("ucnv_toUnicode", module, ucnv_toUnicode);
		getEntryPoint("ucnv_getInvalidChars", module, ucnv_getInvalidChars);
		getEntryPoint("ucnv_getMaxCharSize", module, ucnv_getMaxCharSize);
		getEntryPoint("ucnv_getMinCharSize", module, ucnv_getMinCharSize);
		getEntryPoint("ucnv_setFromUCallBack", module, ucnv_setFromUCallBack);
		getEntryPoint("ucnv_setToUCallBack", module, ucnv_setToUCallBack);

		getEntryPoint("u_strcmp", module, ustrcmp);

		inModule = formatAndLoad(inTemplate, aMajorVersion, aMinorVersion);
		if (!inModule)
			return;

		if (aMajorVersion != this->majorVersion || aMinorVersion != this->minorVersion)
		{
			string err;
			err.printf("Wrong version of IN icu module: loaded %d.%d, expected %d.%d",
						aMajorVersion, aMinorVersion, this->majorVersion, this->minorVersion);

			(Arg::Gds(isc_random) << Arg::Str(err)).raise();
		}

		getEntryPoint("ucal_getTZDataVersion", inModule, ucalGetTZDataVersion);
		getEntryPoint("ucal_getDefaultTimeZone", inModule, ucalGetDefaultTimeZone);
		getEntryPoint("ucal_open", inModule, ucalOpen);
		getEntryPoint("ucal_close", inModule, ucalClose);
		getEntryPoint("ucal_setAttribute", inModule, ucalSetAttribute);
		getEntryPoint("ucal_setMillis", inModule, ucalSetMillis);
		getEntryPoint("ucal_get", inModule, ucalGet);
		getEntryPoint("ucal_setDateTime", inModule, ucalSetDateTime);

		getEntryPoint("ucal_getNow", inModule, ucalGetNow);
		getEntryPoint("ucal_getTimeZoneTransitionDate", inModule, ucalGetTimeZoneTransitionDate);
	}

public:
	static ImplementConversionICU* create(int majorVersion, int minorVersion)
	{
		ImplementConversionICU* o = FB_NEW_POOL(*getDefaultMemoryPool()) ImplementConversionICU(
			majorVersion, minorVersion);

		if (!o->module)
		{
			delete o;
			o = NULL;
		}

		if (o)
		{
			UVersionInfo versionInfo;
			o->u_getVersion(versionInfo);

			o->vMajor = versionInfo[0];
			o->vMinor = versionInfo[1];
		}

		return o;
	}

private:
	AutoPtr<ModuleLoader::Module> module;
	AutoPtr<ModuleLoader::Module> inModule;
};

static ImplementConversionICU* convIcu = NULL;
static GlobalPtr<Mutex> convIcuMutex;


// cache ICU module instances to not load and unload many times
class UnicodeUtil::ICUModules
{
	typedef GenericMap<Pair<Left<string, ICU*> > > ModulesMap;

public:
	explicit ICUModules(MemoryPool& p)
		: modules(p)
	{
	}

	~ICUModules()
	{
		ModulesMap::Accessor modulesAccessor(&modules);
		for (bool found = modulesAccessor.getFirst(); found; found = modulesAccessor.getNext())
			delete modulesAccessor.current()->second;
	}

	ModulesMap modules;
	RWLock lock;
};


static const char* const COLL_30_VERSION = "41.128.4.4";	// ICU 3.0 collator version

static GlobalPtr<UnicodeUtil::ICUModules> icuModules;

#ifdef LINUX
static bool extractVersionFromPath(const PathName& realPath, int& major, int& minor)
{
	major = 0;
	minor = 0;
	int mult = 1;

	const FB_SIZE_T len = realPath.length();
	const char* buf = realPath.begin();

	bool dot = false;
	for (const char* p = buf + len - 1; p >= buf; p--)
	{
		if (*p >= '0' && *p < '9')
		{
			major += (*p - '0') * mult;
			mult *= 10;
		}
		else if (*p == '.' && !dot)
		{
			dot = true;
			minor = major;
			major = 0;
			mult = 1;
		}
		else
		{
			break;
		}
	}

	if (minor && !major)
	{
		major = minor;
		minor = 0;
	}

	return major != 0;
}
#endif

static ModuleLoader::Module* formatAndLoad(const char* templateName,
	int& majorVersion, int& minorVersion)
{
#ifdef ANDROID
	static ModuleLoader::Module* dat = ModuleLoader::loadModule(NULL,
		fb_utils::getPrefix(Firebird::IConfigManager::DIR_LIB, "libicudata.so"));

	Firebird::PathName newName = fb_utils::getPrefix(Firebird::IConfigManager::DIR_LIB, templateName);
	templateName = newName.c_str();
#endif

	ModuleLoader::Module* module = nullptr;

	// System-wide ICU have no version number at file names
	if (!majorVersion)
	{
		PathName filename;
		filename.printf(templateName, "");
		filename.rtrim(".");

		//gds__log("ICU: link %s", filename.c_str());

		module = ModuleLoader::fixAndLoadModule(NULL, filename);

#ifdef LINUX
		// try to resolve symlinks and extract version numbers from suffix
		PathName realPath;
		if (module && module->getRealPath(realPath))
		{
			//gds__log("ICU: module name %s, real path %s", module->fileName.c_str(), realPath.c_str());

			int major, minor;
			if (extractVersionFromPath(realPath, major, minor))
			{
				//gds__log("ICU: extracted version %d.%d", major, minor);
				majorVersion = major;
				minorVersion = minor;
			}
		}
#endif
	}
	else
	{
		// ICU has several schemas for placing version into file name
		const char* const patterns[] =
		{
#ifdef WIN_NT
			"%d",
#endif
			"%d_%d",
			"%d.%d",
			"%d%d"
		};

		PathName s, filename;
		for (auto pattern : patterns)
		{
			s.printf(pattern, majorVersion, minorVersion);
			filename.printf(templateName, s.c_str());

			module = ModuleLoader::fixAndLoadModule(NULL, filename);
			if (module)
				break;
		}

#ifndef WIN_NT
		// There is no sence to try pattern "%d" for different minor versions
		// ASF: In Windows ICU 63.1 libraries use 63.dll suffix. This is handled in 'patterns' above.
		if (!module && minorVersion == 0)
		{
			s.printf("%d", majorVersion);
			filename.printf(templateName, s.c_str());

			module = ModuleLoader::fixAndLoadModule(NULL, filename);
		}
#endif
	}

	return module;
}


static void getVersions(const string& configInfo, ObjectsArray<string>& versions)
{
	charset cs;
	IntlUtil::initAsciiCharset(&cs);

	AutoPtr<CharSet> ascii(Jrd::CharSet::createInstance(*getDefaultMemoryPool(), 0, &cs));

	IntlUtil::SpecificAttributesMap config;
	IntlUtil::parseSpecificAttributes(ascii, configInfo.length(),
		(const UCHAR*) configInfo.c_str(), &config);

	string versionsStr;
	if (config.get("icu_versions", versionsStr))
		versionsStr.trim();
	else
		versionsStr = "default";

	versions.clear();

	FB_SIZE_T start = 0;
	FB_SIZE_T n;

	for (FB_SIZE_T i = versionsStr.find(' '); i != versionsStr.npos;
		start = i + 1, i = versionsStr.find(' ', start))
	{
		if ((n = versionsStr.find_first_not_of(' ', start)) != versionsStr.npos)
			start = n;
		versions.add(versionsStr.substr(start, i - start));
	}

	if ((n = versionsStr.find_first_not_of(' ', start)) != versionsStr.npos)
		start = n;
	versions.add(versionsStr.substr(start));
}


// BOCU-1
USHORT UnicodeUtil::utf16KeyLength(USHORT len)
{
	return (len / 2) * 4;
}


// BOCU-1
USHORT UnicodeUtil::utf16ToKey(USHORT srcLen, const USHORT* src, USHORT dstLen, UCHAR* dst)
{
	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL && dst != NULL);

	if (dstLen < srcLen / sizeof(*src) * 4)
		return INTL_BAD_KEY_LENGTH;

	UErrorCode status = U_ZERO_ERROR;
	ConversionICU& cIcu(getConversionICU());
	UConverter* conv = cIcu.ucnv_open("BOCU-1", &status);
	fb_assert(U_SUCCESS(status));

	const int32_t len = cIcu.ucnv_fromUChars(conv, reinterpret_cast<char*>(dst), dstLen,
		// safe cast - alignment not changed
		reinterpret_cast<const UChar*>(src), srcLen / sizeof(*src), &status);
	fb_assert(U_SUCCESS(status));

	cIcu.ucnv_close(conv);

	return len;
}


ULONG UnicodeUtil::utf16LowerCase(ULONG srcLen, const USHORT* src, ULONG dstLen, USHORT* dst,
	const ULONG* exceptions)
{
	// this is more correct but we don't support completely yet
	/***
	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL && dst != NULL);

	memcpy(dst, src, srcLen);

	UErrorCode errorCode = U_ZERO_ERROR;
	UTransliterator* trans = utrans_open("Any-Lower", UTRANS_FORWARD, NULL, 0, NULL, &errorCode);
	//// TODO: add exceptions in this way: Any-Lower[^\\u03BC] - for U+03BC

	if (errorCode <= 0)
	{
		int32_t capacity = dstLen;
		int32_t len = srcLen / sizeof(USHORT);
		int32_t limit = len;

		utrans_transUChars(trans, reinterpret_cast<UChar*>(dst), &len, capacity, 0, &limit, &errorCode);
		utrans_close(trans);

		len *= sizeof(USHORT);
		if (len > dstLen)
			len = INTL_BAD_STR_LENGTH;

		return len;
	}
	else
		return INTL_BAD_STR_LENGTH;
	***/

	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL && dst != NULL);

	srcLen /= sizeof(*src);
	dstLen /= sizeof(*dst);

	ULONG n = 0;
	ConversionICU& cIcu(getConversionICU());

	for (ULONG i = 0; i < srcLen;)
	{
		uint32_t c;
		U16_NEXT(src, i, srcLen, c);

		if (!exceptions)
			c = cIcu.u_tolower(c);
		else
		{
			const ULONG* p = exceptions;
			while (*p && *p != c)
				++p;

			if (*p == 0)
				c = cIcu.u_tolower(c);
		}

		bool error;
		U16_APPEND(dst, n, dstLen, c, error);
		(void) error;
	}

	return n * sizeof(*dst);
}


ULONG UnicodeUtil::utf16UpperCase(ULONG srcLen, const USHORT* src, ULONG dstLen, USHORT* dst,
	const ULONG* exceptions)
{
	// this is more correct but we don't support completely yet
	/***
	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL && dst != NULL);

	memcpy(dst, src, srcLen);

	UErrorCode errorCode = U_ZERO_ERROR;
	UTransliterator* trans = utrans_open("Any-Upper", UTRANS_FORWARD, NULL, 0, NULL, &errorCode);
	//// TODO: add exceptions in this way: Any-Upper[^\\u03BC] - for U+03BC

	if (errorCode <= 0)
	{
		int32_t capacity = dstLen;
		int32_t len = srcLen / sizeof(USHORT);
		int32_t limit = len;

		utrans_transUChars(trans, reinterpret_cast<UChar*>(dst), &len, capacity, 0, &limit, &errorCode);
		utrans_close(trans);

		len *= sizeof(USHORT);
		if (len > dstLen)
			len = INTL_BAD_STR_LENGTH;

		return len;
	}
	else
		return INTL_BAD_STR_LENGTH;
	***/

	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL && dst != NULL);

	srcLen /= sizeof(*src);
	dstLen /= sizeof(*dst);

	ULONG n = 0;
	ConversionICU& cIcu(getConversionICU());

	for (ULONG i = 0; i < srcLen;)
	{
		uint32_t c;
		U16_NEXT(src, i, srcLen, c);

		if (!exceptions)
			c = cIcu.u_toupper(c);
		else
		{
			const ULONG* p = exceptions;
			while (*p && *p != c)
				++p;

			if (*p == 0)
				c = cIcu.u_toupper(c);
		}

		bool error;
		U16_APPEND(dst, n, dstLen, c, error);
		(void) error;
	}

	return n * sizeof(*dst);
}


ULONG UnicodeUtil::utf16ToUtf8(ULONG srcLen, const USHORT* src, ULONG dstLen, UCHAR* dst,
							   USHORT* err_code, ULONG* err_position)
{
	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL || dst == NULL);
	fb_assert(err_code != NULL);
	fb_assert(err_position != NULL);

	*err_code = 0;

	if (dst == NULL)
		return srcLen / sizeof(*src) * 4;

	srcLen /= sizeof(*src);

	const UCHAR* const dstStart = dst;
	const UCHAR* const dstEnd = dst + dstLen;

	for (ULONG i = 0; i < srcLen; )
	{
		if (dstEnd - dst == 0)
		{
			*err_code = CS_TRUNCATION_ERROR;
			*err_position = i * sizeof(*src);
			break;
		}

		UChar32 c = src[i++];

		if (c <= 0x7F)
			*dst++ = c;
		else
		{
			*err_position = (i - 1) * sizeof(*src);

			if (UTF_IS_SURROGATE(c))
			{
				UChar32 c2;

				if (UTF_IS_SURROGATE_FIRST(c) && i < srcLen && UTF_IS_TRAIL(c2 = src[i]))
				{
					++i;
					c = UTF16_GET_PAIR_VALUE(c, c2);
				}
				else
				{
					*err_code = CS_BAD_INPUT;
					break;
				}
			}

			if (U8_LENGTH(c) <= dstEnd - dst)
			{
				int j = 0;
				U8_APPEND_UNSAFE(dst, j, c);
				dst += j;
			}
			else
			{
				*err_code = CS_TRUNCATION_ERROR;
				break;
			}
		}
	}

	return static_cast<ULONG>((dst - dstStart) * sizeof(*dst));
}


ULONG UnicodeUtil::utf8ToUtf16(ULONG srcLen, const UCHAR* src, ULONG dstLen, USHORT* dst,
							   USHORT* err_code, ULONG* err_position)
{
	fb_assert(src != NULL || dst == NULL);
	fb_assert(err_code != NULL);
	fb_assert(err_position != NULL);

	*err_code = 0;

	if (dst == NULL)
		return srcLen * sizeof(*dst);

	const USHORT* const dstStart = dst;
	const USHORT* const dstEnd = dst + dstLen / sizeof(*dst);
	ConversionICU& cIcu(getConversionICU());

	for (ULONG i = 0; i < srcLen; )
	{
		if (dstEnd - dst == 0)
		{
			*err_code = CS_TRUNCATION_ERROR;
			*err_position = i;
			break;
		}

		UChar32 c = src[i++];

		if (c <= 0x7F)
			*dst++ = c;
		else
		{
			*err_position = i - 1;

			c = cIcu.utf8_nextCharSafeBody(src, reinterpret_cast<int32_t*>(&i), srcLen, c, -1);

			if (c < 0)
			{
				*err_code = CS_BAD_INPUT;
				break;
			}
			else if (c <= 0xFFFF)
				*dst++ = c;
			else
			{
				if (dstEnd - dst > 1)
				{
					*dst++ = UTF16_LEAD(c);
					*dst++ = UTF16_TRAIL(c);
				}
				else
				{
					*err_code = CS_TRUNCATION_ERROR;
					break;
				}
			}
		}
	}

	return static_cast<ULONG>((dst - dstStart) * sizeof(*dst));
}


ULONG UnicodeUtil::utf16ToUtf32(ULONG srcLen, const USHORT* src, ULONG dstLen, ULONG* dst,
								USHORT* err_code, ULONG* err_position)
{
	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL || dst == NULL);
	fb_assert(err_code != NULL);
	fb_assert(err_position != NULL);

	*err_code = 0;

	if (dst == NULL)
		return srcLen / sizeof(*src) * sizeof(*dst);

	// based on u_strToUTF32 from ICU
	const USHORT* const srcStart = src;
	const ULONG* const dstStart = dst;
	const USHORT* const srcEnd = src + srcLen / sizeof(*src);
	const ULONG* const dstEnd = dst + dstLen / sizeof(*dst);

	while (src < srcEnd && dst < dstEnd)
	{
		ULONG ch = *src++;

		if (UTF_IS_LEAD(ch))
		{
			ULONG ch2;
			if (src < srcEnd && UTF_IS_TRAIL(ch2 = *src))
			{
				ch = UTF16_GET_PAIR_VALUE(ch, ch2);
				++src;
			}
			else
			{
				*err_code = CS_BAD_INPUT;
				--src;
				break;
			}
		}

		*(dst++) = ch;
	}

	*err_position = static_cast<ULONG>((src - srcStart) * sizeof(*src));

	if (*err_code == 0 && src < srcEnd)
		*err_code = CS_TRUNCATION_ERROR;

	return static_cast<ULONG>((dst - dstStart) * sizeof(*dst));
}


ULONG UnicodeUtil::utf32ToUtf16(ULONG srcLen, const ULONG* src, ULONG dstLen, USHORT* dst,
								USHORT* err_code, ULONG* err_position)
{
	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL || dst == NULL);
	fb_assert(err_code != NULL);
	fb_assert(err_position != NULL);

	*err_code = 0;

	if (dst == NULL)
		return srcLen;

	// based on u_strFromUTF32 from ICU
	const ULONG* const srcStart = src;
	const USHORT* const dstStart = dst;
	const ULONG* const srcEnd = src + srcLen / sizeof(*src);
	const USHORT* const dstEnd = dst + dstLen / sizeof(*dst);

	while (src < srcEnd && dst < dstEnd)
	{
		const ULONG ch = *src++;

		if (ch <= 0xFFFF)
			*(dst++) = ch;
		else if (ch <= 0x10FFFF)
		{
			*(dst++) = UTF16_LEAD(ch);

			if (dst < dstEnd)
				*(dst++) = UTF16_TRAIL(ch);
			else
			{
				*err_code = CS_TRUNCATION_ERROR;
				--dst;
				break;
			}
		}
		else
		{
			*err_code = CS_BAD_INPUT;
			--src;
			break;
		}
	}

	*err_position = static_cast<ULONG>((src - srcStart) * sizeof(*src));

	if (*err_code == 0 && src < srcEnd)
		*err_code = CS_TRUNCATION_ERROR;

	return static_cast<ULONG>((dst - dstStart) * sizeof(*dst));
}


SSHORT UnicodeUtil::utf16Compare(ULONG len1, const USHORT* str1, ULONG len2, const USHORT* str2,
								 INTL_BOOL* error_flag)
{
	fb_assert(len1 % sizeof(*str1) == 0);
	fb_assert(len2 % sizeof(*str2) == 0);
	fb_assert(str1 != NULL);
	fb_assert(str2 != NULL);
	fb_assert(error_flag != NULL);

	*error_flag = false;

	// safe casts - alignment not changed
	int32_t cmp = getConversionICU().u_strCompare(reinterpret_cast<const UChar*>(str1), len1 / sizeof(*str1),
		reinterpret_cast<const UChar*>(str2), len2 / sizeof(*str2), true);

	return (cmp < 0 ? -1 : (cmp > 0 ? 1 : 0));
}


ULONG UnicodeUtil::utf16Length(ULONG len, const USHORT* str)
{
	fb_assert(len % sizeof(*str) == 0);
	// safe cast - alignment not changed
	return getConversionICU().u_countChar32(reinterpret_cast<const UChar*>(str), len / sizeof(*str));
}


ULONG UnicodeUtil::utf16Substring(ULONG srcLen, const USHORT* src, ULONG dstLen, USHORT* dst,
								  ULONG startPos, ULONG length)
{
	fb_assert(srcLen % sizeof(*src) == 0);
	fb_assert(src != NULL && dst != NULL);

	if (length == 0)
		return 0;

	const USHORT* const dstStart = dst;
	const USHORT* const srcEnd = src + srcLen / sizeof(*src);
	const USHORT* const dstEnd = dst + dstLen / sizeof(*dst);
	ULONG pos = 0;

	while (src < srcEnd && dst < dstEnd && pos < startPos)
	{
		const ULONG ch = *src++;

		if (UTF_IS_LEAD(ch))
		{
			if (src < srcEnd && UTF_IS_TRAIL(*src))
				++src;
		}

		++pos;
	}

	while (src < srcEnd && dst < dstEnd && pos < startPos + length)
	{
		const ULONG ch = *src++;

		*(dst++) = ch;

		if (UTF_IS_LEAD(ch))
		{
			ULONG ch2;
			if (src < srcEnd && UTF_IS_TRAIL(ch2 = *src))
			{
				*(dst++) = ch2;
				++src;
			}
		}

		++pos;
	}

	return static_cast<ULONG>((dst - dstStart) * sizeof(*dst));
}


INTL_BOOL UnicodeUtil::utf8WellFormed(ULONG len, const UCHAR* str, ULONG* offending_position)
{
	fb_assert(str != NULL);

	ConversionICU& cIcu(getConversionICU());
	for (ULONG i = 0; i < len; )
	{
		UChar32 c = str[i++];

		if (c > 0x7F)
		{
			const ULONG save_i = i - 1;

			c = cIcu.utf8_nextCharSafeBody(str, reinterpret_cast<int32_t*>(&i), len, c, -1);

			if (c < 0)
			{
				if (offending_position)
					*offending_position = save_i;
				return false;	// malformed
			}
		}
	}

	return true;	// well-formed
}


INTL_BOOL UnicodeUtil::utf16WellFormed(ULONG len, const USHORT* str, ULONG* offending_position)
{
	fb_assert(str != NULL);
	fb_assert(len % sizeof(*str) == 0);

	len /= sizeof(*str);

	for (ULONG i = 0; i < len;)
	{
		const ULONG save_i = i;

		uint32_t c;
		U16_NEXT(str, i, len, c);

		if (!U_IS_SUPPLEMENTARY(c) && (U16_IS_LEAD(c) || U16_IS_TRAIL(c)))
		{
			if (offending_position)
				*offending_position = save_i * sizeof(*str);
			return false;	// malformed
		}
	}

	return true;	// well-formed
}


INTL_BOOL UnicodeUtil::utf32WellFormed(ULONG len, const ULONG* str, ULONG* offending_position)
{
	fb_assert(str != NULL);
	fb_assert(len % sizeof(*str) == 0);

	const ULONG* strStart = str;

	while (len)
	{
		if (!U_IS_UNICODE_CHAR(*str))
		{
			if (offending_position)
				*offending_position = static_cast<ULONG>((str - strStart) * sizeof(*str));
			return false;	// malformed
		}

		++str;
		len -= sizeof(*str);
	}

	return true;	// well-formed
}

void UnicodeUtil::utf8Normalize(UCharBuffer& data)
{
	ICU* icu = loadICU("", "");

	HalfStaticArray<USHORT, BUFFER_MEDIUM> utf16Buffer(data.getCount());
	USHORT errCode;
	ULONG errPosition;
	ULONG utf16BufferLen = utf8ToUtf16(data.getCount(), data.begin(), data.getCount() * sizeof(USHORT),
		utf16Buffer.getBuffer(data.getCount()), &errCode, &errPosition);

	UTransliterator* trans = icu->getCiAiTransliterator();

	if (trans)
	{
		const int32_t capacity = utf16Buffer.getCount() * sizeof(USHORT);
		int32_t len = utf16BufferLen / sizeof(USHORT);
		int32_t limit = len;

		UErrorCode errorCode = U_ZERO_ERROR;
		icu->utransTransUChars(trans, reinterpret_cast<UChar*>(utf16Buffer.begin()),
			&len, capacity, 0, &limit, &errorCode);
		icu->releaseCiAiTransliterator(trans);

		len = utf16ToUtf8(utf16BufferLen, utf16Buffer.begin(),
			len * 4, data.getBuffer(len * 4, false),
			&errCode, &errPosition);

		data.shrink(len);
	}
}

UnicodeUtil::ICU* UnicodeUtil::loadICU(const string& icuVersion, const string& configInfo)
{
	ObjectsArray<string> versions;
	getVersions(configInfo, versions);

	if (versions.isEmpty())
		gds__log("No ICU versions specified");

	string version = icuVersion.isEmpty() ? versions[0] : icuVersion;
	if (version == "default")
		version = UnicodeUtil::getDefaultIcuVersion();

	for (ObjectsArray<string>::const_iterator i(versions.begin()); i != versions.end(); ++i)
	{
		int majorVersion, minorVersion;
		int n = sscanf((*i == "default" ? version : *i).c_str(), "%d.%d",
			&majorVersion, &minorVersion);

		if (n == 1)
			minorVersion = 0;
		else if (n != 2)
			continue;

		string configVersion;
		configVersion.printf("%d.%d", majorVersion, minorVersion);
		if (version != configVersion)
		{
			minorVersion = 0;
			configVersion.printf("%d", majorVersion);
			if (version != configVersion)
				continue;
		}

		ReadLockGuard readGuard(icuModules->lock, "UnicodeUtil::loadICU");

		ICU* icu;
		if (icuModules->modules.get(version, icu))
			return icu;

		icu = FB_NEW_POOL(*getDefaultMemoryPool()) ICU(majorVersion, minorVersion);

		icu->ucModule = formatAndLoad(ucTemplate, icu->majorVersion, icu->minorVersion);
		if (!icu->ucModule)
		{
			gds__log("failed to load UC icu module version %s", configVersion.c_str());
			delete icu;
			continue;
		}

		icu->inModule = formatAndLoad(inTemplate, majorVersion, minorVersion);
		if (!icu->inModule)
		{
			gds__log("failed to load IN icu module version %s", configVersion.c_str());
			delete icu;
			continue;
		}

		if (icu->majorVersion != majorVersion || icu->minorVersion != minorVersion)
		{
			gds__log("Wrong version of IN icu module: loaded %d.%d, expected %d.%d",
					 majorVersion, minorVersion, icu->majorVersion, icu->minorVersion);
			delete icu;
			continue;
		}

		try
		{
			icu->initialize(icu->ucModule);

			icu->getEntryPoint("u_versionToString", icu->ucModule, icu->uVersionToString);
			icu->getEntryPoint("uloc_countAvailable", icu->ucModule, icu->ulocCountAvailable);
			icu->getEntryPoint("uloc_getAvailable", icu->ucModule, icu->ulocGetAvailable);
			icu->getEntryPoint("uset_close", icu->ucModule, icu->usetClose);
			icu->getEntryPoint("uset_getItem", icu->ucModule, icu->usetGetItem);
			icu->getEntryPoint("uset_getItemCount", icu->ucModule, icu->usetGetItemCount);
			icu->getEntryPoint("uset_open", icu->ucModule, icu->usetOpen);

			icu->getEntryPoint("ucol_close", icu->inModule, icu->ucolClose);
			icu->getEntryPoint("ucol_getContractionsAndExpansions", icu->inModule,
				icu->ucolGetContractionsAndExpansions);
			icu->getEntryPoint("ucol_getRules", icu->inModule, icu->ucolGetRules);
			icu->getEntryPoint("ucol_getSortKey", icu->inModule, icu->ucolGetSortKey);
			icu->getEntryPoint("ucol_open", icu->inModule, icu->ucolOpen);
			icu->getEntryPoint("ucol_openRules", icu->inModule, icu->ucolOpenRules);
			icu->getEntryPoint("ucol_setAttribute", icu->inModule, icu->ucolSetAttribute);
			icu->getEntryPoint("ucol_strcoll", icu->inModule, icu->ucolStrColl);
			icu->getEntryPoint("ucol_getVersion", icu->inModule, icu->ucolGetVersion);
			icu->getEntryPoint("utrans_openU", icu->inModule, icu->utransOpenU);
			icu->getEntryPoint("utrans_close", icu->inModule, icu->utransClose);
			icu->getEntryPoint("utrans_transUChars", icu->inModule, icu->utransTransUChars);
		}
		catch (const status_exception& s)
		{
			iscLogStatus("ICU load error", s.value());
			delete icu;
			continue;
		}

		UErrorCode status = U_ZERO_ERROR;

		UCollator* collator = icu->ucolOpen("", &status);
		if (!collator)
		{
			gds__log("ucolOpen failed");
			delete icu;
			continue;
		}

		icu->ucolGetVersion(collator, icu->collVersion);
		icu->ucolClose(collator);

		// RWLock don't allow lock upgrade (read->write) so we
		// release read and acquire a write lock.
		readGuard.release();
		WriteLockGuard writeGuard(icuModules->lock, "UnicodeUtil::loadICU");

		// In this small amount of time, one may already loaded the
		// same version, so within the write lock we verify again.
		ICU* icu2;
		if (icuModules->modules.get(version, icu2))
		{
			delete icu;
			return icu2;
		}

		icuModules->modules.put(version, icu);
		return icu;
	}

	return NULL;
}


void UnicodeUtil::getICUVersion(ICU* icu, int& majorVersion, int& minorVersion)
{
	majorVersion = icu->majorVersion;
	minorVersion = icu->minorVersion;
}


UnicodeUtil::ConversionICU& UnicodeUtil::getConversionICU()
{
	if (convIcu)
	{
		return *convIcu;
	}

	MutexLockGuard g(convIcuMutex, "UnicodeUtil::getConversionICU");

	if (convIcu)
	{
		return *convIcu;
	}

	// Try "favorite" (distributed on windows) version first
	const int favMaj = 63;
	const int favMin = 1;
	try
	{
		if ((convIcu = ImplementConversionICU::create(favMaj, favMin)))
			return *convIcu;
	}
	catch (const Exception&)
	{ }

	// Try system-wide version
	try
	{
		if ((convIcu = ImplementConversionICU::create(0, 0)))
			return *convIcu;
	}
	catch (const Exception&)
	{ }

	// Do a regular search
	LocalStatus ls;
	CheckStatusWrapper lastError(&ls);
	string version;

	// According to http://userguide.icu-project.org/design#TOC-Version-Numbers-in-ICU
	// we using two ranges of version numbers: 3.0 - 4.8 and 49 - 79.
	// Note 1: the most current version for now is 64, thus it is seems as enough to
	// limit upper bound by value of 79. It should be enlarged when necessary in the
	// future.
	// Note 2: the required function ucal_getTZDataVersion() is available since 3.8.

	for (int major = 79; major >= 3;)
	{
#ifdef WIN_NT
		int minor = 0;
#else
		int minor = 9;
#endif

		if (major == 4)
			minor = 8;
		else if (major <= 4)
			minor = 9;

		for (; minor >= 0; --minor)
		{
			if ((major == favMaj) && (minor == favMin))
			{
				continue;
			}

			try
			{
				if ((convIcu = ImplementConversionICU::create(major, minor)))
					return *convIcu;
			}
			catch (const Exception& ex)
			{
				ex.stuffException(&lastError);
				version.printf("Error loading ICU library version %d.%d", major, minor);
			}
		}

		if (major == 49)
			major = 4;
		else
			major--;
	}

	Arg::Gds err(isc_icu_library);

	if (lastError.getState() & Firebird::IStatus::STATE_ERRORS)
	{
		err << Arg::StatusVector(lastError.getErrors()) <<
			   Arg::Gds(isc_random) << Arg::Str(version);
	}

	err.raise();

	// compiler warning silencer
	return *convIcu;
}


string UnicodeUtil::getDefaultIcuVersion()
{
	string rc;
	UnicodeUtil::ConversionICU& icu(UnicodeUtil::getConversionICU());

	if (icu.vMajor >= 10 && icu.vMinor == 0)
		rc.printf("%d", icu.vMajor);
	else
		rc.printf("%d.%d", icu.vMajor, icu.vMinor);

	return rc;
}


UnicodeUtil::ICU* UnicodeUtil::getCollVersion(const Firebird::string& icuVersion,
	const Firebird::string& configInfo, Firebird::string& collVersion)
{
	ICU* icu = loadICU(icuVersion, configInfo);

	if (!icu)
		return nullptr;

	char version[U_MAX_VERSION_STRING_LENGTH];
	icu->uVersionToString(icu->collVersion, version);

	if (string(COLL_30_VERSION) == version)
		collVersion = "";
	else
		collVersion = version;

	return icu;
}

UnicodeUtil::Utf16Collation* UnicodeUtil::Utf16Collation::create(
	texttype* tt, USHORT attributes,
	Firebird::IntlUtil::SpecificAttributesMap& specificAttributes, const Firebird::string& configInfo)
{
	int attributeCount = 0;
	bool error;

	string locale;
	if (specificAttributes.get(IntlUtil::convertAsciiToUtf16("LOCALE"), locale))
		++attributeCount;

	string collVersion;
	if (specificAttributes.get(IntlUtil::convertAsciiToUtf16("COLL-VERSION"), collVersion))
	{
		++attributeCount;

		collVersion = IntlUtil::convertUtf16ToAscii(collVersion, &error);
		if (error)
		{
			gds__log("IntlUtil::convertUtf16ToAscii failed");
			return NULL;
		}
	}

	string numericSort;
	if (specificAttributes.get(IntlUtil::convertAsciiToUtf16("NUMERIC-SORT"), numericSort))
	{
		++attributeCount;

		numericSort = IntlUtil::convertUtf16ToAscii(numericSort, &error);
		if (error || !(numericSort == "0" || numericSort == "1"))
		{
			gds__log("IntlUtil::convertUtf16ToAscii failed");
			return NULL;
		}
	}

	string disableCompressions;
	if (specificAttributes.get(IntlUtil::convertAsciiToUtf16("DISABLE-COMPRESSIONS"), disableCompressions))
	{
		++attributeCount;

		disableCompressions = IntlUtil::convertUtf16ToAscii(disableCompressions, &error);
		if (error || !(disableCompressions == "0" || disableCompressions == "1"))
		{
			gds__log("IntlUtil::convertUtf16ToAscii failed");
			return NULL;
		}
	}

	locale = IntlUtil::convertUtf16ToAscii(locale, &error);
	if (error)
	{
		gds__log("IntlUtil::convertUtf16ToAscii failed");
		return NULL;
	}

	if ((attributes & ~(TEXTTYPE_ATTR_PAD_SPACE | TEXTTYPE_ATTR_CASE_INSENSITIVE |
			TEXTTYPE_ATTR_ACCENT_INSENSITIVE)) ||
		((attributes & (TEXTTYPE_ATTR_CASE_INSENSITIVE | TEXTTYPE_ATTR_ACCENT_INSENSITIVE)) ==
			TEXTTYPE_ATTR_ACCENT_INSENSITIVE)/* ||
		(specificAttributes.count() - attributeCount) != 0*/)
	{
		gds__log("attributes (%x) failed or %d != %d ?", attributes, specificAttributes.count(), attributeCount);
		return NULL;
	}

	if (collVersion.isEmpty())
		collVersion = COLL_30_VERSION;

	tt->texttype_pad_option = (attributes & TEXTTYPE_ATTR_PAD_SPACE) ? true : false;

	string icuVersion;
	if (specificAttributes.get(IntlUtil::convertAsciiToUtf16("ICU-VERSION"), icuVersion))
		icuVersion = IntlUtil::convertUtf16ToAscii(icuVersion, &error);

	const auto icu = loadICU(icuVersion, collVersion, locale, configInfo);

	UErrorCode status = U_ZERO_ERROR;
	HalfStaticArray<UChar, BUFFER_TINY> rulesBuffer;

	if (disableCompressions == "1")
	{
		UCollator* initialCollator = icu->ucolOpen(locale.c_str(), &status);

		if (!initialCollator)
		{
			gds__log("ucolOpen failed");
			return NULL;
		}

		static const char16_t CONTRACTION_RULES[] = u"[suppressContractions [^]]";
		int32_t rulesLen;
		const UChar* rules = icu->ucolGetRules(initialCollator, &rulesLen);
		rulesBuffer.push(rules, rulesLen);
		rulesBuffer.push((const UChar*) CONTRACTION_RULES, FB_NELEM(CONTRACTION_RULES) - 1);

		icu->ucolClose(initialCollator);
	}

	auto openCollation = [&]()
	{
		if (disableCompressions == "1")
		{
			UParseError parseError;
			return icu->ucolOpenRules(rulesBuffer.begin(), rulesBuffer.getCount(),
				UCOL_DEFAULT, UCOL_DEFAULT, &parseError, &status);
		}
		else
			return icu->ucolOpen(locale.c_str(), &status);
	};

	UCollator* compareCollator = openCollation();
	if (!compareCollator)
	{
		gds__log("ucolOpen failed");
		return NULL;
	}

	UCollator* partialCollator = openCollation();

	if (!partialCollator)
	{
		gds__log("ucolOpen failed");
		icu->ucolClose(compareCollator);
		return NULL;
	}

	UCollator* sortCollator = openCollation();
	if (!sortCollator)
	{
		gds__log("ucolOpen failed");
		icu->ucolClose(compareCollator);
		icu->ucolClose(partialCollator);
		return NULL;
	}

	icu->ucolSetAttribute(partialCollator, UCOL_STRENGTH, UCOL_PRIMARY, &status);

	if ((attributes & (TEXTTYPE_ATTR_CASE_INSENSITIVE | TEXTTYPE_ATTR_ACCENT_INSENSITIVE)) ==
		(TEXTTYPE_ATTR_CASE_INSENSITIVE | TEXTTYPE_ATTR_ACCENT_INSENSITIVE))
	{
		tt->texttype_flags |= TEXTTYPE_SEPARATE_UNIQUE;
		tt->texttype_canonical_width = 4;	// UTF-32
	}
	else if (attributes & TEXTTYPE_ATTR_CASE_INSENSITIVE)
	{
		tt->texttype_flags |= TEXTTYPE_SEPARATE_UNIQUE;
		tt->texttype_canonical_width = 4;	// UTF-32
	}
	else
		tt->texttype_flags = TEXTTYPE_DIRECT_MATCH;

	const bool isNumericSort = numericSort == "1";

	if (isNumericSort)
	{
		icu->ucolSetAttribute(compareCollator, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
		icu->ucolSetAttribute(partialCollator, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
		icu->ucolSetAttribute(sortCollator, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);

		icu->ucolSetAttribute(compareCollator, UCOL_STRENGTH, UCOL_IDENTICAL, &status);
		icu->ucolSetAttribute(sortCollator, UCOL_STRENGTH, UCOL_IDENTICAL, &status);

		tt->texttype_flags |= TEXTTYPE_UNSORTED_UNIQUE;
	}
	else
	{
		if ((attributes & (TEXTTYPE_ATTR_CASE_INSENSITIVE | TEXTTYPE_ATTR_ACCENT_INSENSITIVE)) ==
			(TEXTTYPE_ATTR_CASE_INSENSITIVE | TEXTTYPE_ATTR_ACCENT_INSENSITIVE))
		{
			icu->ucolSetAttribute(compareCollator, UCOL_STRENGTH, UCOL_PRIMARY, &status);
		}
		else if (attributes & TEXTTYPE_ATTR_CASE_INSENSITIVE)
			icu->ucolSetAttribute(compareCollator, UCOL_STRENGTH, UCOL_SECONDARY, &status);
	}

	Utf16Collation* obj = FB_NEW Utf16Collation();
	obj->icu = icu;
	obj->tt = tt;
	obj->attributes = attributes;
	obj->compareCollator = compareCollator;
	obj->partialCollator = partialCollator;
	obj->sortCollator = sortCollator;
	obj->numericSort = isNumericSort;
	obj->maxContractionsPrefixLength = 0;

	USet* contractions = icu->usetOpen(1, 0);
	// status not verified here.
	icu->ucolGetContractionsAndExpansions(partialCollator, contractions, nullptr, false, &status);

	int contractionsCount = icu->usetGetItemCount(contractions);

	for (int contractionIndex = 0; contractionIndex < contractionsCount; ++contractionIndex)
	{
		UChar strChars[10];
		UChar32 start, end;

		status = U_ZERO_ERROR;
		int len = icu->usetGetItem(contractions, contractionIndex, &start, &end, strChars, sizeof(strChars), &status);

		if (len >= 2)
		{
			obj->maxContractionsPrefixLength = len - 1 > obj->maxContractionsPrefixLength ?
				len - 1 : obj->maxContractionsPrefixLength;

			UCHAR key[100];
			int keyLen = icu->ucolGetSortKey(partialCollator, strChars, len, key, sizeof(key));

			for (int prefixLen = 1; prefixLen < len; ++prefixLen)
			{
				const Array<USHORT> str(reinterpret_cast<USHORT*>(strChars), prefixLen);
				auto keySet = obj->contractionsPrefix.get(str);

				if (!keySet)
				{
					keySet = obj->contractionsPrefix.put(str);

					UCHAR prefixKey[100];
					int prefixKeyLen = icu->ucolGetSortKey(partialCollator,
						strChars, prefixLen, prefixKey, sizeof(prefixKey));

					keySet->add(Array<UCHAR>(prefixKey, prefixKeyLen));
				}

				keySet->add(Array<UCHAR>(key, keyLen));
			}
		}
	}

	icu->usetClose(contractions);

	ContractionsPrefixMap::Accessor accessor(&obj->contractionsPrefix);

	for (bool found = accessor.getFirst(); found; found = accessor.getNext())
	{
		auto& keySet = accessor.current()->second;

		if (keySet.getCount() <= 1)
			continue;

		fb_assert(accessor.current()->first.hasData());
		USHORT ch = accessor.current()->first[0];

		if (ch >= 0xFDD0 && ch <= 0xFDEF)
		{
			keySet.clear();
			keySet.add(Array<UCHAR>());
			continue;
		}

		auto firstKeyIt = keySet.begin();
		auto lastKeyIt = --keySet.end();

		const UCHAR* firstKeyDataIt = firstKeyIt->begin();
		const UCHAR* lastKeyDataIt = lastKeyIt->begin();
		const UCHAR* firstKeyDataEnd = firstKeyIt->end();
		const UCHAR* lastKeyDataEnd = lastKeyIt->end();

		if (*firstKeyDataIt == *lastKeyDataIt)
		{
			unsigned common = 0;

			do
			{
				++common;
			} while (++firstKeyDataIt != firstKeyDataEnd && ++lastKeyDataIt != lastKeyDataEnd &&
				*firstKeyDataIt == *lastKeyDataIt);

			Array<UCHAR> commonKey(firstKeyIt->begin(), common);
			keySet.clear();
			keySet.add(commonKey);
		}
		else
		{
			auto secondKeyIt = ++keySet.begin();
			const UCHAR* secondKeyDataIt = secondKeyIt->begin();
			const UCHAR* secondKeyDataEnd = secondKeyIt->end();

			ObjectsArray<Array<UCHAR> > commonKeys;
			commonKeys.add(*firstKeyIt);

			while (secondKeyIt != keySet.end())
			{
				unsigned common = 0;

				while (firstKeyDataIt != firstKeyDataEnd && secondKeyDataIt != secondKeyDataEnd &&
					*firstKeyDataIt == *secondKeyDataIt)
				{
					++common;
					++firstKeyDataIt;
					++secondKeyDataIt;
				}

				unsigned backSize = commonKeys.back()->getCount();

				if (common > backSize)
					commonKeys.back()->append(secondKeyIt->begin() + backSize, common - backSize);
				else if (common < backSize)
				{
					if (common == 0)
						commonKeys.push(*secondKeyIt);
					else
						commonKeys.back()->resize(common);
				}

				if (++secondKeyIt != keySet.end())
				{
					++firstKeyIt;

					firstKeyDataIt = firstKeyIt->begin();
					secondKeyDataIt = secondKeyIt->begin();

					firstKeyDataEnd = firstKeyIt->end();
					secondKeyDataEnd = secondKeyIt->end();
				}
			}

			keySet.clear();

			for (auto ck : commonKeys)
				keySet.add(ck);
		}
	}

	if (obj->maxContractionsPrefixLength)
		tt->texttype_flags |= TEXTTYPE_MULTI_STARTING_KEY;

	return obj;
}


UnicodeUtil::Utf16Collation::~Utf16Collation()
{
	icu->ucolClose(compareCollator);
	icu->ucolClose(partialCollator);
	icu->ucolClose(sortCollator);

	// ASF: we should not "delete icu"
}


USHORT UnicodeUtil::Utf16Collation::keyLength(USHORT len) const
{
	return (len / 4) * 6;
}


USHORT UnicodeUtil::Utf16Collation::stringToKey(USHORT srcLen, const USHORT* src,
												USHORT dstLen, UCHAR* dst,
												USHORT key_type) const
{
	ULONG srcLenLong = srcLen;

	fb_assert(src != NULL && dst != NULL);
	fb_assert(srcLenLong % sizeof(*src) == 0);

	if (dstLen < keyLength(srcLenLong))
	{
		fb_assert(false);
		return INTL_BAD_KEY_LENGTH;
	}

	srcLenLong /= sizeof(*src);

	if (tt->texttype_pad_option)
	{
		const USHORT* pad;

		for (pad = src + srcLenLong - 1; pad >= src; --pad)
		{
			if (*pad != 32)
				break;
		}

		srcLenLong = pad - src + 1;
	}

	if (srcLenLong == 0)
		return 0;

	HalfStaticArray<USHORT, BUFFER_SMALL / 2> buffer;
	const UCollator* coll = NULL;

	switch (key_type)
	{
		case INTL_KEY_PARTIAL:
		case INTL_KEY_MULTI_STARTING:
			coll = partialCollator;
			break;

		case INTL_KEY_UNIQUE:
			coll = compareCollator;
			srcLenLong *= sizeof(*src);
			normalize(&srcLenLong, &src, true, buffer);
			srcLenLong /= sizeof(*src);
			break;

		case INTL_KEY_SORT:
			coll = sortCollator;
			break;

		default:
			fb_assert(false);
			return INTL_BAD_KEY_LENGTH;
	}

	if (key_type == INTL_KEY_MULTI_STARTING)
	{
		bool trailingNumbersRemoved = false;

		if (numericSort)
		{
			// ASF: Wee need to remove trailing numbers to return sub key that
			// matches full key. Example: "abc1" becomes "abc" to match "abc10".
			const USHORT* p = src + srcLenLong - 1;

			for (; p >= src; --p)
			{
				if (!(*p >= '0' && *p <= '9'))
					break;

				trailingNumbersRemoved = true;
			}

			srcLenLong = p - src + 1;
		}

		if (!trailingNumbersRemoved)
		{
			for (int i = MIN(maxContractionsPrefixLength, srcLenLong); i > 0; --i)
			{
				auto keys = contractionsPrefix.get(Array<USHORT>(src + srcLenLong - i, i));

				if (keys)
				{
					const UCHAR* dstStart = dst;
					ULONG prefixLen;

					srcLenLong -= i;

					if (srcLenLong != 0)
					{
						prefixLen = icu->ucolGetSortKey(coll,
							reinterpret_cast<const UChar*>(src), srcLenLong, dst + 2, dstLen - 2);

						if (prefixLen == 0 || prefixLen > dstLen - 2 || prefixLen > MAX_USHORT)
							return INTL_BAD_KEY_LENGTH;

						fb_assert(dst[2 + prefixLen - 1] == '\0');
						--prefixLen;
						dstLen -= 2 + prefixLen;
					}
					else
						prefixLen = 0;

					for (const auto& keyIt : *keys)
					{
						const ULONG keyLen = prefixLen + keyIt.getCount();

						if (keyLen > dstLen - 2 || keyLen > MAX_USHORT)
							return INTL_BAD_KEY_LENGTH;

						dst[0] = UCHAR(keyLen & 0xFF);
						dst[1] = UCHAR(keyLen >> 8);

						if (dst != dstStart)
							memcpy(dst + 2, dstStart + 2, prefixLen);

						memcpy(dst + 2 + prefixLen, keyIt.begin(), keyIt.getCount());
						dst += 2 + keyLen;
						dstLen -= 2 + keyLen;
					}

					return dst - dstStart;
				}
			}
		}

		ULONG keyLen = icu->ucolGetSortKey(coll,
			reinterpret_cast<const UChar*>(src), srcLenLong, dst + 2, dstLen - 3);

		if (keyLen == 0 || keyLen > dstLen - 3 || keyLen > MAX_USHORT)
			return INTL_BAD_KEY_LENGTH;

		fb_assert(dst[2 + keyLen - 1] == '\0');
		--keyLen;

		dst[0] = UCHAR(keyLen & 0xFF);
		dst[1] = UCHAR(keyLen >> 8);

		return keyLen + 2;
	}

	const ULONG keyLen = icu->ucolGetSortKey(coll,
		reinterpret_cast<const UChar*>(src), srcLenLong, dst, dstLen);

	if (keyLen == 0 || keyLen > dstLen || keyLen > MAX_USHORT)
		return INTL_BAD_KEY_LENGTH;

	return keyLen;
}


SSHORT UnicodeUtil::Utf16Collation::compare(ULONG len1, const USHORT* str1,
											ULONG len2, const USHORT* str2,
											INTL_BOOL* error_flag) const
{
	fb_assert(len1 % sizeof(*str1) == 0 && len2 % sizeof(*str2) == 0);
	fb_assert(str1 != NULL && str2 != NULL);
	fb_assert(error_flag != NULL);

	*error_flag = false;

	len1 /= sizeof(*str1);
	len2 /= sizeof(*str2);

	if (tt->texttype_pad_option)
	{
		const USHORT* pad;

		for (pad = str1 + len1 - 1; pad >= str1; --pad)
		{
			if (*pad != 32)
				break;
		}

		len1 = pad - str1 + 1;

		for (pad = str2 + len2 - 1; pad >= str2; --pad)
		{
			if (*pad != 32)
				break;
		}

		len2 = pad - str2 + 1;
	}

	len1 *= sizeof(*str1);
	len2 *= sizeof(*str2);

	HalfStaticArray<USHORT, BUFFER_SMALL / 2> buffer1, buffer2;
	normalize(&len1, &str1, true, buffer1);
	normalize(&len2, &str2, true, buffer2);

	len1 /= sizeof(*str1);
	len2 /= sizeof(*str2);

	return (SSHORT) icu->ucolStrColl(compareCollator,
		// safe casts - alignment not changed
		reinterpret_cast<const UChar*>(str1), len1,
		reinterpret_cast<const UChar*>(str2), len2);
}


ULONG UnicodeUtil::Utf16Collation::canonical(ULONG srcLen, const USHORT* src, ULONG dstLen, ULONG* dst,
	const ULONG* exceptions)
{
	HalfStaticArray<USHORT, BUFFER_SMALL / 2> upperStr;
	normalize(&srcLen, &src, false, upperStr);

	// convert UTF-16 to UTF-32
	USHORT errCode;
	ULONG errPosition;
	return utf16ToUtf32(srcLen, src, dstLen, dst, &errCode, &errPosition) / sizeof(ULONG);
}


UnicodeUtil::ICU* UnicodeUtil::Utf16Collation::loadICU(
	const string& icuVersion, const string& collVersion,
	const string& locale, const string& configInfo)
{
	ObjectsArray<string> versions;
	getVersions(configInfo, versions);

	for (ObjectsArray<string>::const_iterator i(versions.begin()); i != versions.end(); ++i)
	{
		ICU* icu = UnicodeUtil::loadICU(*i, configInfo);
		if (!icu)
			continue;

		if (locale.hasData())
		{
			int avail = icu->ulocCountAvailable();

			while (--avail >= 0)
			{
				if (locale == icu->ulocGetAvailable(avail))
					break;
			}

			if (avail < 0)
			{
				UErrorCode status = U_ZERO_ERROR;
				UCollator* testCollator = icu->ucolOpen(locale.c_str(), &status);
				if (!testCollator)
					continue;

				icu->ucolClose(testCollator);
				if (status != U_ZERO_ERROR)
					continue;
			}
		}

		char version[U_MAX_VERSION_STRING_LENGTH];
		icu->uVersionToString(icu->collVersion, version);

		if (collVersion != version)
			continue;

		return icu;
	}

	string errorMsg;

	if (icuVersion.isEmpty())
	{
		errorMsg.printf(
			"An ICU library with collation version %s is required but was not found. "
			"You may try to install another ICU version with this collation version "
			"or look for 'gfix -icu' in Firebird documentation.",
			collVersion.c_str());
	}
	else
	{
		errorMsg.printf(
			"An ICU library with collation version %s is required but was not found. "
			"You may try to install ICU version %s, used to register the collation in this database "
			"or look for 'gfix -icu' in Firebird documentation.",
			collVersion.c_str(), icuVersion.c_str());
	}

	(Arg::Gds(isc_random) << errorMsg).raise();
}


void UnicodeUtil::Utf16Collation::normalize(ULONG* strLen, const USHORT** str, bool forNumericSort,
	HalfStaticArray<USHORT, BUFFER_SMALL / 2>& buffer) const
{
	fb_assert(*strLen % sizeof(**str) == 0);

	if (forNumericSort && !numericSort)
		return;

	if (attributes & TEXTTYPE_ATTR_CASE_INSENSITIVE)
	{
		*strLen = utf16UpperCase(*strLen, *str, *strLen,
			buffer.getBuffer(*strLen / sizeof(USHORT)), NULL);
		*str = buffer.begin();

		if (attributes & TEXTTYPE_ATTR_ACCENT_INSENSITIVE)
		{
			UTransliterator* trans = icu->getCiAiTransliterator();

			if (trans)
			{
				const int32_t capacity = buffer.getCount();
				int32_t len = *strLen / sizeof(USHORT);
				int32_t limit = len;

				UErrorCode errorCode = U_ZERO_ERROR;
				icu->utransTransUChars(trans, reinterpret_cast<UChar*>(buffer.begin()),
					&len, capacity, 0, &limit, &errorCode);
				icu->releaseCiAiTransliterator(trans);

				*strLen = len * sizeof(USHORT);
			}
		}
	}
}


}	// namespace Jrd
