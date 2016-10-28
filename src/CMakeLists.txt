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
    utilities/gstat/dba.epp
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
    yvalve/array.epp
    yvalve/blob.epp
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
    msgs/build_file.epp
    misc/codes.epp
    qli/help.epp
    qli/meta.epp
    qli/proc.epp
    qli/show.epp
)

if (NOT CMAKE_CROSSCOMPILING)

epp_process(boot    epp_boot_internal_files ${GPRE_BOOT_CMD} -lang_internal -n -m)
epp_process(boot    epp_boot_ocxx_files     ${GPRE_BOOT_CMD} -lang_internal -n -ids -ocxx)
epp_process(boot    epp_boot_files          ${GPRE_BOOT_CMD} -n -m)
epp_process(boot    epp_boot_gds_files      ${GPRE_BOOT_CMD} -n -ids -gds_cxx)

epp_process(master  epp_boot_internal_files ${BOOT_GPRE_CMD} -n -m)
epp_process(master  epp_boot_ocxx_files     ${BOOT_GPRE_CMD} -n -ids -ocxx)
epp_process(master  epp_boot_files          ${BOOT_GPRE_CMD} -n -m)
epp_process(master  epp_boot_gds_files      ${BOOT_GPRE_CMD} -n -ids -gds_cxx)
epp_process(master  epp_master_files        ${BOOT_GPRE_CMD} -n -m)


################################################################################
#
# CUSTOM BUILD STEPS
#
################################################################################

########################################
# BUILD STEP databases
########################################

set(msgs_src
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/facilities2.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/history2.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/locales.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/messages2.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/msg.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/sqlstates.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/symbols2.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/system_errors2.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/transmsgs.de_DE2.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/transmsgs.fr_FR2.sql
)
set(databases_src
    ${CMAKE_CURRENT_SOURCE_DIR}/dbs/security.sql
    ${msgs_src}
)
add_custom_command(
    OUTPUT security.fdb
    DEPENDS
        boot_isql
        boot_gfix
        ${CMAKE_CURRENT_SOURCE_DIR}/dbs/security.sql
    VERBATIM
    #
    COMMAND ${CMAKE_COMMAND} -E remove security.fdb
    COMMAND ${CMAKE_COMMAND} -E remove security_.fdb
    COMMAND ${CMAKE_COMMAND} -E echo "create database 'security_.fdb';" > create_db_security.sql
    COMMAND ${BOOT_ISQL_CMD} -q -i create_db_security.sql
    COMMAND ${BOOT_GFIX_CMD} -write async security_.fdb
    COMMAND ${BOOT_ISQL_CMD} -q security_.fdb -i ${CMAKE_CURRENT_SOURCE_DIR}/dbs/security.sql
    COMMAND ${CMAKE_COMMAND} -E copy_if_different security_.fdb security.fdb
    COMMAND ${BOOT_GFIX_CMD} -write sync security.fdb
)
add_custom_command(
    OUTPUT metadata.fdb
    DEPENDS
        boot_gbak
        ${CMAKE_SOURCE_DIR}/builds/misc/metadata.gbak
    #
    COMMAND ${CMAKE_COMMAND} -E remove metadata.fdb
    COMMAND ${CMAKE_COMMAND} -E remove metadata_.fdb
    COMMAND ${BOOT_GBAK_CMD} -r ${CMAKE_SOURCE_DIR}/builds/misc/metadata.gbak metadata_.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different metadata_.fdb metadata.fdb
)
add_custom_command(
    OUTPUT help.fdb
    DEPENDS
        boot_gbak
        metadata.fdb
        ${CMAKE_SOURCE_DIR}/builds/misc/help.gbak
    #
    COMMAND ${CMAKE_COMMAND} -E remove help.fdb
    COMMAND ${CMAKE_COMMAND} -E remove help_.fdb
    COMMAND ${BOOT_GBAK_CMD} -r ${CMAKE_SOURCE_DIR}/builds/misc/help.gbak help_.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different help_.fdb help.fdb
)
add_custom_target(databases
    DEPENDS
        boot_engine
        help.fdb
        metadata.fdb
        security.fdb
    SOURCES
        ${databases_src}
)
add_dependencies_cc(databases msg_fdb)
project_group(databases "Boot/Custom build steps")


########################################
# BUILD STEP messages
########################################

add_custom_command(
    OUTPUT firebird.msg
    DEPENDS
        build_msg
        msg_fdb
    #
    COMMAND ${CMAKE_COMMAND} -E remove firebird.msg
    COMMAND ${BUILD_MSG_CMD} -D msg.fdb -P ${CMAKE_CURRENT_BINARY_DIR}/ -F firebird.msg -L all
    COMMAND ${BUILD_MSG_CMD} -D msg.fdb -P ${CMAKE_CURRENT_BINARY_DIR}/ -F firebird.msg
    COMMAND ${CMAKE_COMMAND} -E copy_if_different firebird.msg ${output_dir}/firebird.msg
)
add_custom_command(
    OUTPUT codes_headers.dummy
    DEPENDS
        codes
        msg_fdb
    COMMENT ""
    #
    COMMAND ${CMAKE_COMMAND} -E echo "" > codes_headers.dummy
    COMMAND ${CMAKE_COMMAND} -E echo Building codes header...
    COMMAND ${CODES_CMD} ${CMAKE_CURRENT_SOURCE_DIR}/include/gen ${CMAKE_SOURCE_DIR}/lang_helpers
)
add_custom_target(messages ALL DEPENDS firebird.msg codes_headers.dummy)
project_group(messages "Boot/Custom build steps")


########################################
# BUILD STEP msg_fdb
########################################

set(isql_exec_msg ${BOOT_ISQL_CMD} -q msg_.fdb -i ${CMAKE_CURRENT_SOURCE_DIR}/msgs)

add_custom_command(
    OUTPUT msg.fdb
    VERBATIM
    DEPENDS
        boot_isql
        boot_gfix
        ${msgs_src}
    #
    COMMAND ${CMAKE_COMMAND} -E remove msg.fdb
    COMMAND ${CMAKE_COMMAND} -E remove msg_.fdb
    COMMAND ${CMAKE_COMMAND} -E echo "create database 'msg_.fdb';" > create_db_msg.sql
    COMMAND ${BOOT_ISQL_CMD} -q -i create_db_msg.sql
    COMMAND ${BOOT_GFIX_CMD} -write async msg_.fdb
    COMMAND ${isql_exec_msg}/msg.sql
    #
    COMMAND echo loading facilities
    COMMAND ${isql_exec_msg}/facilities2.sql
    COMMAND echo loading sql states
    COMMAND ${isql_exec_msg}/sqlstates.sql
    COMMAND echo loading locales
    COMMAND ${isql_exec_msg}/locales.sql
    COMMAND echo loading history
    COMMAND ${isql_exec_msg}/history2.sql
    COMMAND echo loading messages
    COMMAND ${isql_exec_msg}/messages2.sql
    COMMAND echo loading symbols
    COMMAND ${isql_exec_msg}/symbols2.sql
    COMMAND echo loading system errors
    COMMAND ${isql_exec_msg}/system_errors2.sql
    COMMAND echo loading French translation
    COMMAND ${isql_exec_msg}/transmsgs.fr_FR2.sql
    COMMAND echo loading German translation
    COMMAND ${isql_exec_msg}/transmsgs.de_DE2.sql
    #
    COMMAND ${CMAKE_COMMAND} -E copy_if_different msg_.fdb msg.fdb
)
add_custom_target(msg_fdb
    DEPENDS
        boot_engine
        msg.fdb
)
project_group(msg_fdb "Boot/Custom build steps")


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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different y_tab.h include/gen/parse.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different y_tab.c dsql/parse.cpp
    COMMENT "Generating parse.cpp, parse.h"
    VERBATIM
)
add_custom_target(parse
    DEPENDS y_tab.h y_tab.c
    SOURCES ${parse_src}
)
project_group(parse "Boot/Custom build steps")
set_source_files_properties(dsql/parse.cpp include/gen/parse.h PROPERTIES GENERATED TRUE)


########################################
# BUILD_STEP UpdateCloopInterfaces
########################################

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/IdlFbInterfaces.h
    DEPENDS
        cloop
        ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/FirebirdInterface.idl
    COMMAND cloop
            ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/FirebirdInterface.idl
            c++
            ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/IdlFbInterfaces.h
            IDL_FB_INTERFACES_H Firebird I
    COMMENT "Updating cloop interfaces..."
)
add_custom_target(UpdateCloopInterfaces
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/IdlFbInterfaces.h
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird/FirebirdInterface.idl
)
project_group(UpdateCloopInterfaces "Boot/Custom build steps")

endif() # if (NOT CMAKE_CROSSCOMPILING)


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

endif() # if (NOT CMAKE_CROSSCOMPILING)


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

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# LIBRARY burp
########################################

add_library                 (burp ${burp_src} ${burp_generated_src_master})


########################################
# LIBRARY common
########################################

file(GLOB common_src "common/*.cpp" "common/classes/*.cpp" "common/config/*.cpp" "common/os/${OS_DIR}/*.cpp")
file(GLOB_RECURSE common_include "common/*.h")

if (APPLE)
    file(GLOB common_os_src "common/os/posix/*.cpp")
    list(REMOVE_ITEM common_os_src ${CMAKE_CURRENT_BINARY_DIR}/common/os/posix/mod_loader.cpp)
endif()

add_library                 (common ${common_src} ${common_os_src} ${common_include})
target_link_libraries       (common ${LIB_mpr})
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
set(yvalve_generated_src
    yvalve/array.epp
    yvalve/blob.epp
)
add_epp_suffix(yvalve_generated_src boot)
add_epp_suffix(yvalve_generated_src master)

add_library                 (yvalve_common OBJECT ${yvalve_src} ${yvalve_include})
add_dependencies_cc         (yvalve_common parse UpdateCloopInterfaces)

if (NOT CMAKE_CROSSCOMPILING)

add_library                 (boot_yvalve SHARED $<TARGET_OBJECTS:yvalve_common> ${yvalve_generated_src_boot} ${VERSION_RC})
target_link_libraries       (boot_yvalve
    PRIVATE remote
    PRIVATE common
    PUBLIC libtommath
    PUBLIC ${LIB_Ws2_32}
    PUBLIC ${LIB_readline}
    PUBLIC ${LIB_dl}
    PUBLIC ${LIB_iconv}
    PUBLIC ${LIB_CoreFoundation}
)
set_exported_symbols        (boot_yvalve firebird)
set_output_directory        (boot_yvalve . CURRENT_DIR)
set_output_directory_unix   (boot_yvalve lib CURRENT_DIR)
project_group               (boot_yvalve Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# SHARED LIBRARY yvalve
########################################

add_library                 (yvalve SHARED $<TARGET_OBJECTS:yvalve_common> ${yvalve_generated_src_master} ${VERSION_RC})
target_link_libraries       (yvalve
    PRIVATE remote
    PRIVATE common
    PUBLIC libtommath
    PUBLIC ${LIB_Ws2_32}
    PUBLIC ${LIB_readline}
    PUBLIC ${LIB_dl}
    PUBLIC ${LIB_iconv}
    PUBLIC ${LIB_CoreFoundation}
)
add_dependencies_cc         (yvalve databases)
set_exported_symbols        (yvalve firebird)
set_output_directory_unix   (yvalve lib)
set_target_properties       (yvalve PROPERTIES OUTPUT_NAME fbclient)


########################################
# SHARED LIBRARY boot_engine
########################################

file(GLOB engine_src
    "dsql/*.cpp"
    "jrd/*.cpp"
    "jrd/extds/*.cpp"
    "jrd/recsrc/*.cpp"
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

file(GLOB_RECURSE engine_include "dsql/*.h" "jrd/*.h" include/gen/iberror.h)

add_library                 (engine_common ${engine_src} ${engine_include} ${parse_src})
add_dependencies_cc         (engine_common parse UpdateCloopInterfaces)

if (NOT CMAKE_CROSSCOMPILING)

add_library                 (boot_engine SHARED ${engine_generated_src_boot} ${VERSION_RC})
target_link_libraries       (boot_engine engine_common boot_alice boot_burp common boot_yvalve)
set_output_directory        (boot_engine plugins CURRENT_DIR)
set_exported_symbols        (boot_engine fbplugin)
copy_and_rename_lib         (boot_engine Engine13)
project_group               (boot_engine Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# SHARED LIBRARY engine
########################################

add_library                 (engine SHARED ${engine_generated_src_master} ${VERSION_RC})
target_link_libraries       (engine engine_common alice burp common yvalve)
set_target_properties       (engine PROPERTIES OUTPUT_NAME Engine13)
set_output_directory        (engine plugins)
set_exported_symbols        (engine fbplugin)


###############################################################################
# SHARED LIBRARY intl
###############################################################################

file(GLOB intl_src "intl/*.cpp" "intl/*.h")

add_library                 (intl SHARED ${intl_src} ${VERSION_RC})
target_link_libraries       (intl common yvalve)
set_target_properties       (intl PROPERTIES OUTPUT_NAME fbintl)
set_output_directory        (intl intl)


########################################
# SHARED LIBRARY ib_util
########################################

add_library                 (ib_util SHARED extlib/ib_util.cpp extlib/ib_util.h ${VERSION_RC})
set_exported_symbols        (ib_util ib_util)
set_output_directory_unix   (ib_util lib)


########################################
# SHARED LIBRARY ib_udf
########################################

add_library                 (ib_udf SHARED extlib/ib_udf.cpp extlib/ib_udf.h ${VERSION_RC})
target_link_libraries       (ib_udf ib_util)
set_target_properties       (ib_udf PROPERTIES PREFIX "")
set_exported_symbols        (ib_udf ib_udf)
set_output_directory        (ib_udf UDF)


########################################
# SHARED LIBRARY legacy_usermanager
########################################

set(legacy_usermanager_generated_src
    auth/SecurityDatabase/LegacyManagement.epp
)
add_epp_suffix(legacy_usermanager_generated_src master)

add_library                 (legacy_usermanager SHARED ${legacy_usermanager_generated_src_master} auth/SecurityDatabase/LegacyManagement.h ${VERSION_RC})
target_link_libraries       (legacy_usermanager common yvalve)
set_target_properties       (legacy_usermanager PROPERTIES OUTPUT_NAME Legacy_UserManager)
set_output_directory        (legacy_usermanager plugins)
set_exported_symbols        (legacy_usermanager fbplugin)


########################################
# SHARED LIBRARY udr_engine
########################################

set(udr_engine_src
    plugins/udr_engine/UdrEngine.cpp
)
add_library                 (udr_engine SHARED ${udr_engine_src} ${VERSION_RC})
target_link_libraries       (udr_engine common yvalve)
set_output_directory        (udr_engine plugins)
set_exported_symbols        (udr_engine fbplugin)


########################################
# SHARED LIBRARY fbudf
########################################

set(fbudf_src
    extlib/fbudf/fbudf.cpp
    extlib/fbudf/stdafx.cpp

    extlib/fbudf/fbudf.txt
    extlib/fbudf/fbudf.sql

    jrd/ibase.h
)
file(GLOB fbudf_include "extlib/fbudf/*.h")

add_library                 (fbudf SHARED ${fbudf_src} ${fbudf_include} ${VERSION_RC})
target_link_libraries       (fbudf common yvalve)
set_target_properties       (fbudf PROPERTIES PREFIX "")
set_output_directory        (fbudf UDF)


########################################
# SHARED LIBRARY srp
########################################

add_library                 (srp SHARED auth/SecureRemotePassword/manage/SrpManagement.cpp ${VERSION_RC})
target_link_libraries       (srp remote common yvalve)
set_target_properties       (srp PROPERTIES OUTPUT_NAME Srp)
set_output_directory        (srp plugins)
set_exported_symbols        (srp fbplugin)


########################################
# SHARED LIBRARY legacy_auth
########################################

add_library                 (legacy_auth SHARED auth/SecurityDatabase/LegacyServer.cpp ${VERSION_RC})
target_link_libraries       (legacy_auth common yvalve)
set_target_properties       (legacy_auth PROPERTIES OUTPUT_NAME Legacy_Auth)
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
target_link_libraries       (gpre_boot gpre_common common ${LIB_iconv} ${LIB_Ws2_32})
set_output_directory        (gpre_boot . CURRENT_DIR)
project_group               (gpre_boot Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE boot_gpre
########################################

set(gpre_generated_src
    gpre/std/gpre_meta.epp
)
add_epp_suffix(gpre_generated_src boot)
add_epp_suffix(gpre_generated_src master)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gpre ${gpre_generated_src_boot} ${VERSION_RC})
target_link_libraries       (boot_gpre gpre_common common boot_yvalve)
set_output_directory        (boot_gpre . CURRENT_DIR)
project_group               (boot_gpre Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gpre
########################################

add_executable              (gpre ${gpre_generated_src_master} ${VERSION_RC})
target_link_libraries       (gpre gpre_common common yvalve)


########################################
# EXECUTABLE build_msg
########################################

set(build_msg_generated_src
    msgs/build_file.epp
)
add_epp_suffix(build_msg_generated_src master)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (build_msg ${build_msg_generated_src_master} ${VERSION_RC})
target_link_libraries       (build_msg boot_yvalve)
set_output_directory        (build_msg . CURRENT_DIR)
project_group               (build_msg Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE codes
########################################

set(codes_generated_src
    misc/codes.epp
)
add_epp_suffix(codes_generated_src master)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (codes ${codes_generated_src_master} ${VERSION_RC})
target_link_libraries       (codes boot_yvalve)
set_output_directory        (codes . CURRENT_DIR)
project_group               (codes Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


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


########################################
# EXECUTABLE fb_lock_print
########################################

add_executable              (fb_lock_print lock/print.cpp ${VERSION_RC})
target_link_libraries       (fb_lock_print common yvalve)


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


########################################
# EXECUTABLE fbtracemgr
########################################

set(fbtracemgr_src
    jrd/trace/TraceCmdLine.cpp
    utilities/fbtracemgr/traceMgrMain.cpp
)

add_executable              (fbtracemgr ${fbtracemgr_src} ${VERSION_RC})
target_link_libraries       (fbtracemgr common yvalve)


########################################
# EXECUTABLE boot_gfix
########################################

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gfix alice/main/aliceMain.cpp ${VERSION_RC})
target_link_libraries       (boot_gfix boot_alice common boot_yvalve)
set_output_directory        (boot_gfix . CURRENT_DIR)
project_group               (boot_gfix Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gfix
########################################

add_executable              (gfix alice/main/aliceMain.cpp ${VERSION_RC})
target_link_libraries       (gfix alice common yvalve)


########################################
# EXECUTABLE boot_gbak
########################################

file(GLOB gbak_include "burp/*.h")

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gbak burp/main/burpMain.cpp ${gbak_include} ${VERSION_RC})
target_link_libraries       (boot_gbak boot_burp common boot_yvalve)
set_output_directory        (boot_gbak . CURRENT_DIR)
project_group               (boot_gbak Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gbak
########################################

add_executable              (gbak burp/main/burpMain.cpp ${gbak_include} ${VERSION_RC})
target_link_libraries       (gbak burp common yvalve)


########################################
# EXECUTABLE gsplit
########################################

add_executable              (gsplit burp/split/spit.cpp burp/split/spit.h ${VERSION_RC})
target_link_libraries       (gsplit burp common yvalve)


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

add_executable              (boot_isql ${isql_src} ${isql_generated_src_boot} ${VERSION_RC})
target_link_libraries       (boot_isql common boot_yvalve)
set_output_directory        (boot_isql . CURRENT_DIR)
project_group               (boot_isql Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)

if (WIN32)
    add_custom_command(
        TARGET boot_isql
        POST_BUILD
        #
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/icudt52l.dat ${output_dir}/icudt52l.dat
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/icudt52l_empty.dat ${output_dir}/icudt52l_empty.dat
        #
        # one copy to boot directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/icudt52l.dat $<TARGET_FILE_DIR:boot_isql>/icudt52l.dat
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/icudt52l_empty.dat $<TARGET_FILE_DIR:boot_isql>/icudt52l_empty.dat
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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icudt52.dll ${output_dir}/icudt52.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuin52.dll ${output_dir}/icuin52.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuuc52.dll ${output_dir}/icuuc52.dll
        # zlib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/zlib/${arch}/zlib1.dll ${output_dir}/zlib1.dll
        #
        # one copy to boot directory
        # icu
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icudt52.dll $<TARGET_FILE_DIR:boot_isql>/icudt52.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuin52.dll $<TARGET_FILE_DIR:boot_isql>/icuin52.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/icu/${arch}/Release/bin/icuuc52.dll $<TARGET_FILE_DIR:boot_isql>/icuuc52.dll
        # zlib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/extern/zlib/${arch}/zlib1.dll $<TARGET_FILE_DIR:boot_isql>/zlib1.dll
    )
endif()


########################################
# EXECUTABLE isql
########################################

add_executable              (isql ${isql_src} ${isql_generated_src_master} ${VERSION_RC})
target_link_libraries       (isql common yvalve)


########################################
# EXECUTABLE qli
########################################

file(GLOB qli_src "qli/*.cpp" "qli/*.h")
set(qli_generated_src
    qli/help.epp
    qli/meta.epp
    qli/proc.epp
    qli/show.epp
)
add_epp_suffix(qli_generated_src master)

add_executable          (qli ${qli_src} ${qli_generated_src_master} ${VERSION_RC})
target_link_libraries   (qli common yvalve)


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
    #ALL # uncomment this to copy files every build
    # databases
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GENERATED_DIR}/security.fdb ${output_dir}/security4.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GENERATED_DIR}/help.fdb ${output_dir}/help/help.fdb
    # configs, text files
    COMMAND sed "/@UDF_COMMENT@/d" < ${CMAKE_SOURCE_DIR}/builds/install/misc/firebird.conf.in > ${output_dir}/firebird.conf    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/databases.conf.in ${output_dir}/databases.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/fbintl.conf ${output_dir}/intl/fbintl.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/plugins.conf ${output_dir}/plugins.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/IPLicense.txt ${output_dir}/IPLicense.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/misc/IDPLicense.txt ${output_dir}/IDPLicense.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/utilities/ntrace/fbtrace.conf ${output_dir}/fbtrace.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/plugins/udr_engine/udr_engine.conf ${output_dir}/plugins/udr_engine.conf
    # udf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/extlib/ib_udf.sql ${output_dir}/UDF/ib_udf.sql
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/extlib/ib_udf2.sql ${output_dir}/UDF/ib_udf2.sql
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/extlib/fbudf/fbudf.sql ${output_dir}/UDF/fbudf.sql
    # docs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/CHANGELOG.md ${output_dir}/doc/CHANGELOG.md
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/README.md ${output_dir}/doc/README.md
    # examples
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/api ${output_dir}/examples/api
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/dbcrypt ${output_dir}/examples/dbcrypt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/examples/empbuild/employe2.sql ${output_dir}/examples/empbuild/employe2.sql
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NATIVE_BUILD_DIR}/examples/employe2.fdb ${output_dir}/examples/empbuild/employee.fdb
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/include/gen/iberror.h ${output_dir}/include/iberror.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/yvalve/perf.h ${output_dir}/include/perf.h
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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/arch-specific/win32/install_classic.bat ${output_dir}/install_classic.bat
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/arch-specific/win32/install_super.bat ${output_dir}/install_super.bat
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/builds/install/arch-specific/win32/uninstall.bat ${output_dir}/uninstall.bat        
        # examples
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/build_unix ${output_dir}/examples/build_unix
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/build_win32 ${output_dir}/examples/build_win32
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/examples/readme ${output_dir}/examples/readme.txt
        # headers
        COMMAND echo "#pragma message(\"Non-production version of ibase.h.\")" > ${output_dir}/include/ibase.tmp
        COMMAND echo "#pragma message(\"Using raw, unprocessed concatenation of header files.\")" >> ${output_dir}/include/ibase.tmp
    )
    set(files    
        ${CMAKE_SOURCE_DIR}/src/misc/ibase_header.txt
        ${CMAKE_SOURCE_DIR}/src/include/types_pub.h
        ${CMAKE_SOURCE_DIR}/src/common/dsc_pub.h
        ${CMAKE_SOURCE_DIR}/src/dsql/sqlda_pub.h
        ${CMAKE_SOURCE_DIR}/src/jrd/ibase.h
        ${CMAKE_SOURCE_DIR}/src/jrd/inf_pub.h
        ${CMAKE_SOURCE_DIR}/src/include/consts_pub.h
        ${CMAKE_SOURCE_DIR}/src/jrd/blr.h
        ${CMAKE_SOURCE_DIR}/src/include/gen/iberror.h
    )
    foreach(F ${files})
        string(REPLACE "/" "\\" f ${F})
        add_custom_command(TARGET copy_files POST_BUILD
            COMMAND type ${f} >> ${output_dir}/include/ibase.tmp)
    endforeach()
    add_custom_command(
        TARGET copy_files
        POST_BUILD
        COMMAND sed -f ${CMAKE_SOURCE_DIR}/src/misc/headers.sed < ${output_dir}/include/ibase.tmp > ${output_dir}/include/ibase.h
        COMMAND ${CMAKE_COMMAND} -E remove ${output_dir}/include/ibase.tmp
    )
    file(GLOB files
        "${CMAKE_SOURCE_DIR}/src/extlib/ib_udf*"
        "${CMAKE_SOURCE_DIR}/src/extlib/fbudf/*"
    )
    foreach(F ${files})
        get_filename_component(name ${F} NAME)
        add_custom_command(TARGET copy_files POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${output_dir}/examples/udf/${name})
    endforeach()
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

    endif() # if (NOT CMAKE_CROSSCOMPILING)
endif()


################################################################################
