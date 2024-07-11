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
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../FrontendLexer.h"
#include <variant>

using namespace Firebird;

BOOST_AUTO_TEST_SUITE(ISqlSuite)
BOOST_AUTO_TEST_SUITE(FrontendLexerSuite)


BOOST_AUTO_TEST_SUITE(FrontendLexerTests)

BOOST_AUTO_TEST_CASE(StripCommentsTest)
{
	BOOST_TEST(FrontendLexer::stripComments(
		"/* comment */ select 1 from rdb$database /* comment */") == "select 1 from rdb$database");

	BOOST_TEST(FrontendLexer::stripComments(
		"-- comment\nselect '123' /* comment */ from rdb$database -- comment\n") == "select '123' from rdb$database");

	BOOST_TEST(FrontendLexer::stripComments(
		" select 1 from rdb$database -- comment") == "select 1 from rdb$database");
}

BOOST_AUTO_TEST_CASE(GetSingleStatementTest)
{
	{	// scope
		const std::string s1 =
			"select /* ; */ -- ;\n"
			"  ';' || q'{;}'\n"
			"  from rdb$database;";

		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(FrontendLexer(
			s1 + "\nselect ...").getSingleStatement(";")).withSemicolon == s1);
	}

	{	// scope
		const std::string s1 = "select 1 from rdb$database; -- comment";
		const std::string s2 = "select 2 from rdb$database;";
		const std::string s3 = "select 3 from rdb$database;";
		const std::string s4 = "execute block returns (o1 integer) as begin o1 = 1;";
		const std::string s5 = "end;";
		const std::string s6 = "?";
		const std::string s7 = "? set;";

		FrontendLexer lexer(s1 + "\n" + s2);

		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon == s1);

		lexer.appendBuffer(s3 + "\n" + s4 + s5);

		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon == s2);
		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon == s3);

		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon == s4);
		lexer.rewind();
		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon == s4 + s5);

		lexer.appendBuffer(s6 + "\n" + s7);
		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon == s6);
		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon == s7);
	}

	{	// scope
		const std::string s1 = "-- comment;;";
		const std::string s2 = "select 1 from rdb$database;";

		FrontendLexer lexer(s1);

		BOOST_TEST(std::get<FrontendLexer::IncompleteTokenError>(lexer.getSingleStatement(";")).insideComment);

		lexer.appendBuffer("\n" + s2);

		BOOST_TEST(std::get<FrontendLexer::SingleStatement>(lexer.getSingleStatement(";")).withSemicolon ==
			s1 + "\n" + s2);
	}

	BOOST_TEST(!std::get<FrontendLexer::IncompleteTokenError>(FrontendLexer(
		"select 1 from rdb$database").getSingleStatement(";")).insideComment);
	BOOST_TEST(std::get<FrontendLexer::IncompleteTokenError>(FrontendLexer(
		"select 1 from rdb$database; /*").getSingleStatement(";")).insideComment);
}

BOOST_AUTO_TEST_CASE(SkipSingleLineCommentsTest)
{
	FrontendLexer lexer(
		"-- comment 0\r\n"
		"set -- comment 1\n"
		"stats -- comment 2\r\n"
		"- -- comment 3\n"
		"-- comment 4\n"
	);

	BOOST_TEST(lexer.getToken().processedText == "SET");
	BOOST_TEST(lexer.getToken().processedText == "STATS");
	BOOST_TEST(lexer.getToken().processedText == "-");
	BOOST_TEST(lexer.getToken().type == FrontendLexer::Token::TYPE_EOF);
}

BOOST_AUTO_TEST_CASE(SkipMultiLineCommentsTest)
{
	FrontendLexer lexer(
		"/* comment 0 */\r\n"
		"set /* comment 1\n"
		"comment 1 continuation\n"
		"*/ stats /* comment 2\r\n"
		"* */ / /* comment 3*/ /* comment 4*/"
	);

	BOOST_TEST(lexer.getToken().processedText == "SET");
	BOOST_TEST(lexer.getToken().processedText == "STATS");
	BOOST_TEST(lexer.getToken().processedText == "/");
	BOOST_TEST(lexer.getToken().type == FrontendLexer::Token::TYPE_EOF);
}

BOOST_AUTO_TEST_CASE(ParseStringsTest)
{
	FrontendLexer lexer(
		"'ab''c\"d' "
		"\"ab''c\"\"d\" "
		"q'{ab'c\"d}' "
		"q'(ab'c\"d)' "
		"q'[ab'c\"d]' "
		"q'<ab'c\"d>' "
		"q'!ab'c\"d!' "
	);

	BOOST_TEST(lexer.getToken().processedText == "ab'c\"d");
	BOOST_TEST(lexer.getToken().processedText == "ab''c\"d");
	BOOST_TEST(lexer.getToken().processedText == "ab'c\"d");
	BOOST_TEST(lexer.getToken().processedText == "ab'c\"d");
	BOOST_TEST(lexer.getToken().processedText == "ab'c\"d");
	BOOST_TEST(lexer.getToken().processedText == "ab'c\"d");
	BOOST_TEST(lexer.getToken().processedText == "ab'c\"d");
}

BOOST_AUTO_TEST_SUITE_END()	// FrontendLexerTests


BOOST_AUTO_TEST_SUITE_END()	// FrontendLexerSuite
BOOST_AUTO_TEST_SUITE_END()	// ISqlSuite
