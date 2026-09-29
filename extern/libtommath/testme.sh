#!/bin/bash
#
# return values of this script are:
#   0  success
# 128  a test failed
#  >0  the number of timed-out tests
# 255  parsing of parameters failed

set -e

if [ -f /proc/cpuinfo ]
then
  MAKE_JOBS=$(( ($(cat /proc/cpuinfo | grep -E '^processor[[:space:]]*:' | tail -n -1 | cut -d':' -f2) + 1) * 2 + 1 ))
else
  MAKE_JOBS=8
fi

ret=0
TEST_CFLAGS=""

_help()
{
  cat << EOF
Usage options for $(basename $0) [--with-cc=arg [other options]]

Executing this script without any parameter will only run the default
configuration that has automatically been determined for the
architecture you're running.

    --with-cc=*             The compiler(s) to use for the tests
                            This is an option that will be iterated.

    --test-vs-mtest=*       Run test vs. mtest for '*' operations.
                            Only the first of each options will be
                            taken into account.

To be able to specify options a compiler has to be given with
the option --with-cc=compilername
All other options will be tested with all MP_xBIT configurations.

    --with-{m64,m32,mx32}   The architecture(s) to build and test
                            for, e.g. --with-mx32.
                            This is an option that will be iterated,
                            multiple selections are possible.
                            The mx32 architecture is not supported
                            by clang and will not be executed.

    --cflags=*              Give an option to the compiler,
                            e.g. --cflags=-g
                            This is an option that will always be
                            passed as parameter to CC.

    --make-option=*         Give an option to make,
                            e.g. --make-option="-f makefile.shared"
                            This is an option that will always be
                            passed as parameter to make.

    --with-low-mp           Also build&run tests with -DMP_{8,16,32}BIT.

    --mtest-real-rand       Use real random data when running mtest.

    --with-valgrind
    --with-valgrind=*       Run in valgrind (slow!).

    --limit-valgrind        Run with valgrind on CI only on specific branches.

    --valgrind-options      Additional Valgrind options
                            Some of the options like e.g.:
                            --track-origins=yes add a lot of extra
                            runtime and may trigger the 30 minutes
                            timeout.

Godmode:

    --all                   Choose all architectures and gcc and clang
                            as compilers but does not run valgrind.

    --format                Runs the various source-code formatters
                            and generators and checks if the sources
                            are clean.

    -h
    --help                  This message

    -v
    --version               Prints the version. It is just the number
                            of git commits to this file, no deeper
                            meaning attached
EOF
  exit 0
}

_die()
{
  echo "error $2 while $1"
  if [ "$2" != "124" ]
  then
    exit 128
  else
    echo "assuming timeout while running test - continue"
    local _tail=""
    which tail >/dev/null && _tail="tail -n 1 test_${suffix}.log" && \
    echo "last line of test_"${suffix}".log was:" && $_tail && echo ""
    ret=$(( $ret + 1 ))
  fi
}

_fixup_cflags() {
  compiler_version=$(echo "$1="$($1 -dumpversion))
  case "$compiler_version" in
    clang*=4.2.1)
      # one of my versions of clang complains about some stuff in stdio.h and stdarg.h ...
      TEST_CFLAGS="-Wno-typedef-redefinition"
    ;;
    gcc*=9)
      # gcc 9 seems to sometimes think that variables are uninitialized, but they are.
      TEST_CFLAGS="-Wno-maybe-uninitialized"
    ;;
    *)
      TEST_CFLAGS=""
    ;;
  esac
  echo $compiler_version
}

_make()
{
  echo -ne " Compile $1 $2"
  suffix=$(echo ${1}${2}  | tr ' ' '_')
  _fixup_cflags "$1"
  CC="$1" CFLAGS="$2 $TEST_CFLAGS" make -j$MAKE_JOBS $3 $MAKE_OPTIONS 2>gcc_errors_${suffix}.log
  errcnt=$(wc -l < gcc_errors_${suffix}.log)
  if [[ ${errcnt} -gt 1 ]]; then
    echo " failed"
    cat gcc_errors_${suffix}.log
    exit 128
  fi
}


_runtest()
{
  make clean > /dev/null
  local _timeout=""
  which timeout >/dev/null && _timeout="timeout --foreground 90"
  if [[ "$MAKE_OPTIONS" =~ "tune" ]]
  then
    # "make tune" will run "tune_it.sh" automatically, hence "autotune", but it cannot
    # get switched off without some effort, so we just let it run twice for testing purposes
    echo -e "\rRun autotune $1 $2"
    _make "$1" "$2" ""
    $_timeout $TUNE_CMD > test_${suffix}.log || _die "running autotune" $?
  else
    _make "$1" "$2" "test"
    echo -e "\rRun test $1 $2"
    $_timeout ./test > test_${suffix}.log || _die "running tests" $?
  fi
}

# This is not much more of a C&P of _runtest with a different timeout
# and the additional valgrind call.
# TODO: merge
_runvalgrind()
{
  make clean > /dev/null
  local _timeout=""
  # 30 minutes? Yes. Had it at 20 minutes and the Valgrind run needed over 25 minutes.
  # A bit too close for comfort.
  which timeout >/dev/null && _timeout="timeout --foreground 1800"
echo "MAKE_OPTIONS = \"$MAKE_OPTIONS\""
  if [[ "$MAKE_OPTIONS" =~ "tune"  ]]
  then
echo "autotune branch"
    _make "$1" "$2" ""
    # The shell used for /bin/sh is DASH 0.5.7-4ubuntu1 on the author's machine which fails valgrind, so
    # we just run on instance of etc/tune with the same options as in etc/tune_it.sh
    echo -e "\rRun etc/tune $1 $2 once inside valgrind"
    $_timeout $VALGRIND_BIN $VALGRIND_OPTS $TUNE_CMD > test_${suffix}.log || _die "running etc/tune" $?
  else
    _make "$1" "$2" "test"
    echo -e "\rRun test $1 $2 inside valgrind"
    $_timeout $VALGRIND_BIN $VALGRIND_OPTS ./test > test_${suffix}.log || _die "running tests" $?
  fi
}


_banner()
{
  echo "uname="$(uname -a)
  [[ "$#" != "0" ]] && (echo $1=$($1 -dumpversion)) || true
}

_exit()
{
  if [ "$ret" == "0" ]
  then
    echo "Tests successful"
  else
    echo "$ret tests timed out"
  fi

  exit $ret
}

ARCHFLAGS=""
COMPILERS=""
CFLAGS=""
WITH_LOW_MP=""
TEST_VS_MTEST=""
MTEST_RAND=""
# timed with an AMD A8-6600K
# 25 minutes
#VALGRIND_OPTS=" --track-origins=yes --leak-check=full --show-leak-kinds=all --error-exitcode=1 "
# 9 minutes (14 minutes with --test-vs-mtest=333333 --mtest-real-rand)
VALGRIND_OPTS=" --leak-check=full --show-leak-kinds=all --error-exitcode=1 "
#VALGRIND_OPTS=""
VALGRIND_BIN=""
CHECK_FORMAT=""
CHECK_SYMBOLS=""
C89=""
C89_C99_ROUNDTRIP=""
TUNE_CMD="./etc/tune -t -r 10 -L 3"

alive_pid=0

function kill_alive() {
  disown $alive_pid || true
  kill $alive_pid 2>/dev/null
}

function start_alive_printing() {
  [ "$alive_pid" == "0" ] || return 0;
  for i in `seq 1 10` ; do sleep 300 && echo "Tests still in Progress..."; done &
  alive_pid=$!
  trap kill_alive EXIT
}

while [ $# -gt 0 ];
do
  case $1 in
    "--with-m64" | "--with-m32" | "--with-mx32")
      ARCHFLAGS="$ARCHFLAGS ${1:6}"
    ;;
    --c89)
      C89="1"
    ;;
    --c89-c99-roundtrip)
      C89_C99_ROUNDTRIP="1"
    ;;
    --with-cc=*)
      COMPILERS="$COMPILERS ${1#*=}"
    ;;
    --cflags=*)
      CFLAGS="$CFLAGS ${1#*=}"
    ;;
    --valgrind-options=*)
      VALGRIND_OPTS="$VALGRIND_OPTS ${1#*=}"
    ;;
    --with-valgrind*)
      if [[ ${1#*d} != "" ]]
      then
        VALGRIND_BIN="${1#*=}"
      else
        VALGRIND_BIN="valgrind"
      fi
      start_alive_printing
    ;;
    --limit-valgrind*)
      if [[ ("$GITHUB_BASE_REF" == "develop" && "$PR_NUMBER" == "") || "$GITHUB_REF_NAME" == *"valgrind"* || "$COMMIT_MESSAGE" == *"valgrind"* ]]
      then
        if [[ ${1#*d} != "" ]]
        then
          VALGRIND_BIN="${1#*=}"
        else
          VALGRIND_BIN="valgrind"
        fi
        start_alive_printing
      fi
    ;;
    --make-option=*)
      MAKE_OPTIONS="$MAKE_OPTIONS ${1#*=}"
    ;;
    --with-low-mp)
      WITH_LOW_MP="1"
    ;;
    --test-vs-mtest=*)
      TEST_VS_MTEST="${1#*=}"
      if ! [ "$TEST_VS_MTEST" -eq "$TEST_VS_MTEST" ] 2> /dev/null
      then
         echo "--test-vs-mtest Parameter has to be int"
         exit 255
      fi
      start_alive_printing
    ;;
    --mtest-real-rand)
      MTEST_RAND="-DLTM_MTEST_REAL_RAND"
    ;;
    --format)
      CHECK_FORMAT="1"
    ;;
    --symbols)
      CHECK_SYMBOLS="1"
    ;;
    --all)
      COMPILERS="gcc clang"
      ARCHFLAGS="-m64 -m32 -mx32"
    ;;
    --help | -h)
      _help
    ;;
    --version | -v)
      echo $(git rev-list HEAD --count -- testme.sh) || echo "Unknown. Please run in original libtommath git repository."
      exit 0
    ;;
    *)
      echo "Ignoring option ${1}"
    ;;
  esac
  shift
done

function _check_git() {
  git update-index --refresh >/dev/null || true
  git diff-index --quiet HEAD -- . || ( echo "FAILURE: $*" && exit 1 )
}

[[ "$C89" == "1" ]] && make c89

if [[ "$C89_C99_ROUNDTRIP" == "1" ]]
then
  make c89
  make c99
  _check_git "make c89; make c99"
  exit $?
fi

if [[ "$CHECK_SYMBOLS" == "1" ]]
then
  make -f makefile.shared
  cat << EOF


The following list shows the discrepancy between
the shared library and the Windows dynamic library.
To fix this error, one of the following things
has to be done:
* the script which generates tommath.def has to be modified
    (function generate_def() in helper.pl).
* The exported symbols are really different for some reason
    This has to be manually investigated.

EOF
  exit $(comm -3 <(nm -D --defined-only .libs/libtommath.so | cut -d' ' -f3 | grep -v '^_' | sort) <(tail -n+9 tommath.def | tr -d ' ' | sort) | tee /dev/tty | wc -l)
fi

if [[ "$CHECK_FORMAT" == "1" ]]
then
  make astyle
  _check_git "make astyle"
  perl helper.pl --update-files
  _check_git "helper.pl --update-files"
  perl helper.pl --check-all
  _check_git "helper.pl --check-all"
  exit $?
fi

[[ "$VALGRIND_BIN" == "" ]] && VALGRIND_OPTS=""

# default to CC environment variable if no compiler is defined but some other options
if [[ "$COMPILERS" == "" ]] && [[ "$ARCHFLAGS$MAKE_OPTIONS$CFLAGS" != "" ]]
then
   COMPILERS="${CC:-cc}"
# default to CC environment variable and run only default config if no option is given
elif [[ "$COMPILERS" == "" ]]
then
  _banner "$CC"
  if [[ "$VALGRIND_BIN" != "" ]]
  then
    _runvalgrind "$CC" ""
  else
    _runtest "$CC" ""
  fi
  _exit
fi


archflags=( $ARCHFLAGS )
compilers=( $COMPILERS )

# choosing a compiler without specifying an architecture will use the default architecture
if [ "${#archflags[@]}" == "0" ]
then
  archflags[0]=" "
fi

_banner

if [[ "$TEST_VS_MTEST" != "" ]]
then
   make clean > /dev/null
   _make "${compilers[0]}" "${archflags[0]} $CFLAGS" "mtest_opponent"
   echo
   _make "gcc" "$MTEST_RAND" "mtest"
   echo
   echo "Run test vs. mtest for $TEST_VS_MTEST iterations"
   _timeout=""
   which timeout >/dev/null && _timeout="timeout --foreground 1800"
   $_timeout ./mtest/mtest $TEST_VS_MTEST | $VALGRIND_BIN $VALGRIND_OPTS  ./mtest_opponent > valgrind_test.log 2> test_vs_mtest_err.log
   retval=$?
   head -n 5 valgrind_test.log
   tail -n 2 valgrind_test.log
   exit $retval
fi

for i in "${compilers[@]}"
do
  if [ -z "$(which $i)" ]
  then
    echo "Skipped compiler $i, file not found"
    continue
  fi

  for a in "${archflags[@]}"
  do
    if [[ $(expr "$i" : "clang") -ne 0 && "$a" == "-mx32" ]]
    then
      echo "clang -mx32 tests skipped"
      continue
    fi
    if [[ "$VALGRIND_BIN" != "" ]]
    then
      _runvalgrind "$i" "$a $CFLAGS"
      [ "$WITH_LOW_MP" != "1" ] && continue
      _runvalgrind "$i" "$a -DMP_16BIT $CFLAGS"
      _runvalgrind "$i" "$a -DMP_32BIT $CFLAGS"
    else
      _runtest "$i" "$a $CFLAGS"
      [ "$WITH_LOW_MP" != "1" ] && continue
      _runtest "$i" "$a -DMP_16BIT $CFLAGS"
      _runtest "$i" "$a -DMP_32BIT $CFLAGS"
    fi
  done
done

_exit
