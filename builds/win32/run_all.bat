
@echo off

:: Reset or clear some variables, as appropriate.
set ERRLEV=0
set FBBUILD_NOCLEAN=
set FBBUILD_REAL_CLEAN=
set FBBUILD_BUILDTYPE=release
set FBBUILD_INCLUDE_PDB=
set FBBUILD_MAKE_KITS_ONLY=
set FBBUILD_BUILD_ONLY=0

::Check if on-line help is required
for %%v in ( %1 %2 %3 %4 %5 %6 %7 %8 %9 )  do (
  ( @if /I "%%v"=="-h" (goto :HELP & goto :EOF) )
  ( @if /I "%%v"=="/h" (goto :HELP & goto :EOF) )
  ( @if /I "%%v"=="HELP" (goto :HELP & goto :EOF) )
)


:: Read the command line
for %%v in ( %* )  do (
( if /I "%%v"=="NOCLEAN" (set FBBUILD_NOCLEAN=1) )
( if /I "%%v"=="REALCLEAN" (set FBBUILD_REAL_CLEAN=REALCLEAN) )
( if /I "%%v"=="DEBUG" (set FBBUILD_BUILDTYPE=debug) )
( if /I "%%v"=="PDB" (set FBBUILD_INCLUDE_PDB=1) )
( if /I "%%v"=="REPACK" (set FBBUILD_MAKE_KITS_ONLY=1) )
( if /I "%%v"=="JUSTBUILD" (set FBBUILD_BUILD_ONLY=1) )
)

call :SETVCENV

if defined FBBUILD_MAKE_KITS_ONLY (goto :MAKE_KITS & goto :EOF)


:: Go to work
if not defined FBBUILD_NOCLEAN (call clean_all %FBBUILD_REAL_CLEAN%)
:: We do not support debug builds of icu, so we don't pass %FBBUILD_BUILDTYPE%
call make_icu
if "%ERRLEV%"=="1" goto :END
call make_boot %FBBUILD_BUILDTYPE%
if "%ERRLEV%"=="1" goto :END
call make_all %FBBUILD_BUILDTYPE%
if "%ERRLEV%"=="1" goto :END
call make_examples %FBBUILD_BUILDTYPE%
if "%ERRLEV%"=="1" goto :END

if "%FBBUILD_BUILD_ONLY%"=="1" goto :END

:MAKE_KITS
:: Package everything up
pushd ..\install\arch-specific\win32
call BuildExecutableInstall ISX ZIP EMB %FBBUILD_BUILDTYPE%
if "%ERRLEV%"=="1" ( @echo Oops - some sort of error & popd & goto :END)
if defined FBBUILD_INCLUDE_PDB (
set /A FBBUILD_PACKAGE_NUMBER-=1
call BuildExecutableInstall ISX ZIP EMB %FBBUILD_BUILDTYPE% PDB
)
popd

goto :END
::---------

:HELP
@echo.
@echo The following params may be passed:
@echo.
@echo    NOCLEAN   - don't run CLEAN_ALL.BAT
@echo.
@echo    REALCLEAN   - Run CLEAN_ALL.BAT REALCLEAN
@echo                  This will do a deeper clean.
@echo                  Recommended for multi-platform builds
@echo.
@echo    DEBUG     - Do a DEBUG build (for experienced developers only.)
@echo                This switch is not needed to debug Firebird.
@echo.
@echo    PDB       - Create PDB packages as well as standard kits
@echo.
@echo    REPACK    - Don't build - just repack kits.
@echo.
@echo    JUSTBUILD - Just build - don't create packages.
@echo.
@goto :EOF
::---------


:SETVCENV
::===============================
:: Set up the compiler environment

if DEFINED VS170COMNTOOLS (
@devenv /? >nul 2>nul
@   if errorlevel 9009 (call "%VS170COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS170COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS160COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS160COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS160COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
if DEFINED VS150COMNTOOLS (
@devenv /? >nul 2>nul
@if errorlevel 9009 (call "%VS150COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE%) else ( echo    The file: & @echo      "%VS150COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" %PROCESSOR_ARCHITECTURE% & echo    has already been executed.)
) else (
@goto :HELP
)
)
)
goto :END
::---------


:END

if "%ERRLEV%"=="1" exit /b 1
