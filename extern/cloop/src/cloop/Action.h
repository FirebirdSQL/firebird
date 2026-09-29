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
 *  The Original Code was created by Alexander Peshkov.
 *
 *  Copyright (c) 2021 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLOOP_ACTION_H
#define CLOOP_ACTION_H

#include "Expr.h"

#include <memory>
#include <string>
#include <vector>
#include <stdio.h>

class Method;
class Interface;

struct ActionParametersBlock final
{
	FILE* out = nullptr;
	Language language;
	const std::string& prefix;
	const std::string& exceptionClass;
	const Interface* interface = nullptr;
	const Method* method = nullptr;
};

class Action
{
public:
	virtual ~Action() = default;

public:
	virtual void generate(const ActionParametersBlock& apb, unsigned ident) = 0;
};


class IfThenElseAction final : public Action
{
public:
	void generate(const ActionParametersBlock& apb, unsigned ident) override;

public:
	std::unique_ptr<Expr> exprIf;
	std::unique_ptr<Action> actThen;
	std::unique_ptr<Action> actElse;
};


class CallAction final : public Action
{
public:
	void generate(const ActionParametersBlock& apb, unsigned ident) override;

	void addParam(const std::string& parName)
	{
		parameters.push_back(parName);
	}

public:
	std::string name;
	std::vector<std::string> parameters;
};


class DefAction final : public Action
{
public:
	enum class DefType
	{
		NOT_IMPLEMENTED,
		IGNORE,
	};

public:
	explicit DefAction(DefType dt)
		: defType(dt)
	{
	}

public:
	void generate(const ActionParametersBlock& apb, unsigned ident) override;

public:
	DefType defType;
};

#endif  // CLOOP_ACTION_H
