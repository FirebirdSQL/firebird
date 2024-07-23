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

cmake_minimum_required(VERSION 3.15)

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
elseif (MINGW)
    set(output_dir ${CMAKE_BINARY_DIR})
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
elseif (UNIX OR MINGW)
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

if (FREEBSD OR MINGW)
    # Build-time temporary installation path
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})
endif()

set(FB_PREFIX ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME})
set(FB_IPC_NAME "FirebirdIPI")
set(FB_LOGFILENAME "firebird.log")
set(FB_SERVICE_NAME "gds_db")
set(FB_SERVICE_PORT 3050)

if (WIN32)
    if (MSVC)
        set(FB_PREFIX "c:\\\\Program Files\\\\Firebird\\\\")
    else()
        set(FB_PREFIX ${CMAKE_INSTALL_PREFIX})
    endif()
    set(FB_IPC_NAME "FIREBIRD")
endif()

if (MINGW)
    set(FB_BINDIR "${FB_PREFIX}/bin")
    set(FB_SBINDIR "${FB_PREFIX}/bin")
    set(FB_INCDIR "${FB_PREFIX}/include")
    set(FB_LIBDIR "${FB_PREFIX}/lib")
    set(FB_CONFDIR "${FB_PREFIX}/etc/firebird")
    set(FB_GUARDDIR "${FB_PREFIX}/etc/firebird")
    set(FB_LOGDIR "${FB_PREFIX}/etc/firebird")
    set(FB_MSGDIR "${FB_PREFIX}/etc/firebird")
    set(FB_SECDBDIR "${FB_PREFIX}/etc/firebird")
    set(FB_DOCDIR "${FB_PREFIX}/share/firebird/doc")
    set(FB_INTLDIR "${FB_PREFIX}/share/firebird/intl")
    set(FB_MISCDIR "${FB_PREFIX}/share/firebird/misc")
    set(FB_PLUGDIR "${FB_PREFIX}/share/firebird/plugins")
    set(FB_SAMPLEDBDIR "${FB_PREFIX}/share/firebird/examples/empbuild")
    set(FB_SAMPLEDIR "${FB_PREFIX}/share/firebird/examples")
    set(FB_TZDATADIR "${FB_PREFIX}/share/firebird/tzdata")
endif()

set(AUTOCONFIG_SRC ${CMAKE_SOURCE_DIR}/src/include/gen/autoconfig.h.in)
set(AUTOCONFIG ${CMAKE_BINARY_DIR}/src/include/gen/autoconfig.h)
configure_file(${AUTOCONFIG_SRC} ${AUTOCONFIG} @ONLY)

if (MINGW)
    # Set the final installation path
    set(FB_PREFIX ${FB_INSTALL_PREFIX})

    set(FBCONFIG_SRC ${CMAKE_SOURCE_DIR}/builds/install/arch-specific/mingw/fb_config.in)
    set(FBCONFIG ${CMAKE_BINARY_DIR}/fb_config)
    configure_file(${FBCONFIG_SRC} ${FBCONFIG} @ONLY)
endif()

################################################################################
#
# compiler & linker settings
#
################################################################################

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-DDEV_BUILD)
endif()

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

    set(LIB_ws2_32 ws2_32)
    set(LIB_comctl32 comctl32)
    set(LIB_mpr mpr)
    set(LIB_version version)
endif(WIN32)

if (MINGW)
    # Clear unix-style prefixes
    set(CMAKE_SHARED_LIBRARY_PREFIX)
    set(CMAKE_SHARED_MODULE_PREFIX)
    set(CMAKE_STATIC_LIBRARY_PREFIX)

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -DDEBUG")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -DNDEBUG")
    endif()

    add_definitions(-DWIN_NT -DMINGW -DMINGW_HAS_SECURE_API -DTTMATH_NOASM)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-shift-count-overflow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4 -std=c++17")

    # Suppress myriad of warnings    
    set(cxx_flags "")
    list(APPEND cxx_flags
        -Wundef
        -Wno-format
        -Wno-deprecated-declarations
        -Wno-ignored-attributes
        -Wno-invalid-offsetof
        -Wno-long-long
        -Wno-non-virtual-dtor
        -Wno-parentheses
        -Wno-shift-count-overflow
        -Wno-sign-compare
        -Wno-switch
        -Wno-unused-variable
        -Wno-write-strings
    )
    if (CLANG)
        list(APPEND cxx_flags
            -Wno-varargs
            -Wno-constant-conversion
            -Wno-inconsistent-missing-override
            -Wno-tautological-constant-out-of-range-compare
            -Wno-c++11-narrowing
        )
    else()
        list(APPEND cxx_flags
            -Wno-class-memaccess
            -Wno-overflow
            -Wno-return-local-addr
            -Wno-stringop-overflow
            -Wno-stringop-overread
            -Wno-narrowing
        )
    endif()
    string(REPLACE ";" " " cxx_flags "${cxx_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxx_flags}")

    set(LIB_iconv iconv)
    set(LIB_re2 re2)
endif()

if (UNIX)
    set(OS_DIR posix)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -msse4 -std=c++17")

    set(FB_INTERNAL_TOMMATH ON)
    set(FB_INTERNAL_EDITLINE ON)

    set(LIB_re2 re2)
    if (NOT CMAKE_CROSSCOMPILING)
        set(LIB_editline editline)
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

if (MSVC)
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

endif() # (NOT CMAKE_CROSSCOMPILING)

crosscompile_prebuild_steps()

if (FB_INTERNAL_TOMMATH)
    include_directories("extern/libtommath")
    include_directories("extern/libtomcrypt/src/headers")
    set(LIB_tommath libtommath)
    set(LIB_tomcrypt libtomcrypt)
else()
    set(LIB_tommath tommath)
    set(LIB_tomcrypt tomcrypt)
endif()

if (UNIX)
    if (FB_INTERNAL_EDITLINE)
        include_directories("extern/editline")
    endif()
endif()

include_directories("extern/decNumber")
include_directories("extern/libcds")
include_directories("extern/re2")

if (MSVC OR ANDROID)
    include_directories("extern/icu/include")
    include_directories("extern/zlib")
endif()

include_directories("src/include")
include_directories("src/include/gen")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src/include")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src/include/gen")


if (FB_INTERNAL_TOMMATH)
########################################
# LIBRARY libtommath
########################################

file(GLOB libtommath_src "extern/libtommath/*.c" "extern/libtommath/*.h")

add_library                 (libtommath ${libtommath_src})
project_group               (libtommath Extern)


########################################
# LIBRARY libtomcrypt
########################################

file(GLOB_RECURSE libtomcrypt_src "extern/libtomcrypt/src/*")
list(FILTER libtomcrypt_src EXCLUDE REGEX ".*aes_tab.c")
list(FILTER libtomcrypt_src EXCLUDE REGEX ".*whirltab.c")
list(FILTER libtomcrypt_src EXCLUDE REGEX ".*sober128tab.c")

add_library                 (libtomcrypt ${libtomcrypt_src})
target_compile_definitions  (libtomcrypt PRIVATE LTC_NO_ROLC LTC_SOURCE)
project_group               (libtomcrypt Extern)

endif() # (FB_INTERNAL_TOMMATH)


########################################
# LIBRARY decNumber
########################################

file(GLOB_RECURSE decNumber_src "extern/decNumber/*.c" "extern/decNumber/*.h")
list(FILTER decNumber_src EXCLUDE REGEX ".*decBasic.c")
list(FILTER decNumber_src EXCLUDE REGEX ".*decCommon.c")

add_library                 (decNumber ${decNumber_src})
project_group               (decNumber Extern)


if (FB_INTERNAL_EDITLINE)
########################################
# LIBRARY editline
########################################

file(GLOB editline_src "extern/editline/src/*.c" "extern/editline/src/*.h")

add_library                 (editline ${editline_src})
project_group               (editline Extern)

endif() # (FB_INTERNAL_EDITLINE)


########################################
# LIBRARY libcds
########################################

file(GLOB_RECURSE libcds_src "extern/libcds/src/*.cpp")

add_library                 (libcds ${libcds_src})
target_compile_definitions  (libcds PUBLIC CDS_BUILD_STATIC_LIB)
project_group               (libcds Extern)


########################################
# EXECUTABLE btyacc
########################################

file(GLOB btyacc_src "extern/btyacc/*.c" "extern/btyacc/*.h")

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (btyacc ${btyacc_src})
set_output_directory        (btyacc src CURRENT_DIR)
project_group               (btyacc Extern)

endif() # (NOT CMAKE_CROSSCOMPILING)

########################################
# EXECUTABLE cloop
########################################

file(GLOB cloop_src "extern/cloop/src/cloop/*.cpp" "extern/cloop/src/cloop/*.h")

add_executable              (cloop ${cloop_src})
set_output_directory        (cloop src CURRENT_DIR)
project_group               (cloop Extern)

########################################
# subdirectories
########################################

add_subdirectory("extern/re2")
add_subdirectory("examples")
add_subdirectory("src")

################################################################################
