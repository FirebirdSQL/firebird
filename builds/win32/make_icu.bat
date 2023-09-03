@echo off


:: Set env vars
@call setenvvar.bat %*

::==========
:: MAIN

@echo Extracting pre-built ICU and tzdata
mkdir %FB_ROOT_PATH%\extern\icu\tzdata-extract 2> nul

:: FB_UNZIP could be set by caller, else try to find unzip in PATH or at the GIT folder

if not defined FB_UNZIP (
  @for /f "tokens=*" %%a in ('where unzip.exe 2^> nul') do (@SET FB_UNZIP=%%a)
)

if not defined FB_UNZIP (
  @for /f "tokens=*" %%a in ('where git 2^> nul') do (@SET FB_UNZIP=%%~dpa..\usr\bin\unzip.exe)
)

if not exist "%FB_UNZIP%" set FB_UNZIP=

if not defined FB_UNZIP (
  cscript /nologo unzip.vbs %FB_ROOT_PATH%\extern\icu\icu_windows.zip %FB_ROOT_PATH%\extern\icu
  cscript /nologo unzip.vbs %FB_ROOT_PATH%\extern\icu\icudt.zip %FB_ROOT_PATH%\extern\icu
  cscript /nologo unzip.vbs %FB_ROOT_PATH%\extern\icu\tzdata\le.zip %FB_ROOT_PATH%\extern\icu\tzdata-extract
) else (
  "%FB_UNZIP%" -o %FB_ROOT_PATH%\extern\icu\icu_windows.zip -d %FB_ROOT_PATH%\extern\icu
  "%FB_UNZIP%" -o %FB_ROOT_PATH%\extern\icu\icudt.zip -d %FB_ROOT_PATH%\extern\icu
  "%FB_UNZIP%" -o %FB_ROOT_PATH%\extern\icu\tzdata\le.zip -d %FB_ROOT_PATH%\extern\icu\tzdata-extract
)
if errorlevel 1 call :ERROR build failed - see make_icu_%FB_TARGET_PLATFORM%.log for details

@goto :EOF


:ERROR
::====
@echo.
@echo   An error occurred while running make_icu.bat -
@echo     %*
@echo.
set ERRLEV=1
cancel_script > nul 2>&1
::End of ERROR
::------------
@goto :EOF
