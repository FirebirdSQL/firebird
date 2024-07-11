@echo off

@call setenvvar.bat %*
@if errorlevel 1 (goto :END)

@echo Building build_msg (%FB_OBJ_DIR%)...
@call compile.bat builds\win32\%VS_VER%\FirebirdBoot build_msg_%FB_TARGET_PLATFORM%.log build_msg
@if errorlevel 1 (goto :END)

@echo Building message file...
@%FB_BIN_DIR%\build_msg -f %FB_GEN_DB_DIR%\firebird.msg -c %FB_GEN_DIR%\iberror_c.h
@copy %FB_GEN_DIR%\firebird.msg %FB_BIN_DIR% > nul

:END
