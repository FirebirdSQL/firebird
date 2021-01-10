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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2002 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"

#include "../common/config/config.h"
#include "../common/config/config_file.h"
#include "../common/classes/init.h"
#include "../common/dllinst.h"
#include "../common/os/fbsyslog.h"
#include "../common/utils_proto.h"
#include "../jrd/constants.h"
#include "firebird/Interface.h"
#include "../common/db_alias.h"
#include "../jrd/build_no.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

// NS 2014-07-23 FIXME: Rework error handling
// 1. We shall not silently truncate upper bits of integer values read from configuration files.
// 2. Invalid configuration file values that we ignored shall leave trace in firebird.log
//    to avoid user confusion.
// 3. Incorrect syntax for parameter values shall not be ignored silently
// 4. Integer overflow during parsing of parameter value shall not be ignored silently
//
// Currently user can only guess which parameter values have been applied by the engine
// and which were ignored. Or resort to reading source code and using debugger to find out.

namespace {

/******************************************************************************
 *
 *	firebird.conf implementation
 */

class ConfigImpl : public Firebird::PermanentStorage
{
public:
	explicit ConfigImpl(Firebird::MemoryPool& p)
		: Firebird::PermanentStorage(p), missConf(false)
	{
		try
		{
			ConfigFile file(fb_utils::getPrefix(Firebird::IConfigManager::DIR_CONF, Firebird::CONFIG_FILE),
				ConfigFile::ERROR_WHEN_MISS);
			defaultConfig = FB_NEW Firebird::Config(file);
		}
		catch (const Firebird::status_exception& ex)
		{
			if (ex.value()[1] != isc_miss_config)
			{
				throw;
			}

			missConf = true;

			ConfigFile file(ConfigFile::USE_TEXT, "");
			defaultConfig = FB_NEW Firebird::Config(file);
		}
	}

	/***
	It was a kind of getting ready for changing config remotely...

	void changeDefaultConfig(Firebird::Config* newConfig)
	{
		defaultConfig = newConfig;
	}
	***/

	Firebird::RefPtr<const Firebird::Config>& getDefaultConfig()
	{
		return defaultConfig;
	}

	bool missFirebirdConf() const
	{
		return missConf;
	}

	Firebird::IFirebirdConf* getFirebirdConf()
	{
		Firebird::IFirebirdConf* rc = FB_NEW Firebird::FirebirdConf(defaultConfig);
		rc->addRef();
		return rc;
	}

private:
	Firebird::RefPtr<const Firebird::Config> defaultConfig;

    ConfigImpl(const ConfigImpl&);
    void operator=(const ConfigImpl&);

	bool missConf;
};

/******************************************************************************
 *
 *	Static instance of the system configuration file
 */

Firebird::InitInstance<ConfigImpl> firebirdConf;

}	// anonymous namespace

namespace Firebird
{

IFirebirdConf* getFirebirdConfig()
{
	return firebirdConf().getFirebirdConf();
}

/******************************************************************************
 *
 *	Configuration entries
 */

const char*	GCPolicyCooperative	= "cooperative";
const char*	GCPolicyBackground	= "background";
const char*	GCPolicyCombined	= "combined";

ConfigValue Config::defaults[MAX_CONFIG_KEY];

/******************************************************************************
 *
 *	Config routines
 */

Config::Config(const ConfigFile& file)
	: valuesSource(*getDefaultMemoryPool()),
	notifyDatabase(*getDefaultMemoryPool()),
	serverMode(-1),
	defaultConfig(false)
{
	memset(sourceIdx, 0, sizeof(sourceIdx));
	valuesSource.add(NULL);

	setupDefaultConfig();

	// Array to save string temporarily
	// Will be finally saved by loadValues() in the end of ctor
	ObjectsArray<ConfigFile::String> tempStrings(getPool());

	// Iterate through the known configuration entries
	for (unsigned int i = 0; i < MAX_CONFIG_KEY; i++)
	{
		values[i] = defaults[i];
		if (entries[i].data_type == TYPE_STRING && values[i].strVal)
		{
			ConfigFile::String expand(values[i].strVal);
			if (file.macroParse(expand, NULL) && expand != values[i].strVal)
			{
				ConfigFile::String& saved(tempStrings.add());
				saved = expand;
				values[i] = (ConfigValue) saved.c_str();
			}
		}
	}

	loadValues(file, CONFIG_FILE);
}

Config::Config(const ConfigFile& file, const char* srcName, const Config& base, const PathName& notify)
	: valuesSource(*getDefaultMemoryPool()),
	notifyDatabase(*getDefaultMemoryPool()),
	serverMode(-1),
	defaultConfig(false)
{
	memset(sourceIdx, 0, sizeof(sourceIdx));
	valuesSource.add(NULL);

	for (FB_SIZE_T i = 1; i < base.valuesSource.getCount(); i++)
	{
		const char* src = base.valuesSource[i];
		const size_t len = strlen(src);
		char* str = FB_NEW char[len + 1];
		strcpy(str, src);
		valuesSource.add(str);
	}

	// Iterate through the known configuration entries

	for (unsigned int i = 0; i < MAX_CONFIG_KEY; i++)
	{
		values[i] = base.values[i];
		sourceIdx[i] = base.sourceIdx[i];
	}

	loadValues(file, srcName);

	notifyDatabase = notify;
}

void Config::notify() const
{
	if (!notifyDatabase.hasData())
		return;
	if (notifyDatabaseName(notifyDatabase))
		notifyDatabase.erase();
}

void Config::merge(RefPtr<const Config>& config, const string* dpbConfig)
{
	if (dpbConfig && dpbConfig->hasData())
	{
		ConfigFile txtStream(ConfigFile::USE_TEXT, dpbConfig->c_str());
		config = FB_NEW Config(txtStream, "<DPB>", *(config.hasData() ? config : getDefaultConfig()));
	}
}

void Config::loadValues(const ConfigFile& file, const char* srcName)
{
	unsigned srcIdx = 0;

	// Iterate through the known configuration entries

	for (int i = 0; i < MAX_CONFIG_KEY; i++)
	{
		const ConfigEntry& entry = entries[i];
		const ConfigFile::Parameter* par = file.findParameter(entry.key);

		// Don't assign values to the global keys at non-default config
		if (par && (defaultConfig || !entry.is_global))
		{
			// Assign the actual value

			switch (entry.data_type)
			{
			case TYPE_BOOLEAN:
				values[i].boolVal = par->asBoolean();
				break;
			case TYPE_INTEGER:
				values[i].intVal = par->asInteger();
				break;
			case TYPE_STRING:
				values[i].strVal = par->value.c_str();
				break;
			//case TYPE_STRING_VECTOR:
			//	break;
			}

			if (!srcIdx)
			{
				const size_t len = strlen(srcName);
				char* str = FB_NEW char[len + 1];
				strcpy(str, srcName);
				srcIdx = valuesSource.add(str);

				fb_assert(srcIdx <= MAX_UCHAR);
			}
			sourceIdx[i] = srcIdx;
		}

		if (entry.data_type == TYPE_STRING && values[i] != defaults[i])
		{
			const char* src = values[i].strVal;
			char* dst = FB_NEW_POOL(getPool()) char[strlen(src) + 1];
			strcpy(dst, src);
			values[i] = (ConfigValue) dst;
		}
	}

	checkValues();
}

static const char* txtServerModes[6] = 
{
	"Super", "ThreadedDedicated", 
	"SuperClassic", "ThreadedShared", 
	"Classic", "MultiProcess" 
};

void Config::setupDefaultConfig()
{
	defaultConfig = true;

	for (unsigned i = 0; i < MAX_CONFIG_KEY; i++)
		defaults[i] = entries[i].default_value;

	const bool bootBuild = fb_utils::bootBuild();

	ConfigValue* pDefault = &defaults[KEY_SERVER_MODE];
	serverMode = bootBuild ? MODE_CLASSIC : MODE_SUPER;
	pDefault->strVal = txtServerModes[2 * serverMode];

	pDefault = &defaults[KEY_TEMP_CACHE_LIMIT];
	if (pDefault->intVal < 0)
		pDefault->intVal = (serverMode != MODE_SUPER) ? 8388608 : 67108864;	// bytes

	defaults[KEY_REMOTE_FILE_OPEN_ABILITY].boolVal = bootBuild;

	pDefault = &defaults[KEY_DEFAULT_DB_CACHE_PAGES];
	if (pDefault->intVal < 0)
		pDefault->intVal = (serverMode != MODE_SUPER) ? 256 : 2048;	// pages

	pDefault = &defaults[KEY_GC_POLICY];
	if (!pDefault->strVal)
	{
		pDefault->strVal = (serverMode == MODE_SUPER) ? GCPolicyCombined : GCPolicyCooperative;
	}

	//pDefault = &entries[KEY_WIRE_CRYPT].default_value;
//	if (!*pDefault)
//		*pDefault == (ConfigValue) (xxx == WC_CLIENT) ? WIRE_CRYPT_ENABLED : WIRE_CRYPT_REQUIRED;

}

void Config::checkIntForLoBound(ConfigKey key, SINT64 loBound, bool setDefault)
{
	fb_assert(entries[key].data_type == TYPE_INTEGER);
	if (values[key].intVal < loBound)
		values[key].intVal = setDefault ? defaults[key].intVal : loBound;
}

void Config::checkIntForHiBound(ConfigKey key, SINT64 hiBound, bool setDefault)
{
	fb_assert(entries[key].data_type == TYPE_INTEGER);
	if (values[key].intVal > hiBound)
		values[key].intVal = setDefault ? defaults[key].intVal : hiBound;
}

void Config::checkValues()
{
	checkIntForLoBound(KEY_TEMP_CACHE_LIMIT, 0, true);

	checkIntForLoBound(KEY_TCP_REMOTE_BUFFER_SIZE, 1448, false);
	checkIntForHiBound(KEY_TCP_REMOTE_BUFFER_SIZE, MAX_SSHORT, false);

	checkIntForLoBound(KEY_DEFAULT_DB_CACHE_PAGES, 0, true);

	checkIntForLoBound(KEY_LOCK_MEM_SIZE, 64 * 1024, false);

	const char* strVal = values[KEY_GC_POLICY].strVal;
	if (strVal)
	{
		NoCaseString gcPolicy(strVal);
		if (gcPolicy != GCPolicyCooperative &&
			gcPolicy != GCPolicyBackground &&
			gcPolicy != GCPolicyCombined)
		{
			// user-provided value is invalid - fail to default
			values[KEY_GC_POLICY] = defaults[KEY_GC_POLICY];
		}
	}

	strVal = values[KEY_WIRE_CRYPT].strVal;
	if (strVal)
	{
		NoCaseString wireCrypt(strVal);
		if (wireCrypt != "DISABLED" && wireCrypt != "ENABLED" && wireCrypt != "REQUIRED")
		{
			// user-provided value is invalid - fail to default
			values[KEY_WIRE_CRYPT] = defaults[KEY_WIRE_CRYPT];
		}
	}

	strVal = values[KEY_SERVER_MODE].strVal;
	if (strVal && !fb_utils::bootBuild())
	{
		bool found = false;
		NoCaseString mode(strVal);
		for (int x = 0; x < 6; ++x)
		{
			if (mode == txtServerModes[x])
			{
				serverMode = x / 2;
				found = true;
				break;
			}
		}

		if (!found)
			values[KEY_SERVER_MODE] = defaults[KEY_SERVER_MODE];
	}

	checkIntForLoBound(KEY_FILESYSTEM_CACHE_THRESHOLD, 0, true);

	checkIntForLoBound(KEY_MAX_IDENTIFIER_BYTE_LENGTH, 1, true);
	checkIntForHiBound(KEY_MAX_IDENTIFIER_BYTE_LENGTH, MAX_SQL_IDENTIFIER_LEN, true);

	checkIntForLoBound(KEY_MAX_IDENTIFIER_CHAR_LENGTH, 1, true);
	checkIntForHiBound(KEY_MAX_IDENTIFIER_CHAR_LENGTH, METADATA_IDENTIFIER_CHAR_LEN, true);

	checkIntForLoBound(KEY_SNAPSHOTS_MEM_SIZE, 1, true);
	checkIntForHiBound(KEY_SNAPSHOTS_MEM_SIZE, MAX_ULONG, true);

	checkIntForLoBound(KEY_TIP_CACHE_BLOCK_SIZE, 1, true);
	checkIntForHiBound(KEY_TIP_CACHE_BLOCK_SIZE, MAX_ULONG, true);

	checkIntForLoBound(KEY_INLINE_SORT_THRESHOLD, 0, true);
}


Config::~Config()
{
	// Free allocated memory

	for (int i = 0; i < MAX_CONFIG_KEY; i++)
	{
		if (values[i] == defaults[i])
			continue;

		switch (entries[i].data_type)
		{
		case TYPE_STRING:
			delete[] values[i].strVal;
			break;
		//case TYPE_STRING_VECTOR:
		//	break;
		}
	}

	for (FB_SIZE_T i = 1; i < valuesSource.getCount(); i++)
		delete[] valuesSource[i];
}


/******************************************************************************
 *
 *	Public interface
 */

const RefPtr<const Config>& Config::getDefaultConfig()
{
	return firebirdConf().getDefaultConfig();
}

bool Config::missFirebirdConf()
{
	return firebirdConf().missFirebirdConf();
}

const char* Config::getInstallDirectory()
{
	return fb_get_master_interface()->getConfigManager()->getInstallDirectory();
}

static PathName* rootFromCommandLine = 0;

void Config::setRootDirectoryFromCommandLine(const PathName& newRoot)
{
	delete rootFromCommandLine;
	rootFromCommandLine = FB_NEW_POOL(*getDefaultMemoryPool())
		PathName(*getDefaultMemoryPool(), newRoot);
}

const PathName* Config::getCommandLineRootDirectory()
{
	return rootFromCommandLine;
}

const char* Config::getRootDirectory()
{
	// must check it here - command line must override any other root settings
	if (rootFromCommandLine)
	{
		return rootFromCommandLine->c_str();
	}

	return fb_get_master_interface()->getConfigManager()->getRootDirectory();
}


unsigned int Config::getKeyByName(ConfigName nm)
{
	ConfigFile::KeyType name(nm);
	for (unsigned int i = 0; i < MAX_CONFIG_KEY; i++)
	{
		if (name == entries[i].key)
		{
			return i;
		}
	}

	return ~0;
}

SINT64 Config::getInt(unsigned int key) const
{
	if (key >= MAX_CONFIG_KEY)
		return 0;
	return getInt(static_cast<ConfigKey>(key));
}

const char* Config::getString(unsigned int key) const
{
	if (key >= MAX_CONFIG_KEY)
		return NULL;

	return getStr(static_cast<ConfigKey>(key));
}

bool Config::getBoolean(unsigned int key) const
{
	if (key >= MAX_CONFIG_KEY)
		return false;
	return getBool(static_cast<ConfigKey>(key));
}

bool Config::valueAsString(ConfigValue val, ConfigType type, string& str)
{
	switch (type)
	{
	case TYPE_INTEGER:
		str.printf("%" SQUADFORMAT, val.intVal);
		break;

	case TYPE_STRING:
	{
		if (val.strVal == NULL)
			return false;

		str = val.strVal;
	}
	break;

	case TYPE_BOOLEAN:
		str = val.boolVal ? "true" : "false";
		break;
	}

	return true;
}

const char* Config::getKeyName(unsigned int key)
{
	if (key >= MAX_CONFIG_KEY)
		return nullptr;

	return entries[key].key;
}

ConfigValue Config::specialProcessing(ConfigKey key, ConfigValue val)
{
	// irregular case
	switch (key)
	{
	case KEY_SECURITY_DATABASE:
		if (!val.strVal)
		{
			val.strVal = MasterInterfacePtr()->getConfigManager()->getDefaultSecurityDb();
			if (!val.strVal)
				val.strVal = "security.db";
		}
		break;
	}

	return val;
}

bool Config::getValue(unsigned int key, string& str) const
{
	if (key >= MAX_CONFIG_KEY)
		return false;

	const ConfigValue& val = entries[key].is_global ?
		getDefaultConfig()->values[key] : values[key];

	return valueAsString(specialProcessing(static_cast<ConfigKey>(key), val), entries[key].data_type, str);
}

bool Config::getDefaultValue(unsigned int key, string& str)
{
	if (key >= MAX_CONFIG_KEY)
		return false;

	if (key == KEY_WIRE_CRYPT && !defaults[key].strVal)
	{
		str = "Required";	// see getWireCrypt(WC_SERVER)
		return true;
	}

	return valueAsString(specialProcessing(static_cast<ConfigKey>(key), defaults[key]), entries[key].data_type, str);
}



// Macros below helps to implement non-trivial Config::getXXX functions :
// - checks for correct[non-]static function declaration,
// - declare and initialize local vars "key" and "config" (correct Config 
//   instance to get values from).

#define DECLARE_GLOBAL_KEY(KEY)		\
	static_assert(entries[KEY].is_global, "Requires global key"); \
	const ConfigKey key = KEY;		\
	const Config* config = getDefaultConfig();

#define DECLARE_PER_DB_KEY(KEY)		\
	static_assert(!entries[KEY].is_global, "Requires per-database key"); \
	const ConfigKey key = KEY;		\
	const Config* config = this;

int Config::getServerMode()
{
	DECLARE_GLOBAL_KEY(KEY_SERVER_MODE);
	return config->serverMode;
}

const char* Config::getPlugins(unsigned int type) const
{
	ConfigKey aKey;
	switch (type)
	{
		case IPluginManager::TYPE_PROVIDER:
		{
			DECLARE_PER_DB_KEY(KEY_PLUG_PROVIDERS);
			aKey = key;
			break;
		}
		case IPluginManager::TYPE_AUTH_SERVER:
		{
			DECLARE_PER_DB_KEY(KEY_PLUG_AUTH_SERVER);
			aKey = key;
			break;
		}
		case IPluginManager::TYPE_AUTH_CLIENT:
		{
			DECLARE_PER_DB_KEY(KEY_PLUG_AUTH_CLIENT);
			aKey = key;
			break;
		}
		case IPluginManager::TYPE_AUTH_USER_MANAGEMENT:
		{
			DECLARE_PER_DB_KEY(KEY_PLUG_AUTH_MANAGE);
			aKey = key;
			break;
		}
		case IPluginManager::TYPE_TRACE:
		{
			DECLARE_PER_DB_KEY(KEY_PLUG_TRACE);
			aKey = key;
			break;
		}
		case IPluginManager::TYPE_WIRE_CRYPT:
		{
			DECLARE_PER_DB_KEY(KEY_PLUG_WIRE_CRYPT);
			aKey = key;
			break;
		}
		case IPluginManager::TYPE_KEY_HOLDER:
		{
			DECLARE_PER_DB_KEY(KEY_PLUG_KEY_HOLDER);
			aKey = key;
			break;
		}

		default:
			(Arg::Gds(isc_random) << "Internal error in Config::getPlugins(): unknown plugin type requested").raise();
	}

	return getStr(aKey);
}

int Config::getWireCrypt(WireCryptMode wcMode) const
{
	DECLARE_PER_DB_KEY(KEY_WIRE_CRYPT);

	bool present;
	const char* wc = getStr(key, &present);
	if (present && wc)
	{
		NoCaseString wireCrypt(wc);
		if (wireCrypt == "DISABLED")
			return WIRE_CRYPT_DISABLED;
		if (wireCrypt == "ENABLED")
			return WIRE_CRYPT_ENABLED;
		if (wireCrypt == "REQUIRED")
			return WIRE_CRYPT_REQUIRED;

		// wrong user value, fail to default
		// should not happens, see checkValues()
		fb_assert(false);
	}

	return wcMode == WC_CLIENT ? WIRE_CRYPT_ENABLED : WIRE_CRYPT_REQUIRED;
}

bool Config::getUseFileSystemCache(bool* pPresent) const
{
	DECLARE_PER_DB_KEY(KEY_USE_FILESYSTEM_CACHE);
	return getBool(key, pPresent);
}


///	class FirebirdConf 

// array format: major, minor, release, build
static unsigned short fileVerNumber[4] = {FILE_VER_NUMBER};

static inline unsigned int getPartialVersion()
{
			// major				   // minor
	return (fileVerNumber[0] << 24) | (fileVerNumber[1] << 16);
}

static inline unsigned int getFullVersion()
{
								 // build_no
	return getPartialVersion() | fileVerNumber[3];
}

static unsigned int PARTIAL_MASK = 0xFFFF0000;
static unsigned int KEY_MASK = 0xFFFF;

static inline void checkKey(unsigned int& key)
{
	if ((key & PARTIAL_MASK) != getPartialVersion())
		key = KEY_MASK;
	else
		key &= KEY_MASK;
}

unsigned int FirebirdConf::getVersion(CheckStatusWrapper* status)
{
	return getFullVersion();
}

unsigned int FirebirdConf::getKey(const char* name)
{
	return Config::getKeyByName(name) | getPartialVersion();
}

ISC_INT64 FirebirdConf::asInteger(unsigned int key)
{
	checkKey(key);
	return config->getInt(key);
}

const char* FirebirdConf::asString(unsigned int key)
{
	checkKey(key);
	return config->getString(key);
}

FB_BOOLEAN FirebirdConf::asBoolean(unsigned int key)
{
	checkKey(key);
	return config->getBoolean(key);
}

} // namespace Firebird
