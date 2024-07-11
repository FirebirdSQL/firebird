@echo off

@echo.

@call setenvvar.bat %*
@if errorlevel 1 (goto :END)

@echo Generating parse.cpp and dsql.tab.h

@sed -n "/%%type .*/p" < %FB_ROOT_PATH%\src\dsql\parse.y > types.y
@sed "s/%%type .*//" < %FB_ROOT_PATH%\src\dsql\parse.y > y.y

%FB_ROOT_PATH%\temp\%FB_OBJ_DIR%\btyacc\btyacc -l -d -S %FB_ROOT_PATH%\src\dsql\btyacc_fb.ske y.y 2>y.txt
@if errorlevel 1 (type y.txt && exit /B 1)
@type y.txt

@sed -i "s/#define \([A-Z].*\)/#define TOK_\1/" y_tab.h
@sed -i "s/#define TOK_YY\(.*\)/#define YY\1/" y_tab.h
@sed -n -e "s/.*btyacc: \(.*conflicts.*\)/\1/p" y.txt > %FB_ROOT_PATH%\src\dsql\parse-conflicts.txt

@copy y_tab.h %FB_ROOT_PATH%\src\include\gen\parse.h > nul
@copy y_tab.c %FB_ROOT_PATH%\src\dsql\parse.cpp > nul
@del y.y
@del y.txt
@del types.y
@del y_tab.h
@del y_tab.c
@del sed*

:END
