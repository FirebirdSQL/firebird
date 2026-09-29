# SPDX-License-Identifier: Unlicense
#
# LibTomMath, a free open source portable number theoretic multiple-precision
# integer (MPI) library written entirely in C.
#

cmake_minimum_required(VERSION 3.10)

project(libtommath
    VERSION 1.3.0
    DESCRIPTION "A free open source portable number theoretic multiple-precision integer (MPI) library written entirely in C."
    HOMEPAGE_URL "https://www.libtom.net/LibTomMath"
    LANGUAGES C)

# package release version
# bump if re-releasing the same VERSION + patches
# set to 1 if releasing a new VERSION
set(PACKAGE_RELEASE_VERSION 1)

#-----------------------------------------------------------------------------
# Include cmake modules
#-----------------------------------------------------------------------------
include(GNUInstallDirs)
include(CheckIPOSupported)
include(CMakePackageConfigHelpers)
# default is "No tests"
option(BUILD_TESTING "" OFF)
include(CTest)
include(sources.cmake)

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build shared library and only the shared library if \"ON\", default is static" OFF)

#-----------------------------------------------------------------------------
# Add support for ccache if desired
#-----------------------------------------------------------------------------
find_program(CCACHE ccache)

if(CCACHE)
    option(ENABLE_CCACHE "Enable ccache." ON)
endif()

# use ccache if installed
if(CCACHE AND ENABLE_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
endif()

#-----------------------------------------------------------------------------
# Compose CFLAGS
#-----------------------------------------------------------------------------

# Some information ported from makefile_include.mk


if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE "Release")
endif()

# We only differentiate between MSVC and GCC-compatible compilers
if(MSVC)
    set(LTM_C_FLAGS -W3)
elseif(WATCOM)
    set(LTM_C_FLAGS -fo=.obj -oaxt -3r -w3)
else()
    set(LTM_C_FLAGS -Wall -Wsign-compare -Wextra -Wshadow
                    -Wdeclaration-after-statement -Wbad-function-cast -Wcast-align
                    -Wstrict-prototypes -Wpointer-arith -Wsystem-headers)
    set(CMAKE_C_FLAGS_DEBUG "-g3")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -funroll-loops -fomit-frame-pointer")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g3 -O2")
    set(CMAKE_C_FLAGS_MINSIZEREL "-Os")
endif()

# What compiler do we have and what are their...uhm... peculiarities
if(CMAKE_C_COMPILER_ID MATCHES "(C|c?)lang")
    list(APPEND LTM_C_FLAGS -Wno-typedef-redefinition -Wno-tautological-compare -Wno-builtin-requires-header)
    # Clang requires at least '-O1' for dead code elimination
    set(CMAKE_C_FLAGS_DEBUG "-O1 ${CMAKE_C_FLAGS_DEBUG}")
endif()
if(CMAKE_C_COMPILER MATCHES "mingw")
    list(APPEND LTM_C_FLAGS -Wno-shadow -Wno-expansion-to-defined -Wno-declaration-after-statement -Wno-bad-function-cast)
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    list(APPEND LTM_C_FLAGS -Wno-nullability-completeness)
endif()
if(CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
    list(APPEND LTM_C_FLAGS -no-undefined)
endif()

# TODO: coverage (lgcov)

# If the user set the environment variables at generate-time, append them
# in order to allow overriding our defaults.
# ${LTM_CFLAGS} means the user passed it via sth like:
# $ cmake -DLTM_CFLAGS="foo"
list(APPEND LTM_C_FLAGS ${LTM_CFLAGS})
list(APPEND LTM_LD_FLAGS ${LTM_LDFLAGS})

#-----------------------------------------------------------------------------
# library target
#-----------------------------------------------------------------------------
add_library(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)

target_compile_options(${PROJECT_NAME} BEFORE PRIVATE
    ${LTM_C_FLAGS}
)
target_link_options(${PROJECT_NAME} BEFORE PRIVATE
    ${LTM_LD_FLAGS}
)

set(PUBLIC_HEADERS tommath.h)
set(C89 False CACHE BOOL "(Usually maintained automatically) Enable when the library is in c89 mode to package the correct header files on install")
if(C89)
    list(APPEND PUBLIC_HEADERS tommath_c89.h)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME tommath
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
)

option(COMPILE_LTO "Build with LTO enabled")
if(COMPILE_LTO)
    check_ipo_supported(RESULT COMPILER_SUPPORTS_LTO)
    if(COMPILER_SUPPORTS_LTO)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(SEND_ERROR "This compiler does not support LTO. Reconfigure ${PROJECT_NAME} with -DCOMPILE_LTO=OFF.")
    endif()
endif()

#-----------------------------------------------------------------------------
# demo target
#-----------------------------------------------------------------------------

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(demo)
endif()

#-----------------------------------------------------------------------------
# Install/export targets and files
#-----------------------------------------------------------------------------
set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
set(PROJECT_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake")
set(PROJECT_CONFIG_FILE "${PROJECT_NAME}-config.cmake")
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")

install(TARGETS ${PROJECT_NAME}
    EXPORT ${TARGETS_EXPORT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install libtommath.pc for pkg-config if we build a shared library
if(BUILD_SHARED_LIBS)
    # Let the user override the default directory of the pkg-config file (usually this shouldn't be required to be changed)
    set(CMAKE_INSTALL_PKGCONFIGDIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig" CACHE PATH "Folder where to install .pc files")

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
        @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
        DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR}
    )
endif()

# generate package version file
write_basic_package_version_file(
    ${PROJECT_VERSION_FILE}
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# install version file
install(FILES ${PROJECT_VERSION_FILE}
        DESTINATION ${CONFIG_INSTALL_DIR}
)

# build directory package config
export(EXPORT ${TARGETS_EXPORT_NAME}
       FILE ${PROJECT_CONFIG_FILE}
)

# installed package config
install(EXPORT ${TARGETS_EXPORT_NAME}
        DESTINATION ${CONFIG_INSTALL_DIR}
        FILE ${PROJECT_CONFIG_FILE}
)

# add to CMake registry
export(PACKAGE ${PROJECT_NAME})

#---------------------------------------------------------------------------------------
# Create release packages
#---------------------------------------------------------------------------------------

# determine distribution and architecture
find_program(LSB_RELEASE lsb_release)
find_program(SYSCTL sysctl)
find_program(UNAME uname)

if(UNAME)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE MACHINE_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(SYSCTL)
    execute_process(COMMAND sysctl -b hw.machine_arch OUTPUT_VARIABLE MACHINE_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
    string(TOLOWER ${CMAKE_SYSTEM_NAME} MACHINE_ARCH)
endif()

if(LSB_RELEASE)
    execute_process(COMMAND lsb_release -si OUTPUT_VARIABLE LINUX_DISTRO OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND lsb_release -sc OUTPUT_VARIABLE LINUX_DISTRO_CODENAME OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND lsb_release -sr OUTPUT_VARIABLE LINUX_DISTRO_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

    string(TOLOWER ${LINUX_DISTRO} LINUX_DISTRO)
    if(LINUX_DISTRO_CODENAME STREQUAL "n/a")
        set(DISTRO_PACK_PATH ${LINUX_DISTRO}/${LINUX_DISTRO_VERSION}/)
    else()
        set(DISTRO_PACK_PATH ${LINUX_DISTRO}/${LINUX_DISTRO_CODENAME}/)
    endif()
else()
    set(DISTRO_PACK_PATH ${CMAKE_SYSTEM_NAME}/)
endif()

# make sure untagged versions get a different package name
execute_process(COMMAND git describe --exact-match --tags ERROR_QUIET RESULT_VARIABLE REPO_HAS_TAG)
if(REPO_HAS_TAG EQUAL 0)
    set(PACKAGE_NAME_SUFFIX "")
else()
    set(PACKAGE_NAME_SUFFIX "-git")
    message(STATUS "Use -git suffix")
endif()

# default CPack generators
set(CPACK_GENERATOR TGZ STGZ)

# extra CPack generators
if(LINUX_DISTRO STREQUAL "debian" OR LINUX_DISTRO STREQUAL "ubuntu" OR LINUX_DISTRO STREQUAL "linuxmint")
    list(APPEND CPACK_GENERATOR DEB)
elseif(LINUX_DISTRO STREQUAL "fedora" OR LINUX_DISTRO STREQUAL "opensuse" OR LINUX_DISTRO STREQUAL "centos")
    list(APPEND CPACK_GENERATOR RPM)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    list(APPEND CPACK_GENERATOR FREEBSD)
endif()

set(LTM_DEBIAN_SHARED_PACKAGE_NAME "${PROJECT_NAME}${PACKAGE_NAME_SUFFIX}${PROJECT_VERSION_MAJOR}")

# general CPack config
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/packages/${DISTRO_PACK_PATH})
message(STATUS "CPack: packages will be generated under ${CPACK_PACKAGE_DIRECTORY}")
if(BUILD_SHARED_LIBS)
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}${PROJECT_VERSION_MAJOR}")
    set(CPACK_DEBIAN_PACKAGE_NAME "${LTM_DEBIAN_SHARED_PACKAGE_NAME}")
else()
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}-devel")
    set(CPACK_DEBIAN_LIBRARIES_PACKAGE_NAME "${PROJECT_NAME}${PACKAGE_NAME_SUFFIX}-dev")
endif()
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LibTomMath")
set(CPACK_PACKAGE_VENDOR "libtom projects")
set(CPACK_PACKAGE_CONTACT "libtom@googlegroups.com")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(PACKAGE_NAME_TRAILER ${CPACK_PACKAGE_VERSION}-${PACKAGE_RELEASE_VERSION}_${MACHINE_ARCH})
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${PACKAGE_NAME_TRAILER})

# deb specific CPack config
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_PACKAGE_RELEASE ${PACKAGE_RELEASE_VERSION})
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
if(BUILD_SHARED_LIBS)
    set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
else()
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS ${LTM_DEBIAN_SHARED_PACKAGE_NAME})
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_ALL Libraries)
endif()

# rpm specific CPack config
set(CPACK_RPM_PACKAGE_RELEASE ${PACKAGE_RELEASE_VERSION})
set(CPACK_RPM_PACKAGE_ARCHITECTURE ${MACHINE_ARCH})
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}")
set(CPACK_RPM_PACKAGE_LICENSE "The Unlicense")

# FreeBSD specific CPack config
set(CPACK_FREEBSD_PACKAGE_MAINTAINER "gahr@FreeBSD.org")
set(CPACK_FREEBSD_PACKAGE_ORIGIN "math/libtommath")
set(CPACK_FREEBSD_PACKAGE_CATEGORIES "math")

include(CPack)
