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

#include "Parser.h"
#include "Expr.h"
#include <stdexcept>
#include <stdlib.h>
#include <string.h>

using std::map;
using std::pair;
using std::runtime_error;
using std::string;
using std::vector;


//--------------------------------------


Parser::Parser(Lexer* lexer)
	: exceptionInterface(NULL),
	  lexer(lexer),
	  interface(NULL)
{
}

void Parser::parse()
{
	interface = NULL;

	while (true)
	{
		bool exception = false;
		lexer->getToken(token);

		if (token.type == Token::TYPE_EOF)
			break;
		else if (token.type == TOKEN('['))
		{
			getToken(token, Token::TYPE_EXCEPTION);	// This is the only attribute we allow now.
			exception = true;
			getToken(token, TOKEN(']'));
		}
		else
			lexer->pushToken(token);

		lexer->getToken(token);

		switch (token.type)
		{
			case Token::TYPE_INTERFACE:
				parseInterface(exception);
				break;

			case Token::TYPE_STRUCT:
				if (exception)
					error(token, "Cannot use attribute exception in struct.");
				parseStruct();
				break;

			case Token::TYPE_TYPEDEF:
				if (exception)
					error(token, "Cannot use attribute exception in typedef.");
				parseTypedef();
				break;

			default:
				syntaxError(token);
				break;
		}
	}

	// Check types.

	for (vector<Interface*>::iterator i = interfaces.begin(); i != interfaces.end(); ++i)
	{
		Interface* interface = *i;

		for (vector<Method*>::iterator j = interface->methods.begin();
			 j != interface->methods.end();
			 ++j)
		{
			Method* method = *j;

			checkType(method->returnTypeRef);

			for (vector<Parameter*>::iterator k = method->parameters.begin();
				 k != method->parameters.end();
				 ++k)
			{
				Parameter* parameter = *k;
				checkType(parameter->typeRef);
			}
		}
	}
}

void Parser::parseInterface(bool exception)
{
	interface = new Interface();
	interfaces.push_back(interface);

	interface->name = getToken(token, Token::TYPE_IDENTIFIER).text;
	typesByName.insert(pair<string, BaseType*>(interface->name, interface));

	if (exception)
		exceptionInterface = interface;

	if (lexer->getToken(token).type == TOKEN(':'))
	{
		string superName = getToken(token, Token::TYPE_IDENTIFIER).text;
		map<string, BaseType*>::iterator it = typesByName.find(superName);

		if (it == typesByName.end() || it->second->type != BaseType::TYPE_INTERFACE)
			error(token, string("Super interface '") + superName + "' not found.");

		interface->super = static_cast<Interface*>(it->second);
		interface->version = interface->super->version + 1;
	}
	else
		lexer->pushToken(token);

	getToken(token, TOKEN('{'));

	while (lexer->getToken(token).type != TOKEN('}'))
	{
		if (token.type == Token::TYPE_VERSION)
		{
			getToken(token, TOKEN(':'));
			++interface->version;
		}
		else
			lexer->pushToken(token);

		parseItem();
	}
}

void Parser::parseStruct()
{
	Struct* ztruct = new Struct();

	ztruct->name = getToken(token, Token::TYPE_IDENTIFIER).text;
	typesByName.insert(pair<string, BaseType*>(ztruct->name, ztruct));

	getToken(token, TOKEN(';'));
}

void Parser::parseTypedef()
{
	Typedef* typeDef = new Typedef();

	typeDef->name = getToken(token, Token::TYPE_IDENTIFIER).text;
	typesByName.insert(pair<string, BaseType*>(typeDef->name, typeDef));

	getToken(token, TOKEN(';'));
}

void Parser::parseItem()
{
	Expr* notImplementedExpr = NULL;
	std::string onError;

	while (lexer->getToken(token).type == TOKEN('['))
	{
		lexer->getToken(token);
		switch (token.type)
		{
			case Token::TYPE_NOT_IMPLEMENTED:
				if (notImplementedExpr)
					syntaxError(token);
				getToken(token, TOKEN('('));
				notImplementedExpr = parseExpr();
				getToken(token, TOKEN(')'));
				getToken(token, TOKEN(']'));
				break;

			case Token::TYPE_ON_ERROR:
				if (onError.length())
					syntaxError(token);
				lexer->getToken(token);
				if (token.type != Token::TYPE_IDENTIFIER)
					syntaxError(token);
				onError = token.text;
				getToken(token, TOKEN(']'));
				break;

			default:
				syntaxError(token);
				break;
		}
	}
	lexer->pushToken(token);

	TypeRef typeRef(parseTypeRef());
	string name(getToken(token, Token::TYPE_IDENTIFIER).text);

	if ((!(notImplementedExpr || onError.length())) && typeRef.isConst)
	{
		if (lexer->getToken(token).type == TOKEN('='))
		{
			typeRef.isConst = false;
			parseConstant(typeRef, name);
			return;
		}
		else
			lexer->pushToken(token);
	}

	getToken(token, TOKEN('('));
	parseMethod(typeRef, name, notImplementedExpr, onError);
}

void Parser::parseConstant(const TypeRef& typeRef, const string& name)
{
	Constant* constant = new Constant();
	interface->constants.push_back(constant);

	constant->typeRef = typeRef;
	constant->name = name;
	constant->expr = parseExpr();

	getToken(token, TOKEN(';'));
}

void Parser::parseMethod(const TypeRef& returnTypeRef, const string& name, Expr* notImplementedExpr, const string& onError)
{
	Method* method = new Method();
	interface->methods.push_back(method);

	method->returnTypeRef = returnTypeRef;
	method->name = name;
	method->version = interface->version;
	method->notImplementedExpr = notImplementedExpr;
	method->onErrorFunction = onError;

	if (lexer->getToken(token).type != TOKEN(')'))
	{
		lexer->pushToken(token);

		while (true)
		{
			Parameter* parameter = new Parameter();
			method->parameters.push_back(parameter);

			parameter->typeRef = parseTypeRef();
			parameter->name = getToken(token, Token::TYPE_IDENTIFIER).text;

			lexer->getToken(token);
			lexer->pushToken(token);

			if (token.type == TOKEN(')'))
				break;

			getToken(token, TOKEN(','));
		}

		getToken(token, TOKEN(')'));
	}

	if (lexer->getToken(token).type == Token::TYPE_CONST)
		method->isConst = true;
	else
		lexer->pushToken(token);

	getToken(token, TOKEN(';'));
}

Expr* Parser::parseExpr()
{
	return parseLogicalExpr();
}

Expr* Parser::parseLogicalExpr()
{
	Expr* expr = parseUnaryExpr();

	if (lexer->getToken(token).type == TOKEN('|'))
		expr = new BitwiseOrExpr(expr, parseExpr());
	else
		lexer->pushToken(token);

	return expr;
}

Expr* Parser::parseUnaryExpr()
{
	lexer->getToken(token);

	if (token.type == TOKEN('-'))
		return new NegateExpr(parsePrimaryExpr());
	else
	{
		lexer->pushToken(token);
		return parsePrimaryExpr();
	}
}

Expr* Parser::parsePrimaryExpr()
{
	lexer->getToken(token);

	switch (token.type)
	{
		case Token::TYPE_BOOLEAN_LITERAL:
			return new BooleanLiteralExpr(token.text == "true");

		case Token::TYPE_INT_LITERAL:
		{
			const char* p = token.text.c_str();
			size_t len = strlen(p);
			int base = len > 2 && tolower(p[1]) == 'x' ? 16 : 10;
			long long val = strtoll(p, NULL, base);

			return new IntLiteralExpr((int) val, base == 16);
		}

		case Token::TYPE_IDENTIFIER:
		{
			string text = token.text;

			if (lexer->getToken(token).type == Token::TYPE_DOUBLE_COLON)
			{
				getToken(token, Token::TYPE_IDENTIFIER);
				map<string, BaseType*>::iterator it = typesByName.find(text);

				if (it == typesByName.end() || it->second->type != BaseType::TYPE_INTERFACE)
					error(token, string("Interface '") + text + "' not found.");

				return new ConstantExpr(static_cast<Interface*>(it->second), token.text);
			}
			else
			{
				lexer->pushToken(token);
				return new ConstantExpr(interface, text);
			}
		}

		default:
			syntaxError(token);
			return NULL;	// warning
	}
}

void Parser::checkType(TypeRef& typeRef)
{
	if (typeRef.token.type == Token::TYPE_IDENTIFIER)
	{
		map<string, BaseType*>::iterator it = typesByName.find(typeRef.token.text);

		if (it != typesByName.end())
			typeRef.type = it->second->type;
		else
			error(typeRef.token, string("Interface/struct '") + typeRef.token.text + "' not found.");
	}
}

Token& Parser::getToken(Token& token, Token::Type expected, bool allowEof)
{
	lexer->getToken(token);

	if (token.type != expected && !(allowEof && token.type == Token::TYPE_EOF))
		syntaxError(token);

	return token;
}

TypeRef Parser::parseTypeRef()
{
	TypeRef typeRef;
	lexer->getToken(typeRef.token);

	if (typeRef.token.type == Token::TYPE_CONST)
	{
		typeRef.isConst = true;
		lexer->getToken(typeRef.token);
	}

	switch (typeRef.token.type)
	{
		case Token::TYPE_VOID:
		case Token::TYPE_BOOLEAN:
		case Token::TYPE_INT:
		case Token::TYPE_INT64:
		case Token::TYPE_INTPTR:
		case Token::TYPE_STRING:
		case Token::TYPE_UCHAR:
		case Token::TYPE_UINT:
		case Token::TYPE_UINT64:
		case Token::TYPE_IDENTIFIER:
			break;

		default:
			error(typeRef.token, string("Syntax error at '") +
				typeRef.token.text + "'. Expected a type.");
			break;
	}

	Token token2;
	lexer->getToken(token2);
	if (token2.type == TOKEN('*'))
		typeRef.isPointer = true;
	else
		lexer->pushToken(token2);

	return typeRef;
}

void Parser::syntaxError(const Token& token)
{
	error(token, string("Syntax error at '") + token.text + "'.");
}

void Parser::error(const Token& token, const string& msg)
{
	char buffer[1024];
	sprintf(buffer, "%s:%i:%i: error: %s",
		lexer->filename.c_str(), token.line, token.column, msg.c_str());
	throw runtime_error(buffer);
}
