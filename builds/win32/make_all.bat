@echo off
set ERRLEV=0

:: Set env vars
@call setenvvar.bat

@if errorlevel 1 (call :ERROR Executing setenvvar.bat failed & goto :EOF)

:: verify that boot was run before

@if not exist %FB_GEN_DIR%\dbs\msg.fdb (goto :HELP_BOOT & goto :EOF)


@call set_build_target.bat %*

::==========
:: MAIN

@echo Building %FB_OBJ_DIR%

call compile.bat builds\win32\%VS_VER%\Firebird make_all_%FB_TARGET_PLATFORM%.log
if errorlevel 1 call :ERROR build failed - see make_all_%FB_TARGET_PLATFORM%.log for details

@if "%ERRLEV%"=="1" (
  @goto :EOF
) else (
  @call :MOVE
)
@goto :EOF

::===========
:MOVE
@echo Copying files to output
@set FB_OUTPUT_DIR=%FB_ROOT_PATH%\output_%FB_TARGET_PLATFORM%
@del %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\*.exp 2>nul
@del %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\*.lib 2>nul
@rmdir /q /s %FB_OUTPUT_DIR% 2>nul

@mkdir %FB_OUTPUT_DIR% 2>nul
@mkdir %FB_OUTPUT_DIR%\intl 2>nul
@mkdir %FB_OUTPUT_DIR%\help 2>nul
@mkdir %FB_OUTPUT_DIR%\doc 2>nul
@mkdir %FB_OUTPUT_DIR%\doc\sql.extensions 2>nul
@mkdir %FB_OUTPUT_DIR%\include 2>nul
@mkdir %FB_OUTPUT_DIR%\include\firebird 2>nul
@mkdir %FB_OUTPUT_DIR%\lib 2>nul
@mkdir %FB_OUTPUT_DIR%\system32 2>nul
@mkdir %FB_OUTPUT_DIR%\plugins 2>nul
@mkdir %FB_OUTPUT_DIR%\plugins\udr 2>nul

@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\* %FB_OUTPUT_DIR% >nul
@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\intl\* %FB_OUTPUT_DIR%\intl >nul
@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\system32\* %FB_OUTPUT_DIR%\system32 >nul
@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\plugins\*.dll %FB_OUTPUT_DIR%\plugins >nul
@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\plugins\udr\*.dll %FB_OUTPUT_DIR%\plugins\udr >nul
@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\yvalve\fbclient.lib %FB_OUTPUT_DIR%\lib\fbclient_ms.lib >nul
@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\ib_util\ib_util.lib %FB_OUTPUT_DIR%\lib\ib_util_ms.lib >nul

for %%v in (gpre_boot build_msg codes) do (
@del %FB_OUTPUT_DIR%\%%v.* 2>nul
)

:: Firebird.conf, etc
@copy %FB_GEN_DIR%\firebird.msg %FB_OUTPUT_DIR% > nul
:: The line @UDF_COMMENT@ should be deleted from the target file.
findstr /V "@UDF_COMMENT@" %FB_ROOT_PATH%\builds\install\misc\firebird.conf.in > %FB_OUTPUT_DIR%\firebird.conf
@copy %FB_ROOT_PATH%\builds\install\misc\databases.conf.in %FB_OUTPUT_DIR%\databases.conf >nul
@copy %FB_ROOT_PATH%\builds\install\misc\fbintl.conf %FB_OUTPUT_DIR%\intl >nul
@copy %FB_ROOT_PATH%\builds\install\misc\plugins.conf %FB_OUTPUT_DIR% >nul
@copy %FB_ROOT_PATH%\builds\install\misc\replication.conf %FB_OUTPUT_DIR% >nul
@copy %FB_ROOT_PATH%\src\utilities\ntrace\fbtrace.conf %FB_OUTPUT_DIR% >nul
@copy %FB_ROOT_PATH%\src\plugins\udr_engine\udr_engine.conf %FB_OUTPUT_DIR%\plugins\udr_engine.conf >nul
@copy %FB_ROOT_PATH%\builds\install\misc\IPLicense.txt %FB_OUTPUT_DIR% >nul
@copy %FB_ROOT_PATH%\builds\install\misc\IDPLicense.txt %FB_OUTPUT_DIR% >nul

:: DATABASES
@copy %FB_GEN_DIR%\dbs\security4.FDB %FB_OUTPUT_DIR%\security4.fdb >nul
@copy %FB_GEN_DIR%\dbs\HELP.fdb %FB_OUTPUT_DIR%\help\help.fdb >nul

:: DOCS
@copy %FB_ROOT_PATH%\*.md %FB_OUTPUT_DIR%\doc\ >nul

:: READMES
@copy %FB_ROOT_PATH%\doc\README.* %FB_OUTPUT_DIR%\doc >nul
@copy %FB_ROOT_PATH%\doc\sql.extensions\README.* %FB_OUTPUT_DIR%\doc\sql.extensions >nul

:: Headers
copy %FB_ROOT_PATH%\src\extlib\ib_util.h %FB_OUTPUT_DIR%\include > nul
copy %FB_ROOT_PATH%\src\jrd\perf.h %FB_OUTPUT_DIR%\include >nul
copy %FB_ROOT_PATH%\src\include\gen\iberror.h %FB_OUTPUT_DIR%\include > nul

:: New API headers
xcopy %FB_ROOT_PATH%\src\include\firebird %FB_OUTPUT_DIR%\include\firebird /e > nul

:: UDR
copy %FB_ROOT_PATH%\src\extlib\*.sql %FB_OUTPUT_DIR%\plugins\udr > nul

:: Installers
@copy %FB_INSTALL_SCRIPTS%\install_service.bat %FB_OUTPUT_DIR% >nul
@copy %FB_INSTALL_SCRIPTS%\uninstall_service.bat %FB_OUTPUT_DIR% >nul

:: MSVC runtime
if defined VS150COMNTOOLS (
@copy "%VS150COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC141.CRT\vcruntime140.dll" %FB_OUTPUT_DIR% >nul
@copy "%VS150COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC141.CRT\msvcp140.dll" %FB_OUTPUT_DIR% >nul
) else (
if defined VS140COMNTOOLS (
@copy "%VS140COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC140.CRT\vcruntime140.dll" %FB_OUTPUT_DIR% >nul
@copy "%VS140COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC140.CRT\msvcp140.dll" %FB_OUTPUT_DIR% >nul
) else (
if defined VS120COMNTOOLS (
@copy "%VS120COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC120.CRT\msvcr120.dll" %FB_OUTPUT_DIR% >nul
@copy "%VS120COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC120.CRT\msvcp120.dll" %FB_OUTPUT_DIR% >nul
) else (
if defined VS100COMNTOOLS (
@copy "%VS100COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC100.CRT\msvcr100.dll" %FB_OUTPUT_DIR% >nul
@copy "%VS100COMNTOOLS%\..\..\VC\redist\%FB_VC_CRT_DIR%\Microsoft.VC100.CRT\msvcp100.dll" %FB_OUTPUT_DIR% >nul
)
)
)
)

@goto :EOF

::==============
:HELP_BOOT
@echo.
@echo    You must run make_boot.bat before running this script
@echo.
@goto :EOF

:ERROR
::====
@echo.
@echo   An error occurred while running make_all.bat -
@echo     %*
@echo.
set ERRLEV=1
cancel_script > nul 2>&1
::End of ERROR
::------------
@goto :EOF
