::
:: This bat file doesn't use cd, all the paths are full paths.
:: with this convention this bat file is position independent
:: and it will be easier to move the place of somefiles.
::

@echo off
set ERRLEV=0

:CHECK_ENV
@call setenvvar.bat %*
@if errorlevel 1 (goto :END)

@setlocal EnableDelayedExpansion

::===========
:MAIN
@echo.

@echo Creating directories
:: Create the directory hierarchy.
for %%v in ( alice auth burp dsql gpre isql jrd misc msgs examples yvalve utilities) do (
  @mkdir %FB_GEN_DIR%\%%v 2>nul
)

@mkdir %FB_GEN_DIR%\utilities\gstat 2>nul
@mkdir %FB_GEN_DIR%\auth\SecurityDatabase 2>nul
@mkdir %FB_GEN_DIR%\gpre\std 2>nul

@mkdir %FB_BIN_DIR%\tzdata 2>nul

call :interfaces
if "!ERRLEV!"=="1" goto :END

call :LibTom
if "!ERRLEV!"=="1" goto :END

call :decNumber
if "!ERRLEV!"=="1" goto :END

if "%FB_TARGET_PLATFORM%"=="x64" call :ttmath
if "!ERRLEV!"=="1" goto :END

call :zlib
if "!ERRLEV!"=="1" goto :END

@if "%FB_CLIENT_ONLY%"=="" (
	call :re2
	if "!ERRLEV!"=="1" goto :END

	call :btyacc
	if "!ERRLEV!"=="1" goto :END

	call :libcds
	if "!ERRLEV!"=="1" goto :END

	echo Generating DSQL parser...
	call parse.bat %*
	if "!ERRLEV!"=="1" goto :END

	::=======
	call :gpre_boot
	if "!ERRLEV!"=="1" goto :END

	::=======
	echo Preprocessing the source files needed to build gpre and isql...
	call preprocess.bat %FB_CONFIG% BOOT

	::=======
	call :engine
	if "!ERRLEV!"=="1" goto :END

	call :gpre
	if "!ERRLEV!"=="1" goto :END

	call :isql
	if "!ERRLEV!"=="1" goto :END
)

@mkdir %FB_BIN_DIR% >nul 2>&1
@mkdir %FB_BIN_DIR%\intl\ >nul 2>&1

:: copy conf files only if not exists already
for %%v in (firebird plugins) do (
	if not exist %FB_BIN_DIR%\%%v.conf (
		@copy %FB_ROOT_PATH%\builds\install\misc\%%v.conf %FB_BIN_DIR% >nul 2>&1
	)
)

@if "%FB_CLIENT_ONLY%"=="" (
	:: copy conf files only if not exists already
	for %%v in (databases replication) do (
		if not exist %FB_BIN_DIR%\%%v.conf (
			copy %FB_ROOT_PATH%\builds\install\misc\%%v.conf %FB_BIN_DIR% >nul 2>&1
		)
	)

	if not exist %FB_BIN_DIR%\intl\fbintl.conf (
		copy %FB_ROOT_PATH%\builds\install\misc\fbintl.conf %FB_BIN_DIR%\intl\ >nul 2>&1
	)
)

:: Copy ICU and zlib to the output directory
@copy %FB_ROOT_PATH%\extern\icu\icudt???.dat %FB_BIN_DIR% >nul 2>&1
@copy %FB_ICU_SOURCE_BIN%\*.dll %FB_BIN_DIR% >nul 2>&1
@copy %FB_ROOT_PATH%\extern\icu\tzdata-extract\* %FB_BIN_DIR%\tzdata >nul 2>&1
@copy %FB_ROOT_PATH%\extern\zlib\%FB_TARGET_PLATFORM%\*.dll %FB_BIN_DIR% >nul 2>&1

@if "%FB_CLIENT_ONLY%"=="" (
	::=======
	call :databases
	if "!ERRLEV!"=="1" goto :END

	:: copy security db if not exists already
	if not exist %FB_BIN_DIR%\security6.fdb (
		copy %FB_GEN_DIR%\dbs\security6.fdb %FB_BIN_DIR%
	)

	::=======
	echo Preprocessing the entire source tree...
	call preprocess.bat %FB_CONFIG%
)

::=======
@call create_msgs.bat %FB_CONFIG%
::=======

@call :NEXT_STEP
@goto :END


::===================
:: BUILD btyacc
:btyacc
@echo.
@echo Building btyacc (%FB_OBJ_DIR%)...
@call compile.bat builds\win32\%VS_VER%\FirebirdBoot btyacc_%FB_TARGET_PLATFORM%.log btyacc
if errorlevel 1 call :boot2 btyacc
goto :EOF

::===================
:: BUILD LibTom
:LibTom
@echo.
@echo Building LibTomMath (%FB_OBJ_DIR%)...
@call compile.bat extern\libtommath\libtommath_MSVC%MSVC_VERSION% libtommath_%FB_CONFIG%_%FB_TARGET_PLATFORM%.log libtommath
if errorlevel 1 call :boot2 libtommath_%FB_OBJ_DIR%
@echo Building LibTomCrypt (%FB_OBJ_DIR%)...
@call compile.bat extern\libtomcrypt\libtomcrypt_MSVC%MSVC_VERSION% libtomcrypt_%FB_CONFIG%_%FB_TARGET_PLATFORM%.log libtomcrypt
if errorlevel 1 call :boot2 libtomcrypt_%FB_OBJ_DIR%
goto :EOF

::===================
:: BUILD decNumber
:decNumber
@echo.
@echo Building decNumber (%FB_OBJ_DIR%)...
@call compile.bat extern\decNumber\msvc\decNumber_MSVC%MSVC_VERSION% decNumber_%FB_CONFIG%_%FB_TARGET_PLATFORM%.log decNumber
if errorlevel 1 call :boot2 decNumber_%FB_OBJ_DIR%
goto :EOF

::===================
:: Build libcds
:libcds
@echo.
set FB_LIBCDS=1
@echo Building libcds (%FB_OBJ_DIR%)...
@call compile.bat extern\libcds\projects\Win\vc141\cds libcds_%FB_CONFIG%_%FB_TARGET_PLATFORM%.log cds
if errorlevel 1 call :boot2 libcds%FB_OBJ_DIR%
set FB_LIBCDS=
goto :EOF

::===================
:: BUILD ttmath
:ttmath
@echo.
@echo Building ttmath (%FB_OBJ_DIR%)...
@mkdir %FB_ROOT_PATH%\extern\ttmath\%FB_CONFIG% 2>nul
if /I "%FB_CONFIG%"=="debug" (
  @ml64.exe /c /Zi /Fo %FB_ROOT_PATH%\extern\ttmath\%FB_CONFIG%\ttmathuint_x86_64_msvc.obj %FB_ROOT_PATH%\extern\ttmath\ttmathuint_x86_64_msvc.asm
) else (
  @ml64.exe /c /Fo %FB_ROOT_PATH%\extern\ttmath\%FB_CONFIG%\ttmathuint_x86_64_msvc.obj %FB_ROOT_PATH%\extern\ttmath\ttmathuint_x86_64_msvc.asm
)
if errorlevel 1 call :boot2 ttmath_%FB_OBJ_DIR%
goto :EOF

::===================
:: BUILD re2
:re2
@echo.
@echo Building re2...
@mkdir %FB_ROOT_PATH%\extern\re2\builds\%FB_TARGET_PLATFORM% 2>nul
@pushd %FB_ROOT_PATH%\extern\re2\builds\%FB_TARGET_PLATFORM%
@cmake -G "%MSVC_CMAKE_GENERATOR%" -A %FB_TARGET_PLATFORM% -S %FB_ROOT_PATH%\extern\re2
if errorlevel 1 call :boot2 re2
@cmake --build %FB_ROOT_PATH%\extern\re2\builds\%FB_TARGET_PLATFORM% --target ALL_BUILD --config %FB_CONFIG% > re2_%FB_CONFIG%_%FB_TARGET_PLATFORM%.log
@popd
goto :EOF

::===================
:: Build CLOOP and generate interface headers
:interfaces
@echo.
@echo Building CLOOP and generating interfaces...
@nmake /s /x interfaces_%FB_TARGET_PLATFORM%.log /f gen_helper.nmake updateCloopInterfaces
if errorlevel 1 call :boot2 interfaces
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
@call compile.bat builds\win32\%VS_VER%\FirebirdBoot gpre_boot_%FB_TARGET_PLATFORM%.log gpre_boot
if errorlevel 1 call :boot2 gpre_boot
goto :EOF

::===================
:: BUILD engine
:engine
@echo.
@echo Building engine (%FB_OBJ_DIR%)...
@call compile.bat builds\win32\%VS_VER%\Firebird engine_%FB_TARGET_PLATFORM%.log DLLs\engine
@call compile.bat builds\win32\%VS_VER%\Firebird engine_%FB_TARGET_PLATFORM%.log DLLs\ib_util
if errorlevel 1 call :boot2 engine
@goto :EOF

::===================
:: BUILD gpre
:gpre
@echo.
@echo Building gpre (%FB_OBJ_DIR%)...
@call compile.bat builds\win32\%VS_VER%\Firebird gpre_%FB_TARGET_PLATFORM%.log EXEs\gpre
if errorlevel 1 call :boot2 gpre
@goto :EOF

::===================
:: BUILD isql
:isql
@echo.
@echo Building isql (%FB_OBJ_DIR%)...
@call compile.bat builds\win32\%VS_VER%\Firebird isql_%FB_TARGET_PLATFORM%.log EXEs\isql
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


::==============
:databases
@rmdir /s /q %FB_GEN_DIR%\dbs 2>nul
@mkdir %FB_GEN_DIR%\dbs 2>nul

@echo Create security6.fdb...
@echo create database '%FB_GEN_DB_DIR%\dbs\security6.fdb'; | "%FB_BIN_DIR%\isql" -q > nul
if errorlevel 1 call :boot2 databases & goto :EOF

@echo Apply security.sql...
@"%FB_BIN_DIR%\isql" -q %FB_GEN_DB_DIR%/dbs/security6.fdb -i %FB_ROOT_PATH%\src\dbs\security.sql > nul
if errorlevel 1 call :boot2 databases & goto :EOF

@mklink %FB_GEN_DIR%\dbs\security.fdb %FB_GEN_DIR%\dbs\security6.fdb > nul
if errorlevel 1 (
  @copy %FB_GEN_DIR%\dbs\security6.fdb %FB_GEN_DIR%\dbs\security.fdb > nul
)
if errorlevel 1 call :boot2 databases & goto :EOF

@echo Creating metadata.fdb...
@echo create database '%FB_GEN_DB_DIR%/dbs/metadata.fdb'; | "%FB_BIN_DIR%\isql" -q -sqldialect 1 > nul
if errorlevel 1 call :boot2 databases & goto :EOF

@mklink %FB_GEN_DIR%\dbs\yachts.lnk %FB_GEN_DIR%\dbs\metadata.fdb > nul
if errorlevel 1 (
  @copy %FB_GEN_DIR%\dbs\metadata.fdb %FB_GEN_DIR%\dbs\yachts.lnk > nul
)
if errorlevel 1 call :boot2 databases

@goto :EOF


::==============
:NEXT_STEP
@echo.
@echo    You may now run make_all.bat [DEBUG] [CLEAN]
@echo.
@goto :EOF

:END
endlocal
