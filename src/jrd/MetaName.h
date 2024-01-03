/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		MetaName.h
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

#ifndef JRD_METANAME_H
#define JRD_METANAME_H

#include "../common/classes/fb_string.h"
#include "../common/classes/fb_pair.h"
#include "../jrd/constants.h"

#include <atomic>

// 0 - debugging off, 1 - prints stats with normal setup tables,
// 2 - special setup tables for often grow operation
#define GROW_DEBUG 0

// 0 - statistics off, 1 - statistics on
#define DIC_STATS 0

#if DIC_STATS > 0
#define DIC_STAT_SEGMENT_CALL , retriesSegment
#define DIC_STAT_SEGMENT_PAR , Dictionary::StatCnt& retries
#else
#define DIC_STAT_SEGMENT_CALL
#define DIC_STAT_SEGMENT_PAR
#endif

namespace Firebird {

class MetaString;

}


namespace Jrd {

class Dictionary : public Firebird::PermanentStorage
{
public:
	Dictionary(MemoryPool& p);

#if DIC_STATS > 0
	~Dictionary();
#endif

	class Word
	{
	public:
		Word* next;

	private:
		UCHAR textLen;
		char text[1];

	public:
		const char* c_str() const
		{
			return text;
		}

		FB_SIZE_T length() const
		{
			return textLen;
		}

		void assign(const char* s, FB_SIZE_T l);
	};

	Word* get(const char* str, FB_SIZE_T l);
	void growHash();

#if DIC_STATS > 0
	typedef std::atomic<FB_UINT64> StatCnt;
	StatCnt words, totLength, lostWords, conflicts, retriesHash, retriesSegment;
#endif

private:
	typedef std::atomic<Word*> TableData;

	class HashTable
	{
	public:
		HashTable(MemoryPool& p, unsigned lvl);
		Dictionary::TableData* getEntryByHash(const char* s, FB_SIZE_T len);
		static unsigned getMaxLevel();

		const unsigned level;
		TableData* table;
	};
	std::atomic<HashTable*> hashTable;
	std::atomic<unsigned> nextLevel;

	bool checkConsistency(HashTable* oldValue);
	HashTable* waitForMutex(Word** checkWordPtr = nullptr);

	class Segment
	{
	public:
		Segment();
		Word* getSpace(FB_SIZE_T l DIC_STAT_SEGMENT_PAR);
		static unsigned getWordCapacity();

	private:
		static unsigned getWordLength(FB_SIZE_T len);

#if GROW_DEBUG > 1
		static const unsigned SEG_BUFFER_SIZE = 256;		// size in sizeof(pointer)
#else
		static const unsigned SEG_BUFFER_SIZE = 16384;		// size in sizeof(pointer)
#endif
		void* buffer[SEG_BUFFER_SIZE];
		std::atomic<unsigned> position;
	};
	Segment* segment;
	unsigned segCount;

	Firebird::Mutex mutex;	// The single mutex to protect dictionary when needed
};

class MetaName
{
private:
	Dictionary::Word* word;
	static const char* EMPTY;

	void test();
	Dictionary::Word* get(const char* s, FB_SIZE_T l);

	Dictionary::Word* get(const char* s)
	{
		return get(s, s ? fb_strlen(s) : 0);
	}

public:
	MetaName()
		: word(nullptr)
	{ }

	MetaName(const char* s)
		: word(get(s))
	{ }

	MetaName(const Firebird::MetaString& s);

	MetaName(const char* s, FB_SIZE_T l)
		: word(get(s, l))
	{ }

	MetaName(const MetaName& m)
		: word(m.word)
	{
		test();
	}

	MetaName(const Firebird::AbstractString& s)
		: word(get(s.c_str(), s.length()))
	{ }


	explicit MetaName(MemoryPool&)
		: word(nullptr)
	{ }

	MetaName(MemoryPool&, const char* s)
		: word(get(s))
	{ }

	MetaName(MemoryPool&, const char* s, FB_SIZE_T l)
		: word(get(s, l))
	{ }

	MetaName(MemoryPool&, const MetaName& m)
		: word(m.word)
	{
		test();
	}

	MetaName(MemoryPool&, const Firebird::AbstractString& s)
		: word(get(s.c_str(), s.length()))
	{ }


	MetaName& assign(const char* s, FB_SIZE_T l)
	{
		word = get(s, l);
		return *this;
	}

	MetaName& assign(const char* s)
	{
		word = get(s);
		return *this;
	}

	MetaName& operator=(const char* s)
	{
		word = get(s);
		return *this;
	}

	MetaName& operator=(const Firebird::AbstractString& s)
	{
		word = get(s.c_str(), s.length());
		return *this;
	}

	MetaName& operator=(const MetaName& m)
	{
		word = m.word;
		test();
		return *this;
	}

	MetaName& operator=(const Firebird::MetaString& s);

	FB_SIZE_T length() const
	{
		return word ? word->length() : 0;
	}

	const char* c_str() const
	{
		return word ? word->c_str() : EMPTY;
	}

	const char* nullStr() const
	{
		return word ? word->c_str() : nullptr;
	}

	bool isEmpty() const
	{
		return !word;
	}

	bool hasData() const
	{
		return word;
	}

	char operator[](unsigned n) const
	{
		fb_assert(n < length());
		return word->c_str()[n];
	}

	const char* begin() const
	{
		return word ? word->c_str() : EMPTY;
	}

	const char* end() const
	{
		return word ? &word->c_str()[length()] : EMPTY;
	}

	int compare(const char* s, FB_SIZE_T l) const;

	int compare(const char* s) const
	{
		return compare(s, s ? fb_strlen(s) : 0);
	}

	int compare(const Firebird::AbstractString& s) const
	{
		return compare(s.c_str(), s.length());
	}

	int compare(const MetaName& m) const
	{
		if (word == m.word)
			return 0;

		return compare(m.begin(), m.length());
	}

	bool operator==(const char* s) const
	{
		return compare(s) == 0;
	}

	bool operator!=(const char* s) const
	{
		return compare(s) != 0;
	}

	bool operator==(const Firebird::AbstractString& s) const
	{
		return compare(s) == 0;
	}

	bool operator!=(const Firebird::AbstractString& s) const
	{
		return compare(s) != 0;
	}

	bool operator<=(const MetaName& m) const
	{
		return compare(m) <= 0;
	}

	bool operator>=(const MetaName& m) const
	{
		return compare(m) >= 0;
	}

	bool operator< (const MetaName& m) const
	{
		return compare(m) <  0;
	}

	bool operator> (const MetaName& m) const
	{
		return compare(m) >  0;
	}

	bool operator==(const MetaName& m) const
	{
		return word == m.word;
	}

	bool operator!=(const MetaName& m) const
	{
		return word != m.word;
	}

	void printf(const char*, ...);
	FB_SIZE_T copyTo(char* to, FB_SIZE_T toSize) const;
	operator Firebird::MetaString() const;

protected:
	static void adjustLength(const char* const s, FB_SIZE_T& l);
};

typedef Firebird::Pair<Firebird::Full<MetaName, MetaName> > MetaNamePair;

} // namespace Jrd

#endif // JRD_METANAME_H
