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
#include <map>
#include <string>
#include <vector>


class Expr;


class BaseType
{
public:
	enum Type
	{
		TYPE_INTERFACE,
		TYPE_STRUCT,
		TYPE_TYPEDEF
	};

protected:
	BaseType(Type type)
		: type(type)
	{
	}

	virtual ~BaseType()
	{
	}

public:
	Type type;
	std::string name;
};


class TypeRef
{
public:
	TypeRef()
		: isConst(false),
		  isPointer(false),
		  type(BaseType::TYPE_INTERFACE)
	{
	}

	Token token;
	bool isConst;
	bool isPointer;
	BaseType::Type type;
};


class Parameter
{
public:
	std::string name;
	TypeRef typeRef;
};


class Constant
{
public:
	std::string name;
	TypeRef typeRef;
	Expr* expr;
};


class Method
{
public:
	Method()
		: notImplementedExpr(NULL),
		  version(0),
		  isConst(false)
	{
	}

	std::string name;
	TypeRef returnTypeRef;
	std::vector<Parameter*> parameters;
	Expr* notImplementedExpr;
	unsigned version;
	bool isConst;
	std::string onErrorFunction;
};


class Interface : public BaseType
{
public:
	Interface()
		: BaseType(TYPE_INTERFACE),
		  super(NULL),
		  version(1)
	{
	}

public:
	Interface* super;
	std::vector<Constant*> constants;
	std::vector<Method*> methods;
	unsigned version;
};


class Struct : public BaseType
{
public:
	Struct()
		: BaseType(TYPE_STRUCT)
	{
	}
};


class Typedef : public BaseType
{
public:
	Typedef()
		: BaseType(TYPE_TYPEDEF)
	{
	}
};


class Parser
{
public:
	Parser(Lexer* lexer);

	void parse();
	void parseInterface(bool exception);
	void parseStruct();
	void parseTypedef();
	void parseItem();
	void parseConstant(const TypeRef& typeRef, const std::string& name);
	void parseMethod(const TypeRef& returnTypeRef, const std::string& name, Expr* notImplementedExpr, const std::string& onErrorFunction);

	Expr* parseExpr();
	Expr* parseLogicalExpr();
	Expr* parseUnaryExpr();
	Expr* parsePrimaryExpr();

private:
	void checkType(TypeRef& typeRef);

	Token& getToken(Token& token, Token::Type expected, bool allowEof = false);

	TypeRef parseTypeRef();

	void syntaxError(const Token& token);
	void error(const Token& token, const std::string& msg);

public:
	std::vector<Interface*> interfaces;
	std::map<std::string, BaseType*> typesByName;
	Interface* exceptionInterface;

private:
	Lexer* lexer;
	Token token;
	Interface* interface;
};


#endif	// CLOOP_PARSER_H
