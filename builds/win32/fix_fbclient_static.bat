:: The contents of this file are subject to the Initial
:: Developer's Public License Version 1.0 (the "License");
:: you may not use this file except in compliance with the
:: License. You may obtain a copy of the License at
:: http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
::
:: Software distributed under the License is distributed AS IS,
:: WITHOUT WARRANTY OF ANY KIND, either express or implied.
:: See the License for the specific language governing rights
:: and limitations under the License.
::
:: The Original Code was created by Adriano dos Santos Fernandes
:: for the Firebird Open Source RDBMS project.
::
:: Copyright (c) 2026 Adriano dos Santos Fernandes <adrianosf@gmail.com>
:: and all contributors signed below.
::
:: All Rights Reserved.
:: Contributor(s): ______________________________________.

@echo off
:: Rename every non-public global symbol in fbclient_static.lib so the
:: static client does not override the host application's own symbols.
::
:: Mirrors the POSIX static_client.sh.in approach: generates a comprehensive
:: rename map from llvm-nm output, filters out public API symbols from
:: firebird.def, and renames everything else with a __fbclient_ prefix.
::
:: This covers global C++ operators, decNumber symbols, internal C++ symbols,
:: and all other non-public definitions in one pass.
::
:: MSVC static libraries that pull in system import libs (e.g. ws2_32.lib
:: via remote.vcxproj) embed short-import members that llvm-objcopy cannot
:: process ("unsupported object file format"). So this script:
::   1) extracts the archive
::   2) builds a symbol rename map from every defined global not in the API
::   3) runs llvm-objcopy on each real COFF .obj member
::   4) rebuilds the archive from those objects only (import stubs dropped;
::      the final application must still link ws2_32.lib / mpr.lib itself,
::      same as the shared client import library requires)
::
:: Invoked from builds/win32/make_all.bat after the CLIENT_ONLY=STATIC build.
:: See doc/README.StaticClient.md for the full rationale.
::
:: Usage: fix_fbclient_static.bat <path to fbclient_static.lib>

setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" goto :usage

set "LibPath=%~1"
set "ScriptDir=%~dp0"
set "DefFile=%ScriptDir%defs\firebird.def"
set "WorkDir=%~dp1fix_fbclient_static_tmp"
set "ExtractDir=%WorkDir%\extract"
set "ApiSyms=%WorkDir%\api_syms.txt"
set "NmOut=%WorkDir%\nm_out.txt"
set "RenameMap=%WorkDir%\rename_map.txt"
set "OutLib=%WorkDir%\fbclient_static_out.lib"

if not exist "%LibPath%" (
	echo fix_fbclient_static: library not found: %LibPath%
	exit /b 1
)
if not exist "%DefFile%" (
	echo fix_fbclient_static: firebird.def not found: %DefFile%
	exit /b 1
)

if exist "%WorkDir%" rmdir /s /q "%WorkDir%"
mkdir "%ExtractDir%"
if errorlevel 1 (
	echo fix_fbclient_static: cannot create temp dir: %ExtractDir%
	exit /b 1
)

:: Extract archive
echo Extracting %LibPath%...
pushd "%ExtractDir%"
llvm-ar x "%LibPath%"
if errorlevel 1 (
	echo fix_fbclient_static: llvm-ar extract failed with exit code %ERRORLEVEL%
	popd
	call :cleanup
	exit /b 1
)

:: Step 1: Parse firebird.def for API symbols (undecorated names).
:: For alias lines (sym=real), only the real name is retained since that is
:: what appears in the object files (after x86 _-decoration stripping below).
echo Building API symbol list from firebird.def...
if exist "%ApiSyms%" del /f /q "%ApiSyms%"
for /f "usebackq tokens=1 delims=	 " %%A in ("%DefFile%") do (
	set "_line=%%A"
	if not "!_line!"=="" (
		if not "!_line:~0,1!"==";" (
			if /I not "!_line!"=="EXPORTS" (
				set "_entry="
				for /f "tokens=2 delims==" %%X in ("!_line!") do (
					if not "%%X"=="" set "_entry=%%X"
				)
				if not defined _entry set "_entry=!_line!"
				>>"%ApiSyms%" echo !_entry!
			)
		)
	)
)

:: Step 2: Run llvm-nm on each extracted object (not on the archive, to avoid
:: archive-member prefix lines in the output) and collect all defined globals.
echo Generating symbol rename map from defined globals...
if exist "%NmOut%" del /f /q "%NmOut%"
for %%F in ("*.obj") do (
	llvm-nm --defined-only -g "%%~fF" >> "%NmOut%"
)

:: Step 3: Build rename map. For each defined global:
::   - C++ mangled (?-prefixed): always rename (not an API symbol).
::   - C symbols: strip leading _ (x86 __cdecl), optionally strip @n (x86
::     __stdcall) for API matching; if not in API list, rename with prefix.
:: The @n suffix (if any) is preserved in the renamed target so that the
:: calling-convention encoding remains consistent on x86.
set "RenameCount=0"
if exist "%RenameMap%" del /f /q "%RenameMap%"
for /f "usebackq tokens=1,2,3" %%A in ("%NmOut%") do (
	if not "%%C"=="" (
		set "orig=%%C"
		call :process_symbol
	)
)

if %RenameCount% EQU 0 (
	echo fix_fbclient_static: no symbols to rename
	popd
	call :cleanup
	exit /b 0
)

:: Deduplicate rename map (same symbol may appear in multiple objects)
set "RenameSorted=%WorkDir%\rename_sorted.txt"
sort /UNIQUE "%RenameMap%" /O "%RenameSorted%"
move /y "%RenameSorted%" "%RenameMap%" >nul

echo %RenameCount% symbols will be renamed.

:: Step 4: Apply rename map to each object
echo Renaming symbols in object members...
set "ObjCount=0"
set "FailCount=0"
for %%F in ("*.obj") do (
	llvm-objcopy --redefine-syms="%RenameMap%" "%%~fF"
	if errorlevel 1 (
		echo fix_fbclient_static: llvm-objcopy failed on %%F
		set /a FailCount+=1
	) else (
		set /a ObjCount+=1
	)
)

if !FailCount! NEQ 0 (
	echo fix_fbclient_static: !FailCount! object^(s^) failed redefine
	popd
	call :cleanup
	exit /b 1
)
if !ObjCount! EQU 0 (
	echo fix_fbclient_static: no .obj members found in %LibPath%
	popd
	call :cleanup
	exit /b 1
)

:: Step 5: Rebuild archive from (now-renamed) objects only
echo Rebuilding static library from !ObjCount! object^(s^)...
if exist "%OutLib%" del /f /q "%OutLib%"
llvm-ar rcs "%OutLib%" *.obj
if errorlevel 1 (
	echo fix_fbclient_static: llvm-ar rebuild failed with exit code %ERRORLEVEL%
	popd
	call :cleanup
	exit /b 1
)
popd

copy /y "%OutLib%" "%LibPath%" >nul
if errorlevel 1 (
	echo fix_fbclient_static: failed to replace %LibPath%
	call :cleanup
	exit /b 1
)

echo fix_fbclient_static: updated %LibPath% (%RenameCount% symbols renamed)
call :cleanup
exit /b 0

:usage
echo Usage: %~nx0 ^<path to fbclient_static.lib^>
exit /b 1

:cleanup
if exist "%WorkDir%" rmdir /s /q "%WorkDir%"
goto :eof

:: -------------------------------------------------------------------
:: Process a single symbol from the nm output (orig set by caller).
:: -------------------------------------------------------------------
:process_symbol
set "bare=!orig!"

:: C++ mangled (?-prefixed) -- always rename, never an API symbol
if "!bare:~0,1!"=="?" (
	>>"%RenameMap%" echo !orig! __fbclient_!orig!
	set /a RenameCount+=1
	goto :eof
)

:: Strip leading underscores (x86 __cdecl name decoration)
for /l %%I in (1,1,32) do (
	if not "!bare!"=="" if "!bare:~0,1!"=="_" set "bare=!bare:~1!"
)

:: Strip @n suffix (x86 __stdcall / __fastcall decoration) for API matching
set "check=!bare!"
for /f "tokens=1 delims=@" %%S in ("!check!") do set "check=%%S"

:: API symbol check (undecorated name against firebird.def entries)
findstr /X /I /L "!check!" "%ApiSyms%" >nul 2>&1
if not errorlevel 1 goto :eof

:: Not an API symbol -- rename with __fbclient_ prefix.
:: The bare name (with @n preserved if present) is used as the new-symbol
:: suffix so that x86 __stdcall linkage is not broken.
>>"%RenameMap%" echo !orig! __fbclient_!bare!
set /a RenameCount+=1
goto :eof
