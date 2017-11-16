################################################################################
#
# configure
#
################################################################################

########################################
# FUNCTION check_includes
########################################
function(check_includes files)
    foreach(F ${${files}})
        set(name ${F})
        string(REPLACE "." "_" name ${name})
        string(REPLACE "/" "_" name ${name})
        string(TOUPPER ${name} name)
        check_include_files(${F} HAVE_${name})
        #message("/* Define to 1 if you have the <${F}> header file. */")
        #message("#cmakedefine HAVE_${name} 1")
        #message("")
    endforeach()
endfunction(check_includes)

########################################
# FUNCTION check_functions
########################################
function(check_functions functions)
    foreach(F ${${functions}})
        set(name ${F})
        string(TOUPPER ${name} name)
        check_function_exists(${F} HAVE_${name})
        #message("/* Define to 1 if you have the `${F}' function. */")
        #message("#cmakedefine HAVE_${name} 1")
        #message("")
    endforeach()
endfunction(check_functions)

########################################
# FUNCTION check_type_alignment
########################################
function(check_type_alignment type var)
    if (NOT DEFINED ${var})
        check_c_source_runs("main(){struct s{char a;${type} b;};exit((int)&((struct s*)0)->b);}" ${var})
        #message(STATUS "Performing Test ${var} - It's still OK.")
        message(STATUS "Performing Test ${var} - Success")
        set(${var} ${${var}_EXITCODE} CACHE STRING "${type} alignment" FORCE)
    endif()
endfunction(check_type_alignment)

########################################
# FUNCTION check_symbol
########################################
function(check_symbol symbol var)
    if (NOT ${var}_SYMBOL)
        foreach(f ${ARGN})
            if (NOT ${var})
                unset(${var} CACHE)
                message(STATUS "Looking for ${symbol} - ${f}")
                check_symbol_exists(${symbol} ${f} ${var})
            endif()
        endforeach()
    endif()
    set(${var}_SYMBOL 1 CACHE INTERNAL "Do not check this symbol again")
endfunction(check_symbol)

########################################

include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckPrototypeDefinition)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)

if (ANDROID)
    set(LINUX 1)
endif()

if (IOS)
    set(CMAKE_SYSTEM_PROCESSOR "armv7")
    add_definitions(-D__arm__)
endif()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    set(CLANG 1)
endif()

set(ENABLE_BINRELOC 1)

string(TOUPPER ${CMAKE_SYSTEM_NAME} CMAKE_SYSTEM_NAME_UPPER)
set(${CMAKE_SYSTEM_NAME_UPPER} 1)

string(TOUPPER ${CMAKE_SYSTEM_PROCESSOR} CMAKE_SYSTEM_PROCESSOR_UPPER)
string(FIND ${CMAKE_SYSTEM_PROCESSOR} "arm" ARM)
if (NOT ${ARM} EQUAL -1)
    set(ARM 1)
else()
    set(ARM)
endif()
if (${CMAKE_SYSTEM_PROCESSOR_UPPER} STREQUAL "X86_64" OR
    ${CMAKE_SYSTEM_PROCESSOR_UPPER} STREQUAL "AMD64")
    set(AMD64 1)
    set(I386 1)
endif()
set(${CMAKE_SYSTEM_PROCESSOR_UPPER} 1)

set(SHRLIB_EXT ${CMAKE_SHARED_LIBRARY_SUFFIX})
string(REPLACE "." "" SHRLIB_EXT ${SHRLIB_EXT})

set(CASE_SENSITIVITY "true")
set(SUPPORT_RAW_DEVICES 1)

set(include_files_list
    aio.h
    assert.h
    atomic.h
    atomic_ops.h
    crypt.h
    ctype.h
    dirent.h
    dlfcn.h
    editline.h
    errno.h
    fcntl.h
    float.h
    grp.h
    iconv.h
    io.h
    inttypes.h
    langinfo.h
    libio.h
    linux/falloc.h
    limits.h
    locale.h
    math.h
    memory.h
    mntent.h
    mnttab.h
    ndir.h
    netconfig.h
    netinet/in.h
    poll.h
    pthread.h
    pwd.h
    rpc/rpc.h
    rpc/xdr.h
    semaphore.h
    setjmp.h
    signal.h
    socket.h
    stdarg.h
    stdint.h
    stdlib.h
    string.h
    strings.h
    sys/dir.h
    sys/file.h
    sys/ioctl.h
    sys/ipc.h
    sys/mntent.h
    sys/mnttab.h
    sys/mount.h
    sys/ndir.h
    sys/param.h
    sys/resource.h
    sys/sem.h
    sys/select.h
    sys/siginfo.h
    sys/signal.h
    sys/socket.h
    sys/sockio.h
    sys/stat.h
    sys/syscall.h
    sys/time.h
    sys/timeb.h
    sys/types.h
    sys/uio.h
    sys/wait.h
    termio.h
    termios.h
    unistd.h
    varargs.h
    vfork.h
    winsock2.h
    zlib.h
)
check_includes(include_files_list)

#if test "$EDITLINE_FLG" = "Y"; then
#  AC_HEADER_DIRENT
#  AC_DEFINE(HAVE_EDITLINE_H, 1, [Define this if editline is in use])
#fi

set(functions_list
    accept4
    AO_compare_and_swap_full
    clock_gettime
    ctime_r
    dirname
    fallocate
    fchmod
    fsync
    flock
    fork
    getpagesize
    getcwd getwd
    gettimeofday
    gmtime_r
    initgroups
    localtime_r
    mkstemp
    mmap
    nanosleep
    poll
    posix_fadvise
    pread pwrite
    pthread_cancel
    pthread_keycreate pthread_key_create
    pthread_mutexattr_setprotocol
    pthread_mutexattr_setrobust_np
    pthread_mutex_consistent_np
    pthread_rwlockattr_setkind_np
    qsort_r
    setitimer
    semtimedop
    setpgid
    setpgrp
    setmntent getmntent
    setrlimit getrlimit
    sigaction
    sigset
    snprintf vsnprintf
    strcasecmp stricmp
    strncasecmp strnicmp
    strdup
    strerror_r
    swab _swab
    tcgetattr
    time times
    vfork
)
check_functions(functions_list)

if (APPLE)
    set(HAVE_QSORT_R 0 CACHE STRING "Disabled on OS X" FORCE)
endif()

check_cxx_source_compiles("#include <unistd.h>\nmain(){fdatasync(0);}" HAVE_FDATASYNC)

check_library_exists(dl dladdr "${CMAKE_LIBRARY_PREFIX}" HAVE_DLADDR)
check_library_exists(m fegetenv "${CMAKE_LIBRARY_PREFIX}" HAVE_FEGETENV)
check_library_exists(m llrint "${CMAKE_LIBRARY_PREFIX}" HAVE_LLRINT)
check_library_exists(pthread sem_init "${CMAKE_LIBRARY_PREFIX}" HAVE_SEM_INIT)
check_library_exists(pthread sem_timedwait "${CMAKE_LIBRARY_PREFIX}" HAVE_SEM_TIMEDWAIT)  

check_type_size(caddr_t HAVE_CADDR_T)
check_c_source_compiles("#include <sys/sem.h>\nmain(){union semun s;return 0;}" HAVE_SEMUN)
set(CMAKE_EXTRA_INCLUDE_FILES sys/socket.h sys/types.h)
check_type_size(socklen_t HAVE_SOCKLEN_T)
set(CMAKE_EXTRA_INCLUDE_FILES)

check_type_size(long SIZEOF_LONG)
check_type_size(size_t SIZEOF_SIZE_T)
check_type_size("void *" SIZEOF_VOID_P)

check_type_size(gid_t HAVE_GID_T)
check_type_size(off_t HAVE_OFF_T)
check_type_size(pid_t HAVE_PID_T)
check_type_size(size_t HAVE_SIZE_T)
check_type_size(uid_t HAVE_UID_T)

if (${HAVE_OFF_T} AND ${HAVE_OFF_T} EQUAL 8)
    set(_FILE_OFFSET_BITS 64)
endif()

test_big_endian(WORDS_BIGENDIAN)
check_symbol_exists(INFINITY math.h HAVE_INFINITY)
check_symbol_exists(va_copy stdarg.h HAVE_VA_COPY)
check_symbol(SOCK_CLOEXEC HAVE_DECL_SOCK_CLOEXEC socket.h sys/socket.h)

set(CMAKE_EXTRA_INCLUDE_FILES Windows.h)
check_type_size("char[MAX_PATH]" MAXPATHLEN)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(TIMEZONE_TYPE "struct timezone")
if (APPLE OR MINGW)
    set(TIMEZONE_TYPE "void")
endif()
check_prototype_definition(
    gettimeofday
    "int gettimeofday(struct timeval *tv, ${TIMEZONE_TYPE} *tz)"
    0
    "sys/time.h"
    GETTIMEOFDAY_RETURNS_TIMEZONE
)

check_prototype_definition(
    getmntent
    "int getmntent(FILE *file, struct mnttab *mptr)"
    0
    mntent.h
    GETMNTENT_TAKES_TWO_ARGUMENTS
)

check_struct_has_member("struct dirent" d_type dirent.h HAVE_STRUCT_DIRENT_D_TYPE)

check_c_source_compiles("#include <unistd.h>\nmain(){getpgrp();}" GETPGRP_VOID)
check_c_source_compiles("#include <unistd.h>\nmain(){setpgrp();}" SETPGRP_VOID)

check_c_source_compiles("__thread int a = 42;main(){a = a + 1;}" HAVE___THREAD)
check_c_source_compiles("#include <sys/time.h>\n#include <time.h>\nmain(){}" TIME_WITH_SYS_TIME)

set(CMAKE_REQUIRED_LIBRARIES pthread)
check_c_source_compiles("#include <semaphore.h>\nmain(){sem_t s;sem_init(&s,0,0);}" WORKING_SEM_INIT)
set(CMAKE_REQUIRED_LIBRARIES)

if (EXISTS "/proc/self/exe")
    set(HAVE__PROC_SELF_EXE 1)
endif()

########################################

if (NOT CMAKE_CROSSCOMPILING)
    check_type_alignment(long FB_ALIGNMENT)
    check_type_alignment(double FB_DOUBLE_ALIGN)
else() # CMAKE_CROSSCOMPILING
    set(FB_ALIGNMENT 8)
    set(FB_DOUBLE_ALIGN 8)
    if (ANDROID)
        set(HAVE__PROC_SELF_EXE 1)
    endif()
endif()

########################################

if (WIN32)
    set(ENABLE_BINRELOC 0)
    set(SUPPORT_RAW_DEVICES 0)
    set(WIN_NT 1)
    set(CASE_SENSITIVITY "false")
endif(WIN32)

if (APPLE)
    set(ENABLE_BINRELOC 0)
    set(CASE_SENSITIVITY "false")
endif()

################################################################################
