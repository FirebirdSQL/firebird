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


#define TOKEN(c)	static_cast< ::Token::Type>(c)


struct Token
{
	enum Type
	{
		TYPE_EOF = 256,
		TYPE_IDENTIFIER,
		// punctuation
		TYPE_DOUBLE_COLON,
		// literals
		TYPE_BOOLEAN_LITERAL,
		TYPE_INT_LITERAL,
		// keywords
		TYPE_CONST,
		TYPE_EXCEPTION,
		TYPE_INTERFACE,
		TYPE_NOT_IMPLEMENTED,
		TYPE_STRUCT,
		TYPE_TYPEDEF,
		TYPE_VERSION,
		TYPE_ON_ERROR,
		// types
		TYPE_VOID,
		TYPE_BOOLEAN,
		TYPE_INT,
		TYPE_INT64,
		TYPE_INTPTR,
		TYPE_STRING,
		TYPE_UCHAR,
		TYPE_UINT,
		TYPE_UINT64
	};

	Type type;
	std::string text;
	unsigned line;
	unsigned column;
};


class Lexer
{
private:
	struct Char
	{
		int c;
		unsigned line;
		unsigned column;
	};

public:
	Lexer(const std::string& filename);
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
	unsigned line, column;
	std::stack<Token> tokens;
};


#endif	// CLOOP_LEXER_H
