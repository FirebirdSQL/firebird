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
 *  Copyright (c) 2019 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 */

#include "firebird.h"
#include "../common/SimilarToRegex.h"
#include "../common/StatusArg.h"
#include "../common/unicode_util.h"
#include <unicode/utf8.h>

using namespace Firebird;

namespace
{
	bool hasChar(unsigned len, unsigned pos)
	{
		return pos < len;
	}

	UChar32 getChar(bool latin, const char* str, unsigned len, unsigned& pos)
	{
		if (!hasChar(len, pos))
			status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

		UChar32 c;

		if (latin)
			c = str[pos++];
		else
		{
			U8_NEXT_UNSAFE(str, pos, c);

			if (c < 0)
				status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
		}

		return c;
	}

	static const unsigned COMP_FLAG_PREFER_FEWER = 0x01;
	static const unsigned COMP_FLAG_GROUP_CAPTURE = 0x02;
	static const unsigned COMP_FLAG_CASE_INSENSITIVE = 0x04;
	static const unsigned COMP_FLAG_LATIN = 0x08;
	static const unsigned COMP_FLAG_WELLFORMED = 0x10;

	class SimilarToCompiler
	{
	public:
		SimilarToCompiler(MemoryPool& pool, AutoPtr<RE2>& regexp, unsigned aFlags,
				const char* aPatternStr, unsigned aPatternLen,
				const char* escapeStr, unsigned escapeLen)
			: re2PatternStr(pool),
			  patternStr(aPatternStr),
			  patternPos(0),
			  patternLen(aPatternLen),
			  flags(aFlags),
			  useEscape(escapeStr != nullptr)
		{
			if (!(flags & COMP_FLAG_LATIN) && !(flags & COMP_FLAG_WELLFORMED))
			{
				if (!Jrd::UnicodeUtil::utf8WellFormed(patternLen, reinterpret_cast<const UCHAR*>(patternStr), nullptr))
					status_exception::raise(Arg::Gds(isc_malformed_string));
			}

			if (escapeStr)
			{
				if (!(flags & COMP_FLAG_LATIN) && !(flags & COMP_FLAG_WELLFORMED))
				{
					if (!Jrd::UnicodeUtil::utf8WellFormed(escapeLen, reinterpret_cast<const UCHAR*>(escapeStr), nullptr))
						status_exception::raise(Arg::Gds(isc_malformed_string));
				}

				unsigned escapePos = 0;
				escapeChar = getChar(flags & COMP_FLAG_LATIN, escapeStr, escapeLen, escapePos);

				if (escapePos != escapeLen)
					status_exception::raise(Arg::Gds(isc_escape_invalid));
			}

			if (flags & COMP_FLAG_GROUP_CAPTURE)
				re2PatternStr.append("(");

			int parseFlags;
			parseExpr(&parseFlags);

			if (flags & COMP_FLAG_GROUP_CAPTURE)
				re2PatternStr.append(")");

			// Check for proper termination.
			if (patternPos < patternLen)
				status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

			RE2::Options options;
			options.set_log_errors(false);
			options.set_dot_nl(true);
			options.set_case_sensitive(!(flags & COMP_FLAG_CASE_INSENSITIVE));
			options.set_encoding(flags & COMP_FLAG_LATIN ? RE2::Options::EncodingLatin1 : RE2::Options::EncodingUTF8);

			re2::StringPiece sp((const char*) re2PatternStr.c_str(), re2PatternStr.length());
			regexp = FB_NEW_POOL(pool) RE2(sp, options);

			if (!regexp->ok())
				status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
		}

		bool hasPatternChar()
		{
			return patternPos < patternLen;
		}

		UChar32 getPatternChar()
		{
			return getChar(flags & COMP_FLAG_LATIN, patternStr, patternLen, patternPos);
		}

		UChar32 peekPatternChar()
		{
			auto savePos = patternPos;
			auto c = getPatternChar();
			patternPos = savePos;
			return c;
		}

		bool isRep(UChar32 c) const
		{
			return c == '*' || c == '+' || c == '?' || c == '{';
		}

		bool isRe2Special(UChar32 c)
		{
			switch (c)
			{
				case '\\':
				case '$':
				case '.':
				case '^':
				case '-':
				case '_':
				case '[':
				case ']':
				case '(':
				case ')':
				case '{':
				case '}':
				case '|':
				case '?':
				case '+':
				case '*':
					return true;

				default:
					return false;
			}
		}

		void parseExpr(int* parseFlagOut)
		{
			while (true)
			{
				int parseFlags;
				parseTerm(&parseFlags);
				*parseFlagOut &= ~(~parseFlags & PARSE_FLAG_NOT_EMPTY);
				*parseFlagOut |= parseFlags;

				auto savePos = patternPos;
				UChar32 c;

				if (!hasPatternChar() || (c = getPatternChar()) != '|')
				{
					patternPos = savePos;
					break;
				}

				re2PatternStr.append("|");
			}
		}

		void parseTerm(int* parseFlagOut)
		{
			*parseFlagOut = 0;

			bool first = true;

			while (hasPatternChar())
			{
				auto c = peekPatternChar();

				if (c != '|' && c != ')')
				{
					int parseFlags;
					parseFactor(&parseFlags);

					*parseFlagOut |= parseFlags & PARSE_FLAG_NOT_EMPTY;

					if (first)
					{
						*parseFlagOut |= parseFlags;
						first = false;
					}
				}
				else
					break;
			}
		}

		void parseFactor(int* parseFlagOut)
		{
			int parseFlags;
			parsePrimary(&parseFlags);

			UChar32 op;

			if (!hasPatternChar() || !isRep((op = peekPatternChar())))
			{
				*parseFlagOut = parseFlags;
				return;
			}

			if (!(parseFlags & PARSE_FLAG_NOT_EMPTY) && op != '?')
				status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

			fb_assert(op == '*' || op == '+' || op == '?' || op == '{');

			if (op == '*')
			{
				re2PatternStr.append((flags & COMP_FLAG_PREFER_FEWER) ? "*?" : "*");
				*parseFlagOut = 0;
				++patternPos;
			}
			else if (op == '+')
			{
				re2PatternStr.append((flags & COMP_FLAG_PREFER_FEWER) ? "+?" : "+");
				*parseFlagOut = PARSE_FLAG_NOT_EMPTY;
				++patternPos;
			}
			else if (op == '?')
			{
				re2PatternStr.append((flags & COMP_FLAG_PREFER_FEWER) ? "??" : "?");
				*parseFlagOut = 0;
				++patternPos;
			}
			else if (op == '{')
			{
				const auto repeatStart = patternPos++;

				bool comma = false;
				string s1, s2;

				while (true)
				{
					if (!hasPatternChar())
						status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

					UChar32 c = getPatternChar();

					if (c == '}')
					{
						if (s1.isEmpty())
							status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
						break;
					}
					else if (c == ',')
					{
						if (comma)
							status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
						comma = true;
					}
					else
					{
						if (c >= '0' && c <= '9')
						{
							if (comma)
								s2 += (char) c;
							else
								s1 += (char) c;
						}
						else
							status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
					}
				}

				const int n1 = atoi(s1.c_str());
				*parseFlagOut = n1 == 0 ? 0 : PARSE_FLAG_NOT_EMPTY;

				re2PatternStr.append(patternStr + repeatStart, patternStr + patternPos);

				if (flags & COMP_FLAG_PREFER_FEWER)
					re2PatternStr.append("?");
			}

			if (hasPatternChar() && isRep(peekPatternChar()))
				status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
		}

		void parsePrimary(int* parseFlagOut)
		{
			*parseFlagOut = 0;

			fb_assert(hasPatternChar());
			auto savePos = patternPos;
			auto op = getPatternChar();

			if (op == '_')
			{
				*parseFlagOut |= PARSE_FLAG_NOT_EMPTY;
				re2PatternStr.append(".");
				return;
			}
			else if (op == '%')
			{
				re2PatternStr.append((flags & COMP_FLAG_PREFER_FEWER) ? ".*?" : ".*");
				return;
			}
			else if (op == '[')
			{
				struct
				{
					const char* similarClass;
					const char* re2ClassInclude;
					const char* re2ClassExcludeUtf;
					const char* re2ClassExcludeLatin;
				} static const classes[] =
					{
						{"alnum", "[:alnum:]", "[:^alnum:]", "[:^alnum:]"},
						{"alpha", "[:alpha:]", "[:^alpha:]", "[:^alpha:]"},
						{"digit", "[:digit:]", "[:^digit:]", "[:^digit:]"},
						{"lower", "[:lower:]", "[:^lower:]", "[:^lower:]"},
						{"space", " ", "\\x00-\\x1F\\x21-\\x{10FFFF}", "\\x00-\\x1F\\x21-\\xFF"},
						{"upper", "[:upper:]", "[:^upper:]", "[:^upper:]"},
						{"whitespace", "[:space:]", "[:^space:]", "[:^space:]"}
					};

				struct Item
				{
					int clazz;
					unsigned firstStart, firstEnd, lastStart, lastEnd;
				};

				const UChar32 maxChar = (flags & COMP_FLAG_LATIN) ? 0xFF : 0x10FFFF;
				Array<Item> items;
				unsigned includeCount = 0;
				bool exclude = false;
				bool invalidInclude = false;

				do
				{
					if (!hasPatternChar())
						status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

					unsigned charSavePos = patternPos;
					UChar32 c = getPatternChar();
					bool charClass = false;

					if (useEscape && c == escapeChar)
					{
						if (!hasPatternChar())
							status_exception::raise(Arg::Gds(isc_escape_invalid));

						charSavePos = patternPos;
						c = getPatternChar();

						if (!(c == escapeChar || SimilarToRegex::isSpecialChar(c)))
							status_exception::raise(Arg::Gds(isc_escape_invalid));
					}
					else
					{
						if (c == '[')
							charClass = true;
						else if (c == '^')
						{
							if (exclude)
								status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

							exclude = true;
							continue;
						}
					}

					Item item;
					bool strip = false;

					if (charClass)
					{
						if (!hasPatternChar() || getPatternChar() != ':')
							status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

						charSavePos = patternPos;

						while (hasPatternChar() && getPatternChar() != ':')
							;

						const SLONG len = patternPos - charSavePos - 1;

						if (!hasPatternChar() || getPatternChar() != ']')
							status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

						for (item.clazz = 0; item.clazz < FB_NELEM(classes); ++item.clazz)
						{
							if (fb_utils::strnicmp(patternStr + charSavePos,
									classes[item.clazz].similarClass, len) == 0)
							{
								break;
							}
						}

						if (item.clazz >= FB_NELEM(classes))
							status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
					}
					else
					{
						item.clazz = -1;

						item.firstStart = item.lastStart = charSavePos;
						item.firstEnd = item.lastEnd = patternPos;

						if (hasPatternChar() && peekPatternChar() == '-')
						{
							getPatternChar();

							charSavePos = patternPos;
							c = getPatternChar();

							if (useEscape && c == escapeChar)
							{
								if (!hasPatternChar())
									status_exception::raise(Arg::Gds(isc_escape_invalid));

								charSavePos = patternPos;
								c = getPatternChar();

								if (!(c == escapeChar || SimilarToRegex::isSpecialChar(c)))
									status_exception::raise(Arg::Gds(isc_escape_invalid));
							}

							item.lastStart = charSavePos;
							item.lastEnd = patternPos;

							unsigned cPos = item.firstStart;
							UChar32 c1 = getChar(flags & COMP_FLAG_LATIN, patternStr, patternLen, cPos);

							cPos = item.lastStart;
							UChar32 c2 = getChar(flags & COMP_FLAG_LATIN, patternStr, patternLen, cPos);

							strip = c1 > c2;
						}
					}

					if (strip)
					{
						if (!exclude)
							invalidInclude = true;
					}
					else
					{
						if (!exclude)
							++includeCount;

						items.add(item);
					}

					if (!hasPatternChar())
						status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
				} while (peekPatternChar() != ']');

				exclude = includeCount < items.getCount();

				auto appendItem = [&](const Item& item, bool negated) {
					if (item.clazz != -1)
					{
						re2PatternStr.append(negated ?
							(flags & COMP_FLAG_LATIN ?
								classes[item.clazz].re2ClassExcludeLatin :
								classes[item.clazz].re2ClassExcludeUtf
							) :
							classes[item.clazz].re2ClassInclude);
					}
					else
					{
						if (negated)
						{
							char hex[40];

							unsigned cPos = item.firstStart;
							UChar32 c = getChar(flags & COMP_FLAG_LATIN, patternStr, patternLen, cPos);

							if (c > 0)
							{
								sprintf(hex, "\\x00-\\x{%X}", (int) c - 1);
								re2PatternStr.append(hex);
							}

							cPos = item.lastStart;
							c = getChar(flags & COMP_FLAG_LATIN, patternStr, patternLen, cPos);

							if (c < maxChar)
							{
								sprintf(hex, "\\x{%X}-\\x{%X}", (int) c + 1, maxChar);
								re2PatternStr.append(hex);
							}
						}
						else
						{
							if (isRe2Special(patternStr[item.firstStart]))
								re2PatternStr.append("\\");

							re2PatternStr.append(patternStr + item.firstStart, patternStr + item.firstEnd);

							if (item.lastStart != item.firstStart)
							{
								re2PatternStr.append("-");

								if (isRe2Special(patternStr[item.lastStart]))
									re2PatternStr.append("\\");

								re2PatternStr.append(patternStr + item.lastStart, patternStr + item.lastEnd);
							}
						}
					}
				};

				if (exclude && includeCount > 1)
				{
					re2PatternStr.append("(?:");

					for (unsigned i = 0; i < includeCount; ++i)
					{
						if (i != 0)
							re2PatternStr.append("|");

						re2PatternStr.append("[");
						re2PatternStr.append("^");
						appendItem(items[i], true);

						for (unsigned j = includeCount; j < items.getCount(); ++j)
							appendItem(items[j], false);

						re2PatternStr.append("]");
					}

					re2PatternStr.append(")");
				}
				else
				{
					if (items.hasData() && !(invalidInclude && includeCount == 0))
					{
						re2PatternStr.append("[");

						if (exclude)
							re2PatternStr.append("^");

						for (unsigned i = 0; i < items.getCount(); ++i)
							appendItem(items[i], exclude && i < includeCount);

						re2PatternStr.append("]");
					}
					else if (invalidInclude)
					{
						char str[30];
						sprintf(str, "[^\\x{0}-\\x{%X}]", maxChar);
						re2PatternStr.append(str);
					}
					else
						re2PatternStr.append(".");
				}

				getPatternChar();
				*parseFlagOut |= PARSE_FLAG_NOT_EMPTY;
			}
			else if (op == '(')
			{
				re2PatternStr.append(flags & COMP_FLAG_GROUP_CAPTURE ? "(" : "(?:");

				int parseFlags;
				parseExpr(&parseFlags);

				if (!hasPatternChar() || getPatternChar() != ')')
					status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

				re2PatternStr.append(")");

				*parseFlagOut |= parseFlags & PARSE_FLAG_NOT_EMPTY;
			}
			else
			{
				patternPos = savePos;

				bool controlChar = false;

				do
				{
					auto charSavePos = patternPos;
					op = getPatternChar();

					if (useEscape && op == escapeChar)
					{
						charSavePos = patternPos;
						op = getPatternChar();

						if (!SimilarToRegex::isSpecialChar(op) && op != escapeChar)
							status_exception::raise(Arg::Gds(isc_escape_invalid));
					}
					else
					{
						if (SimilarToRegex::isSpecialChar(op))
						{
							controlChar = true;
							patternPos = charSavePos;
						}
					}

					if (!controlChar)
					{
						if (isRe2Special(op))
							re2PatternStr.append("\\");

						re2PatternStr.append(patternStr + charSavePos, patternStr + patternPos);
					}
				} while (!controlChar && hasPatternChar());

				if (patternPos == savePos)
					status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

				*parseFlagOut |= PARSE_FLAG_NOT_EMPTY;
			}
		}

		const string& getRe2PatternStr() const
		{
			return re2PatternStr;
		}

	private:
		static const int PARSE_FLAG_NOT_EMPTY = 1;	// known never to match empty string

		string re2PatternStr;
		const char* patternStr;
		unsigned patternPos;
		unsigned patternLen;
		UChar32 escapeChar;
		unsigned flags;
		bool useEscape;
	};

	class SubstringSimilarCompiler
	{
	public:
		SubstringSimilarCompiler(MemoryPool& pool, AutoPtr<RE2>& regexp, unsigned aFlags,
				const char* aPatternStr, unsigned aPatternLen,
				const char* escapeStr, unsigned escapeLen)
			: flags(aFlags),
			  patternStr(aPatternStr),
			  patternPos(0),
			  patternLen(aPatternLen)
		{
			unsigned escapePos = 0;
			escapeChar = getChar(flags & COMP_FLAG_LATIN, escapeStr, escapeLen, escapePos);

			if (escapePos != escapeLen)
				status_exception::raise(Arg::Gds(isc_escape_invalid));

			unsigned positions[2];
			unsigned part = 0;

			while (hasPatternChar())
			{
				auto c = getPatternChar();

				if (c != escapeChar)
					continue;

				if (!hasPatternChar())
					status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

				c = getPatternChar();

				if (c == '"')
				{
					if (part >= 2)
						status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

					positions[part++] = patternPos;
				}
			}

			if (part != 2)
				status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));

			AutoPtr<RE2> regexp1, regexp2, regexp3;

			SimilarToCompiler compiler1(pool, regexp1, COMP_FLAG_PREFER_FEWER | (flags & COMP_FLAG_LATIN),
				aPatternStr, positions[0] - escapeLen - 1, escapeStr, escapeLen);

			SimilarToCompiler compiler2(pool, regexp2, (flags & COMP_FLAG_LATIN),
				aPatternStr + positions[0], positions[1] - positions[0] - escapeLen - 1, escapeStr, escapeLen);

			SimilarToCompiler compiler3(pool, regexp3, COMP_FLAG_PREFER_FEWER | (flags & COMP_FLAG_LATIN),
				aPatternStr + positions[1], patternLen - positions[1], escapeStr, escapeLen);

			string finalRe2Pattern;
			finalRe2Pattern.reserve(
				1 + 	// (
				compiler1.getRe2PatternStr().length() +
				2 +		// )(
				compiler2.getRe2PatternStr().length() +
				2 +		// )(
				compiler3.getRe2PatternStr().length() +
				1		// )
			);

			finalRe2Pattern.append("(");
			finalRe2Pattern.append(compiler1.getRe2PatternStr());
			finalRe2Pattern.append(")(");
			finalRe2Pattern.append(compiler2.getRe2PatternStr());
			finalRe2Pattern.append(")(");
			finalRe2Pattern.append(compiler3.getRe2PatternStr());
			finalRe2Pattern.append(")");

			RE2::Options options;
			options.set_log_errors(false);
			options.set_dot_nl(true);
			options.set_case_sensitive(!(flags & COMP_FLAG_CASE_INSENSITIVE));
			options.set_encoding(flags & COMP_FLAG_LATIN ? RE2::Options::EncodingLatin1 : RE2::Options::EncodingUTF8);

			re2::StringPiece sp((const char*) finalRe2Pattern.c_str(), finalRe2Pattern.length());
			regexp = FB_NEW_POOL(pool) RE2(sp, options);

			if (!regexp->ok())
				status_exception::raise(Arg::Gds(isc_invalid_similar_pattern));
		}

		bool hasPatternChar()
		{
			return patternPos < patternLen;
		}

		UChar32 getPatternChar()
		{
			return getChar(flags & COMP_FLAG_LATIN, patternStr, patternLen, patternPos);
		}

		UChar32 peekPatternChar()
		{
			auto savePos = patternPos;
			auto c = getPatternChar();
			patternPos = savePos;
			return c;
		}

	private:
		unsigned flags;
		const char* patternStr;
		unsigned patternPos;
		unsigned patternLen;
		UChar32 escapeChar;
	};
}	// namespace

namespace Firebird {


SimilarToRegex::SimilarToRegex(MemoryPool& pool, unsigned flags,
		const char* patternStr, unsigned patternLen, const char* escapeStr, unsigned escapeLen)
	: PermanentStorage(pool)
{
	SimilarToCompiler compiler(pool, regexp,
		COMP_FLAG_GROUP_CAPTURE | COMP_FLAG_PREFER_FEWER |
			((flags & SimilarToFlag::CASE_INSENSITIVE) ? COMP_FLAG_CASE_INSENSITIVE : 0) |
			((flags & SimilarToFlag::LATIN) ? COMP_FLAG_LATIN : 0) |
			((flags & SimilarToFlag::WELLFORMED) ? COMP_FLAG_WELLFORMED : 0),
		patternStr, patternLen, escapeStr, escapeLen);

	finalizer = pool.registerFinalizer(finalize, this);
}

SimilarToRegex::~SimilarToRegex()
{
	getPool().unregisterFinalizer(finalizer);
}

void SimilarToRegex::finalize(SimilarToRegex* self)
{
	self->regexp.reset();
}

bool SimilarToRegex::matches(const char* buffer, unsigned bufferLen, Array<MatchPos>* matchPosArray)
{
	re2::StringPiece sp(buffer, bufferLen);

	if (matchPosArray)
	{
		const int argsCount = regexp->NumberOfCapturingGroups();

		Array<re2::StringPiece> resSps(argsCount);
		resSps.resize(argsCount);

		Array<RE2::Arg> args(argsCount);
		args.resize(argsCount);

		Array<RE2::Arg*> argsPtr(argsCount);

		{	// scope
			auto resSp = resSps.begin();

			for (auto& arg : args)
			{
				arg = resSp++;
				argsPtr.push(&arg);
			}
		}

		if (RE2::FullMatchN(sp, *regexp.get(), argsPtr.begin(), argsCount))
		{
			matchPosArray->clear();

			for (const auto& resSp : resSps)
			{
				matchPosArray->push(MatchPos{
					static_cast<unsigned>(resSp.data() - sp.begin()),
					static_cast<unsigned>(resSp.length())
				});
			}

			return true;
		}
		else
			return false;
	}
	else
		return RE2::FullMatch(sp, *regexp.get());
}

//---------------------

SubstringSimilarRegex::SubstringSimilarRegex(MemoryPool& pool, unsigned flags,
		const char* patternStr, unsigned patternLen, const char* escapeStr, unsigned escapeLen)
	: PermanentStorage(pool)
{
	SubstringSimilarCompiler compiler(pool, regexp,
		((flags & SimilarToFlag::CASE_INSENSITIVE) ? COMP_FLAG_CASE_INSENSITIVE : 0) |
			((flags & SimilarToFlag::LATIN) ? COMP_FLAG_LATIN : 0) |
			((flags & SimilarToFlag::WELLFORMED) ? COMP_FLAG_WELLFORMED : 0),
		patternStr, patternLen, escapeStr, escapeLen);

	finalizer = pool.registerFinalizer(finalize, this);
}

SubstringSimilarRegex::~SubstringSimilarRegex()
{
	getPool().unregisterFinalizer(finalizer);
}

void SubstringSimilarRegex::finalize(SubstringSimilarRegex* self)
{
	self->regexp.reset();
}

bool SubstringSimilarRegex::matches(const char* buffer, unsigned bufferLen,
	unsigned* resultStart, unsigned* resultLength)
{
	re2::StringPiece sp(buffer, bufferLen);

	re2::StringPiece spResult;

	if (RE2::FullMatch(sp, *regexp.get(), nullptr, &spResult, nullptr))
	{
		*resultStart = spResult.begin() - buffer;
		*resultLength = spResult.length();
		return true;
	}
	else
		return false;
}


}	// namespace Firebird
