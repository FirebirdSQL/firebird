#ifndef JSON_TEST_UTIL_H
#define JSON_TEST_UTIL_H

#include "firebird.h"
#include "../common/status.h"
#include "../jrd/json/path/JsonPath.h"
#include "../jrd/json/path/JPathParser.h"
#include "../jrd/json/JsonRuntime.h"
#include "../jrd/json/JsonUtils.h"

#include "boost/test/unit_test.hpp"

#ifdef DEV_BUILD
#define SKIP_IN_CI
#else
#define SKIP_IN_CI return;
#endif

#define SKIP_TEST return;

namespace TestUtils {

template<class TStatus>
inline void reportError(const TStatus& status, const Firebird::string& debug)
{
	if (status->isDirty())
	{
		BOOST_TEST_INFO(debug.data());

		const ISC_STATUS* vector = status->getErrors();
		SCHAR s[BUFFER_LARGE];

		Firebird::string info;
		while (fb_interpret(s, sizeof(s), &vector))
		{
			info += s;
			info += "\n";
		}

		BOOST_TEST_INFO(info.data());
	}
}

inline FBJSON::JsonPathExpr* parsePath(const std::string_view path, const PassingKeys* keys, bool rethrow = false)
{
	try
	{
		FBJSON::PathParser parser(*getDefaultMemoryPool(), path);
		return parser.parse(keys);
	}
	catch (const Firebird::Exception& ex)
	{
		Firebird::FbLocalStatus status;
		ex.stuffException(&status);

		Firebird::string debug;
		debug.printf("The problem path is: %s", path.data());
		reportError(status, debug);

		if (rethrow)
			throw;

		return {};
	}
}

class PathWrapper
{
public:
	Firebird::AutoPtr<FBJSON::JsonPathExpr> jpath = nullptr;
	PassingKeys keys;

	PathWrapper()
	{ }

	PathWrapper(const std::string_view strPath)
	{
		parse(strPath);
	}

	PathWrapper(PathWrapper&& other)
	{
		jpath = other.jpath.release();
	}

	PathWrapper& operator=(PathWrapper&& other)
	{
		jpath = other.jpath.release();
		return *this;
	}


	void parse(const std::string_view strPath)
	{
		jpath = parsePath(strPath, &keys);
	}

	void parseNoCheck(const std::string_view strPath)
	{
		jpath = parsePath(strPath, &keys, true);
	}

	inline const FBJSON::JsonExprNode* getPathTailNodes(const std::string_view strPath)
	{
		jpath = parsePath(strPath, &keys);
		BOOST_REQUIRE(jpath != nullptr);
		BOOST_REQUIRE(jpath->getTail() != nullptr);

		return jpath->getTail();
	}

};

}

#endif // !JSON_TEST_UTIL_H
