
###############################################################################
# LIBRARY gpre_common
###############################################################################

add_definitions(
    -DGPRE_ADA
    -DGPRE_COBOL
    -DGPRE_FORTRAN
    -DGPRE_PASCAL
)

file(GLOB gpre_common_src "*.cpp" "languages/???.cpp" "*.h")
add_src_win32(gpre_common_src
    languages/fbrmclib.cpp
)

add_library                 (gpre_common ${gpre_common_src})
add_dependencies_cc         (gpre_common UpdateCloopInterfaces)


###############################################################################
# SHARED LIBRARY fbrmclib
###############################################################################

if (WIN32)
    add_library             (fbrmclib SHARED languages/fbrmclib.cpp ${VERSION_RC})
    target_link_libraries   (fbrmclib common yvalve yvalve_common srp)
    set_exported_symbols    (fbrmclib fbrmclib)
endif(WIN32)
