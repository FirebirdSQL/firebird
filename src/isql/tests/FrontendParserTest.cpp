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
#include "boost/test/unit_test.hpp"
#include "../FrontendParser.h"
#include <variant>

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(ISqlSuite)
BOOST_AUTO_TEST_SUITE(FrontendParserSuite)
BOOST_AUTO_TEST_SUITE(FrontendParserTests)


BOOST_AUTO_TEST_CASE(ParseCommandTest)
{
	const FrontendParser::Options parserOptions;

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"add", parserOptions)));
	BOOST_TEST((std::get<FrontendParser::AddNode>(FrontendParser::parse(
		"add table1", parserOptions)).tableName == QualifiedMetaString("TABLE1")));
	BOOST_TEST((std::get<FrontendParser::AddNode>(FrontendParser::parse(
		"add \"table2\"", parserOptions)).tableName == QualifiedMetaString("table2")));

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"blobdump", parserOptions)));

	{
		const auto blobDump1 = std::get<FrontendParser::BlobDumpViewNode>(FrontendParser::parse(
			"blobdump 1:2 /tmp/blob.txt", parserOptions));
		BOOST_TEST(blobDump1.blobId.gds_quad_high == 1);
		BOOST_TEST(blobDump1.blobId.gds_quad_low == 2u);
		BOOST_TEST(blobDump1.file.value() == "/tmp/blob.txt");

		const auto blobDump2 = std::get<FrontendParser::BlobDumpViewNode>(FrontendParser::parse(
			"blobdump 1:2 'C:\\A dir\\blob.txt'", parserOptions));
		BOOST_TEST((blobDump2.file.value() == "C:\\A dir\\blob.txt"));
	}

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"blobview", parserOptions)));

	{
		const auto blobView1 = std::get<FrontendParser::BlobDumpViewNode>(FrontendParser::parse(
			"blobview 1:2", parserOptions));
		BOOST_TEST(blobView1.blobId.gds_quad_high == 1);
		BOOST_TEST(blobView1.blobId.gds_quad_low == 2u);
		BOOST_TEST(!blobView1.file);
	}

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"connect", parserOptions)));

	{
		const auto connect1 = std::get<FrontendParser::ConnectNode>(FrontendParser::parse(
			"connect 'test.fdb'", parserOptions));
		BOOST_TEST(connect1.args[0].getProcessedString() == "test.fdb");

		const auto connect2 = std::get<FrontendParser::ConnectNode>(FrontendParser::parse(
			"connect 'test.fdb' user user", parserOptions));
		BOOST_TEST(connect2.args.size() == 3u);
	}

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"copy", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"copy source destination", parserOptions)));

	{
		const auto copy1 = std::get<FrontendParser::CopyNode>(FrontendParser::parse(
			"copy source \"destination\" localhost:/tmp/database.fdb", parserOptions));
		BOOST_TEST((copy1.source == QualifiedMetaString("SOURCE")));
		BOOST_TEST((copy1.destination == QualifiedMetaString("destination")));
		BOOST_TEST(copy1.database == "localhost:/tmp/database.fdb");
	}

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"create", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"create database", parserOptions)));

	{
		const auto createDatabase1 = std::get<FrontendParser::CreateDatabaseNode>(FrontendParser::parse(
			"create database 'test.fdb'", parserOptions));
		BOOST_TEST(createDatabase1.args[0].getProcessedString() == "test.fdb");

		const auto createDatabase2 = std::get<FrontendParser::CreateDatabaseNode>(FrontendParser::parse(
			"create database 'test.fdb' user user", parserOptions));
		BOOST_TEST(createDatabase2.args.size() == 3u);
	}

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"drop database x", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::DropDatabaseNode>(FrontendParser::parse(
		"drop database", parserOptions)));

	BOOST_TEST(!std::get<FrontendParser::EditNode>(FrontendParser::parse(
		"edit", parserOptions)).file);
	BOOST_TEST(std::get<FrontendParser::EditNode>(FrontendParser::parse(
		"edit /tmp/file.sql", parserOptions)).file.value() == "/tmp/file.sql");

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"exit x", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::ExitNode>(FrontendParser::parse(
		"exit", parserOptions)));

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"explain", parserOptions)));
	BOOST_TEST(std::get<FrontendParser::ExplainNode>(FrontendParser::parse(
		"explain select 1 from rdb$database", parserOptions)).query == "select 1 from rdb$database");

	BOOST_TEST(!std::get<FrontendParser::HelpNode>(FrontendParser::parse(
		"help", parserOptions)).command.has_value());
	BOOST_TEST(std::get<FrontendParser::HelpNode>(FrontendParser::parse(
		"help set", parserOptions)).command.value() == "SET");
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"help set x", parserOptions)));

	BOOST_TEST(!std::get<FrontendParser::HelpNode>(FrontendParser::parse(
		"?", parserOptions)).command.has_value());
	BOOST_TEST(std::get<FrontendParser::HelpNode>(FrontendParser::parse(
		"? set", parserOptions)).command.value() == "SET");
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"? set x", parserOptions)));

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"input", parserOptions)));
	BOOST_TEST(std::get<FrontendParser::InputNode>(FrontendParser::parse(
		"input /tmp/file.sql", parserOptions)).file == "/tmp/file.sql");

	BOOST_TEST(!std::get<FrontendParser::OutputNode>(FrontendParser::parse(
		"output", parserOptions)).file);
	BOOST_TEST(std::get<FrontendParser::OutputNode>(FrontendParser::parse(
		"output /tmp/file.txt", parserOptions)).file.value() == "/tmp/file.txt");

	BOOST_TEST(std::holds_alternative<FrontendParser::QuitNode>(FrontendParser::parse(
		"quit", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"quit x", parserOptions)));

	BOOST_TEST(!std::get<FrontendParser::ShellNode>(FrontendParser::parse(
		"shell", parserOptions)).command);
	BOOST_TEST(std::get<FrontendParser::ShellNode>(FrontendParser::parse(
		"shell ls -l /tmp", parserOptions)).command.value() == "ls -l /tmp");
}

BOOST_AUTO_TEST_CASE(ParseSetTest)
{
	const FrontendParser::Options parserOptions;

	const auto parseSet = [&](const std::string_view text) {
		return std::get<FrontendParser::AnySetNode>(FrontendParser::parse(text, parserOptions));
	};

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"\"set\"", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set x", parserOptions)));

	BOOST_TEST(std::holds_alternative<FrontendParser::SetNode>(parseSet("set")));

	BOOST_TEST(std::get<FrontendParser::SetAutoDdlNode>(parseSet(
		"set auto")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetAutoDdlNode>(parseSet(
		"set auto on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set auto off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetAutoDdlNode>(parseSet(
		"set autoddl")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetAutoDdlNode>(parseSet(
		"set autoddl on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set autoddl off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetAutoTermNode>(parseSet(
		"set autoterm")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetAutoTermNode>(parseSet(
		"set autoterm on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set autoterm off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetBailNode>(parseSet(
		"set bail")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetBailNode>(parseSet(
		"set bail on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set bail off x", parserOptions)));

	BOOST_TEST((std::get<FrontendParser::SetBulkInsertNode>(parseSet(
		"set bulk_insert insert into mytable (a, b) values (1, ?)")).statement ==
			"insert into mytable (a, b) values (1, ?)"));

	BOOST_TEST(std::get<FrontendParser::SetBlobDisplayNode>(parseSet(
		"set blob")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetBlobDisplayNode>(parseSet(
		"set blob on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set blob off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetBlobDisplayNode>(parseSet(
		"set blobdisplay")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetBlobDisplayNode>(parseSet(
		"set blobdisplay on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set blobdisplay off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetCountNode>(parseSet(
		"set count")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetCountNode>(parseSet(
		"set count on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set count off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetEchoNode>(parseSet(
		"set echo")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetEchoNode>(parseSet(
		"set echo on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set echo off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetExecPathDisplayNode>(parseSet(
		"set exec_path_display")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetExecPathDisplayNode>(parseSet(
		"set exec_path_display blr")).arg == "BLR"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set exec_path_display off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetExplainNode>(parseSet(
		"set explain")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetExplainNode>(parseSet(
		"set explain on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set explain off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetHeadingNode>(parseSet(
		"set heading")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetHeadingNode>(parseSet(
		"set heading on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set heading off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetKeepTranParamsNode>(parseSet(
		"set keep_tran")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetKeepTranParamsNode>(parseSet(
		"set keep_tran on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set keep_tran off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetKeepTranParamsNode>(parseSet(
		"set keep_tran_params")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetKeepTranParamsNode>(parseSet(
		"set keep_tran_params on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set keep_tran_params off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetListNode>(parseSet(
		"set list")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetListNode>(parseSet(
		"set list on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set list off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetLocalTimeoutNode>(parseSet(
		"set local_timeout")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetLocalTimeoutNode>(parseSet(
		"set local_timeout 80")).arg == "80"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set local_timeout 90 x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetMaxRowsNode>(parseSet(
		"set maxrows")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetMaxRowsNode>(parseSet(
		"set maxrows 80")).arg == "80"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set maxrows 90 x", parserOptions)));

	BOOST_TEST(!std::get<FrontendParser::SetNamesNode>(parseSet(
		"set names")).name.has_value());
	BOOST_TEST((std::get<FrontendParser::SetNamesNode>(parseSet(
		"set names utf8")).name == QualifiedMetaString("UTF8")));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set names utf8 x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetPerTableStatsNode>(parseSet(
		"set per_tab")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetPerTableStatsNode>(parseSet(
		"set per_tab on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set per_tab off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetPerTableStatsNode>(parseSet(
		"set per_table_stats")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetPerTableStatsNode>(parseSet(
		"set per_table_stats on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set per_table_stats off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetPlanNode>(parseSet(
		"set plan")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetPlanNode>(parseSet(
		"set plan on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set plan off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetPlanOnlyNode>(parseSet(
		"set planonly")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetPlanOnlyNode>(parseSet(
		"set planonly on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set planonly off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetMaxRowsNode>(parseSet(
		"set rowcount")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetMaxRowsNode>(parseSet(
		"set rowcount 80")).arg == "80"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set rowcount 90 x", parserOptions)));

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set sql", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set sql dialect", parserOptions)));
	BOOST_TEST((std::get<FrontendParser::SetSqlDialectNode>(parseSet(
		"set sql dialect 3")).arg == "3"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set sql dialect 3 x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetSqldaDisplayNode>(parseSet(
		"set sqlda_display")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetSqldaDisplayNode>(parseSet(
		"set sqlda_display on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set sqlda_display off x", parserOptions)));

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set sta", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set sta on", parserOptions)));
	BOOST_TEST(std::get<FrontendParser::SetStatsNode>(parseSet(
		"set stat")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetStatsNode>(parseSet(
		"set stat on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set stat off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetStatsNode>(parseSet(
		"set stats")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetStatsNode>(parseSet(
		"set stats on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set stats off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetTermNode>(parseSet(
		"set term")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetTermNode>(parseSet(
		"set term !")).arg == "!"));
	BOOST_TEST((std::get<FrontendParser::SetTermNode>(parseSet(
		"set term Go")).arg == "Go"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set term a b", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetTermNode>(parseSet(
		"set terminator")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetTermNode>(parseSet(
		"set terminator !")).arg == "!"));
	BOOST_TEST((std::get<FrontendParser::SetTermNode>(parseSet(
		"set terminator Go")).arg == "Go"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set terminator a b", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetTimeNode>(parseSet(
		"set time")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetTimeNode>(parseSet(
		"set time on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set time off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetTransactionNode>(parseSet(
		"set transaction")).statement == "set transaction");
	BOOST_TEST(std::get<FrontendParser::SetTransactionNode>(parseSet(
		"set transaction read committed")).statement == "set transaction read committed");

	BOOST_TEST(std::get<FrontendParser::SetWarningsNode>(parseSet(
		"set warning")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetWarningsNode>(parseSet(
		"set warning on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set warning off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetWarningsNode>(parseSet(
		"set warnings")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetWarningsNode>(parseSet(
		"set warnings on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set warnings off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetWarningsNode>(parseSet(
		"set wng")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetWarningsNode>(parseSet(
		"set wng on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set wng off x", parserOptions)));

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set width", parserOptions)));
	BOOST_TEST((std::get<FrontendParser::SetWidthNode>(parseSet(
		"set width x")).column == "X"));
	BOOST_TEST(std::get<FrontendParser::SetWidthNode>(parseSet(
		"set width x")).width.empty());
	BOOST_TEST((std::get<FrontendParser::SetWidthNode>(parseSet(
		"set width x 80")).column == "X"));
	BOOST_TEST((std::get<FrontendParser::SetWidthNode>(parseSet(
		"set width x 90")).width == "90"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set width x 90 y", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetWireStatsNode>(parseSet(
		"set wire")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetWireStatsNode>(parseSet(
		"set wire on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set wire off x", parserOptions)));

	BOOST_TEST(std::get<FrontendParser::SetWireStatsNode>(parseSet(
		"set wire_stats")).arg.empty());
	BOOST_TEST((std::get<FrontendParser::SetWireStatsNode>(parseSet(
		"set wire_stats on")).arg == "ON"));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set wire_stats off x", parserOptions)));

	// Engine commands.
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set decfloat", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set decfloat x", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set generator", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set generator x", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set role", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set role x", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set statistics", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set statistics x", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set time zone", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set trusted", parserOptions)));
	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"set trusted x", parserOptions)));
}

BOOST_AUTO_TEST_CASE(ParseShowTest)
{
	const FrontendParser::Options parserOptions;

	const auto parseShow = [&](const std::string_view text) {
		return std::get<FrontendParser::AnyShowNode>(FrontendParser::parse(text, parserOptions));
	};

	BOOST_TEST((std::holds_alternative<FrontendParser::ShowNode>(parseShow("show"))));

	BOOST_TEST(!std::get<FrontendParser::ShowChecksNode>(parseShow(
		"show check")).name);
	BOOST_TEST((std::get<FrontendParser::ShowChecksNode>(parseShow(
		"show check name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowCollationsNode>(parseShow(
		"show collate")).name);
	BOOST_TEST((std::get<FrontendParser::ShowCollationsNode>(parseShow(
		"show collate name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowCollationsNode>(parseShow(
		"show collation")).name);
	BOOST_TEST((std::get<FrontendParser::ShowCollationsNode>(parseShow(
		"show collation name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(std::holds_alternative<FrontendParser::ShowCommentsNode>(parseShow(
		"show comments")));

	BOOST_TEST(!std::get<FrontendParser::ShowDependenciesNode>(parseShow(
		"show depen")).name);
	BOOST_TEST((std::get<FrontendParser::ShowDependenciesNode>(parseShow(
		"show depen name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowDomainsNode>(parseShow(
		"show domain")).name);
	BOOST_TEST((std::get<FrontendParser::ShowDomainsNode>(parseShow(
		"show domain name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowExceptionsNode>(parseShow(
		"show excep")).name);
	BOOST_TEST((std::get<FrontendParser::ShowExceptionsNode>(parseShow(
		"show excep name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowFiltersNode>(parseShow(
		"show filter")).name);
	BOOST_TEST((std::get<FrontendParser::ShowFiltersNode>(parseShow(
		"show filter name")).name == "NAME"));

	BOOST_TEST(!std::get<FrontendParser::ShowFunctionsNode>(parseShow(
		"show func")).name);
	BOOST_TEST((std::get<FrontendParser::ShowFunctionsNode>(parseShow(
		"show func name")).name == QualifiedMetaString("NAME")));
	BOOST_TEST((std::get<FrontendParser::ShowFunctionsNode>(parseShow(
		"show func schema.package.name")).name == QualifiedMetaString("NAME", "SCHEMA", "PACKAGE")));

	BOOST_TEST(!std::get<FrontendParser::ShowIndexesNode>(parseShow(
		"show ind")).name);
	BOOST_TEST((std::get<FrontendParser::ShowIndexesNode>(parseShow(
		"show index name")).name == QualifiedMetaString("NAME")));
	BOOST_TEST((std::get<FrontendParser::ShowIndexesNode>(parseShow(
		"show indices name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowGeneratorsNode>(parseShow(
		"show gen")).name);
	BOOST_TEST((std::get<FrontendParser::ShowGeneratorsNode>(parseShow(
		"show generator name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowMappingsNode>(parseShow(
		"show map")).name);
	BOOST_TEST((std::get<FrontendParser::ShowMappingsNode>(parseShow(
		"show mapping name")).name == "NAME"));

	BOOST_TEST(!std::get<FrontendParser::ShowPackagesNode>(parseShow(
		"show pack")).name);
	BOOST_TEST((std::get<FrontendParser::ShowPackagesNode>(parseShow(
		"show package name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowProceduresNode>(parseShow(
		"show proc")).name);
	BOOST_TEST((std::get<FrontendParser::ShowProceduresNode>(parseShow(
		"show proc name")).name == QualifiedMetaString("NAME")));
	BOOST_TEST((std::get<FrontendParser::ShowProceduresNode>(parseShow(
		"show proc schema.package.name")).name == QualifiedMetaString("NAME", "SCHEMA", "PACKAGE")));

	BOOST_TEST(!std::get<FrontendParser::ShowPublicationsNode>(parseShow(
		"show pub")).name);
	BOOST_TEST((std::get<FrontendParser::ShowPublicationsNode>(parseShow(
		"show publication name")).name == "NAME"));

	BOOST_TEST(!std::get<FrontendParser::ShowRolesNode>(parseShow(
		"show role")).name);
	BOOST_TEST((std::get<FrontendParser::ShowRolesNode>(parseShow(
		"show roles name")).name == "NAME"));

	BOOST_TEST(std::holds_alternative<FrontendParser::InvalidNode>(FrontendParser::parse(
		"show seccla", parserOptions)));
	BOOST_TEST(!std::get<FrontendParser::ShowSecClassesNode>(parseShow(
		"show seccla *")).detail);
	BOOST_TEST(std::get<FrontendParser::ShowSecClassesNode>(parseShow(
		"show seccla * detail")).detail);
	BOOST_TEST(!std::get<FrontendParser::ShowSecClassesNode>(parseShow(
		"show seccla * detail")).name);
	BOOST_TEST((std::get<FrontendParser::ShowSecClassesNode>(parseShow(
		"show secclasses name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST((!std::get<FrontendParser::ShowSystemNode>(parseShow(
		"show system")).objType.has_value()));
	BOOST_TEST((!std::get<FrontendParser::ShowSystemNode>(parseShow(
		"show system table")).objType == obj_relation));
	BOOST_TEST((std::get<FrontendParser::ShowTablesNode>(parseShow(
		"show table \"test\"")).name == QualifiedMetaString("test")));
	BOOST_TEST((std::get<FrontendParser::ShowTablesNode>(parseShow(
		"show table \"te\"\"st\"")).name == QualifiedMetaString("te\"st")));

	BOOST_TEST(!std::get<FrontendParser::ShowTablesNode>(parseShow(
		"show table")).name);
	BOOST_TEST((std::get<FrontendParser::ShowTablesNode>(parseShow(
		"show tables name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowTriggersNode>(parseShow(
		"show trig")).name);
	BOOST_TEST((std::get<FrontendParser::ShowTriggersNode>(parseShow(
		"show triggers name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(!std::get<FrontendParser::ShowViewsNode>(parseShow(
		"show view")).name);
	BOOST_TEST((std::get<FrontendParser::ShowViewsNode>(parseShow(
		"show views name")).name == QualifiedMetaString("NAME")));

	BOOST_TEST(std::holds_alternative<FrontendParser::ShowWireStatsNode>(parseShow(
		"show wire_stat")));
	BOOST_TEST(std::holds_alternative<FrontendParser::ShowWireStatsNode>(parseShow(
		"show wire_statistics")));
}


BOOST_AUTO_TEST_SUITE_END()	// FrontendParserTests
BOOST_AUTO_TEST_SUITE_END()	// FrontendParserSuite
BOOST_AUTO_TEST_SUITE_END()	// ISqlSuite
