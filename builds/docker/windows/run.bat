@echo off
docker run --rm -v %cd%\..\..\..:C:\firebird asfernandes/firebird-builder:5 %1
