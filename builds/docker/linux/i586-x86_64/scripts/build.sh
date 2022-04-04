#!/bin/sh

# FIXME: Resolve this with a configure option.
sed -i 's/AC_CHECK_LIB(atomic, main)/AC_CHECK_LIB(:libatomic.a, main)/g' configure.ac

./autogen.sh --build=$BUILD_ARCH --prefix=/opt/firebird --enable-binreloc --with-builtin-tomcrypt --with-termlib=:libncurses.a
make -j${CPUCOUNT}
make dist
