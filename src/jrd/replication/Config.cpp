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
#include "../common/StatusArg.h"
#include "../jrd/constants.h"

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
	const ULONG DEFAULT_LOG_SEGMENT_SIZE = 16 * 1024 * 1024;	// 16 MB
	const ULONG DEFAULT_LOG_SEGMENT_COUNT = 8;
	const ULONG DEFAULT_LOG_ARCHIVE_TIMEOUT = 60;				// seconds
	const ULONG DEFAULT_LOG_GROUP_FLUSH_DELAY = 0;
	const ULONG DEFAULT_APPLY_IDLE_TIMEOUT = 10;				// seconds
	const ULONG DEFAULT_APPLY_ERROR_TIMEOUT = 60;				// seconds

	void parseLong(const string& input, ULONG& output)
	{
		char* tail = NULL;
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

	void raiseError(const char* msg)
	{
		(Arg::Gds(isc_random) << Arg::Str(msg)).raise();
	}
}


// Replication::Config class

Config::Config()
	: dbName(getPool()),
	  bufferSize(DEFAULT_BUFFER_SIZE),
	  includeFilter(getPool()),
	  excludeFilter(getPool()),
	  logSegmentSize(DEFAULT_LOG_SEGMENT_SIZE),
	  logSegmentCount(DEFAULT_LOG_SEGMENT_COUNT),
	  logDirectory(getPool()),
	  logFilePrefix(getPool()),
	  logGroupFlushDelay(DEFAULT_LOG_GROUP_FLUSH_DELAY),
	  logArchiveDirectory(getPool()),
	  logArchiveCommand(getPool()),
	  logArchiveTimeout(DEFAULT_LOG_ARCHIVE_TIMEOUT),
	  syncReplicas(getPool()),
	  logSourceDirectory(getPool()),
	  verboseLogging(false),
	  applyIdleTimeout(DEFAULT_APPLY_IDLE_TIMEOUT),
	  applyErrorTimeout(DEFAULT_APPLY_ERROR_TIMEOUT)
{
	sourceGuid.alignment = 0;
}

Config::Config(const Config& other)
	: dbName(getPool(), other.dbName),
	  bufferSize(other.bufferSize),
	  includeFilter(getPool(), other.includeFilter),
	  excludeFilter(getPool(), other.excludeFilter),
	  logSegmentSize(other.logSegmentSize),
	  logSegmentCount(other.logSegmentCount),
	  logDirectory(getPool(), other.logDirectory),
	  logFilePrefix(getPool(), other.logFilePrefix),
	  logGroupFlushDelay(other.logGroupFlushDelay),
	  logArchiveDirectory(getPool(), other.logArchiveDirectory),
	  logArchiveCommand(getPool(), other.logArchiveCommand),
	  logArchiveTimeout(other.logArchiveTimeout),
	  syncReplicas(getPool(), other.syncReplicas),
	  logSourceDirectory(getPool(), other.logSourceDirectory),
	  verboseLogging(other.verboseLogging),
	  applyIdleTimeout(other.applyIdleTimeout),
	  applyErrorTimeout(other.applyErrorTimeout)
{
	sourceGuid.alignment = 0;
}

// This routine is used to match the database on the master side.
// Therefore it checks only the necessary settings.

Config* Config::get(const PathName& lookupName)
{
	fb_assert(lookupName.hasData());

	const PathName filename =
		fb_utils::getPrefix(IConfigManager::DIR_CONF, REPLICATION_CFGFILE);

	MemoryPool& pool = *getDefaultMemoryPool();

	ConfigFile cfgFile(filename, ConfigFile::HAS_SUB_CONF | ConfigFile::NATIVE_ORDER | ConfigFile::CUSTOM_MACROS);

	AutoPtr<Config> config(FB_NEW Config);

	bool defaultFound = false, exactMatch = false;
	const ConfigFile::Parameters& params = cfgFile.getParameters();
	for (const auto& section : params)
	{
		if (section.name != "database")
			raiseError("Unknown section found in the configuration file");

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

			exactMatch = true;
		}

		if (section.sub)
		{
			const ConfigFile::Parameters& elements = section.sub->getParameters();
			for (const auto& el : elements)
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
				else if (key == "log_segment_size")
				{
					parseLong(value, config->logSegmentSize);
				}
				else if (key == "log_segment_count")
				{
					parseLong(value, config->logSegmentCount);
				}
				else if (key == "log_directory")
				{
					config->logDirectory = value.c_str();
					PathUtils::ensureSeparator(config->logDirectory);
				}
				else if (key == "log_file_prefix")
				{
					config->logFilePrefix = value.c_str();
				}
				else if (key == "log_group_flush_delay")
				{
					parseLong(value, config->logGroupFlushDelay);
				}
				else if (key == "log_archive_directory")
				{
					config->logArchiveDirectory = value.c_str();
					PathUtils::ensureSeparator(config->logArchiveDirectory);
				}
				else if (key == "log_archive_command")
				{
					config->logArchiveCommand = value.c_str();
				}
				else if (key == "log_archive_timeout")
				{
					parseLong(value, config->logArchiveTimeout);
				}
			}
		}

		if (!exactMatch)
			continue;

		if (config->logDirectory.hasData() || config->syncReplicas.hasData())
		{
			// If log_directory is specified, then replication is enabled

			if (config->logFilePrefix.isEmpty())
			{
				PathName db_directory, db_filename;
				PathUtils::splitLastComponent(db_directory, db_filename, dbName);
				config->logFilePrefix = db_filename;
			}

			config->dbName = dbName;

			return config.release();
		}
	}

	return NULL;
}

// This routine is used to retrieve the list of replica databases.
// Therefore it checks only the necessary settings.

void Config::enumerate(Firebird::Array<Config*>& replicas)
{
	const PathName filename =
		fb_utils::getPrefix(IConfigManager::DIR_CONF, REPLICATION_CFGFILE);

	MemoryPool& pool = *getDefaultMemoryPool();

	ConfigFile cfgFile(filename, ConfigFile::HAS_SUB_CONF | ConfigFile::NATIVE_ORDER | ConfigFile::CUSTOM_MACROS);

	AutoPtr<Config> defConfig(FB_NEW Config);

	bool defaultFound = false, exactMatch = false;
	const ConfigFile::Parameters& params = cfgFile.getParameters();
	for (const auto& section : params)
	{
		if (section.name != "database")
			raiseError("Unknown section found in the configuration file");

		PathName dbName(section.value.c_str());

		AutoPtr<Config> dbConfig;
		if (!dbName.isEmpty())
			dbConfig = FB_NEW Config(*defConfig);

		Config* const config = dbName.isEmpty() ? defConfig : dbConfig;

		if (section.sub)
		{
			const ConfigFile::Parameters& elements = section.sub->getParameters();
			for (const auto& el : elements)
			{
				const string key(el.name.c_str());
				string value(el.value);

				if (value.isEmpty())
					continue;

				if (key == "log_source_directory")
				{
					config->logSourceDirectory = value.c_str();
					PathUtils::ensureSeparator(config->logSourceDirectory);
				}
				else if (key == "source_guid")
				{
					StringToGuid(&config->sourceGuid, value.c_str());
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
		}

		if (dbName.empty())
		{
			if (defaultFound)
				raiseError("Only one default DATABASE section is allowed");

			defaultFound = true;
			continue;
		}

		if (config->logSourceDirectory.hasData())
		{
			// If source_directory is specified, then replication is enabled

			PathUtils::fixupSeparators(dbName);
			ISC_expand_filename(dbName, true);

			config->dbName = dbName;
			replicas.add(dbConfig.release());
		}
	}

}
