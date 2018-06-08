/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		WipeFile.cpp
 *	DESCRIPTION:	Wipe and delete file
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
 *  The Original Code was created by Dmitry Starodubov
 *   <dmitry.starodubov (at) red-soft.biz> for Red Soft Corporation.
 *
 *  Copyright (c) 2010 Red Soft Corporation <info (at) red-soft.biz>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "WipeFile.h"
#include <time.h>
#include "../../common/config/config.h"

using namespace Firebird;

#ifdef WIN_NT
#include <windows.h>

#define SLASH '\\'
#ifndef MAXPATHLEN
#define MAXPATHLEN MAX_PATH
#endif

#else // WIN_NT
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#define SLASH '/'
#define MAXPATHLEN PATH_MAX
// default sector size
#define SECTOR_SIZE 512
// default buffer size -- 4MB
#define CHUNK_SIZE (4 << 20)
#ifdef O_SYNC
#define SYNC		O_SYNC
#endif
#if (!(defined SYNC) && (defined O_FSYNC))
#define SYNC		O_FSYNC
#endif
#ifdef O_DSYNC
#undef SYNC
#define SYNC		O_DSYNC
#endif
#ifndef SYNC
#define SYNC		0
#endif
#endif // WIN_NT

void FillBuffer(UCHAR **buffer, size_t bufSize){
	srand((unsigned)time(NULL));

	for (size_t i = 0; i < 3; i++) {
#ifdef WIN_NT
		buffer[i] = (UCHAR*) VirtualAlloc(NULL, bufSize, MEM_COMMIT, PAGE_READWRITE);
#else
		buffer[i] = new UCHAR[bufSize];
#endif
		switch (i) {
		case 0:
#ifndef WIN_NT
			memset(buffer[i], 0, bufSize);
#endif
			break;
		case 1:
			memset(buffer[i], 0xFF, bufSize);
			break;
		case 2:
			for (size_t j = 0; j < bufSize; j++)
				buffer[i][j] = (UCHAR) rand();
			break;
		}
	}
}


void FreeBuffer(UCHAR** buffer){
	for (size_t i = 0; i < 3; i++)
#ifdef WIN_NT
		VirtualFree(buffer[i], 0, MEM_RELEASE);
#else
		delete[] buffer[i];
#endif
}


#ifdef WIN_NT

bool SecureOverwrite(HANDLE fileHandle, ULONGLONG length)
{
#define CLEANBUFSIZE 65536
	UCHAR*		cleanBuffer[3];
	int			i, passes;
	ULONGLONG	totalWritten;
	ULONG		bytesWritten, bytesToWrite;
	LONG		seeklength;
	bool		status;

	FillBuffer(cleanBuffer, CLEANBUFSIZE);

	// Do the overwrite
	seeklength = (LONG) length;
	for (passes = 0; passes < MemoryPool::wipePasses; passes++) {
		if (passes != 0) {
			SetFilePointer(fileHandle, -seeklength, NULL, FILE_CURRENT);
		}
		for (i = 0; i < 3; i++) {

			if (i != 0) {
				SetFilePointer(fileHandle, -seeklength, NULL, FILE_CURRENT);
			}

			totalWritten = 0;
			while (totalWritten < length) {
				if (length - totalWritten > 1024*1024) {
					bytesToWrite = 1024*1024;
				}
				else {
					bytesToWrite = (ULONG) (length - totalWritten);
				}
				if (bytesToWrite > CLEANBUFSIZE)
					bytesToWrite = CLEANBUFSIZE;

				status = WriteFile(fileHandle, cleanBuffer[i], bytesToWrite, &bytesWritten, NULL);
				if (!status) {
					FreeBuffer(cleanBuffer);
					return false;
				}

				totalWritten += bytesWritten;
			}
		}
	}

	FreeBuffer(cleanBuffer);

	return true;
}


int WipeFile(HANDLE handle) {
	ULONGLONG bytesToWrite, bytesWritten;
	ULARGE_INTEGER fileLength;
	DWORD dwSizeHigh = 0;
	DWORD dwSizeLow = 0;
	DWORD fileAttrs;
	BY_HANDLE_FILE_INFORMATION fileInfo;

	if (handle == INVALID_HANDLE_VALUE)
		fatal_exception::raise("Invalid handle value in WipeFile");

	if (!GetFileInformationByHandle(handle, &fileInfo))
		fatal_exception::raiseFmt("IO error (%d) retrieving info for file.", GetLastError());

	fileAttrs = fileInfo.dwFileAttributes;
	if (fileAttrs == INVALID_FILE_ATTRIBUTES)
		fatal_exception::raiseFmt("IO error (%d) retrieving attributes for file.", GetLastError());

	if (fileAttrs & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_SPARSE_FILE))
		fatal_exception::raise("Compressed temp files can not be wiped.");

	dwSizeLow = GetFileSize(handle, &dwSizeHigh);

	if (dwSizeLow || dwSizeHigh) {

		dwSizeLow--;
		if (dwSizeLow == (DWORD) -1 && dwSizeHigh)
			dwSizeHigh--;
		SetFilePointer(handle, dwSizeLow, (PLONG) &dwSizeHigh, FILE_BEGIN);

		// Write one zero byte, which causes the file system to fill the entire
		// file's on-disk contents with 0.
		if (!SecureOverwrite(handle, 1)) {
			CloseHandle(handle);
			fatal_exception::raiseFmt("IO error (%d) overwriting file.", GetLastError());
		}

		SetFilePointer(handle, 0, NULL, FILE_BEGIN);
		fileLength.LowPart = dwSizeLow;
		fileLength.HighPart = dwSizeHigh;
		bytesWritten = 0;
		while (bytesWritten < fileLength.QuadPart) {
			int differ = fileLength.QuadPart - bytesWritten;
			bytesToWrite = differ < 65536 ? differ : 65536;
			if (!SecureOverwrite(handle, (DWORD) bytesToWrite)) {
				CloseHandle(handle);
				fatal_exception::raiseFmt("IO error (%d) overwriting file.", GetLastError());
			}
			bytesWritten += bytesToWrite;
		}
	}

	return 0;
}

#else


void SyncFile(const int fd) {
#ifndef SYNC
#ifdef HAVE_FDATASYNC
	fdatasync(fd);
#elif defined(HAVE_FSYNC)
	fsync(fd);
#endif
#endif
}


void DoWrite(const int fd, UCHAR *buf, size_t count) {
	UCHAR* ptr = buf;
	ssize_t c, written = 0;
	while (written < count)
	{
		c = write(fd, ptr, count - written);

		if (c > 0)
		{
			written += c;
			ptr += c;
		}
		else
		{
			fatal_exception::raiseFmt("IO error (%d) writing file.", errno);
		}
	}
}


int WipeFile(int handle) {
	UCHAR*	buf[3];
	size_t	bufSize;
	FB_UINT64	fSize, loop, loopr;
	struct stat fileStat;
	size_t	passes;

	if (handle < 0)
		fatal_exception::raise("Invalid handle value in WipeFile");

	if (!fstat(handle, &fileStat))
		fSize = fileStat.st_size;
	else
		fatal_exception::raiseFmt("IO error (%d) file stat.", errno);

	bufSize = CHUNK_SIZE;

	if (fSize % fileStat.st_blksize)
		fSize += fileStat.st_blksize - (fSize % fileStat.st_blksize);
	else
		++fSize;

	if (fSize > CHUNK_SIZE)
	{
		loop = fSize / CHUNK_SIZE;
		loopr = fSize % CHUNK_SIZE;
	}
	else
	{
		loop = 1;
		loopr = 0;
		if (fSize)
			bufSize = fSize;
	}

	try	{
		FillBuffer(buf, bufSize);

		if (handle < 0)
			fatal_exception::raiseFmt("Error (%d) opening file.", errno);

		for (passes = 0; passes < MemoryPool::wipePasses; passes++) {
			for (size_t j = 0; j < 3; j++) {
				if (lseek(handle, 0, SEEK_SET) == (off_t) -1)
					fatal_exception::raiseFmt("IO error (%d) seeking file.", errno);
				size_t i = 0;
				while (i++ < loop)
					DoWrite(handle, buf[j], bufSize);
				if (loopr)
					DoWrite(handle, buf[j], loopr);
				SyncFile(handle);
			}
		}
	}
	catch (const Firebird::Exception&) {
		FreeBuffer(buf);
		throw;
	}
	FreeBuffer(buf);

	return 0;
}
#endif // WIN_NT
