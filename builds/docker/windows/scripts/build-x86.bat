@echo off

call C:\VisualStudio\Common7\Tools\VsDevCmd.bat -arch=x86

set FB_PROCESSOR_ARCHITECTURE=x86

xcopy /h /e /i /q C:\firebird C:\firebird-build
cd /d C:\firebird-build\builds\win32

call run_all.bat PDB

call run_tests.bat

copy C:\firebird-build\builds\install_images\* C:\firebird\builds\install_images
xcopy /h /e /i /q C:\firebird-build\output_Win32_release C:\firebird\output_Win32_release
