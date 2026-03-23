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

#include "Expr.h"
#include "Parser.h"
#include <format>
#include <utility>

using std::format;
using std::string;


//--------------------------------------


IntLiteralExpr::IntLiteralExpr(std::int64_t value, bool hex)
	: value(value),
	  hex(hex)
{
}

string IntLiteralExpr::generate(Language language, const string& prefix)
{
	if (language == Language::JSON)  // TODO: Does json support hex constants?
		return format("{{ \"type\": \"int-literal\", \"value\": {} }}", value);

	if (hex)
		return format("{}{:x}", language == Language::PASCAL ? "$" : "0x", value);

	return format("{}", value);
}


//--------------------------------------


BooleanLiteralExpr::BooleanLiteralExpr(bool value)
	: value(value)
{
}

string BooleanLiteralExpr::generate(Language language, const string& /*prefix*/)
{
	if (language == Language::JSON)
		return format("{{ \"type\": \"boolean-literal\", \"value\": {} }}", (value ? "true" : "false"));
	else
		return value ? "true" : "false";
}


//--------------------------------------


NegateExpr::NegateExpr(std::unique_ptr<Expr> expr)
	: expr(std::move(expr))
{
}

std::string NegateExpr::generate(Language language, const string& prefix)
{
	if (language == Language::JSON)
		return "{ \"type\": \"-\", \"args\": [ " + expr->generate(language, prefix) + " ] }";
	else
		return "-" + expr->generate(language, prefix);
}


//--------------------------------------


ConstantExpr::ConstantExpr(Interface* interface, string name)
	: interface(interface),
	  name(name)
{
}

string ConstantExpr::generate(Language language, const string& prefix)
{
	string retPrefix;

	if (interface)
	{
		switch (language)
		{
			case Language::C:
				retPrefix = prefix + interface->name + "_";
				break;

			case Language::CPP:
				retPrefix = prefix + interface->name + "::";
				break;

			case Language::PASCAL:
				retPrefix = prefix + interface->name + ".";
				break;

			case Language::JAVA:
				retPrefix = prefix + interface->name + "Intf.";
				break;

			case Language::JSON:
				return "{ \"type\": \"constant\", \"interface\": \"" + interface->name + "\", \"name\": \"" + name +
					"\" }";
		}
	}

	return retPrefix + name;
}


//--------------------------------------


BitwiseOrExpr::BitwiseOrExpr(std::unique_ptr<Expr> expr1, std::unique_ptr<Expr> expr2)
	: expr1(std::move(expr1)),
	  expr2(std::move(expr2))
{
}

string BitwiseOrExpr::generate(Language language, const string& prefix)
{
	if (language == Language::JSON)
	{
		return "{ \"type\": \"|\", \"args\": [ " + expr1->generate(language, prefix) + ", " +
			expr2->generate(language, prefix) + " ] }";
	}
	else
	{
		return expr1->generate(language, prefix) + (language == Language::PASCAL ? " or " : " | ") +
			expr2->generate(language, prefix);
	}
}
