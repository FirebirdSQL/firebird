/*
 *	PROGRAM:	Firebird Trace Services
 *	MODULE:		TraceConfiguration.h
 *	DESCRIPTION:	Trace Configuration Reader
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "TraceConfiguration.h"
#include "../../common/SimilarToRegex.h"
#include "../../common/isc_f_proto.h"

using namespace Firebird;


void TraceCfgReader::readTraceConfiguration(const char* text,
		const PathName& databaseName,
		TracePluginConfig& config)
{
	TraceCfgReader cfgReader(text, databaseName, config);
	cfgReader.readConfig();
}


#define PATH_PARAMETER(NAME, VALUE) \
	if (!found && el->name == #NAME) { \
		Firebird::PathName temp; \
		expandPattern(el, temp); \
		m_config.NAME = temp.c_str(); \
		found = true; \
	}
#define STR_PARAMETER(NAME, VALUE) \
	if (!found && el->name == #NAME) { \
		m_config.NAME = el->value; \
		found = true; \
	}
#define BOOL_PARAMETER(NAME, VALUE) \
	if (!found && el->name == #NAME) { \
		m_config.NAME = parseBoolean(el); \
		found = true; \
	}
#define UINT_PARAMETER(NAME, VALUE) \
	if (!found && el->name == #NAME) { \
		m_config.NAME = parseUInteger(el); \
		found = true; \
	}


#define ERROR_PREFIX "error while parsing trace configuration\n\t"

void TraceCfgReader::readConfig()
{
	ConfigFile cfgFile(ConfigFile::USE_TEXT, m_text, ConfigFile::HAS_SUB_CONF | ConfigFile::NATIVE_ORDER);

	m_subpatterns[0].start = 0;
	m_subpatterns[0].end = m_databaseName.length();
	for (size_t i = 1; i < FB_NELEM(m_subpatterns); i++)
	{
		m_subpatterns[i].start = -1;
		m_subpatterns[i].end = -1;
	}

	bool defDB = false, defSvc = false, exactMatch = false;
	const ConfigFile::Parameters& params = cfgFile.getParameters();
	for (FB_SIZE_T n = 0; n < params.getCount() && !exactMatch; ++n)
	{
		const ConfigFile::Parameter* section = &params[n];

		const bool isDatabase = (section->name == "database");
		if (!isDatabase && section->name != "services")
			//continue;
			fatal_exception::raiseFmt(ERROR_PREFIX
				"line %d: wrong section header, \"database\" or \"service\" is expected",
				section->line);

		const ConfigFile::String pattern = section->value;
		bool match = false;
		if (pattern.empty())
		{
			if (isDatabase)
			{
				if (defDB)
				{
					fatal_exception::raiseFmt(ERROR_PREFIX
						"line %d: second default database section is not allowed",
						section->line);
				}

				match = !m_databaseName.empty();
				//match = m_databaseName.empty();
				defDB = true;
			}
			else
			{
				if (defSvc)
				{
					fatal_exception::raiseFmt(ERROR_PREFIX
						"line %d: second default service section is not allowed",
						section->line);
				}
				match = m_databaseName.empty();
				defSvc = true;
			}
		}
		else if (isDatabase && !m_databaseName.empty())
		{
			PathName noQuotePattern = pattern.ToPathName();
			noQuotePattern.alltrim(" '\'");

			if (m_databaseName == noQuotePattern)
				match = exactMatch = true;
			else
			{
				bool regExpOk = false;
				try
				{
#ifdef WIN_NT	// !CASE_SENSITIVITY
					const unsigned regexFlags = SimilarToFlag::CASE_INSENSITIVE;
#else
					const unsigned regexFlags = 0;
#endif
					string utf8Pattern = pattern;
					ISC_systemToUtf8(utf8Pattern);

					SimilarToRegex matcher(*getDefaultMemoryPool(), regexFlags,
						utf8Pattern.c_str(), utf8Pattern.length(), "\\", 1);

					regExpOk = true;

					PathName utf8DatabaseName = m_databaseName;
					ISC_systemToUtf8(utf8DatabaseName);
					Array<SimilarToRegex::MatchPos> matchPosArray;

					if (matcher.matches(utf8DatabaseName.c_str(), utf8DatabaseName.length(), &matchPosArray))
					{
						for (unsigned i = 0; i < matchPosArray.getCount() && i < FB_NELEM(m_subpatterns); ++i)
						{
							m_subpatterns[i].start = matchPosArray[i].start;
							m_subpatterns[i].end = matchPosArray[i].start + matchPosArray[i].length;
						}

						match = exactMatch = true;
					}
				}
				catch (const Exception&)
				{
					if (regExpOk)
					{
						fatal_exception::raiseFmt(ERROR_PREFIX
							"line %d: error while processing string \"%s\" against regular expression \"%s\"",
							section->line, m_databaseName.c_str(), pattern.c_str());
					}
					else
					{
						fatal_exception::raiseFmt(ERROR_PREFIX
							"line %d: error while compiling regular expression \"%s\"",
							section->line, pattern.c_str());
					}
				}
			}
		}

		if (!match)
			continue;

		if (!section->sub)
		{
			fatal_exception::raiseFmt(ERROR_PREFIX
				"Trace parameters are not present");
		}

		const ConfigFile::Parameters& elements = section->sub->getParameters();
		for (FB_SIZE_T p = 0; p < elements.getCount(); ++p)
		{
			const ConfigFile::Parameter* el = &elements[p];

			if (!el->value.hasData())
			{
				fatal_exception::raiseFmt(ERROR_PREFIX
					"line %d: element \"%s\" have no attribute value set",
					el->line, el->name.c_str());
			}

			bool found = false;
			if (isDatabase)
			{
#define DATABASE_PARAMS
				#include "paramtable.h"
#undef DATABASE_PARAMS
			}
			else
			{
#define SERVICE_PARAMS
				#include "paramtable.h"
#undef SERVICE_PARAMS
			}

			if (!found)
			{
				fatal_exception::raiseFmt(ERROR_PREFIX
					"line %d: element \"%s\" is unknown",
					el->line, el->name.c_str());
			}
		}
	}
}

#undef PATH_PARAMETER
#undef STR_PARAMETER
#undef BOOL_PARAMETER
#undef UINT_PARAMETER

bool TraceCfgReader::parseBoolean(const ConfigFile::Parameter* el) const
{
	ConfigFile::String tempValue(el->value);
	tempValue.upper();

	if (tempValue == "1" || tempValue == "ON" || tempValue == "YES" || tempValue == "TRUE")
		return true;
	if (tempValue == "0" || tempValue == "OFF" || tempValue == "NO" || tempValue == "FALSE")
		return false;

	fatal_exception::raiseFmt(ERROR_PREFIX
		"line %d, element \"%s\": \"%s\" is not a valid boolean value",
		el->line, el->name.c_str(), el->value.c_str());
	return false; // Silence the compiler
}

ULONG TraceCfgReader::parseUInteger(const ConfigFile::Parameter* el) const
{
	const char *value = el->value.c_str();
	ULONG result = 0;
	if (!sscanf(value, "%" ULONGFORMAT, &result))
	{
		fatal_exception::raiseFmt(ERROR_PREFIX
			"line %d, element \"%s\": \"%s\" is not a valid integer value",
			el->line, el->name.c_str(), value);
	}
	return result;
}

void TraceCfgReader::expandPattern(const ConfigFile::Parameter* el, PathName& valueToExpand)
{
	valueToExpand = el->value.ToPathName();

	// strip quotes around value, if any
	valueToExpand.alltrim(" '\"");

	PathName::size_type pos = 0;
	while (pos < valueToExpand.length())
	{
		string::char_type c = valueToExpand[pos];
		if (c == '\\')
		{
			if (pos + 1 >= valueToExpand.length())
			{
				fatal_exception::raiseFmt(ERROR_PREFIX
					"line %d, element \"%s\": pattern is invalid\n\t %s",
					el->line, el->name.c_str(), el->value.c_str());
			}

			c = valueToExpand[pos + 1];
			if (c == '\\')
			{
				// Kill one of the backslash signs and loop again
				valueToExpand.erase(pos, 1);
				pos++;
				continue;
			}

			if (c >= '0' && c <= '9')
			{
				const MatchPos* subpattern = m_subpatterns + (c - '0');
				// Replace value with piece of database name
				valueToExpand.erase(pos, 2);
				if (subpattern->end != -1 && subpattern->start != -1)
				{
					const off_t subpattern_len = subpattern->end - subpattern->start;
					valueToExpand.insert(pos,
						m_databaseName.substr(subpattern->start, subpattern_len).c_str(),
						subpattern_len);
					pos += subpattern_len;
				}
				continue;
			}

			fatal_exception::raiseFmt(ERROR_PREFIX
				"line %d, element \"%s\": pattern is invalid\n\t %s",
				el->line, el->name.c_str(), el->value.c_str());
		}

		pos++;
	}
}
