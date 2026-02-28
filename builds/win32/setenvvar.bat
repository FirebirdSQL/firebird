:: This batch file sets the environment values
:: FB_ROOT_PATH dos format path of the main directory
:: FB_DB_PATH unix format path of the main directory
:: (This is used by gpre and preprocess.bat)
:: VS_VER VisualStudio version (msvc7|msvc8|msvc9)
:: SERVER_NAME server needed to connect to firebird (could include port)
::   Example : localhost/3051
:: (Note - SERVER_NAME is almost deprecated - it is only used by
::   make_examples.bat
::
:: Note for MSVC8/9 and 64-bit build environment: The assumption is that a
:: 64-bit host is available and builds are targeted at either the native
:: 64-bit environment or the 32-bit WOW64 environment. Cross-compiling 64-bit
:: from a 32-bit host is not currently supported from this set of batch files.
::

@echo off

:: 1. Determine target platform
if "%Platform%"=="x86"  set FB_TARGET_PLATFORM=Win32
if "%Platform%"=="x64"  set FB_TARGET_PLATFORM=x64

:: 2.If Platform variable is not set, use PROCESSOR_ARCHITECTURE
if "%FB_TARGET_PLATFORM%"=="Win32" set FB_PROCESSOR_ARCHITECTURE=x86
if "%FB_TARGET_PLATFORM%"=="x64"   set FB_PROCESSOR_ARCHITECTURE=AMD64

::===============================
::Set up the compiler environment

:: To disable VS8/VS9/VS10 build, slightly alter the env var names in "if" conditions below

if DEFINED VS170COMNTOOLS (
@echo    The file: & @echo      "%VS170COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.
@set MSVC_VERSION=15
) else (
if DEFINED VS160COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS160COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS160COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS150COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS150COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS150COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS140COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS120COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS100COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS90COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS80COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS80COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS80COMNTOOLS%\..\..\VC\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS71COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS71COMNTOOLS%vsvars32.bat") else ( echo    The file: & echo      "%VS71COMNTOOLS%vsvars32.bat" & echo    has already been executed.)
) else (
@goto :HELP
)
)
)
)
)
)
)
)
)
@echo.


::=================
:SET_DB_DIR

@SET SERVER_NAME=localhost

@cd ..\..
@for /f "delims=" %%a in ('@cd') do (set FB_ROOT_PATH=%%~sa)
@cd %~dp0
@for /f "tokens=*" %%a in ('@echo %FB_ROOT_PATH:\=/%') do (set FB_DB_PATH=%%a)

::=================
:SET_MSVC_VER

@if "%VS_VER_EXPRESS%" == "" (
  @vcexpress /? >nul 2>nul
  @if not errorlevel 9009 (
    @if DEFINED VS80COMNTOOLS ((set MSVC_VERSION=8) & (set VS_VER=msvc8))
    @if DEFINED VS90COMNTOOLS ((set MSVC_VERSION=9) & (set VS_VER=msvc9))
    @if DEFINED VS100COMNTOOLS ((set MSVC_VERSION=10) & (set VS_VER=msvc10))

    set VS_VER_EXPRESS=1
    goto :SET_FB_TARGET_PLATFORM
  )

  @set VS_VER_EXPRESS=
)

@if "%MSVC_VERSION%" == "" (
  @for /f "delims=." %%a in ('@devenv /?') do (
    @for /f "tokens=6" %%b in ("%%a") do ((set MSVC_VERSION=%%b) & (set VS_VER=msvc%%b) & (goto :SET_FB_TARGET_PLATFORM))
  )
)

@if not defined MSVC_VERSION goto :HELP

:SET_FB_TARGET_PLATFORM
@set FB_TARGET_PLATFORM=Win32

:: Use MSVC 14 (VS 2015) project files for VS 2017/2019 tools.
@if %MSVC_VERSION% GEQ 15 (
  @set MSVC_VERSION=15
  @set VS_VER=msvc15
)

:: If MSVC >= 8 then we can test for processor architecture
:: We certainly don't want to try and set platform=x64 if MSVC7 is installed
@if %MSVC_VERSION% GEQ 8 (
@if "%FB_PROCESSOR_ARCHITECTURE%"=="x86" (set FB_TARGET_PLATFORM=Win32)
@if "%FB_PROCESSOR_ARCHITECTURE%"=="AMD64" (set FB_TARGET_PLATFORM=x64)
)


@set FB_OUTPUT_DIR=%FB_ROOT_PATH%\output_%FB_TARGET_PLATFORM%
@set FB_TEMP_DIR=%FB_ROOT_PATH%\temp\%FB_TARGET_PLATFORM%
@set FB_INSTALL_SCRIPTS=%FB_ROOT_PATH%\builds\install\arch-specific\win32
@set FB_GEN_DIR=%FB_ROOT_PATH%\gen
@set FB_GEN_DB_DIR=%FB_DB_PATH%/gen
@set FB_ICU_SOURCE_BIN=%FB_ROOT_PATH%\extern\icu\%FB_TARGET_PLATFORM%\release\bin\
@set FIREBIRD_BOOT_BUILD=1




goto :END


::===========
:HELP
@echo.
@echo    ERROR:
@echo    A working version of Visual Studio cannot be found
@echo    on your current path.
@echo.
@echo    You need MS Visual Studio 7 or newer to build Firebird
@echo    from these batch files.
@echo.
@echo    A properly installed version of Visual Studio will set
@echo    an environment variable such as %%VS71COMNTOOLS%% or
@echo    %%VS80COMNTOOLS%%. We use that variable to run the
@echo    appropriate batch file to set up the build environment.
@echo.
:: set errorlevel
@exit /B 1

:END
@echo.
@echo    Setting Environment Variables thus...
@echo.
@echo    vs_ver=%VS_VER%
if defined VS_VER_EXPRESS (
@echo    vs_ver_express=%VS_VER_EXPRESS%
)
@echo    platform=%FB_TARGET_PLATFORM%
@echo    msvc_version=%MSVC_VERSION%
@echo    db_path=%FB_DB_PATH%
@echo    root_path=%FB_ROOT_PATH%
@echo    server_name=%SERVER_NAME%
@echo.
@echo    (End of %0)
@echo.

@exit /B 0
