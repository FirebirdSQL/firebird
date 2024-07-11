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
#include "../common/classes/init.h"
#include "../common/config/config.h"
#include "../common/config/config_file.h"
#include "../common/config/ConfigCache.h"
#include "../common/config/dir_list.h"
#include "../common/os/path_utils.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../common/utils_proto.h"
#include "../common/classes/Hash.h"
#include "../common/isc_f_proto.h"
#include "../common/StatusArg.h"
#include "../common/os/os_utils.h"
#include "../common/classes/array.h"
#include <ctype.h>

using namespace Firebird;

namespace
{
	class DatabaseDirectoryList : public DirectoryList
	{
	private:
		const PathName getConfigString() const
		{
			return PathName(Config::getDatabaseAccess());
		}
	public:
		explicit DatabaseDirectoryList(MemoryPool& p)
			: DirectoryList(p)
		{
			initialize();
		}
	};
	InitInstance<DatabaseDirectoryList> databaseDirectoryList;

	const char* const ALIAS_FILE = "databases.conf";

	struct CalcHash
	{
		static FB_SIZE_T chash(FB_SIZE_T sum, FB_SIZE_T hashSize)
		{
			FB_SIZE_T rc = 0;
			while (sum)
			{
				rc += (sum % hashSize);
				sum /= hashSize;
			}

			return rc % hashSize;
		}
	};


	template <typename T>
	struct PathHash : public CalcHash
	{
		static const PathName& generate(const T& item)
		{
			return item.name;
		}

		static FB_SIZE_T hash(const PathName& value, FB_SIZE_T hashSize)
		{
			return hash(value.c_str(), value.length(), hashSize);
		}

		static void upcpy(FB_SIZE_T* toPar, const char* from, FB_SIZE_T length)
		{
			char* to = reinterpret_cast<char*>(toPar);
			while (length--)
			{
				if (CASE_SENSITIVITY)
				{
					*to++ = *from++;
				}
				else
				{
					*to++ = toupper(*from++);
				}
			}
		}

		static FB_SIZE_T hash(const char* value, FB_SIZE_T length, FB_SIZE_T hashSize)
		{
			FB_SIZE_T sum = 0;
			FB_SIZE_T val;

			while (length >= sizeof(FB_SIZE_T))
			{
				upcpy(&val, value, sizeof(FB_SIZE_T));
				sum += val;
				value += sizeof(FB_SIZE_T);
				length -= sizeof(FB_SIZE_T);
			}

			if (length)
			{
				val = 0;
				upcpy(&val, value, length);
				sum += val;
			}

			return chash(sum, hashSize);
		}
	};

#ifdef HAVE_ID_BY_NAME
	template <typename T>
	struct BinHash : public CalcHash
	{
		static const UCharBuffer& generate(const T& item)
		{
			return item.id;
		}

		static FB_SIZE_T hash(const UCharBuffer& value, FB_SIZE_T hashSize)
		{
			return hash(value.begin(), value.getCount(), hashSize);
		}

		static FB_SIZE_T hash(const UCHAR* value, FB_SIZE_T length, FB_SIZE_T hashSize)
		{
			FB_SIZE_T sum = 0;
			FB_SIZE_T val;

			while (length >= sizeof(FB_SIZE_T))
			{
				memcpy(&val, value, sizeof(FB_SIZE_T));
				sum += val;
				value += sizeof(FB_SIZE_T);
				length -= sizeof(FB_SIZE_T);
			}

			if (length)
			{
				val = 0;
				memcpy(&val, value, length);
				sum += val;
			}

			return chash(sum, hashSize);
		}
	};
#endif

	struct DbName;
#ifdef HAVE_ID_BY_NAME
	struct Id;
	typedef HashTable<Id, 127, UCharBuffer, BinHash<Id>, BinHash<Id> > IdHash;

	struct Id : public IdHash::Entry
	{
		Id(MemoryPool& p, const UCharBuffer& x, DbName* d)
			: id(p, x), db(d)
		{ }

		Id* get()
		{
			return this;
		}

		bool isEqual(const UCharBuffer& val) const
		{
			return val == id;
		}

		UCharBuffer id;
		DbName* db;
	};
#endif

	typedef HashTable<DbName, 127, PathName, PathHash<DbName>, PathHash<DbName> > DbHash;
	struct DbName : public DbHash::Entry
	{
		DbName(MemoryPool& p, const PathName& db)
			: name(p, db)
#ifdef HAVE_ID_BY_NAME
			  , id(NULL)
#endif
		{ }

		DbName* get()
		{
			return this;
		}

		bool isEqual(const PathName& val) const
		{
			return val == name;
		}

		PathName name;
		RefPtr<const Config> config;
#ifdef HAVE_ID_BY_NAME
		Id* id;
#endif
	};

	struct AliasName;
	typedef HashTable<AliasName, 251, PathName, PathHash<AliasName>, PathHash<AliasName> > AliasHash;

	struct AliasName : public AliasHash::Entry
	{
		AliasName(MemoryPool& p, const PathName& al, DbName* db)
			: name(p, al), database(db)
		{ }

		AliasName* get()
		{
			return this;
		}

		bool isEqual(const PathName& val) const
		{
			return val == name;
		}

		PathName name;
		DbName* database;
	};

	class AliasesConf : public ConfigCache
	{
	public:
		explicit AliasesConf(MemoryPool& p)
			: ConfigCache(p, fb_utils::getPrefix(Firebird::IConfigManager::DIR_CONF, ALIAS_FILE)),
			  databases(getPool()), aliases(getPool())
#ifdef HAVE_ID_BY_NAME
			  , ids(getPool())
#endif
		{ }

		~AliasesConf()
		{
			clear();
		}

		void loadConfig()
		{
			clear();

			PathName confName = getFileName();
			const char* rootDir = Config::getRootDirectory();
			const FB_SIZE_T rootLen = strlen(rootDir);

			if ((confName.length() > rootLen) && (confName.compare(0, rootLen, rootDir, rootLen) == 0))
				confName.erase(0, rootLen);

			ConfigFile aliasConfig(getFileName(), ConfigFile::HAS_SUB_CONF, this);
			const ConfigFile::Parameters& params = aliasConfig.getParameters();

			for (FB_SIZE_T n = 0; n < params.getCount(); ++n)
			{
				const ConfigFile::Parameter* par = &params[n];

				PathName file(par->value.ToPathName());
				PathUtils::fixupSeparators(file);
				if (PathUtils::isRelative(file) && !ISC_check_if_remote(file, false))
				{
					gds__log("Value %s configured for alias %s "
						"is not a fully qualified path name nor remote server reference, ignored",
								file.c_str(), par->name.c_str());
					continue;
				}
				DbName* db = dbHash.lookup(file);
				if (! db)
				{
					db = FB_NEW_POOL(getPool()) DbName(getPool(), file);
#ifdef HAVE_ID_BY_NAME
					UCharBuffer id;
					os_utils::getUniqueFileId(db->name.c_str(), id);
					if (id.hasData())
					{
						Id* i = idHash.lookup(id);
						if (i)
						{
							delete db;
							fatal_exception::raiseFmt("Same ID for databases %s and %s\n",
													  file.c_str(), i->db->name.c_str());
						}

						linkId(db, id);
					}
#endif
					databases.add(db);
					dbHash.add(db);
				}
				else
				{
					// check for duplicated config
					if (par->sub && db->config.hasData())
					{
						fatal_exception::raiseFmt("Duplicated configuration for database %s\n",
												  file.c_str());
					}
				}
				if (par->sub)
				{
					// load per-database configuration
					db->config =
#ifdef HAVE_ID_BY_NAME
						(!db->id) ?
							FB_NEW Config(*par->sub, confName.c_str(), *Config::getDefaultConfig(), db->name) :
#endif
							FB_NEW Config(*par->sub, confName.c_str(), *Config::getDefaultConfig());
				}

				PathName correctedAlias(par->name.ToPathName());
				PathUtils::fixupSeparators(correctedAlias);
				AliasName* alias = aliasHash.lookup(correctedAlias);
				if (alias)
				{
					fatal_exception::raiseFmt("Duplicated alias %s\n", correctedAlias.c_str());
				}

				alias = FB_NEW_POOL(getPool()) AliasName(getPool(), correctedAlias, db);
				aliases.add(alias);
				aliasHash.add(alias);
			}
		}

#ifdef HAVE_ID_BY_NAME
		void linkId(DbName* db, const UCharBuffer& id)
		{
			fb_assert(!db->id);

			Id* i = FB_NEW_POOL(getPool()) Id(getPool(), id, db);
			ids.add(i);
			idHash.add(i);
			db->id = i;
		}
#endif

	private:
		void clear()
		{
			FB_SIZE_T n;

			// clean old data
			for (n = 0; n < aliases.getCount(); ++n)
			{
				delete aliases[n];
			}
			aliases.clear();

			for (n = 0; n < databases.getCount(); ++n)
			{
				delete databases[n];
			}
			databases.clear();

#ifdef HAVE_ID_BY_NAME
			for (n = 0; n < ids.getCount(); ++n)
			{
				delete ids[n];
			}
			ids.clear();
#endif
		}

		HalfStaticArray<DbName*, 100> databases;
		HalfStaticArray<AliasName*, 200> aliases;
#ifdef HAVE_ID_BY_NAME
		HalfStaticArray<Id*, 100> ids;
#endif

	public:
		DbHash dbHash;
		AliasHash aliasHash;
#ifdef HAVE_ID_BY_NAME
		IdHash idHash;
#endif
	};

	InitInstance<AliasesConf> aliasesConf;
}

// Checks that argument doesn't contain colon or directory separator
static inline bool hasSeparator(const PathName& name)
{
	for (const char* p = name.c_str(); *p; p++)
	{
		if (*p == ':' || *p == '/' || *p == '\\')
			return true;
	}
	return false;
}

// Search for 'alias' in databases.conf, return its value in 'file' if found.
// Returns true if alias is found in databases.conf.
bool resolveAlias(const PathName& alias, PathName& file, RefPtr<const Config>* config)
{
	PathName correctedAlias = alias;
	PathUtils::fixupSeparators(correctedAlias);

	AliasName* a = aliasesConf().aliasHash.lookup(correctedAlias);
	DbName* db = a ? a->database : NULL;
	if (db)
	{
		file = db->name;
		if (config)
		{
			*config = db->config.hasData() ? db->config : Config::getDefaultConfig();
		}

		return true;
	}

	return false;
}

// Search for filenames, containing no path component, in dirs from DatabaseAccess list
// of firebird.conf. If not found try first entry in that list as default entry.
// Returns true if expanded successfully.
static bool resolveDatabaseAccess(const PathName& alias, PathName& file)
{
	file = alias;

	if (hasSeparator(alias))
		return false;

	// try to expand to existing file
	if (!databaseDirectoryList().expandFileName(file, alias))
	{
		// try to use default path
		if (!databaseDirectoryList().defaultName(file, alias))
		{
			return false;
		}
	}

	return true;
}

// Set a prefix to a filename based on the ISC_PATH user variable.
// Returns true if database name is expanded using ISC_PATH.
static bool setPath(const PathName& filename, PathName& expandedName)
{
	// Look for the environment variables to tack onto the beginning of the database path.
	PathName pathname;
	if (!fb_utils::readenv("ISC_PATH", pathname))
		return false;

	// If the file already contains a remote node or any path at all forget it.
	if (hasSeparator(filename))
		return false;

	// concatenate the strings

	expandedName = pathname;

	// CVC: Make the concatenation work if no slash is present.
	char lastChar = expandedName[expandedName.length() - 1];
	if (lastChar != ':' && lastChar != '/' && lastChar != '\\')
		expandedName.append(1, PathUtils::dir_sep);

	expandedName.append(filename);

	return true;
}

// Full processing of database name
// Returns true if alias was found in databases.conf
bool expandDatabaseName(Firebird::PathName alias,
						Firebird::PathName& file,
						Firebird::RefPtr<const Config>* config)
{
	try
	{
		aliasesConf().checkLoadConfig();
	}
	catch (const fatal_exception& ex)
	{
		gds__log("File databases.conf contains bad data: %s", ex.what());
		Arg::Gds(isc_server_misconfigured).raise();
	}

	// remove whitespaces from database name
	alias.trim();

	ReadLockGuard guard(aliasesConf().rwLock, "expandDatabaseName");

	// First of all check in databases.conf
	if (resolveAlias(alias, file, config))
	{
		return true;
	}

	// Now try ISC_PATH environment variable
	if (!setPath(alias, file))
	{
		// At this step check DatabaseAccess paths in firebird.conf
		if (!resolveDatabaseAccess(alias, file))
		{
			// Last chance - regular filename expansion
			file = alias;

			ISC_systemToUtf8(file);
			ISC_unescape(file);
			ISC_utf8ToSystem(file);

			ISC_expand_filename(file, true);

			ISC_systemToUtf8(file);
			ISC_escape(file);
			ISC_utf8ToSystem(file);
		}
	}

	// Search for correct config in databases.conf
	if (config)
	{
		DbName* db = aliasesConf().dbHash.lookup(file);
#ifdef HAVE_ID_BY_NAME
		if (!db)
		{
			UCharBuffer id;
			os_utils::getUniqueFileId(file.c_str(), id);
			if (id.hasData())
			{
				Id* i = aliasesConf().idHash.lookup(id);
				if (i)
				{
					UCharBuffer oldId;
					os_utils::getUniqueFileId(i->db->name.c_str(), oldId);
					if (oldId == id)	// Yes, that's really same file, and we should use same config
						db = i->db;
				}
			}
		}
#endif
		*config = (db && db->config.hasData()) ? db->config : Config::getDefaultConfig();
	}

	return false;
}

// Probably file arrived on the disk
bool notifyDatabaseName(const Firebird::PathName& file)
{
#ifdef HAVE_ID_BY_NAME
	// notifyDatabaseName typically causes changes in aliasesConf()
	// cause it's called only from Config created for missing database.
	// Therefore always take write lock at once.
	WriteLockGuard guard(aliasesConf().rwLock, "notifyDatabaseName");

	DbName* db = aliasesConf().dbHash.lookup(file);
	if (!db)
		return true;
	if (db->id)
		return true;

	UCharBuffer id;
	os_utils::getUniqueFileId(file.c_str(), id);
	if (id.hasData())
	{
		aliasesConf().linkId(db, id);
		return true;
	}
#endif

	return false;
}
