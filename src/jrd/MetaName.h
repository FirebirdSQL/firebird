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

#ifndef METANAME_H
#define METANAME_H

#include "../common/classes/fb_string.h"
#include "../common/classes/fb_pair.h"
#include "../jrd/constants.h"

#include <atomic>

namespace Firebird {

class StrWrapper;

}


namespace Jrd {

class Dictionary : public Firebird::PermanentStorage
{
public:
	Dictionary(MemoryPool& p);

	class Word
	{
	public:
		Word* next;
		char text[1];
	};

	Word* get(const char* str, FB_SIZE_T l);

private:
	static const unsigned HASHSIZE = 10007;
	std::atomic<Word*> hashTable[HASHSIZE];

	class Segment
	{
	public:
		Segment();
		Word* getSpace(FB_SIZE_T l);

	private:
		static const unsigned BUFFERSIZE = 4096;		// size in sizeof(pointer)
		void* buffer[BUFFERSIZE];
		std::atomic<unsigned int> position;
	};

	Segment* segment;
	Firebird::Mutex newSegMutex;
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

	MetaName(const Firebird::StrWrapper& s);

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

	MetaName& operator=(const Firebird::StrWrapper& s);

	FB_SIZE_T length() const
	{
		return word ? fb_strlen(word->text) : 0;
	}

	const char* c_str() const
	{
		return word ? word->text : EMPTY;
	}

	const char* nullStr() const
	{
		return word ? word->text : nullptr;
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
		return word->text[n];
	}

	const char* begin() const
	{
		return word ? word->text : EMPTY;
	}

	const char* end() const
	{
		return word ? &word->text[length()] : EMPTY;
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
	operator Firebird::StrWrapper() const;

protected:
	static void adjustLength(const char* const s, FB_SIZE_T& l);
};

typedef Firebird::Pair<Firebird::Full<MetaName, MetaName> > MetaNamePair;

} // namespace Jrd

#endif // METANAME_H
