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

#ifndef FB_ISQL_FRONTEND_LEXER_H
#define FB_ISQL_FRONTEND_LEXER_H

#include <optional>
#include <string>
#include <string_view>
#include <variant>

class FrontendLexer
{
public:
	struct Token
	{
		enum Type
		{
			TYPE_EOF,
			TYPE_STRING,
			TYPE_META_STRING,
			TYPE_OTHER
		};

		Type type = TYPE_OTHER;
		std::string rawText;
		std::string processedText;

		const std::string& getProcessedString() const
		{
			return type == FrontendLexer::Token::TYPE_STRING || type == FrontendLexer::Token::TYPE_META_STRING ?
				processedText : rawText;
		}
	};

	struct SingleStatement
	{
		std::string withoutSemicolon;
		std::string withSemicolon;
	};

	struct IncompleteTokenError
	{
		bool insideComment;
	};

public:
	FrontendLexer(std::string_view aBuffer = {})
		: buffer(aBuffer),
		  pos(buffer.begin()),
		  end(buffer.end()),
		  deletePos(buffer.begin())
	{
	}

	FrontendLexer(const FrontendLexer&) = delete;
	FrontendLexer& operator=(const FrontendLexer&) = delete;

public:
	static std::string trim(std::string_view str);
	static std::string stripComments(std::string_view statement);

public:
	auto getBuffer() const
	{
		return buffer;
	}

	auto getPos() const
	{
		return pos;
	}

	void setPos(std::string::const_iterator newPos)
	{
		pos = newPos;
	}

	void rewind()
	{
		deletePos = buffer.begin();
	}

	bool isBufferEmpty() const;

	void appendBuffer(std::string_view newBuffer);
	void reset();
	std::variant<SingleStatement, FrontendLexer::IncompleteTokenError> getSingleStatement(std::string_view term);

	Token getToken();
	Token getNameToken();

private:
	std::optional<Token> getStringToken();
	void skipSpacesAndComments();

private:
	std::string buffer;
	std::string::const_iterator pos;
	std::string::const_iterator end;
	std::string::const_iterator deletePos;
};

#endif	// FB_ISQL_FRONTEND_LEXER_H
