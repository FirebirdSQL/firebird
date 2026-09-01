#ifndef COMMON_CVT_FORMAT_H
#define COMMON_CVT_FORMAT_H

#include "firebird.h"
#include "../common/cvt.h"

namespace Firebird
{
	string CVT_format_datetime_to_string(const dsc* desc, const string& format, Callbacks* cb);
	ISC_TIMESTAMP_TZ CVT_format_string_to_datetime(const dsc* desc, const string& format,
		const EXPECT_DATETIME expectedType, Callbacks* cb);
}	// namespace Firebird

#endif // COMMON_CVT_FORMAT_H
