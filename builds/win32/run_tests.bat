@echo off

@echo.

@call setenvvar.bat %*
@if errorlevel 1 (goto :END)

@%FB_BIN_DIR%\common_test --log_level=all
@%FB_BIN_DIR%\engine_test --log_level=all
@%FB_BIN_DIR%\isql_test --log_level=all

:END
