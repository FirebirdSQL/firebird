#include "boost/test/unit_test.hpp"
#include "../../TestContext.h"

#include "../jrd/json/classes/JsonTypes.h"
#include "../jrd/json/classes/JsonScalar.h"
#include "../jrd/json/classes/JsonDatetime.h"
#include "../common/classes/BlrReader.h"
#include "../common/classes/BlrWriter.h"

#include <utility>

using namespace FBJSON;


BOOST_AUTO_TEST_SUITE(JsonSuite)
BOOST_AUTO_TEST_SUITE(JsonClassesTests)

BOOST_AUTO_TEST_SUITE(JsonScalarTests)


BOOST_AUTO_TEST_CASE(JsonScalarValues)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();

	{ // NULL CHECK
		JsonScalar scalar(pool);
		BOOST_CHECK(scalar.isNull());
		BOOST_CHECK(!scalar.isScalar());

		scalar.set(true);
		BOOST_CHECK(!scalar.isNull());
		BOOST_CHECK(scalar.isScalar());
	}

	{
		JsonScalar scalar(pool);
		scalar.setToNull();
		BOOST_CHECK(scalar.isNull());
		BOOST_CHECK(!scalar.isScalar());
	}

	{ // BOOL CHECK
		JsonScalar scalar(pool);
		scalar.set(true);
		BOOST_CHECK(scalar.getType() == FBJSON::ValueType::BOOL);
		BOOST_TEST(scalar.getValue().boolean == true);
		BOOST_TEST(scalar.isTrue());
		BOOST_CHECK(scalar.isScalar());
	}

	{ // BOOL CHECK
		JsonScalar scalar(pool);
		scalar.set(false);
		BOOST_CHECK(scalar.getType() == FBJSON::ValueType::BOOL);
		BOOST_TEST(scalar.getValue().boolean == false);
		BOOST_TEST(scalar.isTrue() == false);
		BOOST_CHECK(scalar.isScalar());
	}

	{ // INT CHECK
		JsonScalar scalar(pool);
		scalar.set(42);
		BOOST_CHECK(scalar.getType() == FBJSON::ValueType::INT);
		BOOST_TEST(scalar.getValue().integer == 42);
		BOOST_TEST(scalar.isNumber() == true);
		BOOST_TEST(scalar.getDouble() == 42);
		BOOST_CHECK(scalar.isScalar());
	}

	{ // DOUBLE CHECK
		JsonScalar scalar(pool);
		scalar.set(42.42);
		BOOST_CHECK(scalar.getType() == FBJSON::ValueType::DOUBLE);
		BOOST_TEST(scalar.getValue().doubleValue == 42.42);
		BOOST_TEST(scalar.isNumber() == true);
		BOOST_TEST(scalar.getDouble() == 42.42);
		BOOST_CHECK(scalar.isScalar());
	}

	{ // STRING CHECK
		const char* testValue = "raw char pointer";

		JsonScalar scalar(pool);
		scalar.set(testValue);
		BOOST_CHECK(scalar.getType() == FBJSON::ValueType::STRING);
		BOOST_TEST(std::string_view(*scalar.getValue().string) == std::string_view(testValue));
		BOOST_CHECK(scalar.isString());
		BOOST_CHECK(scalar.isScalar());
	}

	{
		std::string_view testValue = "string_view string";

		JsonScalar scalar(pool);
		scalar.set(testValue);
		BOOST_CHECK(scalar.getType() == FBJSON::ValueType::STRING);
		BOOST_TEST(std::string_view(*scalar.getValue().string) == testValue);
		BOOST_CHECK(scalar.isString());
		BOOST_CHECK(scalar.isScalar());

		// Reset the same value
		scalar.set(*scalar.getValue().string);
	}

	// DATETIME CHECK

	{
		JsonScalar scalar(pool);
		auto& dt = scalar.setToDatetime();
		BOOST_CHECK(scalar.getType() == FBJSON::ValueType::DATETIME);
		BOOST_CHECK(*scalar.getValue().datetime == dt);
		BOOST_CHECK(scalar.isScalar());
	}
}

BOOST_AUTO_TEST_CASE(GetDoubleMethod)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();

	JsonScalar scalar(pool);
	scalar.setToNull();
	BOOST_CHECK_THROW(scalar.getDouble(), json_skippable_exception);

	scalar.set(false);
	BOOST_CHECK_THROW(scalar.getDouble(), json_skippable_exception);

	scalar.set("123");
	BOOST_CHECK_THROW(scalar.getDouble(), json_skippable_exception);

	scalar.set(123);
	BOOST_TEST(scalar.getDouble() == 123);

	scalar.set(42.42);
	BOOST_TEST(scalar.getDouble() == 42.42);
}


BOOST_AUTO_TEST_CASE(StrMethod)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();

	JsonScalar scalar(pool);
	// scalar.setToNull();
	// BOOST_CHECK_THROW(scalar.str(), json_skippable_exception);

	scalar.set("123");
	BOOST_TEST(scalar.str() == scalar.getValue().string);

	BOOST_TEST(std::string_view(*scalar.str()) == "123");
}

BOOST_AUTO_TEST_CASE(GetStringViewMethod)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();
	JsonScalar scalar(pool);
	// scalar.setToNull();
	// BOOST_CHECK_THROW(scalar.getStringView(), json_skippable_exception);

	scalar.set("123");
	BOOST_TEST(scalar.getStringView() == std::string_view(*scalar.getValue().string));
	BOOST_TEST(scalar.getStringView() == "123");
}

BOOST_AUTO_TEST_CASE(FlagsMethods)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();
	JsonScalar scalar(pool);

	BOOST_CHECK(!scalar.hasFlag(JsonScalar::FLAG_HAS_QUOTES));
	scalar.addFlag(FBJSON::JsonScalar::FLAG_HAS_QUOTES);
	BOOST_CHECK(scalar.hasFlag(JsonScalar::FLAG_HAS_QUOTES));
}

class TestBlrBuffer : public Firebird::BlrWriter
{
public:
	TestBlrBuffer(MemoryPool& pool = *getDefaultMemoryPool()) : Firebird::BlrWriter(pool)
	{ }

	bool isVersion4() final
	{
		return true;
	}

	Firebird::BlrReader& getReader()
	{
		return m_reader = Firebird::BlrReader(getBlrData().begin(), getBlrData().getCount());
	}

private:
	Firebird::BlrReader m_reader;
};


static void writeReadBytes(JsonScalar& inOut)
{
	TestBlrBuffer buffer;
	inOut.writeScalarAsBytes(buffer);
	buffer.appendUChar(0); // dummy end

	inOut.setToNull();
	inOut.readScalarFromBytes(buffer.getReader());
}

BOOST_AUTO_TEST_CASE(ReadWritesScalarBytes)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();
	JsonScalar scalar(pool);

	scalar.setToNull();
	writeReadBytes(scalar);
	BOOST_CHECK(scalar.isNull());

	scalar.set(false);
	writeReadBytes(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::BOOL);
	BOOST_TEST(scalar.getValue().boolean == false);

	scalar.set(true);
	writeReadBytes(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::BOOL);
	BOOST_TEST(scalar.getValue().boolean == true);

	scalar.set(42);
	writeReadBytes(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::INT);
	BOOST_TEST(scalar.getValue().integer == 42);

	scalar.set(42.42);
	writeReadBytes(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::DOUBLE);
	BOOST_TEST(scalar.getValue().doubleValue == 42.42);

	scalar.set("12345");
	writeReadBytes(scalar);
	BOOST_CHECK(scalar.isString());
	BOOST_TEST(scalar.getStringView() == "12345");

	scalar.setToDatetime().set("2018-01-01");
	auto ts1 = scalar.getValue().datetime->getTS();
	writeReadBytes(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::DATETIME);
	BOOST_TEST(scalar.getValue().datetime->asString() == "2018-01-01");
	BOOST_TEST(scalar.getValue().datetime->getTypeName() == "date");

	auto ts2 = scalar.getValue().datetime->getTS();
	BOOST_CHECK(ts1.time_zone == ts2.time_zone);
	BOOST_CHECK(ts1.utc_timestamp.timestamp_date == ts2.utc_timestamp.timestamp_date);
	BOOST_CHECK(ts1.utc_timestamp.timestamp_time == ts2.utc_timestamp.timestamp_time);
}

static void writeReadBlr(JsonScalar& inOut)
{
	TestBlrBuffer buffer;
	inOut.writeScalarAsBytes(buffer);
	buffer.appendUChar(0); // To fix assert with seekForward

	inOut.setToNull();

	auto reader = buffer.getReader();
	inOut.readScalarFromBytes(reader);
}

BOOST_AUTO_TEST_CASE(ReadWritesScalarBlr)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();
	JsonScalar scalar(pool);

	scalar.setToNull();
	writeReadBlr(scalar);
	BOOST_CHECK(scalar.isNull());

	scalar.set(false);
	writeReadBlr(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::BOOL);
	BOOST_TEST(scalar.getValue().boolean == false);

	scalar.set(true);
	writeReadBlr(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::BOOL);
	BOOST_TEST(scalar.getValue().boolean == true);

	scalar.set(42);
	writeReadBlr(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::INT);
	BOOST_TEST(scalar.getValue().integer == 42);

	scalar.set(42.42);
	writeReadBlr(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::DOUBLE);
	BOOST_TEST(scalar.getValue().doubleValue == 42.42);

	scalar.set("12345");
	scalar.addFlag(JsonScalar::FLAG_HAS_QUOTES);
	writeReadBlr(scalar);
	BOOST_CHECK(scalar.isString());
	BOOST_CHECK(!scalar.hasFlag(JsonScalar::FLAG_HAS_QUOTES)); // Only runtime
	BOOST_TEST(scalar.getStringView() == "12345");

	scalar.setToDatetime().set("2018-01-01");
	auto ts1 = scalar.getValue().datetime->getTS();
	writeReadBlr(scalar);
	BOOST_CHECK(scalar.getType() == ValueType::DATETIME);
	BOOST_TEST(scalar.getValue().datetime->asString() == "2018-01-01");
	BOOST_TEST(scalar.getValue().datetime->getTypeName() == "date");

	auto ts2 = scalar.getValue().datetime->getTS();
	BOOST_CHECK(ts1.time_zone == ts2.time_zone);
	BOOST_CHECK(ts1.utc_timestamp.timestamp_date == ts2.utc_timestamp.timestamp_date);
	BOOST_CHECK(ts1.utc_timestamp.timestamp_time == ts2.utc_timestamp.timestamp_time);
}

BOOST_AUTO_TEST_CASE(ScalarToDsc)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();
	JsonScalar scalar(pool);
	Jrd::impure_value out;

	scalar.setToNull();
	out = scalar.makeScalarDsc();
	BOOST_CHECK(out.vlu_desc.isNull());

	scalar.set(false);
	out = scalar.makeScalarDsc();
	BOOST_CHECK(out.vlu_desc.isBoolean());
	BOOST_CHECK(reinterpret_cast<const bool&>(*out.vlu_desc.dsc_address) == false);

	scalar.set(true);
	out = scalar.makeScalarDsc();
	BOOST_CHECK(out.vlu_desc.isBoolean());
	BOOST_CHECK(reinterpret_cast<const bool&>(*out.vlu_desc.dsc_address) == true);

	scalar.set(42);
	out = scalar.makeScalarDsc();
	BOOST_CHECK(out.vlu_desc.dsc_dtype == dtype_int64);
	BOOST_CHECK(reinterpret_cast<const SINT64&>(*out.vlu_desc.dsc_address) == 42);

	scalar.set(42.42);
	out = scalar.makeScalarDsc();
	BOOST_CHECK(out.vlu_desc.dsc_dtype == dtype_double);
	BOOST_CHECK(reinterpret_cast<const double&>(*out.vlu_desc.dsc_address) == 42.42);

	scalar.set("12345");
	out = scalar.makeScalarDsc();
	BOOST_CHECK(out.vlu_desc.dsc_dtype == dtype_text);

	const std::string_view view(reinterpret_cast<const char*>(out.vlu_desc.dsc_address), out.vlu_desc.getStringLength());
	BOOST_CHECK(view == "12345");

	scalar.setToDatetime().set("2018-01-01");
	auto ts1 = scalar.getValue().datetime->getTS();
	out = scalar.makeScalarDsc();
	BOOST_CHECK(out.vlu_desc.dsc_dtype == dtype_timestamp_tz);

	auto ts2 = *reinterpret_cast<ISC_TIMESTAMP_TZ*>(out.vlu_desc.dsc_address);
	BOOST_CHECK(ts1.time_zone == ts2.time_zone);
	BOOST_CHECK(ts1.utc_timestamp.timestamp_date == ts2.utc_timestamp.timestamp_date);
	BOOST_CHECK(ts1.utc_timestamp.timestamp_time == ts2.utc_timestamp.timestamp_time);
}

BOOST_AUTO_TEST_CASE(StringViewDsc)
{
	Firebird::MemoryPool& pool = *getDefaultMemoryPool();

	Firebird::string string = "123";

	JsonScalar::StringView view(pool, &string);

	BOOST_REQUIRE((*view).isString());
	BOOST_REQUIRE((*view).getStringView() == "123");
}

BOOST_AUTO_TEST_SUITE_END()	// JsonScalarTests

BOOST_AUTO_TEST_SUITE_END()	// JsonClassesTests
BOOST_AUTO_TEST_SUITE_END()	// JsonSuite
