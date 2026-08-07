#include "TestContext.h"
#include "../jrd/req.h"

#include "../jrd/trace/TraceManager.h"

#include "boost/test/unit_test.hpp"

#include <filesystem>

// It is to slow to create a new DB for each test, so create it once and keep in cache
class CachedAttach
{
public:
	CachedAttach(Firebird::MemoryPool& pool) :
		status(pool)
	{
		dbPath = std::filesystem::temp_directory_path() / "engine_utests.fdb";
		removeDb();
	}

	Jrd::JAttachment* getAttachment()
	{
		// Delay init
		if (att != nullptr)
			return att;

		att = prov.createDatabase(&status, dbPath.string().data(), 0, nullptr);
		BOOST_REQUIRE(att);

		tra = att->startTransaction(&status, 0, nullptr);
		BOOST_REQUIRE(tra);

		statement = att->prepare(&status, tra, 0, "select 1 from rdb$database", 3, 0);

		return att;
	}

	~CachedAttach()
	{
		att->release();
		Jrd::TraceManager::getStorage()->shutdown();
		removeDb();
	}

	void removeDb()
	{
		if (std::filesystem::exists(dbPath))
			std::filesystem::remove(dbPath);
	}

	Jrd::JProvider prov{nullptr};
	Firebird::FbLocalStatus status;


	std::filesystem::path dbPath;
	Jrd::JAttachment* att = nullptr;

	Jrd::JTransaction* tra = nullptr;
	Jrd::JStatement* statement = nullptr;
};
static Firebird::GlobalPtr<CachedAttach, Firebird::InstanceControl::PRIORITY_DELETE_FIRST> storage;


TestContextHolder::TestContextHolder() :
	m_tdbb(&storage->status, storage->getAttachment(), FB_FUNCTION),
	pool(*getDefaultMemoryPool())
{
	m_tdbb->setRequest(storage->statement->getHandle()->getRequest());
	m_tdbb->setTransaction(storage->tra->getHandle());

	ISC_TIMESTAMP ts = {};
	m_tdbb->getRequest()->setGmtTimeStamp(ts);

	tdbb = JRD_get_thread_data();
}

TestContextHolder::~TestContextHolder()
{
	m_tdbb->setRequest(nullptr);
	m_tdbb->setTransaction(nullptr);
}
