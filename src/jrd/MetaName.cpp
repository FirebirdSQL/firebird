/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		MetaName.cpp
 *	DESCRIPTION:	metadata name holder
 *
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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2005, 2020 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"

#include <stdarg.h>

#include "../jrd/MetaName.h"
#include "../common/classes/MetaString.h"
#include "../common/classes/RefMutex.h"
#include "../jrd/jrd.h"

namespace Jrd {

int MetaName::compare(const char* s, FB_SIZE_T l) const
{
	if (s)
	{
		adjustLength(s, l);
		FB_SIZE_T x = length() < l ? length() : l;
		int rc = memcmp(c_str(), s, x);
		if (rc)
		{
			return rc;
		}
	}
	else
		l = 0;

	return length() - l;
}

void MetaName::adjustLength(const char* const s, FB_SIZE_T& l)
{
	if (l > MAX_SQL_IDENTIFIER_LEN)
	{
		fb_assert(s);
#ifdef DEV_BUILD
		for (FB_SIZE_T i = MAX_SQL_IDENTIFIER_LEN; i < l; ++i)
			fb_assert(s[i] == '\0' || s[i] == ' ');
#endif
		l = MAX_SQL_IDENTIFIER_LEN;
	}
	while (l)
	{
		if (s[l - 1] != ' ')
		{
			break;
		}
		--l;
	}
}

void MetaName::printf(const char* format, ...)
{
	char data[MAX_SQL_IDENTIFIER_LEN + 1];
	va_list params;
	va_start(params, format);
	int l = VSNPRINTF(data, MAX_SQL_IDENTIFIER_LEN, format, params);
	va_end(params);

	if (l < 0 || FB_SIZE_T(l) > MAX_SQL_IDENTIFIER_LEN)
	{
		l = MAX_SQL_IDENTIFIER_LEN;
	}
	data[l] = 0;
	word = get(data, l);
}

FB_SIZE_T MetaName::copyTo(char* to, FB_SIZE_T toSize) const
{
	fb_assert(to);
	fb_assert(toSize);
	if (--toSize > length())
	{
		toSize = length();
	}
	memcpy(to, c_str(), toSize);
	to[toSize] = 0;
	return toSize;
}

MetaName::operator Firebird::MetaString() const
{
	return Firebird::MetaString(c_str(), length());
}

MetaName::MetaName(const Firebird::MetaString& s)
{
	assign(s.c_str(), s.length());
}

MetaName& MetaName::operator=(const Firebird::MetaString& s)
{
	return assign(s.c_str(), s.length());
}

void MetaName::test()
{
#ifdef DEV_BUILD
	if (word)
		fb_assert(word == get(word->text));
#endif
}

const char* MetaName::EMPTY = "";

Dictionary::Dictionary(MemoryPool& p)
	: Firebird::PermanentStorage(p),
	  segment(FB_NEW_POOL(getPool()) Segment)
{
	for (unsigned n = 0; n < HASHSIZE; ++n)
		hashTable[n].store(nullptr, std::memory_order_relaxed);
}

Dictionary::Word* MetaName::get(const char* s, FB_SIZE_T l)
{
	// normalize metadata name
	adjustLength(s, l);
	if (!l)
		return nullptr;

	// get dictionary from TLS
	thread_db* tdbb = JRD_get_thread_data();
	fb_assert(tdbb);
	fb_assert(tdbb->getDatabase());
	Dictionary& dic = tdbb->getDatabase()->dbb_dic;

	// use that dictionary to find appropriate word
	return dic.get(s, l);
}

Dictionary::Word* Dictionary::get(const char* s, FB_SIZE_T l)
{
	Word* newWord = nullptr;

	// first of all calcualte word's hash
	unsigned h = Firebird::InternalHash::hash(l, reinterpret_cast<const UCHAR*>(s), HASHSIZE);

	// restart loop
	for(;;)
	{
		// to be used if adding new word to hash later
		Word* hashWord = hashTable[h].load();

		// try to find existing word
		Word* word = hashWord;
		while (word)
		{
			if (memcmp(word->text, s, l) == 0 && word->text[l] == '\0')
				return word;
			word = word->next;
		}

		// check for previously allocated space presence
		if (!newWord)
		{
			// allocate space for new word
			newWord = segment->getSpace(l);
			if (!newWord)
			{
				Firebird::MutexEnsureUnlock guard(newSegMutex, FB_FUNCTION);
				if (guard.tryEnter())
				{
					// we need new segment
					segment = FB_NEW_POOL(getPool()) Segment;
				}
				else
				{
					// somebody already changes segment - let's wait for it
					guard.enter();
				}

				continue;
			}

			// fill allocated space
			memcpy(newWord->text, s, l);
			newWord->text[l] = '\0';
		}

		// complete operation - try to replace hash pointer
		newWord->next = hashWord;
		if (hashTable[h].compare_exchange_weak(hashWord, newWord,
			std::memory_order_seq_cst, std::memory_order_relaxed))
		{
			return newWord;
		}
	}
}

Dictionary::Segment::Segment()
{
	position.store(0, std::memory_order_relaxed);
}

Dictionary::Word* Dictionary::Segment::getSpace(FB_SIZE_T l)
{
	// calculate aligned length in sizeof(Word*)
	++l;
	l = 1 + (l / sizeof(Word*)) + (l % sizeof(Word*) ? 1 : 0);

	// restart loop
	for(;;)
	{
		// fix old position value
		unsigned int oldPos = position.load();

		// calculate and check new position
		unsigned int newPos = oldPos + l;
		if (newPos >= BUFFERSIZE)
			break;

		// try to store it safely in segment header
		if (position.compare_exchange_weak(oldPos, newPos,
			std::memory_order_seq_cst, std::memory_order_relaxed))
		{
			return reinterpret_cast<Word*>(&buffer[oldPos]);
		}
	}

	// Segment out of space
	return nullptr;
}

} // namespace Jrd
