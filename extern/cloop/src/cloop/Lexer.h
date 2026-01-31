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
 *  The Original Code was created by Adriano dos Santos Fernandes.
 *
 *  Copyright (c) 2014 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLOOP_LEXER_H
#define CLOOP_LEXER_H

#include <stack>
#include <string>
#include <stdio.h>


struct Token
{
	enum class Type
	{
		END_OF_FILE = 256,
		IDENTIFIER,
		// punctuation
		DOUBLE_COLON,
		// literals
		BOOLEAN_LITERAL,
		INT_LITERAL,
		// keywords
		CONST,
		EXCEPTION,
		INTERFACE,
		NOT_IMPLEMENTED,
		NOT_IMPLEMENTED_ACTION,
		STUB,
		STRUCT,
		TYPEDEF,
		VERSION,
		ON_ERROR,
		IF,
		THEN,
		ELSE,
		ENDIF,
		CALL,
		DEFAULT_ACTION,
		// types
		VOID,
		BOOLEAN,
		INT,
		INT64,
		INTPTR,
		STRING,
		UCHAR,
		UINT,
		UINT64
	};

	Type type;
	std::string text;
	unsigned line;
	unsigned column;
};


class Lexer final
{
private:
	struct Char final
	{
		int c;
		unsigned line;
		unsigned column;
	};

public:
	explicit Lexer(const std::string& filename);
	~Lexer();

public:
	Token& getToken(Token& token);
	void pushToken(const Token& token);

private:
	void skip(Char& ch);
	Char& getChar(Char& ch);
	void ungetChar(const Char& ch);

public:
	const std::string filename;

private:
	FILE* in;
	unsigned line = 1;
	unsigned column = 1;
	std::stack<Token> tokens;
};


inline constexpr Token::Type TOKEN(char c)
{
	return static_cast<Token::Type>(c);
}


#endif  // CLOOP_LEXER_H
