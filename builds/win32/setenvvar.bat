:: This batch file sets the environment values
:: FB_ROOT_PATH dos format path of the main directory
:: FB_LONG_ROOT_PATH long format path of the main directory
:: FB_DB_PATH unix format path of the main directory
:: (This is used by gpre and preprocess.bat)
:: VS_VER VisualStudio version (msvc12|msvc14|msvc15)

@echo off

::set FB_PROCESSOR_ARCHITECTURE=AMD64

:: Default target CPU architecture is the native environment
if NOT DEFINED FB_PROCESSOR_ARCHITECTURE (
set FB_PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%
)

::=================
:SET_MSVC_VER

if DEFINED VS170COMNTOOLS (
@set MSVC_VERSION=15
@set MSVC_CMAKE_GENERATOR=Visual Studio 17 2022
) else (
if DEFINED VS160COMNTOOLS (
@set MSVC_VERSION=15
@set MSVC_CMAKE_GENERATOR=Visual Studio 16 2019
) else (
if DEFINED VS150COMNTOOLS (
@set MSVC_VERSION=15
@set MSVC_CMAKE_GENERATOR=Visual Studio 15
) else (
@goto :HELP
)
)
)

set VS_VER=msvc%MSVC_VERSION%

@echo.


::=================
:SET_DB_DIR

@cd ..\..
@for /f "delims=" %%a in ('@cd') do (set FB_LONG_ROOT_PATH=%%a)
@for /f "delims=" %%a in ('@cd') do (set FB_ROOT_PATH=%%~sa)
@cd %~dp0
@for /f "tokens=*" %%a in ('@echo %FB_ROOT_PATH:\=/%') do (set FB_DB_PATH=%%a)


::=================
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
@echo    You need MS Visual Studio 12 or newer to build Firebird
@echo    from these batch files.
@echo.
@echo    A properly installed version of Visual Studio will set
@echo    an environment variable such as %%VS150COMNTOOLS%%.
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
@echo.
@echo    (End of %0)
@echo.

@exit /B 0
