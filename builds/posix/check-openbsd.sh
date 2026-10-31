#!/bin/sh
# Build and test a disposable checkout. Do not run this as root.
set -eu
umask 022

fail()
{
	printf '%s\n' "$*" >&2
	exit 1
}

[ "$(uname -s)" = OpenBSD ] || fail 'This check requires OpenBSD.'
case "$(uname -m)" in
	amd64|i386) ;;
	*) fail 'Only amd64 and i386 are covered by this port.' ;;
esac
[ "$(id -u)" != 0 ] || fail 'Use an unprivileged account.'

fb_source=$(CDPATH= cd "$(dirname "$0")/../.." && pwd -P)
cd "$fb_source"
case "$fb_source" in
	*[!a-zA-Z0-9_./-]*) fail 'Use a source path containing only letters, digits, _, ., / and -.' ;;
esac
[ -f configure.ac ] && [ -d src/common ] || fail 'Firebird source tree not found.'
[ ! -e config.status ] && [ ! -e Makefile ] && [ ! -e gen/Release ] ||
	fail 'Use a fresh, unconfigured checkout; existing build files are left untouched.'
fb_check="$fb_source/.openbsd-check"
[ ! -e "$fb_check" ] || fail 'The .openbsd-check directory already exists; keep its logs for review.'

: "${AUTOCONF_VERSION:=2.72}"
: "${AUTOMAKE_VERSION:=1.18}"
: "${JOBS:=1}"
case "$JOBS" in ''|*[!0-9]*|0*) fail 'JOBS must be a positive integer without leading zeroes.' ;; esac
export AUTOCONF_VERSION AUTOMAKE_VERSION

unset LIBS LD_LIBRARY_PATH FIREBIRD FIREBIRD_LOCK FIREBIRD_MSG FIREBIRD_TMP
unset FIREBIRD_BOOT_BUILD ISC_USER ISC_PASSWORD MAKEFLAGS MFLAGS NOCONFIGURE
unset AUTORECONF ACLOCAL LIBTOOL LIBTOOLIZE
export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
export CC=cc CXX=c++ CFLAGS= CXXFLAGS=
export CPPFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/lib

for tool in cc c++ gmake cmake unzip autoreconf aclocal libtoolize
do
	command -v "$tool" >/dev/null 2>&1 || fail "Required tool not found: $tool"
done

mkdir "$fb_check"
mkdir "$fb_check/locks" "$fb_check/tmp"
{
	uname -a
	cc --version
	c++ --version
	gmake --version
	cmake --version
	autoreconf --version
	aclocal --version
	pkg_info
	df -h .
	swapctl -sk
	ulimit -a
	if [ -f OPENBSD-SNAPSHOT.txt ]; then
		cat OPENBSD-SNAPSHOT.txt
	fi
	if command -v git >/dev/null 2>&1 && [ -d .git ]; then
		git rev-parse HEAD
		git status --short
		git diff --stat
	fi
} >"$fb_check/environment.log" 2>&1

run()
{
	fb_step=$1
	shift
	printf '%s: %s\n' "$fb_step" "$*"
	if "$@" >"$fb_check/$fb_step.log" 2>&1; then
		printf '%s: OK\n' "$fb_step"
	else
		fb_rc=$?
		tail -n 60 "$fb_check/$fb_step.log"
		fail "$fb_step failed (status $fb_rc). Logs: $fb_check"
	fi
}

run configure sh ./autogen.sh --prefix=/opt/firebird-openbsd-test --disable-binreloc --disable-raw-devices --with-builtin-tommath --with-builtin-tomcrypt
export ISC_USER=SYSDBA
run build gmake -j"$JOBS" TARGET=Release
run test-build gmake -C gen TARGET=Release tests

export FIREBIRD="$fb_source/gen/Release/firebird"
export FIREBIRD_LOCK="$fb_check/locks" FIREBIRD_TMP="$fb_check/tmp"
export LD_LIBRARY_PATH="$FIREBIRD/lib:/usr/local/lib"
run common-tests "$FIREBIRD/tests/common_test" --log_level=all
run engine-tests "$FIREBIRD/tests/engine_test" --log_level=all
run stage gmake -C gen -f Makefile.install TARGET=Release DESTDIR="$fb_check/stage" install

export FIREBIRD="$fb_check/stage/opt/firebird-openbsd-test"
export LD_LIBRARY_PATH="$FIREBIRD/lib:/usr/local/lib"
for header in ibase.h ib_util.h iberror.h firebird/Interface.h firebird/fb_c_api.h firebird/impl/iberror_c.h
do
	[ -f "$FIREBIRD/include/$header" ] || fail "Missing staged SDK header: $header"
done

run sdk-build cc -I"$FIREBIRD/include" -L"$FIREBIRD/lib" -Wl,-rpath,"$FIREBIRD/lib" -x c - -lfbclient -o "$fb_check/sdk-test" <<'EOF'
#include <ibase.h>
#include <stdio.h>
int main(void)
{
	ISC_SCHAR version[256] = {0};
	isc_get_client_version(version);
	puts((const char*) version);
	return 0;
}
EOF
run sdk-run "$fb_check/sdk-test"
run sdk-libraries ldd "$fb_check/sdk-test"
printf 'Checks completed. Review the logs in %s. No service was started or installed.\n' "$fb_check"
