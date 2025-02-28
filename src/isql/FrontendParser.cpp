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

#include "firebird.h"
#include "../isql/FrontendParser.h"
#include <cctype>


FrontendParser::AnyNode FrontendParser::internalParse()
{
	static constexpr std::string_view TOKEN_ADD("ADD");
	static constexpr std::string_view TOKEN_BLOBDUMP("BLOBDUMP");
	static constexpr std::string_view TOKEN_BLOBVIEW("BLOBVIEW");
	static constexpr std::string_view TOKEN_CONNECT("CONNECT");
	static constexpr std::string_view TOKEN_COPY("COPY");
	static constexpr std::string_view TOKEN_CREATE("CREATE");
	static constexpr std::string_view TOKEN_DROP("DROP");
	static constexpr std::string_view TOKEN_EDIT("EDIT");
	static constexpr std::string_view TOKEN_EXIT("EXIT");
	static constexpr std::string_view TOKEN_EXPLAIN("EXPLAIN");
	static constexpr std::string_view TOKEN_HELP("HELP");
	static constexpr std::string_view TOKEN_INPUT("INPUT");
	static constexpr std::string_view TOKEN_OUTPUT("OUTPUT");
	static constexpr std::string_view TOKEN_QUIT("QUIT");
	static constexpr std::string_view TOKEN_SET("SET");
	static constexpr std::string_view TOKEN_SHELL("SHELL");
	static constexpr std::string_view TOKEN_SHOW("SHOW");

	const auto commandToken = lexer.getToken();

	if (commandToken.type != Token::TYPE_OTHER)
		return InvalidNode();

	const auto& command = commandToken.processedText;

	if (command == TOKEN_ADD)
	{
		if (auto tableName = parseName())
		{
			AddNode node;
			node.tableName = std::move(tableName.value());

			if (parseEof())
				return node;
		}
	}
	else if (command == TOKEN_BLOBDUMP || command == TOKEN_BLOBVIEW)
	{
		if (const auto blobId = lexer.getToken(); blobId.type != Token::TYPE_EOF)
		{
			BlobDumpViewNode node;

			// Find the high and low values of the blob id
			if (blobId.processedText.empty())
				return InvalidNode();

			sscanf(blobId.processedText.c_str(), "%" xLONGFORMAT":%" xLONGFORMAT,
				&node.blobId.gds_quad_high, &node.blobId.gds_quad_low);

			if (command == TOKEN_BLOBDUMP)
			{
				if (auto file = parseFileName())
				{
					node.file = std::move(file.value());

					if (parseEof())
						return node;
				}
			}
			else
			{
				if (parseEof())
					return node;
			}
		}
	}
	else if (command == TOKEN_CONNECT)
	{
		ConnectNode node;

		do
		{
			auto token = lexer.getToken();

			if (token.type == Token::TYPE_EOF)
			{
				if (node.args.empty())
					break;
				else
					return node;
			}
			else if (token.type != Token::TYPE_OTHER &&
				token.type != Token::TYPE_STRING &&
				token.type != Token::TYPE_META_STRING)
			{
				return InvalidNode();
			}

			node.args.push_back(std::move(token));
		} while(true);
	}
	else if (command == TOKEN_COPY)
	{
		CopyNode node;

		if (auto source = parseName())
			node.source = std::move(source.value());
		else
			return InvalidNode();

		if (auto destination = parseName())
			node.destination = std::move(destination.value());
		else
			return InvalidNode();

		if (auto database = parseFileName())
			node.database = std::move(database.value());
		else
			return InvalidNode();

		if (parseEof())
			return node;
	}
	else if (command == TOKEN_CREATE)
	{
		if (const auto createWhat = lexer.getToken();
			createWhat.type == Token::TYPE_OTHER &&
			(createWhat.processedText == "DATABASE" ||
				(options.schemaAsDatabase && createWhat.processedText == "SCHEMA")))
		{
			CreateDatabaseNode node;

			do
			{
				auto token = lexer.getToken();

				if (token.type == Token::TYPE_EOF)
				{
					if (node.args.empty())
						break;
					else
						return node;
				}
				else if (token.type != Token::TYPE_OTHER &&
					token.type != Token::TYPE_STRING &&
					token.type != Token::TYPE_META_STRING)
				{
					return InvalidNode();
				}

				node.args.push_back(std::move(token));
			} while(true);
		}
	}
	else if (command == TOKEN_DROP)
	{
		if (const auto dropWhat = lexer.getToken();
			dropWhat.type == Token::TYPE_OTHER &&
			(dropWhat.processedText == "DATABASE" ||
				(options.schemaAsDatabase && dropWhat.processedText == "SCHEMA")))
		{
			if (parseEof())
				return DropDatabaseNode();
		}
	}
	else if (command == TOKEN_EDIT)
	{
		EditNode node;
		node.file = parseFileName();

		if (parseEof())
			return node;
	}
	else if (command == TOKEN_EXIT)
	{
		if (parseEof())
			return ExitNode();
	}
	else if (command == TOKEN_EXPLAIN)
	{
		ExplainNode node;

		if (auto query = parseUtilEof())
		{
			node.query = std::move(query.value());
			return node;
		}
	}
	else if (command == TOKEN_HELP || command == "?")
	{
		HelpNode node;

		if (const auto token = lexer.getToken(); token.type == Token::TYPE_EOF)
			return node;
		else if (token.type == Token::TYPE_OTHER)
		{
			node.command = token.processedText;

			if (parseEof())
				return node;
		}
	}
	else if (command.length() >= 2 && TOKEN_INPUT.find(command) == 0)
	{
		if (auto file = parseFileName())
		{
			InputNode node;
			node.file = std::move(file.value());

			if (parseEof())
				return node;
		}
	}
	else if (command.length() >= 3 && TOKEN_OUTPUT.find(command) == 0)
	{
		OutputNode node;
		node.file = parseFileName();

		if (parseEof())
			return node;
	}
	else if (command == TOKEN_QUIT)
	{
		if (parseEof())
			return QuitNode();
	}
	else if (command == TOKEN_SET)
	{
		if (const auto setNode = parseSet(); !std::holds_alternative<InvalidNode>(setNode))
			return setNode;
	}
	else if (command == TOKEN_SHELL)
	{
		ShellNode node;
		node.command = parseUtilEof();
		return node;
	}
	else if (command == TOKEN_SHOW)
	{
		if (const auto showNode = parseShow(); !std::holds_alternative<InvalidNode>(showNode))
			return showNode;
	}

	return InvalidNode();
}

FrontendParser::AnySetNode FrontendParser::parseSet()
{
	static constexpr std::string_view TOKEN_AUTODDL("AUTODDL");
	static constexpr std::string_view TOKEN_AUTOTERM("AUTOTERM");
	static constexpr std::string_view TOKEN_BAIL("BAIL");
	static constexpr std::string_view TOKEN_BLOBDISPLAY("BLOBDISPLAY");
	static constexpr std::string_view TOKEN_BULK_INSERT("BULK_INSERT");
	static constexpr std::string_view TOKEN_COUNT("COUNT");
	static constexpr std::string_view TOKEN_ECHO("ECHO");
	static constexpr std::string_view TOKEN_EXEC_PATH_DISPLAY("EXEC_PATH_DISPLAY");
	static constexpr std::string_view TOKEN_EXPLAIN("EXPLAIN");
	static constexpr std::string_view TOKEN_HEADING("HEADING");
	static constexpr std::string_view TOKEN_KEEP_TRAN_PARAMS("KEEP_TRAN_PARAMS");
	static constexpr std::string_view TOKEN_LIST("LIST");
	static constexpr std::string_view TOKEN_LOCAL_TIMEOUT("LOCAL_TIMEOUT");
	static constexpr std::string_view TOKEN_MAXROWS("MAXROWS");
	static constexpr std::string_view TOKEN_NAMES("NAMES");
	static constexpr std::string_view TOKEN_PER_TABLE_STATS("PER_TABLE_STATS");
	static constexpr std::string_view TOKEN_PLAN("PLAN");
	static constexpr std::string_view TOKEN_PLANONLY("PLANONLY");
	static constexpr std::string_view TOKEN_ROWCOUNT("ROWCOUNT");
	static constexpr std::string_view TOKEN_SQL("SQL");
	static constexpr std::string_view TOKEN_SQLDA_DISPLAY("SQLDA_DISPLAY");
	static constexpr std::string_view TOKEN_STATS("STATS");
	static constexpr std::string_view TOKEN_TERMINATOR("TERMINATOR");
	static constexpr std::string_view TOKEN_TIME("TIME");
	static constexpr std::string_view TOKEN_TRANSACTION("TRANSACTION");
	static constexpr std::string_view TOKEN_WARNINGS("WARNINGS");
	static constexpr std::string_view TOKEN_WIDTH("WIDTH");
	static constexpr std::string_view TOKEN_WNG("WNG");
	static constexpr std::string_view TOKEN_WIRE_STATS("WIRE_STATS");

	switch (const auto setCommandToken = lexer.getToken(); setCommandToken.type)
	{
		case Token::TYPE_EOF:
			return SetNode();

		case Token::TYPE_OTHER:
		{
			const auto& text = setCommandToken.processedText;

			if (const auto parsed = parseSet<SetAutoDdlNode>(text, TOKEN_AUTODDL, 4))
				return parsed.value();
			else if (const auto parsed = parseSet<SetAutoTermNode>(text, TOKEN_AUTOTERM))
				return parsed.value();
			else if (const auto parsed = parseSet<SetBailNode>(text, TOKEN_BAIL))
				return parsed.value();
			else if (const auto parsed = parseSet<SetBlobDisplayNode>(text, TOKEN_BLOBDISPLAY, 4))
				return parsed.value();
			else if (text == TOKEN_BULK_INSERT)
			{
				SetBulkInsertNode node;

				if (const auto statement = parseUtilEof())
				{
					node.statement = statement.value();
					return node;
				}
			}
			else if (const auto parsed = parseSet<SetCountNode>(text, TOKEN_COUNT))
				return parsed.value();
			else if (const auto parsed = parseSet<SetEchoNode>(text, TOKEN_ECHO))
				return parsed.value();
			else if (const auto parsed = parseSet<SetExecPathDisplayNode>(text, TOKEN_EXEC_PATH_DISPLAY))
				return parsed.value();
			else if (const auto parsed = parseSet<SetExplainNode>(text, TOKEN_EXPLAIN))
				return parsed.value();
			else if (const auto parsed = parseSet<SetHeadingNode>(text, TOKEN_HEADING))
				return parsed.value();
			else if (const auto parsed = parseSet<SetKeepTranParamsNode>(text, TOKEN_KEEP_TRAN_PARAMS, 9))
				return parsed.value();
			else if (const auto parsed = parseSet<SetListNode>(text, TOKEN_LIST))
				return parsed.value();
			else if (const auto parsed = parseSet<SetLocalTimeoutNode>(text, TOKEN_LOCAL_TIMEOUT))
				return parsed.value();
			else if (const auto parsed = parseSet<SetMaxRowsNode>(text, TOKEN_MAXROWS))
				return parsed.value();
			else if (text == TOKEN_NAMES)
			{
				SetNamesNode node;
				node.name = parseName();

				if (parseEof())
					return node;
			}
			else if (const auto parsed = parseSet<SetPerTableStatsNode>(text, TOKEN_PER_TABLE_STATS, 7))
				return parsed.value();
			else if (const auto parsed = parseSet<SetPlanNode>(text, TOKEN_PLAN))
				return parsed.value();
			else if (const auto parsed = parseSet<SetPlanOnlyNode>(text, TOKEN_PLANONLY))
				return parsed.value();
			else if (const auto parsed = parseSet<SetMaxRowsNode>(text, TOKEN_ROWCOUNT))
				return parsed.value();
			else if (text == TOKEN_SQL)
			{
				SetSqlDialectNode node;

				if (const auto dialectToken = lexer.getToken();
					dialectToken.type == Token::TYPE_OTHER && dialectToken.processedText == "DIALECT")
				{
					if (const auto arg = lexer.getToken(); arg.type != Token::TYPE_EOF)
					{
						node.arg = arg.processedText;

						if (parseEof())
							return node;
					}
				}
			}
			else if (const auto parsed = parseSet<SetSqldaDisplayNode>(text, TOKEN_SQLDA_DISPLAY))
				return parsed.value();
			else if (const auto parsed = parseSet<SetStatsNode>(text, TOKEN_STATS, 4))
				return parsed.value();
			else if (const auto parsed = parseSet<SetTermNode>(text, TOKEN_TERMINATOR, 4, false))
				return parsed.value();
			else if (const auto parsed = parseSet<SetTimeNode>(text, TOKEN_TIME))
			{
				if (const auto setTimeNode = std::get_if<SetTimeNode>(&parsed.value());
					setTimeNode && setTimeNode->arg == "ZONE")
				{
					return InvalidNode();
				}

				return parsed.value();
			}
			else if (text.length() >= 5 && TOKEN_TRANSACTION.find(text) == 0)
			{
				SetTransactionNode node;
				node.statement = lexer.getBuffer();
				return node;
			}
			else if (const auto parsed = parseSet<SetWarningsNode>(text, TOKEN_WARNINGS, 7))
				return parsed.value();
			else if (const auto parsed = parseSet<SetWarningsNode>(text, TOKEN_WNG))
				return parsed.value();
			else if (text == TOKEN_WIDTH)
			{
				SetWidthNode node;

				if (const auto column = lexer.getToken(); column.type != Token::TYPE_EOF)
				{
					node.column = column.processedText;

					if (const auto width = lexer.getToken(); width.type != Token::TYPE_EOF)
					{
						node.width = width.processedText;

						if (!parseEof())
							return InvalidNode();
					}

					return node;
				}
			}
			else if (const auto parsed = parseSet<SetWireStatsNode>(text, TOKEN_WIRE_STATS, 4))
				return parsed.value();

			break;
		}
	}

	return InvalidNode();
}

template <typename Node>
std::optional<FrontendParser::AnySetNode> FrontendParser::parseSet(std::string_view setCommand,
	std::string_view testCommand, unsigned testCommandMinLen, bool useProcessedText)
{
	if (setCommand == testCommand ||
		(testCommandMinLen && setCommand.length() >= testCommandMinLen &&
			testCommand.find(setCommand) == 0))
	{
		Node node;

		if (const auto arg = lexer.getToken(); arg.type != Token::TYPE_EOF)
		{
			node.arg = useProcessedText ? arg.processedText : arg.rawText;

			if (!parseEof())
				return InvalidNode();
		}

		return node;
	}

	return std::nullopt;
}

FrontendParser::AnyShowNode FrontendParser::parseShow()
{
	static constexpr std::string_view TOKEN_CHECKS("CHECKS");
	static constexpr std::string_view TOKEN_COLLATES("COLLATES");
	static constexpr std::string_view TOKEN_COLLATIONS("COLLATIONS");
	static constexpr std::string_view TOKEN_COMMENTS("COMMENTS");
	static constexpr std::string_view TOKEN_DATABASE("DATABASE");
	static constexpr std::string_view TOKEN_DEPENDENCIES("DEPENDENCIES");
	static constexpr std::string_view TOKEN_DEPENDENCY("DEPENDENCY");
	static constexpr std::string_view TOKEN_DOMAINS("DOMAINS");
	static constexpr std::string_view TOKEN_EXCEPTIONS("EXCEPTIONS");
	static constexpr std::string_view TOKEN_FILTERS("FILTERS");
	static constexpr std::string_view TOKEN_FUNCTIONS("FUNCTIONS");
	static constexpr std::string_view TOKEN_INDEXES("INDEXES");
	static constexpr std::string_view TOKEN_INDICES("INDICES");
	static constexpr std::string_view TOKEN_GENERATORS("GENERATORS");
	static constexpr std::string_view TOKEN_GRANTS("GRANTS");
	static constexpr std::string_view TOKEN_MAPPINGS("MAPPINGS");
	static constexpr std::string_view TOKEN_PACKAGES("PACKAGES");
	static constexpr std::string_view TOKEN_PROCEDURES("PROCEDURES");
	static constexpr std::string_view TOKEN_PUBLICATIONS("PUBLICATIONS");
	static constexpr std::string_view TOKEN_ROLES("ROLES");
	static constexpr std::string_view TOKEN_SECCLASSES("SECCLASSES");
	static constexpr std::string_view TOKEN_SEQUENCES("SEQUENCES");
	static constexpr std::string_view TOKEN_SQL("SQL");
	static constexpr std::string_view TOKEN_SYSTEM("SYSTEM");
	static constexpr std::string_view TOKEN_TABLES("TABLES");
	static constexpr std::string_view TOKEN_TRIGGERS("TRIGGERS");
	static constexpr std::string_view TOKEN_USERS("USERS");
	static constexpr std::string_view TOKEN_VER("VER");
	static constexpr std::string_view TOKEN_VERSION("VERSION");
	static constexpr std::string_view TOKEN_VIEWS("VIEWS");
	static constexpr std::string_view TOKEN_WIRE_STATISTICS("WIRE_STATISTICS");
	static constexpr std::string_view TOKEN_WIRE_STATS("WIRE_STATS");

	switch (const auto showCommandToken = lexer.getToken(); showCommandToken.type)
	{
		case Token::TYPE_EOF:
			return ShowNode();

		case Token::TYPE_OTHER:
		{
			const auto& text = showCommandToken.processedText;

			if (const auto parsed = parseShowOptName<ShowChecksNode>(text, TOKEN_CHECKS, 5))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowCollationsNode>(text, TOKEN_COLLATES, 7))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowCollationsNode>(text, TOKEN_COLLATIONS, 9))
				return parsed.value();
			else if (text.length() >= 7 && TOKEN_COMMENTS.find(text) == 0)
			{
				if (parseEof())
					return ShowCommentsNode();
			}
			else if (text == TOKEN_DATABASE)
			{
				if (parseEof())
					return ShowDatabaseNode();
			}
			else if (const auto parsed = parseShowOptName<ShowDependenciesNode>(text, TOKEN_DEPENDENCIES, 5))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowDependenciesNode>(text, TOKEN_DEPENDENCY, 5))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowDomainsNode>(text, TOKEN_DOMAINS, 6))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowExceptionsNode>(text, TOKEN_EXCEPTIONS, 5))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowFiltersNode>(text, TOKEN_FILTERS, 6))
				return parsed.value();
			else if (text.length() >= 4 && TOKEN_FUNCTIONS.find(text) == 0)
			{
				ShowFunctionsNode node;
				node.name = parseName();

				if (node.name)
				{
					if (const auto token = lexer.getToken();
						token.type == Token::TYPE_OTHER && token.rawText == ".")
					{
						node.package = node.name;
						node.name = parseName();

						if (parseEof())
							return node;
					}
					else if (token.type == Token::TYPE_EOF)
					{
						return node;
					}
				}
				else
					return node;
			}
			else if (const auto parsed = parseShowOptName<ShowIndexesNode>(text, TOKEN_INDEXES, 3))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowIndexesNode>(text, TOKEN_INDICES, 0))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowGeneratorsNode>(text, TOKEN_GENERATORS, 3))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowGrantsNode>(text, TOKEN_GRANTS, 5))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowMappingsNode>(text, TOKEN_MAPPINGS, 3))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowPackagesNode>(text, TOKEN_PACKAGES, 4))
				return parsed.value();
			else if (text.length() >= 4 && TOKEN_PROCEDURES.find(text) == 0)
			{
				ShowProceduresNode node;
				node.name = parseName();

				if (node.name)
				{
					if (const auto token = lexer.getToken();
						token.type == Token::TYPE_OTHER && token.rawText == ".")
					{
						node.package = node.name;
						node.name = parseName();

						if (parseEof())
							return node;
					}
					else if (token.type == Token::TYPE_EOF)
					{
						return node;
					}
				}
				else
					return node;
			}
			else if (const auto parsed = parseShowOptName<ShowPublicationsNode>(text, TOKEN_PUBLICATIONS, 3))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowRolesNode>(text, TOKEN_ROLES, 4))
				return parsed.value();
			else if (text.length() >= 6 && TOKEN_SECCLASSES.find(text) == 0)
			{
				const auto lexerPos = lexer.getPos();
				const auto token = lexer.getNameToken();

				ShowSecClassesNode node;

				if (!(token.type == Token::TYPE_OTHER && token.rawText == "*"))
				{
					lexer.setPos(lexerPos);
					node.name = parseName();

					if (!node.name)
						return InvalidNode();
				}

				const auto optDetail = parseName();
				node.detail = optDetail == "DET" || optDetail == "DETAIL";

				if (!node.detail && optDetail)
					return InvalidNode();

				if (parseEof())
					return node;
			}
			else if (const auto parsed = parseShowOptName<ShowGeneratorsNode>(text, TOKEN_SEQUENCES, 3))
				return parsed.value();
			else if (text == TOKEN_SQL)
			{
				if (const auto dialectToken = lexer.getToken();
					dialectToken.type == Token::TYPE_OTHER && dialectToken.processedText == "DIALECT")
				{
					if (parseEof())
						return ShowSqlDialectNode();
				}
			}
			else if (text.length() >= 3 && TOKEN_SYSTEM.find(text) == 0)
			{
				ShowSystemNode node;

				if (const auto objectType = parseName())
				{
					const auto objectTypeText = std::string(objectType->c_str());

					if ((objectTypeText.length() >= 7 && TOKEN_COLLATES.find(objectTypeText) == 0) ||
						(objectTypeText.length() >= 9 && TOKEN_COLLATIONS.find(objectTypeText) == 0))
					{
						node.objType = obj_collation;
					}
					else if (objectTypeText.length() >= 4 && TOKEN_FUNCTIONS.find(objectTypeText) == 0)
						node.objType = obj_udf;
					else if (objectTypeText.length() >= 5 && TOKEN_TABLES.find(objectTypeText) == 0)
						node.objType = obj_relation;
					else if (objectTypeText.length() >= 4 && TOKEN_ROLES.find(objectTypeText) == 0)
						node.objType = obj_sql_role;
					else if (objectTypeText.length() >= 4 && TOKEN_PROCEDURES.find(objectTypeText) == 0)
						node.objType = obj_procedure;
					else if (objectTypeText.length() >= 4 && TOKEN_PACKAGES.find(objectTypeText) == 0)
						node.objType = obj_package_header;
					else if (objectTypeText.length() >= 3 && TOKEN_PUBLICATIONS.find(objectTypeText) == 0)
						node.objType = obj_publication;
					else
						return InvalidNode();

					if (!parseEof())
						return InvalidNode();
				}

				return node;
			}
			else if (const auto parsed = parseShowOptName<ShowTablesNode>(text, TOKEN_TABLES, 5))
				return parsed.value();
			else if (const auto parsed = parseShowOptName<ShowTriggersNode>(text, TOKEN_TRIGGERS, 4))
				return parsed.value();
			else if (text == TOKEN_USERS)
			{
				if (parseEof())
					return ShowUsersNode();
			}
			else if (text == TOKEN_VER || text == TOKEN_VERSION)
			{
				if (parseEof())
					return ShowVersionNode();
			}
			else if (const auto parsed = parseShowOptName<ShowViewsNode>(text, TOKEN_VIEWS, 4))
				return parsed.value();
			else if (text.length() >= 9 && TOKEN_WIRE_STATISTICS.find(text) == 0 ||
				text == TOKEN_WIRE_STATS)
			{
				if (parseEof())
					return ShowWireStatsNode();
			}

			break;
		}
	}

	return InvalidNode();
}

template <typename Node>
std::optional<FrontendParser::AnyShowNode> FrontendParser::parseShowOptName(std::string_view showCommand,
	std::string_view testCommand, unsigned testCommandMinLen)
{
	if (showCommand == testCommand ||
		(testCommandMinLen && showCommand.length() >= testCommandMinLen &&
			testCommand.find(showCommand) == 0))
	{
		Node node;
		node.name = parseName();

		if (!parseEof())
			return InvalidNode();

		return node;
	}

	return std::nullopt;
}

std::optional<std::string> FrontendParser::parseUtilEof()
{
	const auto startIt = lexer.getPos();
	auto lastPosIt = startIt;
	bool first = true;

	do
	{
		const auto token = lexer.getToken();

		if (token.type == Token::TYPE_EOF)
		{
			if (first)
				return std::nullopt;

			return FrontendLexer::trim(std::string(startIt, lastPosIt));
		}

		lastPosIt = lexer.getPos();
		first = false;
	} while (true);

	return std::nullopt;
}
