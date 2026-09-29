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

#ifndef CLOOP_PARSER_H
#define CLOOP_PARSER_H

#include "Lexer.h"
#include "Action.h"
#include <map>
#include <memory>
#include <string>
#include <vector>


class Expr;


class BaseType
{
public:
	enum class Type
	{
		INTERFACE,
		STRUCT,
		TYPEDEF,
		BOOLEAN,
	};

protected:
	explicit BaseType(Type type)
		: type(type)
	{
	}

public:
	virtual ~BaseType() = default;

public:
	Type type;
	std::string name;
};


class TypeRef final
{
public:
	bool valueIsPointer();

public:
	Token token;
	bool isConst = false;
	bool isPointer = false;
	BaseType::Type type = BaseType::Type::INTERFACE;
};


class Parameter final
{
public:
	std::string name;
	TypeRef typeRef;
};


class Constant final
{
public:
	std::string name;
	TypeRef typeRef;
	std::unique_ptr<Expr> expr;
};


class Method final
{
public:
	std::string name;
	TypeRef returnTypeRef;
	std::vector<std::unique_ptr<Parameter>> parameters;
	std::unique_ptr<Expr> notImplementedExpr;
	std::unique_ptr<Action> notImplementedAction;
	std::unique_ptr<Action> stubAction;
	unsigned version = 0;
	bool isConst = false;
	std::string onErrorFunction;
	std::string statusName;
};


class Interface final : public BaseType
{
public:
	explicit Interface()
		: BaseType(Type::INTERFACE)
	{
	}

public:
	Interface* super = nullptr;
	std::vector<std::unique_ptr<Constant>> constants;
	std::vector<std::unique_ptr<Method>> methods;
	unsigned version = 1;
};


class Struct final : public BaseType
{
public:
	explicit Struct()
		: BaseType(Type::STRUCT)
	{
	}
};


class Boolean final : public BaseType
{
public:
	explicit Boolean()
		: BaseType(Type::BOOLEAN)
	{
	}
};


class Typedef final : public BaseType
{
public:
	explicit Typedef()
		: BaseType(Type::TYPEDEF)
	{
	}
};


class Parser final
{
public:
	explicit Parser(Lexer* lexer);

	void parse();
	void parseInterface(bool exception);
	void parseStruct();
	void parseTypedef();
	void parseBoolean();
	void parseItem();
	void parseConstant(const TypeRef& typeRef, const std::string& name);
	void parseMethod(const TypeRef& returnTypeRef, const std::string& name, std::unique_ptr<Expr> notImplementedExpr,
		const std::string& onErrorFunction, std::unique_ptr<Action> notImplementedAction,
		std::unique_ptr<Action> stubAction);

	std::unique_ptr<Expr> parseExpr();
	std::unique_ptr<Expr> parseLogicalExpr();
	std::unique_ptr<Expr> parseUnaryExpr();
	std::unique_ptr<Expr> parsePrimaryExpr();

	std::unique_ptr<Action> parseAction(DefAction::DefType dt);
	std::unique_ptr<Action> parseIfThenElseAction(DefAction::DefType dt);
	std::unique_ptr<Action> parseCallAction();
	std::unique_ptr<Action> parseDefAction(DefAction::DefType dt);

private:
	void checkType(TypeRef& typeRef);

	Token& getToken(Token& token, Token::Type expected, bool allowEof = false);

	TypeRef parseTypeRef();

	[[noreturn]] void syntaxError(const Token& token);
	[[noreturn]] void error(const Token& token, const std::string& msg);

public:
	std::vector<std::shared_ptr<Interface>> interfaces;
	std::map<std::string, std::shared_ptr<BaseType>> typesByName;
	Interface* exceptionInterface = nullptr;

private:
	Lexer* lexer;
	Token token;
	Interface* interface = nullptr;
};


#endif  // CLOOP_PARSER_H
