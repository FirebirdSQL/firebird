include(SourceGroups)

###############################################################################
# EXECUTABLE fbsvcmgr
###############################################################################

add_executable          (fbsvcmgr fbsvcmgr/fbsvcmgr.cpp ${VERSION_RC})
target_link_libraries   (fbsvcmgr common yvalve)
set_target_properties   (fbsvcmgr PROPERTIES RC_FLAGS -DRC_TARGET_fbsvcmgr)

###############################################################################
# EXECUTABLE nbackup
###############################################################################

set(nbackup_src
    ../jrd/ods.cpp
    nbackup/main/nbkMain.cpp
    nbackup/nbackup.cpp
)
file(GLOB nbackup_include "nbackup/*.h")

add_executable          (nbackup ${nbackup_src} ${nbackup_include} ${VERSION_RC})
target_link_libraries   (nbackup common yvalve)
set_target_properties   (nbackup PROPERTIES RC_FLAGS -DRC_TARGET_nbackup)


###############################################################################
# EXECUTABLE gsec
###############################################################################

set(gsec_src
    gsec/gsec.cpp
    gsec/main/gsecMain.cpp

    gsec/gsec.h
)

add_executable          (gsec ${gsec_src} ${VERSION_RC})
target_link_libraries   (gsec common yvalve)
set_target_properties   (gsec PROPERTIES RC_FLAGS -DRC_TARGET_gsec)


###############################################################################
# SHARED LIBRARY fbtrace
###############################################################################

set(fbtrace_src
    ntrace/PluginLogWriter.cpp
    ntrace/TraceConfiguration.cpp
    ntrace/traceplugin.cpp
    ntrace/TracePluginImpl.cpp

    ntrace/os/platform.h
)
add_src_win32(fbtrace_src ntrace/os/win32/platform.cpp)
add_src_unix (fbtrace_src ntrace/os/posix/platform.cpp)

file(GLOB fbtrace_include "ntrace/*.h")

add_library             (fbtrace SHARED ${fbtrace_src} ${fbtrace_os_src} ${fbtrace_include} ${VERSION_RC})
target_link_libraries   (fbtrace common yvalve)
set_output_directory    (fbtrace plugins)
set_exported_symbols    (fbtrace fbplugin)
set_target_properties   (fbtrace PROPERTIES RC_FLAGS -DRC_TARGET_fbtrace)


###############################################################################

if (WIN32)
    ###########################################################################
    # EXECUTABLE instreg
    ###########################################################################
    set(instreg_src
        install/install_reg.cpp
        install/registry.cpp

        install/registry.h
        install/regis_proto.h
    )
    add_executable              (instreg ${instreg_src} ${VERSION_RC})
    set_target_properties       (instreg PROPERTIES RC_FLAGS -DRC_TARGET_instreg)

    ###########################################################################
    # EXECUTABLE instsvc
    ###########################################################################
    set(instsvc_src
        install/install_svc.cpp
        install/services.cpp

        install/servi_proto.h
    )
    add_executable              (instsvc ${instsvc_src} ${VERSION_RC})
    target_link_libraries       (instsvc common yvalve)
    set_target_properties       (instsvc PROPERTIES RC_FLAGS -DRC_TARGET_instsvc)

    ###########################################################################
    # EXECUTABLE instclient
    ###########################################################################
    set(instclient_src
        install/install.cpp
        install/install_client.cpp

        install/install_proto.h
    )
    add_executable              (instclient ${instclient_src} ${VERSION_RC})
    target_link_libraries       (instclient version)
    set_target_properties       (instclient PROPERTIES RC_FLAGS -DRC_TARGET_instclient)
endif(WIN32)
