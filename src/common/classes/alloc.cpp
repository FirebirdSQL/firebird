/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		alloc.cpp
 *	DESCRIPTION:	Memory Pool Manager (based on B+ tree)
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *
 *  The Original Code was created by James A. Starkey for IBPhoenix.
 *
 *  Copyright (c) 2004 James A. Starkey
 *  All Rights Reserved.
 *
 *  Contributor(s):
 *
 *		Alex Peshkoff <peshkoff@mail.ru>
 *				1. added PermanentStorage and AutoStorage classes.
 *				2. merged parts of Nickolay and Jim code to be used together
 *				3. reworked code to avoid slow behavior for medium-size blocks
 *				   and high memory usage for just created pool
 *
 */

#include "firebird.h"
#include "../common/classes/alloc.h"

#ifdef WIN_NT

#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#endif

#include "../common/classes/fb_tls.h"
#include "../common/classes/locks.h"
#include "../common/classes/init.h"
#include "../common/classes/vector.h"
#include "../common/classes/RefMutex.h"
#include "../common/os/os_utils.h"
#include "../common/os/fbsyslog.h"
#include "iberror.h"

#ifdef USE_VALGRIND
#include <valgrind/memcheck.h>

#ifndef VALGRIND_MAKE_WRITABLE	// Valgrind 3.3
#define VALGRIND_MAKE_WRITABLE	VALGRIND_MAKE_MEM_UNDEFINED
#define VALGRIND_MAKE_NOACCESS	VALGRIND_MAKE_MEM_NOACCESS
#endif

#define VALGRIND_FIX_IT		// overrides suspicious valgrind behavior
#endif	// USE_VALGRIND

namespace {

///#define MEM_DEBUG_EXTERNAL

/*** emergency debugging stuff
static const char* lastFileName;
static int lastLine;
static void* lastBlock;
static void* stopAddress = (void*) 0x2254938;
***/

#undef MEM_DEBUG
#if defined(DEBUG_GDS_ALLOC) && !defined(USE_VALGRIND)
#define MEM_DEBUG
#endif

#ifdef MEM_DEBUG
static const int GUARD_BYTES	= ALLOC_ALIGNMENT; // * 2048;
static const UCHAR INIT_BYTE	= 0xCC;
static const UCHAR GUARD_BYTE	= 0xDD;
static const UCHAR DEL_BYTE		= 0xEE;
#else
static const int GUARD_BYTES = 0;
#endif

template <typename T>
T absVal(T n) noexcept
{
	return n < 0 ? -n : n;
}

#ifdef USE_VALGRIND
// When memory block is deallocated by user from the pool it must pass queue of this
// length before it is actually deallocated and access protection from it removed.
#define DELAYED_FREE_COUNT 1024

// When memory extent is deallocated when pool is destroying it must pass through
// queue of this length before it is actually returned to system
#define DELAYED_EXTENT_COUNT 32

// Circular FIFO buffer of read/write protected extents pending free operation
// Race protected via cache_mutex.
struct DelayedExtent
{
	void* memory; // Extent pointer
	size_t size;  // Size of extent
	int handle;   // Valgrind handle of protected extent block
};

DelayedExtent delayedExtents[DELAYED_EXTENT_COUNT];
size_t delayedExtentCount = 0;
size_t delayedExtentsPos = 0;
#endif

// Uncomment to validate pool on every alloc\release operation.
// Could slowdown pool significantly !
//#define VALIDATE_POOL

typedef Firebird::AtomicCounter::counter_type StatInt;

// We cache this amount of extents to avoid memory mapping overhead
const int MAP_CACHE_SIZE = 16; // == 1 MB
const size_t DEFAULT_ALLOCATION = 65536;

struct ExtentsCache	// C++ aggregate - members are statically initialized to zeros
{
	unsigned count;
	void* data[MAP_CACHE_SIZE];
};

ExtentsCache defaultExtentsCache;
ExtentsCache externalExtentsCache;

#ifndef WIN_NT
struct FailedBlock
{
	size_t blockSize;
	FailedBlock* next;
	FailedBlock** prev;
};

FailedBlock* failedList = NULL;
#endif

void corrupt(const char* text) noexcept
{
#ifdef DEV_BUILD
	fprintf(stderr, "%s\n", text);
	abort();
#endif
}

Firebird::Mutex* cache_mutex = NULL;
int dev_zero_fd = 0;

#if defined(WIN_NT)
size_t get_page_size()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwPageSize;
}
#else
size_t get_page_size()
{
	return sysconf(_SC_PAGESIZE);
}
#endif


inline size_t get_map_page_size()
{
	static volatile size_t map_page_size = 0;
	if (!map_page_size)
	{
		Firebird::MutexLockGuard guard(cache_mutex, "get_map_page_size");
		if (!map_page_size)
			map_page_size = get_page_size();
	}
	return map_page_size;
}

} // anonymous namespace

namespace Firebird {

namespace SemiDoubleLink
{
	// SemiDoubleLink makes it possible to walk list one direction,
	// push/pop/remove members with very efficient back-link to the head pointer somewhere

	template <class T>
	void push(T** where, T* e)
	{
		// set element `e' pointers
		e->prev = where;
		e->next = *where;

		// make next after `e' element (if present) point to it
		if (e->next)
			e->next->prev = &(e->next);

		// make previous element point to `e'
		*(e->prev) = e;
	}

	template <class T>
	void remove(T* e)
	{
		fb_assert(e);
		fb_assert(e->prev);

		// adjust previous pointer in next element ...
		if (e->next)
			e->next->prev = e->prev;

		// ... and next pointer in previous element
		*(e->prev) = e->next;
	}

	template <class T>
	T* pop(T* e)
	{
		if (e)
			remove(e);

		return e;
	}

	template <class T>
	void validate(T* e)
	{
		if (e->next && e->next->prev != &(e->next))
			fatal_exception::raise("bad back link in SemiDoubleLink");
	}
}

#ifdef USE_VALGRIND
// Size of Valgrind red zone applied before and after memory block allocated for user
#define VALGRIND_REDZONE ALLOC_ALIGNMENT
#undef MEM_DEBUG	// valgrind works instead
#else
#define VALGRIND_REDZONE 0
#endif

typedef SLONG INT32;

class MemBlock;
class MemMediumHunk;

class MemHeader
{
public:
	static const size_t SIZE_MASK = 0xFFF8;
	static const size_t MEM_MASK = 0x7;
	static const size_t MEM_HUGE = 0x1;
	static const size_t MEM_REDIRECT = 0x2;
	static const size_t MEM_EXTENT = 0x4;
	static const unsigned OFFSET_SHIFT = 16;

	enum HugeBlock {HUGE_BLOCK};

	union
	{
		MemPool*	pool;
		MemBlock*	next;
	};

private:
	size_t	hdrLength;

public:
#ifdef DEBUG_GDS_ALLOC
	INT32		lineNumber;
	const char	*fileName;
#elif (SIZEOF_VOID_P == 4) && (ALLOC_ALIGNMENT == 16)
	FB_UINT64 dummyAlign;
#endif
#if defined(USE_VALGRIND) && (VALGRIND_REDZONE != 0)
	char mbk_valgrind_redzone[VALGRIND_REDZONE];
#endif

	MemHeader(size_t size)
		: pool(NULL), hdrLength(size)
	{
		fb_assert(size < MAX_USHORT);
		fb_assert(!(size & MEM_MASK));
	}

	MemHeader(size_t size, MemMediumHunk* hunk)
		: pool(NULL), hdrLength(size | ((((UCHAR*)this) - ((UCHAR*)hunk)) << OFFSET_SHIFT))
	{
		off_t fromTheHunk = ((UCHAR*)this) - ((UCHAR*)hunk);	// dup !!!

		fb_assert(size < MAX_USHORT);
		fb_assert(fromTheHunk < MAX_USHORT);
		fb_assert(fromTheHunk > 0);
		fb_assert(!(size & MEM_MASK));
	}

	MemHeader(HugeBlock, size_t size)
		: hdrLength(size | MEM_HUGE)
	{
		fb_assert(!(size & MEM_MASK));
	}

	size_t getSize()
	{
		return hdrLength & MEM_HUGE ? hdrLength & (~MEM_MASK) : hdrLength & SIZE_MASK;
	}

	MemMediumHunk* getHunk()
	{
		fb_assert(!(hdrLength & MEM_HUGE));
		off_t offset = hdrLength >> OFFSET_SHIFT;
		fb_assert(offset > 0);
		return (MemMediumHunk*)(((UCHAR*)this) - offset);
	}

	void setRedirect()
	{
		fb_assert(!redirected());
		hdrLength |= MEM_REDIRECT;
	}

	void resetRedirect(MemPool* parent)
	{
		fb_assert(redirected());
		hdrLength &= ~MEM_REDIRECT;
		pool = parent;
	}

	bool redirected() const
	{
		return hdrLength & MEM_REDIRECT;
	}

	void setExtent()
	{
		fb_assert(!isExtent());
		hdrLength |= MEM_EXTENT;
	}

	void resetExtent()
	{
		fb_assert(isExtent());
		hdrLength &= ~MEM_EXTENT;
	}

	bool isExtent() const
	{
		return hdrLength & MEM_EXTENT;
	}

	void assertBig()
	{
		fb_assert(hdrLength & MEM_HUGE);
	}

#ifdef MEM_DEBUG
	void print_contents(bool used, FILE* file, bool used_only,
		const char* filter_path, const size_t filter_len) noexcept
	{
		if (used || !used_only)
		{
			bool filter = filter_path != NULL;

			if (used && filter && fileName)
				filter = strncmp(filter_path, fileName, filter_len) != 0;

			if (!filter)
			{
				if (used || redirected())
				{
					fprintf(file, "%s %p: size=%" SIZEFORMAT " allocated at %s:%d",
						isExtent() ? "EXTN" : redirected() ? "RDIR" : "USED",
						this, getSize(), fileName, lineNumber);
				}
				else
					fprintf(file, "FREE %p: size=%" SIZEFORMAT, this, getSize());

				if (hdrLength & MEM_HUGE)
					fprintf(file, " HUGE");

				fprintf(file, "\n");
			}
		}
	}
#endif

	void validate(MemPool* p, StatInt& vUse) noexcept
	{
		if (p == pool && !isExtent())
			vUse += getSize();
	}
};

class MemBlock : public MemHeader
{
public:
	union
	{
		UCHAR		body;
		MemBlock**		prev;
	};

	MemBlock(size_t size, MemMediumHunk* hunk)
		: MemHeader(size, hunk)
	{ }

	MemBlock(HugeBlock, size_t size)
		: MemHeader(HUGE_BLOCK, size)
	{ }

	MemBlock(size_t size)
		: MemHeader(size)
	{ }
};


template <typename H>
class MemBaseHunk
{
public:
	H*				next;
	UCHAR*			memory;
	size_t			length;
	size_t			spaceRemaining;

protected:
	MemBaseHunk(size_t spaceAllocated, size_t hunkSize)
		: next(NULL), length(spaceAllocated)
	{
		init(spaceAllocated, hunkSize);
	}

	void newBlock(size_t size)
	{
		fb_assert(size <= spaceRemaining);

		memory += size;
		spaceRemaining -= size;
	}

public:
	void validate(MemPool* pool, size_t hdr, StatInt& vMap, StatInt& vUse) noexcept
	{
		if (length >= DEFAULT_ALLOCATION)
		{
			fb_assert(length == DEFAULT_ALLOCATION);
			vMap += length;
		}

		UCHAR* m = ((UCHAR*) this) + hdr;
		while (m < memory)
		{
			MemBlock* block = (MemBlock*)m;
			block->validate(pool, vUse);
			m += block->getSize();
		}

		if (next)
			next->validate(pool, hdr, vMap, vUse);
	}

#ifdef MEM_DEBUG
	void print_memory(UCHAR* m, FILE* file, MemPool* pool, bool used_only,
		const char* filter_path, const size_t filter_len)
	{
		while (m < memory)
		{
			MemBlock* block = (MemBlock*)m;
			block->print_contents(block->pool == pool, file, used_only, filter_path, filter_len);
			m += block->getSize();
		}
	}
#endif

private:
	void init(size_t spaceAllocated, size_t hunkSize)
	{
		memory = ((UCHAR*) this) + hunkSize;
		spaceRemaining = spaceAllocated - hunkSize;
	}
};

class MemSmallHunk : public MemBaseHunk<MemSmallHunk>
{
public:
	MemSmallHunk(MemSmallHunk** top, size_t spaceAllocated)
		: MemBaseHunk<MemSmallHunk>(spaceAllocated, hdrSize())
	{
		next = *top;
		*top = this;
	}

	MemBlock* newBlock(size_t size)
	{
		MemBlock* rc = new(memory) MemBlock(size);

		MemBaseHunk::newBlock(size);

		return rc;
	}

	void incrUsage()
	{ }

#ifdef MEM_DEBUG
	void print_contents(FILE* file, MemPool* pool, bool used_only,
		const char* filter_path, const size_t filter_len) noexcept
	{
		UCHAR* m = ((UCHAR*) this) + hdrSize();
		fprintf(file, "Small hunk %p: memory=[%p:%p) spaceRemaining=%" SIZEFORMAT " length=%" SIZEFORMAT "\n",
			this, m, memory, spaceRemaining, length);
		print_memory(m, file, pool, used_only, filter_path, filter_len);
	}
#endif

	static size_t hdrSize()
	{
		return MEM_ALIGN(sizeof(MemSmallHunk));
	}
};

class MemMediumHunk : public MemBaseHunk<MemMediumHunk>
{
public:
	MemMediumHunk**	prev;

private:
	unsigned 		useCount;

public:
	MemMediumHunk(MemMediumHunk** top, size_t spaceAllocated)
		: MemBaseHunk(spaceAllocated, hdrSize()),
		  prev(NULL),
		  useCount(0)
	{
		SemiDoubleLink::push(top, this);
	}

	void incrUsage()
	{
		++useCount;
	}

	bool decrUsage()
	{
		fb_assert(useCount > 0);
		return --useCount == 0;
	}

	bool isFree()
	{
		return useCount == 0;
	}

	MemBlock* newBlock(size_t size)
	{
		MemBlock* rc = new(memory) MemBlock(size, this);

		MemBaseHunk::newBlock(size);
		incrUsage();

		return rc;
	}

	void unlinkBlocks()
	{
		UCHAR* m = ((UCHAR*) this) + hdrSize();
		while (m < memory)
		{
			MemBlock* block = (MemBlock*)m;
			m += block->getSize();

			SemiDoubleLink::remove(block);
		}
	}

#ifdef MEM_DEBUG
	void print_contents(FILE* file, MemPool* pool, bool used_only,
		const char* filter_path, const size_t filter_len) noexcept
	{
		UCHAR* m = ((UCHAR*) this) + hdrSize();
		fprintf(file, "Medium hunk %p: memory=[%p:%p) spaceRemaining=%" SIZEFORMAT " length=%" SIZEFORMAT "\n",
			this, m, memory, spaceRemaining, length);
		print_memory(m, file, pool, used_only, filter_path, filter_len);
	}
#endif

	static size_t hdrSize()
	{
		return MEM_ALIGN(sizeof(MemMediumHunk));
	}
};

class MemBigHunk
{
public:
	MemBigHunk*		next;
	MemBigHunk**	prev;
	const size_t	length;
	MemBlock*		block;

	MemBigHunk(MemBigHunk** top, size_t l)
		: next(NULL), prev(NULL), length(l),
		  block(new(((UCHAR*) this) + hdrSize()) MemBlock(MemBlock::HUGE_BLOCK, length - hdrSize()))
	{
		SemiDoubleLink::push(top, this);
	}

#ifdef MEM_DEBUG
	void print_contents(FILE* file, MemPool* pool, bool used_only,
		const char* filter_path, const size_t filter_len) noexcept
	{
		fprintf(file, "Big hunk %p: memory=%p length=%" SIZEFORMAT "\n",
			this, block, length);
		block->print_contents(true, file, used_only, filter_path, filter_len);
	}
#endif

	static size_t hdrSize()
	{
		return MEM_ALIGN(sizeof(MemBigHunk));
	}

	void validate(MemPool* pool, StatInt& vMap, StatInt& vUse)
	{
		SemiDoubleLink::validate(this);
		block->assertBig();
		fb_assert(block->getSize() + hdrSize() == length);

		vMap += FB_ALIGN(length, get_map_page_size());
		block->validate(pool, vUse);
	}
};


enum GetSlotFor { SLOT_ALLOC, SLOT_FREE };

#if ALLOC_ALIGNMENT == 8
const unsigned char lowSlots[] =
{
	0, // 24
	1, // 32
	2, // 40
	3, // 48
	4, // 56
	5, // 64
	6, // 72
	7, // 80
	8, // 88
	8, // 96
	9, // 104
	9, // 112
	10, // 120
	10, // 128
	11, // 136
	11, // 144
	12, // 152
	12, // 160
	13, // 168
	13, // 176
	13, // 184
	14, // 192
	14, // 200
	14, // 208
	15, // 216
	15, // 224
	15, // 232
	16, // 240
	16, // 248
	16, // 256
	16, // 264
	17, // 272
	17, // 280
	17, // 288
	17, // 296
	18, // 304
	18, // 312
	18, // 320
	18, // 328
	18, // 336
	19, // 344
	19, // 352
	19, // 360
	19, // 368
	19, // 376
	20, // 384
	20, // 392
	20, // 400
	20, // 408
	20, // 416
	20, // 424
	21, // 432
	21, // 440
	21, // 448
	21, // 456
	21, // 464
	21, // 472
	22, // 480
	22, // 488
	22, // 496
	22, // 504
	22, // 512
	22, // 520
	22, // 528
	23, // 536
	23, // 544
	23, // 552
	23, // 560
	23, // 568
	23, // 576
	23, // 584
	23, // 592
	24, // 600
	24, // 608
	24, // 616
	24, // 624
	24, // 632
	24, // 640
	24, // 648
	24, // 656
	24, // 664
	25, // 672
	25, // 680
	25, // 688
	25, // 696
	25, // 704
	25, // 712
	25, // 720
	25, // 728
	25, // 736
	25, // 744
	26, // 752
	26, // 760
	26, // 768
	26, // 776
	26, // 784
	26, // 792
	26, // 800
	26, // 808
	26, // 816
	26, // 824
	26, // 832
	27, // 840
	27, // 848
	27, // 856
	27, // 864
	27, // 872
	27, // 880
	27, // 888
	27, // 896
	27, // 904
	27, // 912
	27, // 920
	27, // 928
	28, // 936
	28, // 944
	28, // 952
	28, // 960
	28, // 968
	28, // 976
	28, // 984
	28, // 992
	28, // 1000
	28, // 1008
	28, // 1016
	28, // 1024
};

const unsigned short lowLimits[] =
{
	24, // 0
	32, // 1
	40, // 2
	48, // 3
	56, // 4
	64, // 5
	72, // 6
	80, // 7
	96, // 8
	112, // 9
	128, // 10
	144, // 11
	160, // 12
	184, // 13
	208, // 14
	232, // 15
	264, // 16
	296, // 17
	336, // 18
	376, // 19
	424, // 20
	472, // 21
	528, // 22
	592, // 23
	664, // 24
	744, // 25
	832, // 26
	928, // 27
	1024, // 28
};

const int SLOT_SHIFT = 3;
#elif ALLOC_ALIGNMENT == 16
const unsigned char lowSlots[] =
{
	0, // 32
	1, // 48
	2, // 64
	3, // 80
	4, // 96
	5, // 112
	6, // 128
	7, // 144
	8, // 160
	9, // 176
	9, // 192
	10, // 208
	10, // 224
	11, // 240
	11, // 256
	12, // 272
	12, // 288
	13, // 304
	13, // 320
	14, // 336
	14, // 352
	14, // 368
	15, // 384
	15, // 400
	15, // 416
	16, // 432
	16, // 448
	16, // 464
	17, // 480
	17, // 496
	17, // 512
	17, // 528
	18, // 544
	18, // 560
	18, // 576
	18, // 592
	19, // 608
	19, // 624
	19, // 640
	19, // 656
	19, // 672
	20, // 688
	20, // 704
	20, // 720
	20, // 736
	20, // 752
	21, // 768
	21, // 784
	21, // 800
	21, // 816
	21, // 832
	21, // 848
	22, // 864
	22, // 880
	22, // 896
	22, // 912
	22, // 928
	22, // 944
	23, // 960
	23, // 976
	23, // 992
	23, // 1008
	23, // 1024
};

const unsigned short lowLimits[] =
{
	32, // 0
	48, // 1
	64, // 2
	80, // 3
	96, // 4
	112, // 5
	128, // 6
	144, // 7
	160, // 8
	192, // 9
	224, // 10
	256, // 11
	288, // 12
	320, // 13
	368, // 14
	416, // 15
	464, // 16
	528, // 17
	592, // 18
	672, // 19
	752, // 20
	848, // 21
	944, // 22
	1024, // 23
};

const int SLOT_SHIFT = 4;
#endif

const size_t TINY_SLOTS = FB_NELEM(lowLimits);
const unsigned short* TINY_BLOCK_LIMIT = &lowLimits[TINY_SLOTS - 1];

// Access to slots for small (<= 1Kb) blocks

class LowLimits
{
public:
#if ALLOC_ALIGNMENT == 8
	static const unsigned TOTAL_ELEMENTS = 29;		// TINY_SLOTS
#elif ALLOC_ALIGNMENT == 16
	static const unsigned TOTAL_ELEMENTS = 24;		// TINY_SLOTS
#endif
	static const unsigned TOP_LIMIT = 1024;			// TINY_BLOCK_LIMIT

	static unsigned getSlot(size_t size, GetSlotFor mode)
	{
		// add 2 asserts as long as we have not found better way to declare consts
		fb_assert(TOTAL_ELEMENTS == TINY_SLOTS);
		fb_assert(TOP_LIMIT == *TINY_BLOCK_LIMIT);

		const size_t LOW_LIMIT = lowLimits[0];
		fb_assert(size <= TOP_LIMIT);
		if (size < LOW_LIMIT)
			size = LOW_LIMIT;
		fb_assert(MEM_ALIGN(size) == size);

		unsigned slot = lowSlots[(size - LOW_LIMIT) >> SLOT_SHIFT];
		fb_assert(size <= lowLimits[slot]);
		if (lowLimits[slot] > size && mode == SLOT_FREE)
		{
			if (!slot)
				return ~0;
			--slot;
		}

		return slot;
	}

	static unsigned getSize(unsigned slot)
	{
		fb_assert(slot < TINY_SLOTS);
		return lowLimits[slot];
	}
};


const unsigned char mediumSlots[] =
{
	0, // 1152
	1, // 1280
	2, // 1408
	2, // 1536
	3, // 1664
	3, // 1792
	4, // 1920
	4, // 2048
	5, // 2176
	5, // 2304
	6, // 2432
	6, // 2560
	7, // 2688
	7, // 2816
	7, // 2944
	8, // 3072
	8, // 3200
	8, // 3328
	9, // 3456
	9, // 3584
	9, // 3712
	10, // 3840
	10, // 3968
	10, // 4096
	10, // 4224
	11, // 4352
	11, // 4480
	11, // 4608
	11, // 4736
	12, // 4864
	12, // 4992
	12, // 5120
	12, // 5248
	12, // 5376
	13, // 5504
	13, // 5632
	13, // 5760
	13, // 5888
	13, // 6016
	14, // 6144
	14, // 6272
	14, // 6400
	14, // 6528
	14, // 6656
	14, // 6784
	15, // 6912
	15, // 7040
	15, // 7168
	15, // 7296
	15, // 7424
	15, // 7552
	16, // 7680
	16, // 7808
	16, // 7936
	16, // 8064
	16, // 8192
	16, // 8320
	16, // 8448
	17, // 8576
	17, // 8704
	17, // 8832
	17, // 8960
	17, // 9088
	17, // 9216
	17, // 9344
	17, // 9472
	18, // 9600
	18, // 9728
	18, // 9856
	18, // 9984
	18, // 10112
	18, // 10240
	18, // 10368
	18, // 10496
	18, // 10624
	19, // 10752
	19, // 10880
	19, // 11008
	19, // 11136
	19, // 11264
	19, // 11392
	19, // 11520
	19, // 11648
	19, // 11776
	19, // 11904
	20, // 12032
	20, // 12160
	20, // 12288
	20, // 12416
	20, // 12544
	20, // 12672
	20, // 12800
	20, // 12928
	20, // 13056
	20, // 13184
	20, // 13312
	21, // 13440
	21, // 13568
	21, // 13696
	21, // 13824
	21, // 13952
	21, // 14080
	21, // 14208
	21, // 14336
	21, // 14464
	21, // 14592
	21, // 14720
	21, // 14848
	22, // 14976
	22, // 15104
	22, // 15232
	22, // 15360
	22, // 15488
	22, // 15616
	22, // 15744
	22, // 15872
	22, // 16000
	22, // 16128
	22, // 16256
	22, // 16384
	22, // 16512
	23, // 16640
	23, // 16768
	23, // 16896
	23, // 17024
	23, // 17152
	23, // 17280
	23, // 17408
	23, // 17536
	23, // 17664
	23, // 17792
	23, // 17920
	23, // 18048
	23, // 18176
	23, // 18304
	23, // 18432
	24, // 18560
	24, // 18688
	24, // 18816
	24, // 18944
	24, // 19072
	24, // 19200
	24, // 19328
	24, // 19456
	24, // 19584
	24, // 19712
	24, // 19840
	24, // 19968
	24, // 20096
	24, // 20224
	24, // 20352
	24, // 20480
	25, // 20608
	25, // 20736
	25, // 20864
	25, // 20992
	25, // 21120
	25, // 21248
	25, // 21376
	25, // 21504
	25, // 21632
	25, // 21760
	25, // 21888
	25, // 22016
	25, // 22144
	25, // 22272
	25, // 22400
	25, // 22528
	25, // 22656
	25, // 22784
	26, // 22912
	26, // 23040
	26, // 23168
	26, // 23296
	26, // 23424
	26, // 23552
	26, // 23680
	26, // 23808
	26, // 23936
	26, // 24064
	26, // 24192
	26, // 24320
	26, // 24448
	26, // 24576
	26, // 24704
	26, // 24832
	26, // 24960
	26, // 25088
	26, // 25216
	26, // 25344
	27, // 25472
	27, // 25600
	27, // 25728
	27, // 25856
	27, // 25984
	27, // 26112
	27, // 26240
	27, // 26368
	27, // 26496
	27, // 26624
	27, // 26752
	27, // 26880
	27, // 27008
	27, // 27136
	27, // 27264
	27, // 27392
	27, // 27520
	27, // 27648
	27, // 27776
	27, // 27904
	27, // 28032
	27, // 28160
	28, // 28288
	28, // 28416
	28, // 28544
	28, // 28672
	28, // 28800
	28, // 28928
	28, // 29056
	28, // 29184
	28, // 29312
	28, // 29440
	28, // 29568
	28, // 29696
	28, // 29824
	28, // 29952
	28, // 30080
	28, // 30208
	28, // 30336
	28, // 30464
	28, // 30592
	28, // 30720
	28, // 30848
	28, // 30976
	28, // 31104
	28, // 31232
	28, // 31360
	29, // 31488
	29, // 31616
	29, // 31744
	29, // 31872
	29, // 32000
	29, // 32128
	29, // 32256
	29, // 32384
	29, // 32512
	29, // 32640
	29, // 32768
	29, // 32896
	29, // 33024
	29, // 33152
	29, // 33280
	29, // 33408
	29, // 33536
	29, // 33664
	29, // 33792
	29, // 33920
	29, // 34048
	29, // 34176
	29, // 34304
	29, // 34432
	29, // 34560
	29, // 34688
	29, // 34816
	29, // 34944
	30, // 35072
	30, // 35200
	30, // 35328
	30, // 35456
	30, // 35584
	30, // 35712
	30, // 35840
	30, // 35968
	30, // 36096
	30, // 36224
	30, // 36352
	30, // 36480
	30, // 36608
	30, // 36736
	30, // 36864
	30, // 36992
	30, // 37120
	30, // 37248
	30, // 37376
	30, // 37504
	30, // 37632
	30, // 37760
	30, // 37888
	30, // 38016
	30, // 38144
	30, // 38272
	30, // 38400
	30, // 38528
	30, // 38656
	30, // 38784
	30, // 38912
	31, // 39040
	31, // 39168
	31, // 39296
	31, // 39424
	31, // 39552
	31, // 39680
	31, // 39808
	31, // 39936
	31, // 40064
	31, // 40192
	31, // 40320
	31, // 40448
	31, // 40576
	31, // 40704
	31, // 40832
	31, // 40960
	31, // 41088
	31, // 41216
	31, // 41344
	31, // 41472
	31, // 41600
	31, // 41728
	31, // 41856
	31, // 41984
	31, // 42112
	31, // 42240
	31, // 42368
	31, // 42496
	31, // 42624
	31, // 42752
	31, // 42880
	31, // 43008
	31, // 43136
	31, // 43264
	32, // 43392
	32, // 43520
	32, // 43648
	32, // 43776
	32, // 43904
	32, // 44032
	32, // 44160
	32, // 44288
	32, // 44416
	32, // 44544
	32, // 44672
	32, // 44800
	32, // 44928
	32, // 45056
	32, // 45184
	32, // 45312
	32, // 45440
	32, // 45568
	32, // 45696
	32, // 45824
	32, // 45952
	32, // 46080
	32, // 46208
	32, // 46336
	32, // 46464
	32, // 46592
	32, // 46720
	32, // 46848
	32, // 46976
	32, // 47104
	32, // 47232
	32, // 47360
	32, // 47488
	32, // 47616
	32, // 47744
	32, // 47872
	32, // 48000
	32, // 48128
	33, // 48256
	33, // 48384
	33, // 48512
	33, // 48640
	33, // 48768
	33, // 48896
	33, // 49024
	33, // 49152
	33, // 49280
	33, // 49408
	33, // 49536
	33, // 49664
	33, // 49792
	33, // 49920
	33, // 50048
	33, // 50176
	33, // 50304
	33, // 50432
	33, // 50560
	33, // 50688
	33, // 50816
	33, // 50944
	33, // 51072
	33, // 51200
	33, // 51328
	33, // 51456
	33, // 51584
	33, // 51712
	33, // 51840
	33, // 51968
	33, // 52096
	33, // 52224
	33, // 52352
	33, // 52480
	33, // 52608
	33, // 52736
	33, // 52864
	33, // 52992
	33, // 53120
	33, // 53248
	33, // 53376
	33, // 53504
	34, // 53632
	34, // 53760
	34, // 53888
	34, // 54016
	34, // 54144
	34, // 54272
	34, // 54400
	34, // 54528
	34, // 54656
	34, // 54784
	34, // 54912
	34, // 55040
	34, // 55168
	34, // 55296
	34, // 55424
	34, // 55552
	34, // 55680
	34, // 55808
	34, // 55936
	34, // 56064
	34, // 56192
	34, // 56320
	34, // 56448
	34, // 56576
	34, // 56704
	34, // 56832
	34, // 56960
	34, // 57088
	34, // 57216
	34, // 57344
	34, // 57472
	34, // 57600
	34, // 57728
	34, // 57856
	34, // 57984
	34, // 58112
	34, // 58240
	34, // 58368
	34, // 58496
	34, // 58624
	34, // 58752
	34, // 58880
	34, // 59008
	34, // 59136
	34, // 59264
	34, // 59392
	34, // 59520
	35, // 59648
	35, // 59776
	35, // 59904
	35, // 60032
	35, // 60160
	35, // 60288
	35, // 60416
	35, // 60544
	35, // 60672
	35, // 60800
	35, // 60928
	35, // 61056
	35, // 61184
	35, // 61312
	35, // 61440
	35, // 61568
	35, // 61696
	35, // 61824
	35, // 61952
	35, // 62080
	35, // 62208
	35, // 62336
	35, // 62464
	35, // 62592
	35, // 62720
	35, // 62848
	35, // 62976
	35, // 63104
	35, // 63232
	35, // 63360
	35, // 63488
	35, // 63616
	35, // 63744
	35, // 63872
	35, // 64000
	35, // 64128
	35, // 64256
	35, // 64384
	35  // 64512
};

const unsigned short mediumLimits[] =
{
	1152, // 0
	1280, // 1
	1536, // 2
	1792, // 3
	2048, // 4
	2304, // 5
	2560, // 6
	2944, // 7
	3328, // 8
	3712, // 9
	4224, // 10
	4736, // 11
	5376, // 12
	6016, // 13
	6784, // 14
	7552, // 15
	8448, // 16
	9472, // 17
	10624, // 18
	11904, // 19
	13312, // 20
	14848, // 21
	16512, // 22
	18432, // 23
	20480, // 24
	22784, // 25
	25344, // 26
	28160, // 27
	31360, // 28
	34944, // 29
	38912, // 30
	43264, // 31
	48128, // 32
	53504, // 33
	59520, // 34
	64512  // 35
};

const size_t MEDIUM_SLOTS = FB_NELEM(mediumLimits);
const unsigned short* MEDIUM_BLOCK_LIMIT = &mediumLimits[MEDIUM_SLOTS - 1];
const size_t PARENT_REDIRECT_THRESHOLD = 48 * 1024;

// Access to slots for medium (>1Kb, <64Kb) blocks

class MediumLimits
{
public:
	static const unsigned TOTAL_ELEMENTS = 36;		// MEDIUM_SLOTS
	static const unsigned TOP_LIMIT = 64512;		// MEDIUM_BLOCK_LIMIT

	static unsigned getSlot(size_t size, GetSlotFor mode)
	{
		// add 2 asserts as long as we have not found better way to declare consts
		fb_assert(TOTAL_ELEMENTS == MEDIUM_SLOTS);
		fb_assert(TOP_LIMIT == *MEDIUM_BLOCK_LIMIT);

		const size_t LOW_LIMIT = 1032;
		fb_assert(size <= TOP_LIMIT);
		fb_assert(size >= LOW_LIMIT);
		fb_assert(MEM_ALIGN(size) == size);

		unsigned slot = mediumSlots[(size - LOW_LIMIT) >> 7];
		fb_assert(size <= mediumLimits[slot]);
		if (mediumLimits[slot] > size && mode == SLOT_FREE)
		{
			if (!slot)
				return ~0;
			--slot;
		}

		return slot;
	}

	static unsigned getSize(unsigned slot)
	{
		fb_assert(slot < MEDIUM_SLOTS);
		return mediumLimits[slot];
	}
};


// List of free small blocks of given size

class LinkedList
{
public:
	typedef MemSmallHunk Hunk;
	static const unsigned MEM_OVERHEAD = offsetof(MemBlock, body);

	static MemBlock* getElement(MemBlock** from)
	{
		MemBlock* rc = *from;
		if (rc)
			*from = rc->next;
		return rc;
	}

	static void putElement(MemBlock** to, MemBlock* block)
	{
		block->next = *to;
		*to = block;
	}

	void decrUsage(MemSmallHunk*, MemPool*)
	{ }

	static void validate(MemBlock* block, unsigned length)
	{
		for (; block; block = block->next)
		{
			if (block->getSize() != length)
				corrupt("length trashed for block in slot");
		}
	}
};


// List of free medium blocks of given size

class DoubleLinkedList
{
public:
	typedef MemMediumHunk Hunk;
	static const unsigned MEM_OVERHEAD = offsetof(MemBlock, body);

	DoubleLinkedList()
		: candidateForFree(NULL)
	{ }

	static MemBlock* getElement(MemBlock** from)
	{
		MemBlock* rc = SemiDoubleLink::pop(*from);
		if (rc)
		{
			MemMediumHunk* hunk = rc->getHunk();
			hunk->incrUsage();
		}
		return rc;
	}

	void putElement(MemBlock** to, MemBlock* block);

	static void validate(MemBlock* block, unsigned length)
	{
		for (; block; block = block->next)
		{
			if (block->getSize() != length)
				corrupt("length trashed for block in slot");
			SemiDoubleLink::validate(block);
		}
	}

	void decrUsage(MemMediumHunk* hunk, MemPool* pool);

private:
	MemMediumHunk* candidateForFree;
};


// Array (size == number of slots) of pointers to lists of free blocks

template <class ListBuilder, class Limits>
class FreeObjects
{
private:
	typedef MemBlock* FreeObjPtr;
	typedef typename ListBuilder::Hunk Extent;

public:
	FreeObjects()
		: currentExtent(NULL)
	{
		memset(freeObjects, 0, sizeof(freeObjects));
	}

	~FreeObjects();

	FreeObjPtr allocateBlock(MemPool* pool, size_t from, size_t& size)
	{
		size_t full_size = size + (from ? 0 : ListBuilder::MEM_OVERHEAD);
		if (full_size > Limits::TOP_LIMIT)
			return NULL;

		unsigned slot = Limits::getSlot(full_size, SLOT_ALLOC);
		full_size = Limits::getSize(slot);

		FreeObjPtr blk = ListBuilder::getElement(&freeObjects[slot]);
		if (!blk && from)
		{
			for (unsigned slot1 = slot - 1; Limits::getSize(slot1) >= from; --slot1)
			{
				blk = ListBuilder::getElement(&freeObjects[slot1]);
				if (blk)
				{
					full_size = Limits::getSize(slot1);
					break;
				}

				// This should not happen but try to be as safe as possible
				fb_assert(slot1 > 0);
				if (!slot1)
					break;
			}
		}

		if (!blk)
			blk = newBlock(pool, slot);

		size = full_size - ListBuilder::MEM_OVERHEAD;
		return blk;
	}

	bool deallocateBlock(FreeObjPtr blk)
	{
		size_t size = blk->getSize();

		if (size > Limits::TOP_LIMIT)
			return false;		// Not our block

		unsigned slot = Limits::getSlot(size, SLOT_ALLOC);
		listBuilder.putElement(&freeObjects[slot], blk);
		return true;
	}

#ifdef MEM_DEBUG
	void print_contents(FILE* file, MemPool* pool, bool used_only,
						const char* filter_path, const size_t filter_len) noexcept
	{
		for (Extent* ext = currentExtent; ext; ext = ext->next)
			ext->print_contents(file, pool, used_only, filter_path, filter_len);
	}
#endif

	void validate(MemPool* pool, StatInt& vMap, StatInt& vUse)
	{
		for (unsigned int slot = 0; slot < Limits::TOTAL_ELEMENTS; ++slot)
			ListBuilder::validate(freeObjects[slot], Limits::getSize(slot));

		if (currentExtent)
			currentExtent->validate(pool, currentExtent->hdrSize(), vMap, vUse);
	}

private:
	FreeObjPtr freeObjects[Limits::TOTAL_ELEMENTS];
	ListBuilder listBuilder;
	Extent* currentExtent;

	MemBlock* newBlock(MemPool* pool, unsigned slot);
};


// Implementation of memory pool

class MemPool
{
private:
	void initialize();

public:
	static MemPool* defaultMemPool;

	MemPool(MemoryStats& stats, ExtentsCache* extentsCache);
	MemPool(MemPool& parent, MemoryStats& stats, ExtentsCache* extentsCache);
	virtual ~MemPool(void);

public:
	static inline constexpr MemPool* getPoolFromPointer(void* ptr) noexcept
	{
		if (ptr)
		{
			auto block = (MemBlock*) ((UCHAR*) ptr - offsetof(MemBlock, body));
			return block->pool;
		}

		return nullptr;
	}

private:
	static const size_t minAllocation = 65536;
	static const size_t roundingSize = ALLOC_ALIGNMENT;

	FreeObjects<LinkedList, LowLimits> smallObjects;
	Vector<MemBlock*, 16> parentRedirected;
	FreeObjects<DoubleLinkedList, MediumLimits> mediumObjects;
	MemBigHunk*		bigHunks;

	Mutex			mutex;
	int				blocksAllocated;
	int				blocksActive;
	bool			pool_destroying, parent_redirect;

	MemoryStats* stats;	// Statistics group for the pool
	MemPool* parent;	// Parent pool if present
	ExtentsCache* extentsCache;
	AtomicCounter used_memory, mapped_memory;	// Memory used

private:

#ifdef VALIDATE_POOL
	class Validator
	{
	public:
		Validator(MemPool* p) :
			m_pool(p)
		{
			validate();
		}

		~Validator()
		{
			validate();
		}

	private:
		MemPool* m_pool;

		void validate()
		{
			if (m_pool)
			{
				char buf[256];
				if (!m_pool->validate(buf, sizeof(buf)))
				{
					Syslog::Record(Syslog::Warning, buf);
#ifdef MEM_DEBUG
					m_pool->print_contents("validate.failed", 0, NULL);
#endif
				}
			}
		}
	};
#else
	class Validator
	{
	public:
		Validator(MemPool*) {}
	};
#endif // VALIDATE_POOL

	MemBlock* alloc(size_t from, size_t& length, bool flagRedirect);
	void releaseBlock(MemBlock *block, bool flagDecr) noexcept;

public:
	void* allocate(size_t size ALLOC_PARAMS);
	MemBlock* allocate2(size_t from, size_t& size ALLOC_PARAMS);

private:
	virtual void memoryIsExhausted(void);
	void* allocRaw(size_t length);
	static void releaseMemory(void* block, bool flagExtent) noexcept;
	static void releaseRaw(bool destroying, void *block, size_t size, ExtentsCache* extentsCache) noexcept;
	void* getExtent(size_t from, size_t& to);

public:
	static void releaseExtent(bool destroying, void *block, size_t size, MemPool* pool) noexcept;

	// pass desired size, return actual extent size
	template <class Extent>
	void newExtent(size_t& size, Extent** linkedList);

private:
#ifdef USE_VALGRIND
	// Circular FIFO buffer of read/write protected blocks pending free operation
	MemBlock* delayedFree[DELAYED_FREE_COUNT];
	size_t delayedFreeCount;
	size_t delayedFreePos;
#endif

public:
	static void deletePool(MemPool* pool);
	static void globalFree(void* block) noexcept;

	static void deallocate(void* block) noexcept;
	bool validate(char* buf, FB_SIZE_T size);

	// Create memory pool instance
	static MemPool* createPool(MemPool* parent, MemoryStats& stats);

	MemoryStats& getStatsGroup() noexcept
	{
		return *stats;
	}

	// Set statistics group for pool. Usage counters will be decremented from
	// previously set group and added to new
	void setStatsGroup(MemoryStats& stats) noexcept;

	// Initialize and finalize global memory pool
	static MemPool* initDefaultPool()
	{
		fb_assert(!defaultMemPool);

		alignas(alignof(MemPool)) static char mpBuffer[sizeof(MemPool)];
		defaultMemPool = new(mpBuffer) MemPool(*MemoryPool::default_stats_group, &defaultExtentsCache);
		return defaultMemPool;
	}

	static void cleanupDefaultPool()
	{
		defaultMemPool->~MemPool();
		defaultMemPool = NULL;

		while (defaultExtentsCache.count)
			releaseRaw(true, defaultExtentsCache.data[--defaultExtentsCache.count], DEFAULT_ALLOCATION, nullptr);

		cleanupFailedList();
	}

	static void cleanupExternalPool()
	{
		while (externalExtentsCache.count)
			releaseRaw(true, externalExtentsCache.data[--externalExtentsCache.count], DEFAULT_ALLOCATION, nullptr);

		cleanupFailedList();
	}

	static void cleanupFailedList()
	{
#ifndef WIN_NT
		unsigned oldCount = 0;

		for (;;)
		{
			unsigned newCount = 0;
			FailedBlock* oldList = failedList;

			if (oldList)
			{
				fb_assert(oldList->prev);
				oldList->prev = &oldList;
				failedList = NULL;
			}

			while (oldList)
			{
				++newCount;
				FailedBlock* fb = oldList;
				SemiDoubleLink::pop(oldList);
				releaseRaw(true, fb, fb->blockSize, nullptr);
			}

			if (newCount == oldCount)
				break;

			oldCount = newCount;
		}
#endif // WIN_NT
	}

	// Statistics
	void increment_usage(size_t size) noexcept
	{
		stats->increment_usage(size);
		used_memory += size;
	}

	void decrement_usage(size_t size) noexcept
	{
		stats->decrement_usage(size);
		used_memory -= size;
	}

	void increment_mapping(size_t size) noexcept
	{
		stats->increment_mapping(size);
		mapped_memory += size;
	}

	void decrement_mapping(size_t size) noexcept
	{
		stats->decrement_mapping(size);
		mapped_memory -= size;
	}

#ifdef MEM_DEBUG
	// Print out pool contents. This is debugging routine
	void print_contents(FILE*, unsigned flags, const char* filter_path) noexcept;
	// The same routine, but more easily callable from the debugger
	void print_contents(const char* filename, unsigned flags, const char* filter_path) noexcept;

private:
	MemPool* next;
	MemPool* child;
#endif

friend class MemoryPool;
};


void DoubleLinkedList::putElement(MemBlock** to, MemBlock* block)
{
	MemPool* pool = block->pool;
	MemMediumHunk* hunk = block->getHunk();

	SemiDoubleLink::push(to, block);

	decrUsage(hunk, pool);
}

void DoubleLinkedList::decrUsage(MemMediumHunk* hunk, MemPool* pool)
{
	if (hunk->decrUsage())
	{
		if (candidateForFree && (candidateForFree != hunk) && candidateForFree->isFree())
		{
			candidateForFree->unlinkBlocks();
			SemiDoubleLink::remove(candidateForFree);

			fb_assert(pool);
			MemPool::releaseExtent(false, candidateForFree, candidateForFree->length, pool);
		}

		candidateForFree = hunk;
	}
}


template <class ListBuilder, class Limits>
MemBlock* FreeObjects<ListBuilder, Limits>::newBlock(MemPool* pool, unsigned slot)
{
	size_t size = Limits::getSize(slot);

	if (currentExtent && currentExtent->spaceRemaining < size)
	{
		// put remaining memory into appr. slots
		while (currentExtent->spaceRemaining >= Limits::getSize(0) &&
			   currentExtent->spaceRemaining > ListBuilder::MEM_OVERHEAD)
		{
			unsigned sl1 = Limits::getSlot(currentExtent->spaceRemaining, SLOT_FREE);
			if (sl1 == ~0u)
				break;

			unsigned size1 = Limits::getSize(sl1);
			MemBlock* b = currentExtent->newBlock(size1);

			listBuilder.putElement(&freeObjects[sl1], b);
		}
		currentExtent->spaceRemaining = 0;
		listBuilder.decrUsage(currentExtent, pool);
	}

	if (!(currentExtent && currentExtent->spaceRemaining))
	{
		size_t size2 = size;
		pool->newExtent(size2, &currentExtent);
		currentExtent->incrUsage();
	}

	return currentExtent->newBlock(size);
}

template <class ListBuilder, class Limits>
FreeObjects<ListBuilder, Limits>::~FreeObjects()
{
	while (currentExtent)
	{
		Extent* e = currentExtent;
		currentExtent = currentExtent->next;

		MemPool::releaseExtent(true, e, e->length, NULL);
	}
}


// This is required for modules that do not define any GlobalPtr themself
GlobalPtr<Mutex> forceCreationOfDefaultMemoryPool;

MemoryPool*		MemoryPool::defaultMemoryManager = NULL;
MemoryStats*	MemoryPool::default_stats_group = NULL;
MemoryPool*		MemoryPool::externalMemoryManager = NULL;
MemPool*		MemPool::defaultMemPool = NULL;


// Initialize process memory pool (called from InstanceControl).

void MemoryPool::initDefaultPool()
{
	alignas(alignof(Mutex)) static char mtxBuffer[sizeof(Mutex)];
	cache_mutex = new(mtxBuffer) Mutex;

	alignas(alignof(MemoryStats)) static char msBuffer[sizeof(MemoryStats)];
	default_stats_group = new(msBuffer) MemoryStats;

	alignas(alignof(MemoryPool)) static char mpBuffer[sizeof(MemoryPool)];
	defaultMemoryManager = new(mpBuffer) MemoryPool(MemPool::initDefaultPool());
}

// Should be last routine, called by InstanceControl,
// being therefore the very last routine in firebird module.

void MemoryPool::cleanupDefaultPool()
{
#ifdef VALGRIND_FIX_IT
	VALGRIND_DISCARD(
		VALGRIND_MAKE_MEM_DEFINED(cache_mutex, sizeof(Mutex)));
	VALGRIND_DISCARD(
		VALGRIND_MAKE_MEM_DEFINED(default_stats_group, sizeof(MemoryStats)));
	VALGRIND_DISCARD(
		VALGRIND_MAKE_MEM_DEFINED(defaultMemoryManager, sizeof(MemPool)));
#endif

	if (defaultMemoryManager)
	{
		//defaultMemoryManager->~MemoryPool();
		MemPool::cleanupDefaultPool();
		defaultMemoryManager = NULL;
	}

	if (default_stats_group)
	{
		default_stats_group->~MemoryStats();
		default_stats_group = NULL;
	}

	if (cache_mutex)
	{
		cache_mutex->~Mutex();
		cache_mutex = NULL;
	}
}


MemPool::MemPool(MemoryStats& s, ExtentsCache* cache)
	: pool_destroying(false),
	  parent_redirect(false),
	  stats(&s),
	  parent(NULL),
	  extentsCache(cache)
{
	fb_assert(offsetof(MemBlock, body) == MEM_ALIGN(offsetof(MemBlock, body)));
	initialize();
}

MemPool::MemPool(MemPool& p, MemoryStats& s, ExtentsCache* cache)
	: pool_destroying(false),
	  parent_redirect(true),
	  stats(&s),
	  parent(&p),
	  extentsCache(cache)
{
	initialize();
}

void MemPool::initialize()
{
	blocksAllocated = 0;
	blocksActive = 0;

#ifdef USE_VALGRIND
	delayedFreeCount = 0;
	delayedFreePos = 0;

	VALGRIND_CREATE_MEMPOOL(this, VALGRIND_REDZONE, 0);
#endif

	bigHunks = NULL;
	pool_destroying = false;

#ifdef MEM_DEBUG
	next = child = NULL;

	if (parent)
	{
		MutexLockGuard linkGuard(parent->mutex, FB_FUNCTION);
		next = parent->child;
		parent->child = this;
	}
#endif
}

MemPool::~MemPool(void)
{
	pool_destroying = true;

	decrement_usage(used_memory.value());
	decrement_mapping(mapped_memory.value());

#ifdef USE_VALGRIND
	VALGRIND_DESTROY_MEMPOOL(this);

	// Do not forget to discard stack traces for delayed free blocks
	for (size_t i = 0; i < delayedFreeCount; i++)
	{
		MemBlock* block = delayedFree[i];
		void* object = &block->body;

		VALGRIND_DISCARD(
            VALGRIND_MAKE_MEM_DEFINED(block, offsetof(MemBlock, body)));
		VALGRIND_DISCARD(
            VALGRIND_MAKE_WRITABLE(object, block->getSize()));
	}
#endif

	// release big objects
	while (bigHunks)
	{
		MemBigHunk* hunk = bigHunks;
		bigHunks = hunk->next;
		releaseRaw(pool_destroying, hunk, hunk->length, extentsCache);
	}

	if (parent)
	{
		// release blocks redirected to parent
#ifdef VALIDATE_POOL
		MutexLockGuard guard(parent->mutex, FB_FUNCTION);
#endif
		while (parentRedirected.getCount())
		{
			MemBlock* block = parentRedirected.pop();
			block->resetRedirect(parent);
			parent->releaseBlock(block, false);
		}
	}

#ifdef MEM_DEBUG
	if (parent)
	{
		MutexLockGuard unlinkGuard(parent->mutex, FB_FUNCTION);
		bool flag = false;
		for (MemPool** pp = &(parent->child); *pp; pp = &((*pp)->next))
		{
			if (*pp == this)
			{
				*pp = (*pp)->next;
				flag = true;
				break;
			}
		}
		fb_assert(flag);
	}
#endif
}

template <class Extent>
void MemPool::newExtent(size_t& size, Extent** linkedList)
{
	// No large enough block found. We need to extend the pool
	void* memory = NULL;
	const unsigned TOTAL_OVERHEAD = DoubleLinkedList::MEM_OVERHEAD + GUARD_BYTES + VALGRIND_REDZONE;
	const unsigned FROM_LIMIT = mediumLimits[10];	// 4224 // 10
	const unsigned TO_LIMIT = mediumLimits[15];		// 7552 // 15

	size_t ext_size = size + MEM_ALIGN(sizeof(Extent));
	const bool allocByParent = parent && (ext_size <= TO_LIMIT);

	if (allocByParent)
	{
		size_t from = FROM_LIMIT;
		if (ext_size + TOTAL_OVERHEAD > from)
			from = ext_size + TOTAL_OVERHEAD;
		ext_size = TO_LIMIT;
		if (ext_size < from)
			ext_size = from;

		fb_assert(ext_size < DEFAULT_ALLOCATION);
		memory = parent->getExtent(from, ext_size);
	}
	else
	{
		fb_assert(ext_size <= DEFAULT_ALLOCATION);
		ext_size = DEFAULT_ALLOCATION;
		memory = allocRaw(ext_size);
		fb_assert(ext_size == DEFAULT_ALLOCATION); // Make sure extent size is as expected
	}

	Extent* extent = new(memory) Extent(linkedList, ext_size);
	size = extent->spaceRemaining;
}

MemoryPool* MemoryPool::createPool(MemoryPool* parentPool, MemoryStats& stats)
{
	if (!parentPool)
		parentPool = getDefaultMemoryPool();

	MemPool* p = FB_NEW_POOL(*parentPool) MemPool(*(parentPool->pool), stats, &defaultExtentsCache);
	return FB_NEW_POOL(*parentPool) MemoryPool(p);
}

void MemPool::setStatsGroup(MemoryStats& newStats) noexcept
{
	MutexLockGuard guard(mutex, "MemPool::setStatsGroup");

	const size_t sav_used_memory = used_memory.value();
	const size_t sav_mapped_memory = mapped_memory.value();

	stats->decrement_mapping(sav_mapped_memory);
	stats->decrement_usage(sav_used_memory);

	this->stats = &newStats;

	stats->increment_mapping(sav_mapped_memory);
	stats->increment_usage(sav_used_memory);
}

MemoryStats& MemoryPool::getStatsGroup() noexcept
{
	return pool->getStatsGroup();
}

void MemoryPool::setStatsGroup(MemoryStats& newStats) noexcept
{
	pool->setStatsGroup(newStats);
}

MemBlock* MemPool::alloc(size_t from, size_t& length, bool flagRedirect)
{
	MutexEnsureUnlock guard(mutex, "MemPool::alloc");
	guard.enter();

	// If this is a small block, look for it there

	MemBlock* block = smallObjects.allocateBlock(this, from, length);
	if (block)
		return block;

	if (parent_redirect && flagRedirect && length < PARENT_REDIRECT_THRESHOLD)
	{
		guard.leave();
		block = parent->alloc(from, length, false);
		guard.enter();

		if (block)
		{
			if (parent_redirect)	// someone else redirected block in this pool?
			{
				block->setRedirect();

				parentRedirected.push(block);
				if (parentRedirected.getCount() == parentRedirected.getCapacity())
					parent_redirect = false;

				return block;
			}
			else					// worst case - very low possibility
			{
				guard.leave();
				parent->releaseBlock(block, false);
				guard.enter();
			}
		}
	}

	block = mediumObjects.allocateBlock(this, from, length);
	if (block)
		return block;

	/*
	 *  OK, we've got a "big block" on hands.  To maximize confusing, the indicated
	 *  "length" of a free big block is the length of MemHeader plus body*/

	fb_assert(from == 0);
	size_t hunkLength = MemBigHunk::hdrSize() + offsetof(MemBlock, body) + length;

	// Allocate the new hunk

	MemBigHunk* hunk = new(allocRaw(hunkLength)) MemBigHunk(&bigHunks, hunkLength);
	return hunk->block;
}

MemBlock* MemPool::allocate2(size_t from, size_t& size
#ifdef DEBUG_GDS_ALLOC
	, const char* fileName, int line
#endif
)
{
	size_t length = from ? size : ROUNDUP(size + VALGRIND_REDZONE, roundingSize) + GUARD_BYTES;
	MemBlock* memory = alloc(from, length, true);
	size = length - (VALGRIND_REDZONE + GUARD_BYTES);

#ifdef USE_VALGRIND
	VALGRIND_MEMPOOL_ALLOC(this, &memory->body, size);
#endif

	memory->pool = this;

#ifdef DEBUG_GDS_ALLOC
	memory->fileName = fileName;
	memory->lineNumber = line;
#endif

#ifdef MEM_DEBUG
	memset(&memory->body, INIT_BYTE, size);
	memset(&memory->body + size, GUARD_BYTE, memory->getSize() - offsetof(MemBlock,body) - size);
#endif

	++blocksAllocated;
	++blocksActive;

	fb_assert((U_IPTR)(&memory->body) % ALLOC_ALIGNMENT == 0);
	return memory;
}


void* MemPool::allocate(size_t size ALLOC_PARAMS)
{
#ifdef VALIDATE_POOL
	MutexLockGuard guard(mutex, "MemPool::allocate");
	Validator vld(this);
#endif

	MemBlock* memory = allocate2(0, size ALLOC_PASS_ARGS);

	increment_usage(memory->getSize());

	return &memory->body;
}


void MemPool::releaseMemory(void* object, bool flagExtent) noexcept
{
	if (object)
	{
		MemBlock* block = (MemBlock*) ((UCHAR*) object - offsetof(MemBlock, body));
		MemPool* pool = block->pool;

#ifdef VALIDATE_POOL
		MutexLockGuard guard(pool->mutex, "MemPool::releaseMemory");
#endif
		if (flagExtent)
			block->resetExtent();

#ifdef USE_VALGRIND
		// Synchronize delayed free queue using pool mutex
		MutexLockGuard guard(pool->mutex, "MemPool::deallocate USE_VALGRIND");

		// Notify Valgrind that block is freed from the pool
		VALGRIND_MEMPOOL_FREE(pool, object);

		// block is placed in delayed buffer - mark as NOACCESS for that time
		VALGRIND_DISCARD(VALGRIND_MAKE_NOACCESS(block, offsetof(MemBlock, body)));

		// Extend circular buffer if possible
		if (pool->delayedFreeCount < FB_NELEM(pool->delayedFree))
		{
			pool->delayedFree[pool->delayedFreeCount] = block;
			pool->delayedFreeCount++;
			return;
		}

		// Shift circular buffer pushing out oldest item
		MemBlock* requested_block = block;

		block = pool->delayedFree[pool->delayedFreePos];
		object = &block->body;

		// Re-enable access to MemBlock
		VALGRIND_DISCARD(VALGRIND_MAKE_MEM_DEFINED(block, offsetof(MemBlock, body)));

		// Remove protection from memory block
#ifdef VALGRIND_FIX_IT
		VALGRIND_DISCARD(
			VALGRIND_MAKE_MEM_DEFINED(object, block->getSize() - VALGRIND_REDZONE));
#else
		VALGRIND_DISCARD(
			VALGRIND_MAKE_WRITABLE(object, block->getSize() - VALGRIND_REDZONE));
#endif

		// Replace element in circular buffer
		pool->delayedFree[pool->delayedFreePos] = requested_block;

		// Move queue pointer to next element and cycle if needed
		if (++(pool->delayedFreePos) >= FB_NELEM(pool->delayedFree))
			pool->delayedFreePos = 0;
#endif

#ifdef DEBUG_GDS_ALLOC
		block->fileName = NULL;
#endif
		pool->releaseBlock(block, !flagExtent);
	}
}

void MemPool::releaseBlock(MemBlock* block, bool decrUsage) noexcept
{
	if (block->pool != this)
		corrupt("bad block released");

#ifdef MEM_DEBUG
	for (const UCHAR* end = (UCHAR*) block + block->getSize(), *p = end - GUARD_BYTES; p < end;)
	{
		if (*p++ != GUARD_BYTE)
			corrupt("guard bytes overwritten");
	}
#endif

	--blocksActive;
	const size_t length = block->getSize();

	MutexEnsureUnlock guard(mutex, "MemPool::releaseBlock");
	guard.enter();

	Validator vld(decrUsage ? this : NULL);

	if (decrUsage)
		decrement_usage(length);

	// If length is less than threshold, this is a small block
	if (smallObjects.deallocateBlock(block))
		return;

	// Redirected to parent block?
	if (block->redirected())
	{
		FB_SIZE_T pos;
		if (parentRedirected.find(block, pos))
			parentRedirected.remove(pos);
		guard.leave();

#ifdef VALIDATE_POOL
		MutexLockGuard guard(parent->mutex, "MemPool::releaseBlock /parent");
#endif
		block->resetRedirect(parent);
		parent->releaseBlock(block, false);
		return;
	}

	// Medium block - with another threshold
	if (mediumObjects.deallocateBlock(block))
		return;

	// This must be BIG block
	block->assertBig();

#ifdef MEM_DEBUG
	memset(&block->body, DEL_BYTE, length - offsetof(MemBlock, body));
#endif

	MemBigHunk* hunk = (MemBigHunk*)(((UCHAR*)block) - MemBigHunk::hdrSize());
	SemiDoubleLink::remove(hunk);
	decrement_mapping(FB_ALIGN(hunk->length, get_map_page_size()));
	releaseRaw(pool_destroying, hunk, hunk->length, nullptr);
}

void MemPool::memoryIsExhausted(void)
{
	Firebird::BadAlloc::raise();
}

void* MemPool::allocRaw(size_t size)
{
#ifndef USE_VALGRIND
	if (size == DEFAULT_ALLOCATION)
	{
		MutexLockGuard guard(cache_mutex, "MemPool::allocRaw");
		if (extentsCache->count)
		{
			// Use most recently used object to encourage caching
			increment_mapping(size);
			return extentsCache->data[--extentsCache->count];
		}
	}
#endif

	size = FB_ALIGN(size, get_map_page_size());

#ifdef WIN_NT

	void* result = VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
	if (!result)
	{

#else // WIN_NT

	void* result = NULL;
	if (failedList)
	{
		MutexLockGuard guard(cache_mutex, "MemPool::allocRaw");
		for (FailedBlock* fb = failedList; fb; fb = fb->next)
		{
			if (fb->blockSize == size)
			{
				result = fb;
				SemiDoubleLink::pop(fb);
				break;
			}
		}
	}

	if (!result)
	{

#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef MAP_ANONYMOUS

		result = os_utils::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

#else // MAP_ANONYMOUS

		if (dev_zero_fd < 0)
			dev_zero_fd = os_utils::open("/dev/zero", O_RDWR);
		result = os_utils::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, dev_zero_fd, 0);

#endif // MAP_ANONYMOUS

		if (result == MAP_FAILED)

#endif // WIN_NT

		{
			// failure happens!
			memoryIsExhausted();
			return NULL;
		}
	}

#ifdef USE_VALGRIND
	// Let Valgrind forget that block was zero-initialized
	VALGRIND_DISCARD(VALGRIND_MAKE_WRITABLE(result, size));
#endif

	increment_mapping(size);
	return result;
}


void* MemPool::getExtent(size_t from, size_t& to)		// pass desired minimum size, return actual extent size
{
#ifdef VALIDATE_POOL
	MutexLockGuard guard(mutex, "MemPool::getExtent");
#endif
	MemBlock* extent = allocate2(from, to ALLOC_ARGS);
	extent->setExtent();
	return &extent->body;
}


void MemPool::releaseExtent(bool destroying, void* block, size_t size, MemPool* pool) noexcept
{
	if (size < DEFAULT_ALLOCATION)
		releaseMemory(block, true);
	else
	{
		if (pool)
			pool->decrement_mapping(size);
		releaseRaw(true, block, size, (pool ? pool->extentsCache : nullptr));
	}
}


void MemPool::releaseRaw(bool destroying, void* block, size_t size, ExtentsCache* extentsCache) noexcept
{
#ifndef USE_VALGRIND
	if (extentsCache && (size == DEFAULT_ALLOCATION))
	{
		MutexLockGuard guard(cache_mutex, "MemPool::releaseRaw");
		if (extentsCache->count < MAP_CACHE_SIZE)
		{
			extentsCache->data[extentsCache->count++] = block;
			return;
		}
	}
#else
	// Set access protection for block to prevent memory from deleted pool being accessed
	int handle = /* //VALGRIND_MAKE_NOACCESS */ VALGRIND_MAKE_MEM_DEFINED(block, size);

	size = FB_ALIGN(size, get_map_page_size());

	void* unmapBlockPtr = block;
	size_t unmapBlockSize = size;

	// Employ extents delayed free logic only when pool is destroying.
	// In normal case all blocks pass through queue of sufficent length by themselves
	if (destroying)
	{
		// Synchronize delayed free queue using extents mutex
		MutexLockGuard guard(cache_mutex, "MemPool::releaseRaw");

		// Extend circular buffer if possible
		if (delayedExtentCount < FB_NELEM(delayedExtents))
		{
			DelayedExtent* item = &delayedExtents[delayedExtentCount];
			item->memory = block;
			item->size = size;
			item->handle = handle;
			delayedExtentCount++;
			return;
		}

		DelayedExtent* item = &delayedExtents[delayedExtentsPos];

		// Free message associated with old extent in Valgrind
		VALGRIND_DISCARD(item->handle);

		// Set up the block we are going to unmap
		unmapBlockPtr = item->memory;
		unmapBlockSize = item->size;

		// Replace element in circular buffer
		item->memory = block;
		item->handle = handle;
		item->size = size;

		// Move queue pointer to next element and cycle if needed
		delayedExtentsPos++;
		if (delayedExtentsPos >= FB_NELEM(delayedExtents))
			delayedExtentsPos = 0;
	}
	else
	{
		// Let Valgrind forget about unmapped block
		VALGRIND_DISCARD(handle);
	}
#endif

	size = FB_ALIGN(size, get_map_page_size());
#ifdef WIN_NT
	if (!VirtualFree(block, 0, MEM_RELEASE))
	{
#else // WIN_NT

#if (defined SOLARIS) && (defined HAVE_CADDR_T)
	int rc = munmap((caddr_t) block, size);
#else
	int rc = munmap(block, size);
#endif
	if (rc)
	{
		if (errno == ENOMEM)
		{
			FailedBlock* failed = (FailedBlock*) block;
			failed->blockSize = size;

			MutexLockGuard guard(cache_mutex, "MemPool::releaseRaw");
			SemiDoubleLink::push(&failedList, failed);

			return;
		}
#endif // WIN_NT
		corrupt("OS memory deallocation error");
	}
}

void MemPool::globalFree(void* block) noexcept
{
	deallocate(block);
}

void* MemoryPool::calloc(size_t size ALLOC_PARAMS)
{
	void* block = allocate(size ALLOC_PASS_ARGS);
	memset(block, 0, size);
	return block;
}

void MemPool::deallocate(void* block) noexcept
{
	releaseMemory(block, false);
}

void MemPool::deletePool(MemPool* pool)
{
	delete pool;
}

bool MemPool::validate(char* buf, FB_SIZE_T size)
{
	StatInt vMap = 0, vUse = 0;

	smallObjects.validate(this, vMap, vUse);
	mediumObjects.validate(this, vMap, vUse);

	// validate big objects
	for (MemBigHunk* h = bigHunks; h; h = h->next)
		h->validate(this, vMap, vUse);

	// validate blocks redirected to parent
	for (FB_SIZE_T n = 0; n < parentRedirected.getCount(); ++n)
	{
		MemBlock* b = parentRedirected[n];
		if (!b->isExtent())
			vUse += parentRedirected[n]->getSize();
	}

	if (vMap != mapped_memory.value() || vUse != used_memory.value())
	{
		char buf[256];
		fb_utils::snprintf(buf, sizeof(buf), "Memory statistics does not match pool: "
			"mapped=%" SQUADFORMAT "(%" SQUADFORMAT " st), used=%" SQUADFORMAT "(%" SQUADFORMAT " st)",
			SINT64(vMap), SINT64(mapped_memory.value()), SINT64(vUse), SINT64(used_memory.value()));
		return false;
	}

	return true;
}

#ifdef MEM_DEBUG
void MemPool::print_contents(const char* filename, unsigned flags, const char* filter_path) noexcept
{
	FILE* out = os_utils::fopen(filename, "w");
	if (!out)
		return;

	print_contents(out, flags, filter_path);
	fclose(out);
}

// This member function can't be const because there are calls to the mutex.
void MemPool::print_contents(FILE* file, unsigned flags, const char* filter_path) noexcept
{
	bool used_only = flags & MemoryPool::PRINT_USED_ONLY;

	MutexLockGuard guard(mutex, "MemPool::print_contents");

	fprintf(file, "********* Printing contents of pool %p (parent %p) used=%" SQUADFORMAT " mapped=%" SQUADFORMAT "\n",
		this, parent, SINT64(used_memory.value()), SINT64(mapped_memory.value()));

	char buf[256];
	if (!validate(buf, sizeof(buf)))
	{
		fprintf(file, "%s\n", buf);
	}

	if (!used_only)
	{
		filter_path = NULL;
	}
	const size_t filter_len = filter_path ? strlen(filter_path) : 0;

	// small & medium hunks
	smallObjects.print_contents(file, this, used_only, filter_path, filter_len);
	mediumObjects.print_contents(file, this, used_only, filter_path, filter_len);

	// big hunks
	for (MemBigHunk* hunk = bigHunks; hunk; hunk = hunk->next)
		hunk->print_contents(file, this, used_only, filter_path, filter_len);

	if (flags & MemoryPool::PRINT_RECURSIVE)
	{
		for (MemPool* p = child; p; p = p->next)
			p->print_contents(file, flags, filter_path);
	}
}
#endif

// Declare thread-specific variable for context memory pool
#ifndef TLS_CLASS
TLS_DECLARE(MemoryPool*, contextPool);
#else
TLS_DECLARE(MemoryPool*, *contextPoolPtr);
#endif	// TLS_CLASS

MemoryPool* MemoryPool::setContextPool(MemoryPool* newPool)
{
#ifndef TLS_CLASS
	MemoryPool* const old = TLS_GET(contextPool);
	TLS_SET(contextPool, newPool);
#else
	MemoryPool* const old = TLS_GET(*contextPoolPtr);
	TLS_SET(*contextPoolPtr, newPool);
#endif	// TLS_CLASS
	return old;
}

MemoryPool* MemoryPool::getContextPool()
{
#ifndef TLS_CLASS
	return TLS_GET(contextPool);
#else
	return TLS_GET(*contextPoolPtr);
#endif	// TLS_CLASS
}

void MemoryPool::contextPoolInit()
{
#ifdef TLS_CLASS
	// Allocate TLS entry for context pool
	contextPoolPtr = FB_NEW_POOL(*getDefaultMemoryPool()) TLS_CLASS<MemoryPool*>;
	// To be deleted by InstanceControl::InstanceList::destructors() at TLS priority
#endif	// TLS_CLASS
}

MemoryPool& AutoStorage::getAutoMemoryPool()
{
	MemoryPool* p = MemoryPool::getContextPool();
	if (!p)
	{
		p = getDefaultMemoryPool();
		fb_assert(p);
	}

	return *p;
}

void* MemoryPool::allocate(size_t size ALLOC_PARAMS)
{
	return pool->allocate(size ALLOC_PASS_ARGS);
}

void MemoryPool::deallocate(void* block) noexcept
{
	pool->deallocate(block);
}

void MemoryPool::deletePool(MemoryPool* pool)
{
	while (pool->finalizers)
	{
		auto finalizer = pool->finalizers;
		fb_assert(!finalizer->prev);

		pool->finalizers = finalizer->next;

		if (pool->finalizers)
		{
			fb_assert(pool->finalizers->prev == finalizer);
			pool->finalizers->prev = nullptr;
		}

		finalizer->next = nullptr;
		finalizer->finalize();
	}

	MemPool::deletePool(pool->pool);
	pool->pool = NULL;
	delete pool;
}

void MemoryPool::print_contents(FILE* file, unsigned flags, const char* filter_path) noexcept
{
#ifdef MEM_DEBUG
	pool->print_contents(file, flags, filter_path);
#endif
}

void MemoryPool::print_contents(const char* filename, unsigned flags, const char* filter_path) noexcept
{
#ifdef MEM_DEBUG
	pool->print_contents(filename, flags, filter_path);
#endif
}

void MemoryPool::internalRegisterFinalizer(Finalizer* finalizer)
{
	fb_assert(finalizer);

	MutexLockGuard guard(pool->mutex, "MemoryPool::internalRegisterFinalizer");

	finalizer->prev = nullptr;
	finalizer->next = finalizers;

	if (finalizers)
	{
		fb_assert(!finalizers->prev);
		finalizers->prev = finalizer;
	}

	finalizers = finalizer;
}

void MemoryPool::unregisterFinalizer(Finalizer*& finalizer)
{
	{	// scope
		MutexLockGuard guard(pool->mutex, "MemoryPool::unregisterFinalizer");

		if (finalizer->prev)
		{
			fb_assert(finalizer->prev->next == finalizer);
			finalizer->prev->next = finalizer->next;
		}
		else
			finalizers = finalizer->next;

		if (finalizer->next)
			finalizer->next->prev = finalizer->prev;
	}

	delete finalizer;
	finalizer = nullptr;
}


class ExternalMemoryHandler
{
friend class MemoryPool;

private:
	struct Objects
	{
		MemoryStats memoryStats;
		MemPool memPool{memoryStats, &externalExtentsCache};
		MemoryPool memoryPool{&memPool};
	};

	static ExternalMemoryHandler* instance;

public:
	enum class State : UCHAR
	{
		ALIVE,
		DEAD,
		DYING
	};

public:
	ExternalMemoryHandler()
	{
		Mutex::initMutexes();

#ifdef MEM_DEBUG_EXTERNAL
		printf("ExternalMemoryHandler::ExternalMemoryHandler()\n");
#endif

		instance = this;

		new(objectsBuffer) Objects();

		MemoryPool::externalMemoryManager = &objects().memoryPool;

		atexit([] {
			const auto currentUsage = instance->objects().memoryStats.getCurrentUsage();

#ifdef MEM_DEBUG_EXTERNAL
			printf("ExternalMemoryHandler atexit: %" SIZEFORMAT "\n", currentUsage);
#endif

			ExternalMemoryHandler::printContents("atexit");

			if (currentUsage == 0)
				ExternalMemoryHandler::free();
			else
				instance->state = State::DYING;
		});
	}

	void revive()
	{
#ifdef MEM_DEBUG_EXTERNAL
		printf("ExternalMemoryHandler::revive()\n");
#endif
		new(this) ExternalMemoryHandler();
	}

	static void printContents(const char* moment)
	{
#if defined(MEM_DEBUG) && defined(DEBUG_GDS_ALLOC)
		if (!MemoryPool::externalMemoryManager)
			return;

		static bool alreadyPrinted = false;
		Firebird::AutoPtr<FILE> file;

		{	// scope
			char name[PATH_MAX];

			if (os_utils::getCurrentModulePath(name, sizeof(name)))
				strncat(name, ".memdebug.external.log", sizeof(name) - 1);
			else
				strcpy(name, "memdebug.external.log");

			file = os_utils::fopen(name, alreadyPrinted ? "at" : "w+t");
		}

		if (file)
		{
			fprintf(file, "********* Moment: %s\n", moment);

			MemoryPool::externalMemoryManager->print_contents(file,
				Firebird::MemoryPool::PRINT_USED_ONLY | Firebird::MemoryPool::PRINT_RECURSIVE);
			file = NULL;
			alreadyPrinted = true;
		}
#endif
	}

	static void free()
	{
		if (instance->state != State::DEAD)
		{
			instance->state = State::DEAD;
			instance->objects().~Objects();
			instance->~ExternalMemoryHandler();
			instance = nullptr;

			MemPool::cleanupExternalPool();
		}

		MemoryPool::externalMemoryManager = nullptr;
	}

	inline Objects& objects()
	{
		return *(Objects*) objectsBuffer;
	}

	alignas(alignof(Objects)) char objectsBuffer[sizeof(Objects)];
	State state = State::ALIVE;
};

ExternalMemoryHandler* ExternalMemoryHandler::instance = nullptr;

void initExternalMemoryPool()
{
	static ExternalMemoryHandler handler;

	if (handler.state == ExternalMemoryHandler::State::DEAD)
		handler.revive();
}


void MemoryPool::globalFree(void* block) noexcept
{
	auto pool = MemPool::getPoolFromPointer(block);

	MemPool::globalFree(block);

	if (auto externalMemoryHandler = ExternalMemoryHandler::instance;
		externalMemoryHandler &&
		externalMemoryHandler->state == ExternalMemoryHandler::State::DYING &&
		pool == &externalMemoryHandler->objects().memPool)
	{
		const auto currentUsage = externalMemoryHandler->objects().memoryStats.getCurrentUsage();

#ifdef MEM_DEBUG_EXTERNAL
		printf("MemoryPool::globalFree() - dying ExternalMemoryHandler: %" SIZEFORMAT "\n", currentUsage);
#endif

		ExternalMemoryHandler::printContents("globalFree");

		if (currentUsage == 0)
			ExternalMemoryHandler::free();
	}
}


#if defined(DEV_BUILD)
void AutoStorage::ProbeStack() const
{
	//
	// AutoStorage() default constructor can be used only
	// for objects on the stack. ProbeStack() uses the
	// following assumptions to check it:
	//	1. One and only one stack is used for all kind of variables.
	//	2. Objects don't grow > 128K.
	//
	char probeVar = '\0';
	const char* myStack = &probeVar;
	const char* thisLocation = (const char*) this;
	ptrdiff_t distance = thisLocation - myStack;
	fb_assert(absVal(distance) < 128 * 1024);
}
#endif

} // namespace Firebird


// These operators are needed for foreign libraries which use redefined new/delete.
// Global operator "delete" is always redefined by firebird,
// in a case when we actually need "new" only with file/line information
// this version should be also present as a pair for "delete".

void* operator new(size_t s)
{
	return getExternalMemoryPool()->allocate(s ALLOC_ARGS);
}

void* operator new[](size_t s)
{
	return getExternalMemoryPool()->allocate(s ALLOC_ARGS);
}

void operator delete(void* mem) noexcept
{
	MemoryPool::globalFree(mem);
}

void operator delete[](void* mem) noexcept
{
	MemoryPool::globalFree(mem);
}
