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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "../common/utils_proto.h"
#include "../isql/FrontendLexer.h"
#include <algorithm>
#include <cctype>


std::string FrontendLexer::trim(std::string_view str)
{
	auto finish = str.end();
	auto start = str.begin();

	while (start != finish && fb_utils::isspace(*start))
		++start;

	if (start == finish)
		return {};

	--finish;

	while (finish > start && fb_utils::isspace(*finish))
		--finish;

	return std::string(start, finish + 1);
}


std::string FrontendLexer::stripComments(std::string_view statement)
{
	try
	{
		// Statement may end with a line comment without a newline.
		// Add it here so it may be skipped correctly.
		FrontendLexer lexer(std::string(statement) + "\n");
		std::string processedStatement;

		while (lexer.pos < lexer.end)
		{
			auto oldPos = lexer.pos;

			lexer.skipSpacesAndComments();

			if (lexer.pos > oldPos)
				processedStatement += ' ';

			oldPos = lexer.pos;

			if (!lexer.getStringToken().has_value() && lexer.pos < lexer.end)
				++lexer.pos;

			processedStatement += std::string(oldPos, lexer.pos);
		}

		return trim(processedStatement);
	}
	catch (const IncompleteTokenError& error)
	{
		return trim(statement);
	}
}

bool FrontendLexer::isBufferEmpty() const
{
	return trim(std::string(deletePos, end)).empty();
}

void FrontendLexer::appendBuffer(std::string_view newBuffer)
{
	const auto posIndex = pos - buffer.begin();
	const auto deletePosIndex = deletePos - buffer.begin();
	buffer.append(newBuffer);
	pos = buffer.begin() + posIndex;
	end = buffer.end();
	deletePos = buffer.begin() + deletePosIndex;
}

void FrontendLexer::reset()
{
	buffer.clear();
	pos = buffer.begin();
	end = buffer.end();
	deletePos = buffer.begin();
}

std::variant<FrontendLexer::SingleStatement, FrontendLexer::IncompleteTokenError> FrontendLexer::getSingleStatement(
	std::string_view term)
{
	const auto posIndex = pos - deletePos;
	buffer.erase(buffer.begin(), deletePos);
	pos = buffer.begin() + posIndex;
	end = buffer.end();
	deletePos = buffer.begin();

	try
	{
		if (pos < end)
		{
			skipSpacesAndComments();

			const auto savePos = pos;

			if (end - pos > 1 && *pos == '?')
			{
				if (*++pos == '\r')
					++pos;

				if (pos < end && *pos == '\n')
				{
					deletePos = ++pos;
					const auto statement = trim(std::string(buffer.cbegin(), pos));
					return SingleStatement{statement, statement};
				}
			}

			pos = savePos;
		}

		while (pos < end)
		{
			if (std::size_t(end - pos) >= term.length() && std::equal(term.begin(), term.end(), pos))
			{
				const auto initialStatement = std::string(buffer.cbegin(), pos);
				pos += term.length();
				const auto trailingPos = pos;
				skipSpacesAndComments();
				deletePos = pos;

				const auto statement1 = initialStatement + std::string(trailingPos, pos);
				const auto statement2 = initialStatement + ";" + std::string(trailingPos, pos);

				return SingleStatement{trim(statement1), trim(statement2)};
			}

			if (!getStringToken().has_value() && pos < end)
				++pos;

			skipSpacesAndComments();
		}
	}
	catch (const IncompleteTokenError& error)
	{
		return error;
	}

	return IncompleteTokenError{false};
}

FrontendLexer::Token FrontendLexer::getToken()
{
	skipSpacesAndComments();

	Token token;

	if (pos >= end)
	{
		token.type = Token::TYPE_EOF;
		return token;
	}

	if (const auto optStringToken = getStringToken(); optStringToken.has_value())
		return optStringToken.value();

	const auto start = pos;

	switch (toupper(*pos))
	{
		case ';':
		case '.':
			token.type = Token::TYPE_OTHER;
			token.processedText = *pos++;
			break;

		default:
			while (pos != end && !fb_utils::isspace(*pos))
				++pos;

			token.processedText = std::string(start, pos);
			std::transform(token.processedText.begin(), token.processedText.end(),
				token.processedText.begin(), toupper);
			break;
	}

	token.rawText = std::string(start, pos);

	return token;
}

FrontendLexer::Token FrontendLexer::getNameToken()
{
	skipSpacesAndComments();

	Token token;

	if (pos >= end)
	{
		token.type = Token::TYPE_EOF;
		return token;
	}

	if (const auto optStringToken = getStringToken(); optStringToken.has_value())
		return optStringToken.value();

	/*** Revert to strict parsing with schemas support branch.
	const auto start = pos;
	bool first = true;

	while (pos < end)
	{
		const auto c = *pos++;

		if (!((c >= 'A' && c <= 'Z') ||
			  (c >= 'a' && c <= 'z') ||
			  c == '{' ||
			  c == '}' ||
			  (!first && c >= '0' && c <= '9') ||
			  (!first && c == '$') ||
			  (!first && c == '_')))
		{
			if (!first)
				--pos;

			break;
		}

		first = false;
	}

	token.processedText = token.rawText = std::string(start, pos);
	std::transform(token.processedText.begin(), token.processedText.end(),
		token.processedText.begin(), toupper);

	return token;
	***/

	const auto start = pos;

	switch (toupper(*pos))
	{
		case ';':
			token.type = Token::TYPE_OTHER;
			token.processedText = *pos++;
			break;

		default:
			while (pos != end && !fb_utils::isspace(*pos) && *pos != '.')
				++pos;

			token.processedText = std::string(start, pos);
			std::transform(token.processedText.begin(), token.processedText.end(),
				token.processedText.begin(), toupper);
			break;
	}

	token.rawText = std::string(start, pos);

	return token;
}

std::optional<FrontendLexer::Token> FrontendLexer::getStringToken()
{
	if (pos >= end)
		return std::nullopt;

	Token token;
	const auto start = pos;

	switch (toupper(*pos))
	{
		case '\'':
		case '"':
		{
			const auto quote = *pos++;

			while (pos != end)
			{
				if (*pos == quote)
				{
					if ((pos + 1) < end && *(pos + 1) == quote)
						++pos;
					else
						break;
				}

				token.processedText += *pos++;
			}

			if (pos == end)
			{
				pos = start;
				throw IncompleteTokenError{false};
			}
			else
			{
				++pos;
				token.type = quote == '\'' ? Token::TYPE_STRING : Token::TYPE_META_STRING;
			}

			break;
		}

		case 'Q':
			if (pos + 1 != end && pos[1] == '\'')
			{
				if (pos + 4 < end)
				{
					char endChar;

					switch (pos[2])
					{
						case '{':
							endChar = '}';
							break;

						case '[':
							endChar = ']';
							break;

						case '(':
							endChar = ')';
							break;

						case '<':
							endChar = '>';
							break;

						default:
							endChar = pos[2];
							break;
					}

					pos += 3;

					while (pos + 1 < end)
					{
						if (*pos == endChar && pos[1] == '\'')
						{
							pos += 2;
							token.type = Token::TYPE_STRING;
							break;
						}

						token.processedText += *pos++;
					}
				}

				if (token.type != Token::TYPE_STRING)
				{
					pos = start;
					throw IncompleteTokenError{false};
				}

				break;
			}
			[[fallthrough]];

		default:
			return std::nullopt;
	}

	token.rawText = std::string(start, pos);

	return token;
}

void FrontendLexer::skipSpacesAndComments()
{
	while (pos != end && (fb_utils::isspace(*pos) || *pos == '-' || *pos == '/'))
	{
		while (pos != end && fb_utils::isspace(*pos))
			++pos;

		if (pos == end)
			break;

		if (*pos == '-')
		{
			const auto start = pos;

			if (pos + 1 != end && pos[1] == '-')
			{
				bool finished = false;
				pos += 2;

				while (pos != end)
				{
					const auto c = *pos++;

					if (c == '\r')
					{
						if (pos != end && *pos == '\n')
							++pos;

						finished = true;
						break;
					}
					else if (c == '\n')
					{
						finished = true;
						break;
					}
				}

				if (!finished)
				{
					pos = start;
					throw IncompleteTokenError{true};
				}
			}
			else
				break;
		}
		else if (*pos == '/')
		{
			const auto start = pos;

			if (pos + 1 != end && pos[1] == '*')
			{
				bool finished = false;
				pos += 2;

				while (pos != end)
				{
					const auto c = *pos++;

					if (c == '*' && pos != end && *pos == '/')
					{
						++pos;
						finished = true;
						break;
					}
				}

				if (!finished)
				{
					pos = start;
					throw IncompleteTokenError{true};
				}
			}
			else
				break;
		}
	}
}
