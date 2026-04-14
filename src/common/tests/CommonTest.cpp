#include "firebird.h"
#include "firebird/Interface.h"
#include "../common/classes/init.h"
#include "../common/classes/TempFile.h"
#include "../common/config/config.h"
#include "../common/gdsassert.h"

#define BOOST_TEST_MODULE CommonTest
#include "boost/test/included/unit_test.hpp"


int ISC_EXPORT fb_shutdown(unsigned int, const int)
{
	fb_assert(false);
	return 0;
}

namespace Firebird
{
	namespace
	{
		struct Init
		{
			Init(auto&&)
			{
				Config::setRootDirectoryFromCommandLine(TempFile::getTempPath());
			}
		};

		GlobalPtr<Init> init;
	}

	IMaster* API_ROUTINE fb_get_master_interface()
	{
		fb_assert(false);
		return nullptr;
	}
}
