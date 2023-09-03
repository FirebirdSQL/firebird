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

#include "../common/classes/alloc.h"
#include "../common/classes/auto.h"
#include "../common/config/config_file.h"
#include "../common/config/config.h"
#include "../common/config/ConfigCache.h"
#include "../common/os/path_utils.h"
#include "../common/os/os_utils.h"
#include "../common/ScanDir.h"
#include "../common/utils_proto.h"
#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

using namespace Firebird;

bool ConfigFile::getLine(Stream* stream, String& str, unsigned int& number)
{
	// this loop efficiently skips almost all comment lines
	while (stream->getLine(str, number))
	{
		if ((str[0] != '#') || (flags & NO_COMMENTS))
			return true;
	}

	return false;
}

namespace {

bool hasWildCards(const PathName& s)
{
	return s.find_first_of("?*") != PathName::npos;
}

class MainStream : public ConfigFile::Stream
{
public:
	MainStream(const char* fname, bool errorWhenMissing)
		: file(os_utils::fopen(fname, "rt")), fileName(fname), l(0)
	{
		if (errorWhenMissing && !file)
		{
			// config file does not exist
			(Arg::Gds(isc_miss_config) << fname << Arg::OsError()).raise();
		}
	}

	bool getLine(ConfigFile::String& input, unsigned int& line)
	{
		input = "";
		if (!file)
		{
			return false;
		}

		// this loop efficiently skips almost all empty lines
		do
		{
			if (feof(file))
			{
				return false;
			}
			if (!input.LoadFromFile(file))
			{
				return false;
			}
			++l;
			input.alltrim(" \t\r");
		} while (input.isEmpty());

		line = l;
		return true;
	}

	bool active() const
	{
		return file.hasData();
	}

	const char* getFileName() const
	{
		return fileName.c_str();
	}

private:
	AutoPtr<FILE> file;
	Firebird::PathName fileName;
	unsigned int l;
};

class TextStream : public ConfigFile::Stream
{
public:
	explicit TextStream(const char* configText)
		: s(configText), l(0)
	{
		if (s && !*s)
		{
			s = NULL;
		}
	}

	bool getLine(ConfigFile::String& input, unsigned int& line)
	{
		do
		{
			if (!s)
			{
				input = "";
				return false;
			}

			const char* ptr = strchr(s, '\n');
			if (!ptr)
			{
				input.assign(s);
				s = NULL;
			}
			else
			{
				input.assign(s, ptr - s);
				s = ptr + 1;
				if (!*s)
				{
					s = NULL;
				}
			}

			++l;
			input.alltrim(" \t\r");
		} while (input.isEmpty());

		line = l;
		return true;
	}

	const char* getFileName() const
	{
		return NULL;
	}

private:
	const char* s;
	unsigned int l;
};

class SubStream : public ConfigFile::Stream
{
public:
	SubStream(const char* fName)
		: fileName(fName), cnt(0)
	{ }

	bool getLine(ConfigFile::String& input, unsigned int& line)
	{
		if (cnt >= data.getCount())
		{
			input = "";
			return false;
		}

		input = data[cnt].first;
		line = data[cnt].second;
		++cnt;

		return true;
	}

	void putLine(const ConfigFile::String& input, unsigned int line)
	{
		data.push(Line(input, line));
	}

	const char* getFileName() const
	{
		return fileName;
	}

private:
	typedef Pair<Left<ConfigFile::String, unsigned int> > Line;
	ObjectsArray<Line> data;
	const char* fileName;
	FB_SIZE_T cnt;
};

} // anonymous namespace


ConfigFile::ConfigFile(const Firebird::PathName& file, USHORT fl, ConfigCache* cache)
	: AutoStorage(),
	  parameters(getPool()),
	  flags(fl),
	  includeLimit(0),
	  filesCache(cache)
{
	MainStream s(file.c_str(), flags & ERROR_WHEN_MISS);
	parse(&s);
}

ConfigFile::ConfigFile(const char* file, USHORT fl, ConfigCache* cache)
	: AutoStorage(),
	  parameters(getPool()),
	  flags(fl),
	  includeLimit(0),
	  filesCache(cache)
{
	MainStream s(file, flags & ERROR_WHEN_MISS);
	parse(&s);
}

ConfigFile::ConfigFile(UseText, const char* configText, USHORT fl)
	: AutoStorage(),
	  parameters(getPool()),
	  flags(fl),
	  includeLimit(0),
	  filesCache(NULL)
{
	TextStream s(configText);
	parse(&s);
}

ConfigFile::ConfigFile(MemoryPool& p, const Firebird::PathName& file, USHORT fl, ConfigCache* cache)
	: AutoStorage(p),
	  parameters(getPool()),
	  flags(fl),
	  includeLimit(0),
	  filesCache(cache)
{
	MainStream s(file.c_str(), flags & ERROR_WHEN_MISS);
	parse(&s);
}

ConfigFile::ConfigFile(MemoryPool& p, ConfigFile::Stream* s, USHORT fl)
	: AutoStorage(p),
	  parameters(getPool()),
	  flags(fl),
	  includeLimit(0),
	  filesCache(NULL)
{
	parse(s);
}

ConfigFile::Stream::~Stream()
{
}

/******************************************************************************
 *
 *	Parse line, taking quotes into account
 */

ConfigFile::LineType ConfigFile::parseLine(const char* fileName, const String& inputPar, Parameter& par)
{
	int inString = 0;
	String::size_type valStart = 0;
	String::size_type eol = String::npos;
	String input = inputPar;
	bool hasSub = false;
	const char* include = "include";
	const unsigned incLen = static_cast<unsigned>(strlen(include));

	for (String::size_type n = 0; n < input.length(); ++n)
	{
		switch (input[n])
		{
		case '"':
			if (par.name.isEmpty())		// quoted string to the left of = doesn't make sense
				return LINE_BAD;
			if (inString >= 2)		// one more quote after quoted string doesn't make sense
				return LINE_BAD;
			inString++;
			break;

		case '=':
			if (par.name.isEmpty())
			{
				par.name = input.substr(0, n).ToNoCaseString();
				par.name.rtrim(" \t\r");
				if (par.name.isEmpty())		// not good - no key
					return LINE_BAD;
				valStart = n + 1;
				par.hasValue = true;
			}
			else if (inString >= 2)		// Something after the end of line
				return LINE_BAD;
			break;

		case '#':
			if (flags & NO_COMMENTS)
			{
				if (inString >= 2)      // Something after the end of line
					return LINE_BAD;
				break;
			}
			if (inString != 1)
			{
				eol = n;
				n = input.length();	// skip the rest of symbols
			}
			break;

		case ' ':
		case '\t':
			if (n == incLen && par.name.isEmpty())
			{
				KeyType inc = input.substr(0, n).ToNoCaseString();
				if (inc == include)
				{
					par.value = input.substr(n);
					par.value.alltrim(" \t\r");

					if (!macroParse(par.value, fileName))
					{
						return LINE_BAD;
					}
					return LINE_INCLUDE;
				}
			}
			// fall down ...
		case '\r':
			break;

		case '{':
		case '}':
			if (n < input.length() - 1 && input[n + 1] == input[n])
			{
				input.erase(n, 1);
				break;
			}
			else if (flags & HAS_SUB_CONF)
			{
				if (inString != 1)
				{
					if (input[n] == '}')
					{
						String s = input.substr(n + 1);
						s.ltrim(" \t\r");
						if (s.hasData() && ((s[0] != '#') || (flags & NO_COMMENTS)))
						{
							return LINE_BAD;
						}
						par.value = input.substr(0, n);
						return LINE_END_SUB;
					}

					hasSub = true;
					inString = 2;
					eol = n;
				}
				break;
			}
			// fall through ....

		default:
			if (inString >= 2)		// Something after the end of line
				return LINE_BAD;
			break;
		}
	}

	if (inString == 1)				// If we are still inside a string, it's error
		return LINE_BAD;

	if (par.name.isEmpty())
	{
		par.name = input.substr(0, eol).ToNoCaseString();
		par.name.rtrim(" \t\r");
		par.value.erase();
	}
	else
	{
		par.value = input.substr(valStart, eol - valStart);
		par.value.alltrim(" \t\r");
		par.value.alltrim("\"");
	}

	// Now expand macros in value
	if (!macroParse(par.value, fileName))
	{
		return LINE_BAD;
	}

	return hasSub ? LINE_START_SUB : LINE_REGULAR;
}

/******************************************************************************
 *
 *	Substitute macro values in a string
 */

bool ConfigFile::macroParse(String& value, const char* fileName) const
{
	String::size_type pos = 0;
	String::size_type subFrom;

	while ((subFrom = value.find("$(", pos)) != String::npos)
	{
		String::size_type subTo = value.find(")", subFrom);
		if (subTo == String::npos)
			return false;

		String macro;
		String m = value.substr(subFrom + 2, subTo - (subFrom + 2));

		++subTo;

		if (!translate(fileName, m, macro))
		{
			if (flags & CUSTOM_MACROS)
			{
				pos = subTo;
				continue;
			}

			return false;
		}

		// Avoid incorrect slashes in pathnames
		PathUtils::fixupSeparators(value.begin());
		PathUtils::fixupSeparators(macro.begin());

		if (subFrom > 0 && value[subFrom - 1] == PathUtils::dir_sep &&
			macro.length() > 0 && macro[0] == PathUtils::dir_sep)
		{
			--subFrom;
		}
		if (subTo < value.length() && value[subTo] == PathUtils::dir_sep &&
			macro.length() > 0 && macro[macro.length() - 1] == PathUtils::dir_sep)
		{
			++subTo;
		}

		// Now perform operation
		value.replace(subFrom, subTo - subFrom, macro);
		pos = subFrom + macro.length();
	}

	return true;
}

/******************************************************************************
 *
 *	Find macro value
 */

bool ConfigFile::translate(const char* fileName, const String& from, String& to) const
{
	if (from == "root")
	{
		to = Config::getRootDirectory();
	}
	else if (from == "install")
	{
		to = Config::getInstallDirectory();
	}
	else if (from == "this")
	{
		if (!fileName)
		{
			return false;
		}

		PathName tempPath(fileName);

#ifdef UNIX
		if (PathUtils::isSymLink(tempPath))
		{
			// If $(this) is a symlink, expand it.
			TEXT temp[MAXPATHLEN];
			const int n = readlink(fileName, temp, sizeof(temp));

			if (n != -1)
			{
				tempPath.assign(temp, n);

				if (PathUtils::isRelative(tempPath))
				{
					PathName parent;
					PathUtils::splitLastComponent(parent, tempPath, fileName);
					PathUtils::concatPath(tempPath, parent, temp);
				}
			}
		}
#endif

		PathName path, file;
		PathUtils::splitLastComponent(path, file, tempPath);
		to = path.ToString();
	}
	else if (!substituteStandardDir(from, to))
	{
		return false;
	}

	return true;
}

/******************************************************************************
 *
 *	Return parameter value as boolean
 */

bool ConfigFile::substituteStandardDir(const String& from, String& to) const
{
	using namespace fb_utils;

	struct Dir {
		unsigned code;
		const char* name;
	} dirs[] = {
#define NMDIR(a) {Firebird::IConfigManager::a, "FB_"#a},
		NMDIR(DIR_CONF)
		NMDIR(DIR_SECDB)
		NMDIR(DIR_PLUGINS)
		NMDIR(DIR_UDF)
		NMDIR(DIR_SAMPLE)
		NMDIR(DIR_SAMPLEDB)
		NMDIR(DIR_INTL)
		NMDIR(DIR_MSG)
#undef NMDIR
		{Firebird::IConfigManager::DIR_COUNT, NULL}
	};

	for (const Dir* d = dirs; d->name; ++d)
	{
		const char* target = &(d->name[3]);		// skip FB_
		if (from.equalsNoCase(target))
		{
			to = getPrefix(d->code, "").c_str();
			return true;
		}
	}

	return false;
}

/******************************************************************************
 *
 *	Return parameter corresponding the given key
 */

const ConfigFile::Parameter* ConfigFile::findParameter(const KeyType& name) const
{
	fb_assert(!(flags & NATIVE_ORDER));
	FB_SIZE_T pos;
	return parameters.find(name, pos) ? &parameters[pos] : NULL;
}

/******************************************************************************
 *
 *	Return parameter corresponding the given key and value
 */

const ConfigFile::Parameter* ConfigFile::findParameter(const KeyType& name, const String& value) const
{
	fb_assert(!(flags & NATIVE_ORDER));
	FB_SIZE_T pos;
	if (!parameters.find(name, pos))
	{
		return NULL;
	}

	while (pos < parameters.getCount() && parameters[pos].name == name)
	{
		if (parameters[pos].value == value)
		{
			return &parameters[pos];
		}
		++pos;
	}

	return NULL;
}

/******************************************************************************
 *
 *	Take into an account fault line
 */

void ConfigFile::badLine(const char* fileName, const String& line)
{
	(Arg::Gds(isc_conf_line) << (fileName ? fileName : "Passed text") << line).raise();
}

/******************************************************************************
 *
 *	Load file immediately
 */

void ConfigFile::parse(Stream* stream)
{
	String inputLine;
	Parameter* previous = NULL;
	unsigned int line;
	const char* streamName = stream->getFileName();

	parameters.setSortMode(FB_ARRAY_SORT_MANUAL);

	while (getLine(stream, inputLine, line))
	{
		Parameter current;
		current.line = line;

		switch (parseLine(streamName, inputLine, current))
		{
		case LINE_END_SUB:
		case LINE_BAD:
			badLine(streamName, inputLine);
			return;

		case LINE_REGULAR:
			if (current.name.isEmpty())
			{
				badLine(streamName, inputLine);
				return;
			}

			previous = &parameters[parameters.add(current)];
			break;

		case LINE_INCLUDE:
			include(streamName, current.value.ToPathName());
			break;

		case LINE_START_SUB:
			if (current.name.hasData())
			{
				FB_SIZE_T n = parameters.add(current);
				previous = &parameters[n];
			}

			{ // subconf scope
				SubStream subStream(stream->getFileName());
				int level = 1;
				while (getLine(stream, inputLine, line))
				{
					switch(parseLine(streamName, inputLine, current))
					{
					case LINE_START_SUB:
						fb_assert(level > 0);
						level++;
						subStream.putLine(inputLine, line);
						continue;

					case LINE_END_SUB:
						level--;
						if (level > 0)
						{
							subStream.putLine(inputLine, line);
							continue;
						}
						else if (level == 0)
						{
							if (current.value.hasData())
								subStream.putLine(current.value, line);
							break;
						}
						else // level < 0 impossible
						{
							fb_assert(false);
						}

					case LINE_BAD:
						badLine(streamName, inputLine);
						return;

					default:
						subStream.putLine(inputLine, line);
						continue;
					}
					break;
				}

				if (level > 0)
					badLine(streamName, "< missed closing bracket '}' >");

				previous->sub = FB_NEW_POOL(getPool())
					ConfigFile(getPool(), &subStream, flags);
			}
			break;
		}
	}

	if (!(flags & NATIVE_ORDER))
		parameters.sort();
}

//#define DEBUG_INCLUDES

/******************************************************************************
 *
 *	Parse include operator
 */

void ConfigFile::include(const char* currentFileName, const PathName& parPath)
{
#ifdef DEBUG_INCLUDES
	fprintf(stderr, "include into %s file(s) %s\n", currentFileName, parPath.c_str());
#endif
	// We should better limit include depth
	AutoSetRestore<unsigned> depth(&includeLimit, includeLimit + 1);
	if (includeLimit > INCLUDE_LIMIT)
	{
		(Arg::Gds(isc_conf_include) << currentFileName << parPath << Arg::Gds(isc_include_depth)).raise();
	}

	// for relative paths first of all prepend with current path (i.e. path of current conf file)
	PathName path;
	if (PathUtils::isRelative(parPath))
	{
		PathName dummy;
		PathUtils::splitLastComponent(path, dummy, currentFileName);
	}
	PathUtils::concatPath(path, path, parPath);

	// split path into components
	PathName pathPrefix;
	PathUtils::splitPrefix(path, pathPrefix);
	bool hadWildCards = hasWildCards(path);		// Expect no *? in prefix
	FilesArray components;
	while (path.hasData())
	{
		PathName cur, tmp;
		PathUtils::splitLastComponent(tmp, cur, path);

#ifdef DEBUG_INCLUDES
		fprintf(stderr, "include: path=%s cur=%s tmp=%s\n", path.c_str(), cur.c_str(), tmp.c_str());
#endif

		components.push(cur);
		path = tmp;
	}

	// analyze components for wildcards
	if (!wildCards(currentFileName, pathPrefix, components))
	{
		// no matches found - check for presence of wild symbols in path
		if (!hadWildCards)
		{
			(Arg::Gds(isc_conf_include) << currentFileName << parPath << Arg::Gds(isc_include_miss)).raise();
		}
	}
}

/******************************************************************************
 *
 *	Parse wildcards
 *		- calls parse for found files
 *		- fills filesCache
 *		- returns true if some match was found
 */

bool ConfigFile::wildCards(const char* currentFileName, const PathName& pathPrefix, FilesArray& components)
{
	// Any change in directory can cause config change
	PathName prefix(pathPrefix);
	if(!pathPrefix.hasData())
		prefix = PathUtils::curr_dir_link;

	bool found = false;
	PathName next(components.pop());
	bool mustBeDir = components.hasData();

#ifdef DEBUG_INCLUDES
	fprintf(stderr, "wildCards: prefix=%s next=%s left=%d\n",
		prefix.c_str(), next.c_str(), components.getCount());
#endif

	ScanDir list(prefix.c_str(), next.c_str());
	while (list.next())
	{
		PathName name;
		const PathName fileName = list.getFileName();
		if (fileName == PathUtils::curr_dir_link || fileName == PathUtils::up_dir_link)
			continue;
		if (mustBeDir && !list.isDirectory())
			continue;
		PathUtils::concatPath(name, pathPrefix, fileName);

#ifdef DEBUG_INCLUDES
		fprintf(stderr, "in Scan: name=%s pathPrefix=%s list.fileName=%s\n",
			name.c_str(), pathPrefix.c_str(), fileName.c_str());
#endif

		if (filesCache)
		{
			bool added = filesCache->addFile(name);
			if (!mustBeDir && !added)
				continue; // Prevent loops in includes
		}

		if (mustBeDir)	// should be directory
		{
			found = wildCards(currentFileName, name, components) || found;
		}
		else
		{
			MainStream include(name.c_str(), false);
			if (include.active())
			{
				found = true;
				parse(&include);
			}
		}
	}
	// Return component back to allow use it for next path as well
	components.push(next);

	return found;
}

/******************************************************************************
 *
 *	Return parameter value as 64-bit integer
 *		- takes into an account k, m and g multipliers
 */

SINT64 ConfigFile::Parameter::asInteger() const
{
	if (value.isEmpty())
		return 0;

	SINT64 ret = 0;
	int sign = 1;
	int state = 1; // 1 - sign, 2 - numbers, 3 - multiplier

	Firebird::string trimmed = value;
	trimmed.trim(" \t");

	if (trimmed.isEmpty())
		return 0;

	const char* ch = trimmed.c_str();
	for (; *ch; ch++)
		switch (*ch)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (state > 2)
				return 0;
			state = 2;

			ret = ret * 10 + (*ch - '0');
			break;

		case '-':
			if (state > 1)
				return 0;

			sign = -sign;
			break;

		case ' ': case '\t':
			if (state == 1)
				break;
			return 0;

		case 'k': case 'K':
			if (state != 2)
				return 0;
			state = 3;

			ret = ret * 1024;
			break;

		case 'm': case 'M':
			if (state != 2)
				return 0;
			state = 3;

			ret = ret * 1024 * 1024;
			break;

		case 'g': case 'G':
			if (state != 2)
				return 0;
			state = 3;

			ret = ret * 1024 * 1024 * 1024;
			break;

		default:
			return 0;
		};

	return sign * ret;
}

/******************************************************************************
 *
 *	Return parameter value as boolean
 */

bool ConfigFile::Parameter::asBoolean() const
{
	return (atoi(value.c_str()) != 0) ||
		value.equalsNoCase("true") ||
		value.equalsNoCase("yes") ||
		value.equalsNoCase("y");
}
