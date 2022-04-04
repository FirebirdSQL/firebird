@echo off
docker run --rm -v %cd%\..\..\..:C:\firebird -v %cd%\..\..\..\output:C:\firebird-out asfernandes/firebird-builder:5 %1
