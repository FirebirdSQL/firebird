@echo off
if "%~1"=="" (
instreg install -z
)
setlocal
set INSTALL_SERVICE=install -z
set START_SERVICE=start
if not "%~1"=="" (
set INSTALL_SERVICE=%INSTALL_SERVICE% -n %1
set START_SERVICE=%START_SERVICE% -n %1
)
instsvc %INSTALL_SERVICE%
instsvc %START_SERVICE%
endlocal
@echo.
@echo Firebird installed and started as a service using
@echo architecture specified in firebird.conf.
@echo.

