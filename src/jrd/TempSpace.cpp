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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2006 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"

#include "iberror.h"
#include "../common/classes/TempFile.h"
#include "../common/config/config.h"
#include "../common/config/dir_list.h"
#include "../common/gdsassert.h"
#include "../common/isc_proto.h"
#include "../common/os/path_utils.h"
#include "../jrd/jrd.h"

#include "../jrd/TempSpace.h"

using namespace Firebird;
using namespace Jrd;

// Static definitions/initializations

GlobalPtr<Mutex> TempSpace::initMutex;
TempDirectoryList* TempSpace::tempDirs = NULL;
FB_SIZE_T TempSpace::minBlockSize = 0;

namespace
{
	const size_t MIN_TEMP_BLOCK_SIZE = 64 * 1024;

	class TempCacheLimitGuard
	{
	public:
		explicit TempCacheLimitGuard(Database* dbb) :
			m_dbb(dbb),
			m_size(0)
		{}

		~TempCacheLimitGuard()
		{
			if (m_size)
				m_dbb->decTempCacheUsage(m_size);
		}

		bool reserve(FB_SIZE_T size)
		{
			if (m_dbb->incTempCacheUsage(size))
			{
				m_size = size;
				return true;
			}
			return false;
		}

		void commit()
		{
			m_size = 0;
		}

	private:
		Database* const m_dbb;
		FB_SIZE_T m_size;
	};
}

//
// In-memory block class
//

FB_SIZE_T TempSpace::MemoryBlock::read(offset_t offset, void* buffer, FB_SIZE_T length)
{
	if (offset + length > size)
	{
		length = size - offset;
	}
	memcpy(buffer, ptr + offset, length);
	return length;
}

FB_SIZE_T TempSpace::MemoryBlock::write(offset_t offset, const void* buffer, FB_SIZE_T length)
{
	if (offset + length > size)
	{
		length = size - offset;
	}
	memcpy(ptr + offset, buffer, length);
	return length;
}

//
// On-disk block class
//

FB_SIZE_T TempSpace::FileBlock::read(offset_t offset, void* buffer, FB_SIZE_T length)
{
	if (offset + length > size)
	{
		length = size - offset;
	}
	offset += seek;
	return file->read(offset, buffer, length);
}

FB_SIZE_T TempSpace::FileBlock::write(offset_t offset, const void* buffer, FB_SIZE_T length)
{
	if (offset + length > size)
	{
		length = size - offset;
	}
	offset += seek;
	return file->write(offset, buffer, length);
}

//
// TempSpace::TempSpace
//
// Constructor
//

TempSpace::TempSpace(MemoryPool& p, const PathName& prefix, bool dynamic)
		: pool(p), filePrefix(p, prefix),
		  logicalSize(0), physicalSize(0), localCacheUsage(0),
		  head(NULL), tail(NULL), tempFiles(p),
		  initialBuffer(p), initiallyDynamic(dynamic),
		  freeSegments(p)
{
	if (!tempDirs)
	{
		MutexLockGuard guard(initMutex, FB_FUNCTION);
		if (!tempDirs)
		{
			MemoryPool& def_pool = *getDefaultMemoryPool();
			tempDirs = FB_NEW_POOL(def_pool) TempDirectoryList(def_pool);
			minBlockSize = Config::getTempBlockSize();

			if (minBlockSize < MIN_TEMP_BLOCK_SIZE)
				minBlockSize = MIN_TEMP_BLOCK_SIZE;
			else
				minBlockSize = FB_ALIGN(minBlockSize, MIN_TEMP_BLOCK_SIZE);
		}
	}
}

//
// TempSpace::~TempSpace
//
// Destructor
//

TempSpace::~TempSpace()
{
	while (head)
	{
		Block* temp = head->next;
		delete head;
		head = temp;
	}

	if (localCacheUsage)
	{
		Database* const dbb = GET_DBB();
		dbb->decTempCacheUsage(localCacheUsage);
	}

	while (tempFiles.getCount())
		delete tempFiles.pop();
}

//
// TempSpace::read
//
// Reads bytes from the temporary space
//

FB_SIZE_T TempSpace::read(offset_t offset, void* buffer, FB_SIZE_T length)
{
	fb_assert(offset + length <= logicalSize);

	if (length)
	{
		// search for the first needed block
		Block* block = findBlock(offset);

		UCHAR* p = static_cast<UCHAR*>(buffer);
		FB_SIZE_T l = length;

		// read data from the block chain
		for (Block* itr = block; itr && l; itr = itr->next, offset = 0)
		{
			const FB_SIZE_T n = itr->read(offset, p, l);
			p += n;
			l -= n;
		}

		fb_assert(!l);
	}

	return length;
}

//
// TempSpace::write
//
// Writes bytes to the temporary space
//

FB_SIZE_T TempSpace::write(offset_t offset, const void* buffer, FB_SIZE_T length)
{
	fb_assert(offset <= logicalSize);

	if (offset + length > logicalSize)
	{
		// not enough space, allocate one more block
		extend(offset + length - logicalSize);
	}

	if (length)
	{
		// search for the first needed block
		Block* const block = findBlock(offset);

		const UCHAR* p = static_cast<const UCHAR*>(buffer);
		FB_SIZE_T l = length;

		// write data to as many blocks as necessary
		for (Block* itr = block; itr && l; itr = itr->next, offset = 0)
		{
			const FB_SIZE_T n = itr->write(offset, p, l);
			p += n;
			l -= n;
		}

		fb_assert(!l);
	}

	return length;
}

//
// TempSpace::extend
//
// Increases size of the temporary space
//

void TempSpace::extend(FB_SIZE_T size)
{
	logicalSize += size;

	if (logicalSize > physicalSize)
	{
		const FB_SIZE_T initialSize = initialBuffer.getCount();

		// If the dynamic mode is specified, then we allocate new blocks
		// by growing the same initial memory block in the specified chunks.
		// Once the limit (64KB) is reached, we switch to the generic algorithm
		// (1MB blocks), copy the existing data there and free the initial buffer.
		//
		// This mode should not be used if the caller never works with small blocks.
		// Also, it MUST NOT be used if the caller deals with inMemory() or allocateBatch()
		// routines and caches the pointers to use them later. These pointers may become
		// invalid after resizing the initial block or after switching to large blocks.

		if (initiallyDynamic && logicalSize < MIN_TEMP_BLOCK_SIZE)
		{
			// allocate or extend the initial dynamic block, it will grow up to 64KB
			if (!initialSize)
			{
				fb_assert(!head && !tail);
				head = tail = FB_NEW_POOL(pool) InitialBlock(initialBuffer.getBuffer(size), size);
			}
			else
			{
				fb_assert(head == tail);
				size += initialSize;
				initialBuffer.resize(size);
				new(head) InitialBlock(initialBuffer.begin(), size);
			}

			physicalSize = size;
			return;
		}

		if (initialSize)
		{
			fb_assert(head == tail);
			delete head;
			head = tail = NULL;
			size = static_cast<FB_SIZE_T>(FB_ALIGN(logicalSize, minBlockSize));
			physicalSize = size;
		}
		else
		{
			size = static_cast<FB_SIZE_T>(FB_ALIGN(logicalSize - physicalSize, minBlockSize));
			physicalSize += size;
		}

		Block* block = NULL;

		{	// scope
			TempCacheLimitGuard guard(GET_DBB());

			if (guard.reserve(size))
			{
				try
				{
					// allocate block in virtual memory
					block = FB_NEW_POOL(pool) MemoryBlock(FB_NEW_POOL(pool) UCHAR[size], tail, size);
					localCacheUsage += size;
					guard.commit();
				}
				catch (const BadAlloc&)
				{
					// not enough memory
				}
			}
		}

		// NS 2014-07-31: FIXME: missing exception handling.
		// error thrown in block of code below will leave TempSpace in inconsistent state:
		// logical/physical size already increased while allocation has in fact failed.
		if (!block)
		{
			// allocate block in the temp file
			TempFile* const file = setupFile(size);
			fb_assert(file);
			if (tail && tail->sameFile(file))
			{
				fb_assert(!initialSize);
				tail->size += size;
				return;
			}
			block = FB_NEW_POOL(pool) FileBlock(file, tail, size);
		}

		// preserve the initial contents, if any
		if (initialSize)
		{
			block->write(0, initialBuffer.begin(), initialSize);
			initialBuffer.free();
		}

		// append new block to the chain
		if (!head)
		{
			head = block;
		}
		tail = block;
	}
}

//
// TempSpace::findBlock
//
// Locates the space block corresponding to the given global offset
//

TempSpace::Block* TempSpace::findBlock(offset_t& offset) const
{
	fb_assert(offset <= logicalSize);

	Block* block = NULL;

	if (offset < physicalSize / 2)
	{
		// walk forward
		block = head;
		while (block && offset >= block->size)
		{
			offset -= block->size;
			block = block->next;
		}
		fb_assert(block);
	}
	else
	{
		// walk backward
		block = tail;
		while (block && physicalSize - offset > block->size)
		{
			offset += block->size;
			block = block->prev;
		}
		fb_assert(block);
		offset -= physicalSize - block->size;
	}

	fb_assert(offset <= block->size);
	return block;
}

//
// TempSpace::setupFile
//
// Allocates the required space in some temporary file
//

TempFile* TempSpace::setupFile(FB_SIZE_T size)
{
	StaticStatusVector status_vector;

	for (FB_SIZE_T i = 0; i < tempDirs->getCount(); i++)
	{
		TempFile* file = NULL;

		PathName directory = (*tempDirs)[i];
		PathUtils::ensureSeparator(directory);

		for (FB_SIZE_T j = 0; j < tempFiles.getCount(); j++)
		{
			PathName dirname, filename;
			PathUtils::splitLastComponent(dirname, filename, tempFiles[j]->getName());
			PathUtils::ensureSeparator(dirname);
			if (!directory.compare(dirname))
			{
				file = tempFiles[j];
				break;
			}
		}

		try
		{
			if (!file)
			{
				file = FB_NEW_POOL(pool) TempFile(pool, filePrefix, directory);
				tempFiles.add(file);
			}

			file->extend(size);
		}
		catch (const system_error& ex)
		{
			ex.stuffException(status_vector);
			continue;
		}

		return file;
	}

	// no room in all directories
	Arg::Gds status(isc_out_of_temp_space);
	status.append(Arg::StatusVector(status_vector.begin()));
	iscLogStatus(NULL, status.value());
	status.raise();

	return NULL; // compiler silencer
}

//
// TempSpace::allocateSpace
//
// Allocate available space in free segments. Extend file if necessary
//

offset_t TempSpace::allocateSpace(FB_SIZE_T size)
{
	// Find the best available space. This is defined as the smallest free space
	// that is big enough. This preserves large blocks.
	Segment* best = NULL;

	// Search through the available space in the not used segments list
	for (bool found = freeSegments.getFirst(); found; found = freeSegments.getNext())
	{
		Segment* const space = &freeSegments.current();
		// If this is smaller than our previous best, use it
		if (space->size >= size && (!best || (space->size < best->size))) {
			best = space;
		}
	}

	// If we didn't find any space, allocate it at the end of the file
	if (!best)
	{
		extend(size);
		return getSize() - size;
	}

	// Set up the return parameters
	const offset_t position = best->position;
	best->size -= size;
	best->position += size;

	// If the hunk was an exact fit, remove the segment from the list
	if (!best->size)
	{
		if (!freeSegments.locate(best->position))
			fb_assert(false);

		freeSegments.fastRemove();
	}

	return position;
}

//
// TempSpace::releaseSpace
//
// Return previously allocated segment back into not used segments list and
// join it with adjacent segments if found
//

void TempSpace::releaseSpace(offset_t position, FB_SIZE_T size)
{
	fb_assert(size > 0);
	fb_assert(position < getSize());	// Block starts in file
	const offset_t end = position + size;
	fb_assert(end <= getSize());		// Block ends in file

	if (freeSegments.locate(locEqual, end))
	{
		// The next segment is found to be adjacent
		Segment* const next_seg = &freeSegments.current();
		next_seg->position -= size;
		next_seg->size += size;

		if (freeSegments.getPrev())
		{
			// Check the prior segment for being adjacent
			Segment* const prior_seg = &freeSegments.current();
			if (position == prior_seg->position + prior_seg->size)
			{
				next_seg->position -= prior_seg->size;
				next_seg->size += prior_seg->size;
				freeSegments.fastRemove();
			}
		}

		return;
	}

	if (freeSegments.locate(locLess, position))
	{
		// Check the prior segment for being adjacent
		Segment* const prior_seg = &freeSegments.current();
		if (position == prior_seg->position + prior_seg->size)
		{
			prior_seg->size += size;
			return;
		}
	}

	freeSegments.add(Segment(position, size));
}

//
// TempSpace::inMemory
//
// Return contiguous chunk of memory if present at given location
//

UCHAR* TempSpace::inMemory(offset_t begin, size_t size) const
{
	const Block* block = findBlock(begin);
	return block ? block->inMemory(begin, size) : NULL;
}

//
// TempSpace::findMemory
//
// Return contiguous chunk of memory and adjust starting offset
// of search range if found
//

UCHAR* TempSpace::findMemory(offset_t& begin, offset_t end, size_t size) const
{
	offset_t local_offset = begin;
	const offset_t save_begin = begin;
	const Block* block = findBlock(local_offset);

	while (block && (begin + size <= end))
	{
		UCHAR* const mem = block->inMemory(local_offset, size);
		if (mem)
		{
			return mem;
		}

		begin += block->size - local_offset;
		local_offset = 0;
		block = block->next;
	}

	begin = save_begin;
	return NULL;
}

//
// TempSpace::validate
//
// Validate internal lists for consistency and return back to caller
// amount of available free space
//

bool TempSpace::validate(offset_t& free) const
{
	free = 0;
	FreeSegmentTree::ConstAccessor accessor(&freeSegments);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext())
	{
		const offset_t size = accessor.current().size;
		fb_assert(size != 0);
		free += size;
	}

	offset_t disk = 0;
	for (FB_SIZE_T i = 0; i < tempFiles.getCount(); i++)
		disk += tempFiles[i]->getSize();

	return ((initialBuffer.getCount() + localCacheUsage + disk) == physicalSize);
}


//
// TempSpace::allocateBatch
//
// Allocate up to 'count' contiguous chunks of memory available in free
// segments if any. Adjust size of chunks between minSize and maxSize
// accordingly to available free space (assuming all of the free space
// is in memory blocks). Algorithm is very simple and can be improved in future
//

ULONG TempSpace::allocateBatch(ULONG count, FB_SIZE_T minSize, FB_SIZE_T maxSize, Segments& segments)
{
	// adjust passed chunk size to amount of free memory we have and number
	// of runs still not allocated.
	offset_t freeMem = 0;

	for (bool found = freeSegments.getFirst(); found; found = freeSegments.getNext())
		freeMem += freeSegments.current().size;

	freeMem = MIN(freeMem / count, maxSize);
	freeMem = MAX(freeMem, minSize);
	freeMem = MIN(freeMem, minBlockSize);
	freeMem &= ~(FB_ALIGNMENT - 1);

	bool is_positioned = freeSegments.getFirst();
	while (segments.getCount() < count && is_positioned)
	{
		Segment* freeSpace = &freeSegments.current();
		offset_t freeSeek = freeSpace->position;
		const offset_t freeEnd = freeSpace->position + freeSpace->size;

		UCHAR* const mem = findMemory(freeSeek, freeEnd, freeMem);

		if (mem)
		{
			fb_assert(freeSeek + freeMem <= freeEnd);
#ifdef DEV_BUILD
			offset_t seek1 = freeSeek;
			UCHAR* const p = findMemory(seek1, freeEnd, freeMem);
			fb_assert(p == mem);
			fb_assert(seek1 == freeSeek);
#endif
			if (freeSeek != freeSpace->position)
			{
				const offset_t skip_size = freeSeek - freeSpace->position;
				const Segment skip_space(freeSpace->position, skip_size);

				freeSpace->position += skip_size;
				freeSpace->size -= skip_size;
				fb_assert(freeSpace->size != 0);

				if (!freeSegments.add(skip_space))
					fb_assert(false);

				if (!freeSegments.locate(skip_space.position + skip_size))
					fb_assert(false);

				freeSpace = &freeSegments.current();
			}

			SegmentInMemory seg;
			seg.memory = mem;
			seg.position = freeSeek;
			seg.size = freeMem;
			segments.add(seg);

			freeSpace->position += freeMem;
			freeSpace->size -= freeMem;

			if (!freeSpace->size)
			{
				is_positioned = freeSegments.fastRemove();
			}
		}
		else
		{
			is_positioned = freeSegments.getNext();
		}
	}

	return segments.getCount();
}
