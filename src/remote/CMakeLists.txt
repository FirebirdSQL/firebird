
###############################################################################
# LIBRARY remote
###############################################################################

set(remote_src
    merge.cpp
    parser.cpp
    protocol.cpp
    remote.cpp
    inet.cpp

    remote.h

     ../common/xdr.cpp
     ../common/sdl.cpp
     ../common/classes/zip.cpp
     ../common/Auth.cpp

    ../auth/SecureRemotePassword/srp.cpp
    ../auth/SecureRemotePassword/srp.h
    ../auth/SecureRemotePassword/Message.h
    ../auth/trusted/AuthSspi.cpp
)
add_src_win32(remote_src
    os/win32/xnet.cpp
    os/win32/xnet.h
    os/win32/xnet_proto.h
)
file(GLOB remote_include "*.h")

add_library             (remote ${remote_src} ${remote_include})
add_dependencies_cc     (remote UpdateCloopInterfaces)


###############################################################################
# EXECUTABLE fbserver
###############################################################################

set(fbserver_src
    ../auth/SecureRemotePassword/server/SrpServer.cpp
    ../auth/SecureRemotePassword/server/SrpServer.h
    ../auth/SecDbCache.cpp   
    server/ReplServer.cpp
    server/server.cpp

    ../common/classes/SafeArg.cpp
    ../common/classes/MsgPrint.cpp
    ../common/cvt.cpp
    ../common/IntlParametersBlock.cpp
    ../common/classes/BlrWriter.cpp
    ../common/call_service.cpp
    ../common/Tokens.cpp
)
add_src_win32(fbserver_src
    server/os/win32/chop.cpp
    server/os/win32/cntl.cpp
    server/os/win32/property.cpp
    server/os/win32/window.cpp
    server/os/win32/srvr_w32.cpp
    server/os/win32/server.ico
    server/os/win32/caution.ico
    server/os/win32/window.rc
    # server/ReplServer.cpp
)
add_src_unix(fbserver_src
    ../auth/SecureRemotePassword/srp.cpp
    server/os/posix/inet_server.cpp

    inet.cpp
    merge.cpp
    parser.cpp
    protocol.cpp
    remote.cpp
)

add_executable              (fbserver WIN32 ${fbserver_src})
target_link_libraries       (fbserver remote common yvalve engine_common ${LIB_comctl32})
set_target_properties       (fbserver PROPERTIES OUTPUT_NAME firebird)
