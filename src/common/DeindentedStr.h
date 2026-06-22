/*
 *	PROGRAM:		Firebird common classes
 *	MODULE:			DeindentedStr.h
 *	DESCRIPTION:	Compile-time deindentation for multi-line strings
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
 *  Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_DEINDENTED_STR_H
#define COMMON_DEINDENTED_STR_H


namespace Firebird
{
	template <unsigned N>
	struct DeindentedStr
	{
		constexpr const char* c_str() const
		{
			return text;
		}

		char text[N]{};
	};

	namespace DeindentedStrDetailImpl
	{
		constexpr bool isLineBreak(char c)
		{
			return c == '\n' || c == '\r';
		}

		constexpr bool isBlankLineChar(char c)
		{
			return c == ' ' || c == '\t';
		}

		template <unsigned N>
		constexpr bool isEmptyLine(const char (&text)[N], unsigned start)
		{
			for (auto p = start; p < N && text[p] != '\0' && !isLineBreak(text[p]); ++p)
			{
				if (!isBlankLineChar(text[p]))
					return false;
			}

			return true;
		}

		template <unsigned N>
		constexpr unsigned skipLineBreak(const char (&text)[N], unsigned p)
		{
			if (p < N && text[p] == '\r')
				++p;

			if (p < N && text[p] == '\n')
				++p;

			return p;
		}
	}	// namespace DeindentedStrDetailImpl

	template <unsigned N>
	constexpr DeindentedStr<N> deindentStr(const char (&text)[N])
	{
		DeindentedStr<N> ret;

		unsigned inputPos = 0;

		while (inputPos < N && text[inputPos] != '\0' && DeindentedStrDetailImpl::isEmptyLine(text, inputPos))
		{
			while (inputPos < N && text[inputPos] != '\0' && !DeindentedStrDetailImpl::isLineBreak(text[inputPos]))
				++inputPos;

			inputPos = DeindentedStrDetailImpl::skipLineBreak(text, inputPos);
		}

		unsigned indent = 0;

		while (inputPos + indent < N && DeindentedStrDetailImpl::isBlankLineChar(text[inputPos + indent]))
			++indent;

		unsigned outputPos = 0;
		unsigned lastNonEmptyOutputPos = 0;
		bool lineHasData = false;
		bool lineStart = true;

		while (inputPos < N && text[inputPos] != '\0')
		{
			if (lineStart)
			{
				unsigned skipped = 0;

				while (skipped < indent && inputPos < N && DeindentedStrDetailImpl::isBlankLineChar(text[inputPos]))
				{
					++inputPos;
					++skipped;
				}

				lineStart = false;
			}

			if (text[inputPos] == '\r')
			{
				ret.text[outputPos++] = text[inputPos++];

				if (inputPos < N && text[inputPos] == '\n')
					ret.text[outputPos++] = text[inputPos++];

				if (lineHasData)
					lastNonEmptyOutputPos = outputPos;

				lineHasData = false;
				lineStart = true;
				continue;
			}

			if (text[inputPos] == '\n')
			{
				ret.text[outputPos++] = text[inputPos++];

				if (lineHasData)
					lastNonEmptyOutputPos = outputPos;

				lineHasData = false;
				lineStart = true;
				continue;
			}

			if (!DeindentedStrDetailImpl::isBlankLineChar(text[inputPos]))
				lineHasData = true;

			ret.text[outputPos++] = text[inputPos++];
		}

		if (lineHasData)
			lastNonEmptyOutputPos = outputPos;

		ret.text[lastNonEmptyOutputPos] = '\0';

		return ret;
	}
}	// namespace Firebird

#endif	// COMMON_DEINDENTED_STR_H
