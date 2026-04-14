/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		DbImplementation.cpp
 *	DESCRIPTION:	Database implementation
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  https://www.ibphoenix.com/about/firebird/idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/DbImplementation.h"

#include "../jrd/ods.h"

namespace {

[[maybe_unused]] static constexpr UCHAR CpuIntel = 0;
[[maybe_unused]] static constexpr UCHAR CpuAmd = 1;
[[maybe_unused]] static constexpr UCHAR CpuUltraSparc = 2;
[[maybe_unused]] static constexpr UCHAR CpuPowerPc = 3;
[[maybe_unused]] static constexpr UCHAR CpuPowerPc64 = 4;
[[maybe_unused]] static constexpr UCHAR CpuMipsel = 5;
[[maybe_unused]] static constexpr UCHAR CpuMips = 6;
[[maybe_unused]] static constexpr UCHAR CpuArm = 7;
[[maybe_unused]] static constexpr UCHAR CpuIa64 = 8;
[[maybe_unused]] static constexpr UCHAR CpuS390 = 9;
[[maybe_unused]] static constexpr UCHAR CpuS390x = 10;
[[maybe_unused]] static constexpr UCHAR CpuSh = 11;
[[maybe_unused]] static constexpr UCHAR CpuSheb = 12;
[[maybe_unused]] static constexpr UCHAR CpuHppa = 13;
[[maybe_unused]] static constexpr UCHAR CpuAlpha = 14;
[[maybe_unused]] static constexpr UCHAR CpuArm64 = 15;
[[maybe_unused]] static constexpr UCHAR CpuPowerPc64el = 16;
[[maybe_unused]] static constexpr UCHAR CpuM68k = 17;
[[maybe_unused]] static constexpr UCHAR CpuRiscV64 = 18;
[[maybe_unused]] static constexpr UCHAR CpuMips64el = 19;
[[maybe_unused]] static constexpr UCHAR CpuLoongArch = 20;

[[maybe_unused]] static constexpr UCHAR OsWindows = 0;
[[maybe_unused]] static constexpr UCHAR OsLinux = 1;
[[maybe_unused]] static constexpr UCHAR OsDarwin = 2;
[[maybe_unused]] static constexpr UCHAR OsSolaris = 3;
[[maybe_unused]] static constexpr UCHAR OsHpux = 4;
[[maybe_unused]] static constexpr UCHAR OsAix = 5;
[[maybe_unused]] static constexpr UCHAR OsMms = 6;
[[maybe_unused]] static constexpr UCHAR OsFreeBsd = 7;
[[maybe_unused]] static constexpr UCHAR OsNetBsd = 8;

[[maybe_unused]] static constexpr UCHAR CcMsvc = 0;
[[maybe_unused]] static constexpr UCHAR CcGcc = 1;
[[maybe_unused]] static constexpr UCHAR CcXlc = 2;
[[maybe_unused]] static constexpr UCHAR CcAcc = 3;
[[maybe_unused]] static constexpr UCHAR CcSunStudio = 4;
[[maybe_unused]] static constexpr UCHAR CcIcc = 5;

static constexpr UCHAR EndianLittle = 0;
static constexpr UCHAR EndianBig = 1;
static constexpr UCHAR EndianMask = 1;

const char* hardware[] = {
	"Intel/i386",
	"AMD/Intel/x64",
	"UltraSparc",
	"PowerPC",
	"PowerPC64",
	"MIPSEL",
	"MIPS",
	"ARM",
	"IA64",
	"s390",
	"s390x",
	"SH",
	"SHEB",
	"HPPA",
	"Alpha",
	"ARM64",
	"PowerPC64el",
	"M68k",
	"RiscV64",
	"MIPS64EL",
	"LOONGARCH"
};

const char* operatingSystem[] = {
	"Windows",
	"Linux",
	"Darwin",
	"Solaris",
	"HPUX",
	"AIX",
	"MVS",
	"FreeBSD",
	"NetBSD"
};

const char* compiler[] = {
	"MSVC",
	"gcc",
	"xlC",
	"aCC",
	"SunStudio",
	"icc"
};

// This table lists pre-fb3 implementation codes
constexpr UCHAR backwardTable[FB_NELEM(hardware) * FB_NELEM(operatingSystem)] =
{
//				Intel	AMD		Sparc	PPC		PPC64	MIPSEL	MIPS	ARM		IA64	s390	s390x	SH		SHEB	HPPA	Alpha	ARM64	PPC64el	M68k	RiscV64 MIPS64EL LoongArch
/* Windows */	50,		68,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* Linux */		60,		66,		65,		69,		86,		71,		72,		75, 	76,		79, 	78,		80,		81,		82,		83,		84,		85,		87,		88,		90,		93,
/* Darwin */	70,		73,		0,		63,		77,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* Solaris */	0,		0,		30,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* HPUX */		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		31,		0,		0,		0,		0,		0,		0,		0,
/* AIX */		0,		0,		0,		35,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* MVS */		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* FreeBSD */	61,		67,		0,		92,		91,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		89,		0,		0,		0,		0,
/* NetBSD */	62,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0
};


constexpr UCHAR backEndianess[FB_NELEM(hardware)] =
{
//	Intel	AMD		Sparc	PPC		PPC64	MIPSEL	MIPS	ARM		IA64	s390	s390x	SH		SHEB	HPPA	Alpha	ARM64	PPC64el	M68k	RiscV64 MIPS64EL LoongArch
	0,		0,		1,		1,		1,		0,		1,		0,		0,		1,		1,		0,		1,		1,		0,		0,		0,		1,		0,		0,		0,
};

} // anonymous namespace

namespace Firebird {

DbImplementation::DbImplementation(const Ods::header_page* h) noexcept
	: di_cpu(h->hdr_db_impl.hdr_cpu), di_os(h->hdr_db_impl.hdr_os),
	  di_cc(h->hdr_db_impl.hdr_cc), di_flags(h->hdr_db_impl.hdr_compat)
{
}

#define GET_ARRAY_ELEMENT(array, elem) ((elem) < FB_NELEM(array) ? array[(elem)] : "** Unknown **")

const char* DbImplementation::cpu() const noexcept
{
	return GET_ARRAY_ELEMENT(hardware, di_cpu);
}

const char* DbImplementation::os() const noexcept
{
	return GET_ARRAY_ELEMENT(operatingSystem, di_os);
}

const char* DbImplementation::cc() const noexcept
{
	return GET_ARRAY_ELEMENT(compiler, di_cc);
}

string DbImplementation::implementation() const
{
	string rc("Firebird/");
	rc += os();
	rc += "/";
	rc += cpu();
	return rc;
}

const char* DbImplementation::endianess() const noexcept
{
	return (di_flags & EndianMask) == EndianBig ? "big" : "little";
}

const DbImplementation DbImplementation::current(
		FB_CPU, FB_OS, FB_CC,
#ifdef WORDS_BIGENDIAN
		EndianBig);
#else
		EndianLittle);
#endif

bool DbImplementation::compatible(const DbImplementation& v) const noexcept
{
	return di_flags == v.di_flags;
}

void DbImplementation::store(Ods::header_page* h) const noexcept
{
	h->hdr_db_impl.hdr_cpu = di_cpu;
	h->hdr_db_impl.hdr_os = di_os;
	h->hdr_db_impl.hdr_cc = di_cc;
	h->hdr_db_impl.hdr_compat = di_flags;
}

void DbImplementation::stuff(UCHAR** info) const noexcept
{
	UCHAR* p = *info;
	*p++ = di_cpu;
	*p++ = di_os;
	*p++ = di_cc;
	*p++ = di_flags;
	*info = p;
}

DbImplementation DbImplementation::pick(const UCHAR* info) noexcept
{
	//DbImplementation(UCHAR p_cpu, UCHAR p_os, UCHAR p_cc, UCHAR p_flags)
	return DbImplementation(info[0], info[1], info[2], info[3]);
}

DbImplementation DbImplementation::fromBackwardCompatibleByte(UCHAR bcImpl) noexcept
{
	for (UCHAR os = 0; os < FB_NELEM(operatingSystem); ++os)
	{
		for (UCHAR hw = 0; hw < FB_NELEM(hardware); ++hw)
		{
			const USHORT ind = USHORT(os) * FB_NELEM(hardware) + USHORT(hw);
			if (backwardTable[ind] == bcImpl)
			{
				return DbImplementation(hw, os, 0xFF, backEndianess[hw] ? EndianBig : EndianLittle);
			}
		}
	}

	return DbImplementation(0xFF, 0xFF, 0xFF, 0x80);
}

UCHAR DbImplementation::backwardCompatibleImplementation() const noexcept
{
	if (di_cpu >= FB_NELEM(hardware) || di_os >= FB_NELEM(operatingSystem))
	{
		return 0;
	}

	return backwardTable[USHORT(di_os) * FB_NELEM(hardware) + USHORT(di_cpu)];
}

} // namespace Firebird
