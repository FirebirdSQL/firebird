include(SourceGroups)

###############################################################################
#
# examples
#
###############################################################################

########################################
# EXECUTABLE empbuild
########################################

if (NOT CMAKE_CROSSCOMPILING)

add_custom_command(
    OUTPUT empbuild.fdb
    DEPENDS
        engine
        isql
        ${CMAKE_CURRENT_SOURCE_DIR}/empbuild/empbld.sql
    COMMAND ${CMAKE_COMMAND} -E remove empbuild.fdb
    COMMAND ${ISQL_CMD} -q -i ${CMAKE_CURRENT_SOURCE_DIR}/empbuild/empbld.sql
    COMMAND ${CMAKE_COMMAND} -E copy_if_different empbuild.fdb empbuild.tmp
    COMMAND ${CMAKE_COMMAND} -E remove empbuild.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different empbuild.tmp empbuild.fdb
)
add_custom_command(
    OUTPUT empbuild.cpp
    DEPENDS
        gpre
        ${CMAKE_CURRENT_SOURCE_DIR}/empbuild/empbuild.epp
        empbuild.fdb
    COMMENT "Calling GPRE for empbuild.epp"
    COMMAND ${GPRE_CMD} -r -m -n ${CMAKE_CURRENT_SOURCE_DIR}/empbuild/empbuild.epp empbuild.cpp
)

add_executable          (empbuild empbuild.cpp empbuild.fdb ${CMAKE_CURRENT_SOURCE_DIR}/empbuild/empbuild.epp)
target_link_libraries   (empbuild yvalve)
set_output_directory    (empbuild . CURRENT_DIR)
add_dependencies_cc     (empbuild engine)
project_group           (empbuild Examples)

file(GLOB files
    "${CMAKE_CURRENT_SOURCE_DIR}/empbuild/*.sql"
    "${CMAKE_CURRENT_SOURCE_DIR}/empbuild/*.inp"
)
foreach(F ${files})
    get_filename_component(name ${F} NAME)
    add_custom_command(TARGET empbuild POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${F} ${name})
endforeach()


########################################
# BUILD STEP employee_db
########################################

add_custom_command(
    OUTPUT employe2.fdb
    DEPENDS
        empbuild
        gfix
        ${files}
    COMMAND ${CMAKE_COMMAND} -E remove employe2.fdb
    COMMAND ${EMPBUILD_CMD} employe2.fdb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different employe2.fdb empbuild/employee.fdb
)
add_custom_target       (employee_db ALL DEPENDS employe2.fdb SOURCES ${files})
project_group           (employee_db "Examples/Custom build steps")

endif() # (NOT CMAKE_CROSSCOMPILING)


########################################
# SHARED LIBRARY udrcpp_example
########################################

file(GLOB udrcpp_example_src "udr/*")

add_library             (udrcpp_example SHARED ${udrcpp_example_src} ${VERSION_RC})
set_target_properties   (udrcpp_example PROPERTIES RC_FLAGS -DRC_TARGET_udrcpp_example)
target_link_libraries   (udrcpp_example yvalve)
set_exported_symbols    (udrcpp_example udr_plugin)
set_output_directory    (udrcpp_example plugins/udr)
project_group           (udrcpp_example Examples)


########################################
# SHARED LIBRARY dbcrypt_example
########################################

set(dbcrypt_conf_src
    ${CMAKE_CURRENT_SOURCE_DIR}/dbcrypt/fbSampleDbCrypt.conf
)

add_library                 (dbcrypt_example SHARED dbcrypt/DbCrypt.cpp)
set_target_properties       (dbcrypt_example PROPERTIES OUTPUT_NAME fbSampleDbCrypt)
set_output_directory        (dbcrypt_example prebuilt/plugins CURRENT_DIR)
add_dependencies_cc         (dbcrypt_example UpdateCloopInterfaces)
project_group               (dbcrypt_example Examples)

add_custom_command(TARGET dbcrypt_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${dbcrypt_conf_src} ${output_dir}/examples/prebuilt/plugins/fbSampleDbCrypt.conf
)


########################################
# SHARED LIBRARY cryptkeyholder_example
########################################

set(cryptkeyholder_conf_src
    ${CMAKE_CURRENT_SOURCE_DIR}/dbcrypt/fbSampleKeyHolder.conf
)

add_library                 (cryptkeyholder_example SHARED dbcrypt/CryptKeyHolder.cpp)
set_target_properties       (cryptkeyholder_example PROPERTIES OUTPUT_NAME fbSampleKeyHolder)
set_output_directory        (cryptkeyholder_example prebuilt/plugins CURRENT_DIR)
add_dependencies_cc         (cryptkeyholder_example UpdateCloopInterfaces)
project_group               (cryptkeyholder_example Examples)

add_custom_command(TARGET cryptkeyholder_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${cryptkeyholder_conf_src} ${output_dir}/examples/prebuilt/plugins/fbSampleKeyHolder.conf
)


########################################
# SHARED LIBRARY extauth
########################################

add_library                 (extauth SHARED extauth/ExtAuth.cpp extauth/TcWrapper.cpp)
set_target_properties       (extauth PROPERTIES OUTPUT_NAME fbSampleExtAuth)
target_compile_definitions  (extauth PRIVATE LTM_DESC)
target_link_libraries       (extauth ${LIB_tomcrypt})
set_output_directory        (extauth prebuilt/plugins CURRENT_DIR)
project_group               (extauth Examples)


########################################
# EXECUTABLE extauthkeygen
########################################

add_executable              (extauthkeygen extauth/keygen.cpp extauth/TcWrapper.cpp)
set_target_properties       (extauthkeygen PROPERTIES OUTPUT_NAME fbSampleExtAuthKeygen)
target_compile_definitions  (extauthkeygen PRIVATE LTM_DESC)
target_link_libraries       (extauthkeygen ${LIB_tomcrypt})
set_output_directory        (extauthkeygen prebuilt/bin CURRENT_DIR)
project_group               (extauthkeygen Examples)


########################################
# EXECUTABLE dbcryptapp
########################################

add_executable              (dbcryptapp dbcrypt/CryptApplication.cpp)
set_target_properties       (dbcryptapp PROPERTIES OUTPUT_NAME fbSampleDbCryptApp)
target_link_libraries       (dbcryptapp yvalve)
set_output_directory        (dbcryptapp prebuilt/bin CURRENT_DIR)
project_group               (dbcryptapp Examples)


###############################################################################
