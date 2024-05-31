#ifndef COMMON_CVT_FORMAT_H
#define COMMON_CVT_FORMAT_H

#include "firebird.h"
#include "../common/cvt.h"

Firebird::string CVT_format_datetime_to_string(const dsc* desc, const Firebird::string& format, Firebird::Callbacks* cb);
ISC_TIMESTAMP_TZ CVT_format_string_to_datetime(const dsc* desc, const Firebird::string& format,
	const Firebird::EXPECT_DATETIME expectedType, Firebird::Callbacks* cb);

#endif // COMMON_CVT_FORMAT_H
