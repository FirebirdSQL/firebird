/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Michal Kubecek
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2014 Michal Kubecek <mike@mk-sys.cz>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef REMOTE_SOCKADDR_H
#define REMOTE_SOCKADDR_H

#include <string.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifndef WIN_NT
#include <netinet/in.h>
#else

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Wspiapi.h>

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif

#endif

#include "../remote/remote.h"

class SockAddr
{
private:
	struct SockAddrPrefixPosixWindows
	{
		uint16_t sa_family;
	};

	struct SockAddrPrefixMacOs
	{
		uint8_t sa_len;
		uint8_t sa_family;
	};

	union sa_data {
		struct sockaddr sock;
		struct sockaddr_in inet;
		struct sockaddr_in6 inet6;

		SockAddrPrefixPosixWindows posixWindowsPrefix;
		SockAddrPrefixMacOs macOsPrefix;
	} data;
	socklen_t len;
	static const unsigned MAX_LEN = sizeof(sa_data);

	void checkAndFixFamily();

public:
	void convertFromMacOsToPosixWindows();
	void convertFromPosixWindowsToMacOs();
	void clear();
	const SockAddr& operator = (const SockAddr& x);

	SockAddr() { clear(); }
	SockAddr(const SockAddr& x) { *this = x; }
	SockAddr(const unsigned char* p_data, unsigned p_len);
	~SockAddr() {}

	struct sockaddr* ptr() { return &data.sock; }
	const struct sockaddr* ptr() const { return &data.sock; }
	struct sockaddr_in* ptrIn() { return &data.inet; }
	const struct sockaddr_in* ptrIn() const { return &data.inet; }
	struct sockaddr_in6* ptrIn6() { return &data.inet6; }
	const struct sockaddr_in6* ptrIn6() const { return &data.inet6; }

	unsigned length() const { return len; }
	unsigned short family() const;
	unsigned short port() const;
	void setPort(unsigned short x);
	int connect(SOCKET s) const;
	int accept(SOCKET s);
	int getsockname(SOCKET s);
	int getpeername(SOCKET s);
	void unmapV4();
};

// Definitions below taken from sources (socket.h) on the correspondent operating systems.
// If something else arrives, it should be added here and into checkAndFixFamily() also.

#define AF_INET6_POSIX		10
#define AF_INET6_WINDOWS	23
#define AF_INET6_DARWIN		30

#if AF_INET6 == AF_INET6_POSIX
#elif AF_INET6 == AF_INET6_WINDOWS
#elif AF_INET6 == AF_INET6_DARWIN
#else
#error Unknown value of AF_INET6 !
#endif

inline void SockAddr::checkAndFixFamily()
{
	switch (data.sock.sa_family)
	{
	case AF_INET:
		fb_assert(len == sizeof(sockaddr_in));
		break;

	case AF_INET6_POSIX:
	case AF_INET6_WINDOWS:
	case AF_INET6_DARWIN:
		data.sock.sa_family = AF_INET6;
		fb_assert(len == sizeof(sockaddr_in6));
		break;

	default:
		fb_assert(false);
		break;
	}
}

inline void SockAddr::convertFromMacOsToPosixWindows()
{
	SockAddrPrefixMacOs macOsPrefix;
	macOsPrefix = data.macOsPrefix;

	data.posixWindowsPrefix.sa_family = macOsPrefix.sa_family;
}

inline void SockAddr::convertFromPosixWindowsToMacOs()
{
	SockAddrPrefixPosixWindows posixWindowsPrefix;
	posixWindowsPrefix = data.posixWindowsPrefix;

	data.macOsPrefix.sa_family = posixWindowsPrefix.sa_family;
	data.macOsPrefix.sa_len = length();
}


inline void SockAddr::clear()
{
	len = 0;
	memset(&data, 0, sizeof(data));
}


inline const SockAddr& SockAddr::operator = (const SockAddr& x)
{
	memcpy(&data, &x.data, MAX_LEN);
	len = x.len;

	checkAndFixFamily();
	return *this;
}


inline SockAddr::SockAddr(const unsigned char* p_data, unsigned p_len)
{
	if (p_len > MAX_LEN)
		p_len = MAX_LEN;
	memcpy(&data, p_data, p_len);
	len = p_len;

	checkAndFixFamily();
}


inline int SockAddr::connect(SOCKET s) const
{
	return ::connect(s, ptr(), len);
}


inline int SockAddr::accept(SOCKET s)
{
	return os_utils::accept(s, ptr(), &len);
}


inline int SockAddr::getsockname(SOCKET s)
{
	len = MAX_LEN;
	int R = ::getsockname(s, ptr(), &len);
	if (R < 0)
		clear();
	return R;
}


inline int SockAddr::getpeername(SOCKET s)
{
	len = MAX_LEN;
	int R = ::getpeername(s, ptr(), &len);
	if (R < 0)
		clear();
	return R;
}


inline unsigned short SockAddr::family() const
{
	return data.sock.sa_family;
}


inline unsigned short SockAddr::port() const
{
	switch (family())
	{
	case AF_INET:
		return ntohs(data.inet.sin_port);
	case AF_INET6:
		return ntohs(data.inet6.sin6_port);
	}
	return 0; // exception?
}


inline void SockAddr::setPort(unsigned short x)
{
	switch (family())
	{
	case AF_INET:
		data.inet.sin_port = htons(x);
		return;
	case AF_INET6:
		data.inet6.sin6_port = htons(x);
		return;
	}
	// exception?
}


// if address is embedded IPv4, convert it to normal IPv4
inline void SockAddr::unmapV4()
{
	if (family() != AF_INET6)
		return;

	// IPv6 mapped IPv4 addresses are ::ffff:0:0/32
	static const unsigned char v4mapped_pfx[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

	if (memcmp(data.inet6.sin6_addr.s6_addr, v4mapped_pfx, sizeof(v4mapped_pfx)) != 0)
		return;

	unsigned short port = ntohs(data.inet6.sin6_port);
	struct in_addr addr;
	memcpy(&addr, (char*)(&data.inet6.sin6_addr.s6_addr) + sizeof(v4mapped_pfx), sizeof(addr));

	data.inet.sin_family = AF_INET;
	data.inet.sin_port = htons(port);
	data.inet.sin_addr.s_addr = addr.s_addr;
	len = sizeof(struct sockaddr_in);
}


#endif // REMOTE_SOCKADDR_H
