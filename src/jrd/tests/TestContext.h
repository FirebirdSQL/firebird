#ifndef TEST_CONTEXT_H
#define TEST_CONTEXT_H

#include "firebird.h"
#include "../jrd/jrd.h"

class TestContextHolder
{
public:
	TestContextHolder();
	~TestContextHolder();

private:
	Jrd::EngineContextHolder m_tdbb; // must be at stack

public: // Available in tests
	Firebird::MemoryPool& pool;
	Jrd::thread_db* tdbb{};
};

using EngineHolder = TestContextHolder;

#endif // TEST_CONTEXT_H
