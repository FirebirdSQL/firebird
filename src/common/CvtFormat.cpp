/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         CvtFormat.cpp
 *      DESCRIPTION:    Converter from DateTime to string and vice versa based on format
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
 *  The Original Code was created by Ivanov Artyom
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2024 Ivanov Artyom <artyom.ivanov@red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../common/classes/auto.h"
#include "../common/classes/fb_string.h"
#include "../common/TimeZones.h"
#include "../common/TimeZoneUtil.h"
#include "../common/dsc.h"
#include "../common/StatusArg.h"

#include <cmath>
#include <string_view>

using namespace Firebird;

namespace
{
	class TimeZoneTrie
	{
	public:
		static constexpr int EnglishAlphabet = 26;
		static constexpr int Digits = 10;
		static constexpr int OtherSymbols = sizeof("/-+_") - 1;

		static constexpr int MaxPatternDiversity = EnglishAlphabet + Digits + OtherSymbols;

		static constexpr USHORT UninitializedTimezoneId = 0;

		struct TrieNode
		{
			AutoPtr<TrieNode> childrens[MaxPatternDiversity];
			USHORT timezoneId = UninitializedTimezoneId;
		};

		TimeZoneTrie(MemoryPool& pool)
			: m_root(FB_NEW_POOL(pool) TrieNode()), m_pool(pool)
		{
			USHORT id = 0;
			for (const char* p : BUILTIN_TIME_ZONE_LIST)
			{
				string timezoneName(p);
				timezoneName.upper();
				insertValue(timezoneName.c_str(), MAX_USHORT - id++);
			}
		}

		bool contains(const char* value, USHORT& outTimezoneId, unsigned int& outParsedTimezoneLength)
		{
			const TrieNode* currentNode = m_root;
			FB_SIZE_T valueLength = fb_strlen(value);

			for (outParsedTimezoneLength = 0; outParsedTimezoneLength < valueLength; outParsedTimezoneLength++)
			{
				int index = calculateIndex(value[outParsedTimezoneLength]);

				if (index < 0 || currentNode->childrens[index] == nullptr)
					break;
				currentNode = currentNode->childrens[index];
			}

			if (currentNode->timezoneId == UninitializedTimezoneId)
				return false;

			outTimezoneId = currentNode->timezoneId;
			return true;
		}

	private:
		void insertValue(const char* value, USHORT timezoneId)
		{
			TrieNode* currentNode = m_root;
			FB_SIZE_T valueLength = fb_strlen(value);

			for (unsigned int i = 0; i < valueLength; i++)
			{
				int index = calculateIndex(value[i]);

				if (currentNode->childrens[index] == nullptr)
					currentNode->childrens[index] = FB_NEW_POOL(m_pool) TrieNode();
				currentNode = currentNode->childrens[index];
			}

			currentNode->timezoneId = timezoneId;
		}

		int calculateIndex(char symbol) const
		{
			int index = -1;
			symbol = UPPER(symbol);

			if (symbol >= '0' && symbol <= '9')
				index = symbol - '0';
			else if (symbol >= 'A' && symbol <= 'Z')
				index = symbol - 'A' + Digits;
			else
			{
				switch (symbol)
				{
					case '/': index = Digits + EnglishAlphabet; break;
					case '-': index = Digits + EnglishAlphabet + 1; break;
					case '+': index = Digits + EnglishAlphabet + 2; break;
					case '_': index = Digits + EnglishAlphabet + 3; break;
				}
			}

			fb_assert(index < MaxPatternDiversity);
			return index;
		}

	private:
		AutoPtr<TrieNode> m_root;
		MemoryPool& m_pool;
	};

	InitInstance<TimeZoneTrie> timeZoneTrie;


	#define CVT_FORMAT(id, format) constexpr Patterns format = 1llu << (id - 1);
	#define CVT_FORMAT2(id, format1, format2) constexpr Patterns format2 = 1llu << (id - 1);
	#define CVT_FORMAT_FLAG(id, format) constexpr Patterns format = 1llu << (id - 1);
	namespace Format
	{
		typedef FB_UINT64 Patterns;

		constexpr Patterns NONE = 0;
		#include "CvtFormatImpl.h"
	}
	#undef CVT_FORMAT
	#undef CVT_FORMAT_FLAG
	#undef CVT_FORMAT2

	#define CVT_FORMAT(id, format) constexpr const char* format = #format;
	#define CVT_FORMAT2(id, format1, format2) constexpr const char* format2 = #format1;
	#define CVT_FORMAT_FLAG(id, format)
	namespace FormatStr
	{
		#include "CvtFormatImpl.h"
	}
	#undef CVT_FORMAT
	#undef CVT_FORMAT2
	#undef CVT_FORMAT_FLAG

	constexpr const char* const TO_DATETIME_PATTERNS[] = {
		FormatStr::YEAR, FormatStr::YYYY, FormatStr::YYY, FormatStr::YY, FormatStr::Y, FormatStr::Q, FormatStr::MM,
		FormatStr::MON, FormatStr::MONTH, FormatStr::RM, FormatStr::WW, FormatStr::W, FormatStr::D, FormatStr::DAY,
		FormatStr::DD, FormatStr::DDD, FormatStr::DY, FormatStr::J, FormatStr::HH, FormatStr::HH12, FormatStr::HH24,
		FormatStr::MI, FormatStr::SS, FormatStr::SSSSS, FormatStr::FF1, FormatStr::FF2, FormatStr::FF3, FormatStr::FF4,
		FormatStr::FF5, FormatStr::FF6, FormatStr::FF7, FormatStr::FF8, FormatStr::FF9, FormatStr::TZH, FormatStr::TZM,
		FormatStr::TZR, FormatStr::AM, FormatStr::PM
	};

	constexpr const char* const TO_STRING_PATTERNS[] = {
		FormatStr::YEAR, FormatStr::YYYY, FormatStr::YYY, FormatStr::YY, FormatStr::Y, FormatStr::RRRR, FormatStr::RR,
		FormatStr::MM, FormatStr::MON, FormatStr::MONTH, FormatStr::RM, FormatStr::DD, FormatStr::J, FormatStr::HH,
		FormatStr::HH12, FormatStr::HH24, FormatStr::MI, FormatStr::SS, FormatStr::SSSSS, FormatStr::FF1, FormatStr::FF2,
		FormatStr::FF3, FormatStr::FF4, FormatStr::TZH, FormatStr::TZM, FormatStr::TZR, FormatStr::AM, FormatStr::PM
	};

	constexpr SSHORT UninitializedTimezoneOffsetValue = INT16_MIN;

	struct StringToDateTimeData
	{
		StringToDateTimeData()
		{
			memset(&times, 0, sizeof(times));
		}

		struct tm times;
		int fractions = 0;
		SSHORT timezoneOffsetInMinutes = UninitializedTimezoneOffsetValue;
		USHORT timezoneId = TimeZoneTrie::UninitializedTimezoneId;
	};

	struct Token
	{
		Format::Patterns pattern = Format::NONE;
		std::string_view patternStr;
	};

	struct DeferredToken
	{
		Token token;
		std::string_view data;
	};
}

namespace
{
	template <typename T>
	constexpr int sign(T value)
	{
		return (value >= T(0)) ? 1 : -1;
	}

	bool isDigit(char symbol)
	{
		return std::isdigit(static_cast<unsigned char>(symbol));
	}

	bool isAlpha(char symbol)
	{
		return std::isalpha(static_cast<unsigned char>(symbol));
	}

	constexpr bool isSeparator(char symbol)
	{
		switch (symbol)
		{
			case '.':
			case '/':
			case ',':
			case ';':
			case ':':
			case ' ':
			case '-':
			case '\"':
				return true;

			default:
				return false;
		}
	}

	constexpr Format::Patterns mapFormatStrToFormatPattern(std::string_view pattern)
	{
		if (pattern.empty())
			return Format::NONE;

		switch (pattern[0])
		{
			case 'Y':
			{
				if (pattern == FormatStr::Y)
					return Format::Y;
				if (pattern == FormatStr::YY)
					return Format::YY;
				if (pattern == FormatStr::YYY)
					return Format::YYY;
				if (pattern == FormatStr::YYYY)
					return Format::YYYY;
				if (pattern == FormatStr::YEAR)
					return Format::YEAR;
				break;
			}

			case 'Q':
				if (pattern == FormatStr::Q)
					return Format::Q;
				break;

			case 'M':
				if (pattern == FormatStr::MI)
					return Format::MI;
				if (pattern == FormatStr::MM)
					return Format::MM;
				if (pattern == FormatStr::MON)
					return Format::MON;
				if (pattern == FormatStr::MONTH)
					return Format::MONTH;
				break;

			case 'R':
				if (pattern == FormatStr::RM)
					return Format::RM;
				if (pattern == FormatStr::RR)
					return Format::RR;
				if (pattern == FormatStr::RRRR)
					return Format::RRRR;
				break;

			case 'W':
				if (pattern == FormatStr::W)
					return Format::W;
				if (pattern == FormatStr::WW)
					return Format::WW;
				break;

			case 'D':
				if (pattern == FormatStr::D)
					return Format::D;
				if (pattern == FormatStr::DAY)
					return Format::DAY;
				if (pattern == FormatStr::DD)
					return Format::DD;
				if (pattern == FormatStr::DDD)
					return Format::DDD;
				if (pattern == FormatStr::DY)
					return Format::DY;
				break;

			case 'J':
				if (pattern == FormatStr::J)
					return Format::J;
				break;

			case 'H':
				if (pattern == FormatStr::HH
				 || pattern == FormatStr::HH12)
				{
					return Format::HH12;
				}
				if (pattern == FormatStr::HH24)
					return Format::HH24;
				break;

			case 'S':
				if (pattern == FormatStr::SS)
					return Format::SS;
				if (pattern == FormatStr::SSSSS)
					return Format::SSSSS;
				break;

			case 'F':
				if (!strncmp(pattern.data(), "FF", pattern.length() - 1))
				{
					int number = pattern.back() - '0';
					if (number < 1 || number > 9)
						return Format::NONE;

					return Format::FF1 << (number - 1);
				}
				break;

			case 'A':
				if (pattern == FormatStr::AM)
					return Format::AM;
				break;

			case 'P':
				if (pattern == FormatStr::PM)
					return Format::PM;
				break;

			case 'T':
				if (pattern == FormatStr::TZH)
					return Format::TZH;
				if (pattern == FormatStr::TZM)
					return Format::TZM;
				if (pattern == FormatStr::TZR)
					return Format::TZR;
				break;

			default:
				break;
		}

		return Format::NONE;
	}

	constexpr bool patternIsCompatibleWithDscType(const dsc* desc, Format::Patterns pattern)
	{
		switch (pattern)
		{
			case Format::Y:
			case Format::YY:
			case Format::YYY:
			case Format::YYYY:
			case Format::YEAR:
			case Format::RR:
			case Format::RRRR:
			case Format::Q:
			case Format::MM:
			case Format::MON:
			case Format::MONTH:
			case Format::RM:
			case Format::WW:
			case Format::W:
			case Format::D:
			case Format::DAY:
			case Format::DD:
			case Format::DDD:
			case Format::DY:
			case Format::J:
				return !desc->isTime();

			case Format::HH:
			case Format::HH12:
			case Format::HH24:
			case Format::MI:
			case Format::SS:
			case Format::SSSSS:
			case Format::FF1:
			case Format::FF2:
			case Format::FF3:
			case Format::FF4:
			case Format::FF5:
			case Format::FF6:
			case Format::FF7:
			case Format::FF8:
			case Format::FF9:
			case Format::AM:
			case Format::PM:
				return !desc->isDate();

			case Format::TZH:
			case Format::TZM:
			case Format::TZR:
				return desc->isDateTimeTz();

			default:
				fb_assert(false);
				return false;
		}
	}

	constexpr UCHAR expectDatetimeToDscType(EXPECT_DATETIME expectedDatetime)
	{
		switch (expectedDatetime)
		{
			case expect_timestamp:
				return dtype_timestamp;
			case expect_timestamp_tz:
				return dtype_timestamp_tz;
			case expect_sql_date:
				return dtype_sql_date;
			case expect_sql_time:
				return dtype_sql_time;
			case expect_sql_time_tz:
				return dtype_sql_time_tz;
			default:
				fb_assert(false);
				return 0;
		}
	}

	// Start reading format and comparing it to patterns, until we stop finding similarities
	template <FB_SIZE_T PatternsSize>
	constexpr std::string_view getPatternFromFormat(const char* format, const char* const (&patterns)[PatternsSize],
		FB_SIZE_T formatLength, FB_SIZE_T formatStart, FB_SIZE_T& offset)
	{
		std::string_view patternStr;
		for (; offset < formatLength; offset++)
		{
			patternStr = std::string_view(format + formatStart, offset - formatStart + 1);
			bool isFound = false;

			for (unsigned int j = 0; j < PatternsSize; j++)
			{
				if (!strncmp(patterns[j], patternStr.data(), patternStr.length()))
				{
					isFound = true;
					break;
				}
			}
			if (!isFound)
				break;
		}

		// Our current pattern contains real pattern + one extra symbol, so we need to drop it
		// if it's not a end of format
		if (offset != formatLength)
			return patternStr.substr(0, patternStr.length() - 1);
		return patternStr;
	}

	void invalidPatternException(std::string_view pattern, Callbacks* cb)
	{
		cb->err(Arg::Gds(isc_invalid_date_format) << string(pattern.data(), pattern.length()));
	}

	void incompatibleDateFormatException(std::string_view pattern, Callbacks* cb)
	{
		cb->err(Arg::Gds(isc_incompatible_date_format_with_current_date_type) << string(pattern.data(), pattern.length()));
	}

	//-----------------------------------------------------------------------------------------------------------------
	//                                               DATETIME_TO_STRING
	//-----------------------------------------------------------------------------------------------------------------

	void decomposeDscDataTime(const dsc* desc, struct tm& outTimes, int& outFractions, Callbacks* cb)
	{
		switch (desc->dsc_dtype)
		{
			case dtype_sql_time:
				Firebird::TimeStamp::decode_time(*(GDS_TIME*) desc->dsc_address,
					&outTimes.tm_hour, &outTimes.tm_min, &outTimes.tm_sec, &outFractions);
				break;

			case dtype_sql_time_tz:
			case dtype_ex_time_tz:
				TimeZoneUtil::decodeTime(*(ISC_TIME_TZ*) desc->dsc_address,
					true, TimeZoneUtil::NO_OFFSET, &outTimes, &outFractions);
				break;

			case dtype_sql_date:
				Firebird::TimeStamp::decode_date(*(GDS_DATE*) desc->dsc_address, &outTimes);
				break;

			case dtype_timestamp:
				Firebird::TimeStamp::decode_timestamp(*(GDS_TIMESTAMP*) desc->dsc_address, &outTimes, &outFractions);
				break;

			case dtype_timestamp_tz:
			case dtype_ex_timestamp_tz:
				TimeZoneUtil::decodeTimeStamp(*(ISC_TIMESTAMP_TZ*) desc->dsc_address,
					true, TimeZoneUtil::NO_OFFSET, &outTimes, &outFractions);
				break;

			default:
				cb->err(Arg::Gds(isc_invalid_data_type_for_date_format));
		}
	}

	string convertFormatToUpper(const string& format, Callbacks* cb)
	{
		string formatUpper(format);

		// Convert format to upper case except for text in double quotes
		for (FB_SIZE_T i = 0; i < formatUpper.length(); i++)
		{
			const char symbol = formatUpper[i];
			if (symbol != '\"')
			{
				formatUpper[i] = toupper(symbol);
				continue;
			}

			while (true)
			{
				const FB_SIZE_T pos = formatUpper.find('\"', i + 1);
				if (pos == string::npos)
					cb->err(Arg::Gds(isc_invalid_raw_string_in_date_format));
				FB_SIZE_T tempPos = pos;
				if (formatUpper[--tempPos] == '\\')
				{
					int backslashCount = 1;
					while (formatUpper[--tempPos] == '\\')
						backslashCount++;
					if (backslashCount % 2 == 1)
					{
						i = pos;
						continue;
					}
				}
				i = pos;
				break;
			}
		}

		return formatUpper;
	}

	std::vector<Token> parseDateTimeToStringFormat(const dsc* desc, const string& formatUpper, Callbacks* cb)
	{
		std::vector<Token> tokens;

		FB_SIZE_T formatOffset = 0;
		const FB_SIZE_T formatLength = formatUpper.length();

		for (FB_SIZE_T i = 0; i < formatLength; i++)
		{
			const char symbol = formatUpper[i];

			if (isSeparator(symbol))
			{
				if (symbol == '\"')
				{
					FB_SIZE_T start = i++;
					for (; i < formatLength; i++)
					{
						if (formatUpper[i] == '\"')
							break;
						else if (formatUpper[i] == '\\')
							++i;
					}
					// Insert with " so we can identify this SEPARATOR later in processing tokens
					tokens.push_back({Format::SEPARATOR, {formatUpper.c_str() + start, i - start}});
				}
				else
					tokens.push_back({Format::SEPARATOR, {formatUpper.c_str() + i, 1}});

				formatOffset = i + 1;
				continue;
			}

			std::string_view patternStr = getPatternFromFormat(formatUpper.c_str(), TO_DATETIME_PATTERNS, formatLength,
				formatOffset, i);

			const Format::Patterns pattern = mapFormatStrToFormatPattern(patternStr);
			if (pattern == Format::NONE)
				invalidPatternException(patternStr, cb);
			if (!patternIsCompatibleWithDscType(desc, pattern))
				incompatibleDateFormatException(patternStr, cb);

			tokens.push_back({pattern, patternStr});

			formatOffset = i;
			i--;
		}

		return tokens;
	}

	string intToRoman(int num)
	{
		constexpr std::pair<const char* const, const int> romanNumeralsMap[] = {
			{"M",  1000},
			{"CM", 900},
			{"D",  500},
			{"CD", 400},
			{"C",  100},
			{"XC", 90},
			{"L",  50},
			{"XL", 40},
			{"X",  10},
			{"IX", 9},
			{"V",  5},
			{"IV", 4},
			{"I",  1}
		};

		string roman;
		for (const auto& romanNumeralsPair : romanNumeralsMap)
		{
			while (num >= romanNumeralsPair.second)
			{
				roman += romanNumeralsPair.first;
				num -= romanNumeralsPair.second;
			}
		}
		return roman;
	}

	constexpr std::pair<int, const char*> calculate12hoursFrom24hours(int hours)
	{
		const char* period = nullptr;
		if (hours >= 12)
		{
			period = FormatStr::PM;
			if (hours > 12)
				hours -= 12;
		}
		else
		{
			period = FormatStr::AM;
			if (hours == 0)
				hours = 12;
		}

		return {hours, period};
	}

	SSHORT extractTimezoneOffset(const dsc* desc)
	{
		SSHORT timezoneOffset = 0;

		switch (desc->dsc_dtype)
		{
			case dtype_sql_time_tz:
			case dtype_ex_time_tz:
				TimeZoneUtil::extractOffset(*(ISC_TIME_TZ*) desc->dsc_address, &timezoneOffset);
				break;

			case dtype_timestamp_tz:
			case dtype_ex_timestamp_tz:
				TimeZoneUtil::extractOffset(*(ISC_TIMESTAMP_TZ*) desc->dsc_address, &timezoneOffset);
				break;
		}

		return timezoneOffset;
	}

	string extractTimezoneName(const dsc* desc)
	{
		char timezoneBuffer[TimeZoneUtil::MAX_SIZE];
		unsigned int length = 0;

		switch (desc->dsc_dtype)
		{
			case dtype_sql_time_tz:
			case dtype_ex_time_tz:
				length = TimeZoneUtil::format(timezoneBuffer, sizeof(timezoneBuffer),
					((ISC_TIME_TZ*) desc->dsc_address)->time_zone);
				break;

			case dtype_timestamp_tz:
			case dtype_ex_timestamp_tz:
				length = TimeZoneUtil::format(timezoneBuffer, sizeof(timezoneBuffer),
					((ISC_TIMESTAMP_TZ*) desc->dsc_address)->time_zone);
				break;
		}

		return string(timezoneBuffer, length);
	}

	string processDateTimeToStringTokens(const dsc* desc, const std::vector<Token>& tokens, const struct tm& times, int fractions, Callbacks* cb)
	{
		string result;

		const auto begin = tokens.begin();
		for (auto it = begin; it != tokens.end(); it++)
		{
			string patternResult;

			switch (it->pattern)
			{
				case Format::Y:
					patternResult.printf("%d", (times.tm_year + 1900) % 10);
					break;
				case Format::YY:
					patternResult.printf("%02d", (times.tm_year + 1900) % 100);
					break;
				case Format::YYY:
					patternResult.printf("%03d", (times.tm_year + 1900) % 1000);
					break;
				case Format::YYYY:
					patternResult.printf("%04d", (times.tm_year + 1900) % 10000);
					break;
				case Format::YEAR:
					patternResult.printf("%d", (times.tm_year + 1900));
					break;

				case Format::Q:
				{
					const int quarter = times.tm_mon / 3 + 1;
					patternResult.printf("%d", quarter);
					break;
				}

				case Format::MM:
					patternResult.printf("%02d", (times.tm_mon + 1));
					break;
				case Format::MON:
					patternResult.printf("%s", FB_SHORT_MONTHS[times.tm_mon]);
					break;
				case Format::MONTH:
					patternResult.printf("%s", FB_LONG_MONTHS_UPPER[times.tm_mon]);
					break;

				case Format::RM:
				{
					string roman = intToRoman(times.tm_mon + 1);
					patternResult.printf("%s", roman.c_str());
					break;
				}

				case Format::W:
				{
					const int week = (times.tm_mday - 1) / 7;
					patternResult.printf("%d", week + 1);
					break;
				}
				case Format::WW:
				{
					const int week = NoThrowTimeStamp::convertGregorianDateToWeekDate(times);
					patternResult.printf("%02d", week);
					break;
				}
				case Format::D:
					patternResult.printf("%d", times.tm_wday + 1);
					break;
				case Format::DAY:
					patternResult.printf("%s", FB_LONG_DAYS_UPPER[times.tm_wday]);
					break;
				case Format::DD:
					patternResult.printf("%02d", times.tm_mday);
					break;
				case Format::DDD:
					patternResult.printf("%03d", times.tm_yday + 1);
					break;
				case Format::DY:
					patternResult.printf("%s", FB_SHORT_DAYS[times.tm_wday]);
					break;
				case Format::J:
				{
					const int JulianDay = NoThrowTimeStamp::convertGregorianDateToJulianDate(times.tm_year + 1900,
						times.tm_mon + 1, times.tm_mday);
					patternResult.printf("%d", JulianDay);
					break;
				}
				case Format::HH:
				case Format::HH12:
				{
					const auto [hours, period] = calculate12hoursFrom24hours(times.tm_hour);
					patternResult.printf("%02d", hours);
					break;
				}
				case Format::HH24:
					patternResult.printf("%02d", times.tm_hour);
					break;
				case Format::MI:
					patternResult.printf("%02d", times.tm_min);
					break;
				case Format::SS:
					patternResult.printf("%02d", times.tm_sec);
					break;
				case Format::SSSSS:
				{
					const int secondsInDay = times.tm_hour * 60 * 60 + times.tm_min * 60 + times.tm_sec;
					patternResult.printf("%d", secondsInDay);
					break;
				}
				case Format::FF1:
				case Format::FF2:
				case Format::FF3:
				case Format::FF4:
				case Format::FF5:
				case Format::FF6:
				case Format::FF7:
				case Format::FF8:
				case Format::FF9:
				{
					int number = it->patternStr.back() - '0';

					const int fractionsPrecision = fractions != 0 ? std::log10(fractions) + 1 : 1;
					int additionalZerosCount = 0;
					if (number > fractionsPrecision)
					{
						additionalZerosCount = number - fractionsPrecision;
						number = fractionsPrecision;
					}

					patternResult.printf("%d%.*s", fractions / (int) powf(10, fractionsPrecision - number),
						additionalZerosCount, "00000000");
					break;
				}

				case Format::AM:
				case Format::PM:
				{
					const auto [hours, period] = calculate12hoursFrom24hours(times.tm_hour);
					patternResult.printf("%s", period);
					break;
				}

				case Format::TZH:
				{
					const SSHORT timezoneOffset = extractTimezoneOffset(desc);
					const int timezoneSign = sign(timezoneOffset);
					const SSHORT offsetInHours = abs(timezoneOffset / 60);

					string printfFormat = "%02d";
					if (timezoneSign < 0)
						printfFormat = "-" + printfFormat;
					else
						printfFormat = "+" + printfFormat;

					patternResult.printf(printfFormat.c_str(), offsetInHours);

					if (it == begin)
						break;

					auto prevIt = it - 1;
					while (prevIt > begin && prevIt->pattern == Format::SEPARATOR)
						--prevIt;

					if (prevIt->pattern == Format::TZM)
						patternResult.erase(0, 1);
					break;
				}
				case Format::TZM:
				{
					const SSHORT timezoneOffset = extractTimezoneOffset(desc);
					const int timezoneSign = sign(timezoneOffset);
					const SSHORT offsetInMinutes = abs(timezoneOffset % 60);

					string printfFormat = "%02d";
					if (timezoneSign < 0)
						printfFormat = "-" + printfFormat;
					else if (timezoneOffset >= 0)
						printfFormat = "+" + printfFormat;

					patternResult.printf(printfFormat.c_str(), offsetInMinutes);

					if (it == begin)
						break;

					auto prevIt = it - 1;
					while (prevIt > begin && prevIt->pattern == Format::SEPARATOR)
						--prevIt;

					if (prevIt->pattern == Format::TZH)
						patternResult.erase(0, 1);
					break;
				}
				case Format::TZR:
					patternResult = extractTimezoneName(desc);
					break;

				case Format::SEPARATOR:
				{
					std::string_view format = it->patternStr;

					if (format[0] == '\"')
					{
						patternResult.resize(format.length(), '\0');
						for (FB_SIZE_T i = 1, j = 0; i < format.length(); i++, j++)
						{
							if (format[i] == '\"')
								break;
							else if (format[i] == '\\')
								patternResult[j] = format[++i];
							else
								patternResult[j] = format[i];
						}
						patternResult.recalculate_length();
					}
					else
						patternResult.assign(format.data(), format.length());
					break;
				}

				default:
					fb_assert(false);
					invalidPatternException(it->patternStr, cb);
			}

			result += patternResult;
		}

		return result;
	}

	//-----------------------------------------------------------------------------------------------------------------
	//                                               STRING_TO_DATETIME
	//-----------------------------------------------------------------------------------------------------------------

	std::vector<Token> parseStringToDateTimeFormat(const dsc* desc, const string& formatUpper, Format::Patterns& outFormatPatterns, Callbacks* cb)
	{
		outFormatPatterns = Format::NONE;

		std::vector<Token> tokens;

		const FB_SIZE_T formatLength = formatUpper.length();
		FB_SIZE_T formatOffset = 0;

		for (FB_SIZE_T i = 0; i < formatLength; i++)
		{
			// Iterate through format and string until we meet any non separator symbol
			for (; i < formatLength; i++, formatOffset++)
			{
				if (!isSeparator(formatUpper[i]))
					break;
			}
			// All remaining characters were separators, so we have fully read the format, get out
			if (i == formatLength)
				break;

			std::string_view patternStr = getPatternFromFormat(formatUpper.c_str(), TO_STRING_PATTERNS, formatLength,
				formatOffset, i);

			const Format::Patterns pattern = mapFormatStrToFormatPattern(patternStr);
			if (pattern == Format::NONE)
				invalidPatternException(patternStr, cb);
			if (outFormatPatterns & pattern)
				cb->err(Arg::Gds(isc_can_not_use_same_pattern_twice) << string(patternStr.data(), patternStr.length()));
			if (!patternIsCompatibleWithDscType(desc, pattern))
				incompatibleDateFormatException(patternStr, cb);

			tokens.push_back({pattern, patternStr});
			outFormatPatterns |= pattern;

			formatOffset = i;
			i--;
		}

		return tokens;
	}

	// These rules are taken from ISO/IEC 9075-2:2023(E) 9.52 Datetime templates
	constexpr void validateFormatFlags(Format::Patterns formatFlags, Callbacks* cb)
	{
		// CT shall contain at most one of each of the following: <datetime template year>
		if (Format::Patterns value = formatFlags & (Format::Y | Format::YY | Format::YYY | Format::YYYY | Format::YEAR))
		{
			switch (value)
			{
				case Format::Y:
				case Format::YY:
				case Format::YYY:
				case Format::YYYY:
				case Format::YEAR:
					break;
				default:
					cb->err(Arg::Gds(isc_only_one_pattern_can_be_used) << Arg::Str("Y/YY/YYY/YYYY/YEAR"));
			}
		}

		// CT shall contain at most one of each of the following: <datetime template rounded year>
		if (formatFlags & Format::RR && formatFlags & Format::RRRR)
			cb->err(Arg::Gds(isc_only_one_pattern_can_be_used) << Arg::Str("RR/RRRR"));

		// CT shall not contain both <datetime template year> and <datetime template rounded year>
		if ((formatFlags & (Format::Y | Format::YY | Format::YYY | Format::YYYY | Format::YEAR)) && (formatFlags & (Format::RR | Format::RRRR)))
			cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("Y/YY/YYY/YYYY/YEAR") << Arg::Str("RR/RRRR"));

		// If CT contains <datetime template day of year>, then CT shall not contain <datetime template month>
		// or <datetime template day of month>.
		if (formatFlags & Format::DDD && formatFlags & (Format::MM | Format::DD))
			cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("DDD") << Arg::Str("MM/DD"));

		if (formatFlags & (Format::HH | Format::HH12 | Format::AM | Format::PM))
		{
			// If CT contains <datetime template 24-hour>, then CT shall not contain <datetime template 12-hour> or <datetime template am/pm>.
			if (formatFlags & Format::HH24)
				cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("HH24") << Arg::Str("HH/HH12/A.M./P.M."));

			// If CT contains <datetime template 12-hour>, then CT shall contain <datetime template am/pm> and shall not contain <datetime template 24-hour>.
			// If CT contains <datetime template am/pm>, then CT shall contain <datetime template 12-hour> and shall not contain <datetime template 24-hour>.
			if (static_cast<bool>(formatFlags & (Format::HH12 | Format::HH)) != static_cast<bool>(formatFlags & (Format::AM | Format::PM)))
			{
				cb->err(Arg::Gds(isc_pattern_cant_be_used_without_other_pattern_and_vice_versa)
					<< Arg::Str("HH/HH12") << Arg::Str("A.M./P.M."));
			}

			// CT shall contain at most one of each of the following: <datetime template 12-hour>
			if (formatFlags & Format::HH && formatFlags & Format::HH12)
				cb->err(Arg::Gds(isc_only_one_pattern_can_be_used) << Arg::Str("HH/HH12"));

			// CT shall contain at most one of each of the following: <datetime template am/pm>
			if (formatFlags & Format::AM && formatFlags & Format::PM)
				cb->err(Arg::Gds(isc_only_one_pattern_can_be_used) << Arg::Str("A.M./P.M."));
		}

		// CT shall contain at most one of each of the following: <datetime template fraction>
		if (Format::Patterns value = formatFlags & (Format::FF1 | Format::FF2 | Format::FF3 | Format::FF4
												  | Format::FF5 | Format::FF6 | Format::FF7 | Format::FF8
												  | Format::FF9))
		{
			switch (value)
			{
				case Format::FF1:
				case Format::FF2:
				case Format::FF3:
				case Format::FF4:
				case Format::FF5:
				case Format::FF6:
				case Format::FF7:
				case Format::FF8:
				case Format::FF9:
					break;
				default:
					cb->err(Arg::Gds(isc_only_one_pattern_can_be_used) << Arg::Str("FF1/FF2/FF3/FF4/FF5/FF6/FF7/FF8/FF9"));
			}
		}

		// If CT contains <datetime template second of day>, then CT shall not contain any of the following:
		// <datetime template 12-hour>, <datetime template 24-hour>, <datetime template minute>, <datetime
		// template second of minute>, or <datetime template am/pm>.
		if (formatFlags & Format::SSSSS)
		{
			if (formatFlags & Format::HH12)
				cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("SSSSS") << Arg::Str("HH/HH12"));
			if (formatFlags & Format::HH24)
				cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("SSSSS") << Arg::Str("HH24"));
			if (formatFlags & Format::MI)
				cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("SSSSS") << Arg::Str("MI"));
			if (formatFlags & Format::SS)
				cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("SSSSS") << Arg::Str("HH/HH12"));
			if (formatFlags & (Format::AM | Format::PM))
				cb->err(Arg::Gds(isc_incompatible_format_patterns) << Arg::Str("SSSSS") << Arg::Str("A.M./P.M."));
		}

		// If CT contains <datetime template time zone minute>, then CT shall contain <datetime template time zone hour>.
		if (formatFlags & Format::TZM && !(formatFlags & Format::TZH))
			cb->err(Arg::Gds(isc_pattern_cant_be_used_without_other_pattern) << Arg::Str("TZM") << Arg::Str("TZH"));
	}

	constexpr int romanToInt(const char* str, FB_SIZE_T length, FB_SIZE_T& offset)
	{
		int result = 0;
		int temp = 0;

		for (; offset < length; offset++)
		{
			int value = 0;

			switch (str[offset])
			{
				case 'I': value = 1; break;
				case 'V': value = 5; break;
				case 'X': value = 10; break;
				case 'L': value = 50; break;
				case 'C': value = 100; break;
				case 'D': value = 500; break;
				case 'M': value = 1000; break;
				default: return result;
			}

			result += value;
			if (temp < value)
				result -= temp * 2;
			temp = value;
		}

		return result;
	}

	constexpr int applyPeriod(std::string_view period, int twelveHours, Firebird::Callbacks* cb)
	{
		if (period == FormatStr::AM)
		{
			if (twelveHours == 12)
				return 0;
			return twelveHours;
		}
		else if (period == FormatStr::PM)
			return twelveHours == 12 ? twelveHours : 12 + twelveHours;

		cb->err(Arg::Gds(isc_incorrect_hours_period) << string(period.data(), period.length()));
		return 0; // suppress compiler warning/error
	}

	constexpr int roundYearPatternImplementation(int parsedRRValue, int currentYear)
	{
		const int firstTwoDigits = currentYear / 100;
		const int lastTwoDigits = currentYear % 100;

		int result = 0;

		if (parsedRRValue < 50)
		{
			result = lastTwoDigits < 50
				? firstTwoDigits * 100 + parsedRRValue
				: (firstTwoDigits + 1) * 100 + parsedRRValue;
		}
		else
		{
			result = lastTwoDigits < 50
				? (firstTwoDigits - 1) * 100 + parsedRRValue
				: firstTwoDigits * 100 + parsedRRValue;
		}

		return result;
	}

	constexpr int getIntFromString(const char* str, FB_SIZE_T length, FB_SIZE_T& offset, FB_SIZE_T parseLength, bool withSign = false)
	{
		int result = 0;
		int sign = 1;

		if (withSign)
		{
			// To check '-' sign we need to move back, cuz '-' is also used as separator,
			// so it will be skipped when we trying to remove "empty" space between values
			if (str[offset] == '+')
				offset++;
			else if (offset != 0 && str[offset - 1] == '-')
				sign = -1;
		}

		const FB_SIZE_T parseLengthWithOffset = offset + parseLength;
		for (; offset < parseLengthWithOffset && offset < length; offset++)
		{
			if (!isDigit(str[offset]))
				return result * sign;

			result = result * 10 + (str[offset] - '0');
		}

		return result * sign;
	}

	constexpr std::string_view getSubstringFromString(const char* str, FB_SIZE_T length, FB_SIZE_T& offset, FB_SIZE_T parseLength = 0,
		bool onlyCharacters = true)
	{
		FB_SIZE_T wordLen = 0;
		FB_SIZE_T startPoint = offset;

		const FB_SIZE_T parseLengthWithOffset = parseLength > 0 ? offset + parseLength : std::numeric_limits<decltype(parseLength)>::max();
		for (; offset < parseLengthWithOffset && offset < length; offset++)
		{
			if (onlyCharacters && !isAlpha(str[offset]))
				break;
			++wordLen;
		}

		return std::string_view(str + startPoint, wordLen);
	}

	template <typename TIterator>
	constexpr TIterator getPreviousOrCurrentIterator(TIterator it, TIterator begin)
	{
		if (it == begin)
			return it;
		return it - 1;
	}

	void processStringToDateTimeDeferredTokens(const std::vector<DeferredToken>& deferredTokens,
		StringToDateTimeData& outCvtData, Firebird::Callbacks* cb)
	{
		struct tm& outTimes = outCvtData.times;

		for (const DeferredToken& deferredToken : deferredTokens)
		{
			switch (deferredToken.token.pattern)
			{
				case Format::AM:
				case Format::PM:
				{
					std::string_view period = deferredToken.data;

					outTimes.tm_hour = applyPeriod(period, outTimes.tm_hour, cb);
					break;
				}

				default:
					fb_assert(false);
					invalidPatternException(deferredToken.token.patternStr, cb);
			}
		}
	}

	void processStringToDateTimeTokens(const std::vector<Token>& tokens, string inputStr, StringToDateTimeData& outCvtData,
		Firebird::Callbacks* cb)
	{
		const char* str = inputStr.c_str();
		const FB_SIZE_T strLength = inputStr.length();

		struct tm& outTimes = outCvtData.times;
		int& outFractions = outCvtData.fractions;
		SSHORT& outTimezoneInMinutes = outCvtData.timezoneOffsetInMinutes;
		USHORT& outTimezoneId = outCvtData.timezoneId;

		// This vector is uses for deferred tokens, it should be processed after other tokens (e.g. A.M./P.M. after HH/HH12)
		std::vector<DeferredToken> deferredTokens;

		FB_SIZE_T strOffset = 0;

		const auto begin = tokens.begin();
		for (auto it = tokens.begin(); it != tokens.end(); it++)
		{
			for (; strOffset < strLength; strOffset++)
			{
				if (!isSeparator(str[strOffset]))
					break;
			}
			if (strOffset >= strLength)
				cb->err(Arg::Gds(isc_data_for_format_is_exhausted) << string(it->patternStr.data()));

			std::string_view patternStr = it->patternStr;

			switch (it->pattern)
			{
				case Format::Y:
				{
					// Set last digit to zero
					const int currentYear = (outTimes.tm_year + 1900) / 10 * 10;
					const int year = getIntFromString(str, strLength, strOffset, 1);

					outTimes.tm_year = currentYear + year - 1900;
					break;
				}
				case Format::YY:
				{
					// Set 2 last digits to zero
					const int currentAge = (outTimes.tm_year + 1900) / 100 * 100;
					const int parsedYear = getIntFromString(str, strLength, strOffset, 2);

					outTimes.tm_year = currentAge + parsedYear - 1900;
					break;
				}
				case Format::YYY:
				{
					// Set 3 last digits to zero
					const int currentThousand = (outTimes.tm_year + 1900) / 1000 * 1000;
					const int parsedYear = getIntFromString(str, strLength, strOffset, 3);

					outTimes.tm_year = currentThousand + parsedYear - 1900;
					break;
				}
				case Format::YYYY:
				{
					const int year = getIntFromString(str, strLength, strOffset, 4);

					outTimes.tm_year = year - 1900;
					break;
				}
				case Format::YEAR:
				{
					const int year = getIntFromString(str, strLength, strOffset, strLength - strOffset);
					if (year > 9999)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(0) << Arg::Num(9999));
					}
					outTimes.tm_year = year - 1900;
					break;
				}
				case Format::MI:
				{
					const int minutes = getIntFromString(str, strLength, strOffset, 2);
					if (minutes > 59)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(0) << Arg::Num(59));
					}

					outTimes.tm_min = minutes;
					break;
				}
				case Format::MM:
				{
					int month = getIntFromString(str, strLength, strOffset, 2);
					if (month < 1 || month > 12)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(1) << Arg::Num(12));
					}

					outTimes.tm_mon = month - 1;
					break;
				}
				case Format::MON:
				{
					bool isFound = false;

					std::string_view monthShortName = getSubstringFromString(str, strLength, strOffset, 3);
					for (FB_SIZE_T i = 0; i < FB_NELEM(FB_SHORT_MONTHS) - 1; i++)
					{
						if (std::equal(monthShortName.begin(), monthShortName.end(),
								FB_SHORT_MONTHS[i], FB_SHORT_MONTHS[i] + strlen(FB_SHORT_MONTHS[i]),
								[](char a, char b) { return a == UPPER(b); }))
						{
							outTimes.tm_mon = i;
							isFound = true;
							break;
						}
					}

					if (!isFound)
						cb->err(Arg::Gds(isc_month_name_mismatch) << string(monthShortName.data(), monthShortName.length()));
					break;
				}
				case Format::MONTH:
				{
					bool isFound = false;

					std::string_view monthFullName = getSubstringFromString(str, strLength, strOffset);
					for (FB_SIZE_T i = 0; i < FB_NELEM(FB_LONG_MONTHS_UPPER) - 1; i++)
					{
						if (std::equal(monthFullName.begin(), monthFullName.end(),
								FB_LONG_MONTHS_UPPER[i], FB_LONG_MONTHS_UPPER[i] + strlen(FB_LONG_MONTHS_UPPER[i]),
								[](char a, char b) { return a == UPPER(b); }))
						{
							outTimes.tm_mon = i;
							isFound = true;
							break;
						}
					}

					if (!isFound)
						cb->err(Arg::Gds(isc_month_name_mismatch) << string(monthFullName.data(), monthFullName.length()));
					break;
				}

				case Format::RR:
				{
					// tm_year already contains current date
					const int parsedYear = getIntFromString(str, strLength, strOffset, 2);
					outTimes.tm_year = roundYearPatternImplementation(parsedYear, outTimes.tm_year + 1900) - 1900;
					break;
				}
				case Format::RRRR:
				{
					const int startOffset = strOffset;
					const int parsedYear = getIntFromString(str, strLength, strOffset, 4);
					const int numberOfSymbols = strOffset - startOffset;

					outTimes.tm_year = numberOfSymbols <= 2
						? roundYearPatternImplementation(parsedYear, outTimes.tm_year + 1900)
						: parsedYear;
					outTimes.tm_year -= 1900;
					break;
				}
				case Format::RM:
				{
					const int month = romanToInt(str, strLength, strOffset);
					if (month == 0 || month > 12)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(1) << Arg::Num(12));
					}

					outTimes.tm_mon = month - 1;
					break;
				}

				case Format::DD:
				{
					const int day = getIntFromString(str, strLength, strOffset, 2);
					if (day == 0 || day > 31)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(1) << Arg::Num(31));
					}

					outTimes.tm_mday = day;
					break;
				}

				case Format::J:
				{
					const int JDN = getIntFromString(str, strLength, strOffset, strLength - strOffset);

					constexpr int minJDN = 1721426; // 0.0.0
					constexpr int maxJDN = 5373484; // 31.12.9999
					if (JDN < minJDN || JDN > maxJDN)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(minJDN) << Arg::Num(maxJDN));
					}

					int year = 0, month = 0, day = 0;
					NoThrowTimeStamp::convertJulianDateToGregorianDate(JDN, year, month, day);
					outTimes.tm_year = year - 1900;
					outTimes.tm_mon = month - 1;
					outTimes.tm_mday = day;
					break;
				}

				case Format::HH:
				case Format::HH12:
				{
					const int hours = getIntFromString(str, strLength, strOffset, 2);
					if (hours < 1 || hours > 12)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(1) << Arg::Num(12));
					}

					outTimes.tm_hour = hours;
					break;
				}
				case Format::HH24:
				{
					const int hours = getIntFromString(str, strLength, strOffset, 2);
					if (hours > 23)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(0) << Arg::Num(23));
					}

					outTimes.tm_hour = hours;
					break;
				}

				case Format::SS:
				{
					const int seconds = getIntFromString(str, strLength, strOffset, 2);
					if (seconds > 59)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(0) << Arg::Num(59));
					}

					outTimes.tm_sec = seconds;
					break;
				}
				case Format::SSSSS:
				{
					constexpr int maximumSecondsInDay = NoThrowTimeStamp::SECONDS_PER_DAY - 1;

					const int secondsInDay = getIntFromString(str, strLength, strOffset, 5);
					if (secondsInDay > maximumSecondsInDay)
					{
						cb->err(Arg::Gds(isc_value_for_pattern_is_out_of_range) <<
							string(patternStr.data(), patternStr.length()) << Arg::Num(0) << Arg::Num(maximumSecondsInDay));
					}

					const int hours = secondsInDay / 24;
					const int minutes = secondsInDay / 60 - hours * 60;
					const int seconds = secondsInDay - minutes * 60 - hours * 60 * 60;

					outTimes.tm_hour = hours;
					outTimes.tm_min = minutes;
					outTimes.tm_sec = seconds;
					break;
				}

				case Format::FF1:
				case Format::FF2:
				case Format::FF3:
				case Format::FF4:
				{
					const int number = patternStr.back() - '0';

					const int fractions = getIntFromString(str, strLength, strOffset, number);
					outFractions = fractions * pow(10, -ISC_TIME_SECONDS_PRECISION_SCALE - number);
					break;
				}

				case Format::AM:
				case Format::PM:
				{
					// This is deferred pattern
					std::string_view period = getSubstringFromString(str, strLength, strOffset,
						fb_strlen(FormatStr::AM), false);
					deferredTokens.push_back({*it, period});
					break;
				}

				case Format::TZH:
				{
					auto prevIt = getPreviousOrCurrentIterator(it, begin);
					if (prevIt->pattern == Format::TZM)
					{
						outTimezoneInMinutes += sign(outTimezoneInMinutes) *
							getIntFromString(str, strLength, strOffset, strLength - strOffset) * 60;
					}
					else
						outTimezoneInMinutes = getIntFromString(str, strLength, strOffset, strLength - strOffset, true) * 60;
					break;
				}
				case Format::TZM:
				{
					auto prevIt = getPreviousOrCurrentIterator(it, begin);
					if (prevIt->pattern == Format::TZH)
					{
						outTimezoneInMinutes += sign(outTimezoneInMinutes) *
							getIntFromString(str, strLength, strOffset, strLength - strOffset);
					}
					else
						outTimezoneInMinutes = getIntFromString(str, strLength, strOffset, strLength - strOffset, true);
					break;
				}
				case Format::TZR:
				{
					unsigned int parsedTimezoneNameLength = 0;
					const bool timezoneNameIsCorrect = timeZoneTrie().contains(str + strOffset, outTimezoneId, parsedTimezoneNameLength);
					if (!timezoneNameIsCorrect)
						status_exception::raise(Arg::Gds(isc_invalid_timezone_region) << string(str + strOffset, parsedTimezoneNameLength));

					strOffset += parsedTimezoneNameLength;
					break;
				}
				default:
					invalidPatternException(patternStr, cb);
			}
		}

		processStringToDateTimeDeferredTokens(deferredTokens, outCvtData, cb);

		for (; strOffset < strLength; strOffset++)
		{
			if (!isSeparator(str[strOffset]))
				break;
		}
		if (strOffset < strLength)
			cb->err(Arg::Gds(isc_trailing_part_of_string) << string(str + strOffset));
	}

	void timeStampToUtc(ISC_TIMESTAMP_TZ& timestampTZ, USHORT sessionTimeZone, EXPECT_DATETIME expectedType,
		Callbacks* cb)
	{
		if (expectedType == expect_sql_time_tz || expectedType == expect_timestamp_tz || timestampTZ.time_zone != sessionTimeZone)
			TimeZoneUtil::localTimeStampToUtc(timestampTZ);

		if (timestampTZ.time_zone != sessionTimeZone)
		{
			if (expectedType == expect_sql_time)
			{
				ISC_TIME_TZ timeTz;
				timeTz.utc_time = timestampTZ.utc_timestamp.timestamp_time;
				timeTz.time_zone = timestampTZ.time_zone;
				timestampTZ.utc_timestamp.timestamp_time = TimeZoneUtil::timeTzToTime(timeTz, cb);
			}
			else if (expectedType == expect_timestamp)
				*(ISC_TIMESTAMP*) &timestampTZ = TimeZoneUtil::timeStampTzToTimeStamp(timestampTZ, sessionTimeZone);
		}
	}

	void validateTimeStamp(const ISC_TIMESTAMP timestamp, const EXPECT_DATETIME expectedType, const dsc* desc,
		Callbacks* cb)
	{
		if (!NoThrowTimeStamp::isValidTimeStamp(timestamp))
		{
			switch (expectedType)
			{
				case expect_sql_date:
					cb->err(Arg::Gds(isc_date_range_exceeded));
					break;
				case expect_sql_time:
				case expect_sql_time_tz:
					cb->err(Arg::Gds(isc_time_range_exceeded));
					break;
				case expect_timestamp:
				case expect_timestamp_tz:
					cb->err(Arg::Gds(isc_datetime_range_exceeded));
					break;
				default: // this should never happen!
					CVT_conversion_error(desc, cb->err);
					break;
			}
		}
	}

	ISC_TIMESTAMP_TZ constructTimeStampTz(const StringToDateTimeData& cvtData, Callbacks* cb)
	{
		ISC_TIMESTAMP_TZ timestampTZ;
		timestampTZ.utc_timestamp = NoThrowTimeStamp::encode_timestamp(&cvtData.times, cvtData.fractions);

		const ISC_USHORT sessionTimeZone = cb->getSessionTimeZone();

		if (cvtData.timezoneOffsetInMinutes == UninitializedTimezoneOffsetValue && cvtData.timezoneId == TimeZoneTrie::UninitializedTimezoneId)
			timestampTZ.time_zone = sessionTimeZone;
		else if (cvtData.timezoneId != TimeZoneTrie::UninitializedTimezoneId)
			timestampTZ.time_zone = cvtData.timezoneId;
		else
		{
			timestampTZ.time_zone = TimeZoneUtil::makeFromOffset(sign(cvtData.timezoneOffsetInMinutes),
				abs(cvtData.timezoneOffsetInMinutes) / 60, abs(cvtData.timezoneOffsetInMinutes) % 60);
		}

		return timestampTZ;
	}
}

string CVT_format_datetime_to_string(const dsc* desc, const string& format, Callbacks* cb)
{
	if (format.isEmpty())
		cb->err(Arg::Gds(isc_sysf_invalid_null_empty) << Arg::Str(STRINGIZE(format)));

	struct tm times;
	memset(&times, 0, sizeof(struct tm));

	int fractions = 0;

	decomposeDscDataTime(desc, times, fractions, cb);

	string formatUpper = convertFormatToUpper(format, cb);
	std::vector<Token> tokens = parseDateTimeToStringFormat(desc, formatUpper, cb);
	string result = processDateTimeToStringTokens(desc, tokens, times, fractions, cb);

	return result;
}

ISC_TIMESTAMP_TZ CVT_format_string_to_datetime(const dsc* desc, const Firebird::string& format,
	const EXPECT_DATETIME expectedType, Firebird::Callbacks* cb)
{
	if (!DTYPE_IS_TEXT(desc->dsc_dtype))
		cb->err(Arg::Gds(isc_invalid_data_type_for_date_format));

	if (format.isEmpty())
		cb->err(Arg::Gds(isc_sysf_invalid_null_empty) << Arg::Str(STRINGIZE(format)));

	USHORT dtype;
	UCHAR* sourceString;
	const USHORT stringLength = CVT_get_string_ptr_common(desc, &dtype, &sourceString, nullptr, 0, 0, cb);

	string stringUpper(stringLength, '\0');
	for (int i = 0; i < stringLength; i++)
		stringUpper[i] = toupper(sourceString[i]);

	string formatUpper(format.length(), '\0');
	for (unsigned int i = 0; i < format.length(); i++)
		formatUpper[i] = toupper(format[i]);

	StringToDateTimeData cvtData;

	NoThrowTimeStamp::decode_date(cb->getLocalDate(), &cvtData.times);

	dsc tempDesc;
	tempDesc.dsc_dtype = expectDatetimeToDscType(expectedType);
	Format::Patterns formatPatterns = Format::NONE;
	std::vector<Token> tokens = parseStringToDateTimeFormat(&tempDesc, formatUpper, formatPatterns, cb);
	validateFormatFlags(formatPatterns, cb);

	processStringToDateTimeTokens(tokens, stringUpper, cvtData, cb);

	ISC_TIMESTAMP_TZ timestampTZ = constructTimeStampTz(cvtData, cb);
	timeStampToUtc(timestampTZ, cb->getSessionTimeZone(), expectedType, cb);
	validateTimeStamp(timestampTZ.utc_timestamp, expectedType, desc, cb);

	return timestampTZ;
}
