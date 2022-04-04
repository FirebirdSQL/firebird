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

#ifndef CLOOP_GENERATOR_H
#define CLOOP_GENERATOR_H

#include "Parser.h"
#include <set>
#include <string>


#define DUMMY_VTABLE	1
#define DUMMY_INSTANCE	1


class Generator
{
public:
	virtual ~Generator()
	{
	}

	virtual void generate() = 0;

protected:
	static const char* const AUTOGEN_MSG;
};


class FileGenerator : public Generator
{
public:
	FileGenerator(const std::string& filename, const std::string& prefix);
	virtual ~FileGenerator();

protected:
	FILE* out;
	std::string prefix;
};


class CBasedGenerator : public FileGenerator
{
protected:
	CBasedGenerator(const std::string& filename, const std::string& prefix, bool cPlusPlus);

protected:
	std::string convertType(const TypeRef& typeRef);

protected:
	bool cPlusPlus;
};


class CppGenerator : public CBasedGenerator
{
public:
	CppGenerator(const std::string& filename, const std::string& prefix, Parser* parser,
		const std::string& headerGuard, const std::string& nameSpace);

public:
	virtual void generate();

private:
	Parser* parser;
	std::string headerGuard;
	std::string nameSpace;
};


class CHeaderGenerator : public CBasedGenerator
{
public:
	CHeaderGenerator(const std::string& filename, const std::string& prefix, Parser* parser,
		const std::string& headerGuard);

public:
	virtual void generate();

private:
	Parser* parser;
	std::string headerGuard;
};


class CImplGenerator : public CBasedGenerator
{
public:
	CImplGenerator(const std::string& filename, const std::string& prefix, Parser* parser,
		const std::string& includeFilename);

public:
	virtual void generate();

private:
	Parser* parser;
	std::string includeFilename;
};


class PascalGenerator : public FileGenerator
{
public:
	PascalGenerator(const std::string& filename, const std::string& prefix, Parser* parser,
		const std::string& unitName, const std::string& additionalUses,
		const std::string& interfaceFile, const std::string& implementationFile,
		const std::string& exceptionClass, const std::string& functionsFile);

public:
	virtual void generate();

private:
	std::string convertParameter(const Parameter& parameter);
	std::string convertType(const TypeRef& typeRef);
	std::string escapeName(std::string name, bool interfaceName = false);

	void insertFile(const std::string& filename);

private:
	Parser* parser;
	std::string unitName;
	std::string additionalUses;
	std::string interfaceFile;
	std::string implementationFile;
	std::string exceptionClass;
	std::string functionsFile;
	std::set<std::string> pointerTypes;
};


void identify(FILE* out, unsigned ident);


#endif	// CLOOP_GENERATOR_H
