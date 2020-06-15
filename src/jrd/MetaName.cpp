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

int MetaName::compare(const char* s, FB_SIZE_T len) const
{
	if (s)
	{
		adjustLength(s, len);
		FB_SIZE_T x = length() < len ? length() : len;
		int rc = memcmp(c_str(), s, x);
		if (rc)
		{
			return rc;
		}
	}
	else
		len = 0;

	return length() - len;
}

void MetaName::adjustLength(const char* const s, FB_SIZE_T& len)
{
	if (len > MAX_SQL_IDENTIFIER_LEN)
	{
		fb_assert(s);
#ifdef DEV_BUILD
		for (FB_SIZE_T i = MAX_SQL_IDENTIFIER_LEN; i < len; ++i)
			fb_assert(s[i] == '\0' || s[i] == ' ');
#endif
		len = MAX_SQL_IDENTIFIER_LEN;
	}
	while (len)
	{
		if (s[len - 1] != ' ')
		{
			break;
		}
		--len;
	}
}

void MetaName::printf(const char* format, ...)
{
	char data[MAX_SQL_IDENTIFIER_LEN + 1];
	va_list params;
	va_start(params, format);
	int len = VSNPRINTF(data, MAX_SQL_IDENTIFIER_LEN, format, params);
	va_end(params);

	if (len < 0 || FB_SIZE_T(len) > MAX_SQL_IDENTIFIER_LEN)
	{
		len = MAX_SQL_IDENTIFIER_LEN;
	}
	data[len] = 0;
	word = get(data, len);
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
#if defined(DEV_BUILD) || GROW_DEBUG > 0
	if (word)
	{
		Dictionary::Word* checkWord = get(word->c_str(), word->length());
		fb_assert(checkWord == word);
#ifndef DEV_BUILD
		if (word != checkWord)
			abort();
#endif
	}
#endif
}

const char* MetaName::EMPTY = "";

#if GROW_DEBUG > 1
static const unsigned int hashSize[] = {1000, 2000, 4000, 6000, 8000, 10000,
										12000, 14000, 16000, 18000, 20000,
										22000, 24000, 26000, 28000, 30000,
										32000, 34000, 36000, 38000, 40000,
										42000, 44000, 46000, 48000, 50000,
										52000, 54000, 56000, 58000, 60000};
#else
static const unsigned int hashSize[] = { 10007, 100003, 1000003 };
#endif

Dictionary::Dictionary(MemoryPool& p)
	: Firebird::PermanentStorage(p),
#if DIC_STATS > 0
	  words(0), totLength(0), lostWords(0), conflicts(0), retriesHash(0), retriesSegment(0),
#endif
	  hashTable(FB_NEW_POOL(getPool()) HashTable(getPool(), 0)),
	  nextLevel(0),
	  segment(FB_NEW_POOL(getPool()) Segment),
	  segCount(1)
{ }

#if DIC_STATS > 0
Dictionary::~Dictionary()
{
#define LINESEP "\n\t\t"
	gds__log("Dictionary statistics:" LINESEP
				"words %" UQUADFORMAT LINESEP
				"average length %.02f" LINESEP
				"hash size at level %u is %u" LINESEP
				"lost words %" UQUADFORMAT LINESEP
				"conflicts on mutex %" UQUADFORMAT LINESEP
				"retries in hash table %" UQUADFORMAT LINESEP
				"segments total %u" LINESEP
				"retries in segment %" UQUADFORMAT "\n",
			words.load(), double(totLength) / words.load(),
			hashTable.load()->level, hashSize[hashTable.load()->level],
			lostWords.load(), conflicts.load(),
			retriesHash.load(), segCount, retriesSegment.load());
}
#endif

Dictionary::HashTable::HashTable(MemoryPool& p, unsigned lvl)
	: level(lvl),
	  table(FB_NEW_POOL(p) TableData[hashSize[level]])
{
	for (unsigned n = 0; n < hashSize[level]; ++n)
		table[n].store(nullptr, std::memory_order_relaxed);
}

unsigned Dictionary::HashTable::getMaxLevel()
{
	return FB_NELEM(hashSize) - 1;
}

void Dictionary::Word::assign(const char* s, FB_SIZE_T len)
{
	fb_assert(len < MAX_UCHAR);
	textLen = len;
	memcpy(text, s, len);
	text[len] = '\0';
}

Dictionary::Word* MetaName::get(const char* s, FB_SIZE_T len)
{
	// normalize metadata name
	adjustLength(s, len);
	if (!len)
		return nullptr;

	// get dictionary from TLS
	thread_db* tdbb = JRD_get_thread_data();
	fb_assert(tdbb);
	fb_assert(tdbb->getDatabase());
	Dictionary& dic = tdbb->getDatabase()->dbb_dic;

	// use that dictionary to find appropriate word
	return dic.get(s, len);
}

Dictionary::TableData* Dictionary::HashTable::getEntryByHash(const char* s, FB_SIZE_T len)
{
	unsigned h = Firebird::InternalHash::hash(len, reinterpret_cast<const UCHAR*>(s), hashSize[level]);
	return &table[h];
}

bool Dictionary::checkConsistency(Dictionary::HashTable* oldValue)
{
	return oldValue->level == nextLevel.load();
}

Dictionary::Word* Dictionary::get(const char* s, FB_SIZE_T len)
{
	Word* newWord = nullptr;

	// first of all get current hash table and entry appropriate for a string
	HashTable* t = hashTable.load();
	TableData* d = t->getEntryByHash(s, len);

	// restart loop
	for(;;)
	{
		// to be used if adding new word to hash later
		Word* hashWord = d->load();

		// try to find existing word
		Word* word = hashWord;
		while (word)
		{
			if (word->length() == len && memcmp(word->c_str(), s, len) == 0)
			{
				// Avoid finding duplicate - if at this step when word is located
				// hash level did not change we definitely got correct word
				if (!checkConsistency(t))
					break;

#if DIC_STATS > 0
				if (newWord)
					++lostWords;
#endif

				return word;
			}
			word = word->next;
		}

		// check for previously allocated space presence
		if (!newWord)
		{
			// we have not done anything tragic yet
			// now it's good time to check - does anyone increments hash size
			if (!checkConsistency(t))
			{
				// Wait for completion, switch to new hash table & repeat everything
				t = waitForMutex();
				d = t->getEntryByHash(s, len);
				continue;
			}

			// allocate space for new word
			newWord = segment->getSpace(len DIC_STAT_SEGMENT_CALL);
			if (!newWord)
			{
				Firebird::MutexEnsureUnlock guard(mutex, FB_FUNCTION);
				if (guard.tryEnter())
				{
					// retry allocation to avoid a case when someone already changed segment
					newWord = segment->getSpace(len DIC_STAT_SEGMENT_CALL);

					// do we really need new segment?
					if (!newWord)
					{
						segment = FB_NEW_POOL(getPool()) Segment;
						++segCount;
						unsigned lvl = nextLevel.load();
						if (lvl < HashTable::getMaxLevel() &&
							segCount * Segment::getWordCapacity() > hashSize[lvl])
						{
							growHash();
						}
					}
					else
						newWord->assign(s, len);
				}
				else
				{
					// somebody already changes segment and/or grows hash size - let's wait for it
					HashTable* tNew = waitForMutex();
					if (tNew != t)
					{
						t = tNew;
						d = t->getEntryByHash(s, len);
					}
				}

				continue;
			}

			// fill allocated space
			newWord->assign(s, len);
		}

		// by all means minimize a chance of getting duplicate word:
		// check consistency right before adding word to dictionary
		if (!checkConsistency(t))
		{
			// Wait for completion, switch to new hash table & repeat everything
			t = waitForMutex();
			d = t->getEntryByHash(s, len);
			continue;
		}

		// complete operation - try to replace hash pointer
		newWord->next = hashWord;
		if (d->compare_exchange_weak(hashWord, newWord,
			std::memory_order_seq_cst, std::memory_order_relaxed))
		{
			// very rare case - finally avoid having duplicates
			if (!checkConsistency(t))
			{
				// we do not know when did shit happen
				// let's check new word and fix things if something gone wrong
				t = waitForMutex(&newWord);
				if (t)
				{
					// must repeat with new hash table
					d = t->getEntryByHash(s, len);
					continue;
				}
			}

#if DIC_STATS > 0
			++words;
			totLength += len;
#endif
			return newWord;
		}

#if DIC_STATS > 0
		++retriesHash;
#endif
	}
}

void Dictionary::growHash()
{
	fb_assert(mutex.locked());

	// move one level up size of hash table
	HashTable* tab = hashTable.load();
	unsigned lvl = ++nextLevel;
	fb_assert(lvl == tab->level + 1);
	fb_assert(lvl <= HashTable::getMaxLevel());

#if GROW_DEBUG > 0
	fprintf(stderr, "Hash grow to %d\n", hashSize[lvl]);
#endif

	// create new hash table
	HashTable* newTab = FB_NEW_POOL(getPool()) HashTable(getPool(), lvl);

	// we have mutex locked - but it does not mean others will not touch old hash table
	// that's why do not forget to be careful when getting elements from it
	// on the other hand new table may be modified freely - it's local for this thread after creation

	for (unsigned n = 0; n < hashSize[tab->level]; ++n)
	{
		// detach list of words from old table in safe way
		TableData* tableEntry = &tab->table[n];
		Word* list = tableEntry->load();
		while(!tableEntry->compare_exchange_weak(list, nullptr))
			;		// empty body of the loop

		// append detached list of words to new table word by word
		while (list)
		{
			Word* next = list->next;

			TableData* e = newTab->getEntryByHash(list->c_str(), list->length());

			list->next = e->load(std::memory_order_relaxed);
			e->store(list, std::memory_order_relaxed);

			list = next;
		}
	}

	// new hash table is ready - install it
	// noone will concurrently modify it but it can be read, i.e. membar is needed
	hashTable.store(newTab);
}

Dictionary::HashTable* Dictionary::waitForMutex(Jrd::Dictionary::Word** checkWordPtr)
{
	Firebird::MutexLockGuard guard(mutex, FB_FUNCTION);

#if DIC_STATS > 0
	++conflicts;
#endif

	HashTable* t = hashTable.load();
	if (!checkWordPtr)
		return t;

	// may be we already have that word in new table
	FB_SIZE_T len = (*checkWordPtr)->length();
	const char* s = (*checkWordPtr)->c_str();
	Word* word = t->getEntryByHash(s, len)->load();
	while (word)
	{
		if (word->length() == len && memcmp(word->c_str(), s, len) == 0)
		{
			// successfully found same word in new table - use it
			*checkWordPtr = word;
			return nullptr;
		}
		word = word->next;
	}

	// checkWord was added to old hash table right now & is missing in new one
	// we should repeat adding it to the new hash table
	return t;
}

Dictionary::Segment::Segment()
{
	position.store(0, std::memory_order_relaxed);
}

unsigned Dictionary::Segment::getWordCapacity()
{
	const unsigned AVERAGE_BYTES_LEN = 16;
	return SEG_BUFFER_SIZE / getWordLength(AVERAGE_BYTES_LEN);
}

unsigned Dictionary::Segment::getWordLength(FB_SIZE_T len)
{
	// calculate length in sizeof(Word*)
	len += 2;
	return 1 + (len / sizeof(Word*)) + (len % sizeof(Word*) ? 1 : 0);
}

Dictionary::Word* Dictionary::Segment::getSpace(FB_SIZE_T len DIC_STAT_SEGMENT_PAR)
{
	len = getWordLength(len);

	// get old position value
	unsigned oldPos = position.load();

	// restart loop
	for(;;)
	{
		// calculate and check new position
		unsigned newPos = oldPos + len;
		if (newPos >= SEG_BUFFER_SIZE)
			break;

		// try to store it safely in segment header
		if (position.compare_exchange_strong(oldPos, newPos))
		{
			return reinterpret_cast<Word*>(&buffer[oldPos]);
		}

#if DIC_STATS > 0
		++retries;
#endif
	}

	// Segment out of space
	return nullptr;
}

} // namespace Jrd
