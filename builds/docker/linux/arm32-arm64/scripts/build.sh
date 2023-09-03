#!/bin/sh
set -e

./autogen.sh \
	--host=$BUILD_ARCH \
	--prefix=/opt/firebird \
	--enable-binreloc \
	--with-builtin-tomcrypt \
	--with-termlib=:libncurses.a \
	--with-atomiclib=:libatomic.a

make -j${CPUCOUNT}
make tests -j${CPUCOUNT}
make run_tests
make dist
