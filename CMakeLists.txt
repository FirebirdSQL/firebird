#
# firebird (trunk)
#
#  This file has following organization:
#  1. cmake settings
#  2. project settings
#  3. packages
#  4. configure
#  5. compiler & linker settings
#  6. pre-build
#  7. build
#

################################################################################
#
# cmake settings
#
################################################################################

cmake_minimum_required(VERSION 3.20)

# In-source builds are not possible and so disabled.
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR
        "CMake generation for Firebird is not possible within the source directory!"
        "\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
        "\n "
        "\n rm CMakeCache.txt"
        "\n mkdir build"
        "\n cd build"
        "\n cmake .."
    )
endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/builds/cmake)

if (NATIVE_BUILD_DIR)
    get_filename_component(NATIVE_BUILD_DIR ${NATIVE_BUILD_DIR} ABSOLUTE)
else()
    set(NATIVE_BUILD_DIR ${CMAKE_BINARY_DIR})
endif()

# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake Targets")

################################################################################
#
# project settings
#
################################################################################

project("firebird" C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

########################################
# build type settings
########################################

# Define build mode
if (NOT DEFINED CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build.")
endif()

# Qt with MSVC does not have multiconfiguration option by default,
#  so we create it.
set(DEFAULT_CONFIGURATION)

# Set build types if none was specified
if (NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Debug Release MinSizeRel RelWithDebInfo)
    set(DEFAULT_CONFIGURATION ${CMAKE_BUILD_TYPE})
endif()

########################################
# Output directory settings
########################################

set(output_dir ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
set(boot_dir ${CMAKE_BINARY_DIR}/src)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${output_dir})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir})

if (MSVC OR XCODE)
    set(output_dir ${output_dir}/$<CONFIG>)
    set(boot_dir ${boot_dir}/$<CONFIG>)
endif()

if (MSVC AND DEFAULT_CONFIGURATION)
    foreach(conf ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${conf} conf2)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${conf2} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DEFAULT_CONFIGURATION})
    endforeach()
endif()

if (XCODE)
    foreach(conf ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${conf} conf2)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${conf2} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${conf}/bin)
    endforeach()
elseif (UNIX)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${output_dir}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir}/bin)
endif()

################################################################################
#
# packages
#
################################################################################

########################################
# icu
########################################

if (UNIX)
    if (NOT ICU_INCLUDE_DIR)
        find_path(ICU_INCLUDE_DIR unicode/ucnv.h
            PATHS
                /usr/include
                /usr/include/x86_64-linux-gnu
                /usr/local/include
                /opt/local/include
                /usr/local/opt/icu4c/include
            CMAKE_FIND_ROOT_PATH_BOTH
        )
        if ("${ICU_INCLUDE_DIR}" STREQUAL "ICU_INCLUDE_DIR-NOTFOUND")
            message(FATAL_ERROR "ICU headers not found!")
        endif()
    endif()
    if (CMAKE_CROSSCOMPILING)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ICU_INCLUDE_DIR}/unicode
            ${CMAKE_BINARY_DIR}/unicode
        )
        set(ICU_INCLUDE_DIR ${CMAKE_BINARY_DIR})
    endif()
    set(ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIR})
    include_directories(${ICU_INCLUDE_DIRS})
endif()

################################################################################
#
# configure
#
################################################################################

include(Configure)

if (FREEBSD)
    # temporary
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})
endif()

################################################################################
#
# Platform/CPU Detection and Definitions (from configure.ac)
#
################################################################################

# Set OS and CPU specific definitions used by common.h and other headers.
# This logic mirrors the main case statement in the autoconf script.

include(CheckTypeSize)
include(TestBigEndian)

# Check for byte order (endianness)
test_big_endian(WORDS_BIGENDIAN)
if(WORDS_BIGENDIAN)
    add_definitions(-DWORDS_BIGENDIAN)
endif()

if (WIN32)
    add_definitions(-DWIN_NT)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        add_definitions(-DAMD64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        add_definitions(-DARM64)
    else()
        add_definitions(-DI386)
    endif()
elseif(APPLE)
    add_definitions(-DDARWIN)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        add_definitions(-DARM64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        add_definitions(-DAMD64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
        add_definitions(-DI386)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "powerpc|ppc")
        check_type_size("void*" SIZEOF_VOID_P)
        if (SIZEOF_VOID_P EQUAL 8)
            add_definitions(-D__ppc64__)
        else()
            add_definitions(-D__ppc__)
        endif()
    endif()
elseif(LINUX)
    add_definitions(-DLINUX)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        add_definitions(-DAMD64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
        add_definitions(-DI386)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        add_definitions(-DARM64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        add_definitions(-DARM)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ia64")
        add_definitions(-DIA64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
        add_definitions(-DPPC64EL)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
        add_definitions(-DPPC64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "powerpc|ppc")
        add_definitions(-DPPC)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
        add_definitions(-DRISCV64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "loongarch")
        add_definitions(-DLOONGARCH)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
        add_definitions(-Dsparc)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips64el")
        add_definitions(-DMIPS64EL)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mipsel")
        add_definitions(-DMIPSEL)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips")
        add_definitions(-DMIPSEB)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "s390x")
        add_definitions(-D__s390x__)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "s390")
        add_definitions(-D__s390__)
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    add_definitions(-DFREEBSD)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
        add_definitions(-DAMD64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
        add_definitions(-DI386)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
        add_definitions(-DPPC64EL)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64")
        add_definitions(-DPPC64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "powerpc|ppc")
        add_definitions(-DPPC)
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    add_definitions(-DNETBSD)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
        add_definitions(-D__i386__)
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
    add_definitions(-D_AIX)
    check_type_size("void*" SIZEOF_VOID_P)
    add_definitions(-DSIZEOF_VOID_P=${SIZEOF_VOID_P})
elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    add_definitions(-D__sun -DSOLARIS)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
        add_definitions(-D__sparc)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        add_definitions(-D__amd64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
        add_definitions(-D__i386)
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "HP-UX")
    add_definitions(-DHPUX)
endif()

########################################
# LIBRARY Boost
########################################

# Provide an option to use system-wide Boost, defaulting to the embedded version.
option(USE_SYSTEM_BOOST "Use system-wide Boost library instead of embedded copy" OFF)

if(USE_SYSTEM_BOOST)
    # Find a system-installed Boost library.
    message(STATUS "Searching for system-wide Boost library...")
    find_package(Boost 1.70.0 COMPONENTS system thread REQUIRED)

    if(Boost_FOUND)
        message(STATUS "Found system Boost: ${Boost_VERSION_STRING}")
        # The variables Boost_INCLUDE_DIRS and Boost_LIBRARIES are set by find_package
        set(BOOST_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
        set(BOOST_LIBRARIES ${Boost_LIBRARIES})
    else()
        message(FATAL_ERROR "System Boost was requested but not found. Please install the Boost development packages (e.g., libboost-all-dev on Debian/Ubuntu).")
    endif()
else()
    # Use the embedded header-only Boost library.
    message(STATUS "Using embedded Boost library")
    set(BOOST_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/extern/boost)
    # For a header-only setup, there are no libraries to link.
    set(BOOST_LIBRARIES "")
endif()

########################################
# LIBRARY libcds
########################################

message(STATUS "Using embedded libcds library")

# Configure the libcds sub-project before adding it.
# We disable its tests as we don't need to build/run them as part of Firebird.
set(WITH_TESTS OFF CACHE BOOL "Disable libcds's own tests" FORCE)
set(ENABLE_UNIT_TEST OFF CACHE BOOL "Disable libcds's unit tests" FORCE)
set(ENABLE_STRESS_TEST OFF CACHE BOOL "Disable libcds's stress tests" FORCE)
add_subdirectory(extern/libcds)

# We define a variable pointing to the static version of the library ('cds-s').
# This is the target we will link against.
set(CDS_LIBRARIES cds-s)

########################################

set(FB_PREFIX ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME})
set(FB_IPC_NAME "FirebirdIPI")
set(FB_LOGFILENAME "firebird.log")
set(FB_SERVICE_NAME "gds_db")
set(FB_SERVICE_PORT 3050)

if (WIN32)
    set(FB_PREFIX "c:\\\\Program Files\\\\Firebird\\\\")
    set(FB_IPC_NAME "FIREBIRD")
endif()

set(AUTOCONFIG_SRC ${CMAKE_SOURCE_DIR}/src/include/gen/autoconfig.h.in)
set(AUTOCONFIG ${CMAKE_BINARY_DIR}/src/include/gen/autoconfig.h)
configure_file(${AUTOCONFIG_SRC} ${AUTOCONFIG} @ONLY)

################################################################################
#
# compiler & linker settings
#
################################################################################

add_definitions(-DDEV_BUILD)

if (WIN32)
    set(OS_DIR win32)
    set(VERSION_RC ${CMAKE_SOURCE_DIR}/src/jrd/version.rc)

    if (MSVC)
        set(disable_msvc_warnings "/wd4996")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP ${disable_msvc_warnings}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP ${disable_msvc_warnings}")

        if (MSVC_STATIC_RUNTIME)
            set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        endif()
    endif(MSVC)

    set(LIB_Ws2_32 Ws2_32)
    set(LIB_comctl32 comctl32)
    set(LIB_mpr mpr)
    set(LIB_version version)
endif(WIN32)

if (MINGW)
    # clear unix-style prefixes
    set(CMAKE_SHARED_LIBRARY_PREFIX)
    set(CMAKE_SHARED_MODULE_PREFIX)
    set(CMAKE_STATIC_LIBRARY_PREFIX)

    # 0x0601 is Windows 7 - see also comments in autoconfig_msvc.h
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4")
endif()

if (UNIX)
    set(OS_DIR posix)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -msse4")

    if (NOT CMAKE_CROSSCOMPILING)
        set(LIB_readline readline)
    endif()
    if (NOT FREEBSD)
        set(LIB_dl dl)
    endif()
endif()

if (NOT LINUX AND (CLANG OR IOS))
    set(LIB_iconv iconv)
endif()

if (FREEBSD OR APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif()

if (APPLE)
    set(OS_DIR darwin)

    if (NOT CMAKE_CROSSCOMPILING)
        include_directories(/opt/local/include)
        link_directories(/opt/local/lib)
        link_directories(/usr/local/opt/icu4c/lib)
    endif()

    find_library(LIB_CoreFoundation CoreFoundation)
elseif (UNIX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

################################################################################
#
# pre-build
#
################################################################################

if (WIN32)
    # icu
    if (NOT ICU_EXTRACT)
        message(STATUS "Extracting pre-built ICU")
        set(ICU_EXTRACT ${CMAKE_CURRENT_SOURCE_DIR}/extern/icu/icu.exe -y
            CACHE STRING "ICU extraction variable")
        execute_process(COMMAND ${ICU_EXTRACT})
    endif()

	# determine icu version
	foreach(v RANGE 63 83)
		if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/icu/icudt${v}l.dat)
			set(ICU_VERSION ${v} CACHE STRING "ICU version")
		endif()
	endforeach()

    # zlib
    if (NOT ZLIB_EXTRACT)
        message(STATUS "Extracting pre-built zlib")
        set(ZLIB_EXTRACT ${CMAKE_CURRENT_SOURCE_DIR}/extern/zlib/zlib.exe -y
            CACHE STRING "zlib extraction variable")
        execute_process(COMMAND ${ZLIB_EXTRACT})
    endif()
endif()

if (UNIX)
    file(GLOB vers_src "${CMAKE_SOURCE_DIR}/builds/posix/*.vers")
    foreach(f ${vers_src})
        get_filename_component(name ${f} NAME)
        set(name ${CMAKE_BINARY_DIR}/builds/posix/${name})

        if (AIX)
            file(WRITE ${name} "#!\n")
        elseif (LINUX)
            file(WRITE ${name} "{\nglobal:\n")
        endif()

        file(STRINGS ${f} strings)
        foreach(s ${strings})
            string(REGEX REPLACE "#.*$" "" s "${s}")
            string(STRIP "${s}" s)
            if (NOT "${s}" STREQUAL "")
                if (AIX)
                    file(APPEND ${name} "\t${s}\n")
                elseif (APPLE)
                    file(APPEND ${name} "\t_${s}\n")
                elseif (HPUX)
                    file(APPEND ${name} "+e ${s}\n")
                else()
                    file(APPEND ${name} "\t${s};\n")
                endif()
            endif()
        endforeach()

        if (LINUX)
            file(APPEND ${name} "local:\n\t*;\n};\n")
        endif()
    endforeach()
endif()

################################################################################
#
# build
#
################################################################################

include(BuildFunctions)

if (NOT CMAKE_CROSSCOMPILING)

create_boot_commands()
create_master_commands()

endif() # if (NOT CMAKE_CROSSCOMPILING)

crosscompile_prebuild_steps()

include_directories("extern/libtommath")
include_directories("extern/libtomcrypt/src/headers")
include_directories("extern/decNumber")
include_directories("extern/icu/include")
include_directories("extern/zlib")

include_directories("src/include")
include_directories("src/include/gen")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src/include")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src/include/gen")

########################################
# LIBRARY libtommath
########################################

file(GLOB libtommath_src "extern/libtommath/*.c" "extern/libtommath/*.h")

add_library             (libtommath STATIC ${libtommath_src})
project_group           (libtommath Extern)


########################################
# LIBRARY libtomcrypt
########################################

file(GLOB_RECURSE libtomcrypt_src "extern/libtomcrypt/src/*")
list(FILTER libtomcrypt_src EXCLUDE REGEX ".*aes_tab.c")
list(FILTER libtomcrypt_src EXCLUDE REGEX ".*whirltab.c")
list(FILTER libtomcrypt_src EXCLUDE REGEX ".*sober128tab.c")

add_library                 (libtomcrypt ${libtomcrypt_src})
target_link_libraries       (libtomcrypt libtommath)
target_compile_definitions  (libtomcrypt PRIVATE LTC_NO_ROLC LTC_SOURCE)
project_group               (libtomcrypt Extern)


########################################
# LIBRARY decNumber
########################################

file(GLOB_RECURSE decNumber_src "extern/decNumber/*.c" "extern/decNumber/*.h")
list(FILTER decNumber_src EXCLUDE REGEX ".*decBasic.c")
list(FILTER decNumber_src EXCLUDE REGEX ".*decCommon.c")

add_library                 (decNumber ${decNumber_src})
project_group               (decNumber Extern)

########################################
# LIBRARY re2
########################################

# This is needed for a full server build (not client-only).
# We can either use a system-provided re2 or the embedded one.
option(USE_SYSTEM_RE2 "Use system-wide re2 library instead of embedded copy" OFF)

if(USE_SYSTEM_RE2)
    # Find system re2
    find_path(RE2_INCLUDE_DIR re2/re2.h)
    find_library(RE2_LIBRARY NAMES re2)

    if(RE2_INCLUDE_DIR AND RE2_LIBRARY)
        message(STATUS "Found system RE2: ${RE2_LIBRARY}")
        include_directories(${RE2_INCLUDE_DIR})
        set(RE2_LIBRARIES ${RE2_LIBRARY})
    else()
        message(FATAL_ERROR "System RE2 not found. Please install re2 development package or disable USE_SYSTEM_RE2 by setting it to OFF.")
    endif()
else()
    # Use embedded re2 by adding its directory as a sub-project.
    message(STATUS "Using embedded re2 library")
    # Configure the re2 sub-project: build it as a static library and disable its tests.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build RE2 as a static library" FORCE)
    set(RE2_BUILD_TESTING OFF CACHE BOOL "Disable RE2's own tests" FORCE)

    add_subdirectory(extern/re2)
    set(RE2_LIBRARIES re2)
endif()

########################################
# EXECUTABLE btyacc
########################################

file(GLOB btyacc_src "extern/btyacc/*.c" "extern/btyacc/*.h")

if (NOT CMAKE_CROSSCOMPILING)

add_executable          (btyacc ${btyacc_src})
set_property(TARGET btyacc PROPERTY C_STANDARD 17)
set_property(TARGET btyacc PROPERTY C_STANDARD_REQUIRED OFF)
set_property(TARGET btyacc PROPERTY C_EXTENSIONS ON)
project_group           (btyacc Extern)
set_output_directory    (btyacc . CURRENT_DIR)

endif() # if (NOT CMAKE_CROSSCOMPILING)

########################################
# EXECUTABLE cloop
########################################

file(GLOB cloop_src "extern/cloop/src/cloop/*.cpp" "extern/cloop/src/cloop/*.h")

add_executable          (cloop ${cloop_src})
project_group           (cloop Extern)

########################################
# subdirectories
########################################

add_subdirectory("examples")
add_subdirectory("src")

################################################################################
