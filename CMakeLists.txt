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

cmake_minimum_required(VERSION 3.10.0)
project("firebird" C CXX)
set(CMAKE_CXX_STANDARD 20)
cmake_policy(SET CMP0077 NEW)


option(WITH_LIBTOM_STATIC   "on POSIX build and link libtommath and libtomcript as static"  OFF)
option(USE_TTMATH_MASM      "use assemler func in libtommath on win-arm-64 (need add ml64.exe in PATH)" OFF)
option(REBUILD_ICU          "download from git and rebuild ICU library (on win need add MSBuild.exe in PATH)"  OFF)


if (WIN32)
    set(os_type_ID win)
elseif(APPLE)
    set(os_type_ID osx)
elseif(LINUX)
    set(os_type_ID lin)
elseif(ANDROID)
    set(os_type_ID and)
elseif(UNIX)
    set(os_type_ID nix)
else()
    set(os_type_ID non)
endif()

if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(CPU_IS_64 ON) 
    set(cpu_bits_ID 64)
    message("== CPU is 64 bit")
else(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    set(CPU_IS_32 ON) 
    set(cpu_bits_ID 32)
    message("== CPU is 32 bit")
endif()



if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86" ) 

    set(CPU_IS_X86 ON) 
    set(I386 1)

    if (${CPU_IS_64})
        set(AMD64 1)
        add_definitions(-DRC_ARH_x64)   
        message("== build for amd64 system" )
    else(${CPU_IS_32})
        add_definitions(-DRC_ARH_x32)   
        message("== build for x86-32 system" )
    endif()

    set(cpu_type_ID x86)

elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "ARM" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch")

    set(CPU_IS_ARM ON) 

    set(cpu_type_ID arm)

    if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(ARM64 1)
        unset(ARM)
        add_definitions(-DRC_ARH_arm64)   
        add_definitions(-DARM64)   
        add_definitions(-DCPU_TYPE=arm64)  
        message("== build for  arm64 system" )
    elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
        set(ARM 1) 
        add_definitions(-DRC_ARH_arm32)   
        add_definitions(-DARM)   
        add_definitions(-DCPU_TYPE=arm)   
        message("== build for  arm32 system" )
    endif()

endif()

set(platform_ID "${os_type_ID}-${cpu_type_ID}-${cpu_bits_ID}")


# In-source builds are not possible and so disabled.
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
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

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/builds/cmake)

if (NATIVE_BUILD_DIR)
    get_filename_component(NATIVE_BUILD_DIR ${NATIVE_BUILD_DIR} ABSOLUTE)
else()
    set(NATIVE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake Targets")

################################################################################
#
# project settings
#
################################################################################



########################################
# compiler detect
########################################

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CLANG 1)
endif()

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

set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME})
set(boot_dir   ${CMAKE_CURRENT_BINARY_DIR}/src)
set(test_dir   ${NATIVE_BUILD_DIR}/tests)
set(distr_dir  ${NATIVE_BUILD_DIR}/distr)

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
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${output_dir}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir}/bin)
endif()

################################################################################
#
# packages
#
################################################################################



################################################################################
#
# include
#
################################################################################

include_directories("src/include")
include_directories("src/include/gen")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src/include")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src/include/gen")
include_directories("extern/editline/src/editline")
include_directories("extern/re2")
include_directories("extern/boost")
include_directories("extern/libtommath")
include_directories("extern/libtomcrypt/src/headers")
include_directories("extern/decNumber")
include_directories("extern/icu/include")
include_directories("extern/zlib")
include_directories("extern/libcds")


################################################################################
#
# configure
#
################################################################################

string(REPLACE "${CMAKE_SOURCE_DIR}" "" OWNER_RROJECTS_TREE "${CMAKE_CURRENT_SOURCE_DIR}" )

include(builds/cmake/BuildFunctions.cmake)
include(builds/cmake/Configure.cmake)

if (FREEBSD)
    # temporary
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
endif()

set(FB_PREFIX ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME})
set(FB_IPC_NAME     "FirebirdIPI")
set(FB_LOGFILENAME  "firebird.log")
set(FB_CONFILENAME  "firebird.conf")
set(FB_SERVICE_NAME "gds_db")
set(FB_SERVICE_PORT  3050)

if (WIN32)
    set(FB_PREFIX "c:\\\\Program Files\\\\Firebird\\\\")
    set(FB_IPC_NAME "FIREBIRD")
endif()

set(AUTOCONFIG_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/include/gen/autoconfig.h.in)
set(AUTOCONFIG ${CMAKE_CURRENT_BINARY_DIR}/src/include/gen/autoconfig.h)
configure_file(${AUTOCONFIG_SRC} ${AUTOCONFIG} @ONLY)

if (WIN32) # OR APPLE
    set(WITH_LIBTOM_STATIC ON)    
endif()

set(USE_CDS 1)

if (USE_CDS)
    add_definitions(-DCDS_BUILD_STATIC_LIB)
else()
    add_definitions("-DCDS_UNAVAILABLE")
endif()

if (HAVE_TOMCRYPT_H OR WITH_LIBTOM_STATIC)
    add_definitions("-DHAVE_TOMCRYPT")
endif()

if (NOT REQUIRED_ICU_VER)
    set(REQUIRED_ICU_VER "77.1") 
endif()


if (WIN32)

    string(FIND "${CMAKE_EXE_LINKER_FLAGS}" "/machine:x64" check_arch_ms)
    string(FIND "${CMAKE_EXE_LINKER_FLAGS}" "-m64"         check_arch_gnu)

    if (NOT ${check_arch_ms} EQUAL -1 OR ${check_arch_gnu} EQUAL -1)
        if (CPU_IS_ARM)
            set(arch "arm64")
        else()
            set(arch "x64")
        endif()
    else()
        if (CPU_IS_ARM)
            set(arch "arm32")
        else()
            set(arch "Win32")
        endif()
    endif()

    if (USE_TTMATH_MASM)
        message("== use TTMATH MASM")
    else()
        message("== not use TTMATH MASM")
        add_definitions("-DTTMATH_NOASM")
    endif()                             

endif()


################################################################################
#
# version
#
################################################################################

    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/jrd/build_no.h  build_no_h_content)
    string(REPLACE "\"" "" build_no_h_content ${build_no_h_content} )  
    string(REGEX MATCH "FB_MAJOR_VER ([0-9]*)" ver_major ${build_no_h_content})  
    string(REGEX MATCH "([0-9]+)" ver_major ${ver_major})  
    string(REGEX MATCH "FB_MINOR_VER ([0-9]*)" ver_minor ${build_no_h_content})
    string(REGEX MATCH "([0-9]+)" ver_minor ${ver_minor})  
    string(REGEX MATCH    "FB_REV_NO ([0-9]*)" ver_patch ${build_no_h_content})
    string(REGEX MATCH "([0-9]+)" ver_patch ${ver_patch})  
    string(REGEX MATCH  "FB_BUILD_NO ([0-9]*)" ver_build ${build_no_h_content})
    string(REGEX MATCH "([0-9]+)" ver_build ${ver_build})  

    set(PROJECT_VERSION_MAJOR ${ver_major})
    set(PROJECT_VERSION_MINOR ${ver_minor})
    set(PROJECT_VERSION_PATCH ${ver_patch})
    set(PROJECT_VERSION_BUILD ${ver_build})
    set(PROJECT_VERSION_FULL "${ver_major}.${ver_minor}.${ver_patch}.${ver_build}")

################################################################################
#
# compiler & linker settings
#
################################################################################

if (WIN32)
    set(OS_DIR win32)
    set(VERSION_RC ${CMAKE_CURRENT_SOURCE_DIR}/src/jrd/version.rc)

    add_definitions(-D_WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # not need ???       

    if (NOT CMAKE_VS_MSBUILD_COMMAND)
        set(CMAKE_VS_MSBUILD_COMMAND "MSBuild.exe")
    endif()
    if (NOT CMAKE_VS_MSASAM_COMMAND)
        set(CMAKE_VS_MSASAM_COMMAND  "ml64.exe")    
    endif()

    if (CLANG)
        # by default CMake with llvm-rc create file with .rc.res.res extention instade of .rc.res"    
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <DEFINES> -fo <OBJECT> <SOURCE>")

        if (CPU_IS_X86)
           add_compile_options("-mcrc32")
        endif()

        link_libraries("Bcrypt") 
    endif()

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4 -std=c++20")
endif()

if (UNIX)
    set(OS_DIR posix)

    if (NOT CPU_IS_ARM)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4")
    endif()

    if (NOT CMAKE_CROSSCOMPILING)
        # set(LIB_readline readline)
    endif()
    if (NOT FREEBSD)
        set(LIB_dl dl)
    endif()

endif()

if (NOT LINUX AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR IOS))
    if (NOT WIN32)
        set(LIB_iconv iconv)
    endif()
endif()

if (FREEBSD OR APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif()

if (APPLE)
    set(OS_DIR darwin)

    link_libraries("-framework Foundation")
    link_libraries("-framework Security")

    if (NOT CMAKE_CROSSCOMPILING)
        include_directories(/opt/local/include)
        link_directories(/opt/local/lib)
        link_directories(${ICU_DIR}/distr/lib)
        link_directories(/usr/local/opt/icu4c/lib)  
    endif()

    find_library(LIB_CoreFoundation CoreFoundation)
elseif (UNIX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

if (UNIX)

    add_compile_options("-fvisibility=hidden") 
    set(COMMON_FLAGS "${COMMON_FLAGS} -fPIC -fsigned-char")

    if (APPLE)
        set(LIB_readline edit)
    endif()

    if (LINUX)
        set(LIB_readline editline)
        set(LIB_tinfo   tinfo)
        set(COMMON_FLAGS "${COMMON_FLAGS} -DLINUX -pipe -fmessage-length=0")
        set(PROD_FLAGS   "${PROD_FLAGS} -O3") 
        set(DEV_FLAGS    "${DEV_FLAGS}  -ggdb -Wall -Wno-non-virtual-dtor -DDEV_BUILD")    
        
        if (CPU_IS_ARM AND CPU_IS_64)
            set(COMMON_FLAGS "${COMMON_FLAGS} -DARM64")
        endif()
        if (CPU_IS_ARM AND CPU_IS_32)
            set(COMMON_FLAGS "${COMMON_FLAGS} -DARM")
            set(DEV_FLAGS    "${DEV_FLAGS} -p")
        endif()

        if (CPU_IS_AMD AND CPU_IS_64)
            set(COMMON_FLAGS "${COMMON_FLAGS} -DAMD64 -DFB_SEND_FLAGS=MSG_NOSIGNAL")
        endif()
        if (CPU_IS_AMD AND CPU_IS_32)

        endif()
    endif()
endif()

set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG}     ${COMMON_FLAGS} ${DEV_FLAGS}" )
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   ${COMMON_FLAGS} ${PROD_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   ${COMMON_FLAGS} ${DEV_FLAGS}" )
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMMON_FLAGS} ${PROD_FLAGS}")

message("== CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE}")
message("== CMAKE_CXX_FLAGS_DEBUG   ${CMAKE_CXX_FLAGS_DEBUG}")

################################################################################
#
# pre-build
#
################################################################################

if (WIN32)
    # icu
    if (NOT REBUILD_ICU)
        
        message("== Extracting pre-built ICU")
        set(ICU_EXTRACT ${CMAKE_CURRENT_SOURCE_DIR}/extern/icu/icu_windows.zip CACHE STRING "ICU extraction variable")

        set(ICU_LIB_ARHIV ${CMAKE_CURRENT_SOURCE_DIR}/extern/icu/icu_windows.zip)
        set(ICU_DAT_ARHIV ${CMAKE_CURRENT_SOURCE_DIR}/extern/icu/icudt.zip)

        file(ARCHIVE_EXTRACT INPUT ${ICU_LIB_ARHIV} DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/extern/icu") 
        file(ARCHIVE_EXTRACT INPUT ${ICU_DAT_ARHIV} DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/extern/icu") 
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

    file(GLOB vers_src "${CMAKE_CURRENT_SOURCE_DIR}/builds/posix/*.vers")
    foreach(f ${vers_src})
        get_filename_component(name ${f} NAME)
        set(name "${CMAKE_CURRENT_BINARY_DIR}/builds/posix/${name}")

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

include(builds/cmake/BuildFunctions.cmake)

if (NOT CMAKE_CROSSCOMPILING)

create_boot_commands()
create_master_commands()

endif() # if (NOT CMAKE_CROSSCOMPILING)

crosscompile_prebuild_steps()

link_directories   ("${CMAKE_CURRENT_BINARY_DIR}/extern")
link_directories   ("${output_dir}")


########################################
# subdir extern 
########################################

add_subdirectory(extern)

include_directories(${ICU_DIR_include})
   link_directories(${ICU_DIR_distr})


########################################
# subdirectories
########################################

add_subdirectory("examples")
add_subdirectory("src")


################################################################################
#
# Dispaly inventory
#
################################################################################

include(builds/cmake/BuildFunctions.cmake)
display_inventory()
