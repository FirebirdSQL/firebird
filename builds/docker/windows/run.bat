@echo off
docker run --rm -v %cd%\..\..\..:C:\firebird firebirdsql/firebird-builder-windows:fb6-x86-x64-windows-v1 %1
