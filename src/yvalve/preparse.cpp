/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		preparse.cpp
 *	DESCRIPTION:	Dynamic SQL pre parser / parser on client side.
 *			This module will probably change to a YACC parser.
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include <stdlib.h>
#include <string.h>
#include "../yvalve/prepa_proto.h"
#include "../yvalve/gds_proto.h"
#include "../yvalve/YObjects.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/StatusArg.h"
#include "../common/Tokens.h"

#include <firebird/Interface.h>

enum pp_vals {
	PP_CREATE = 0,
	PP_DATABASE = 1,
	PP_SCHEMA = 2,
	PP_PAGE_SIZE = 3,
	PP_USER = 4,
	PP_PASSWORD = 5,
	PP_PAGESIZE = 6,
	PP_LENGTH = 7,
	PP_PAGES = 8,
	PP_PAGE = 9,
	PP_SET = 10,
	PP_NAMES = 11,
	PP_ROLE = 12
};


static void generate_error(const Firebird::NoCaseString&, SSHORT, char = 0);

struct pp_table
{
	SCHAR symbol[10];
	SSHORT code;
};

// This should be kept in sync with the rule db_initial_desc of parse.y for CREATE DATABASE.
// Should delete SCHEMA in the future.
static const pp_table pp_symbols[] =
{
	{"CREATE",  PP_CREATE},
	{"DATABASE",  PP_DATABASE},
	{"SCHEMA", PP_SCHEMA},
	{"PAGE_SIZE", PP_PAGE_SIZE},
	{"USER", PP_USER},
	{"PASSWORD", PP_PASSWORD},
	{"PAGESIZE", PP_PAGESIZE},
	{"LENGTH", PP_LENGTH},
	{"PAGES", PP_PAGES},
	{"PAGE", PP_PAGE},
	{"SET", PP_SET},
	{"NAMES", PP_NAMES},
	{"ROLE", PP_ROLE},
	{"", 0}
};


// define the tokens

enum token_vals {
	NO_MORE_TOKENS = -1,
	TOKEN_TOO_LONG = -2,
	UNEXPECTED_END_OF_COMMAND = -3,
	UNEXPECTED_TOKEN = -4,
	STRING = 257,
	NUMERIC = 258,
	SYMBOL = 259
};

static const char* quotes = "\"\'";


using namespace Firebird;


static NoCaseString getToken(unsigned& pos, const Tokens& toks, int symbol = SYMBOL)
{
	if (pos >= toks.getCount())
		generate_error("", UNEXPECTED_END_OF_COMMAND);

	NoCaseString curTok(NoCaseString(toks[pos].text, toks[pos].length));

	switch(symbol)
	{
	case SYMBOL:
		break;

	case STRING:
		if (!strchr(quotes, toks[pos].text[0]))
			generate_error(curTok, UNEXPECTED_TOKEN);
		return toks[pos++].stripped().ToNoCaseString();

	case NUMERIC:
		{
			const char* const end = &toks[pos].text[toks[pos].length];
			for (const char* ptr = toks[pos].text; ptr < end; ++ptr)
			{
				if (*ptr < '0' || *ptr > '9')
					generate_error(curTok, UNEXPECTED_TOKEN);
			}
		}
		break;

	default:
		if (symbol > 0 && symbol <= 127)	// good ascii symbol
		{
			if (toks[pos].length != 1 || toks[pos].text[0] != symbol)
				generate_error(curTok, UNEXPECTED_TOKEN);
		}
		fb_assert(false);
		break;
	}

	++pos;
	return curTok;
}


/**

 	PREPARSE_execute

    @brief

    @param status
    @param ptrAtt
    @param stmt_length
    @param stmt
    @param stmt_eaten
    @param dialect

 **/
bool PREPARSE_execute(CheckStatusWrapper* status, Why::YAttachment** ptrAtt,
					  string& stmt, bool* stmt_eaten, USHORT dialect)
{
	// no use creating separate pool for a couple of strings
	ContextPoolHolder context(getDefaultMemoryPool());

	try
	{
		if (stmt.isEmpty())
		{
			return false;	// let others care
		}

		bool hasUser = true;
		status->init();
		for (int qStrip = 0; qStrip < 2; ++qStrip)
		{
			hasUser = false;

			Tokens tks;
			tks.quotes(quotes);
			tks.parse(stmt.length(), stmt.c_str());

			for (int tokenPos = tks.getCount() - 1; tokenPos >= 0; --tokenPos)
			{
				const Tokens::Tok& token = tks[tokenPos];

				if (token.length > 0 && token.text[0] == '"')
				{
					string newToken = "'";

					for (unsigned i = 1; i < token.length - 1; ++i)
					{
						switch (token.text[i])
						{
							case '\'':
								newToken += "''";
								break;

							case '"':
								++i;
								newToken += '"';
								break;

							default:
								newToken += token.text[i];
						}
					}

					newToken += "'";
					stmt.replace(token.origin, token.length, newToken);
				}
			}

			unsigned pos = 0;

			if (getToken(pos, tks) != pp_symbols[PP_CREATE].symbol)
			{
				return false;
			}

			NoCaseString token(getToken(pos, tks));
			if (token != pp_symbols[PP_DATABASE].symbol && token != pp_symbols[PP_SCHEMA].symbol)
			{
				return false;
			}

			PathName file_name(getToken(pos, tks, STRING).ToPathName());
			*stmt_eaten = false;
			ClumpletWriter dpb(ClumpletReader::dpbList, MAX_DPB_SIZE);

			dpb.insertByte(isc_dpb_overwrite, 0);
			dpb.insertInt(isc_dpb_sql_dialect, dialect);

			SLONG page_size = 0;
			bool matched;

			do
			{
				try
				{
					token = getToken(pos, tks);
				}
				catch (const Exception&)
				{
					*stmt_eaten = true;
					break;
				}

				matched = false;
				for (int i = 3; pp_symbols[i].symbol[0] && !matched; i++)
				{
					if (token == pp_symbols[i].symbol)
					{
						// CVC: What's strange, this routine doesn't check token.length()
						// but it proceeds blindly, trying to exhaust the token itself.

						switch (pp_symbols[i].code)
						{
						case PP_PAGE_SIZE:
						case PP_PAGESIZE:
							token = getToken(pos, tks);
							if (token == "=")
								token = getToken(pos, tks, NUMERIC);

							page_size = token.length() > 8 ? 100000000 : atol(token.c_str());
							dpb.insertInt(isc_dpb_page_size, page_size);
							matched = true;
							break;

						case PP_USER:
							token = getToken(pos, tks, qStrip ? STRING : SYMBOL);

							dpb.insertString(isc_dpb_user_name, token.ToString());
							matched = true;
							hasUser = true;
							break;

						case PP_PASSWORD:
							token = getToken(pos, tks, STRING);

							dpb.insertString(isc_dpb_password, token.ToString());
							matched = true;
							break;

						case PP_ROLE:
							token = getToken(pos, tks);

							dpb.insertString(isc_dpb_sql_role_name, token.ToString());
							matched = true;
							break;

						case PP_SET:
							token = getToken(pos, tks);
							if (token != pp_symbols[PP_NAMES].symbol)
								generate_error(token, UNEXPECTED_TOKEN);
							token = getToken(pos, tks, STRING);

							dpb.insertString(isc_dpb_lc_ctype, token.ToString());
							matched = true;
							break;

						case PP_LENGTH:
							token = getToken(pos, tks);
							if (token == "=")
								token = getToken(pos, tks, NUMERIC);

							// Skip a token for value
							matched = true;
							break;

						case PP_PAGE:
						case PP_PAGES:
							matched = true;
							break;
						} // switch
					} // if
				} // for
			} while (matched);

			RefPtr<Why::Dispatcher> dispatcher(FB_NEW Why::Dispatcher);
			*ptrAtt = dispatcher->createDatabase(status, file_name.c_str(),
				dpb.getBufferLength(), dpb.getBuffer());

			if ((!hasUser) || ((status->getState() & IStatus::STATE_ERRORS) == 0) ||
				(status->getErrors()[1] != isc_login))
			{
				break;
			}
		}
	}
	catch (const Exception& ex)
	{
		if (!(status->getState() & IStatus::STATE_ERRORS))
			ex.stuffException(status);
		return true;
	}

	return true;
}


/**

 	generate_error

    @brief

    @param status
    @param token
    @param error
    @param result

 **/
static void generate_error(const NoCaseString& token, SSHORT error, char result)
{
	string err_string;

	ISC_STATUS_ARRAY temp_status;
	temp_status[0] = isc_arg_gds;
	temp_status[1] = isc_sqlerr;
	temp_status[2] = isc_arg_number;
	temp_status[3] = -104;
	temp_status[4] = isc_arg_gds;

	switch (error)
	{
	case UNEXPECTED_END_OF_COMMAND:
		temp_status[5] = isc_command_end_err;
		temp_status[6] = isc_arg_end;
		break;

	case UNEXPECTED_TOKEN:
	case TOKEN_TOO_LONG:
		if (result)
		{
			err_string = result;
			err_string += token.ToString();
			err_string += result;
		}
		else
			err_string = token.ToString();
		temp_status[5] = isc_token_err;
		temp_status[6] = isc_arg_gds;
		temp_status[7] = isc_random;
		temp_status[8] = isc_arg_string;
		temp_status[9] = (ISC_STATUS)(err_string.c_str());
		temp_status[10] = isc_arg_end;
		break;
	}

	Arg::StatusVector(temp_status).raise();
}
