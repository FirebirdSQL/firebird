/*
 *	PROGRAM:	Common class definition
 *	MODULE:		zip.h
 *	DESCRIPTION:	ZIP compression library loader.
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2012, 2018 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_ZIP_H
#define COMMON_ZIP_H

#ifdef HAVE_ZLIB_H
#include <zlib.h>

#include "../common/classes/auto.h"
#include "../common/os/mod_loader.h"

namespace Firebird {
	class ZLib
	{
	public:
		explicit ZLib(Firebird::MemoryPool&);

		int ZEXPORT (*deflateInit_)(z_stream* strm, int level, const char *version, int stream_size);
		int ZEXPORT (*inflateInit_)(z_stream* strm, const char *version, int stream_size);
		int ZEXPORT (*deflate)(z_stream* strm, int flush);
		int ZEXPORT (*inflate)(z_stream* strm, int flush);
		void ZEXPORT (*deflateEnd)(z_stream* strm);
		void ZEXPORT (*inflateEnd)(z_stream* strm);

		operator bool() { return z.hasData(); }
		bool operator!() { return !z.hasData(); }

		static void* allocFunc(void*, uInt items, uInt size);
		static void freeFunc(void*, void* address);

		ISC_STATUS_ARRAY status;

	private:
		AutoPtr<ModuleLoader::Module> z;

		void symbols();
	};
}
#endif // HAVE_ZLIB_H

#endif // COMMON_ZIP_H
