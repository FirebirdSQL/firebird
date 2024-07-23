include(SourceGroups)

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

