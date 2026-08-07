################################################################################
#
# macros and functions
#
################################################################################


function(replace_in_file SOURCE TARGET FILE_PATH)
    file(READ "${FILE_PATH}" FILE_CONTENT)
    string(REPLACE "${SOURCE}" "${TARGET}" FILE_CONTENT "${FILE_CONTENT}" )
    file(WRITE "${FILE_PATH}" "${FILE_CONTENT}")
endfunction()


function(fix_win_crlf FILE_PATH)
    if (NOT WIN32)
        replace_in_file("\\r\\n" "\\n" "${FILE_PATH}")
    endif()        
endfunction()

function(crlf_dos_to_unix FILE_PATH)

    file(GLOB_RECURSE files_all "${FILE_PATH}/*")

    foreach(file_path ${files_all})
        replace_in_file("\\r\\n" "\\n" "${file_path}")
        # execute_process(COMMAND sed -i "s/\r//g" ${file_path}  WORKING_DIRECTORY ${FILE_PATH})
    endforeach()

endfunction()


########################################
# FUNCTION set_output_directory
########################################
function(set_output_directory target dir)
    set(out ${output_dir})
    if (MSVC OR XCODE) # multiconfiguration builds
        set(out ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
    if ("${ARGV2}" STREQUAL "CURRENT_DIR")
        set(out .)
        if (UNIX AND "${dir}" STREQUAL ".")
            set(dir bin)
        endif()
    endif()
    if (MSVC OR XCODE)
        foreach(conf ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${conf} conf2)
            set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${conf2} ${out}/${conf}/${dir})
            set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${conf2} ${out}/${conf}/${dir})
        endforeach()
    else() # single configuration
        add_custom_command(TARGET ${target} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${out}/${dir})
        set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${out}/${dir})
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${out}/${dir})
    endif()
endfunction(set_output_directory)


########################################
# FUNCTION set_output_directory
########################################
function(set_tests_directory target)
    add_custom_command(TARGET ${target} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${test_dir})
    set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${test_dir})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${test_dir})
    set_target_properties(${target} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${test_dir})
endfunction(set_tests_directory)


########################################
# FUNCTION set_output_directory_unix
########################################
function(set_output_directory_unix target dir)
    if (UNIX)
        set_output_directory(${target} ${dir} ${ARGN})
    endif()
endfunction(set_output_directory_unix)

########################################
# FUNCTION set_exported_symbols
########################################
if (WIN32)
    function(set_exported_symbols target filename)
        set(def_file ${filename}.def)
        if ("${filename}" STREQUAL "empty")
            set(def_file)
        elseif("${filename}" STREQUAL "fbplugin")
            set(def_file "plugin.def")
        endif()
        if (NOT "${def_file}" STREQUAL "")
            if (MSVC)
                set_target_properties(${target} PROPERTIES LINK_FLAGS "/DEF:\"${CMAKE_SOURCE_DIR}/builds/win32/defs/${def_file}\"")
            else()    
                # set_target_properties(${target} PROPERTIES LINK_FLAGS "-Wl,${CMAKE_SOURCE_DIR}/builds/win32/defs/${def_file}")
            endif()
        endif()
    endfunction(set_exported_symbols)
endif()

if (UNIX)
    function(set_exported_symbols target filename)
        set(def_file ${filename}.vers)
        if ("${filename}" STREQUAL "ib_udf")
            set(def_file)
        endif()
        if (NOT "${def_file}" STREQUAL "")
            set(wl_option "--version-script")
            if (APPLE)
                set(wl_option "-exported_symbols_list")
            endif()

            set(VERS_DIR "${CMAKE_BINARY_DIR}${OWNER_RROJECTS_TREE}")
            # message(" ---- ${VERS_DIR} ")
            set_target_properties(${target} PROPERTIES LINK_FLAGS -Wl,${wl_option},${VERS_DIR}/builds/posix/${def_file})
        endif()
    endfunction(set_exported_symbols)
endif(UNIX)


########################################
# FUNCTION epp_process
########################################
function(epp_process type files)
    set(epp_suffix ".${type}.cpp")

    foreach(F ${${files}})
        set(in  ${CMAKE_CURRENT_SOURCE_DIR}/${F})
        set(out ${CMAKE_CURRENT_BINARY_DIR}/${F}${epp_suffix})

        get_filename_component(dir ${out} PATH)
        if (MSVC OR XCODE)
            set(dir ${dir}/$<CONFIG>)
        endif()

        if ("${type}" STREQUAL "boot")
            add_custom_command(
                OUTPUT ${out}
                DEPENDS gpre_boot ${in}
                COMMENT "Calling GPRE boot for ${F}"
                #
                COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}
                COMMAND ${ARGN} ${in} ${out}
            )
        elseif ("${type}" STREQUAL "master")
            get_filename_component(file ${out} NAME)
            set(dir ${dir}/${file}.d)
            add_custom_command(
                OUTPUT ${out}
                DEPENDS databases boot_gpre ${in}
                COMMENT "Calling GPRE master for ${F}"
                #
                COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different metadata.fdb ${dir}/yachts.lnk
                COMMAND ${CMAKE_COMMAND} -E copy_if_different security.fdb ${dir}/security.fdb
                COMMAND ${CMAKE_COMMAND} -E copy_if_different msg.fdb ${dir}/msg.fdb
                COMMAND ${ARGN} -b ${dir}/ ${in} ${out}
            )
        endif()
    endforeach()
endfunction(epp_process)

########################################
# FUNCTION add_epp_suffix
########################################
function(add_epp_suffix files suffix)
    foreach(F ${${files}})
        list(APPEND ${files}_${suffix} ${CMAKE_CURRENT_SOURCE_DIR}/${F})
        list(APPEND ${files}_${suffix} ${GENERATED_DIR}/${F}.${suffix}.cpp)
    endforeach()
    set_source_files_properties(${${files}_${suffix}} PROPERTIES GENERATED TRUE)
    set(${files}_${suffix} ${${files}_${suffix}} PARENT_SCOPE)
endfunction(add_epp_suffix)

########################################
# FUNCTION set_win32
########################################
function(set_win32 var)
    if (WIN32)
        set(${var} "${ARGN}" PARENT_SCOPE)
    endif()
endfunction(set_win32)

########################################
# FUNCTION set_unix
########################################
function(set_unix var)
    if (UNIX)
        set(${var} "${ARGN}" PARENT_SCOPE)
    endif()
endfunction(set_unix)

########################################
# FUNCTION set_apple
########################################
function(set_apple var)
    if (APPLE)
        set(${var} "${ARGN}" PARENT_SCOPE)
    endif()
endfunction(set_apple)

########################################
# FUNCTION add_src_win32
########################################
function(add_src_win32 var)
    if (WIN32)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_win32)

########################################
# FUNCTION add_src_unix
########################################
function(add_src_unix var)
    if (UNIX)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_unix)

########################################
# FUNCTION add_src_unix_not_apple
########################################
function(add_src_unix_not_apple var)
    if (UNIX AND NOT APPLE)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_unix_not_apple)

########################################
# FUNCTION add_src_apple
########################################
function(add_src_apple var)
    if (APPLE)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_apple)

########################################
# FUNCTION copy_and_rename_lib
########################################
function(copy_and_rename_lib target name)
    set(name2 $<TARGET_FILE_DIR:${target}>/${CMAKE_SHARED_LIBRARY_PREFIX}${name}${CMAKE_SHARED_LIBRARY_SUFFIX})
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${target}> ${name2}
    )
endfunction(copy_and_rename_lib)

########################################
# FUNCTION project_group
########################################
function(project_group target name)
    set_target_properties(${target} PROPERTIES FOLDER ${name})
endfunction(project_group)

########################################
# FUNCTION set_generated_directory
########################################
function(set_generated_directory)
    if (NOT CMAKE_CROSSCOMPILING)
        set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
    else()
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}" "${NATIVE_BUILD_DIR}" GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR})
        set(GENERATED_DIR ${GENERATED_DIR} PARENT_SCOPE)
    endif()
endfunction(set_generated_directory)

########################################
# FUNCTION add_dependencies_cc (cross compile)
########################################
function(add_dependencies_cc target)
    if (NOT CMAKE_CROSSCOMPILING)
        add_dependencies(${target} ${ARGN})
    endif()
endfunction(add_dependencies_cc)

########################################
# FUNCTION add_dependencies_unix_cc (cross compile)
########################################
function(add_dependencies_unix_cc target)
    if (UNIX)
        add_dependencies_cc(${target} ${ARGN})
    endif()
endfunction(add_dependencies_unix_cc)

########################################
# FUNCTION crosscompile_prebuild_steps
########################################
function(crosscompile_prebuild_steps)
    if (CMAKE_CROSSCOMPILING)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NATIVE_BUILD_DIR}/src/include/gen/parse.h ${CMAKE_CURRENT_BINARY_DIR}/src/include/gen/parse.h)
    endif()
endfunction(crosscompile_prebuild_steps)

########################################
# FUNCTION create_command
########################################
function(create_command command type out)
    set(dir ${output_dir})
    if ("${type}" STREQUAL "boot")
        set(dir ${boot_dir})
    endif()

    set_win32(env "PATH=${dir}\;%PATH%")
    set_unix (env "PATH=${dir}/bin:$PATH")
    set(env "${env}"
        FIREBIRD=${dir}
    )

    set(cmd_name ${command})
    if (MSVC OR XCODE)
        set(conf _$<CONFIG>)
    endif()

    set(pre_cmd)
    set(ext .sh)
    set(export export)
    set(options $*)
    set(perm)
    if (WIN32)
        set(pre_cmd @)
        set(ext .bat)
        set(export set)
        set(options %*)
    endif()
    set(cmd_name ${cmd_name}${conf}${ext})
    set(cmd_name ${CMAKE_CURRENT_BINARY_DIR}/src/${cmd_name})

    set(content)
    foreach(e ${env})
        set(content "${content}${pre_cmd}${export} ${e}\n")
    endforeach()

    set(cmd $<TARGET_FILE:${cmd}>)
    set(content "${content}${pre_cmd}${cmd} ${options}")
    file(GENERATE OUTPUT ${cmd_name} CONTENT "${content}")

    if (UNIX)
        set(cmd_name chmod u+x ${cmd_name} COMMAND ${cmd_name})
    endif()

    string(TOUPPER ${command} CMD)
    set(${CMD}_CMD ${cmd_name} PARENT_SCOPE)
    set(${out} ${CMD}_CMD PARENT_SCOPE)
endfunction(create_command)

########################################
# FUNCTION create_boot_commands
########################################
function(create_boot_commands)
    set(cmd_list
        boot_isql
        boot_gpre
        boot_gbak
        boot_gfix
        build_msg
        # codes
        gpre_boot
    )
    foreach(cmd ${cmd_list})
        create_command(${cmd} boot out)
        set(${out} ${${out}} PARENT_SCOPE)
    endforeach()
endfunction(create_boot_commands)

########################################
# FUNCTION create_master_commands
########################################
function(create_master_commands)
    set(cmd_list
        isql
        gpre
        empbuild
    )
    foreach(cmd ${cmd_list})
        create_command(${cmd} master out)
        set(${out} ${${out}} PARENT_SCOPE)
    endforeach()
endfunction(create_master_commands)

################################################################################

function(display_inventory)

    message(STATUS "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||")
    message(STATUS "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||")
    message(STATUS "       project : ${CMAKE_PROJECT_NAME}")    
    message(STATUS "       version : major ${PROJECT_VERSION_MAJOR} minor ${PROJECT_VERSION_MINOR} patch ${PROJECT_VERSION_PATCH} build ${PROJECT_VERSION_BUILD}")
    message(STATUS "      IPC name : ${FB_IPC_NAME}")
    message(STATUS "      log name : ${FB_LOGFILENAME}")
    message(STATUS "     conf name : ${FB_CONFILENAME}")
    message(STATUS "  service name : ${FB_SERVICE_NAME}")
    message(STATUS "  service port : ${FB_SERVICE_PORT}")
    message(STATUS "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||")
    message(STATUS "   platform ID : ${platform_ID}")    
    message(STATUS "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||")
#    message(STATUS "toolchain file : ${CMAKE_TOOLCHAIN_FILE}")
    message(STATUS "           CPU : ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "            OS : ${CMAKE_SYSTEM_NAME}")
    message(STATUS "OS pointer len : ${CMAKE_SIZEOF_VOID_P}")
    message(STATUS " debug/release : ${CMAKE_BUILD_TYPE}")
    message(STATUS "   generator`s : [${CMAKE_GENERATOR}]")
    message(STATUS "   source from : [${CMAKE_SOURCE_DIR}]")
    message(STATUS "   building in : [${CMAKE_BINARY_DIR}]")
    message(STATUS "   output   to : [${CMAKE_SOURCE_DIR}/distr]")
    message(STATUS "   by compiler : [${CMAKE_CXX_COMPILER_ID}]::[${CMAKE_BUILD_TYPE}]")
    message(STATUS "   rc compiler : [${CMAKE_RC_COMPILER}]")    
    message(STATUS "  asm compiler : [${CMAKE_ASM_COMPILER}]")    
    message(STATUS "   with linker : [${CMAKE_LINKER}]")    
    message(STATUS "   install  to : [${CMAKE_INSTALL_PREFIX}]")
    message(STATUS "   with  rpath : [${CMAKE_INSTALL_RPATH}]")
    message(STATUS "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||")
    message(STATUS "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||")
endfunction()
