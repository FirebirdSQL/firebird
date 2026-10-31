#include "firebird.h"

#ifdef OPENBSD

#include "boost/test/unit_test.hpp"
#include "boost/preprocessor/stringize.hpp"
#include "../common/os/mod_loader.h"
#include <dlfcn.h>
#include <memory>
#include <unicode/uversion.h>

BOOST_AUTO_TEST_SUITE(OpenBsdModuleSuite)

BOOST_AUTO_TEST_CASE(UnversionedLibraryName)
{
	std::unique_ptr<ModuleLoader::Module> module(ModuleLoader::loadModule(nullptr, "libicuuc.so"));
	BOOST_REQUIRE(module);
	BOOST_TEST(module->findSymbol(nullptr, BOOST_STRINGIZE(u_getVersion)) != nullptr);
}

BOOST_AUTO_TEST_CASE(DependencySymbolRejected)
{
	// u_getVersion belongs to libicuuc, not its dependent libicui18n.
	std::unique_ptr<void, decltype(&dlclose)> handle(dlopen("libicui18n.so", RTLD_LAZY), dlclose);
	BOOST_REQUIRE(handle);
	BOOST_REQUIRE(dlsym(handle.get(), BOOST_STRINGIZE(u_getVersion)) != nullptr);

	std::unique_ptr<ModuleLoader::Module> module(ModuleLoader::loadModule(nullptr, "libicui18n.so"));
	BOOST_REQUIRE(module);
	BOOST_TEST(module->findSymbol(nullptr, BOOST_STRINGIZE(u_getVersion)) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()	// OpenBsdModuleSuite

#endif // OPENBSD
