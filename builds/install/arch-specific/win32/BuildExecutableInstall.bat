::  Initial Developer's Public License.
::  The contents of this file are subject to the  Initial Developer's Public
::  License Version 1.0 (the "License"). You may not use this file except
::  in compliance with the License. You may obtain a copy of the License at
::    http://www.ibphoenix.com?a=ibphoenix&page=ibp_idpl
::  Software distributed under the License is distributed on an "AS IS" basis,
::  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
::  for the specific language governing rights and limitations under the
::  License.
::
::  The Original Code is copyright 2003-2004 Paul Reeves.
::
::  The Initial Developer of the Original Code is Paul Reeves
::
::  All Rights Reserved.
::
::=============================================================================
@echo off
set SCRIPT_FULL_NAME=%~d0%~p0%~n0%~x0
set SCRIPT_SHORT_NAME=%~n0%~x0

@goto :MAIN
@goto :EOF
::============================================================================

:SET_PARAMS
::=========

@echo off
:: reset ERRLEV to clear error from last run in same cmd shell
set ERRLEV=

:: Assume we are preparing a production build
set FBBUILD_BUILDTYPE=release

:: Don't ship pdb files by default
set FBBUILD_SHIP_PDB=no_pdb
:: Reset "make" vars to zero
set FBBUILD_ZIP_PACK=0
set FBBUILD_ISX_PACK=0

if not defined FB2_SNAPSHOT (set FB2_SNAPSHOT=0)


:: See what we have on the command line

@for %%v in ( %* )  do (
  ( if /I "%%v"=="DEBUG" (set FBBUILD_BUILDTYPE=debug) )
  ( if /I "%%v"=="PDB" (set FBBUILD_SHIP_PDB=ship_pdb) )
  ( if /I "%%v"=="ZIP" (set FBBUILD_ZIP_PACK=1) )
  ( if /I "%%v"=="ISX" (set FBBUILD_ISX_PACK=1) )
  ( if /I "%%v"=="ALL" ( (set FBBUILD_ZIP_PACK=1) & (set FBBUILD_ISX_PACK=1) ) )
)

:: Are we doing a snapshot build? If so we always do less work.
if "%FB2_SNAPSHOT%"=="1" (
  ( set FBBUILD_ISX_PACK=0 )
)


::End of SET_PARAMS
::-----------------
@goto :EOF


:CHECK_ENVIRONMENT
::================
:: Make sure we have everything we need. If something is missing then
:: let's bail out now.

@echo     o Checking for sed...
@(cmd /c "sed.exe --version 2>&1 > nul ") || ( call :ERROR Could not locate sed & goto :EOF )

@echo     o Checking for unix2dos...
@(cmd /c "unix2dos.exe --quiet --version 2>&1 > nul" ) || ( call :ERROR Could not locate unix2dos & goto :EOF )

@for /f "usebackq tokens=*" %%c in ( `where /f md5sum 2^>nul` ) do set MD5_COMMAND=%%c
@if defined MD5_COMMAND (
  echo     o POSIX md5sum utility found at %MD5_COMMAND%
)

@if %FBBUILD_ZIP_PACK% EQU 1 (
  if not defined SEVENZIP (
    if exist "%ProgramW6432%\7-Zip\7z.exe" set SEVENZIP=%ProgramW6432%\7-Zip
  )
  if not defined SEVENZIP (
    call :ERROR SEVENZIP environment variable is not defined.
    goto :EOF
  ) else (
    echo     o Compression utility found.
  )
)

@if %FBBUILD_ISX_PACK% NEQ 1 goto :SKIP_INNO

@if defined INNO6_SETUP_PATH (
  set ISCC_COMMAND="%INNO6_SETUP_PATH%\iscc.exe"
)
:: If the environment variable is not set let's search in PATH
@if not defined ISCC_COMMAND (
  for /f "usebackq tokens=*" %%c in ( `where /f iscc 2^>nul` ) do set ISCC_COMMAND=%%c
)
@if not defined ISCC_COMMAND (
  echo  Required Inno Setup compiler not found
  exit /b 1
)
@echo     o Inno Setup found as %ISCC_COMMAND%.

:SKIP_INNO

@if not defined WIX (
  echo.
  echo The WIX environment var not defined.
  echo WiX is needed to build the MSI kits of the CRT runtimes.
  echo.
) else (
  echo     o WiX found at "%WIX%".
)

@if not defined FB_EXTERNAL_DOCS (
  echo.
  echo The FB_EXTERNAL_DOCS environment var is not defined
  echo It should point to the directory containing the relevant release notes
  echo in adobe pdf format.
  echo.
) else (
  echo     o Package will include documentation from "%FB_EXTERNAL_DOCS%".
)


::End of CHECK_ENVIRONMENT
::------------------------
@goto :EOF


:SET_VERSION
::==========

:: Cut off everything that is not #define to let Inno Setup use it
@findstr /B /L "#define" "%FB_ROOT_PATH%\src\jrd\build_no.h" > "%FB_ROOT_PATH%\gen\jrd\build_no.h"
:: Read version parameters from build_no.h
@for /F "tokens=2*" %%a in ( %FB_ROOT_PATH%\gen\jrd\build_no.h ) do (
  echo   Setting %%a to %%~b
  SET %%a=%%~b
)

:: Set our package number at 0 and increment every
:: time we rebuild in a single session
@if not defined FBBUILD_PACKAGE_NUMBER (
  set FBBUILD_PACKAGE_NUMBER=0
) else (
  set /A FBBUILD_PACKAGE_NUMBER+=1
)
@echo   Setting FBBUILD_PACKAGE_NUMBER to %FBBUILD_PACKAGE_NUMBER%

:: Generate FBBUILD_FILENAME_SUFFIX from FB_BUILD_SUFFIX and add prefix '-'
@for /f "tokens=3-5" %%a in ( "%FB_BUILD_SUFFIX%" ) do (
  if "%%~a" == "Release"  (
    set FBBUILD_FILENAME_SUFFIX=-RC%%~c
  ) else (
    set FBBUILD_FILENAME_SUFFIX=-%%~a%%~b
  )
)

:: Set up our final destination
@set FBBUILD_INSTALL_IMAGES=%FB_ROOT_PATH%\builds\install_images
@if not exist "%FBBUILD_INSTALL_IMAGES%" ( mkdir "%FBBUILD_INSTALL_IMAGES%" )

:: Determine Product Status
@if %FB_BUILD_TYPE%==V (
  set FBBUILD_PROD_STATUS=PROD
) else (
  set FBBUILD_PROD_STATUS=DEV
)

@if "%FB_TARGET_PLATFORM%"=="x64" (
  set FBBUILD_FILE_ID=%PRODUCT_VER_STRING%-%FBBUILD_PACKAGE_NUMBER%%FBBUILD_FILENAME_SUFFIX%-windows-x64
) else (
  set FBBUILD_FILE_ID=%PRODUCT_VER_STRING%-%FBBUILD_PACKAGE_NUMBER%%FBBUILD_FILENAME_SUFFIX%-windows-x86
)

@setlocal
@echo.
@if not exist %FB_GEN_DIR%\readmes ( mkdir %FB_GEN_DIR%\readmes )
@set SED_COMMAND=sed -e s/\$MAJOR/%FB_MAJOR_VER%/g ^
  -e s/\$MINOR/%FB_MINOR_VER%/g ^
  -e s/\$RELEASE/%FB_REV_NO%/g
@echo   Processing version strings in Readme_%FBBUILD_PROD_STATUS%.txt
@%SED_COMMAND% Readme_%FBBUILD_PROD_STATUS%.txt > %FB_GEN_DIR%\readmes\Readme.txt
@for %%f in ( installation_readme.txt installation_scripted.txt ) do (
  echo   Processing version strings in %%f
  %SED_COMMAND% %%f > %FB_GEN_DIR%\readmes\%%f
)
@for %%d in (ba cz de es fr hu it pl pt ru si ja ) do (
  if not exist %FB_GEN_DIR%\readmes\%%d ( mkdir %FB_GEN_DIR%\readmes\%%d )
  for %%f in ( %%d\*.txt  ) do (
    echo   Processing version strings in %%f
    %SED_COMMAND% %%f > %FB_GEN_DIR%\readmes\%%f
  )
)

@endlocal

:: Dump env vars to file for later testing.
@set > %FB_ROOT_PATH%\builds\install\arch-specific\win32\test_installer\fb_build_vars_%PROCESSOR_ARCHITECTURE%.txt

::End of SET_VERSION
::----------------
@goto :EOF


:COPY_XTRA
:: system dll's we need
:: MSVC should be installed with redistributable packages.
::=====================


@echo   Copying MSVC runtime libraries...
@if not exist %FB_OUTPUT_DIR%\system32 (
  mkdir %FB_OUTPUT_DIR%\system32
)

:: Note the confusion of RT library numbers here! These notes, as of time of
:: writing 2021-12-21, are accurate for current versions of Visual Studio
:: - 2017, 2019, 2022. Basic MS runtime version is v140. Except that is only
:: used in file names. In reality Visual Studio uses 141, 142 or 143 in
:: directory names and merge module names as well as producing file names with
:: these version numbers. %MSVC_RUNTIME_FILE_VERSION% should represent 140.
:: %MSVC_RUNTIME_LIBRARY_VERSION% is based on the Visual Studio version used.
:: These variables are set in setenvvar.bat.
:: Note 2: 32-bit vcruntime140_1.dll does not exist, hence the test for file existence
@for %%f in ( msvcp%MSVC_RUNTIME_FILE_VERSION%.dll msvcp%MSVC_RUNTIME_FILE_VERSION%_1.dll vcruntime%MSVC_RUNTIME_FILE_VERSION%.dll vcruntime%MSVC_RUNTIME_FILE_VERSION%_1.dll ) do (
  if exist "%VCToolsRedistDir%\%VSCMD_ARG_TGT_ARCH%\Microsoft.VC%MSVC_RUNTIME_LIBRARY_VERSION%.CRT\%%f" (
    echo   Copying "%VCToolsRedistDir%\%VSCMD_ARG_TGT_ARCH%\Microsoft.VC%MSVC_RUNTIME_LIBRARY_VERSION%.CRT\%%f"
    copy  "%VCToolsRedistDir%\%VSCMD_ARG_TGT_ARCH%\Microsoft.VC%MSVC_RUNTIME_LIBRARY_VERSION%.CRT\%%f" %FB_OUTPUT_DIR%\ >nul
    if ERRORLEVEL 1 (
      call :ERROR Copying "%VCToolsRedistDir%\%VSCMD_ARG_TGT_ARCH%\Microsoft.VC%MSVC_RUNTIME_LIBRARY_VERSION%.CRT\%%f" failed with error %ERRLEV%  & goto :EOF
    )
  )
)


@if "%VSCMD_ARG_TGT_ARCH%"=="x86" (
  echo     Generating fbclient_bor.lib
  where /Q implib.exe
  if not ERRORLEVEL 1 (
    implib %FB_OUTPUT_DIR%\lib\fbclient_bor.lib %FB_OUTPUT_DIR%\fbclient.dll > nul
  ) else (
    call :WARNING implib not found
  )
)


@if "%FBBUILD_SHIP_PDB%"=="ship_pdb" (
  echo   Copying pdb files...
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\chacha\chacha.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\default_profiler\default_profiler.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\engine\engine*.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\fb_lock_print\fb_lock_print.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\fbserver\firebird.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\fbsvcmgr\fbsvcmgr.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\fbtrace\fbtrace.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\fbtracemgr\fbtracemgr.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\gbak\gbak.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\gfix\gfix.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\gstat\gstat.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\ib_util\ib_util.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\intl\fbintl.pdb %FB_OUTPUT_DIR%\intl\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\isql\isql.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\legacy_auth\legacy_auth.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\legacy_usermanager\legacy_usermanager.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\nbackup\nbackup.pdb %FB_OUTPUT_DIR%\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\srp\srp.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\udf_compat\udf_compat.pdb %FB_OUTPUT_DIR%\plugins\udr\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\udrcpp_example\udrcpp_example.pdb %FB_OUTPUT_DIR%\plugins\udr\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\udr_engine\udr_engine.pdb %FB_OUTPUT_DIR%\plugins\ > nul
  copy %FB_TEMP_DIR%\%FBBUILD_BUILDTYPE%\yvalve\fbclient.pdb %FB_OUTPUT_DIR%\ > nul
)

@echo   Started copying docs...
@rmdir /S /Q %FB_OUTPUT_DIR%\doc 2>nul
@mkdir %FB_OUTPUT_DIR%\doc
@copy %FB_ROOT_PATH%\*.md %FB_OUTPUT_DIR%\doc\ > nul
@copy %FB_ROOT_PATH%\doc\*.* %FB_OUTPUT_DIR%\doc\ > nul
@if ERRORLEVEL 1 (
  call :ERROR COPY of main documentation tree failed.
  goto :EOF
)


@echo   Copy various upgrade scripts and docs
@for %%d in ( v3.0 v4.0 ) do (
    mkdir %FB_OUTPUT_DIR%\misc\upgrade\%%d 2>nul
    copy %FB_ROOT_PATH%\src\misc\upgrade\%%d\*.* %FB_OUTPUT_DIR%\misc\upgrade\%%d > nul
    if ERRORLEVEL 1 (
        call :ERROR copy %FB_ROOT_PATH%\src\misc\upgrade\%%d\*.* %FB_OUTPUT_DIR%\misc\upgrade\%%d failed.
        goto :EOF
    )
)

@echo   Copy INTL script
@copy %FB_ROOT_PATH%\src\misc\intl.sql %FB_OUTPUT_DIR%\misc\ > nul
@if ERRORLEVEL 1 (
  call :ERROR copy %FB_ROOT_PATH%\src\misc\intl.sql %FB_OUTPUT_DIR%\misc failed.
  goto :EOF
)

@echo   Copying other documentation...
@copy  %FB_GEN_DIR%\readmes\installation_readme.txt %FB_OUTPUT_DIR%\doc\installation_readme.txt > nul

@mkdir %FB_OUTPUT_DIR%\doc\sql.extensions 2>nul
@if ERRORLEVEL 2 ( ( call :ERROR MKDIR for doc\sql.extensions dir failed) & ( goto :EOF ) )
@copy %FB_ROOT_PATH%\doc\sql.extensions\*.* %FB_OUTPUT_DIR%\doc\sql.extensions\ > nul
@if ERRORLEVEL 1 ( ( call :ERROR Copying doc\sql.extensions failed ) & ( goto :EOF ) )

:: External docs aren't necessary for a snapshot build, so we don't throw
:: an error if FB_EXTERNAL_DOCS is not defined. On the other hand,
:: if the docs are available then we must include them.
@if defined FB_EXTERNAL_DOCS (
    echo   Copying essential pdf docs...
    for %%v in ( Firebird-%FB_MAJOR_VER%.%FB_MINOR_VER%.%FB_REV_NO%%FBBUILD_FILENAME_SUFFIX%-ReleaseNotes.pdf ) do (
        echo     ... %FB_EXTERNAL_DOCS%\%%v to %FB_OUTPUT_DIR%\doc\%%v
        copy /Y %FB_EXTERNAL_DOCS%\%%v %FB_OUTPUT_DIR%\doc\%%v > nul
        if ERRORLEVEL 1 (
          call :ERROR Copying %FB_EXTERNAL_DOCS%\%%v to %FB_OUTPUT_DIR%\doc\%%v FAILED. & goto :EOF
        )
    )

    echo   Copying optional pdf docs...
    for %%v in ( firebird-%FB_MAJOR_VER%-quickstartguide.pdf  ) do (
        echo     ... %%v
        copy /Y %FB_EXTERNAL_DOCS%\%%v %FB_OUTPUT_DIR%\doc\%%v > nul
        if ERRORLEVEL 1 (
            call :WARNING Copying %FB_EXTERNAL_DOCS%\%%v to %FB_OUTPUT_DIR%\doc\%%v FAILED.
        )
    )

  echo   Finished copying pdf docs...
  echo.
)

@echo   Cleaning irrelevant files...
:: Clean out text notes that are either not relevant to Windows or
:: are only of use to engine developers.
@for %%v in (  README.makefiles README.user.embedded README.user.troubleshooting README.build.mingw.html README.build.msvc.html fb2-todo.txt cleaning-todo.txt install_win32.txt README.coding.style emacros-cross_ref.html firebird_conf.txt *.*~) do (
  @del %FB_OUTPUT_DIR%\doc\%%v 2>nul
)

@echo   Copy license...
:: Add license
for %%v in (IPLicense.txt IDPLicense.txt ) do (
    @copy %FB_ROOT_PATH%\builds\install\misc\%%v %FB_OUTPUT_DIR%\%%v > nul
)

:: And readme
@copy %FB_GEN_DIR%\readmes\Readme.txt %FB_OUTPUT_DIR%\ > nul

::  Walk through all docs and transform any that are not .txt, .pdf or .html to .txt
@echo   Setting .txt filetype to ascii docs.
@for /R %FB_OUTPUT_DIR%\doc %%v in ( * ) do (
  if /I not "%%~xv" == ".md" (
    if /I not "%%~xv" == ".txt" (
      if /I not "%%~xv" == ".pdf" (
        if /I not "%%~xv" == ".htm" (
          if /I not "%%~xv" == ".html" (
            ren %%v %%~nxv.txt
          )
        )
      )
    )
  )
)

@if %FB2_SNAPSHOT% EQU 1 (
  copy %FB_ROOT_PATH%\builds\install\arch-specific\win32\readme_snapshot.txt %FB_OUTPUT_DIR%\readme_snapshot.txt > nul
)

@echo   Completed copying docs.

:: Examples were already copied by make_examples

::End of COPY_XTRA
::----------------
@goto :EOF


:BUILD_CRT_MSI
:: Generate runtimes as an MSI file.
:: This requires WiX 3.0 to be installed
::============
@if %MSVC_VERSION% EQU 15 (
  if not exist %FB_OUTPUT_DIR%\system32\vccrt%MSVC_RUNTIME_LIBRARY_VERSION%_%FB_TARGET_PLATFORM%.msi (
    "%WIX%\bin\candle.exe" -v -sw1091 %FB_ROOT_PATH%\builds\win32\msvc%MSVC_VERSION%\VCCRT_%FB_TARGET_PLATFORM%.wxs -out %FB_GEN_DIR%\vccrt_%FB_TARGET_PLATFORM%.wixobj
    if ERRORLEVEL 1 (
      call :ERROR Could not generate wixobj for MSVC Runtime MSI %MSVC_RUNTIME_LIBRARY_VERSION%  &  goto :EOF
    ) else (
      "%WIX%\bin\light.exe" -sw1076 %FB_GEN_DIR%\vccrt_%FB_TARGET_PLATFORM%.wixobj -out %FB_OUTPUT_DIR%\system32\vccrt%MSVC_RUNTIME_LIBRARY_VERSION%_%FB_TARGET_PLATFORM%.msi
      if ERRORLEVEL 1 (
        call :ERROR Could not generate MSVCC Runtime MSI %MSVC_RUNTIME_LIBRARY_VERSION% & goto :EOF
      )
    )
  ) else (
    echo   Using an existing build of %FB_OUTPUT_DIR%\system32\vccrt%MSVC_RUNTIME_LIBRARY_VERSION%_%FB_TARGET_PLATFORM%.msi
  )
)

::End of BUILD_CRT_MSI
::--------------------
@goto :EOF


:INCLUDE_DIR
::==========
:: Prepare other files needed for deployment to /include dir
@setlocal
:: grab some missing bits'n'pieces from different parts of the source tree
::=========================================================================
@echo   Copying ib_util etc
@copy %FB_ROOT_PATH%\src\misc\pascal\ib_util.pas %FB_OUTPUT_DIR%\include > nul || (call :ERROR Copying ib_util.pas failed. & goto :EOF )

@echo   Copying other include files required for development...
@set OUTPATH=%FB_OUTPUT_DIR%\include
@copy %FB_ROOT_PATH%\src\include\gen\firebird.pas %OUTPATH%\firebird\ > nul || (@call :ERROR Failure executing copy %FB_ROOT_PATH%\src\include\gen\firebird.pas %OUTPATH%\firebird\  )
@if ERRORLEVEL 1 goto :END

@endlocal

::End of INCLUDE_DIR
::------------------
@goto :EOF


:DB_CONF
:: Generate sample databases file
::===============================
@echo   Creating sample databases.conf
@copy %FB_ROOT_PATH%\builds\install\misc\databases.conf %FB_OUTPUT_DIR%\databases.conf > nul

::End of DB_CONF
::-----------------
@goto :EOF


:FB_MSG
::=================================================================
:: firebird.msg is generated as part of the build process
:: in builds\win32 by build_msg.bat. Copying from there to output dir
::=================================================================
@if not exist %FB_OUTPUT_DIR%\firebird.msg (
    copy %FB_GEN_DIR%\firebird.msg %FB_OUTPUT_DIR%\firebird.msg > nul
    if ERRORLEVEL 1 ( call :ERROR Could not copy firebird.msg  & goto :EOF )
)

::End of FB_MSG
::-------------
@goto :EOF


:SET_CRLF
:: Get a list of all files in the tree make sure
:: that and they all have windows EOL
::===============================================
@for /R %FB_OUTPUT_DIR% %%W in ( *.txt *.conf *.sql *.c *.cpp *.hpp *.h *.bat *.pas *.e *.def *.rc *.md *.html ) do (
  unix2dos --safe %%W > nul 2>&1 || exit /b 1
)

::End of SET_CRLF
::-------------
@goto :EOF


:ZIP_PACK
::=======
:: Forcefully disable delayed expansion because of exclamation marks in 7z switches
@setlocal DisableDelayedExpansion

@set SKIP_FILES=-x!*.log -x!*.exp -x!*_test.exe -x!installation_readme.txt

@if "%FBBUILD_SHIP_PDB%" == "ship_pdb" (
    set FBBUILD_ZIPFILE=%FBBUILD_INSTALL_IMAGES%\Firebird-%FBBUILD_FILE_ID%-withDebugSymbols.zip
) else (
    set FBBUILD_ZIPFILE=%FBBUILD_INSTALL_IMAGES%\Firebird-%FBBUILD_FILE_ID%.zip
    set SKIP_FILES=%SKIP_FILES% -x!*.pdb
)

:: No need to ship this file with the msi runtime
@set SKIP_FILES=%SKIP_FILES% -x!*.wixpdb

@if "%FB2_EXAMPLES%" == "0" set SKIP_FILES=%SKIP_FILES% -xr-!examples

@if exist %FBBUILD_ZIPFILE% del %FBBUILD_ZIPFILE%

@"%SEVENZIP%\7z.exe" a -r -tzip -mx9 %SKIP_FILES% %FBBUILD_ZIPFILE% %FB_OUTPUT_DIR%\*

@endlocal

::End of ZIP_PACK
::----------------
@goto :EOF


:ISX_PACK
::=======
:: Now let's go and build the installable .exe
::
:: Note - define INNO6_SETUP_PATH with double quotes if it is installed into a path string using spaces.
:: eg set INNO6_SETUP_PATH="C:\Program Files\Inno Setup 6"
::
::=================================================
@echo.
@call %ISCC_COMMAND% %FB_ROOT_PATH%\builds\install\arch-specific\win32\FirebirdInstall.iss
@echo.

::End of ISX_PACK
::---------------
@goto :EOF


:DO_MD5SUMS
::=========
:: Generate the md5sum checksum file
::==================================
@if not defined MD5_COMMAND (
  call :WARNING md5sum utility not found. Cannot generate md5 sums.
  goto :EOF
)
@set FBBUILD_MD5SUMS_FILENAME=Firebird-%PRODUCT_VER_STRING%-%FBBUILD_PACKAGE_NUMBER%%FBBUILD_FILENAME_SUFFIX%-windows.md5sum
@echo   Generating md5sums for %FBBUILD_MD5SUMS_FILENAME%

:: write sums into temporary file to avoid including it into the process
@pushd %FBBUILD_INSTALL_IMAGES%
@call %MD5_COMMAND% Firebird-%PRODUCT_VER_STRING%-%FBBUILD_PACKAGE_NUMBER%*.* > md5sum.tmp

:: then rename it to the proper name
@if not ERRORLEVEL 1 (
  del %FBBUILD_MD5SUMS_FILENAME% >nul 2>nul
  ren md5sum.tmp %FBBUILD_MD5SUMS_FILENAME%
) else (
     echo Error calling %0 & popd & @goto :END
)
@popd

::End of DO_MD5SUMS
::-----------------
@goto :EOF


:HELP
::===
@echo.
@echo.
@echo   Parameters can be passed in any order.
@echo   Currently the recognised params are:
@echo.
@echo       DEBUG  Use binaries from 'debug' dir, not 'release' dir.
@echo              (Requires a debug build. NOTE: A debug build is
@echo               not required to create packages with debug info.)
@echo.
@echo       PDB    Include pdb files.
@echo              (These files roughly double the size of the package.)
@echo.
@echo       ISX    Create installable binary from InnoSetup Extensions compiler.
@echo              (You need to set the INNO6_SETUP_PATH environment variable.)
@echo.
@echo       ZIP    Create Zip package.
@echo              (SEVENZIP is currently used and the SEVENZIP env var must be set.)
@echo.
@echo       ALL    Build InnoSetup and Zip packages.
@echo.
@echo       HELP   This help screen.
@echo.
@echo   In addition, the following environment variables are checked by ISS script:
@echo.
@echo     FB2_ISS_DEBUG=1 - Prepare an InnoSetup script that is
@echo                       easier to debug
@echo.
@echo     FB2_EXAMPLES=0  - Don't include examples in the install kit.
@echo.
@echo.
@echo   Required Files
@echo.
@echo     To successfully package Firebird you will need to make sure several
@echo     packages are installed and correctly configured on your system.
@echo.
@echo     o InnoSetup is needed to create the binary installer. See the header
@echo       of the .iss file to see which minimum version is required.
@echo.
@echo     o 7ZIP is required to create the zip package
@echo.
@echo     o sed is required for packaging. Use the sed provided by
@echo       gnuwin32. The cygwin one is not guaranteed to work.
@echo.
@echo     o WiX v3.0 is required to build installable msi packages of the
@echo       MS runtime libraries.
@echo.

::End of HELP
::-----------
@goto :EOF


:ERROR
::====
@set ERRLEV=%ERRORLEVEL%
@echo.
@echo   Error code %ERRLEV% in %SCRIPT_SHORT_NAME%
@echo     %*
@echo.
@if "%FBBUILD_PROD_STATUS%" == "PROD" (
    echo.
    echo   Production status is Final or Release Candidate
    echo   Error %ERRLEV% must be fixed before continuing
    echo.
) else (
    set ERRLEV=
    ver > nul
)
::End of ERROR
::------------
@goto :END


:WARNING
::======
@set ERRLEV=%ERRORLEVEL%
@echo.
@echo   **** WARNING - Execution of a non-critical component failed with error level %ERRLEV%. ****
@echo   %*
@echo.
@if "%FBBUILD_PROD_STATUS%" == "PROD" (
    echo.
    echo   Production status is Final or Release Candidate
    echo   Error %ERRLEV% must be fixed before continuing
    echo.
) else (
    set ERRLEV=
    ver > nul
)
::End of WARNING
::--------------
@goto :EOF


:MAIN
::====
::Check if on-line help is required
@for %%v in ( %1 %2 %3 %4 %5 %6 %7 %8 %9 )  do (
  if /I "%%v"=="-h" ( goto :HELP & goto :EOF )
  if /I "%%v"=="/h" ( goto :HELP & goto :EOF )
  if /I "%%v"=="HELP" ( goto :HELP & goto :EOF )
)

pushd ..\..\..\win32
::This must be called from the directory it resides in.
@call setenvvar.bat %*
@if ERRORLEVEL 1 ( popd & ( call :ERROR Failure after calling setenvvar.bat ) & goto :END )
popd

@if not defined FB2_ISS_DEBUG (set FB2_ISS_DEBUG=0)

@echo.
@echo   Reading command-line parameters...
@(call :SET_PARAMS %* )
@if ERRORLEVEL 1 (call :ERROR Calling SET_PARAMS & goto :END)

@echo.
@echo   Checking that all required components are available...
@( call :CHECK_ENVIRONMENT ) || ( echo Error calling CHECK_ENVIRONMENT & goto :END )
@echo.

@echo   Setting version number...
@( call :SET_VERSION ) || ( echo Error calling SET_VERSION & goto :END )
@echo.

@echo   Copying additional files needed for installation, documentation etc.
@( call :COPY_XTRA ) || ( echo Error calling COPY_XTRA & goto :END )
@echo.

:: WIX is not necessary for a snapshot build, so we don't throw
:: an error if WIX is not defined. On the other hand,
:: if it is there anyway, use it.
@if defined WIX (
  echo   Building MSI runtimes
  ( call :BUILD_CRT_MSI ) || ( echo Error calling BUILD_CRT_MSI & goto :END )
  echo.
)

@echo   Prepare include directory
@( call :INCLUDE_DIR ) || ( echo Error calling INCLUDE_DIR & goto :END )
@echo.

@echo   Writing databases conf
@( call :DB_CONF ) || ( echo Error calling DB_CONF & goto :END )
@echo.
@echo   Copying firebird.msg
@( call :FB_MSG ) || ( echo Error calling FB_MSG & goto :END )
@echo.

@echo   Fix up line endings...
@( call :SET_CRLF ) || ( echo Error calling SET_CRLF & goto :EOF )
@echo.

@if %FBBUILD_ZIP_PACK% EQU 1 (
  echo   Zipping files for zip pack
  ( call :ZIP_PACK ) || ( echo Error calling ZIP_PACK & goto :END )
  echo.
)

@if %FBBUILD_ISX_PACK% EQU 1 (
  echo   Now let's compile the InnoSetup scripts
  ( call :ISX_PACK ) || ( echo Error calling ISX_PACK & goto :END )
  echo.
)

@( call :DO_MD5SUMS ) || ( echo Error calling DO_MD5SUMS & goto :END)

@echo.
@echo Completed building installation kit(s)
@echo.

:: If we got this far then be sure to reset ERRLEV
:: because run_all.bat will check for ERRLEV
@set ERRLEV=

::End of MAIN
::-----------
@goto :END


:END

exit /b %ERRLEV%


