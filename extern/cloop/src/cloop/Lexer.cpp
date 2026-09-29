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

#include "Lexer.h"
#include <format>
#include <stdexcept>

using std::format;
using std::runtime_error;
using std::string;


//--------------------------------------


Lexer::Lexer(const string& filename)
	: filename(filename)
{
	in = fopen(filename.c_str(), "r");

	if (!in)
		throw runtime_error(string("Input file not found: ") + filename + ".");
}

Lexer::~Lexer()
{
	fclose(in);
}

Token& Lexer::getToken(Token& token)
{
	if (!tokens.empty())
	{
		token = tokens.top();
		tokens.pop();
		return token;
	}

	token.text = "";

	Char ch;
	skip(ch);

	token.line = ch.line;
	token.column = ch.column;

	if (ch.c == -1)
	{
		token.type = Token::Type::END_OF_FILE;
		return token;
	}
	else if ((ch.c >= 'a' && ch.c <= 'z') || (ch.c >= 'A' && ch.c <= 'Z') || ch.c == '_')
	{
		while (
			(ch.c >= 'a' && ch.c <= 'z') || (ch.c >= 'A' && ch.c <= 'Z') || ch.c == '_' || (ch.c >= '0' && ch.c <= '9'))
		{
			token.text += static_cast<char>(ch.c);
			getChar(ch);
		}

		ungetChar(ch);

		// literals
		if (token.text == "false" || token.text == "true")
			token.type = Token::Type::BOOLEAN_LITERAL;
		// keywords
		else if (token.text == "const")
			token.type = Token::Type::CONST;
		else if (token.text == "exception")
			token.type = Token::Type::EXCEPTION;
		else if (token.text == "interface")
			token.type = Token::Type::INTERFACE;
		else if (token.text == "notImplemented")
			token.type = Token::Type::NOT_IMPLEMENTED;
		else if (token.text == "notImplementedAction")
			token.type = Token::Type::NOT_IMPLEMENTED_ACTION;
		else if (token.text == "struct")
			token.type = Token::Type::STRUCT;
		else if (token.text == "typedef")
			token.type = Token::Type::TYPEDEF;
		else if (token.text == "version")
			token.type = Token::Type::VERSION;
		else if (token.text == "onError")
			token.type = Token::Type::ON_ERROR;
		else if (token.text == "if")
			token.type = Token::Type::IF;
		else if (token.text == "then")
			token.type = Token::Type::THEN;
		else if (token.text == "else")
			token.type = Token::Type::ELSE;
		else if (token.text == "endif")
			token.type = Token::Type::ENDIF;
		else if (token.text == "call")
			token.type = Token::Type::CALL;
		else if (token.text == "defaultAction")
			token.type = Token::Type::DEFAULT_ACTION;
		else if (token.text == "stub")
			token.type = Token::Type::STUB;
		// types
		else if (token.text == "void")
			token.type = Token::Type::VOID;
		else if (token.text == "boolean")
			token.type = Token::Type::BOOLEAN;
		else if (token.text == "int")
			token.type = Token::Type::INT;
		else if (token.text == "int64")
			token.type = Token::Type::INT64;
		else if (token.text == "intptr")
			token.type = Token::Type::INTPTR;
		else if (token.text == "string")
			token.type = Token::Type::STRING;
		else if (token.text == "uchar")
			token.type = Token::Type::UCHAR;
		else if (token.text == "uint")
			token.type = Token::Type::UINT;
		else if (token.text == "uint64")
			token.type = Token::Type::UINT64;
		else
			token.type = Token::Type::IDENTIFIER;
	}
	else if (ch.c >= '0' && ch.c <= '9')
	{
		token.type = Token::Type::INT_LITERAL;
		token.text += static_cast<char>(ch.c);

		if ((getChar(ch).c == 'x' || ch.c == 'X') && token.text[0] == '0')
		{
			token.text += static_cast<char>(ch.c);

			while ((getChar(ch).c >= '0' && ch.c <= '9') || (tolower(ch.c) >= 'a' && tolower(ch.c) <= 'f'))
			{
				token.text += static_cast<char>(ch.c);
			}

			if (token.text.length() == 2)
			{
				throw runtime_error(format("{}:{}:{}: error: Invalid hexadecimal prefix.", filename, line, column));
			}
		}
		else
		{
			ungetChar(ch);

			while (getChar(ch).c >= '0' && ch.c <= '9')
				token.text += static_cast<char>(ch.c);
		}

		ungetChar(ch);
	}
	else
	{
		token.type = static_cast<Token::Type>(ch.c);
		token.text = static_cast<char>(ch.c);

		if (getChar(ch).c == ':')
		{
			token.type = Token::Type::DOUBLE_COLON;
			token.text += static_cast<char>(ch.c);
		}
		else
			ungetChar(ch);
	}

	return token;
}

void Lexer::pushToken(const Token& token)
{
	tokens.push(token);
}

void Lexer::skip(Char& ch)  // skip spaces and comments
{
	while (true)
	{
		while (getChar(ch).c == ' ' || ch.c == '\t' || ch.c == '\r' || ch.c == '\n')
			;

		// check for comments

		if (ch.c != '/')
			return;

		Char firstCh = ch;
		getChar(ch);

		switch (ch.c)
		{
			case '*':
			{
				bool inComment = true;

				while (inComment)
				{
					while (getChar(ch).c != '*' && ch.c != -1)
						;

					if (ch.c == -1)
					{
						throw runtime_error(
							format("{}:{}:{}: error: Unterminated comment.", filename, firstCh.line, firstCh.column));
					}
					else
					{
						getChar(ch);

						if (ch.c == '/')
							inComment = false;
						else
							ungetChar(ch);
					}
				}

				break;
			}

			case '/':
				while (getChar(ch).c != '\n' && ch.c != -1)
					;

				break;

			default:  // not a comment
				ungetChar(ch);
				ch = firstCh;
				return;
		}
	}

	// should never be here
}

Lexer::Char& Lexer::getChar(Char& ch)
{
	ch.c = fgetc(in);
	ch.line = line;
	ch.column = column;

	if (ch.c == '\n')
	{
		++line;
		column = 1;
	}
	else
		++column;

	return ch;
}

void Lexer::ungetChar(const Char& ch)
{
	ungetc(ch.c, in);
	line = ch.line;
	column = ch.column;
}
