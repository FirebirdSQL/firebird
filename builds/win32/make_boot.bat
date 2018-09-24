::
:: This bat file doesn't use cd, all the paths are full paths.
:: with this convention this bat file is position independent
:: and it will be easier to move the place of somefiles.
::

@echo off
set ERRLEV=0

:CHECK_ENV
@call setenvvar.bat
@if errorlevel 1 (goto :END)

@call set_build_target.bat %*


::===========
:MAIN
@echo.
@echo Creating directories
@rmdir /s /q %FB_GEN_DIR% 2>nul
:: Remove previously generated output, and recreate the directory hierarchy. 
for %%v in ( alice auth burp dsql gpre isql jrd misc msgs qli examples yvalve) do (
  @mkdir %FB_GEN_DIR%\%%v 
)

@rmdir /s /q %FB_GEN_DIR%\utilities 2>nul

@mkdir %FB_GEN_DIR%\utilities 2>nul
@mkdir %FB_GEN_DIR%\utilities\gstat 2>nul
@mkdir %FB_GEN_DIR%\auth\SecurityDatabase 2>nul
@mkdir %FB_GEN_DIR%\gpre\std 2>nul

::=======
call :btyacc
if "%ERRLEV%"=="1" goto :END

call :LibTom
if "%ERRLEV%"=="1" goto :END

call :decNumber
if "%ERRLEV%"=="1" goto :END

call :zlib
if "%ERRLEV%"=="1" goto :END

@echo Generating DSQL parser...
@call parse.bat %*
if "%ERRLEV%"=="1" goto :END

::=======
call :gpre_boot
if "%ERRLEV%"=="1" goto :END

::=======
@echo Preprocessing the source files needed to build gbak, gpre and isql...
@call preprocess.bat BOOT

::=======
call :engine
if "%ERRLEV%"=="1" goto :END

call :gbak
if "%ERRLEV%"=="1" goto :END

call :gpre
if "%ERRLEV%"=="1" goto :END

call :isql
if "%ERRLEV%"=="1" goto :END

@findstr /V "@UDF_COMMENT@" %FB_ROOT_PATH%\builds\install\misc\firebird.conf.in > %FB_BIN_DIR%\firebird.conf

:: Copy ICU and zlib both to Debug and Release configurations

@call set_build_target.bat %* RELEASE
@mkdir %FB_BIN_DIR%
@copy %FB_ROOT_PATH%\extern\icu\icudt???.dat %FB_BIN_DIR% >nul 2>&1
@copy %FB_ICU_SOURCE_BIN%\*.dll %FB_BIN_DIR% >nul 2>&1
@copy %FB_ROOT_PATH%\extern\zlib\%FB_TARGET_PLATFORM%\*.dll %FB_BIN_DIR% >nul 2>&1

@call set_build_target.bat %* DEBUG
@mkdir %FB_BIN_DIR%
@copy %FB_ROOT_PATH%\extern\icu\icudt???.dat %FB_BIN_DIR% >nul 2>&1
@copy %FB_ICU_SOURCE_BIN%\*.dll %FB_BIN_DIR% >nul 2>&1
@copy %FB_ROOT_PATH%\extern\zlib\%FB_TARGET_PLATFORM%\*.dll %FB_BIN_DIR% >nul 2>&1

@call set_build_target.bat %*


::=======
@call :databases

::=======
@echo Preprocessing the entire source tree...
@call preprocess.bat

::=======
@call :msgs
if "%ERRLEV%"=="1" goto :END

@call :codes
if "%ERRLEV%"=="1" goto :END

::=======
@call create_msgs.bat msg
::=======

@call :NEXT_STEP
@goto :END


::===================
:: BUILD btyacc
:btyacc
@echo.
@echo Building btyacc (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\FirebirdBoot btyacc_%FB_TARGET_PLATFORM%.log btyacc
if errorlevel 1 call :boot2 btyacc
goto :EOF

::===================
:: BUILD LibTom
:: NS: Note we need both debug and non-debug version as it is a static library linked to CRT
:: and linking executable with both debug and non-debug CRT results in undefined behavior
:LibTom
@echo.
@call set_build_target.bat %* RELEASE
@echo Building LibTomMath (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\extern\libtommath\libtommath_MSVC%MSVC_VERSION% libtommath_%FB_OBJ_DIR%_%FB_TARGET_PLATFORM%.log libtommath
if errorlevel 1 call :boot2 libtommath_%FB_OBJ_DIR%
@echo Building LibTomCrypt (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\extern\libtomcrypt\libtomcrypt_MSVC%MSVC_VERSION% libtomcrypt_%FB_OBJ_DIR%_%FB_TARGET_PLATFORM%.log libtomcrypt
if errorlevel 1 call :boot2 libtomcrypt_%FB_OBJ_DIR%

@call set_build_target.bat %* DEBUG
@echo Building LibTomMath (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\extern\libtommath\libtommath_MSVC%MSVC_VERSION% libtommath_%FB_OBJ_DIR%_%FB_TARGET_PLATFORM%.log libtommath
if errorlevel 1 call :boot2 libtommath_%FB_OBJ_DIR%
@echo Building LibTomCrypt (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\extern\libtomcrypt\libtomcrypt_MSVC%MSVC_VERSION% libtomcrypt_%FB_OBJ_DIR%_%FB_TARGET_PLATFORM%.log libtomcrypt
if errorlevel 1 call :boot2 libtomcrypt_%FB_OBJ_DIR%

@call set_build_target.bat %*
goto :EOF

::===================
:: BUILD decNumber
:decNumber
@echo.
@call set_build_target.bat %* RELEASE
@echo Building decNumber (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\extern\decNumber\msvc\decNumber_MSVC%MSVC_VERSION% decNumber_%FB_OBJ_DIR%_%FB_TARGET_PLATFORM%.log decNumber
if errorlevel 1 call :boot2 decNumber_%FB_OBJ_DIR%
@call set_build_target.bat %* DEBUG
@echo Building decNumber (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\extern\decNumber\msvc\decNumber_MSVC%MSVC_VERSION% decNumber_%FB_OBJ_DIR%_%FB_TARGET_PLATFORM%.log decNumber
if errorlevel 1 call :boot2 decNumber_%FB_OBJ_DIR%
@call set_build_target.bat %*
goto :EOF

::===================
:: Extract zlib
:zlib
@echo Extracting pre-built zlib
if exist %FB_ROOT_PATH%\extern\zlib\zlib.h (
  @echo %FB_ROOT_PATH%\extern\zlib\zlib.h already extracted
) else (
  %FB_ROOT_PATH%\extern\zlib\zlib.exe -y > zlib_%FB_TARGET_PLATFORM%.log
  if errorlevel 1 call :boot2 zlib
)
goto :EOF

::===================
:: BUILD gpre_boot
:gpre_boot
@echo.
@echo Building gpre_boot (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\FirebirdBoot gpre_boot_%FB_TARGET_PLATFORM%.log gpre_boot
if errorlevel 1 call :boot2 gpre_boot
goto :EOF

::===================
:: BUILD engine
:engine
@echo.
@echo Building engine (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\Firebird engine_%FB_TARGET_PLATFORM%.log engine
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\Firebird engine_%FB_TARGET_PLATFORM%.log ib_util
if errorlevel 1 call :boot2 engine
@goto :EOF

::===================
:: BUILD gbak
:gbak
@echo.
@echo Building gbak (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\Firebird gbak_%FB_TARGET_PLATFORM%.log gbak
if errorlevel 1 call :boot2 gbak
@goto :EOF

::===================
:: BUILD gpre
:gpre
@echo.
@echo Building gpre (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\Firebird gpre_%FB_TARGET_PLATFORM%.log gpre
if errorlevel 1 call :boot2 gpre
@goto :EOF

::===================
:: BUILD isql
:isql
@echo.
@echo Building isql (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\Firebird isql_%FB_TARGET_PLATFORM%.log isql
if errorlevel 1 call :boot2 isql
@goto :EOF

::===================
:: ERROR boot
:boot2
echo.
echo Error building %1, see %1_%FB_TARGET_PLATFORM%.log
echo.
set ERRLEV=1
goto :EOF


::===================
:: BUILD messages
:msgs
@echo.
@echo Building build_msg (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\FirebirdBoot build_msg_%FB_TARGET_PLATFORM%.log build_msg
if errorlevel 1 goto :msgs2
@goto :EOF
:msgs2
echo.
echo Error building build_msg, see build_msg_%FB_TARGET_PLATFORM%.log
echo.
set ERRLEV=1
goto :EOF

::===================
:: BUILD codes
:codes
@echo.
@echo Building codes (%FB_OBJ_DIR%)...
@call compile.bat %FB_ROOT_PATH%\builds\win32\%VS_VER%\FirebirdBoot codes_%FB_TARGET_PLATFORM%.log codes
if errorlevel 1 goto :codes2
@goto :EOF
:codes2
echo.
echo Error building codes, see codes_%FB_TARGET_PLATFORM%.log
echo.
set ERRLEV=1
goto :EOF

::==============
:databases
@rmdir /s /q %FB_GEN_DIR%\dbs 2>nul
@mkdir %FB_GEN_DIR%\dbs 2>nul

@echo Create security4.fdb...
@echo create database '%FB_GEN_DB_DIR%\dbs\security4.fdb'; | "%FB_BIN_DIR%\isql" -q
@echo Apply security.sql...
@"%FB_BIN_DIR%\isql" -q %FB_GEN_DB_DIR%/dbs/security4.fdb -i %FB_ROOT_PATH%\src\dbs\security.sql
@copy %FB_GEN_DIR%\dbs\security4.fdb %FB_GEN_DIR%\dbs\security.fdb > nul

@echo Restore metadata.gbak...
@%FB_BIN_DIR%\gbak -r %FB_ROOT_PATH%\builds\misc\metadata.gbak %FB_GEN_DB_DIR%/dbs/metadata.fdb

@call create_msgs.bat db

@%FB_BIN_DIR%\gbak -r %FB_ROOT_PATH%\builds\misc\help.gbak %FB_GEN_DB_DIR%/dbs/help.fdb
@copy %FB_GEN_DIR%\dbs\metadata.fdb %FB_GEN_DIR%\dbs\yachts.lnk > nul

@goto :EOF


::==============
:NEXT_STEP
@echo.
@echo    You may now run make_all.bat [DEBUG] [CLEAN]
@echo.
@goto :EOF

:END
