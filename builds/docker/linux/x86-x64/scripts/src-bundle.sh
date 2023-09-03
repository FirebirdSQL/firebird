#!/bin/sh
set -e

./autogen.sh \
	--host=$BUILD_ARCH \
	--prefix=/opt/firebird \
	--enable-binreloc \
	--with-builtin-tomcrypt \
	--with-termlib=:libncurses.a \
	--with-atomiclib=:libatomic.a

./src/misc/src_bundle.sh
