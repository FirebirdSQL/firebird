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

#ifndef CLOOP_EXPR_H
#define CLOOP_EXPR_H

#include <cstdint>
#include <memory>
#include <string>


class Interface;


enum class Language
{
	C,
	CPP,
	PASCAL,
	JAVA,
	JSON,
};


class Expr
{
public:
	virtual ~Expr() = default;

public:
	virtual std::string generate(Language language, const std::string& prefix) = 0;
};


class IntLiteralExpr final : public Expr
{
public:
	explicit IntLiteralExpr(std::int64_t value, bool hex);

public:
	std::string generate(Language language, const std::string& prefix) override;

private:
	std::int64_t value;
	bool hex;
};


class BooleanLiteralExpr final : public Expr
{
public:
	explicit BooleanLiteralExpr(bool value);

public:
	std::string generate(Language language, const std::string& prefix) override;

private:
	bool value;
};


class NegateExpr final : public Expr
{
public:
	explicit NegateExpr(std::unique_ptr<Expr> expr);

public:
	std::string generate(Language language, const std::string& prefix) override;

private:
	std::unique_ptr<Expr> expr;
};


class ConstantExpr final : public Expr
{
public:
	explicit ConstantExpr(Interface* interface, std::string name);

public:
	std::string generate(Language language, const std::string& prefix) override;

private:
	Interface* interface;
	std::string name;
};


class BitwiseOrExpr final : public Expr
{
public:
	explicit BitwiseOrExpr(std::unique_ptr<Expr> expr1, std::unique_ptr<Expr> expr2);

public:
	std::string generate(Language language, const std::string& prefix) override;

private:
	std::unique_ptr<Expr> expr1;
	std::unique_ptr<Expr> expr2;
};


#endif  // CLOOP_EXPR_H
