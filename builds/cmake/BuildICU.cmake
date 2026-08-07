
function(download_and_unzip SOURCE_URL TARGET_PATH)

    file(DOWNLOAD ${SOURCE_URL} ${TARGET_PATH} SHOW_PROGRESS STATUS ICU_DL_STATUS ${ICU_CHECK_HASH})

    # check download result
    list(GET ICU_DL_STATUS 0 ICU_DL_STATUS_CODE)
    if (NOT ICU_DL_STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "download from ${SOURCE_URL} failed with code: ${ICU_DL_STATUS_CODE}")
    endif()

    file(ARCHIVE_EXTRACT INPUT "${TARGET_PATH}" DESTINATION "${ICU_DIR_source}" VERBOSE) 
    file(REMOVE "${TARGET_PATH}")
    
endfunction()

function(build_icu4c_win_MSVS  win_arh)

    set(ICU_DIR_result_win "${ICU_DIR_result}/${win_arh}/Release/bin")
    set(ICU_DIR_build_win  "${ICU_DIR_build}/win-${win_arh}")

    if (NOT EXISTS ${ICU_DIR_result_win}/${icu4c_lib_name})

        file(REMOVE_RECURSE ${ICU_DIR_result_win})
             make_directory(${ICU_DIR_result_win})

        file(REMOVE_RECURSE ${ICU_DIR_build_win})
             make_directory(${ICU_DIR_build_win})

        file(COPY "${ICU_DIR_source}/"   DESTINATION "${ICU_DIR_build_win}")
        set(allinone_sln "${ICU_DIR_build_win}/icu/source/allinone/allinone.sln")

        execute_process(
            COMMAND ${CMAKE_VS_MSBUILD_COMMAND} ${allinone_sln} 
                    -p:Configuration=Release /t:Rebuild -p:Platform=${win_arh} -p:OutDir=${ICU_DIR_build_win}/result
            WORKING_DIRECTORY ${ICU_DIR_build_win}
        )
        file(COPY "${ICU_DIR_build_win}/icu/bin64/"   DESTINATION "${ICU_DIR_result_win}" PATTERN "*.dll"  PATTERN "*.dat")
        file(COPY "${ICU_DIR_build_win}/icu/include/" DESTINATION "${ICU_DIR_include}")

        message("== library icu4c for win-${win_arh} rebuld now ${ICU_DIR_result_win}/${icu4c_lib_name} ")
    else()
        message("== library icu4c win-${win_arh} built earlier  ${ICU_DIR_result_win}/${icu4c_lib_name} ")
    endif()

endfunction()


function(build_icu4c_win_LLVM  nix_arh)

    set(ICU_DIR_build_nix  "${ICU_DIR_build}/${nix_arh}")
    set(ICU_DIR_result_nix "${ICU_DIR_result}/${nix_arh}/lib")
    set(ICU_DIR_include    "${ICU_DIR_result}/${nix_arh}/include" )

    set(ICU_DIR_include    "${ICU_DIR_include}"    PARENT_SCOPE)
    set(ICU_DIR_distr      "${ICU_DIR_result_nix}" PARENT_SCOPE)

    # file(REMOVE_RECURSE ${ICU_DIR_result_nix})


    if (NOT EXISTS ${ICU_DIR_result_nix}/${icu4c_lib_name})

        message("== library icu4c prapere to rebuld now. Plese wait... [${ICU_DIR_result_nix}/${icu4c_lib_name}] ")

        set(output_dir "${ICU_DIR_build_nix}/result")

        file(REMOVE_RECURSE ${ICU_DIR_result_nix})
             make_directory(${ICU_DIR_result_nix})

        file(REMOVE_RECURSE ${ICU_DIR_build_nix})
             make_directory(${ICU_DIR_build_nix})

        file(COPY "${ICU_DIR_source}/"   DESTINATION "${ICU_DIR_build_nix}")
    
        fix_win_crlf("${ICU_DIR_build_nix}/icu/source/runConfigureICU")
        fix_win_crlf("${ICU_DIR_build_nix}/icu/source/configure")
        fix_win_crlf("${ICU_DIR_build_nix}/icu/source/config.sub")
        fix_win_crlf("${ICU_DIR_build_nix}/icu/source/mkinstalldirs")

        set(HOST_ENV env CFLAGS=${HOST_CFLAGS} CXXFLAGS=${HOST_CXXFLAGS} LDFLAGS=${HOST_LDFLAGS}  CC=${HOST_CC} CXX=${HOST_CXX} )

        execute_process(
            COMMAND chmod +x "${ICU_DIR_build_nix}/icu/source/configure"
            COMMAND chmod +x "${ICU_DIR_build_nix}/icu/source/runConfigureICU"
            COMMAND chmod +x "${ICU_DIR_build_nix}/icu/source/install-sh"
            COMMAND chmod +x "${ICU_DIR_build_nix}/icu/source/config.sub"
            COMMAND ${HOST_ENV} ${ICU_DIR_build_nix}/icu/source/configure 
            --srcdir=${ICU_DIR_build_nix}/icu/source 
            --host=${ICU_BUILD} 
            --build=${ICU_BUILD} 
            --prefix=${output_dir} 
            --disable-tests 
            --disable-samples
        WORKING_DIRECTORY ${ICU_DIR_build_nix}
        ) # --with-data-packaging=archive # - not acceptable on osx
        execute_process(COMMAND ${HOST_ENV} ${CMAKE_MAKE_PROGRAM}          WORKING_DIRECTORY ${ICU_DIR_build_nix})
        execute_process(COMMAND ${HOST_ENV} ${CMAKE_MAKE_PROGRAM} install  WORKING_DIRECTORY ${ICU_DIR_build_nix})

        if (APPLE)
            execute_process(COMMAND install_name_tool -change libicudata.${icu_MAJOR}.dylib @loader_path/libicudata.${icu_MAJOR}.dylib ${output_dir}/lib/${icu4c_lib_name} )
            set(share_lib_ext "dylib")
        else()    
            set(share_lib_ext "so*")
        endif()

        file(COPY "${output_dir}/lib/"                  DESTINATION "${ICU_DIR_result_nix}" FILES_MATCHING PATTERN "*.${share_lib_ext}" PATTERN "icu" EXCLUDE PATTERN "pkgconfig" EXCLUDE)
        file(COPY "${output_dir}/share/icu/${ICU_VER}/" DESTINATION "${ICU_DIR_result_nix}" FILES_MATCHING PATTERN "*.dat" PATTERN "config" EXCLUDE)
        file(COPY "${output_dir}/include/"              DESTINATION "${ICU_DIR_include}")

        message("== library icu4c rebuld now ${ICU_DIR_result_nix}/${icu4c_lib_name} ")
    else()
        message("== library icu4c built earlier  ${ICU_DIR_result_nix}/${icu4c_lib_name} ")
    endif()

endfunction()


function(build_icu4c ICU_VER)

    string(REGEX MATCH "^([0-9]*)" icu_MAJOR "${ICU_VER}")
    string(REGEX MATCH "[^.]*$"    icu_MINOR "${ICU_VER}")

    set(ICU_DIR_source   "${CMAKE_CURRENT_SOURCE_DIR}/icu/download")
    set(ICU_DIR_build    "${CMAKE_CURRENT_BINARY_DIR}/icu")
    set(ICU_DIR_result   "${CMAKE_CURRENT_SOURCE_DIR}/icu")
    set(ICU_DIR_include  "${ICU_DIR_result}/include" )


    string(REPLACE "." "-" ICU_URL_TAG_VER ${ICU_VER})
    string(REPLACE "." "_" ICU_URL_ARH_VER ${ICU_VER})
    message ("== ICU build ver: ${icu_MAJOR}.${icu_MINOR} dir: ${ICU_DIR_source} ==")

    set(HOST_CC       ${CMAKE_C_COMPILER}) 
    set(HOST_CXX      ${CMAKE_CXX_COMPILER}) 
    set(HOST_CXXFLAGS "") 
    set(HOST_CXXFLAGS "-std=c++20") 
    set(HOST_LDFLAGS  "-static-libstdc++")

    if (APPLE)
        set(ICU_PROC ${CMAKE_SYSTEM_PROCESSOR})  # aarch64
        set(ICU_PROV apple)
        set(ICU_OS   darwin)
        set(icu4c_lib_name "libicuuc.${icu_MAJOR}.${icu_MINOR}.dylib")
    endif()
    if (WIN32)
        set(ICU_PROC x86_64)
        set(ICU_PROV msys)
        set(ICU_OS   windows)
        set(icu4c_lib_name "icuuc${icu_MAJOR}.dll")
        set(HOST_LDFLAGS  "  -lAdvapi32")
    endif()
    if (LINUX)        
        set(ICU_PROC ${CMAKE_SYSTEM_PROCESSOR})
        set(ICU_PROV unknow)
        set(ICU_OS   linux)
        set(icu4c_lib_name "libicuuc.so.${icu_MAJOR}.${icu_MINOR}")
    endif()

    set(ICU_BUILD ${ICU_PROC}-${ICU_PROV}-${ICU_OS})
    set(ICU_VER_file "${ICU_DIR_source}/icu.ver")

    if (EXISTS "${ICU_VER_file}")
        file(READ "${ICU_VER_file}" ICU_EXISTS_VER)
    else()
        set(ICU_EXISTS_VER "NONE")    
    endif()

    message("== icu version need ${ICU_VER}, exists ${ICU_EXISTS_VER}")

    if (NOT "${ICU_EXISTS_VER}" EQUAL "${ICU_VER}")

        file(REMOVE_RECURSE ${ICU_DIR_build})
        file(REMOVE_RECURSE ${ICU_DIR_source})

        make_directory(${ICU_DIR_source})
        file(WRITE "${ICU_VER_file}" ${ICU_VER})

        set(icu_URL "https://github.com/unicode-org/icu/releases/download/release-${ICU_URL_TAG_VER}/icu4c-${ICU_URL_ARH_VER}")
        message ("== ICU download from ${icu_URL}")        

        download_and_unzip("${icu_URL}-src.zip"  "${ICU_DIR_source}/icu4c-${ICU_BUILD_VER}-src.zip")
        download_and_unzip("${icu_URL}-data.zip" "${ICU_DIR_source}/icu4c-${ICU_BUILD_VER}-data.zip")

        fix_win_crlf("${ICU_DIR_source}/icu/source/runConfigureICU")
        fix_win_crlf("${ICU_DIR_source}/icu/source/configure")
        fix_win_crlf("${ICU_DIR_source}/icu/source/config.sub")
        fix_win_crlf("${ICU_DIR_source}/icu/source/mkinstalldirs")

        if (NOT WIN32)
            execute_process(COMMAND cd ${ICU_DIR_source}/icu/source && chmod +x runConfigureICU configure install-sh config.sub )
        endif()
        
    endif()

    if (WIN32)            
        build_icu4c_win_MSVS(x64)
        build_icu4c_win_MSVS(Win32)
        
        if (${arch} EQUAL "arm64")
            build_icu4c_win_MSVS(arm64)
        endif()
    else()
        build_icu4c_win_LLVM("${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(ICU_DIR_include  "${ICU_DIR_include}" PARENT_SCOPE)
    set(ISU_VER_MAJOR    "${icu_MAJOR}"       PARENT_SCOPE)
    set(ISU_VER_MINOR    "${icu_MINOR}"       PARENT_SCOPE)
    set(ICU_DIR_distr    "${ICU_DIR_distr}"   PARENT_SCOPE)
    
endfunction()
