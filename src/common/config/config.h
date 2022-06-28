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

#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include "../common/classes/alloc.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/RefCounted.h"
#include "../common/config/config_file.h"
#include "../common/classes/ImplementHelper.h"
#include "../jrd/constants.h"

#include <type_traits>

/**
	Since the original (isc.cpp) code wasn't able to provide powerful and
	easy-to-use abilities to work with complex configurations, a decision
	has been made to create a completely new one.

	This class is a public interface for our generic configuration manager
	and allows to access all configuration values by its getXXX() member
	functions. Each of these functions corresponds to one and only one key
	and has one input argument - default value, which is used when the
	requested key is missing or the configuration file is not found. Supported
	value datatypes are "const char*", "int" and "bool". Usual default values for
	these datatypes are empty string, zero and false respectively. There are
	two types of member functions - scalar and vector. The former ones return
	single value of the given type. The latter ones return vector which
	contains an ordered array of values.

	There's one exception - getRootDirectory() member function, which returns
	root pathname of the current installation. This value isn't stored in the
	configuration file, but is managed by the code itself. But there's a way
	to override this value via the configuration file as well.

	To add new configuration item, you have to take the following steps:

		1. Add logical key to ConfigKey enumeration (config.h)
		2. Add key description to entries[] array (config.h)
		   (note: both physical and logical keys MUST have the same ordinal
				  position within appropriate structures)
		3. Add member function to Config class (config.h) and implement it
		   in config.h or config.cpp module.
		4. For per-database configurable parameters, please use
				type getParameterName() const;
		   form, for world-wide (global) parameters
				static type getParameterName();
		   should be used. Also, for world-wide parameters, values of default
		   config instance (see getDefaultConfig()) should be used.
		5. Macros CONFIG_GET_GLOBAL_XXX and CONFIG_GET_PER_DB_XXX helps to
		   declare and implement trivial getXXX functions and to enforce rule (4).
**/

namespace Firebird
{

extern const char*	GCPolicyCooperative;
extern const char*	GCPolicyBackground;
extern const char*	GCPolicyCombined;

const int WIRE_CRYPT_DISABLED = 0;
const int WIRE_CRYPT_ENABLED = 1;
const int WIRE_CRYPT_REQUIRED = 2;

enum WireCryptMode {WC_CLIENT, WC_SERVER};		// Have different defaults

const int MODE_SUPER = 0;
const int MODE_SUPERCLASSIC = 1;
const int MODE_CLASSIC = 2;

const char* const CONFIG_FILE = "firebird.conf";

struct ConfigValue
{
	ConfigValue() : intVal(0) {};
	constexpr ConfigValue(const char* val) : strVal(val) {};
	constexpr ConfigValue(bool val) : boolVal(val) {};
	constexpr ConfigValue(int val) : intVal(val) {};

	union
	{
		const char* strVal;
		bool boolVal;
		SINT64 intVal;
	};

	// simple bitwise comparison
	bool operator== (const ConfigValue& other) const
	{
		return this->intVal == other.intVal;
	}

	bool operator!= (const ConfigValue& other) const
	{
		return !(*this == other);
	}
};

enum ConfigKey
{
	KEY_TEMP_BLOCK_SIZE,
	KEY_TEMP_CACHE_LIMIT,
	KEY_REMOTE_FILE_OPEN_ABILITY,
	KEY_GUARDIAN_OPTION,
	KEY_CPU_AFFINITY_MASK,
	KEY_TCP_REMOTE_BUFFER_SIZE,
	KEY_TCP_NO_NAGLE,
	KEY_TCP_LOOPBACK_FAST_PATH,
	KEY_DEFAULT_DB_CACHE_PAGES,
	KEY_CONNECTION_TIMEOUT,
	KEY_DUMMY_PACKET_INTERVAL,
	KEY_DEFAULT_TIME_ZONE,
	KEY_LOCK_MEM_SIZE,
	KEY_LOCK_HASH_SLOTS,
	KEY_LOCK_ACQUIRE_SPINS,
	KEY_EVENT_MEM_SIZE,
	KEY_DEADLOCK_TIMEOUT,
	KEY_REMOTE_SERVICE_NAME,
	KEY_REMOTE_SERVICE_PORT,
	KEY_IPC_NAME,
	KEY_MAX_UNFLUSHED_WRITES,
	KEY_MAX_UNFLUSHED_WRITE_TIME,
	KEY_PROCESS_PRIORITY_LEVEL,
	KEY_REMOTE_AUX_PORT,
	KEY_REMOTE_BIND_ADDRESS,
	KEY_EXTERNAL_FILE_ACCESS,
	KEY_DATABASE_ACCESS,
	KEY_UDF_ACCESS,
	KEY_TEMP_DIRECTORIES,
	KEY_BUGCHECK_ABORT,
	KEY_TRACE_DSQL,
	KEY_LEGACY_HASH,
	KEY_GC_POLICY,
	KEY_REDIRECTION,
	KEY_DATABASE_GROWTH_INCREMENT,
	KEY_FILESYSTEM_CACHE_THRESHOLD,
	KEY_RELAXED_ALIAS_CHECKING,
	KEY_TRACE_CONFIG,
	KEY_MAX_TRACELOG_SIZE,
	KEY_FILESYSTEM_CACHE_SIZE,
	KEY_PLUG_PROVIDERS,
	KEY_PLUG_AUTH_SERVER,
	KEY_PLUG_AUTH_CLIENT,
	KEY_PLUG_AUTH_MANAGE,
	KEY_PLUG_TRACE,
	KEY_SECURITY_DATABASE,
	KEY_SERVER_MODE,
	KEY_WIRE_CRYPT,
	KEY_PLUG_WIRE_CRYPT,
	KEY_PLUG_KEY_HOLDER,
	KEY_REMOTE_ACCESS,
	KEY_IPV6_V6ONLY,
	KEY_WIRE_COMPRESSION,
	KEY_MAX_IDENTIFIER_BYTE_LENGTH,
	KEY_MAX_IDENTIFIER_CHAR_LENGTH,
	KEY_ENCRYPT_SECURITY_DATABASE,
	KEY_STMT_TIMEOUT,
	KEY_CONN_IDLE_TIMEOUT,
	KEY_ON_DISCONNECT_TRIG_TIMEOUT,
	KEY_CLIENT_BATCH_BUFFER,
	KEY_OUTPUT_REDIRECTION_FILE,
	KEY_EXT_CONN_POOL_SIZE,
	KEY_EXT_CONN_POOL_LIFETIME,
	KEY_SNAPSHOTS_MEM_SIZE,
	KEY_TIP_CACHE_BLOCK_SIZE,
	KEY_READ_CONSISTENCY,
	KEY_CLEAR_GTT_RETAINING,
	KEY_DATA_TYPE_COMPATIBILITY,
	KEY_USE_FILESYSTEM_CACHE,
	KEY_INLINE_SORT_THRESHOLD,
	KEY_TEMP_PAGESPACE_DIR,
	KEY_MAX_STATEMENT_CACHE_SIZE,
	KEY_PARALLEL_WORKERS,
	KEY_MAX_PARALLEL_WORKERS,
	MAX_CONFIG_KEY		// keep it last
};


enum ConfigType
{
	TYPE_BOOLEAN,
	TYPE_INTEGER,
	TYPE_STRING
	//TYPE_STRING_VECTOR // CVC: Unused
};

typedef const char* ConfigName;

struct ConfigEntry
{
	ConfigType data_type;
	ConfigName key;
	bool is_global;
	ConfigValue default_value;
};

constexpr ConfigEntry entries[MAX_CONFIG_KEY] =
{
	{TYPE_INTEGER,	"TempBlockSize",			true,	1048576},	// bytes
	{TYPE_INTEGER,	"TempCacheLimit",			false,	-1},		// bytes
	{TYPE_BOOLEAN,	"RemoteFileOpenAbility",	true,	false},
	{TYPE_INTEGER,	"GuardianOption",			true,	true},
	{TYPE_INTEGER,	"CpuAffinityMask",			true,	0},
	{TYPE_INTEGER,	"TcpRemoteBufferSize",		true,	8192},		// bytes
	{TYPE_BOOLEAN,	"TcpNoNagle",				false,	true},
	{TYPE_BOOLEAN,	"TcpLoopbackFastPath",		false,	true},
	{TYPE_INTEGER,	"DefaultDbCachePages",		false,	-1},		// pages
	{TYPE_INTEGER,	"ConnectionTimeout",		false,	180},		// seconds
	{TYPE_INTEGER,	"DummyPacketInterval",		false,	0},			// seconds
	{TYPE_STRING,	"DefaultTimeZone",			true,	nullptr},
	{TYPE_INTEGER,	"LockMemSize",				false,	1048576},	// bytes
	{TYPE_INTEGER,	"LockHashSlots",			false,	8191},		// slots
	{TYPE_INTEGER,	"LockAcquireSpins",			false,	0},
	{TYPE_INTEGER,	"EventMemSize",				false,	65536},		// bytes
	{TYPE_INTEGER,	"DeadlockTimeout",			false,	10},		// seconds
	{TYPE_STRING,	"RemoteServiceName",		false,	FB_SERVICE_NAME},
	{TYPE_INTEGER,	"RemoteServicePort",		false,	0},
	{TYPE_STRING,	"IpcName",					false,	FB_IPC_NAME},
#ifdef WIN_NT
	{TYPE_INTEGER,	"MaxUnflushedWrites",		false,	100},
	{TYPE_INTEGER,	"MaxUnflushedWriteTime",	false,	5},
#else
	{TYPE_INTEGER,	"MaxUnflushedWrites",		false,	-1},
	{TYPE_INTEGER,	"MaxUnflushedWriteTime",	false,	-1},
#endif
	{TYPE_INTEGER,	"ProcessPriorityLevel",		true,	0},
	{TYPE_INTEGER,	"RemoteAuxPort",			false,	0},
	{TYPE_STRING,	"RemoteBindAddress",		true,	0},
	{TYPE_STRING,	"ExternalFileAccess",		false,	"None"},	// locations of external files for tables
	{TYPE_STRING,	"DatabaseAccess",			true,	"Full"},	// locations of databases
	{TYPE_STRING,	"UdfAccess",				true,	"None"},	// locations of UDFs
	{TYPE_STRING,	"TempDirectories",			true,	0},
#ifdef DEV_BUILD
	{TYPE_BOOLEAN,	"BugcheckAbort",			true,	true},	// whether to abort engine when internal error is found
#else
	{TYPE_BOOLEAN,	"BugcheckAbort",			true,	false},	// whether to abort engine when internal error is found
#endif
	{TYPE_INTEGER,	"TraceDSQL",				true,	0},			// bitmask
	{TYPE_BOOLEAN,	"LegacyHash",				true,	true},		// let use old passwd hash verification
	{TYPE_STRING,	"GCPolicy",					false,	nullptr},	// garbage collection policy
	{TYPE_BOOLEAN,	"Redirection",				true,	false},
	{TYPE_INTEGER,	"DatabaseGrowthIncrement",	false,	128 * 1048576},	// bytes
	{TYPE_INTEGER,	"FileSystemCacheThreshold",	false,	65536},		// page buffers
	{TYPE_BOOLEAN,	"RelaxedAliasChecking",		true,	false},		// if true relax strict alias checking rules in DSQL a bit
	{TYPE_STRING,	"AuditTraceConfigFile",		true,	""},		// location of audit trace configuration file
	{TYPE_INTEGER,	"MaxUserTraceLogSize",		true,	10},		// maximum size of user session trace log
	{TYPE_INTEGER,	"FileSystemCacheSize",		true,	0},			// percent
	{TYPE_STRING,	"Providers",				false,	"Remote, " CURRENT_ENGINE ", Loopback"},
	{TYPE_STRING,	"AuthServer",				false,	"Srp256"},
#ifdef WIN_NT
	{TYPE_STRING,	"AuthClient",				false,	"Srp256, Srp, Win_Sspi, Legacy_Auth"},
#else
	{TYPE_STRING,	"AuthClient",				false,	"Srp256, Srp, Legacy_Auth"},
#endif
	{TYPE_STRING,	"UserManager",				false,	"Srp"},
	{TYPE_STRING,	"TracePlugin",				false,	"fbtrace"},
	{TYPE_STRING,	"SecurityDatabase",			false,	nullptr},	// sec/db alias - rely on ConfigManager::getDefaultSecurityDb(
	{TYPE_STRING,	"ServerMode",				true,	nullptr},	// actual value differs in boot/regular cases and set at setupDefaultConfig(
	{TYPE_STRING,	"WireCrypt",				false,	nullptr},
	{TYPE_STRING,	"WireCryptPlugin",			false,	"ChaCha64, ChaCha, Arc4"},
	{TYPE_STRING,	"KeyHolderPlugin",			false,	""},
	{TYPE_BOOLEAN,	"RemoteAccess",				false,	true},
	{TYPE_BOOLEAN,	"IPv6V6Only",				false,	false},
	{TYPE_BOOLEAN,	"WireCompression",			false,	false},
	{TYPE_INTEGER,	"MaxIdentifierByteLength",	false,	(int)MAX_SQL_IDENTIFIER_LEN},
	{TYPE_INTEGER,	"MaxIdentifierCharLength",	false,	(int)METADATA_IDENTIFIER_CHAR_LEN},
	{TYPE_BOOLEAN,	"AllowEncryptedSecurityDatabase",	false,	false},
	{TYPE_INTEGER,	"StatementTimeout",			false,	0},
	{TYPE_INTEGER,	"ConnectionIdleTimeout",	false,	0},
	{TYPE_INTEGER,	"OnDisconnectTriggerTimeout",	false,	180},
	{TYPE_INTEGER,	"ClientBatchBuffer",		false,	128 * 1024},
#ifdef DEV_BUILD
	{TYPE_STRING,	"OutputRedirectionFile", 	true,	"-"},
#else
#ifdef WIN_NT
	{TYPE_STRING,	"OutputRedirectionFile", 	true,	"nul"},
#else
	{TYPE_STRING,	"OutputRedirectionFile", 	true,	"/dev/null"},
#endif
#endif
	{TYPE_INTEGER,	"ExtConnPoolSize",			true,	0},
	{TYPE_INTEGER,	"ExtConnPoolLifeTime",		true,	7200},
	{TYPE_INTEGER,	"SnapshotsMemSize",			false,	65536},		// bytes
	{TYPE_INTEGER,	"TipCacheBlockSize",		false,	4194304},	// bytes
	{TYPE_BOOLEAN,	"ReadConsistency",			false,	true},
	{TYPE_BOOLEAN,	"ClearGTTAtRetaining",		false,	false},
	{TYPE_STRING,	"DataTypeCompatibility",	false,	nullptr},
	{TYPE_BOOLEAN,	"UseFileSystemCache",		false,	true},
	{TYPE_INTEGER,	"InlineSortThreshold",		false,	1000},		// bytes
	{TYPE_STRING,	"TempTableDirectory",		false,	""},
	{TYPE_INTEGER,	"MaxStatementCacheSize",	false,	2 * 1048576},	// bytes
	{TYPE_INTEGER,	"ParallelWorkers",			true,	1},
	{TYPE_INTEGER,	"MaxParallelWorkers",		true,	1}
};


class Config : public RefCounted, public GlobalStorage
{
public:

private:
	static ConfigValue specialProcessing(ConfigKey key, ConfigValue val);

	void loadValues(const ConfigFile& file, const char* srcName);
	void setupDefaultConfig();
	void checkValues();

	// helper check-value functions
	void checkIntForLoBound(ConfigKey key, SINT64 loBound, bool setDefault);
	void checkIntForHiBound(ConfigKey key, SINT64 hiBound, bool setDefault);

	const char* getStr(ConfigKey key, bool* pPresent = nullptr) const
	{
		if (pPresent)
			*pPresent = testKey(key);

		return specialProcessing(key, values[key]).strVal;
	}

	bool getBool(ConfigKey key, bool* pPresent = nullptr) const
	{
		if (pPresent)
			*pPresent = testKey(key);

		return specialProcessing(key, values[key]).boolVal;
	}

	SINT64 getInt(ConfigKey key, bool* pPresent = nullptr) const
	{
		if (pPresent)
			*pPresent = testKey(key);

		return specialProcessing(key, values[key]).intVal;
	}

	static bool valueAsString(ConfigValue val, ConfigType type, string& str);

	static ConfigValue defaults[MAX_CONFIG_KEY];
	ConfigValue values[MAX_CONFIG_KEY];

	// Array of value source names, NULL item is for default value
	HalfStaticArray<const char*, 4> valuesSource;

	// Index of value source, zero if not set
	UCHAR sourceIdx[MAX_CONFIG_KEY];

	// test if given key value was set in config
	bool testKey(unsigned int key) const
	{
		return sourceIdx[key] != 0;
	}

	mutable PathName notifyDatabase;

	// set in default config only
	int serverMode;
	bool defaultConfig;

public:
	explicit Config(const ConfigFile& file);				// use to build default config
	Config(const ConfigFile& file, const char* srcName, const Config& base, const PathName& notify = "");	// use to build db-specific config with notification
	~Config();

	// Call it when database with given config is created

	void notify() const;

	// Check for missing firebird.conf

	static bool missFirebirdConf();

	// Interface to support command line root specification.
	// This ugly solution was required to make it possible to specify root
	// in command line to load firebird.conf from that root, though in other
	// cases firebird.conf may be also used to specify root.

	static void setRootDirectoryFromCommandLine(const PathName& newRoot);
	static const PathName* getCommandLineRootDirectory();

	// Master config - needed to provide per-database config
	static const RefPtr<const Config>& getDefaultConfig();

	// Merge config entries from DPB into existing config
	static void merge(RefPtr<const Config>& config, const string* dpbConfig);

	// reports key to be used by the following functions
	static unsigned int getKeyByName(ConfigName name);
	// helpers to build interface for firebird.conf file
	SINT64 getInt(unsigned int key) const;
	const char* getString(unsigned int key) const;
	bool getBoolean(unsigned int key) const;

	// Number of known keys
	static unsigned int getKeyCount()
	{
		return MAX_CONFIG_KEY;
	}

	static const char* getKeyName(unsigned int key);

	// false if value is null or key is not exists
	bool getValue(unsigned int key, string& str) const;
	static bool getDefaultValue(unsigned int key, string& str);
	// return true if value is set at some level
	bool getIsSet(unsigned int key) const { return testKey(key); }

	const char* getValueSource(unsigned int key) const
	{
		return valuesSource[sourceIdx[key]];
	}

	// Static functions apply to instance-wide values,
	// non-static may be specified per database.

	// Installation directory
	static const char* getInstallDirectory();

	// Root directory of current installation
	static const char* getRootDirectory();


	// CONFIG_GET_GLOBAL_XXX (CONFIG_GET_PER_DB_XXX) set of macros helps to
	// create trivial static (non-static) getXXX functions.
	// Correctness of declaration and implementation is enforced with help
	// of entries[XXX].is_global.

#define CONFIG_GET_GLOBAL_KEY(T, FN, KEY, SUBFN) \
	static T FN() \
	{ \
		static_assert(entries[KEY].is_global, "Requires global key"); \
		static_assert(!std::is_member_function_pointer<decltype(&Config::FN)>::value, "Must be a static function");  \
		return (T) getDefaultConfig()->SUBFN(KEY); \
	}

#define CONFIG_GET_PER_DB_KEY(T, FN, KEY, SUBFN) \
	T FN() const \
	{ \
		static_assert(!entries[KEY].is_global, "Requires per-database key"); \
		static_assert(std::is_member_function_pointer<decltype(&Config::FN)>::value, "Must be a non-static function");  \
		return (T) this->SUBFN(KEY); \
	}

#define CONFIG_GET_GLOBAL_INT(FN, KEY)	CONFIG_GET_GLOBAL_KEY(int, FN, KEY, getInt)
#define CONFIG_GET_GLOBAL_STR(FN, KEY)	CONFIG_GET_GLOBAL_KEY(const char*, FN, KEY, getStr)
#define CONFIG_GET_GLOBAL_BOOL(FN, KEY)	CONFIG_GET_GLOBAL_KEY(bool, FN, KEY, getBool)

#define CONFIG_GET_PER_DB_INT(FN, KEY)	CONFIG_GET_PER_DB_KEY(int, FN, KEY, getInt)
#define CONFIG_GET_PER_DB_STR(FN, KEY)	CONFIG_GET_PER_DB_KEY(const char*, FN, KEY, getStr)
#define CONFIG_GET_PER_DB_BOOL(FN, KEY)	CONFIG_GET_PER_DB_KEY(bool, FN, KEY, getBool)


	// Allocation chunk for the temporary spaces
	CONFIG_GET_GLOBAL_INT(getTempBlockSize, KEY_TEMP_BLOCK_SIZE);

	// Caching limit for the temporary data
	CONFIG_GET_PER_DB_KEY(FB_UINT64, getTempCacheLimit, KEY_TEMP_CACHE_LIMIT, getInt);

	// Whether remote (NFS) files can be opened
	CONFIG_GET_GLOBAL_BOOL(getRemoteFileOpenAbility, KEY_REMOTE_FILE_OPEN_ABILITY);

	// Startup option for the guardian
	CONFIG_GET_GLOBAL_INT(getGuardianOption, KEY_GUARDIAN_OPTION);

	// CPU affinity mask
	CONFIG_GET_GLOBAL_KEY(FB_UINT64, getCpuAffinityMask, KEY_CPU_AFFINITY_MASK, getInt);

	// XDR buffer size
	CONFIG_GET_GLOBAL_INT(getTcpRemoteBufferSize, KEY_TCP_REMOTE_BUFFER_SIZE);

	// Disable Nagle algorithm
	CONFIG_GET_PER_DB_BOOL(getTcpNoNagle, KEY_TCP_NO_NAGLE);

	// Enable or disable the TCP Loopback Fast Path option
	CONFIG_GET_PER_DB_BOOL(getTcpLoopbackFastPath, KEY_TCP_LOOPBACK_FAST_PATH);

	// Let IPv6 socket accept only IPv6 packets
	CONFIG_GET_PER_DB_BOOL(getIPv6V6Only, KEY_IPV6_V6ONLY);

	// Default database cache size
	CONFIG_GET_PER_DB_INT(getDefaultDbCachePages, KEY_DEFAULT_DB_CACHE_PAGES);

	// Connection timeout
	CONFIG_GET_PER_DB_INT(getConnectionTimeout, KEY_CONNECTION_TIMEOUT);

	// Dummy packet interval
	CONFIG_GET_PER_DB_INT(getDummyPacketInterval, KEY_DUMMY_PACKET_INTERVAL);

	CONFIG_GET_GLOBAL_STR(getDefaultTimeZone, KEY_DEFAULT_TIME_ZONE);

	// Lock manager memory size
	CONFIG_GET_PER_DB_INT(getLockMemSize, KEY_LOCK_MEM_SIZE);

	// Lock manager hash slots
	CONFIG_GET_PER_DB_INT(getLockHashSlots, KEY_LOCK_HASH_SLOTS);

	// Lock manager acquire spins
	CONFIG_GET_PER_DB_INT(getLockAcquireSpins, KEY_LOCK_ACQUIRE_SPINS);

	// Event manager memory size
	CONFIG_GET_PER_DB_INT(getEventMemSize, KEY_EVENT_MEM_SIZE);

	// Deadlock timeout
	CONFIG_GET_PER_DB_INT(getDeadlockTimeout, KEY_DEADLOCK_TIMEOUT);

	// Service name for remote protocols
	CONFIG_GET_PER_DB_STR(getRemoteServiceName, KEY_REMOTE_SERVICE_NAME);

	// Service port for INET
	CONFIG_GET_PER_DB_KEY(unsigned short, getRemoteServicePort, KEY_REMOTE_SERVICE_PORT, getInt);

	// Name for IPC-related objects
	CONFIG_GET_PER_DB_STR(getIpcName, KEY_IPC_NAME);

	// Unflushed writes number
	CONFIG_GET_PER_DB_INT(getMaxUnflushedWrites, KEY_MAX_UNFLUSHED_WRITES);

	// Unflushed write time
	CONFIG_GET_PER_DB_INT(getMaxUnflushedWriteTime, KEY_MAX_UNFLUSHED_WRITE_TIME);

	// Process priority level
	CONFIG_GET_GLOBAL_INT(getProcessPriorityLevel, KEY_PROCESS_PRIORITY_LEVEL);

	// Port for event processing
	CONFIG_GET_PER_DB_INT(getRemoteAuxPort, KEY_REMOTE_AUX_PORT);

	// Server binding NIC address
	CONFIG_GET_GLOBAL_STR(getRemoteBindAddress, KEY_REMOTE_BIND_ADDRESS);

	// Directory list for external tables
	CONFIG_GET_PER_DB_STR(getExternalFileAccess, KEY_EXTERNAL_FILE_ACCESS);

	// Directory list for databases
	CONFIG_GET_GLOBAL_STR(getDatabaseAccess, KEY_DATABASE_ACCESS);

	// Directory list for UDF libraries
	CONFIG_GET_GLOBAL_STR(getUdfAccess, KEY_UDF_ACCESS);

	// Temporary directories list
	CONFIG_GET_GLOBAL_STR(getTempDirectories, KEY_TEMP_DIRECTORIES);

	// DSQL trace bitmask
	CONFIG_GET_GLOBAL_INT(getTraceDSQL, KEY_TRACE_DSQL);

	// Abort on BUGCHECK and structured exceptions
	CONFIG_GET_GLOBAL_BOOL(getBugcheckAbort, KEY_BUGCHECK_ABORT);

	// Let use of des hash to verify passwords
	CONFIG_GET_GLOBAL_BOOL(getLegacyHash, KEY_LEGACY_HASH);

	// GC policy
	CONFIG_GET_PER_DB_STR(getGCPolicy, KEY_GC_POLICY);

	// Redirection
	CONFIG_GET_GLOBAL_BOOL(getRedirection, KEY_REDIRECTION);

	CONFIG_GET_PER_DB_INT(getDatabaseGrowthIncrement, KEY_DATABASE_GROWTH_INCREMENT);

	CONFIG_GET_PER_DB_INT(getFileSystemCacheThreshold, KEY_FILESYSTEM_CACHE_THRESHOLD);

	CONFIG_GET_GLOBAL_KEY(FB_UINT64, getFileSystemCacheSize, KEY_FILESYSTEM_CACHE_SIZE, getInt);

	CONFIG_GET_GLOBAL_BOOL(getRelaxedAliasChecking, KEY_RELAXED_ALIAS_CHECKING);

	CONFIG_GET_GLOBAL_STR(getAuditTraceConfigFile, KEY_TRACE_CONFIG);

	CONFIG_GET_GLOBAL_KEY(FB_UINT64, getMaxUserTraceLogSize, KEY_MAX_TRACELOG_SIZE, getInt);

	static int getServerMode();

	const char* getPlugins(unsigned int type) const;

	CONFIG_GET_PER_DB_STR(getSecurityDatabase, KEY_SECURITY_DATABASE);

	int getWireCrypt(WireCryptMode wcMode) const;

	CONFIG_GET_PER_DB_BOOL(getRemoteAccess, KEY_REMOTE_ACCESS);

	CONFIG_GET_PER_DB_BOOL(getWireCompression, KEY_WIRE_COMPRESSION);

	CONFIG_GET_PER_DB_INT(getMaxIdentifierByteLength, KEY_MAX_IDENTIFIER_BYTE_LENGTH);

	CONFIG_GET_PER_DB_INT(getMaxIdentifierCharLength, KEY_MAX_IDENTIFIER_CHAR_LENGTH);

	CONFIG_GET_PER_DB_BOOL(getCryptSecurityDatabase, KEY_ENCRYPT_SECURITY_DATABASE);

	// set in seconds
	CONFIG_GET_PER_DB_KEY(unsigned int, getStatementTimeout, KEY_STMT_TIMEOUT, getInt);

	// set in minutes
	CONFIG_GET_PER_DB_KEY(unsigned int, getConnIdleTimeout, KEY_CONN_IDLE_TIMEOUT, getInt);

	// set in seconds
	CONFIG_GET_PER_DB_KEY(unsigned int, getOnDisconnectTrigTimeout, KEY_ON_DISCONNECT_TRIG_TIMEOUT, getInt);

	CONFIG_GET_PER_DB_KEY(unsigned int, getClientBatchBuffer, KEY_CLIENT_BATCH_BUFFER, getInt);

	CONFIG_GET_GLOBAL_STR(getOutputRedirectionFile, KEY_OUTPUT_REDIRECTION_FILE);

	CONFIG_GET_GLOBAL_INT(getExtConnPoolSize, KEY_EXT_CONN_POOL_SIZE);

	CONFIG_GET_GLOBAL_INT(getExtConnPoolLifeTime, KEY_EXT_CONN_POOL_LIFETIME);

	CONFIG_GET_PER_DB_KEY(ULONG, getSnapshotsMemSize, KEY_SNAPSHOTS_MEM_SIZE, getInt);

	CONFIG_GET_PER_DB_KEY(ULONG, getTipCacheBlockSize, KEY_TIP_CACHE_BLOCK_SIZE, getInt);

	CONFIG_GET_PER_DB_BOOL(getReadConsistency, KEY_READ_CONSISTENCY);

	CONFIG_GET_PER_DB_BOOL(getClearGTTAtRetaining, KEY_CLEAR_GTT_RETAINING);

	CONFIG_GET_PER_DB_STR(getDataTypeCompatibility, KEY_DATA_TYPE_COMPATIBILITY);

	bool getUseFileSystemCache(bool* pPresent = nullptr) const;

	CONFIG_GET_PER_DB_KEY(ULONG, getInlineSortThreshold, KEY_INLINE_SORT_THRESHOLD, getInt);

	CONFIG_GET_PER_DB_STR(getTempPageSpaceDirectory, KEY_TEMP_PAGESPACE_DIR);

	CONFIG_GET_PER_DB_INT(getMaxStatementCacheSize, KEY_MAX_STATEMENT_CACHE_SIZE);

	CONFIG_GET_GLOBAL_INT(getParallelWorkers, KEY_PARALLEL_WORKERS);

	CONFIG_GET_GLOBAL_INT(getMaxParallelWorkers, KEY_MAX_PARALLEL_WORKERS);
};

// Implementation of interface to access master configuration file
class FirebirdConf final :
	public RefCntIface<IFirebirdConfImpl<FirebirdConf, CheckStatusWrapper> >
{
public:
	FirebirdConf(const Config* existingConfig)
		: config(existingConfig)
	{ }

	// IFirebirdConf implementation
	unsigned int getKey(const char* name);
	SINT64 asInteger(unsigned int key);
	const char* asString(unsigned int key);
	FB_BOOLEAN asBoolean(unsigned int key);
	unsigned int getVersion(CheckStatusWrapper* status);

private:
	RefPtr<const Config> config;
};

// Create default instance of IFirebirdConf interface
IFirebirdConf* getFirebirdConfig();

} // namespace Firebird

#endif // COMMON_CONFIG_H
