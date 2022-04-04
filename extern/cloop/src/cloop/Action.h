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

#include <string>
#include <vector>
#include <stdio.h>

class Method;
class Interface;

struct ActionParametersBlock
{
	FILE* out;
	Language language;
	const std::string& prefix;
	const std::string& exceptionClass;
	const std::string& statusName;
	Interface* interface;
	Method* method;
};

class Action
{
public:
	virtual ~Action()
	{ }

	virtual void generate(const ActionParametersBlock& apb, unsigned ident) = 0;
};


class IfThenElseAction : public Action
{
public:
	IfThenElseAction()
		: exprIf(nullptr), actThen(nullptr), actElse(nullptr)
	{ }

	IfThenElseAction(const IfThenElseAction&) = default;

	void generate(const ActionParametersBlock& apb, unsigned ident) override;

	Expr* exprIf;
	Action* actThen;
	Action* actElse;
};


class CallAction : public Action
{
public:
	CallAction() = default;

	CallAction(const CallAction&) = default;

	void generate(const ActionParametersBlock& apb, unsigned ident) override;

	void addParam(const std::string& parName)
	{
		parameters.push_back(parName);
	}

	std::string name;
	std::vector<std::string> parameters;
};


class DefAction : public Action
{
public:
	enum DefType { DEF_NOT_IMPLEMENTED };

	DefAction(DefType dt)
		: defType(dt)
	{ }

	void generate(const ActionParametersBlock& apb, unsigned ident) override;

	DefType defType;
};

#endif //CLOOP_ACTION_H
