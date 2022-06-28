@echo off
set FB_DBG=
set FB_CONFIG=release
set FB_CLEAN=
set FB_ICU=

for %%v in ( %* )  do (
  ( if /I "%%v"=="DEBUG" ( (set FB_DBG=TRUE) && (set FB_CONFIG=debug) ) )
  ( if /I "%%v"=="CLEAN" (set FB_CLEAN=:rebuild) )
  ( if /I "%%v"=="ICU" ( (set FB_ICU=1) && (set FB_DBG=) ) )
  ( if /I "%%v"=="RELEASE" ( (set FB_DBG=) && (set FB_CONFIG=release) ) )
)

set FB_OBJ_DIR=%FB_TARGET_PLATFORM%\%FB_CONFIG%
set FB_BIN_DIR=%FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\firebird\

@echo    Executed %0
@echo.
