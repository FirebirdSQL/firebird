:: This batch file sets the environment values
:: FB_ROOT_PATH dos format path of the main directory
:: FB_DB_PATH unix format path of the main directory
:: (This is used by gpre and preprocess.bat)
:: VS_VER VisualStudio version (msvc10|msvc12|msvc14)
:: SERVER_NAME server needed to connect to firebird (could include port)
::   Example : localhost/3051
:: (Note - SERVER_NAME is almost deprecated - it is only used by
::   make_examples.bat
::

@echo off

::set FB_PROCESSOR_ARCHITECTURE=AMD64

:: Default target CPU architecture is the native environment
if NOT DEFINED FB_PROCESSOR_ARCHITECTURE (
set FB_PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%
)

::===============================
::Set up the compiler environment

:: To disable some particular MSVC build, slightly alter the env var names in "if" conditions below

if DEFINED VS150COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS150COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS150COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %FB_PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
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
@goto :HELP
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
    @if DEFINED VS100COMNTOOLS ((set MSVC_VERSION=10) & (set VS_VER=msvc10))

    set VS_VER_EXPRESS=1
    goto :SET_FB_TARGET_PLATFORM
  )

  @set VS_VER_EXPRESS=
)

@for /f "delims=." %%a in ('@devenv /?') do (
  @for /f "tokens=6" %%b in ("%%a") do ((set MSVC_VERSION=%%b) & (set VS_VER=msvc%%b) & (goto :SET_FB_TARGET_PLATFORM))
)

@if not defined MSVC_VERSION goto :HELP


:SET_FB_TARGET_PLATFORM
@set FB_TARGET_PLATFORM=Win32
@if "%FB_PROCESSOR_ARCHITECTURE%"=="x86" (set FB_TARGET_PLATFORM=Win32)
@if "%FB_PROCESSOR_ARCHITECTURE%"=="AMD64" (set FB_TARGET_PLATFORM=x64)


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
@echo    You need MS Visual Studio 8 or newer to build Firebird
@echo    from these batch files.
@echo.
@echo    A properly installed version of Visual Studio will set
@echo    an environment variable such as %%VS80COMNTOOLS%%.
@echo    We use that variable to run the appropriate batch file
@echo    to set up the build environment.
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
