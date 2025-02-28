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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2024 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef FB_ISQL_FRONTEND_PARSER_H
#define FB_ISQL_FRONTEND_PARSER_H

#include "../isql/FrontendLexer.h"
#include "../jrd/obj.h"
#include "../common/classes/MetaString.h"
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class FrontendParser
{
private:
	using Token = FrontendLexer::Token;

public:
	struct Options
	{
		bool schemaAsDatabase = false;
	};

	struct InvalidNode {};

	struct AddNode { Firebird::MetaString tableName; };
	struct BlobDumpViewNode { ISC_QUAD blobId; std::optional<std::string> file; };
	struct ConnectNode { std::vector<Token> args; };
	struct CopyNode { Firebird::MetaString source; Firebird::MetaString destination; std::string database; };
	struct CreateDatabaseNode { std::vector<Token> args; };
	struct DropDatabaseNode {};
	struct EditNode { std::optional<std::string> file; };
	struct ExitNode {};
	struct ExplainNode { std::string query; };
	struct HelpNode { std::optional<std::string> command; };
	struct InputNode { std::string file; };
	struct OutputNode { std::optional<std::string> file; };
	struct QuitNode {};
	struct ShellNode { std::optional<std::string> command; };

	struct SetNode {};
	struct SetAutoDdlNode { std::string arg; };
	struct SetAutoTermNode { std::string arg; };
	struct SetBailNode { std::string arg; };
	struct SetBlobDisplayNode { std::string arg; };
	struct SetBulkInsertNode { std::string statement; };
	struct SetCountNode { std::string arg; };
	struct SetEchoNode { std::string arg; };
	struct SetExecPathDisplayNode { std::string arg; };
	struct SetExplainNode { std::string arg; };
	struct SetHeadingNode { std::string arg; };
	struct SetKeepTranParamsNode { std::string arg; };
	struct SetListNode { std::string arg; };
	struct SetLocalTimeoutNode { std::string arg; };
	struct SetMaxRowsNode { std::string arg; };
	struct SetNamesNode { std::optional<Firebird::MetaString> name; };
	struct SetPerTableStatsNode { std::string arg; };
	struct SetPlanNode { std::string arg; };
	struct SetPlanOnlyNode { std::string arg; };
	struct SetSqldaDisplayNode { std::string arg; };
	struct SetSqlDialectNode { std::string arg; };
	struct SetStatsNode { std::string arg; };
	struct SetTermNode { std::string arg; };
	struct SetTimeNode { std::string arg; };
	struct SetTransactionNode { std::string statement; };
	struct SetWarningsNode { std::string arg; };
	struct SetWidthNode { std::string column; std::string width; };
	struct SetWireStatsNode { std::string arg; };

	struct ShowNode {};
	struct ShowChecksNode { std::optional<Firebird::MetaString> name; };
	struct ShowCollationsNode { std::optional<Firebird::MetaString> name; };
	struct ShowCommentsNode {};
	struct ShowDatabaseNode {};
	struct ShowDomainsNode { std::optional<Firebird::MetaString> name; };
	struct ShowDependenciesNode { std::optional<Firebird::MetaString> name; };
	struct ShowExceptionsNode { std::optional<Firebird::MetaString> name; };
	struct ShowFiltersNode { std::optional<Firebird::MetaString> name; };
	struct ShowFunctionsNode { std::optional<Firebird::MetaString> name; std::optional<Firebird::MetaString> package; };
	struct ShowGeneratorsNode { std::optional<Firebird::MetaString> name; };
	struct ShowGrantsNode { std::optional<Firebird::MetaString> name; };
	struct ShowIndexesNode { std::optional<Firebird::MetaString> name; };
	struct ShowMappingsNode { std::optional<Firebird::MetaString> name; };
	struct ShowPackagesNode { std::optional<Firebird::MetaString> name; };
	struct ShowProceduresNode { std::optional<Firebird::MetaString> name; std::optional<Firebird::MetaString> package; };
	struct ShowPublicationsNode { std::optional<Firebird::MetaString> name; };
	struct ShowRolesNode { std::optional<Firebird::MetaString> name; };
	struct ShowSecClassesNode { std::optional<Firebird::MetaString> name; bool detail = false; };
	struct ShowSqlDialectNode {};
	struct ShowSystemNode { std::optional<ObjectType> objType; };
	struct ShowTablesNode { std::optional<Firebird::MetaString> name; };
	struct ShowTriggersNode { std::optional<Firebird::MetaString> name; };
	struct ShowUsersNode {};
	struct ShowVersionNode {};
	struct ShowViewsNode { std::optional<Firebird::MetaString> name; };
	struct ShowWireStatsNode {};

	using AnySetNode = std::variant<
		SetNode,

		SetAutoDdlNode,
		SetAutoTermNode,
		SetBailNode,
		SetBlobDisplayNode,
		SetBulkInsertNode,
		SetCountNode,
		SetEchoNode,
		SetExecPathDisplayNode,
		SetExplainNode,
		SetHeadingNode,
		SetKeepTranParamsNode,
		SetListNode,
		SetLocalTimeoutNode,
		SetMaxRowsNode,
		SetNamesNode,
		SetPerTableStatsNode,
		SetPlanNode,
		SetPlanOnlyNode,
		SetSqldaDisplayNode,
		SetSqlDialectNode,
		SetStatsNode,
		SetTermNode,
		SetTimeNode,
		SetTransactionNode,
		SetWarningsNode,
		SetWidthNode,
		SetWireStatsNode,

		InvalidNode
	>;

	using AnyShowNode = std::variant<
		ShowNode,

		ShowChecksNode,
		ShowCollationsNode,
		ShowCommentsNode,
		ShowDatabaseNode,
		ShowDomainsNode,
		ShowDependenciesNode,
		ShowExceptionsNode,
		ShowFiltersNode,
		ShowFunctionsNode,
		ShowGeneratorsNode,
		ShowGrantsNode,
		ShowIndexesNode,
		ShowMappingsNode,
		ShowPackagesNode,
		ShowProceduresNode,
		ShowPublicationsNode,
		ShowRolesNode,
		ShowSecClassesNode,
		ShowSqlDialectNode,
		ShowSystemNode,
		ShowTablesNode,
		ShowTriggersNode,
		ShowUsersNode,
		ShowVersionNode,
		ShowViewsNode,
		ShowWireStatsNode,

		InvalidNode
	>;

	using AnyNode = std::variant<
		AddNode,
		BlobDumpViewNode,
		ConnectNode,
		CopyNode,
		CreateDatabaseNode,
		DropDatabaseNode,
		EditNode,
		ExitNode,
		ExplainNode,
		HelpNode,
		InputNode,
		OutputNode,
		QuitNode,
		ShellNode,

		AnySetNode,
		AnyShowNode,

		InvalidNode
	>;

	template <typename>
	static inline constexpr bool AlwaysFalseV = false;

private:
	FrontendParser(std::string_view statement, const Options& aOptions)
		: lexer(statement),
		  options(aOptions)
	{
	}

public:
	FrontendParser(const FrontendParser&) = delete;
	FrontendParser& operator=(const FrontendParser&) = delete;

public:
	static AnyNode parse(std::string_view statement, const Options& options)
	{
		try
		{
			FrontendParser parser(statement, options);
			return parser.internalParse();
		}
		catch (const FrontendLexer::IncompleteTokenError&)
		{
			return InvalidNode();
		}
	}

private:
	AnyNode internalParse();
	AnySetNode parseSet();

	template <typename Node>
	std::optional<AnySetNode> parseSet(std::string_view setCommand,
		std::string_view testCommand, unsigned testCommandMinLen = 0, bool useProcessedText = true);

	AnyShowNode parseShow();

	template <typename Node>
	std::optional<AnyShowNode> parseShowOptName(std::string_view showCommand,
		std::string_view testCommand, unsigned testCommandMinLen = 0);

	bool parseEof()
	{
		return lexer.getToken().type == Token::TYPE_EOF;
	}

	std::optional<Firebird::MetaString> parseName()
	{
		const auto token = lexer.getNameToken();

		if (token.type != Token::TYPE_EOF)
			return Firebird::MetaString(token.processedText.c_str());

		return std::nullopt;
	}

	std::optional<std::string> parseFileName()
	{
		const auto token = lexer.getToken();

		if (token.type == Token::TYPE_STRING || token.type == Token::TYPE_META_STRING)
			return token.processedText;
		else if (token.type != Token::TYPE_EOF)
			return token.rawText;

		return std::nullopt;
	}

	std::optional<std::string> parseUtilEof();

private:
	FrontendLexer lexer;
	const Options options;
};

#endif	// FB_ISQL_FRONTEND_PARSER_H
