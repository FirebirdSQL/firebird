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
    yvalve/array.epp
    yvalve/blob.epp
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

    jrd/SystemTriggers.epp
    jrd/Package.epp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/msgs/sqlstates.sql
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
        boot_isql
        boot_gfix
    VERBATIM
    #
    COMMAND ${CMAKE_COMMAND} -E remove metadata.fdb
    COMMAND ${CMAKE_COMMAND} -E remove metadata_.fdb
    COMMAND ${CMAKE_COMMAND} -E echo "create database 'metadata_.fdb';" > create_db_metadata.sql
    COMMAND ${BOOT_ISQL_CMD} -q -i create_db_metadata.sql
    COMMAND ${BOOT_GFIX_CMD} -sql_dialect 1 metadata_.fdb
    COMMAND ${BOOT_GFIX_CMD} -mode read_only metadata_.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different metadata_.fdb metadata.fdb
)
add_custom_target(databases
    DEPENDS
        boot_engine
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
    COMMAND ${BUILD_MSG_CMD} -F ${CMAKE_CURRENT_BINARY_DIR}/firebird.msg 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different firebird.msg ${output_dir}/firebird.msg
)
# ???
# add_custom_command(
#     OUTPUT codes_headers.dummy
#     DEPENDS
#         # codes
#         msg_fdb
#     COMMENT ""
#     #
#     COMMAND ${CMAKE_COMMAND} -E echo "" > codes_headers.dummy
#     COMMAND ${CMAKE_COMMAND} -E echo Building codes header...
#     COMMAND ${CODES_CMD} ${CMAKE_CURRENT_SOURCE_DIR}/include/gen ${CMAKE_CURRENT_SOURCE_DIR}/../lang_helpers
# )
# add_custom_target(messages ALL DEPENDS firebird.msg codes_headers.dummy)
add_custom_target(messages ALL DEPENDS firebird.msg)
project_group(messages "Boot/Custom build steps")


###############################################################################
# BUILD STEP  boot_output_dir copy .conf files to boot dir
###############################################################################

set(boot_output_dir ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(copy_boot_confs
    # need for generate msg.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/fbintl.conf  ${boot_output_dir}/intl/fbintl.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:intl>                                                        ${boot_output_dir}/intl/    
    # need for generate employe2.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/fbintl.conf  ${output_dir}/intl/fbintl.conf
)
project_group(copy_boot_confs "Boot/Custom build steps")

########################################
# BUILD STEP msg_fd
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different msg_.fdb msg.fdb
)
add_custom_target(msg_fdb
    DEPENDS
        copy_boot_confs
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

    # So GNU & Mac will interpret this differently:
    # GNU : No extension is supplied immediately after the -i, so create no backup, use s/hello/bye/g as the text-editing command, and act on the file just_a_file.txt in-place.
    # Mac (BSD) : Use s/hello/bye/g as the backup file extension (!) and just_a_file.txt as the editing command (regular expression).
    # Result: the command code used is j (not a valid command code for substitution, e.g. s), hence we get the error invalid command code j.

    if (APPLE)
        set(sed_i_option -i '' -e)
    else()
        set(sed_i_option -i)
    endif()

    if (WIN32)
        set(btyacc_env "TMPDIR=$ENV{TMP}")
    else()
        set(btyacc_env "BTYACC_ENV_DUMMY=DUMMY")
    endif()

add_custom_command(
    OUTPUT y_tab.h y_tab.c
    DEPENDS
        btyacc
        ${parse_src}
    COMMAND sed -n "/%type .*/p" ${CMAKE_CURRENT_SOURCE_DIR}/dsql/parse.y > types.y
    COMMAND sed   "s/%type .*//" ${CMAKE_CURRENT_SOURCE_DIR}/dsql/parse.y > y.y
    COMMAND sed ${sed_i_option} "s/\\r//" types.y  # set EndOfLine to unux style
    COMMAND sed ${sed_i_option} "s/\\r//" y.y      # set EndOfLine to unux style
    COMMAND ${CMAKE_COMMAND} -E env "${btyacc_env}" -- ${CMAKE_CURRENT_BINARY_DIR}/btyacc -l -d -S ${CMAKE_CURRENT_SOURCE_DIR}/dsql/btyacc_fb.ske y.y
    COMMAND sed ${sed_i_option} "s/#define \\([A-Z].*\\)/#define TOK_\\1/g" y_tab.h
    COMMAND sed ${sed_i_option} "s/#define TOK_YY\\(.*\\)/#define YY\\1/g" y_tab.h

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

file(GLOB common_src 
"common/*.cpp" 
"common/classes/*.cpp" 
"common/config/*.cpp" 
"common/os/${OS_DIR}/*.cpp" 
"common/os/${OS_DIR}/*.m" 
"common/sha2/*.cpp")

file(GLOB_RECURSE common_include "common/*.h")

if (APPLE)
    file(GLOB common_os_src "common/os/posix/*.cpp")
    list(REMOVE_ITEM common_os_src ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/common/os/posix/mod_loader.cpp)
endif()

add_library                 (common ${common_src} ${common_os_src} ${common_include})
target_link_libraries       (common ${LIB_mpr} ${LIB_tommath} ${LIB_tomcrypt} decNumber   re2)
add_dependencies_cc         (common UpdateCloopInterfaces)

if (UNIX)
    target_link_libraries       (common dl)
endif()

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
    PUBLIC ${LIB_Ws2_32}
    PUBLIC ${LIB_readline}
    PUBLIC ${LIB_tinfo}
    PUBLIC ${LIB_dl}
    PUBLIC ${LIB_iconv}
    PUBLIC ${LIB_CoreFoundation}
    PUBLIC ${LIB_tommath}
    PUBLIC ${LIB_tomcrypt} 
)
if (LINUX)
    add_dependencies            (boot_yvalve editline-static)
endif()
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
    PUBLIC ${LIB_Ws2_32}
    PUBLIC ${LIB_readline}
    PUBLIC ${LIB_tinfo}
    PUBLIC ${LIB_dl}
    PUBLIC ${LIB_iconv}
    PUBLIC ${LIB_CoreFoundation}
)
if (LINUX)
    add_dependencies        (yvalve editline-static)
endif()
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
    "jrd/optimizer/*.cpp"
    "jrd/recsrc/*.cpp"
    "jrd/replication/*.cpp"
    "jrd/trace/*.cpp"
    "jrd/Package.h"
    "jrd/os/${OS_DIR}/*.cpp"
    "jrd/sys-packages/*.cpp"

    "yvalve/*.cpp"
    "yvalve/config/os/${OS_DIR}/*.c*"
    
    "remote/os/${OS_DIR}/*.cpp"
    "remote/*.cpp"
    "remote/client/*.cpp"
    "common/classes/ThreadCleanup.cpp"
)
set(engine_src ${engine_src}
    lock/lock.cpp
    utilities/gsec/gsec.cpp
    utilities/gstat/ppg.cpp
    utilities/nbackup/nbackup.cpp
    # parse
    ${GENERATED_DIR}/dsql/parse.cpp


    auth/SecurityDatabase/LegacyClient.cpp
    auth/SecureRemotePassword/client/SrpClient.cpp
    auth/SecureRemotePassword/srp.cpp

    auth/trusted/AuthSspi.cpp
    plugins/crypt/arc4/Arc4.cpp

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

    jrd/SystemTriggers.epp
    jrd/Package.epp
)
add_epp_suffix(engine_generated_src boot)
add_epp_suffix(engine_generated_src master)

file(GLOB_RECURSE engine_include "dsql/*.h" "jrd/*.h" include/gen/iberror.h)

add_library                 (engine_common ${engine_src} ${engine_include} ${parse_src})
add_dependencies_cc         (engine_common parse UpdateCloopInterfaces)

if (NOT CMAKE_CROSSCOMPILING)

add_library                 (boot_engine SHARED ${engine_generated_src_boot} ${VERSION_RC})
target_link_libraries       (boot_engine engine_common boot_alice boot_burp common boot_yvalve ${LIB_cds} ${LIB_atomic_ID})
set_output_directory        (boot_engine plugins CURRENT_DIR)
set_exported_symbols        (boot_engine fbplugin)
copy_and_rename_lib         (boot_engine Engine14)
project_group               (boot_engine Boot)
add_dependencies            (boot_engine srp)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# SHARED LIBRARY engine
########################################

add_library                 (engine SHARED ${engine_generated_src_master} ${VERSION_RC})
target_link_libraries       (engine engine_common alice burp common yvalve yvalve_common ${LIB_cds} ${LIB_atomic_ID})
set_target_properties       (engine PROPERTIES OUTPUT_NAME Engine14)
set_output_directory        (engine plugins)
set_exported_symbols        (engine fbplugin)


###############################################################################
# SHARED LIBRARY intl
###############################################################################

file(GLOB intl_src "intl/*.cpp" "intl/*.h")

add_library                 (intl SHARED ${intl_src} ${VERSION_RC})
target_link_libraries       (intl common yvalve yvalve_common remote)
set_target_properties       (intl PROPERTIES OUTPUT_NAME fbintl)
set_output_directory        (intl intl)


########################################
# SHARED LIBRARY ib_util
########################################

add_library                 (ib_util SHARED extlib/ib_util.cpp ${VERSION_RC})
set_exported_symbols        (ib_util ib_util)
set_output_directory_unix   (ib_util lib)


########################################
# SHARED LIBRARY legacy_usermanager
########################################

set(legacy_usermanager_generated_src
    auth/SecurityDatabase/LegacyManagement.epp
)
add_epp_suffix(legacy_usermanager_generated_src master)

add_library                 (legacy_usermanager SHARED ${legacy_usermanager_generated_src_master} auth/SecurityDatabase/LegacyManagement.h ${VERSION_RC})
target_link_libraries       (legacy_usermanager common yvalve yvalve_common remote)
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
target_link_libraries       (udr_engine common yvalve yvalve_common remote)
set_output_directory        (udr_engine plugins)
set_exported_symbols        (udr_engine fbplugin)


########################################
# SHARED LIBRARY srp
########################################

set(srp_src 
    auth/SecureRemotePassword/manage/SrpManagement.cpp
)

add_library                 (srp SHARED ${srp_src} ${VERSION_RC})
target_link_libraries       (srp remote common yvalve yvalve_common)
set_target_properties       (srp PROPERTIES OUTPUT_NAME Srp)
set_output_directory        (srp plugins)
set_exported_symbols        (srp fbplugin)


########################################
# SHARED LIBRARY legacy_auth
########################################

set(legacy_auth_src
    auth/SecurityDatabase/LegacyServer.cpp
    auth/SecDbCache.cpp
)

add_library                 (legacy_auth SHARED ${legacy_auth_src}  ${VERSION_RC})
target_link_libraries       (legacy_auth common yvalve yvalve_common srp  remote)
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
target_link_libraries       (gpre_boot gpre_common common ${LIB_iconv} ${LIB_Ws2_32} ${LIB_dl})
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

file(GLOB gpre_src
    ${yvalve_src}    
    remote/*.cpp
    remote/os/${OS_DIR}/xnet.cpp

    auth/SecureRemotePassword/srp.cpp
    auth/trusted/AuthSspi.cpp
)


if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gpre ${gpre_generated_src_boot} ${gpre_src} ${VERSION_RC})
target_link_libraries       (boot_gpre gpre_common common boot_yvalve )
set_output_directory        (boot_gpre . CURRENT_DIR)
project_group               (boot_gpre Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gpre
########################################

add_executable              (gpre ${gpre_generated_src_master} ${VERSION_RC})
target_link_libraries       (gpre gpre_common common yvalve  yvalve_common remote)


########################################
# EXECUTABLE build_msg
########################################

set(build_msg_src
    msgs/build_file.cpp
)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (build_msg ${build_msg_src} ${VERSION_RC})
target_link_libraries       (build_msg boot_yvalve)
set_output_directory        (build_msg . CURRENT_DIR)
project_group               (build_msg Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE codes
########################################


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
target_link_libraries       (gstat common yvalve yvalve_common srp remote)
add_dependencies_cc         (gstat engine)


########################################
# EXECUTABLE fb_lock_print
########################################

add_executable              (fb_lock_print lock/print.cpp ${VERSION_RC})
target_link_libraries       (fb_lock_print common yvalve yvalve_common remote)


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
target_link_libraries       (fbguard common yvalve yvalve_common srp ${LIB_comctl32} ${LIB_version})


########################################
# EXECUTABLE fbtracemgr
########################################

set(fbtracemgr_src
    jrd/trace/TraceCmdLine.cpp
    utilities/fbtracemgr/traceMgrMain.cpp
)

add_executable              (fbtracemgr ${fbtracemgr_src} ${VERSION_RC})
target_link_libraries       (fbtracemgr common yvalve remote yvalve_common)


########################################
# EXECUTABLE boot_gfix
########################################

file(GLOB gfix_src     
    alice/main/aliceMain.cpp
    remote/*.cpp
    remote/os/${OS_DIR}/xnet.cpp
    auth/SecureRemotePassword/srp.cpp
    auth/trusted/AuthSspi.cpp
)

if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gfix ${gfix_src} $<TARGET_OBJECTS:yvalve_common> ${VERSION_RC})
target_link_libraries       (boot_gfix boot_alice common yvalve boot_yvalve)
set_output_directory        (boot_gfix . CURRENT_DIR)
project_group               (boot_gfix Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gfix
########################################

add_executable              (gfix alice/main/aliceMain.cpp ${VERSION_RC})
target_link_libraries       (gfix alice common yvalve yvalve_common remote)


########################################
# EXECUTABLE boot_gbak
########################################

file(GLOB gbak_include "burp/*.h")

file(GLOB gbak_src     
    remote/*.cpp
    remote/os/${OS_DIR}/xnet.cpp
    auth/SecureRemotePassword/srp.cpp
    auth/trusted/AuthSspi.cpp
)


if (NOT CMAKE_CROSSCOMPILING)

add_executable              (boot_gbak burp/main/burpMain.cpp $<TARGET_OBJECTS:yvalve_common> ${gbak_src} ${gbak_include} ${VERSION_RC})
target_link_libraries       (boot_gbak boot_burp common boot_yvalve)
set_output_directory        (boot_gbak . CURRENT_DIR)
project_group               (boot_gbak Boot)

endif() # if (NOT CMAKE_CROSSCOMPILING)


########################################
# EXECUTABLE gbak
########################################

add_executable              (gbak burp/main/burpMain.cpp ${gbak_include} ${VERSION_RC})
target_link_libraries       (gbak burp common yvalve yvalve_common remote)


########################################
# EXECUTABLE gsplit
########################################

add_executable              (gsplit burp/split/spit.cpp burp/split/spit.h ${VERSION_RC})
target_link_libraries       (gsplit burp common yvalve  yvalve_common remote)


########################################
# EXECUTABLE boot_isql
########################################

file(GLOB isql_src "isql/*.cpp" "isql/*.h" 
   "yvalve/*.cpp"
   "yvalve/config/os/${OS_DIR}/*.c*"
   "remote/os/${OS_DIR}/*.cpp"
   "remote/client/*.cpp"
   "remote/*.cpp"

    auth/SecurityDatabase/LegacyClient.cpp
    auth/SecureRemotePassword/client/SrpClient.cpp
    auth/SecureRemotePassword/srp.cpp
    auth/trusted/AuthSspi.cpp
    plugins/crypt/arc4/Arc4.cpp
)

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

    set(ZLIB_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../extern/zlib/${arch}")

    set(ICU_BINARY_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../extern/icu/${arch}/Release/bin")
    set(ICU_DATA_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/../extern/icu")

    if (EXISTS "${ICU_DATA_DIR}/icudt${ICU_VERSION}l.dat")
    
        add_custom_command(
            TARGET boot_isql
            POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_DATA_DIR}/icudt${ICU_VERSION}l.dat ${output_dir}/icudt${ICU_VERSION}l.dat
                # one copy to boot directory
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_DATA_DIR}/icudt${ICU_VERSION}l.dat $<TARGET_FILE_DIR:boot_isql>/icudt${ICU_VERSION}l.dat
        )

    endif()


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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_BINARY_DIR}/icudt${ICU_VERSION}.dll ${output_dir}/icudt${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_BINARY_DIR}/icuin${ICU_VERSION}.dll ${output_dir}/icuin${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_BINARY_DIR}/icuuc${ICU_VERSION}.dll ${output_dir}/icuuc${ICU_VERSION}.dll
        # zlib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZLIB_BINARY_DIR}/zlib1.dll ${output_dir}/zlib1.dll
        #
        # one copy to boot directory
        # icu
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_BINARY_DIR}/icudt${ICU_VERSION}.dll $<TARGET_FILE_DIR:boot_isql>/icudt${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_BINARY_DIR}/icuin${ICU_VERSION}.dll $<TARGET_FILE_DIR:boot_isql>/icuin${ICU_VERSION}.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICU_BINARY_DIR}/icuuc${ICU_VERSION}.dll $<TARGET_FILE_DIR:boot_isql>/icuuc${ICU_VERSION}.dll
        # zlib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZLIB_BINARY_DIR}/zlib1.dll $<TARGET_FILE_DIR:boot_isql>/zlib1.dll
    )
else()

    if (EXISTS "${ICU_DIR_distr}")

        add_custom_command(
            TARGET boot_isql
            POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${ICU_DIR_distr}/ ${output_dir}/lib
        )

    endif()

endif()


########################################
# EXECUTABLE isql
########################################

add_executable              (isql ${isql_src} ${isql_generated_src_master} ${VERSION_RC})
target_link_libraries       (isql common yvalve)


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
    ALL # uncomment this to copy files every build
    # databases

    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GENERATED_DIR}/security.fdb ${output_dir}/security6.fdb
    # configs, text files
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/firebird.conf ${output_dir}/${FB_CONFILENAME}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/databases.conf ${output_dir}/databases.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/fbintl.conf ${output_dir}/intl/fbintl.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/plugins.conf ${output_dir}/plugins.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/IPLicense.txt ${output_dir}/IPLicense.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/misc/IDPLicense.txt ${output_dir}/IDPLicense.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/utilities/ntrace/fbtrace.conf ${output_dir}/fbtrace.conf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/plugins/udr_engine/udr_engine.conf ${output_dir}/plugins/udr_engine.conf
    # docs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/CHANGELOG.md ${output_dir}/doc/CHANGELOG.md
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/README.md ${output_dir}/doc/README.md
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/doc ${output_dir}/doc

    # examples
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/api ${output_dir}/examples/api
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/dbcrypt ${output_dir}/examples/dbcrypt
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/dbcrypt ${output_dir}/examples/extauth    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/empbuild/employe2.sql ${output_dir}/examples/empbuild/employe2.sql
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NATIVE_BUILD_DIR}/examples/employe2.fdb ${output_dir}/examples/empbuild/employee.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/include ${output_dir}/examples/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/interfaces ${output_dir}/examples/interfaces
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/package ${output_dir}/examples/package
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/stat ${output_dir}/examples/stat
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/udf ${output_dir}/examples/udf
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/udr ${output_dir}/examples/udr
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/stat ${output_dir}/examples/stat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/functions.c ${output_dir}/examples/functions.c
    # headers
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ibase.h          ${output_dir}/include/ibase.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/iberror.h        ${output_dir}/include/iberror.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/misc/pascal/ib_util.pas  ${output_dir}/include/ib_util.pas    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/gen/Firebird.pas ${output_dir}/include/firebird/Firebird.pas    
    COMMAND ${CMAKE_COMMAND} -E copy_directory    ${CMAKE_CURRENT_SOURCE_DIR}/include/firebird        ${output_dir}/include/firebird
    # misc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/misc/intl.sql  ${output_dir}/misc/intl.sql    
    COMMAND ${CMAKE_COMMAND} -E copy_directory    ${CMAKE_CURRENT_SOURCE_DIR}/misc/upgrade/v3.0    ${output_dir}/misc/upgrade/v3.0
    COMMAND ${CMAKE_COMMAND} -E copy_directory    ${CMAKE_CURRENT_SOURCE_DIR}/misc/upgrade/v4.0    ${output_dir}/misc/upgrade/v4.0
    # tzdata
)
add_dependencies_cc     (copy_files databases employee_db) 
project_group(copy_files "Custom build steps")

# # headers
# file(GLOB files "${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/firebird/*.h")
# foreach(F ${files})
#     get_filename_component(name ${F} NAME)
#     add_custom_command(TARGET copy_files POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${output_dir}/include/firebird/${name})
# endforeach()

# docs
file(GLOB files "${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/doc/README.*")
foreach(F ${files})
    get_filename_component(name ${F} NAME)
    add_custom_command(TARGET copy_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${output_dir}/doc/${name})
endforeach()

file(GLOB files "${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/doc/sql.extensions/README.*")
foreach(F ${files})
    get_filename_component(name ${F} NAME)
    add_custom_command(TARGET copy_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${output_dir}/doc/sql.extensions/${name})
endforeach()

if (WIN32)
    set(fbclient_lib_src ${CMAKE_CURRENT_BINARY_DIR}/fbclient.lib)
    set(fbclient_lib_dst fbclient_ms.lib)
    set( ib_util_lib_src ${CMAKE_CURRENT_BINARY_DIR}/ib_util.lib)
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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/arch-specific/win32/install_service.bat ${output_dir}/install_service.bat
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/builds/install/arch-specific/win32/uninstall_service.bat ${output_dir}/uninstall_service.bat
        # examples
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/build_unix ${output_dir}/examples/build_unix
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/build_win32 ${output_dir}/examples/build_win32
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../examples/readme ${output_dir}/examples/readme.txt
        # headers
        # COMMAND echo "#pragma message(\"Non-production version of ibase.h.\")" > ${output_dir}/include/ibase.tmp
        # COMMAND echo "#pragma message(\"Using raw, unprocessed concatenation of header files.\")" >> ${output_dir}/include/ibase.tmp
    )
    set(files
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/misc/ibase_header.txt
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/fb_types.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/common/dsc_pub.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/dsql/sqlda_pub.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/jrd/ibase.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/jrd/inf_pub.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/dyn_consts.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/jrd/blr.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/gen/iberror.h
    )
    # foreach(F ${files})
    #     add_custom_command(TARGET copy_files POST_BUILD
    #         COMMAND type ${f} >> ${output_dir}/include/ibase.tmp)
    # endforeach()
endif()


if (UNIX)
    add_custom_command(TARGET copy_files POST_BUILD
        # examples
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/examples/readme ${output_dir}/examples/README
    )

    if (NOT CMAKE_CROSSCOMPILING)

    set(files
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/fb_types.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/dyn_consts.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/ibase.h
        ${CMAKE_SOURCE_DIR}${OWNER_RROJECTS_TREE}/src/include/iberror.h
    )
    foreach(F ${files})
        get_filename_component(name ${F} NAME)
        add_custom_command(TARGET copy_files POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${name})
    endforeach()

    endif() # if (NOT CMAKE_CROSSCOMPILING)
endif()


################################################################################
#  common test
################################################################################
file(GLOB common_test_src     
    common/tests/*.h
    common/tests/*.cpp
    common/classes/tests/*.h
    common/classes/tests/*.cpp
    common/ipc/tests/*.h
    common/ipc/tests/*.cpp
)
set(common_test_src 
    ${common_test_src} 
    yvalve/gds.cpp
    )
add_executable             (common_test ${common_test_src})
target_link_libraries      (common_test common yvalve)
set_tests_directory        (common_test)
project_group              (common_test Tests)
################################################################################
#  Engine test
################################################################################
file(GLOB Engine_test_src 
    jrd/tests/*.cpp 
    jrd/tests/*.h 
    lock/tests/*.cpp 
    lock/tests/*.h
    )

add_executable             (libEngine14_test ${Engine_test_src} ${engine_generated_src_boot} )
target_link_libraries      (libEngine14_test engine_common alice  burp common yvalve ${LIB_cds} ${LIB_atomic_ID})
target_link_directories    (libEngine14_test PUBLIC ${output_dir}/lib)
set_tests_directory        (libEngine14_test)
project_group              (libEngine14_test Tests)

################################################################################
# isql test
################################################################################
file(GLOB isql_test_src 
    isql/tests/*.h 
    isql/tests/*.cpp
    )
set(isql_test_src 
    ${isql_test_src} 
    ${isql_src} 
    ${isql_generated_src_boot} 
    )
list(REMOVE_ITEM isql_test_src "${CMAKE_CURRENT_SOURCE_DIR}/isql/main.cpp")

add_executable             (isql_test ${isql_test_src})
target_link_libraries      (isql_test common boot_yvalve)
set_tests_directory        (isql_test)
project_group              (isql_test Tests)

################################################################################
# run tests
################################################################################

add_custom_target(run_tests
    ALL
    COMMAND  common_test       --log_level=error
    COMMAND  libEngine14_test  --log_level=error
    COMMAND  isql_test         --log_level=error
)
project_group        (run_tests  Tests)
add_dependencies     (run_tests common_test libEngine14_test isql_test)
add_dependencies     (run_tests copy_files) 


################################################################################
# distribytive
################################################################################


make_directory( "${CMAKE_BINARY_DIR}/../distr")

add_custom_target(make_distribytive
    ALL
    COMMAND ${CMAKE_COMMAND} -E remove cloop.*  WORKING_DIRECTORY "${output_dir}"
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_BINARY_DIR}/../distr/${PROJECT_NAME}-${PROJECT_VERSION_FULL}-${platform_ID}.zip" --format=zip -- . WORKING_DIRECTORY "${output_dir}"
)
project_group        (make_distribytive distribytive)
add_dependencies     (make_distribytive copy_files) 


