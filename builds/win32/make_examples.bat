@echo off

:: Set env vars
@call setenvvar.bat
@if errorlevel 1 (goto :EOF)

@call set_build_target.bat %*

:: verify that boot was run before
@if not exist %FB_BIN_DIR%\isql.exe (goto :HELP_BOOT & goto :EOF)
@if not exist %FB_BIN_DIR%\gpre.exe (goto :HELP_BOOT & goto :EOF)
@if not exist %FB_BIN_DIR%\fbclient.dll (goto :HELP_BOOT & goto :EOF)

::Uncomment this to build intlemp
::set FB2_INTLEMP=1

::===========
:MAIN
@echo BUILD_EMPBUILD
@call :BUILD_EMPBUILD

@echo.
@echo Building %FB_OBJ_DIR%
@call compile.bat builds\win32\%VS_VER%\FirebirdExamples empbuild_%FB_TARGET_PLATFORM%.log empbuild
@if defined FB2_INTLEMP (
  @call compile.bat builds\win32\%VS_VER%\FirebirdExamples intlbuild_%FB_TARGET_PLATFORM%.log intlbuild
)
@call compile.bat builds\win32\%VS_VER%\FirebirdExamples udrcpp_example_%FB_TARGET_PLATFORM%.log udrcpp_example
if errorlevel 1 (
    @call :ERROR building udrcpp example failed - see udrcpp_example_%FB_TARGET_PLATFORM%.log for details
    @goto :EOF
)
@call compile.bat examples\extauth\msvc\ExtAuth_%VS_VER% ExtAuth_%FB_TARGET_PLATFORM%.log
if errorlevel 1 (
    @call :ERROR building ExtAuth examples failed - see ExtAuth_%FB_TARGET_PLATFORM%.log for details
    @goto :EOF
)
@call compile.bat examples\dbcrypt\msvc\DbCrypt_%VS_VER% DbCrypt_%FB_TARGET_PLATFORM%.log
if errorlevel 1 (
    @call :ERROR building DbCrypt examples failed - see DbCrypt_%FB_TARGET_PLATFORM%.log for details
    @goto :EOF
)

@echo.
@call :MOVE
@call :BUILD_EMPLOYEE
@call :MOVE2
@goto :EOF


:BUILD_EMPBUILD
::===========
@echo.
@echo Building empbuild.fdb
@copy /y %FB_ROOT_PATH%\examples\empbuild\*.sql   %FB_GEN_DIR%\examples\ > nul
@copy /y %FB_ROOT_PATH%\examples\empbuild\*.inp   %FB_GEN_DIR%\examples\ > nul

@echo.
:: Here we must use cd because isql does not have an option to set a base directory
@pushd "%FB_LONG_ROOT_PATH%\gen\examples"
@echo   Creating empbuild.fdb...
@echo.
@del empbuild.fdb 2> nul
@%FB_BIN_DIR%\isql -i empbld.sql


if defined FB2_INTLEMP (
@echo   Creating intlbuild.fdb...
@echo.
@copy %FB_ROOT_PATH%\builds\install\misc\fbintl.conf %FB_BIN_DIR%\intl >nul
@del intlbuild.fdb 2> nul
@%FB_BIN_DIR%\isql -i intlbld.sql
)

@popd

@echo.
@echo path = %FB_GEN_DB_DIR%\examples
@echo   Preprocessing empbuild.e...
@echo.
@%FB_BIN_DIR%\gpre.exe -r -m -n -z %FB_ROOT_PATH%\examples\empbuild\empbuild.e %FB_GEN_DIR%\examples\empbuild.c -b %FB_GEN_DB_DIR%/examples/

if defined FB2_INTLEMP (
@echo   Preprocessing intlbld.e...
@echo.
@%FB_BIN_DIR%\gpre.exe -r -m -n -z %FB_ROOT_PATH%\examples\empbuild\intlbld.e %FB_GEN_DIR%\examples\intlbld.c -b %FB_GEN_DB_DIR%/examples/
)

::End of BUILD_EMPBUILD
::---------------------
@goto :EOF


:MOVE
::===========
@echo.
@rmdir /q /s %FB_OUTPUT_DIR%\examples 2>nul
@mkdir %FB_OUTPUT_DIR%\examples
@mkdir %FB_OUTPUT_DIR%\examples\api
@mkdir %FB_OUTPUT_DIR%\examples\dbcrypt
@mkdir %FB_OUTPUT_DIR%\examples\dbcrypt\msvc
@mkdir %FB_OUTPUT_DIR%\examples\build_win32
@mkdir %FB_OUTPUT_DIR%\examples\empbuild
@mkdir %FB_OUTPUT_DIR%\examples\extauth
@mkdir %FB_OUTPUT_DIR%\examples\extauth\msvc
@mkdir %FB_OUTPUT_DIR%\examples\include
@mkdir %FB_OUTPUT_DIR%\examples\interfaces
@mkdir %FB_OUTPUT_DIR%\examples\package
@mkdir %FB_OUTPUT_DIR%\examples\stat
@mkdir %FB_OUTPUT_DIR%\examples\udf
@mkdir %FB_OUTPUT_DIR%\examples\udr
@mkdir %FB_OUTPUT_DIR%\plugins\udr 2>nul
@mkdir %FB_OUTPUT_DIR%\examples\prebuilt\bin
@mkdir %FB_OUTPUT_DIR%\examples\prebuilt\plugins

@echo Moving files to output directory
copy %FB_ROOT_PATH%\examples\* %FB_OUTPUT_DIR%\examples > nul
ren %FB_OUTPUT_DIR%\examples\readme readme.txt > nul
copy %FB_ROOT_PATH%\examples\api\* %FB_OUTPUT_DIR%\examples\api > nul
copy %FB_ROOT_PATH%\examples\dbcrypt\* %FB_OUTPUT_DIR%\examples\dbcrypt > nul
copy %FB_ROOT_PATH%\examples\dbcrypt\msvc\* %FB_OUTPUT_DIR%\examples\dbcrypt\msvc > nul
copy %FB_ROOT_PATH%\examples\dbcrypt\*.conf %FB_OUTPUT_DIR%\examples\prebuilt\plugins > nul
copy %FB_ROOT_PATH%\examples\build_win32\* %FB_OUTPUT_DIR%\examples\build_win32 > nul
:: @copy %FB_ROOT_PATH%\examples\empbuild\* %FB_OUTPUT_DIR%\examples\empbuild > nul
@copy %FB_ROOT_PATH%\examples\extauth\* %FB_OUTPUT_DIR%\examples\extauth > nul
@copy %FB_ROOT_PATH%\examples\extauth\msvc\* %FB_OUTPUT_DIR%\examples\extauth\msvc > nul
copy %FB_ROOT_PATH%\examples\empbuild\employe2.sql %FB_OUTPUT_DIR%\examples\empbuild > nul
copy %FB_ROOT_PATH%\examples\include\* %FB_OUTPUT_DIR%\examples\include > nul
copy %FB_ROOT_PATH%\examples\interfaces\* %FB_OUTPUT_DIR%\examples\interfaces > nul
copy %FB_ROOT_PATH%\examples\package\* %FB_OUTPUT_DIR%\examples\package > nul
copy %FB_ROOT_PATH%\examples\stat\* %FB_OUTPUT_DIR%\examples\stat > nul
copy %FB_ROOT_PATH%\examples\udf\* %FB_OUTPUT_DIR%\examples\udf > nul
copy %FB_ROOT_PATH%\examples\udr\* %FB_OUTPUT_DIR%\examples\udr > nul
copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\plugins\udr\*.dll %FB_OUTPUT_DIR%\plugins\udr >nul
copy %FB_ROOT_PATH%\examples\prebuilt\%FB_OBJ_DIR%\bin\*.exe %FB_OUTPUT_DIR%\examples\prebuilt\bin > nul
copy %FB_ROOT_PATH%\examples\prebuilt\%FB_OBJ_DIR%\plugins\*.dll %FB_OUTPUT_DIR%\examples\prebuilt\plugins > nul

::@copy %FB_GEN_DIR%\examples\empbuild.c %FB_OUTPUT_DIR%\examples\empbuild\ > nul
::@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\examples\empbuild.exe %FB_GEN_DIR%\examples\empbuild.exe > nul
::if defined FB2_INTLEMP (
::if "%VS_VER%"=="msvc6" (
::@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\examples\intlbld.exe %FB_GEN_DIR%\examples\intlbuild.exe > nul
::) else (
::@copy %FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\examples\intlbuild.exe %FB_GEN_DIR%\examples\intlbuild.exe > nul
::)
::)

::End of MOVE
::-----------
@goto :EOF

:BUILD_EMPLOYEE
::===========
:: only to test if it works

@echo.
@echo Building employee.fdb

:: Do no mess with global variables
setlocal

:: This allows us to use the new engine in embedded mode to build
:: the employee database.
@set FIREBIRD=%FB_BIN_DIR%
@set PATH=%FB_BIN_DIR%;%PATH%

:: Here we must use cd because isql does not have an option to set a base directory
:: and empbuild.exe uses isql
:: BEWARE: It will run without error if you have FB client from previous version
::         installed in System32 and server run but created database will have
::         wrong ODS.
@pushd "%FB_GEN_DIR%\examples"
if exist employee.fdb del employee.fdb

%FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\empbuild\empbuild.exe %FB_GEN_DB_DIR%/examples/employee.fdb
if errorlevel 44 (call :ERROR empbuild.exe failed - see empbuild_%FB_TARGET_PLATFORM%.log for details )

@if defined FB2_INTLEMP (
@echo Building intlemp.fdb
  @del %FB_GEN_DIR%\examples\intlemp.fdb 2>nul
  @del isql.tmp 2>nul
  @echo s;intlemp.fdb;%FB_GEN_DIR%\examples\intlemp.fdb;g > isql.tmp
  @%FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\intlbuild\intlbuild.exe %FB_GEN_DB_DIR%/examples/intlemp.fdb
)

@popd
endlocal

::End of BUILD_EMPLOYEE
::---------------------
@goto :EOF

::==============
:MOVE2
@copy %FB_GEN_DIR%\examples\employee.fdb %FB_OUTPUT_DIR%\examples\empbuild\ > nul
if errorlevel 1 (
  @call :ERROR copying employee database to %FB_OUTPUT_DIR%\examples\empbuild failed - see make_examples_%FB_TARGET_PLATFORM%.log for details
  @goto :EOF
)

if defined FB2_INTLEMP (
  if exist %FB_GEN_DIR%\examples\intlemp.fdb (
  @copy %FB_GEN_DIR%\examples\intlemp.fdb %FB_OUTPUT_DIR%\examples\empbuild\ > nul
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
@echo   Error  - %*
@echo.
set ERRLEV=1

exit /b 1

::End of ERROR
::------------
