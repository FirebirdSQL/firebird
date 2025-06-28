#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../dsql/DsqlCompilerScratch.h"

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(BlrGenSuite)


BOOST_AUTO_TEST_SUITE(DsqlCompilerScratchTests)

// HELPERS
// --------

DsqlCompilerScratch* makeScratch()
{
	auto& pool = *getDefaultMemoryPool();
	return FB_NEW DsqlCompilerScratch(pool, nullptr, nullptr);
}

const std::string_view typeOfTableObject = "TABLE OBJECT A";
const std::string_view typeOfTablePackage = "TABLE PACKAGE A";
const std::string_view typeOfTableSchema = "TABLE SCHEMA A";\

const std::string_view typeOfNameObject = "NAME OBJECT A";
const std::string_view typeOfNamePackage = "NAME PACKAGE A";
const std::string_view typeOfNameSchema = "NAME SCHEMA A";

const std::string_view collate = "COLLATE A";

Jrd::dsql_fld genDefaultFiled(MemoryPool& pool)
{
	Jrd::dsql_fld field(pool);

	field.dtype = dtype_text;
	field.typeOfTable.object = typeOfTableObject.data();
	field.typeOfTable.package = typeOfTablePackage.data();
	field.typeOfTable.schema = typeOfTableSchema.data();

	field.typeOfName.object = typeOfNameObject.data();
	field.typeOfName.package = typeOfNamePackage.data();
	field.typeOfName.schema = typeOfNameSchema.data();

	field.fullDomain = true;
	field.textType = ttype_utf8;
	field.notNull = false;
	field.explicitCollation = true;
	field.collate.object = collate.data();
	return field;
}


void genDomainFiled(DsqlCompilerScratch& expected, UCHAR domainBlr)
{
	expected.appendUChar(blr_column_name3);
	expected.appendUChar(domainBlr);
	expected.appendMetaString(typeOfTableSchema.data());
	expected.appendMetaString(typeOfTableObject.data());
	expected.appendMetaString(typeOfNameObject.data());
	expected.appendUChar(1);
	expected.appendUShort(ttype_utf8);
}


// TESTS
// --------

BOOST_AUTO_TEST_CASE(TestDefaultTextField)
{
	ThreadContextHolder context;
	auto& pool = *getDefaultMemoryPool();
	context->setDatabase(Database::create(nullptr, false));

	Jrd::TypeClause field = genDefaultFiled(pool);

	DsqlCompilerScratch& scratch = *makeScratch();
	scratch.putType(&field, true);

	DsqlCompilerScratch& expected = *makeScratch();
	genDomainFiled(expected, blr_domain_full);

	BOOST_CHECK_EQUAL_COLLECTIONS(
		scratch.getBlrData().begin(), scratch.getBlrData().end(),
		expected.getBlrData().begin(), expected.getBlrData().end()
	);
}


BOOST_AUTO_TEST_CASE(TestDefaultTextFieldSameSchema)
{
	ThreadContextHolder context;
	auto& pool = *getDefaultMemoryPool();
	context->setDatabase(Database::create(nullptr, false));

	Jrd::TypeClause field = genDefaultFiled(pool);

	DsqlCompilerScratch& scratch = *makeScratch();
	scratch.ddlSchema = field.typeOfTable.schema;
	scratch.putType(&field, true);

	DsqlCompilerScratch& expected = *makeScratch();
	expected.appendUChar(blr_column_name2);
	expected.appendUChar(blr_domain_full);
	expected.appendMetaString(field.typeOfTable.object.c_str());
	expected.appendMetaString(field.typeOfName.object.c_str());
	expected.appendUShort(field.textType);

	BOOST_CHECK_EQUAL_COLLECTIONS(
		scratch.getBlrData().begin(), scratch.getBlrData().end(),
		expected.getBlrData().begin(), expected.getBlrData().end()
	);
}


BOOST_AUTO_TEST_CASE(TestFalseDomain)
{
	ThreadContextHolder context;
	auto& pool = *getDefaultMemoryPool();
	context->setDatabase(Database::create(nullptr, false));

	// Test domain false with different schema
	{
		Jrd::TypeClause field = genDefaultFiled(pool);
		field.fullDomain = false;

		DsqlCompilerScratch& scratch = *makeScratch();
		scratch.putType(&field, true);

		DsqlCompilerScratch& expected = *makeScratch();
		genDomainFiled(expected, blr_domain_type_of);

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}

	// Test domain false with same schema
	{
		Jrd::TypeClause field = genDefaultFiled(pool);
		field.fullDomain = false;

		DsqlCompilerScratch& scratch = *makeScratch();
		scratch.putType(&field, true);

		DsqlCompilerScratch& expected = *makeScratch();
		expected.appendUChar(blr_column_name3);
		expected.appendUChar(blr_domain_type_of);
		expected.appendMetaString(field.typeOfTable.schema.c_str());
		expected.appendMetaString(field.typeOfTable.object.c_str());
		expected.appendMetaString(field.typeOfName.object.c_str());
		expected.appendUChar(1);
		expected.appendUShort(field.textType);

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}
}


BOOST_AUTO_TEST_CASE(TestUseSubType)
{
	ThreadContextHolder context;
	auto& pool = *getDefaultMemoryPool();
	context->setDatabase(Database::create(nullptr, false));

	// Test subType true
	{
		DsqlCompilerScratch& scratch = *makeScratch();
		Jrd::TypeClause field = genDefaultFiled(pool);
		field.typeOfName.clear();
		scratch.putType(&field, true);

		DsqlCompilerScratch& expected = *makeScratch();
		expected.appendUChar(blr_text2);
		expected.appendUShort(field.textType);
		expected.appendUShort(field.length);

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}

	{
		DsqlCompilerScratch& scratch = *makeScratch();
		Jrd::TypeClause field = genDefaultFiled(pool);
		field.typeOfName.clear();
		scratch.putType(&field, false);

		DsqlCompilerScratch& expected = *makeScratch();
		expected.appendUChar(blr_text);
		expected.appendUShort(field.length);

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}
}


BOOST_AUTO_TEST_CASE(TestFalseExplicitCollation)
{
	ThreadContextHolder context;
	auto& pool = *getDefaultMemoryPool();
	context->setDatabase(Database::create(nullptr, false));

	{ // Different shame

		DsqlCompilerScratch& scratch = *makeScratch();

		Jrd::dsql_fld field = genDefaultFiled(pool);
		field.explicitCollation = false;
		scratch.putType(&field, true);

		DsqlCompilerScratch& expected = *makeScratch();
		expected.appendUChar(blr_column_name3);
		expected.appendUChar(blr_domain_full);
		expected.appendMetaString(field.typeOfTable.schema.c_str());
		expected.appendMetaString(field.typeOfTable.object.c_str());
		expected.appendMetaString(field.typeOfName.object.c_str());
		expected.appendUChar(0);

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}

	{ // Same shame
		DsqlCompilerScratch& scratch = *makeScratch();

		Jrd::dsql_fld field = genDefaultFiled(pool);
		field.explicitCollation = false;
		scratch.ddlSchema = field.typeOfTable.schema;

		scratch.putType(&field, true);

		DsqlCompilerScratch& expected = *makeScratch();
		expected.appendUChar(blr_column_name);
		expected.appendUChar(blr_domain_full);
		expected.appendMetaString(field.typeOfTable.object.c_str());
		expected.appendMetaString(field.typeOfName.object.c_str());

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}

}


BOOST_AUTO_TEST_CASE(TestPutTypeEmptyCollate)
{
	ThreadContextHolder context;
	auto& pool = *getDefaultMemoryPool();
	context->setDatabase(Database::create(nullptr, false));
	DsqlCompilerScratch& scratch = *makeScratch();

	Jrd::TypeClause field = genDefaultFiled(pool);
	field.collate.clear();
	scratch.putType(&field, true);

	DsqlCompilerScratch& expected = *makeScratch();
	expected.appendUChar(blr_column_name3);
	expected.appendUChar(blr_domain_full);
	expected.appendMetaString(field.typeOfTable.schema.c_str());
	expected.appendMetaString(field.typeOfTable.object.c_str());
	expected.appendMetaString(field.typeOfName.object.c_str());
	expected.appendUChar(0);

	BOOST_CHECK_EQUAL_COLLECTIONS(
		scratch.getBlrData().begin(), scratch.getBlrData().end(),
		expected.getBlrData().begin(), expected.getBlrData().end()
	);
}


BOOST_AUTO_TEST_CASE(TestEmptyTypeOfTable)
{
	ThreadContextHolder context;
	auto& pool = *getDefaultMemoryPool();
	context->setDatabase(Database::create(nullptr, false));

	{ // Different schema
		DsqlCompilerScratch& scratch = *makeScratch();
		Jrd::TypeClause field = genDefaultFiled(pool);
		field.typeOfTable.clear();
		scratch.putType(&field, true);

		DsqlCompilerScratch& expected = *makeScratch();
		expected.appendUChar(blr_domain_name3);
		expected.appendUChar(blr_domain_full);
		expected.appendMetaString(field.typeOfName.schema.c_str());
		expected.appendMetaString(field.typeOfName.object.c_str());
		expected.appendUChar(1);
		expected.appendUShort(field.textType);

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}


	{ // Same schema
		DsqlCompilerScratch& scratch = *makeScratch();
		Jrd::TypeClause field = genDefaultFiled(pool);
		field.typeOfTable.clear();

		scratch.ddlSchema = field.typeOfName.schema;
		scratch.putType(&field, true);

		DsqlCompilerScratch& expected = *makeScratch();
		expected.appendUChar(blr_domain_name2);
		expected.appendUChar(blr_domain_full);
		expected.appendMetaString(field.typeOfName.object.c_str());
		expected.appendUShort(field.textType);

		BOOST_CHECK_EQUAL_COLLECTIONS(
			scratch.getBlrData().begin(), scratch.getBlrData().end(),
			expected.getBlrData().begin(), expected.getBlrData().end()
		);
	}
}

BOOST_AUTO_TEST_SUITE_END()	// DsqlCompilerScratchTests


BOOST_AUTO_TEST_SUITE_END()	// CompressorSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
