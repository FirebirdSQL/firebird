:: This batch file sets the environment values
:: FB_ROOT_PATH dos format path of the main directory
:: FB_LONG_ROOT_PATH long format path of the main directory
:: FB_DB_PATH unix format path of the main directory
:: (This is used by gpre and preprocess.bat)
:: VS_VER VisualStudio version (msvc15)

@echo off

:: Default target CPU architecture is the native environment
@if NOT DEFINED FB_PROCESSOR_ARCHITECTURE (
  set FB_PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%
)


::=================
:SET_MSVC_VER

:: NOTE: We introduce a new variable here - FB_VSCOMNTOOLS
:: This is intended to remove multiple tests for Visual Studio installs.
:: If FB_VSCOMNTOOLS is not already defined it will dynamically pick the
:: newest version of Visual Studio. If a specific version of Visual
:: is required then FB_VSCOMNTOOLS should be set in the environment prior
:: to running the build process.
:: NOTE 2: Do not change the assignment to FB_VSCOMNTOOLS after vcvarsall
:: has been executed. This change is not be detected (but it could be).
:: For now, if you wish to try a different version of Visual Studio you
:: should open a new command prompt and start afresh.
::

@if not DEFINED FB_VSCOMNTOOLS (
  if DEFINED VS170COMNTOOLS (
    set "FB_VSCOMNTOOLS=%VS170COMNTOOLS%"
  ) else (
    if DEFINED VS160COMNTOOLS (
      set "FB_VSCOMNTOOLS=%VS160COMNTOOLS%"
    ) else (
      if DEFINED VS150COMNTOOLS (
        set "FB_VSCOMNTOOLS=%VS150COMNTOOLS%"
      ) else (
        goto :HELP
      )
    )
  )
)

:: Now set some firebird build specific variables that depend upon the
:: version of Visual Studio that is being used for the build.
@if DEFINED FB_VSCOMNTOOLS (
  if "%FB_VSCOMNTOOLS%" == "%VS170COMNTOOLS%" (
    set MSVC_VERSION=15
    set MSVC_CMAKE_GENERATOR=Visual Studio 17 2022
  )
  if "%FB_VSCOMNTOOLS%" == "%VS160COMNTOOLS%" (
    set MSVC_VERSION=15
    set MSVC_CMAKE_GENERATOR=Visual Studio 16 2019
  )
  if "%FB_VSCOMNTOOLS%" == "%VS150COMNTOOLS%" (
    set MSVC_VERSION=15
    set MSVC_CMAKE_GENERATOR=Visual Studio 15
  )
) else (
    goto :HELP
)

:: Run vsvarsall just once during the build...
@if DEFINED FB_VSCOMNTOOLS (
  @if not defined VCToolsVersion (
    call "%FB_VSCOMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE%
  ) else (
    @echo    The file:
    @echo      "%FB_VSCOMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE%
    @echo    has already been executed.
  )
)

:: VS_VER is used to locate the correct set of Visual Studio project files.
:: They are stored in builds\win32\$VS_VER%. Currently (2022-06-16) only one set exists.
@set VS_VER=msvc%MSVC_VERSION%


:: We need to deploy runtime dlls with 140 in the file name. But these files are
:: stored in a directory name dependant upon the precise minor version of the
:: runtime. So we need to extract two values:
::   MAJOR runtime version
::   MINOR runtime version
:: which we then use to determine the location of the various runtime files reqd.
:: For now, MSVC_RUNTIME_LIBRARY_VERSION indicates the VS specific location.
:: MSVC_RUNTIME_FILE_VERSION represents the version string in the runtime file name.
:: If anyone can come up with better naming conventions please do not hesitate to
:: suggest them.
:: This has been tested on VS 2017, 2019 and 2022
:: Note that VCToolsVersion is only defined after vcvarsall.bat been run.
@if defined VCToolsVersion (
  set MSVC_RUNTIME_MAJOR_VERSION=%VCToolsVersion:~0,2%
  set MSVC_RUNTIME_MINOR_VERSION=%VCToolsVersion:~3,1%
  set MSVC_RUNTIME_LIBRARY_VERSION=%MSVC_RUNTIME_MAJOR_VERSION%%MSVC_RUNTIME_MINOR_VERSION%
  set MSVC_RUNTIME_FILE_VERSION=%MSVC_RUNTIME_MAJOR_VERSION%0
)
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
@echo    A working version of Visual Studio cannot be found.
@echo.
@echo    MS Visual Studio 2017 (MSVC 15) or newer is required to build Firebird
@echo    from these batch files.
@echo.
@echo    An environment variable such as %%VS150COMNTOOLS%% needs to be set.
@echo    This variable is not set automatically by the Visual Studio installer.
@echo    It must be set manually for your installation. For example:
@echo.
@echo      "set VS150COMNTOOLS=C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\Common7\\Tools"
@echo.
@echo    We use that variable to run the appropriate Visual Studio batch file
@echo    to set up the build environment.
@echo.
:: set errorlevel
@set ERRLEV=1
@exit /B 1

:END
@echo.
@echo    Setting Environment Variables thus...
@echo.
@echo    vs_ver=%VS_VER%
@echo    FB_VSCOMNTOOLS=%FB_VSCOMNTOOLS%
@echo    platform=%FB_TARGET_PLATFORM%
@echo    msvc_version=%MSVC_VERSION%
@echo    db_path=%FB_DB_PATH%
@echo    root_path=%FB_ROOT_PATH%
@echo.
@echo    (End of %0)
@echo.

@exit /B 0
