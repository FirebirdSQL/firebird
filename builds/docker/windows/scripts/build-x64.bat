REM FIXME @echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64

set FB_PROCESSOR_ARCHITECTURE=AMD64
set FB_OUTPUT_SUFFIX=x64

xcopy /h /e /i /q C:\firebird C:\firebird-build
cd /d C:\firebird-build\builds\win32

REM call run_all.bat
call run_all.bat JUSTBUILD

xcopy /h /e /i /q C:\firebird-build\output_%FB_OUTPUT_SUFFIX%\* C:\firebird-out
