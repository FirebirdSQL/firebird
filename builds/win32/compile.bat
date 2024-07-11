@echo off
::==============
:: compile.bat solution, output, [projects...]
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
	set config=release
) else (
	set config=debug
)

:: Special case for CDS, set in make_boot only
@if "%FB_LIBCDS%"=="1" (
	set config=%config%-static
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
