#
# firebird (trunk)
#
#  This file has following organization:
#  1. preprocess
#  2. custom build steps (file generators)
#  3. libraries
#  4. shared libraries
#  5. executables
#  6. subdirectories
#  7. copy other files to output dir (docs, includes, ...)
#

include(SourceGroups)

set_generated_directory()

################################################################################
#
# PREPROCESS
#
################################################################################

########################################
# PREPROCESS epp boot and master files
########################################

set(epp_boot_internal_files
    burp/backup.epp
    burp/restore.epp
    burp/OdsDetection.epp
)
set(epp_boot_ocxx_files
    isql/extract.epp
    isql/isql.epp
    isql/show.epp
)
set(epp_boot_files
    alice/alice_meta.epp
    gpre/std/gpre_meta.epp
    utilities/stats.epp
    utilities/gstat/dba.epp
)
set(epp_boot_gds_files
    dsql/metd.epp
    dsql/DdlNodes.epp
    dsql/PackageNodes.epp
    jrd/dfw.epp
    jrd/dpm.epp
    jrd/dyn_util.epp
    jrd/fun.epp
    jrd/grant.epp
    jrd/ini.epp
    jrd/met.epp
    jrd/scl.epp
    jrd/Function.epp
)
set(epp_master_files
    auth/SecurityDatabase/LegacyManagement.epp
)

if (NOT CMAKE_CROSSCOMPILING)

epp_process(boot    epp_boot_internal_files ${GPRE_BOOT_CMD} -lang_internal -n -ids -ocxx -m)
epp_process(boot    epp_boot_ocxx_files     ${GPRE_BOOT_CMD} -lang_internal -n -ids -ocxx)
epp_process(boot    epp_boot_files          ${GPRE_BOOT_CMD} -n -m)
epp_process(boot    epp_boot_gds_files      ${GPRE_BOOT_CMD} -n -ids -gds_cxx)

epp_process(master  epp_boot_internal_files ${BOOT_GPRE_CMD} -n -m -ids -ocxx -m)
epp_process(master  epp_boot_ocxx_files     ${BOOT_GPRE_CMD} -n -ids -ocxx)
epp_process(master  epp_boot_files          ${BOOT_GPRE_CMD} -n -m)
epp_process(master  epp_boot_gds_files      ${BOOT_GPRE_CMD} -n -ids -gds_cxx)
epp_process(master  epp_master_files        ${GPRE_CMD} -n -m)


################################################################################
#
# CUSTOM BUILD STEPS
#
################################################################################

########################################
# BUILD STEP databases
########################################

add_custom_command(
    OUTPUT security.fdb
    DEPENDS
        boot_engine
        boot_isql
        boot_gfix
        ${CMAKE_CURRENT_SOURCE_DIR}/dbs/security.sql
    VERBATIM
    #
    COMMAND ${CMAKE_COMMAND} -E remove security.fdb
    COMMAND ${CMAKE_COMMAND} -E remove security.tmp
    COMMAND ${CMAKE_COMMAND} -E echo "create database 'security.tmp';" > create_db_security.sql
    COMMAND ${BOOT_ISQL_CMD} -q -i create_db_security.sql
    COMMAND ${BOOT_GFIX_CMD} -write async security.tmp
    COMMAND ${BOOT_ISQL_CMD} -q security.tmp -i ${CMAKE_CURRENT_SOURCE_DIR}/dbs/security.sql
    COMMAND ${CMAKE_COMMAND} -E copy_if_different security.tmp security.fdb
    COMMAND ${BOOT_GFIX_CMD} -write sync security.fdb
)
add_custom_command(
    OUTPUT metadata.fdb
    DEPENDS
        boot_engine
        boot_isql
        boot_gfix
    VERBATIM
    #
    COMMAND ${CMAKE_COMMAND} -E remove metadata.fdb
    COMMAND ${CMAKE_COMMAND} -E remove metadata.tmp
    COMMAND ${CMAKE_COMMAND} -E echo "create database 'metadata.tmp';" > create_db_metadata.sql
    COMMAND ${BOOT_ISQL_CMD} -q -i create_db_metadata.sql
    COMMAND ${BOOT_GFIX_CMD} -sql_dialect 1 metadata.tmp
    COMMAND ${BOOT_GFIX_CMD} -mode read_only metadata.tmp
    COMMAND ${CMAKE_COMMAND} -E copy_if_different metadata.tmp metadata.fdb
)
add_custom_target(databases
    DEPENDS
        metadata.fdb
        security.fdb
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/dbs/security.sql
)
add_dependencies_cc(databases messages)
project_group(databases "Boot/Custom build steps")

if (MINGW)
    add_custom_command(TARGET databases POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different metadata.fdb ${output_dir}/etc/firebird/metadata.fdb
        COMMAND ${CMAKE_COMMAND} -E copy_if_different security.fdb ${output_dir}/etc/firebird/security.fdb
    )
endif()


########################################
# BUILD STEP messages
########################################

add_custom_command(
    OUTPUT firebird.msg
    DEPENDS
        build_msg
        ib_util
    #
    COMMAND ${CMAKE_COMMAND} -E remove firebird.msg
    COMMAND ${BUILD_MSG_CMD} -F firebird.msg -C ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/impl/iberror_c.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different firebird.msg ${output_dir}/firebird.msg
)
add_custom_target(messages DEPENDS firebird.msg)
project_group(messages "Boot/Custom build steps")

if (MINGW)
    add_custom_command(TARGET messages POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}/etc/firebird
        COMMAND ${CMAKE_COMMAND} -E copy_if_different firebird.msg ${output_dir}/etc/firebird/firebird.msg
    )
endif()


########################################
# BUILD_STEP parse
########################################

set(parse_src
    ${CMAKE_CURRENT_SOURCE_DIR}/dsql/parse.y
    ${CMAKE_CURRENT_SOURCE_DIR}/dsql/btyacc_fb.ske
)
add_custom_command(
    OUTPUT y_tab.h y_tab.c
    DEPENDS
        btyacc
        ${parse_src}
    COMMAND sed -n "/%type .*/p" ${CMAKE_CURRENT_SOURCE_DIR}/dsql/parse.y > types.y
    COMMAND sed "s/%type .*//" ${CMAKE_CURRENT_SOURCE_DIR}/dsql/parse.y > y.y
    COMMAND btyacc -l -d -S ${CMAKE_CURRENT_SOURCE_DIR}/dsql/btyacc_fb.ske y.y

    COMMAND sed -i "s/#define \\([A-Z].*\\)/#define TOK_\\1/g" y_tab.h
    COMMAND sed -i "s/#define TOK_YY\\(.*\\)/#define YY\\1/g" y_tab.h

    COMMAND ${CMAKE_COMMAND} -E copy_if_different y_tab.h include/gen/parse.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different y_tab.c dsql/parse.cpp
    COMMENT "Generating parse.cpp, parse.h"
    VERBATIM
)
add_custom_target(parse
    DEPENDS y_tab.h y_tab.c
    SOURCES ${parse_src} y_tab.h y_tab.c
)
project_group(parse "Boot/Custom build steps")
set_source_files_properties(dsql/parse.cpp include/gen/parse.h PROPERTIES GENERATED TRUE)


########################################
# BUILD_STEP UpdateCloopInterfaces
########################################

add_custom_command(
    OUTPUT ${GENERATED_DIR}/include/firebird/IdlFbInterfaces.h
    DEPENDS
        cloop
        ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/FirebirdInterface.idl
    COMMAND cloop
            ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/FirebirdInterface.idl
            c++
            ${GENERATED_DIR}/include/firebird/IdlFbInterfaces.h
            IDL_FB_INTERFACES_H Firebird I
    COMMENT "Updating cloop interfaces..."
)
add_custom_target(UpdateCloopInterfaces
    DEPENDS ${GENERATED_DIR}/include/firebird/IdlFbInterfaces.h
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/FirebirdInterface.idl
)
project_group(UpdateCloopInterfaces "Boot/Custom build steps")

endif() # (NOT CMAKE_CROSSCOMPILING)


################################################################################
#
# LIBRARIES
#
################################################################################

########################################
# LIBRARY boot_alice
########################################

file(GLOB alice_src "alice/*.cpp" "alice/*.h")

set(alice_generated_src
    alice/alice_meta.epp
)
add_epp_suffix(alice_generated_src boot)
add_epp_suffix(alice_generated_src master)

if (NOT CMAKE_CROSSCOMPILING)

add_library                 (boot_alice ${alice_src} ${alice_generated_src_boot})
project_group               (boot_alice Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# LIBRARY alice
########################################

add_library                 (alice ${alice_src} ${alice_generated_src_master})


########################################
# LIBRARY boot_burp
########################################

file(GLOB burp_src "burp/*.cpp" "burp/*.h")

set(burp_generated_src
    burp/backup.epp
    burp/OdsDetection.epp
    burp/restore.epp
)
add_epp_suffix(burp_generated_src boot)
add_epp_suffix(burp_generated_src master)

if (NOT CMAKE_CROSSCOMPILING)

add_library                 (boot_burp ${burp_src} ${burp_generated_src_boot})
project_group               (boot_burp Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# LIBRARY burp
########################################

add_library                 (burp ${burp_src} ${burp_generated_src_master})


########################################
# LIBRARY common
########################################

file(GLOB common_src "common/*.cpp" "common/classes/*.cpp" "common/config/*.cpp" "common/os/${OS_DIR}/*.cpp" "common/sha2/*.cpp")
file(GLOB_RECURSE common_include "common/*.h")

if (APPLE)
    file(GLOB common_os_src "common/os/posix/*.cpp")
    list(REMOVE_ITEM common_os_src ${CMAKE_CURRENT_BINARY_DIR}/common/os/posix/mod_loader.cpp)
endif()

add_library                 (common ${common_src} ${common_os_src} ${common_include})
target_link_libraries       (common ${LIB_mpr} ${LIB_tomcrypt} ${LIB_tommath} ${LIB_re2} ${LIB_dl} decNumber)
add_dependencies_cc         (common UpdateCloopInterfaces)


################################################################################
#
# SHARED LIBRARIES
#
################################################################################

########################################
# SHARED LIBRARY boot_yvalve
########################################

file(GLOB yvalve_src "yvalve/*.cpp" "yvalve/config/os/${OS_DIR}/*.c*")
file(GLOB_RECURSE yvalve_include "yvalve/*.h")

set(yvalve_src ${yvalve_src}
    auth/SecureRemotePassword/client/SrpClient.cpp
    auth/SecurityDatabase/LegacyClient.cpp
    plugins/crypt/arc4/Arc4.cpp
    remote/client/BlrFromMessage.cpp
    remote/client/interface.cpp
)
add_src_win32(yvalve_src
    jrd/os/win32/ibinitdll.cpp
)

add_library                 (yvalve_common OBJECT ${yvalve_src} ${yvalve_include})
add_dependencies_cc         (yvalve_common parse UpdateCloopInterfaces)

if (NOT CMAKE_CROSSCOMPILING)

add_library                 (boot_yvalve SHARED $<TARGET_OBJECTS:yvalve_common>)
target_link_libraries       (boot_yvalve
    PRIVATE remote
    PRIVATE common
    PUBLIC ${LIB_ws2_32}
    PUBLIC ${LIB_editline}
    PUBLIC ${LIB_dl}
    PUBLIC ${LIB_iconv}
    PUBLIC ${LIB_CoreFoundation}
)
set_exported_symbols        (boot_yvalve firebird)
set_output_directory        (boot_yvalve . CURRENT_DIR)
set_output_directory_unix   (boot_yvalve lib CURRENT_DIR)
project_group               (boot_yvalve Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# SHARED LIBRARY yvalve
########################################

add_library                 (yvalve SHARED $<TARGET_OBJECTS:yvalve_common> ${VERSION_RC})
target_link_libraries       (yvalve
    PRIVATE remote
    PRIVATE common
    PUBLIC ${LIB_ws2_32}
    PUBLIC ${LIB_editline}
    PUBLIC ${LIB_dl}
    PUBLIC ${LIB_iconv}
    PUBLIC ${LIB_CoreFoundation}
)
add_dependencies_cc         (yvalve databases)
set_exported_symbols        (yvalve firebird)
set_output_directory_unix   (yvalve lib)
set_target_properties       (yvalve PROPERTIES OUTPUT_NAME fbclient)
set_target_properties       (yvalve PROPERTIES RC_FLAGS -DRC_TARGET_fbclient)


########################################
# SHARED LIBRARY boot_engine
########################################

file(GLOB engine_src
    "dsql/*.cpp"
    "jrd/*.cpp"
    "jrd/extds/*.cpp"
    "jrd/optimizer/*.cpp"
    "jrd/recsrc/*.cpp"
    "jrd/replication/*.cpp"
    "jrd/trace/*.cpp"
    "jrd/os/${OS_DIR}/*.cpp"
)
set(engine_src ${engine_src}
    lock/lock.cpp
    utilities/gsec/gsec.cpp
    utilities/gstat/ppg.cpp
    utilities/nbackup/nbackup.cpp
    # parse
    ${GENERATED_DIR}/dsql/parse.cpp
)
add_src_apple(engine_src
    jrd/os/posix/unix.cpp
)
set(engine_generated_src
    dsql/DdlNodes.epp
    dsql/metd.epp
    dsql/PackageNodes.epp
    jrd/dfw.epp
    jrd/dpm.epp
    jrd/dyn_util.epp
    jrd/fun.epp
    jrd/Function.epp
    jrd/grant.epp
    jrd/ini.epp
    jrd/met.epp
    jrd/scl.epp
    utilities/gstat/dba.epp
)
add_epp_suffix(engine_generated_src boot)
add_epp_suffix(engine_generated_src master)

file(GLOB_RECURSE engine_include "dsql/*.h" "jrd/*.h" include/iberror.h)

add_library                 (engine_common ${engine_src} ${engine_include} ${parse_src})
target_link_libraries       (engine_common libcds)
add_dependencies_cc         (engine_common parse UpdateCloopInterfaces)

if (NOT CMAKE_CROSSCOMPILING)

add_library                 (boot_engine SHARED ${engine_generated_src_boot})
target_link_libraries       (boot_engine engine_common boot_alice boot_burp common boot_yvalve)
set_output_directory        (boot_engine plugins CURRENT_DIR)
set_exported_symbols        (boot_engine fbplugin)
copy_and_rename_lib         (boot_engine Engine14)
project_group               (boot_engine Boot)

if (MINGW)
    add_custom_command(TARGET boot_engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}/share/firebird/plugins
        COMMAND ${CMAKE_COMMAND} -E copy_if_different plugins/Engine14.dll ${output_dir}/share/firebird/plugins/engine14.dll
    )
endif()

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# SHARED LIBRARY engine
########################################

add_library                 (engine SHARED ${engine_generated_src_master} ${VERSION_RC})
target_link_libraries       (engine engine_common alice burp common yvalve)
set_target_properties       (engine PROPERTIES OUTPUT_NAME Engine14)
set_target_properties       (engine PROPERTIES RC_FLAGS -DRC_TARGET_engine14)
set_output_directory        (engine plugins)
set_exported_symbols        (engine fbplugin)

if (MINGW)
    add_custom_command(TARGET engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${output_dir}/plugins/Engine14.dll ${output_dir}/share/firebird/plugins/engine14.dll
        COMMAND ${CMAKE_COMMAND} -E remove ${output_dir}/plugins/Engine14.dll
    )
endif()


###############################################################################
# SHARED LIBRARY intl
###############################################################################

file(GLOB intl_src "intl/*.cpp" "intl/*.h")

add_library                 (intl SHARED ${intl_src} ${VERSION_RC})
target_link_libraries       (intl common yvalve)
set_target_properties       (intl PROPERTIES OUTPUT_NAME fbintl)
set_target_properties       (intl PROPERTIES RC_FLAGS -DRC_TARGET_fbintl)
set_output_directory        (intl intl)

if (MINGW)
    add_custom_command(TARGET intl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}/share/firebird/intl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${output_dir}/intl/fbintl.dll ${output_dir}/share/firebird/intl/fbintl.dll
    )
endif()

########################################
# SHARED LIBRARY ib_util
########################################

add_library                 (ib_util SHARED extlib/ib_util.cpp ${VERSION_RC})
set_exported_symbols        (ib_util ib_util)
set_target_properties       (ib_util PROPERTIES RC_FLAGS -DRC_TARGET_ib_util)
set_output_directory_unix   (ib_util lib)


########################################
# SHARED LIBRARY PLUGIN legacy_usermanager
########################################

set(legacy_usermanager_generated_src
    auth/SecurityDatabase/LegacyManagement.epp
)
add_epp_suffix(legacy_usermanager_generated_src master)

add_library                 (legacy_usermanager SHARED ${legacy_usermanager_generated_src_master} auth/SecurityDatabase/LegacyManagement.h ${VERSION_RC})
target_link_libraries       (legacy_usermanager common yvalve)
set_target_properties       (legacy_usermanager PROPERTIES OUTPUT_NAME Legacy_UserManager)
set_target_properties       (legacy_usermanager PROPERTIES RC_FLAGS -DRC_TARGET_legacy_usermanager)
add_dependencies_cc         (legacy_usermanager engine)
set_output_directory        (legacy_usermanager plugins)
set_exported_symbols        (legacy_usermanager fbplugin)


########################################
# SHARED LIBRARY PLUGIN chacha
########################################

add_library                 (chacha SHARED plugins/crypt/chacha/ChaCha.cpp ${VERSION_RC})
target_link_libraries       (chacha common yvalve)
set_target_properties       (chacha PROPERTIES OUTPUT_NAME ChaCha)
set_target_properties       (chacha PROPERTIES RC_FLAGS -DRC_TARGET_chacha)
set_output_directory        (chacha plugins)
set_exported_symbols        (chacha fbplugin)


########################################
# SHARED LIBRARY PLUGIN default_profiler
########################################

add_library                 (default_profiler SHARED plugins/profiler/Profiler.cpp ${VERSION_RC})
target_link_libraries       (default_profiler common yvalve)
set_target_properties       (default_profiler PROPERTIES OUTPUT_NAME Default_Profiler)
set_target_properties       (default_profiler PROPERTIES RC_FLAGS -DRC_TARGET_default_profiler)
set_output_directory        (default_profiler plugins)
set_exported_symbols        (default_profiler fbplugin)


########################################
# SHARED LIBRARY PLUGIN udf_compat
########################################

set(udf_compat_sql_src
    ${CMAKE_CURRENT_SOURCE_DIR}/extlib/UdfBackwardCompatibility.sql
)

add_library                 (udf_compat SHARED extlib/UdfBackwardCompatibility.cpp ${VERSION_RC})
target_link_libraries       (udf_compat ib_util)
set_target_properties       (udf_compat PROPERTIES RC_FLAGS -DRC_TARGET_udf_compat)
set_output_directory        (udf_compat plugins/udr)
set_exported_symbols        (udf_compat fbplugin)

add_custom_command(TARGET udf_compat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${udf_compat_sql_src} ${output_dir}/plugins/udr/udf_compat.sql
)

########################################
# SHARED LIBRARY PLUGIN udr_engine
########################################

add_library                 (udr_engine SHARED plugins/udr_engine/UdrEngine.cpp ${VERSION_RC})
target_link_libraries       (udr_engine common yvalve)
set_target_properties       (udr_engine PROPERTIES RC_FLAGS -DRC_TARGET_udr_engine)
set_output_directory        (udr_engine plugins)
set_exported_symbols        (udr_engine fbplugin)


########################################
# SHARED LIBRARY PLUGIN srp
########################################

add_library                 (srp SHARED auth/SecureRemotePassword/manage/SrpManagement.cpp ${VERSION_RC})
target_link_libraries       (srp remote common yvalve)
set_target_properties       (srp PROPERTIES OUTPUT_NAME Srp)
set_target_properties       (srp PROPERTIES RC_FLAGS -DRC_TARGET_srp)
set_output_directory        (srp plugins)
set_exported_symbols        (srp fbplugin)


########################################
# SHARED LIBRARY PLUGIN legacy_auth
########################################

set(legacy_auth_src
    auth/SecurityDatabase/LegacyServer.cpp
)
add_src_win32(legacy_auth_src
    auth/SecDbCache.cpp
)

add_library                 (legacy_auth SHARED ${legacy_auth_src} ${VERSION_RC})
target_link_libraries       (legacy_auth common yvalve)
set_target_properties       (legacy_auth PROPERTIES OUTPUT_NAME Legacy_Auth)
set_target_properties       (legacy_auth PROPERTIES RC_FLAGS -DRC_TARGET_legacy_auth)
set_output_directory        (legacy_auth plugins)
set_exported_symbols        (legacy_auth fbplugin)


################################################################################
#
# EXECUTABLES
#
################################################################################

########################################
# EXECUTABLE gpre_boot
########################################

set(gpre_boot_src
    gpre/boot/gpre_meta_boot.cpp
    yvalve/gds.cpp
)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (gpre_boot ${gpre_boot_src} ${VERSION_RC})
target_link_libraries       (gpre_boot gpre_common common ${LIB_iconv} ${LIB_ws2_32} ${LIB_dl})
set_target_properties       (gpre_boot PROPERTIES RC_FLAGS -DRC_TARGET_gpre_boot)
set_output_directory        (gpre_boot . CURRENT_DIR)
project_group               (gpre_boot Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE boot_gpre
########################################

set(gpre_generated_src
    gpre/std/gpre_meta.epp
)
add_epp_suffix(gpre_generated_src boot)
add_epp_suffix(gpre_generated_src master)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gpre ${gpre_generated_src_boot})
target_link_libraries       (boot_gpre gpre_common common boot_yvalve)
set_output_directory        (boot_gpre . CURRENT_DIR)
project_group               (boot_gpre Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gpre
########################################

add_executable              (gpre ${gpre_generated_src_master} ${VERSION_RC})
target_link_libraries       (gpre gpre_common common yvalve)
set_target_properties       (gpre PROPERTIES RC_FLAGS -DRC_TARGET_gpre)


########################################
# EXECUTABLE build_msg
########################################

add_executable              (build_msg msgs/build_file.cpp ${VERSION_RC})
set_target_properties       (build_msg PROPERTIES RC_FLAGS -DRC_TARGET_build_msg)
set_output_directory        (build_msg . CURRENT_DIR)
project_group               (build_msg Boot)


########################################
# EXECUTABLE gstat
########################################

set(gstat_src
    jrd/btn.cpp
    jrd/ods.cpp
    utilities/gstat/main/gstatMain.cpp
    utilities/gstat/ppg.cpp
)
set(gstat_generated_src
    utilities/gstat/dba.epp
)
add_epp_suffix(gstat_generated_src master)

add_executable              (gstat ${gstat_src} ${gstat_generated_src_master} ${VERSION_RC})
target_link_libraries       (gstat common yvalve)
add_dependencies_cc         (gstat engine)
set_target_properties       (gstat PROPERTIES RC_FLAGS -DRC_TARGET_gstat)


########################################
# EXECUTABLE fb_lock_print
########################################

add_executable              (fb_lock_print lock/print.cpp ${VERSION_RC})
target_link_libraries       (fb_lock_print common yvalve)
set_target_properties       (fb_lock_print PROPERTIES RC_FLAGS -DRC_TARGET_fb_lock_print)


########################################
# EXECUTABLE fbguard
########################################

add_src_win32(fbguard_src
    iscguard/cntl_guard.cpp
    iscguard/iscguard.cpp
    remote/server/os/win32/chop.cpp

    iscguard/iscguard.rc
)
add_src_unix(fbguard_src
    utilities/guard/guard.cpp
    utilities/guard/util.cpp
)

add_executable              (fbguard WIN32 ${fbguard_src})
target_link_libraries       (fbguard common yvalve ${LIB_comctl32} ${LIB_version})
set_target_properties       (fbguard PROPERTIES LINK_FLAGS -mwindows)
set_target_properties       (fbguard PROPERTIES RC_FLAGS -DRC_TARGET_fbguard)


########################################
# EXECUTABLE fbtracemgr
########################################

set(fbtracemgr_src
    jrd/trace/TraceCmdLine.cpp
    utilities/fbtracemgr/traceMgrMain.cpp
)

add_executable              (fbtracemgr ${fbtracemgr_src} ${VERSION_RC})
target_link_libraries       (fbtracemgr common yvalve)
set_target_properties       (fbtracemgr PROPERTIES RC_FLAGS -DRC_TARGET_fbtracemgr)


########################################
# EXECUTABLE boot_gfix
########################################

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gfix alice/main/aliceMain.cpp)
target_link_libraries       (boot_gfix boot_alice common boot_yvalve)
set_output_directory        (boot_gfix . CURRENT_DIR)
project_group               (boot_gfix Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gfix
########################################

add_executable              (gfix alice/main/aliceMain.cpp ${VERSION_RC})
target_link_libraries       (gfix alice common yvalve)
set_target_properties       (gfix PROPERTIES RC_FLAGS -DRC_TARGET_gfix)

########################################
# EXECUTABLE boot_gbak
########################################

file(GLOB gbak_include "burp/*.h")

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gbak burp/main/burpMain.cpp ${gbak_include})
target_link_libraries       (boot_gbak boot_burp common boot_yvalve)
set_output_directory        (boot_gbak . CURRENT_DIR)
project_group               (boot_gbak Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gbak
########################################

add_executable              (gbak burp/main/burpMain.cpp ${gbak_include} ${VERSION_RC})
target_link_libraries       (gbak burp common yvalve)
set_target_properties       (gbak PROPERTIES RC_FLAGS -DRC_TARGET_gbak)


########################################
# EXECUTABLE gsplit
########################################

add_executable              (gsplit burp/split/spit.cpp burp/split/spit.h ${VERSION_RC})
target_link_libraries       (gsplit burp common yvalve)
set_target_properties       (gsplit PROPERTIES RC_FLAGS -DRC_TARGET_gsplit)


########################################
# EXECUTABLE boot_isql
########################################

file(GLOB isql_src "isql/*.cpp" "isql/*.h")

set(isql_generated_src
    isql/extract.epp
    isql/isql.epp
    isql/show.epp
)
add_epp_suffix(isql_generated_src boot)
add_epp_suffix(isql_generated_src master)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_isql ${isql_src} ${isql_generated_src_boot})
target_link_libraries       (boot_isql common boot_yvalve)
set_output_directory        (boot_isql . CURRENT_DIR)
project_group               (boot_isql Boot)

endif() # (NOT CMAKE_CROSSCOMPILING)

if (MSVC)
    add_custom_command(
        TARGET boot_isql
        POST_BUILD
        #
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/icudt${ICU_VERSION}l.dat ${output_dir}/icudt${ICU_VERSION}l.dat
        #
        # one copy to boot directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/icudt${ICU_VERSION}l.dat $<TARGET_FILE_DIR:boot_isql>/icudt${ICU_VERSION}l.dat
    )

    string(FIND "${CMAKE_EXE_LINKER_FLAGS}" "/machine:x64" arch)
    if (NOT ${arch} EQUAL -1)
        set(arch "x64")
    else()
        set(arch "Win32")
    endif()
    add_custom_command(
        TARGET boot_isql
        POST_BUILD
        # icu
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icudt${ICU_VERSION}.dll ${output_dir}/icudt${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuin${ICU_VERSION}.dll ${output_dir}/icuin${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuuc${ICU_VERSION}.dll ${output_dir}/icuuc${ICU_VERSION}.dll
        # zlib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/zlib/${arch}/zlib1.dll ${output_dir}/zlib1.dll
        #
        # one copy to boot directory
        # icu
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icudt${ICU_VERSION}.dll $<TARGET_FILE_DIR:boot_isql>/icudt${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuin${ICU_VERSION}.dll $<TARGET_FILE_DIR:boot_isql>/icuin${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuuc${ICU_VERSION}.dll $<TARGET_FILE_DIR:boot_isql>/icuuc${ICU_VERSION}.dll
        # zlib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/zlib/${arch}/zlib1.dll $<TARGET_FILE_DIR:boot_isql>/zlib1.dll
    )
endif()


########################################
# EXECUTABLE isql
########################################

add_executable              (isql ${isql_src} ${isql_generated_src_master} ${VERSION_RC})
target_link_libraries       (isql common yvalve)
set_target_properties       (isql PROPERTIES LINK_FLAGS -mwindows)
set_target_properties       (isql PROPERTIES RC_FLAGS -DRC_TARGET_isql)


################################################################################
#
# subdirectories
#
################################################################################

add_subdirectory("gpre")
add_subdirectory("remote")
add_subdirectory("utilities")

###############################################################################
#
# copy files to output dir
#
###############################################################################

add_custom_target(copy_files
    ALL
    # databases
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GENERATED_DIR}/security.fdb ${output_dir}/security6.fdb
    # configs, text files
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/firebird.conf ${output_dir}/firebird.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/databases.conf ${output_dir}/databases.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/fbintl.conf ${output_dir}/intl/fbintl.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/plugins.conf ${output_dir}/plugins.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/replication.conf ${output_dir}/replication.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/IPLicense.txt ${output_dir}/IPLicense.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/IDPLicense.txt ${output_dir}/IDPLicense.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/utilities/ntrace/fbtrace.conf ${output_dir}/fbtrace.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/plugins/udr_engine/udr_engine.conf ${output_dir}/plugins/udr_engine.conf
    # docs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/CHANGELOG.md ${output_dir}/doc/CHANGELOG.md
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/README.md ${output_dir}/doc/README.md
    # examples
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/api ${output_dir}/examples/api
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/dbcrypt ${output_dir}/examples/dbcrypt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/examples/empbuild/employe2.sql ${output_dir}/examples/empbuild/employe2.sql
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/include ${output_dir}/examples/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/interfaces ${output_dir}/examples/interfaces
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/package ${output_dir}/examples/package
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/stat ${output_dir}/examples/stat
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/udf ${output_dir}/examples/udf
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/udr ${output_dir}/examples/udr
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/stat ${output_dir}/examples/stat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/examples/functions.c ${output_dir}/examples/functions.c
    # headers
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/extlib/ib_util.h ${output_dir}/include/ib_util.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/include/iberror.h ${output_dir}/include/iberror.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/yvalve/perf.h ${output_dir}/include/perf.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/include/firebird/FirebirdInterface.idl ${output_dir}/include/firebird/FirebirdInterface.idl
)
add_dependencies_cc     (copy_files databases)
add_dependencies_unix_cc(copy_files makeHeader)
project_group(copy_files "Custom build steps")

# headers
file(GLOB files "${CMAKE_SOURCE_DIR}/src/include/firebird/*.h")
foreach(F ${files})
    get_filename_component(name ${F} NAME)
    add_custom_command(TARGET copy_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${output_dir}/include/firebird/${name})
endforeach()

# docs
file(GLOB files "${CMAKE_SOURCE_DIR}/doc/README.*")
foreach(F ${files})
    get_filename_component(name ${F} NAME)
    add_custom_command(TARGET copy_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${output_dir}/doc/${name})
endforeach()

file(GLOB files "${CMAKE_SOURCE_DIR}/doc/sql.extensions/README.*")
foreach(F ${files})
    get_filename_component(name ${F} NAME)
    add_custom_command(TARGET copy_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${output_dir}/doc/sql.extensions/${name})
endforeach()

if (WIN32)
    set(fbclient_lib_src $<CONFIG>/fbclient.lib)
    set(fbclient_lib_dst fbclient_ms.lib)
    set( ib_util_lib_src $<CONFIG>/ib_util.lib)
    set( ib_util_lib_dst ib_util_ms.lib)
    if (MINGW)
        set(fbclient_lib_src libfbclient.dll.a)
        set(fbclient_lib_dst libfbclient.dll.a)
        set( ib_util_lib_src libib_util.dll.a)
        set( ib_util_lib_dst libib_util.dll.a)
    endif()
    add_custom_command(
        TARGET copy_files
        POST_BUILD
        # lib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${fbclient_lib_src} ${output_dir}/lib/${fbclient_lib_dst}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ib_util_lib_src} ${output_dir}/lib/${ib_util_lib_dst}
        # installers
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/arch-specific/win32/install_service.bat ${output_dir}/install_service.bat
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/arch-specific/win32/uninstall_service.bat ${output_dir}/uninstall_service.bat
        # examples
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/build_unix ${output_dir}/examples/build_unix
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/build_win32 ${output_dir}/examples/build_win32
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/examples/readme ${output_dir}/examples/readme.txt
        # headers
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/include/firebird/impl ${output_dir}/include/firebird/impl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/include/ibase.h ${output_dir}/include/ibase.h
    )
endif()


if (UNIX)
    add_custom_command(TARGET copy_files POST_BUILD
        # examples
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/examples/readme ${output_dir}/examples/README
    )

    if (NOT CMAKE_CROSSCOMPILING)

    add_executable          (makeHeader misc/makeHeader.cpp)
    set_output_directory    (makeHeader . CURRENT_DIR)
    set(files
        ${CMAKE_SOURCE_DIR}/src/include/types_pub.h
        ${CMAKE_SOURCE_DIR}/src/include/consts_pub.h
        ${CMAKE_SOURCE_DIR}/src/dsql/sqlda_pub.h
        ${CMAKE_SOURCE_DIR}/src/common/dsc_pub.h
        ${CMAKE_SOURCE_DIR}/src/jrd/ibase.h
        ${CMAKE_SOURCE_DIR}/src/jrd/inf_pub.h
        ${CMAKE_SOURCE_DIR}/src/jrd/blr.h
        ${CMAKE_SOURCE_DIR}/src/include/gen/iberror.h
    )
    foreach(F ${files})
        get_filename_component(name ${F} NAME)
        add_custom_command(TARGET copy_files POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${name})
    endforeach()
    add_custom_command(TARGET copy_files POST_BUILD
        # include
        COMMAND makeHeader < ibase.h > ${output_dir}/include/ibase.h
    )

    endif() # (NOT CMAKE_CROSSCOMPILING)
endif()


################################################################################
