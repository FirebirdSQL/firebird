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
 *  Copyright (c) 2014 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/config/config_file.h"
#include "../common/os/path_utils.h"
#include "../common/isc_f_proto.h"
#include "../common/status.h"
#include "../common/StatusArg.h"
#include "../jrd/constants.h"

#include "Utils.h"
#include "Config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include <stdlib.h>
#include <time.h>

using namespace Firebird;
using namespace Replication;

namespace
{
	const char* REPLICATION_CFGFILE = "replication.conf";

	const ULONG DEFAULT_BUFFER_SIZE = 1024 * 1024; 				// 1 MB
	const ULONG DEFAULT_SEGMENT_SIZE = 16 * 1024 * 1024;	// 16 MB
	const ULONG DEFAULT_SEGMENT_COUNT = 8;
	const ULONG DEFAULT_ARCHIVE_TIMEOUT = 60;				// seconds
	const ULONG DEFAULT_GROUP_FLUSH_DELAY = 0;
	const ULONG DEFAULT_APPLY_IDLE_TIMEOUT = 10;				// seconds
	const ULONG DEFAULT_APPLY_ERROR_TIMEOUT = 60;				// seconds

	void parseLong(const string& input, ULONG& output)
	{
		char* tail = nullptr;
		auto number = strtol(input.c_str(), &tail, 10);
		if (tail && *tail == 0 && number > 0)
			output = (ULONG) number;
	}

	void parseBoolean(const string& input, bool& output)
	{
		if (input == "true" || input == "yes" || input == "on" || input == "1")
			output = true;
		else if (input == "false" || input == "no" || input == "off" || input == "0")
			output = false;
	}

	void configError(const string& type, const string& key, const string& value)
	{
		string msg;
		msg.printf("%s specifies %s: %s", key.c_str(), type.c_str(), value.c_str());
		raiseError(msg.c_str());
	}

	void checkAccess(const PathName& path, const string& key)
	{
		if (path.hasData() && !PathUtils::canAccess(path, 6))
			configError("missing or inaccessible directory", key, path.c_str());
	}

	void composeError(CheckStatusWrapper* status, const Exception& ex)
	{
		string prefix;
		prefix.printf("Incorrect entry in %s", REPLICATION_CFGFILE);

		Arg::StatusVector sv;
		sv << Arg::Gds(isc_random) << Arg::Str(prefix);
		sv << Arg::StatusVector(ex);

		status->setErrors(sv.value());
	}
}


// Replication::Config class

Config::Config()
	: dbName(getPool()),
	  bufferSize(DEFAULT_BUFFER_SIZE),
	  includeFilter(getPool()),
	  excludeFilter(getPool()),
	  segmentSize(DEFAULT_SEGMENT_SIZE),
	  segmentCount(DEFAULT_SEGMENT_COUNT),
	  journalDirectory(getPool()),
	  filePrefix(getPool()),
	  groupFlushDelay(DEFAULT_GROUP_FLUSH_DELAY),
	  archiveDirectory(getPool()),
	  archiveCommand(getPool()),
	  archiveTimeout(DEFAULT_ARCHIVE_TIMEOUT),
	  syncReplicas(getPool()),
	  sourceDirectory(getPool()),
	  sourceGuid{},
	  verboseLogging(false),
	  applyIdleTimeout(DEFAULT_APPLY_IDLE_TIMEOUT),
	  applyErrorTimeout(DEFAULT_APPLY_ERROR_TIMEOUT),
	  pluginName(getPool()),
	  logErrors(true),
	  reportErrors(false),
	  disableOnError(true)
{
}

Config::Config(const Config& other)
	: dbName(getPool(), other.dbName),
	  bufferSize(other.bufferSize),
	  includeFilter(getPool(), other.includeFilter),
	  excludeFilter(getPool(), other.excludeFilter),
	  segmentSize(other.segmentSize),
	  segmentCount(other.segmentCount),
	  journalDirectory(getPool(), other.journalDirectory),
	  filePrefix(getPool(), other.filePrefix),
	  groupFlushDelay(other.groupFlushDelay),
	  archiveDirectory(getPool(), other.archiveDirectory),
	  archiveCommand(getPool(), other.archiveCommand),
	  archiveTimeout(other.archiveTimeout),
	  syncReplicas(getPool(), other.syncReplicas),
	  sourceDirectory(getPool(), other.sourceDirectory),
	  sourceGuid{},
	  verboseLogging(other.verboseLogging),
	  applyIdleTimeout(other.applyIdleTimeout),
	  applyErrorTimeout(other.applyErrorTimeout),
	  pluginName(getPool(), other.pluginName),
	  logErrors(other.logErrors),
	  reportErrors(other.reportErrors),
	  disableOnError(other.disableOnError)
{
}

// This routine is used to match the database on the master side.
// Therefore it checks only the necessary settings.

Config* Config::get(const PathName& lookupName)
{
	fb_assert(lookupName.hasData());

	try
	{
		const PathName filename =
			fb_utils::getPrefix(IConfigManager::DIR_CONF, REPLICATION_CFGFILE);

		MemoryPool& pool = *getDefaultMemoryPool();

		ConfigFile cfgFile(filename, ConfigFile::HAS_SUB_CONF |
									 ConfigFile::NATIVE_ORDER |
									 ConfigFile::CUSTOM_MACROS);

		AutoPtr<Config> config(FB_NEW Config);

		bool defaultFound = false, exactMatch = false;

		for (const auto& section : cfgFile.getParameters())
		{
			if (section.name != "database")
				continue;

			PathName dbName(section.value.c_str());

			if (dbName.empty())
			{
				if (defaultFound)
					raiseError("Only one default DATABASE section is allowed");

				defaultFound = true;
			}
			else
			{
				PathUtils::fixupSeparators(dbName);
				ISC_expand_filename(dbName, true);

				if (dbName != lookupName)
					continue;

				config->dbName = dbName;

				exactMatch = true;
			}

			if (!section.sub)
				continue;

			for (const auto& el : section.sub->getParameters())
			{
				const string key(el.name.c_str());
				string value(el.value);

				if (value.isEmpty())
					continue;

				if (key == "sync_replica")
				{
					config->syncReplicas.add(value);
				}
				else if (key == "buffer_size")
				{
					parseLong(value, config->bufferSize);
				}
				else if (key == "include_filter")
				{
					ISC_systemToUtf8(value);
					config->includeFilter = value;
				}
				else if (key == "exclude_filter")
				{
					ISC_systemToUtf8(value);
					config->excludeFilter = value;
				}
				else if (key == "journal_segment_size")
				{
					parseLong(value, config->segmentSize);
				}
				else if (key == "journal_segment_count")
				{
					parseLong(value, config->segmentCount);
				}
				else if (key == "journal_directory")
				{
					config->journalDirectory = value.c_str();
					PathUtils::ensureSeparator(config->journalDirectory);
					checkAccess(config->journalDirectory, key);
				}
				else if (key == "journal_file_prefix")
				{
					config->filePrefix = value.c_str();
				}
				else if (key == "journal_group_flush_delay")
				{
					parseLong(value, config->groupFlushDelay);
				}
				else if (key == "journal_archive_directory")
				{
					config->archiveDirectory = value.c_str();
					PathUtils::ensureSeparator(config->archiveDirectory);
					checkAccess(config->archiveDirectory, key);
				}
				else if (key == "journal_archive_command")
				{
					config->archiveCommand = value.c_str();
				}
				else if (key == "journal_archive_timeout")
				{
					parseLong(value, config->archiveTimeout);
				}
				else if (key == "plugin")
				{
					config->pluginName = value;
				}
				else if (key == "log_errors")
				{
					parseBoolean(value, config->logErrors);
				}
				else if (key == "report_errors")
				{
					parseBoolean(value, config->reportErrors);
				}
				else if (key == "disable_on_error")
				{
					parseBoolean(value, config->disableOnError);
				}
			}

			if (exactMatch)
				break;
		}

		// TODO: As soon as plugin name is moved into RDB$PUBLICATIONS,
		// delay config parse until real replication start
		if (config->pluginName.hasData())
			return config.release();

		if (config->journalDirectory.hasData() || config->syncReplicas.hasData())
		{
			// If either journal_directory or sync_replicas is specified,
			// then replication is enabled

			if (config->dbName.isEmpty())
				config->dbName = lookupName;

			if (config->filePrefix.isEmpty())
			{
				PathName db_directory, db_filename;
				PathUtils::splitLastComponent(db_directory, db_filename, config->dbName);
				config->filePrefix = db_filename;
			}

			return config.release();
		}
	}
	catch (const Exception& ex)
	{
		FbLocalStatus localStatus;
		composeError(&localStatus, ex);

		logPrimaryStatus(lookupName, &localStatus);
	}

	return nullptr;
}

// This routine is used to retrieve the list of replica databases.
// Therefore it checks only the necessary settings.

void Config::enumerate(Firebird::Array<Config*>& replicas)
{
	PathName dbName;

	try
	{
		const PathName filename =
			fb_utils::getPrefix(IConfigManager::DIR_CONF, REPLICATION_CFGFILE);

		MemoryPool& pool = *getDefaultMemoryPool();

		ConfigFile cfgFile(filename, ConfigFile::HAS_SUB_CONF |
									 ConfigFile::NATIVE_ORDER |
									 ConfigFile::CUSTOM_MACROS);

		AutoPtr<Config> defConfig(FB_NEW Config);

		bool defaultFound = false, exactMatch = false;

		for (const auto& section : cfgFile.getParameters())
		{
			if (section.name != "database")
				continue;

			AutoPtr<Config> dbConfig;
			Config* config = nullptr;

			dbName = section.value.c_str();

			if (dbName.empty())
			{
				if (defaultFound)
					raiseError("Only one default DATABASE section is allowed");

				defaultFound = true;
				config = defConfig;
			}
			else
			{
				config = dbConfig = FB_NEW Config(*defConfig);
			}

			if (!section.sub)
				continue;

			for (const auto& el : section.sub->getParameters())
			{
				const string key(el.name.c_str());
				string value(el.value);

				if (value.isEmpty())
					continue;

				if (key == "journal_source_directory")
				{
					config->sourceDirectory = value.c_str();
					PathUtils::ensureSeparator(config->sourceDirectory);
				}
				else if (key == "source_guid")
				{
					if (!StringToGuid(&config->sourceGuid, value.c_str()))
						configError("invalid (misformatted) value", key, value);
				}
				else if (key == "verbose_logging")
				{
					parseBoolean(value, config->verboseLogging);
				}
				else if (key == "apply_idle_timeout")
				{
					parseLong(value, config->applyIdleTimeout);
				}
				else if (key == "apply_error_timeout")
				{
					parseLong(value, config->applyErrorTimeout);
				}
			}

			if (dbName.hasData() && config->sourceDirectory.hasData())
			{
				// If source_directory is specified, then replication is enabled

				PathUtils::fixupSeparators(dbName);
				ISC_expand_filename(dbName, true);

				config->dbName = dbName;
				replicas.add(dbConfig.release());
			}
		}
	}
	catch (const Exception& ex)
	{
		FbLocalStatus localStatus;
		composeError(&localStatus, ex);

		logReplicaStatus(dbName, &localStatus);
	}
}

bool Config::hasReplicas()
{
	try
	{
		const PathName filename =
			fb_utils::getPrefix(IConfigManager::DIR_CONF, REPLICATION_CFGFILE);

		ConfigFile cfgFile(filename, ConfigFile::HAS_SUB_CONF |
									 ConfigFile::NATIVE_ORDER |
									 ConfigFile::CUSTOM_MACROS);

		bool hasDbs = false, hasSource = false;

		for (const auto& section : cfgFile.getParameters())
		{
			if (section.name == "database" && section.value.hasData())
				hasDbs = true;

			if (!section.sub)
				continue;

			for (const auto& el : section.sub->getParameters())
			{
				if (el.name == "journal_source_directory" && el.value.hasData())
				{
					hasSource = true;
					break;
				}
			}
		}

		if (hasDbs && hasSource)
			return true;
	}
	catch (const Exception&)
	{} // no-op

	return false;
}
