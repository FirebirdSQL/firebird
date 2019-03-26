@echo off
set FB_DBG=
set FB_OBJ_DIR=release
set FB_CLEAN=/build
set FB_ICU=
set FB_VC_CRT_ARCH=%FB_PROCESSOR_ARCHITECTURE%

for %%v in ( %* )  do (
  ( if /I "%%v"=="DEBUG" ( (set FB_DBG=TRUE) && (set FB_OBJ_DIR=debug) ) )
  ( if /I "%%v"=="CLEAN" (set FB_CLEAN=/rebuild) )
  ( if /I "%%v"=="ICU" ( (set FB_ICU=1) && (set FB_DBG=) ) )
  ( if /I "%%v"=="RELEASE" ( (set FB_DBG=) && (set FB_OBJ_DIR=release) ) )
)

if %FB_VC_CRT_ARCH% == AMD64 ( set FB_VC_CRT_ARCH=x64)

if defined VS150COMNTOOLS (
  if exist "%VS150COMNTOOLS%\..\..\VC\redist\MSVC" (
    for /D %%d in ( "%VS150COMNTOOLS%\..\..\VC\redist\MSVC\*" ) do (
      set FB_VC_CRT_VER=MSVC\%%~nxd
    )
  )
)

if defined FB_VC_CRT_VER (
  set FB_VC_CRT_DIR=%FB_VC_CRT_VER%\%FB_VC_CRT_ARCH%
) else (
  set FB_VC_CRT_DIR=%FB_VC_CRT_ARCH%
)

set FB_OBJ_DIR=%FB_TARGET_PLATFORM%\%FB_OBJ_DIR%
set FB_BIN_DIR=%FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\

@echo    Executed %0
@echo.
