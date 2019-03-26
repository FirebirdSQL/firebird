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


#ifndef JRD_REPLICATION_CONFIG_H
#define JRD_REPLICATION_CONFIG_H

#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/fb_string.h"
#include "../common/os/guid.h"

namespace Replication
{
	struct Config : public Firebird::GlobalStorage
	{
		Config();

		static Config* get(const Firebird::PathName& dbName);
		static void enumerate(Firebird::Array<Config*>& replicas);

		Firebird::PathName dbName;
		ULONG bufferSize;
		Firebird::string includeFilter;
		Firebird::string excludeFilter;
		ULONG logSegmentSize;
		ULONG logSegmentCount;
		Firebird::PathName logDirectory;
		Firebird::PathName logFilePrefix;
		ULONG logGroupFlushDelay;
		Firebird::PathName logArchiveDirectory;
		Firebird::string logArchiveCommand;
		ULONG logArchiveTimeout;
		Firebird::ObjectsArray<Firebird::string> syncReplicas;
		Firebird::PathName logSourceDirectory;
		Firebird::Guid sourceGuid;
		bool verboseLogging;
		ULONG applyIdleTimeout;
		ULONG applyErrorTimeout;
	};
};

#endif // JRD_REPLICATION_CONFIG_H
