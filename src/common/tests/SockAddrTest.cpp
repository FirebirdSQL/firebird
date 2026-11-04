#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../remote/SockAddr.h"

BOOST_AUTO_TEST_SUITE(CommonSuite)
BOOST_AUTO_TEST_SUITE(SockAddrTests)

BOOST_AUTO_TEST_CASE(IPv6FamilyConversion)
{
	const USHORT families[] = {
		AF_INET6_POSIX, AF_INET6_WINDOWS, AF_INET6_OPENBSD,
		AF_INET6_FREEBSD, AF_INET6_DARWIN
	};

	for (const auto family : families)
	{
		sockaddr_in6 raw = {};
		raw.sin6_family = family;
		raw.sin6_port = htons(3050);
		raw.sin6_addr.s6_addr[15] = 1;

		const SockAddr address(reinterpret_cast<const UCHAR*>(&raw), sizeof(raw));
		BOOST_TEST(address.family() == AF_INET6);
		BOOST_TEST(address.length() == sizeof(raw));
		BOOST_TEST(address.port() == 3050);
		BOOST_TEST(memcmp(&address.ptrIn6()->sin6_addr, &raw.sin6_addr,
			sizeof(raw.sin6_addr)) == 0);
	}
}

BOOST_AUTO_TEST_CASE(IPv4FamilyUnchanged)
{
	sockaddr_in raw = {};
	raw.sin_family = AF_INET;
	raw.sin_port = htons(3050);
	raw.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	const SockAddr address(reinterpret_cast<const UCHAR*>(&raw), sizeof(raw));
	BOOST_TEST(address.family() == AF_INET);
	BOOST_TEST(address.length() == sizeof(raw));
	BOOST_TEST(address.port() == 3050);
	BOOST_TEST(address.ptrIn()->sin_addr.s_addr == raw.sin_addr.s_addr);
}

BOOST_AUTO_TEST_CASE(AuxiliaryPortLayouts)
{
	const UCHAR addresses[][4] = {
		{10, 0, 0x0b, 0xea},	// little-endian POSIX family
		{0, 10, 0x0b, 0xea},	// big-endian POSIX family
		{28, 24, 0x0b, 0xea}	// BSD length and family
	};
	for (const auto& address : addresses)
	{
		unsigned short port = 0;
		BOOST_TEST(SockAddr::getPort(address, sizeof(address), port));
		BOOST_TEST(port == 3050);
	}
}

BOOST_AUTO_TEST_CASE(TruncatedAuxiliaryPort)
{
	const UCHAR address[] = {28, 24, 0x0b, 0xea};
	for (unsigned length = 0; length < sizeof(address); ++length)
	{
		unsigned short port = 123;
		BOOST_TEST(!SockAddr::getPort(address, length, port));
		BOOST_TEST(port == 123);
	}
}

BOOST_AUTO_TEST_SUITE_END()	// SockAddrTests
BOOST_AUTO_TEST_SUITE_END()	// CommonSuite
