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
::==============
:: compile_static.bat solution, output, [projects...]
::
::   Note: Our projects create object files in temp/$platform/$config
::     but we call devenv with $config|$platform (note variable in reverse order
::     and odd syntax).

setlocal
set solution=%1
set output_path=%~dp2
set output=%2
set projects=

@if "%FB_DBG%"=="" (
	set config=releaseStatic
) else (
	set config=debugStatic
)

shift
shift

:loop_start

if "%1" == "" goto loop_end

set projects=%projects% /target:%1%FB_CLEAN%

shift
goto loop_start

:loop_end

if not exist "%output_path%" mkdir "%output_path%"
msbuild "%FB_LONG_ROOT_PATH%\%solution%.sln" /maxcpucount /p:Configuration=%config% /p:Platform=%FB_TARGET_PLATFORM% %projects% /fileLoggerParameters:LogFile=%output%

endlocal

goto :EOF
