#!/bin/sh
set -e

gcc -v
ld -v
make -v

./autogen.sh \
	--host=$BUILD_ARCH \
	--prefix=/opt/firebird \
	--enable-binreloc \
	--with-builtin-tomcrypt \
	--with-termlib=:libncurses.a \
	--with-atomiclib=:libatomic.a

make -j$(nproc)
make tests -j$(nproc)
make run_tests
make dist
