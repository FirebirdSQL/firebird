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
#include <format>
#include <cstdint>
#include <memory>
#include <utility>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>

using std::format;
using std::map;
using std::runtime_error;
using std::string;
using std::vector;


//--------------------------------------


Parser::Parser(Lexer* lexer)
	: lexer(lexer)
{
}

void Parser::parse()
{
	interface = nullptr;

	while (true)
	{
		bool exception = false;
		lexer->getToken(token);

		if (token.type == Token::Type::END_OF_FILE)
			break;
		else if (token.type == TOKEN('['))
		{
			getToken(token, Token::Type::EXCEPTION);  // This is the only attribute we allow now.
			exception = true;
			getToken(token, TOKEN(']'));
		}
		else
			lexer->pushToken(token);

		lexer->getToken(token);

		switch (token.type)
		{
			case Token::Type::INTERFACE:
				parseInterface(exception);
				break;

			case Token::Type::STRUCT:
				if (exception)
					error(token, "Cannot use attribute exception in struct.");
				parseStruct();
				break;

			case Token::Type::TYPEDEF:
				if (exception)
					error(token, "Cannot use attribute exception in typedef.");
				parseTypedef();
				break;

			case Token::Type::BOOLEAN:
				if (exception)
					error(token, "Cannot use attribute exception in boolean.");
				parseBoolean();
				break;

			default:
				syntaxError(token);
				break;
		}
	}

	// Check types, assign statusName to methods.

	for (const auto& interface : interfaces)
	{
		for (const auto& method : interface->methods)
		{
			checkType(method->returnTypeRef);

			for (const auto& parameterPtr : method->parameters)
				checkType(parameterPtr->typeRef);

			if (!method->parameters.empty() && exceptionInterface &&
				method->parameters.front()->typeRef.token.text == exceptionInterface->name)
			{
				method->statusName = method->parameters.front()->name;
			}
		}
	}
}

void Parser::parseInterface(bool exception)
{
	auto newInterface = std::make_shared<Interface>();
	newInterface->name = getToken(token, Token::Type::IDENTIFIER).text;

	if (typesByName.find(newInterface->name) != typesByName.end())
		error(token, string("Type '") + newInterface->name + "' already defined.");

	typesByName.emplace(newInterface->name, newInterface);
	interface = newInterface.get();
	interfaces.push_back(newInterface);

	if (exception)
		exceptionInterface = interface;

	if (lexer->getToken(token).type == TOKEN(':'))
	{
		string superName = getToken(token, Token::Type::IDENTIFIER).text;
		auto superIt = typesByName.find(superName);

		if (superIt == typesByName.end() || superIt->second->type != BaseType::Type::INTERFACE)
			error(token, string("Super interface '") + superName + "' not found.");

		interface->super = static_cast<Interface*>(superIt->second.get());
		interface->version = interface->super->version + 1;
	}
	else
		lexer->pushToken(token);

	getToken(token, TOKEN('{'));

	while (lexer->getToken(token).type != TOKEN('}'))
	{
		if (token.type == Token::Type::VERSION)
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
	auto ztruct = std::make_shared<Struct>();
	ztruct->name = getToken(token, Token::Type::IDENTIFIER).text;

	if (typesByName.find(ztruct->name) != typesByName.end())
		error(token, string("Type '") + ztruct->name + "' already defined.");

	typesByName.emplace(ztruct->name, std::move(ztruct));

	getToken(token, TOKEN(';'));
}

void Parser::parseBoolean()
{
	auto b = std::make_shared<Boolean>();
	b->name = getToken(token, Token::Type::IDENTIFIER).text;

	if (typesByName.find(b->name) != typesByName.end())
		error(token, string("Type '") + b->name + "' already defined.");

	typesByName.emplace(b->name, std::move(b));

	getToken(token, TOKEN(';'));
}

void Parser::parseTypedef()
{
	auto typeDef = std::make_shared<Typedef>();
	typeDef->name = getToken(token, Token::Type::IDENTIFIER).text;

	if (typesByName.find(typeDef->name) != typesByName.end())
		error(token, string("Type '") + typeDef->name + "' already defined.");

	typesByName.emplace(typeDef->name, std::move(typeDef));

	getToken(token, TOKEN(';'));
}

void Parser::parseItem()
{
	std::unique_ptr<Expr> notImplementedExpr;
	std::unique_ptr<Action> notImplementedAction;
	std::unique_ptr<Action> stubAction;
	std::string onError;

	while (lexer->getToken(token).type == TOKEN('['))
	{
		lexer->getToken(token);
		switch (token.type)
		{
			case Token::Type::NOT_IMPLEMENTED:
				if (notImplementedExpr)
					syntaxError(token);
				getToken(token, TOKEN('('));
				notImplementedExpr = parseExpr();
				getToken(token, TOKEN(')'));
				break;

			case Token::Type::ON_ERROR:
				if (onError.length())
					syntaxError(token);
				lexer->getToken(token);
				if (token.type != Token::Type::IDENTIFIER)
					syntaxError(token);
				onError = token.text;
				break;

			case Token::Type::NOT_IMPLEMENTED_ACTION:
				if (notImplementedAction)
					syntaxError(token);
				notImplementedAction = parseAction(DefAction::DefType::NOT_IMPLEMENTED);
				break;

			case Token::Type::STUB:
				if (stubAction)
					syntaxError(token);
				stubAction = parseAction(DefAction::DefType::IGNORE);
				break;

			default:
				syntaxError(token);
				break;
		}
		getToken(token, TOKEN(']'));
	}
	lexer->pushToken(token);

	TypeRef typeRef(parseTypeRef());
	string name(getToken(token, Token::Type::IDENTIFIER).text);

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
	parseMethod(
		typeRef, name, std::move(notImplementedExpr), onError, std::move(notImplementedAction), std::move(stubAction));
}

void Parser::parseConstant(const TypeRef& typeRef, const string& name)
{
	auto constant = std::make_unique<Constant>();
	constant->typeRef = typeRef;
	constant->name = name;
	constant->expr = parseExpr();
	interface->constants.push_back(std::move(constant));

	getToken(token, TOKEN(';'));
}

std::unique_ptr<Action> Parser::parseAction(DefAction::DefType dt)
{
	switch (lexer->getToken(token).type)
	{
		case Token::Type::IF:
			return parseIfThenElseAction(dt);

		case Token::Type::CALL:
			return parseCallAction();

		case Token::Type::DEFAULT_ACTION:
			return parseDefAction(dt);

		default:
			break;
	}

	syntaxError(token);
	return nullptr;
}

std::unique_ptr<Action> Parser::parseIfThenElseAction(DefAction::DefType dt)
{
	auto act = std::make_unique<IfThenElseAction>();

	act->exprIf = parseLogicalExpr();

	getToken(token, Token::Type::THEN);
	act->actThen = parseAction(dt);

	lexer->getToken(token);
	if (token.type == Token::Type::ELSE)
		act->actElse = parseAction(dt);
	else
		lexer->pushToken(token);

	getToken(token, Token::Type::ENDIF);

	return act;
}

std::unique_ptr<Action> Parser::parseCallAction()
{
	auto act = std::make_unique<CallAction>();

	act->name = getToken(token, Token::Type::IDENTIFIER).text;

	getToken(token, TOKEN('('));
	do
	{
		act->addParam(getToken(token, Token::Type::IDENTIFIER).text);
	} while (static_cast<int>(lexer->getToken(token).type) == ',');

	if (static_cast<int>(token.type) == ')')
		return act;

	syntaxError(token);
	return nullptr;
}

std::unique_ptr<Action> Parser::parseDefAction(DefAction::DefType dt)
{
	return std::make_unique<DefAction>(dt);
}

void Parser::parseMethod(const TypeRef& returnTypeRef, const string& name, std::unique_ptr<Expr> notImplementedExpr,
	const string& onError, std::unique_ptr<Action> notImplementedAction, std::unique_ptr<Action> stubAction)
{
	auto method = std::make_unique<Method>();
	Method* methodPtr = method.get();
	methodPtr->returnTypeRef = returnTypeRef;
	methodPtr->name = name;
	methodPtr->version = interface->version;
	methodPtr->notImplementedExpr = std::move(notImplementedExpr);
	methodPtr->notImplementedAction = std::move(notImplementedAction);
	methodPtr->stubAction = std::move(stubAction);
	methodPtr->onErrorFunction = onError;

	if (lexer->getToken(token).type != TOKEN(')'))
	{
		lexer->pushToken(token);

		while (true)
		{
			auto parameter = std::make_unique<Parameter>();
			Parameter* parameterPtr = parameter.get();
			parameterPtr->typeRef = parseTypeRef();
			parameterPtr->name = getToken(token, Token::Type::IDENTIFIER).text;
			methodPtr->parameters.push_back(std::move(parameter));

			lexer->getToken(token);
			lexer->pushToken(token);

			if (token.type == TOKEN(')'))
				break;

			getToken(token, TOKEN(','));
		}

		getToken(token, TOKEN(')'));
	}

	if (lexer->getToken(token).type == Token::Type::CONST)
		methodPtr->isConst = true;
	else
		lexer->pushToken(token);

	getToken(token, TOKEN(';'));

	interface->methods.push_back(std::move(method));
}

std::unique_ptr<Expr> Parser::parseExpr()
{
	return parseLogicalExpr();
}

std::unique_ptr<Expr> Parser::parseLogicalExpr()
{
	auto expr = parseUnaryExpr();

	if (lexer->getToken(token).type == TOKEN('|'))
	{
		auto rhs = parseExpr();
		expr = std::make_unique<BitwiseOrExpr>(std::move(expr), std::move(rhs));
	}
	else
		lexer->pushToken(token);

	return expr;
}

std::unique_ptr<Expr> Parser::parseUnaryExpr()
{
	lexer->getToken(token);

	if (token.type == TOKEN('-'))
		return std::make_unique<NegateExpr>(parsePrimaryExpr());
	else
	{
		lexer->pushToken(token);
		return parsePrimaryExpr();
	}
}

std::unique_ptr<Expr> Parser::parsePrimaryExpr()
{
	lexer->getToken(token);

	switch (token.type)
	{
		case Token::Type::BOOLEAN_LITERAL:
			return std::make_unique<BooleanLiteralExpr>(token.text == "true");

		case Token::Type::INT_LITERAL:
		{
			const char* p = token.text.c_str();
			size_t len = strlen(p);
			int base = len > 2 && tolower(p[1]) == 'x' ? 16 : 10;
			std::int64_t val = static_cast<std::int64_t>(strtoll(p, NULL, base));

			return std::make_unique<IntLiteralExpr>(val, base == 16);
		}

		case Token::Type::IDENTIFIER:
		{
			string text = token.text;

			if (lexer->getToken(token).type == Token::Type::DOUBLE_COLON)
			{
				getToken(token, Token::Type::IDENTIFIER);
				const auto it = typesByName.find(text);

				if (it == typesByName.end() || it->second->type != BaseType::Type::INTERFACE)
					error(token, string("Interface '") + text + "' not found.");

				return std::make_unique<ConstantExpr>(static_cast<Interface*>(it->second.get()), token.text);
			}
			else
			{
				lexer->pushToken(token);
				return std::make_unique<ConstantExpr>(interface, text);
			}
		}

		case Token::Type::DOUBLE_COLON:
			getToken(token, Token::Type::IDENTIFIER);
			return std::make_unique<ConstantExpr>(nullptr, token.text);

		default:
			syntaxError(token);
	}
}

void Parser::checkType(TypeRef& typeRef)
{
	if (typeRef.token.type == Token::Type::IDENTIFIER)
	{
		const auto it = typesByName.find(typeRef.token.text);

		if (it != typesByName.end())
			typeRef.type = it->second->type;
		else
			error(typeRef.token, string("Interface/struct '") + typeRef.token.text + "' not found.");
	}
}

Token& Parser::getToken(Token& token, Token::Type expected, bool allowEof)
{
	lexer->getToken(token);

	if (token.type != expected && !(allowEof && token.type == Token::Type::END_OF_FILE))
		syntaxError(token);

	return token;
}

TypeRef Parser::parseTypeRef()
{
	TypeRef typeRef;
	lexer->getToken(typeRef.token);

	if (typeRef.token.type == Token::Type::CONST)
	{
		typeRef.isConst = true;
		lexer->getToken(typeRef.token);
	}

	switch (typeRef.token.type)
	{
		case Token::Type::VOID:
		case Token::Type::BOOLEAN:
		case Token::Type::INT:
		case Token::Type::INT64:
		case Token::Type::INTPTR:
		case Token::Type::STRING:
		case Token::Type::UCHAR:
		case Token::Type::UINT:
		case Token::Type::UINT64:
		case Token::Type::IDENTIFIER:
			break;

		default:
			error(typeRef.token, string("Syntax error at '") + typeRef.token.text + "'. Expected a type.");
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

[[noreturn]] void Parser::syntaxError(const Token& token)
{
	error(token, string("Syntax error at '") + token.text + "'.");
}

[[noreturn]] void Parser::error(const Token& token, const string& msg)
{
	throw runtime_error(format("{}:{}:{}: error: {}", lexer->filename, token.line, token.column, msg));
}

bool TypeRef::valueIsPointer()
{
	if (isPointer)
		return true;

	switch (token.type)
	{
		case Token::Type::STRING:
			return true;

		case Token::Type::IDENTIFIER:
			if (type == BaseType::Type::INTERFACE)
				return true;
			break;

		default:
			break;
	}

	return false;
}
