
cmake_minimum_required(VERSION 3.10.0)

project("fb-extern" C CXX)
set(CMAKE_CXX_STANDARD 20)

########################################
# icu
########################################

include(../builds/cmake/BuildICU.cmake)

    if (APPLE OR REBUILD_ICU)
        build_icu4c(${REQUIRED_ICU_VER})        
        set(ICU_DIR_include "${ICU_DIR_include}" PARENT_SCOPE)
        set(ICU_DIR_distr   "${ICU_DIR_distr}"   PARENT_SCOPE)
        set(ICU_VERSION     "${ISU_VER_MAJOR}"   PARENT_SCOPE)
    endif()

if (UNIX)
      set(ICU_INCLUDE_DIR "${ICU_DIR_include}")

    if (NOT ICU_INCLUDE_DIR)
        find_path(ICU_INCLUDE_DIR unicode/ucnv.h
            PATHS
                /usr/include
                /usr/include/x86_64-linux-gnu
                /usr/include/aarch64-linux-gnu
                /usr/local/include
                /opt/local/include
                /usr/local/opt/icu4c/include
            CMAKE_FIND_ROOT_PATH_BOTH
        )
        if ("${ICU_INCLUDE_DIR}" STREQUAL "ICU_INCLUDE_DIR-NOTFOUND")
            message(FATAL_ERROR "ICU headers not found!")
        endif()

      set(ICU_INCLUDE_DIR "${ICU_INCLUDE_DIR}" PARENT_SCOPE)

    endif()
    if (CMAKE_CROSSCOMPILING)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ICU_INCLUDE_DIR}/unicode
            ${CMAKE_CURRENT_BINARY_DIR}/unicode
        )
        set(ICU_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    set(ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIR})
    include_directories(${ICU_INCLUDE_DIRS})
endif()

########################################
# LIBRARY editline
########################################
if (LINUX)

    set(EDITLINE_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/editline")
    set(EDITLINE_BIN_DIR  "${CMAKE_CURRENT_BINARY_DIR}/editline")

    file(REMOVE_RECURSE ${EDITLINE_BIN_DIR})
         make_directory(${EDITLINE_BIN_DIR})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${EDITLINE_SRC_DIR} ${EDITLINE_BIN_DIR})

    crlf_dos_to_unix(${EDITLINE_BIN_DIR})

    execute_process(COMMAND chmod +x ./configure  WORKING_DIRECTORY ${EDITLINE_BIN_DIR})
                
    replace_in_file("../../../gen/extern/editline/libedit.a" "../../lib${LIB_readline}.a" "${EDITLINE_BIN_DIR}/src/Makefile.in")

    add_custom_target  (editline-static SOURCES ${EDITLINE_SRC_DIR})
    project_group      (editline-static Extern)

    add_custom_command(TARGET editline-static PRE_BUILD COMMAND ./configure --enable-static --disable-shared --enable-widec WORKING_DIRECTORY ${EDITLINE_BIN_DIR})
    add_custom_command(TARGET editline-static PRE_BUILD COMMAND make all  WORKING_DIRECTORY ${EDITLINE_BIN_DIR})

endif()

########################################
# LIBRARY cds
########################################

if (USE_CDS)

    set(LIB_cds  "fb-cds-static")
    set(LIB_cds  ${LIB_cds} PARENT_SCOPE)

    file(GLOB cds_src 
        libcds/src/*.cpp
    )  
    add_library                 (${LIB_cds} STATIC ${cds_src})        
    project_group               (${LIB_cds} Extern)

    message("== use cds library from ${CMAKE_CURRENT_BINARY_DIR}/libcds/bin/")

endif()

########################################
# LIBRARY re2
########################################

file( GLOB re2_src  
 "re2/re2/*.cc" 
 "re2/re2/*.h"
 "re2/util/rune.cc"  
 "re2/util/strutil.cc"
)
add_library                 (re2 ${re2_src})
target_compile_definitions  (re2 PRIVATE NOMINMAX)
project_group               (re2 Extern)

# if(BUILD_LIBTOM)

########################################
# LIBRARY libtommath
########################################

set(LIB_tommath  "tommath")
set(LIB_tommath  ${LIB_tommath} PARENT_SCOPE)

    file(GLOB libtommath_src 
    libtommath/*.c 
    libtommath/*.h
    )

if (WITH_LIBTOM_STATIC)
    add_library                   (${LIB_tommath} ${libtommath_src})
else()    
    add_library                   (${LIB_tommath} SHARED ${libtommath_src})
    target_compile_options        (${LIB_tommath} PRIVATE "-fvisibility=default") 
endif()    
    set_target_properties         (${LIB_tommath} PROPERTIES CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${output_dir})
    project_group                 (${LIB_tommath} Extern)

    if (WIN32 AND USE_TTMATH_MASM)

        if (CLANG)
            set(LIB_builtins_Clang "clang_rt.builtins-x86_64.lib")
        endif()

        add_custom_command(TARGET ${LIB_tommath} PRE_BUILD  
        COMMAND "${CMAKE_VS_MSASAM_COMMAND}" -c -Fo
                "${CMAKE_CURRENT_BINARY_DIR}/ttmathuint_x86_64_msvc.obj"
                "${CMAKE_CURRENT_SOURCE_DIR}/ttmath/ttmathuint_x86_64_msvc.asm"
        )
        set(libtommath_x86_64_masm_obj  "${CMAKE_CURRENT_BINARY_DIR}/ttmathuint_x86_64_msvc.obj")

        target_link_libraries     (${LIB_tommath} ${LIB_builtins_Clang} ${libtommath_x86_64_masm_obj})
        target_link_directories   (${LIB_tommath} PRIVATE ${clang_rt_lib_dir_win})
    
        message("use TTMATH MASM")

    endif()                             

########################################
# LIBRARY libtomcrypt
########################################

set(LIB_tomcrypt "tomcrypt")
set(LIB_tomcrypt ${LIB_tomcrypt} PARENT_SCOPE)

# if (WITH_LIBTOM_STATIC)

    file(GLOB_RECURSE libtomcrypt_src 
    "libtomcrypt/src/*"
    )

    list(FILTER libtomcrypt_src EXCLUDE REGEX ".*aes_tab.c")
    list(FILTER libtomcrypt_src EXCLUDE REGEX ".*whirltab.c")
    list(FILTER libtomcrypt_src EXCLUDE REGEX ".*sober128tab.c")

if (WITH_LIBTOM_STATIC)
    add_library                 (${LIB_tomcrypt} ${libtomcrypt_src})
else()    
    add_library                 (${LIB_tomcrypt} SHARED ${libtomcrypt_src})
    target_compile_options      (${LIB_tomcrypt} PRIVATE "-fvisibility=default") 
endif()    
    set_target_properties       (${LIB_tomcrypt} PROPERTIES CMAKE_ARHIVE_OUTPUT_DIRECTORY ${output_dir})
    target_compile_definitions  (${LIB_tomcrypt} PRIVATE LTC_NO_ROLC LTC_SOURCE)
    target_link_libraries       (${LIB_tomcrypt} ${LIB_tommath})
    project_group               (${LIB_tomcrypt} Extern)

# endif()

########################################
# LIBRARY decNumber
########################################

file(GLOB_RECURSE decNumber_src "decNumber/*.c" "decNumber/*.h")
list(FILTER decNumber_src EXCLUDE REGEX ".*decBasic.c")
list(FILTER decNumber_src EXCLUDE REGEX ".*decCommon.c")

add_library                 (decNumber ${decNumber_src})
project_group               (decNumber Extern)

########################################
# EXECUTABLE btyacc
########################################

file(GLOB btyacc_src "btyacc/*.c" "btyacc/*.h")

if (NOT CMAKE_CROSSCOMPILING)
    add_executable          (btyacc ${btyacc_src})
    project_group           (btyacc Extern)
    set_target_properties   (btyacc PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${NATIVE_BUILD_DIR}/src")
    set_target_properties   (btyacc PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${NATIVE_BUILD_DIR}/src")
    set_target_properties   (btyacc PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${NATIVE_BUILD_DIR}/src")
endif() 


########################################
# EXECUTABLE cloop
########################################

file(GLOB cloop_src 
    "cloop/src/cloop/*.cpp" 
    "cloop/src/cloop/*.h"
)

add_executable          (cloop ${cloop_src})
project_group           (cloop Extern)

