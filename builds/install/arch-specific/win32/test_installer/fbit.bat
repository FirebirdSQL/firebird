@setlocal enabledelayedexpansion

:: DEBUG is intended for setting echo on globally. Each sub-routine tests
:: for DEBUG and if set VERBOSE is set. As each sub-routine exits VERBOSE
:: unset.
:: Uncomment 'call SET_VERBOSE' where needed to turn on VERBOSE
:: NOTE - DEBUG and VERBOSE still need some work around the use of the @ prefix.
@set DEBUG=
@if not defined DEBUG @echo off

:: Use this to check UAC status
:: - 0x1 means UAC is on and user will be asked for permission
:: - 0x0 means that UAC is off and install/uninstall will run without asking for permission
:: - User with Admin rights is still recommended.
::reg query HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System\ /v EnableLUA | grep REG | cut -d" " -f 13

:: NOTE - This script is not intended as an example of best practice for a
:: scripted install of Firebird. It is designed to test almost all possible
:: scriptable combinations and is thus far more complicated than a typical
:: install script need be. However, it can be used for testing. Note that chosen
:: settings used for each test run are saved into an .inf file, along with a
:: log of the install run.

@goto :MAIN %*
@goto :EOF

::=======================================================
:SET_GLOBAL_ENV
@call :SET_VERBOSE_IF_DEBUG_ON
::@call :SET_VERBOSE
@if defined DEBUG @echo Entering %0

:: Uncomment this if the default command prompt takes up too much space
::@PROMPT=fbit_prompt$G

:: Set this to the location of the firebird installer you want to test.
::@set FBINST_EXEC=%USERPROFILE%\Desktop\Firebird-5.0.0.0000_x64.exe

:: if we have just built firebird we can test the install immediately
if defined FBBUILD_FILE_ID (
	if defined FBBUILD_FILENAME_SUFFIX (
		@set FBINST_EXEC=%FBBUILD_INSTALL_IMAGES%\Firebird-%FBBUILD_FILE_ID%%FBBUILD_FILENAME_SUFFIX%.exe
	) else (
		@set FBINST_EXEC=%FBBUILD_INSTALL_IMAGES%\Firebird-%FBBUILD_FILE_ID%.exe
	)
) else (
rem FBINST_EXEC must point to the package we want to test...
    if not defined FBINST_EXEC (
    rem Set the actual path and filename here - or set it in the environment before running fbit.
    set FBINST_EXEC=%USERPROFILE%\Desktop\Firebird-5.0.0.0000_0_x64_RC1.exe
    )
)

:: This should be set dynamically, perhaps. But for now it is hard-coded.
@set FIREBIRD_BASE_VER=Firebird_5_0

:: It is possible that successive installs into the same directory may
:: generate different uninstallers but for now we hard code the default.
@set UNINSTALLEXE=unins000.exe

:: The log dir should probably be under %TEMP% (or something like that)
:: In any case we try to create it if it doesn't exist
@set FBINSTALLLOGDIR=%USERPROFILE%\fbit-tests\logs

:: We use this dir to store copies of each install for future comparison
:: Perhaps this should be under the User's dir?
@set FBINSTALLCOPYDIR=%USERPROFILE%\fbit-tests\install_archive

:: This is just the default root directory for all versions of firebird
@set FIREBIRDROOTDIR="%ProgramFiles%\Firebird"

:: This is the default click through install dir.
:: It is created by the installer if it does not exist.
@set FIREBIRD=%FIREBIRDROOTDIR%\%FIREBIRD_BASE_VER%

:: Set this to 1 if you want to see the generated final command in the screen
:: output. Innosetup stores it automatically in the log file so this is not
:: necessary in normal use. Displaying it also makes the screen output harder
:: to read.
@set SHOW_FINAL_CMD=

:: change as reqd
@if not defined ISC_PASSWORD (
    @set ISC_PASSWORD="secret"
)

@set TAB=	&

@if not defined DRYRUN (
  if not exist %FBINSTALLLOGDIR% ( @mkdir %FBINSTALLLOGDIR% >nul 2>nul )
  if not exist %FBINSTALLCOPYDIR% ( @mkdir %FBINSTALLCOPYDIR% >nul 2>nul )
)

@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::====SET_GLOBAL_ENV============================================


::=======================================================
:GET_OPTS
@call :SET_VERBOSE_IF_DEBUG_ON
::@call :SET_VERBOSE
@if defined DEBUG @echo Entering %0

:: Automatically parse the commandline and place all valid options into ENV VARS
:: Courtesy of this link:
:: https://stackoverflow.com/questions/3973824/windows-bat-file-optional-argument-parsing
::@setlocal enabledelayedexpansion

:: NOTE 1: These variable names are case insensitive - we can pass CLEAN or
::         clean, for example.
:: NOTE 2: Variables with defaults are impossible to undefine via passing
::         parameters. They must be undefined in code.
:: NOTE 3: Most variables are flags. If a flag has a default it will take the
::         value of the next flag on the command line. For example
::           flag: flagwithdefault:1
::         and this order of params:
::           flagwithdefault flag
::         will end up as
::           flagwithdefault=flag
::         Basically all this means that these variables should not be passed to runtime:
::         INTERACTIVE INSTALL INSTALLTYPE SERVER_INSTALL SERVICE_TASK SUPERSERVER

set "options=APPTASK: CLASSICSERVER: CLEAN: CLIENT: CMD_PARAMS: COMPONENTS: COPYGDSLIB: DEVINST: DRYRUN: FINALCMD: FULL_CMD: FORCE: HELP: INTERACTIVE:1 INSTALL:1 INSTALLTYPE:ServerInstall NOARCHIVE: NOAUTOSTART: NOCANCEL: NOCOPYFBLIB: NOMSG: NOUNINSTALL: PASSWORD: RUN_TIMESTAMP: SCRIPTED: SERVER_INSTALL:1 SILENT: SP: SERVICE_TASK: SUPERCLASSIC: SUPERSERVER: TASK_LIST: TESTNAME:"" UNINSTALL: VERYSILENT: XRESULT:0"
@if defined VERBOSE @echo on
for %%O in (%options%) do (
  for /f "tokens=1,* delims=:" %%A in ("%%O") do (
    set "%%A=%%~B"
  )
)
@if defined VERBOSE (
  @call :PRINT_VARS
  if NOT defined DEBUG pause
)

:loop
@if not "%~1"=="" (
  set "test=!options:*%~1:=! "
  if "!test!"=="!options! " (
      echo Error: Invalid option %~1
  ) else if "!test:~0,1!"==" " (
      set "%~1=1"
  ) else (
      set "%~1=%~2"
      shift
  )
  @if defined VERBOSE (
    @set %1
    if NOT defined DEBUG pause
)
  shift
  goto :loop
)

@if defined VERBOSE ( @call :PRINT_VARS )
@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::=======================================================

::=======================================================
:SET_PARAMS
@call :SET_VERBOSE_IF_DEBUG_ON
@if defined DEBUG @echo Entering %0
::@call :SET_VERBOSE

:: Current possible install options (FB4 RC1)
:: [Types] - ServerInstall DeveloperInstall ClientInstall CustomInstall
:: [Components] - ServerComponent DevAdminComponent ClientComponent
:: [Tasks] - UseClassicServerTask UseSuperClassicTask UseSuperClassicTask\UseGuardianTask ^
::  UseSuperServerTask UseSuperServerTask\UseGuardianTask UseApplicationTask UseServiceTask ^
::  AutoStartTask CopyFbClientToSysTask CopyFbClientAsGds32Task

:: Defaults are
:: SetupType=serverinstall
:: Components=servercomponent,devadmincomponent,clientcomponent
:: Tasks=usesuperservertask,useservicetask,autostarttask,copyfbclienttosystask

:: InnoSetup can use MERGETASKS to add non-exclusive tasks to the default list.
:: For example:
::   MERGETASKS="UseSuperServerTask\UseGuardianTask,CopyFbClientAsGds32Task"
:: UNFORTUNATELY we can't negate tasks with ! if delayed variable expansion is
:: used so this option is not very useful to us.
::
:: Instead we use TASKS to entirely redefine the task list. For example:
::   TASKS="UseClassicServerTask,UseApplicationTask,CopyFbClientAsGds32Task"

:: There are other things we could test, maybe one day. See the installer help
:: dialogue or innosetup help for setup commandline params.

:: FBIT uses SCRIPTED to automatically set /SP- /VERYSILENT /SUPPRESSMSGBOXES
:: INTERACTIVE and SCRIPTED are incompatible. INTERACTIVE is default but the
:: variable is never tested.


@if defined SCRIPTED (
    set VERYSILENT=VERYSILENT
    set SP=SP-
    set NOMSG=SUPPRESSMSGBOXES
    set INTERACTIVE=
)

@if defined UNINSTALL (
    set INSTALL=
    @if defined CLEAN ( set CMD_PARAMS=!CMD_PARAMS! /CLEAN )
rem We now have everything we need for uninstall so jump to the end
    goto :SET_CMD_PARAMS
)

:: Fix up any incompatible assignments :::::::::::::::::::::::::::::::::::::::::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

@if defined CLIENT (
    set INSTALLTYPE=ClientInstall
    set DEVINST=
    set SERVER_INSTALL=
    set TASK_LIST=
    set SERVICE_TASK=
    set CLASSICSERVER=
    set SUPERCLASSIC=
    set SUPERSERVER=

)

@if defined DEVINST (
    set INSTALLTYPE=DeveloperInstall
    set CLIENT=
    set SERVER_INSTALL=
    set TASK_LIST=
    set SERVICE_TASK=
    set CLASSICSERVER=
    set SUPERCLASSIC=
    set SUPERSERVER=
)

:: Theoretically this next line is redundant
@if defined SERVER_INSTALL (

    set INSTALLTYPE=ServerInstall
    set CLIENT=
    set DEVINST=

    @if defined CLASSICSERVER (
        set SUPERSERVER=
        set SUPERCLASSIC=
        set TASK_LIST=UseClassicServerTask
    ) else (
        @if defined SUPERCLASSIC (
            set SUPERSERVER=
            set CLASSICSERVER=
            set TASK_LIST=UseSuperClassicTask
        ) else (
            rem @if defined SUPERSERVER (
            set SUPERCLASSIC=
            set CLASSICSERVER=
            set SUPERSERVER=1
            set TASK_LIST=UseSuperServerTask
        )
    )
)
::@call :SET_VERBOSE
@call :PRINT_VARS In %0 - End of Fixup
:::::::::: End Fix Up incompatible assignments :::::::::::::::::::::::::::::::::


:: Now build our task list

:: At this stage, if TASK_LIST is not defined then we are not doing a server install
@if defined TASK_LIST (
    if defined APPTASK (
        set TASK_LIST=!TASK_LIST!,UseApplicationTask
        set INSTALLTYPE=CustomInstall
    ) else (
        set TASK_LIST=!TASK_LIST!,UseServiceTask
    )

    if NOT defined NOAUTOSTART (
        set TASK_LIST=!TASK_LIST!,AutoStartTask
        set INSTALLTYPE=CustomInstall
    )
)


@if NOT defined NOCOPYFBLIB (
    if not defined TASK_LIST (
        set TASK_LIST=CopyFbClientToSysTask
    ) else (
        set TASK_LIST=!TASK_LIST!,CopyFbClientToSysTask
    )
  set INSTALLTYPE=CustomInstall
)


@if defined COPYGDSLIB (
    if not defined TASK_LIST (
        set TASK_LIST=CopyFbClientAsGds32Task
    ) else (
        set TASK_LIST=!TASK_LIST!,CopyFbClientAsGds32Task
    )
  set INSTALLTYPE=CustomInstall
)
@call :PRINT_VARS In %0 - End of set TASK_LIST


:SET_CMD_PARAMS
:: set up the CMD_PARAMS variable we will use

@if defined FORCE (set CMD_PARAMS=!CMD_PARAMS! /FORCE )

@if defined NOCANCEL (set CMD_PARAMS=!CMD_PARAMS! /NOCANCEL )

:: Setting PASSWORD is only relevant for a server install
@if defined PASSWORD (
    if defined SERVER_INSTALL (
        set SYSDBAPASSWORD=%ISC_PASSWORD%
        set CMD_PARAMS=!CMD_PARAMS! /SYSDBAPASSWORD=%SYSDBAPASSWORD%
        set INSTALLTYPE=CustomInstall
    )
)

@if defined NOMSG set CMD_PARAMS=!CMD_PARAMS! /SUPPRESSMSGBOXES
@if defined SILENT set CMD_PARAMS=!CMD_PARAMS! /SILENT
@if defined SP set CMD_PARAMS=!CMD_PARAMS! /SP-
@if defined VERYSILENT set CMD_PARAMS=!CMD_PARAMS! /VERYSILENT

:: Setting CustomInstall clears the default COMPONENTS list so we
:: must define it manually
::echo INSTALLTYPE %INSTALLTYPE%
@if /I "%INSTALLTYPE%" == "CustomInstall" (
    if defined CLIENT ( set COMPONENTS=ClientComponent)
    if defined DEVINST ( set COMPONENTS=DevAdminComponent,ClientComponent)
    if defined SERVER_INSTALL ( set COMPONENTS=ServerComponent,DevAdminComponent,ClientComponent)
) else (
    set COMPONENTS=ServerComponent,DevAdminComponent,ClientComponent
)

@if defined INSTALL (
    if defined TASK_LIST (
        set FULL_CMD=/TYPE=!INSTALLTYPE! /TASKS="!TASK_LIST!" /COMPONENTS="!COMPONENTS!" !CMD_PARAMS!
    ) else (
        set FULL_CMD=/TYPE=!INSTALLTYPE! /COMPONENTS="!COMPONENTS!" !CMD_PARAMS!
    )
) else (
    set FULL_CMD=!CMD_PARAMS!
)
@call :PRINT_VARS In %0 - After setting COMPONENTS and FULL_CMD

@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::===SET_PARAMS==========================================

::=======================================================
:PRINT_VARS
:: if a variable is not defined we don't print it, except for critical
:: variables such as FINALCMD that MUST be defined.
@if not defined VERBOSE goto :EOF
@if not "%~1" == "" ( echo %* )
@echo Variables set during script execution are:
@set ADIRNAME 2>nul
@set APPTASK 2>nul
@set CLASSICSERVER 2>nul
@set CLEAN 2>nul
@set CLIENT 2>nul
@set CMD_PARAMS 2>nul
@set COMPONENTS 2>nul
@set COPYGDSLIB 2>nul
@set DEVINST 2>nul
@set DRYRUN 2>nul
@set FBINST_EXEC 2>nul
@set FIREBIRD 2>nul
@set FINALCMD 2>nul
@set FULL_CMD 2>nul
@set FORCE 2>nul
@set INTERACTIVE 2>nul
@set INSTALL 2>nul
@set ISC_USER 2>nul
@set ISC_PASSWORD 2>nul
@set MERGE_TASKS 2>nul
@set NOARCHIVE= 2>nul
@set NOAUTOSTART 2>nul
@set NOCANCEL 2>nul
@set NOCOPYFBLIB 2>nul
@set NOMSG 2>nul
@set NOUNINSTALL 2>nul
@set PASSWORD 2>nul
@set RUN_TIMESTAMP 2>nul
@set SCRIPTED 2>nul
@set SERVER_INSTALL 2>nul
@set SERVICE_TASK 2>nul
@set SILENT 2>nul
@set SP 2>nul
@set SUPERCLASSIC 2>nul
@set SUPERSERVER 2>nul
@set TASK_LIST 2>nul
@set TESTNAME 2>nul
@set UNINSTALL 2>nul
@set VERYSILENT 2>nul
@echo.
@if NOT defined DEBUG pause
@goto :EOF
::=======================================================

::=======================================================
:RUN_INSTALLER
@echo.
@echo.
@call :SET_VERBOSE_IF_DEBUG_ON
@if defined DEBUG @echo Entering %~0 %*
@call :CHECK_ENV
@if defined _err ( goto :EOF)

@call :GET_OPTS %*
@call :SET_PARAMS %*
::@call :SET_VERBOSE
@if defined VERBOSE @echo FULL_CMD is %FULL_CMD%
@if defined VERBOSE ( if NOT defined DEBUG pause )
@call :TIMESTAMP
@if defined DEBUG echo After call TIMESTAMP
@set RUN_TIMESTAMP=%TIMESTAMP%
@set INSTALL_TIMESTAMP=%TIMESTAMP%
@if defined DEBUG echo Before set FINALCMD
@set FINALCMD=%FBINST_EXEC% %FULL_CMD% /DIR=%FIREBIRD% /LOG=%FBINSTALLLOGDIR%\install%RUN_TIMESTAMP%.log /SAVEINF=%FBINSTALLLOGDIR%\install%RUN_TIMESTAMP%-saved.inf
@if defined DEBUG echo After set FINALCMD

@if defined DRYRUN (
    @echo DRYRUN - Not executing call %FINALCMD%
) else (
    @if defined DEBUG @echo DRYRUN not set
    @if defined SHOW_FINAL_CMD (@echo Executing %FINALCMD%)
    @call %FINALCMD%

    @if ERRORLEVEL 1 (
    rem  @echo Calling %FBINST_EXEC% failed with %ERRORLEVEL%
        set _err=%ERRORLEVEL%
        @call :ISS_ERROR  %_err% %FBINST_EXEC% %FULL_CMD%
        set /A XRESULT+=1
        @goto :EOF
    ) else (
        @echo Calling %FBINST_EXEC%......................SUCCESS!
    )
    @echo.
    @echo Now checking system state...

    @if defined SERVER_INSTALL (
        call :CHECKSERVICECONFIGURED
        call :CHECKSERVICEINSTALLED
    )
    @if not defined NOCOPYFBLIB (
        call :CHECKFILEEXISTS c:\windows\system32\fbclient.dll good bad err_is_fail
    ) else (
        call :CHECKFILEEXISTS c:\windows\system32\fbclient.dll bad good no_err_is_fail
    )

    @if not defined COPYGDSLIB (
        call :CHECKFILEEXISTS c:\windows\system32\gds32.dll bad good no_err_is_fail
    ) else (
        call :CHECKFILEEXISTS c:\windows\system32\gds32.dll good bad err_is_fail
    )
    @echo Calling COPY_INSTALL
    @call :COPY_INSTALL
    @echo.
)
@echo.
@echo %0 completed with %XRESULT% errors
@set XRESULT=0

@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@echo.
@echo.
@goto :EOF
::===RUN_INSTALLER=================================


::=========================================================
:RUN_UNINSTALLER
@echo.
@echo.
@call :SET_VERBOSE_IF_DEBUG_ON
::@call :SET_VERBOSE
@if defined DEBUG @echo Entering %0 %*
@if defined NOUNINSTALL ( echo NOUNINSTALL set. Not running uninstaller & exit /b 1)

::@call :RESET_INSTALL_ENV
@call :GET_OPTS %* UNINSTALL
@call :SET_PARAMS
::@call :SET_VERBOSE
@if defined VERBOSE @echo on
@if defined VERBOSE @call :PRINT_VARS
@if defined VERBOSE @echo FULL_CMD is %FULL_CMD%
@if defined NOUNINSTALL (
  @echo NOUNINSTALL was passed. Exiting %0.
  @goto :EOF
)
@call :TIMESTAMP
@set RUN_TIMESTAMP=%TIMESTAMP%
@set FINALCMD=%FIREBIRD%\%UNINSTALLEXE% %FULL_CMD% /log=%FBINSTALLLOGDIR%\uninstall%RUN_TIMESTAMP%.log
@if defined VERBOSE ( if NOT defined DEBUG (pause) )
@if defined DRYRUN (
    echo DRYRUN - Not executing call %FINALCMD%
    @if defined DEBUG @echo Leaving %0
    @call :UNSET_VERBOSE
    @echo.
    @echo.
    goto :EOF
)
    @if defined SHOW_FINAL_CMD @echo Executing %FINALCMD%
    @call %FINALCMD% 2>nul
    if errorlevel 1 (
        set _err=%ERRORLEVEL%
    ) else (
        set _err=0
    )
    if %_err% GEQ 1 (
        set _err=%ERRORLEVEL%
        @call :ISS_ERROR  %_err% %UNINSTALLEXE% %FULL_CMD%
        set /A XRESULT+=1
    ) else (
        echo Calling %FIREBIRD%\%UNINSTALLEXE% ................SUCCESS!
    )

    rem We need to give time to the uninstaller to clean up
    rem If the install is interactive we need to close the final msg box first
    rem If the install is scripted we need to pause a few seconds.
    rem Change as required for your system.
    if INTERACTIVE equ 1 (
        echo Close the uninstaller dialog now!!!
        timeout /t 10
    ) else (
        timeout /t 3
    )

    echo.
    echo Now checking system state...
    @call :CHECKFILEEXISTS c:\windows\system32\fbclient.dll bad good no_err_is_fail
    @call :CHECKFILEEXISTS c:\windows\system32\gds32.dll bad good no_err_is_fail

    if defined CLEAN (
        @call :CHECKSHAREDDLLS
        @call :CHECKFILEEXISTS %FIREBIRD% bad good no_err_is_fail
    )
    echo.
    @call :COPY_INSTALL
    echo.
)
echo.

echo %0 completed with %XRESULT% errors
set XRESULT=0
@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@echo.
@echo.
@goto :EOF
::====CLICK_THROUGH_UNINSTALL========================


::=====================================
:CHECKFILEEXISTS
@if defined DEBUG @echo Entering %0
@call :SET_VERBOSE_IF_DEBUG_ON

:: DIR returns an error if file not found and zero if file is returned so we
:: have to turn things around a bit if we want to test for a file that SHOULD
:: be removed. In that case no error is a bad sign!
::
:: This sub-routine takes four params
:: - %1 filename or dirname to verify
:: - %2 - string for no error from dir. ie if we expect the file to exist
::        then pass GOOD. If we don't expect it then we pass BAD.
:: - %3 - string to output if DIR throws an error
:: - %4 - flag to indicate if 0 is an error or not
::@call :SET_VERBOSE
@if defined VERBOSE @echo on
@if defined VERBOSE @echo %*
dir %1 >nul 2>nul
@if errorlevel 1 (
  set _err=%ERRORLEVEL%
) else (
  set _err=0
)
@if %_err% EQU 0 (
  @echo %TAB% %1 exists - %2 !
) else (
  @echo %TAB% %1 not found - %3 !
)

@if "%4"=="err_is_fail" (
  if %_err% GTR 0 (
    set /A XRESULT+=1
    @echo XRESULT++
  )
)
@if "%4"=="no_err_is_fail" (
  if %_err% EQU 0 (
    set /A XRESULT+=1
    @echo XRESULT++
  )
)

@call :RESET_ERRORLEVEL
@call :UNSET_VERBOSE
@if defined DEBUG @echo Leaving %0
@goto :EOF
::===CHECKFILEEXISTS==================================


::=====================================
:CHECKSHAREDDLLS
@if defined DEBUG @echo Entering %0
@call :SET_VERBOSE_IF_DEBUG_ON
@reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\SharedDLLs > %TEMP%\shareddlls.txt
::@grep --ignore-case --count firebird %TEMP%\shareddlls.txt > %TEMP%\shareddllscount.txt
type %TEMP%\shareddlls.txt | find /C /I "firebird" > %TEMP%\shareddllscount.txt
set /p SHAREDDLLSCOUNT= < %TEMP%\shareddllscount.txt
@if NOT defined DEBUG del /q %TEMP%\shareddll*.txt
@if %SHAREDDLLSCOUNT% GTR 0 (
    @echo %TAB% Oops - residue in HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\SharedDLLs
    set /A XRESULT+=1
    @echo XRESULT++
)
@call :RESET_ERRORLEVEL
@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::===CHECKSHAREDDLLS===================


::=====================================
:CHECKSERVICECONFIGURED
@if defined DEBUG @echo Entering %0
@call :SET_VERBOSE_IF_DEBUG_ON

:: Add test for server arch set in firebird.conf

  if defined CLASSICSERVER ( set STR_TO_TEST="servermode = classic" )
  if defined SUPERCLASSIC ( set STR_TO_TEST="servermode = superclassic" )
  if defined SUPERSERVER ( set STR_TO_TEST="servermode = super" )

  call :CHECKSTRING %STR_TO_TEST% %FIREBIRD%\\firebird.conf
  if ERRORLEVEL 1 (
    @echo %TAB% %STR_TO_TEST% not set in %FIREBIRD%\\firebird.conf
    set /A XRESULT+=1
    @echo XRESULT++
  )

@call :RESET_ERRORLEVEL
@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::===CHECKSERVICECONFIGURED===================



::=====================================
:CHECKSERVICEINSTALLED
@if defined DEBUG @echo Entering %0
@call :SET_VERBOSE_IF_DEBUG_ON

  %FIREBIRD%\\instsvc.exe q


@call :RESET_ERRORLEVEL
@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::===CHECKSERVICEINSTALLED===================


::=====================================
:COPY_INSTALL
@call :SET_VERBOSE_IF_DEBUG_ON
@if defined DEBUG @echo Entering %0
::@call :SET_VERBOSE
@if defined VERBOSE @echo on

:: ADIRNAME should normally be set during install and persist for uninstall
@if not defined ADIRNAME (
    @if defined INSTALL_TIMESTAMP (
        @if defined TESTNAME (
            @set ADIRNAME=%FBINSTALLCOPYDIR%\%FIREBIRD_BASE_VER%_%INSTALLTYPE%_%TESTNAME%_%INSTALL_TIMESTAMP%
        ) else (
            @set ADIRNAME=%FBINSTALLCOPYDIR%\%FIREBIRD_BASE_VER%_%INSTALLTYPE%_%INSTALL_TIMESTAMP%
        )
    ) else (
        @set ADIRNAME=%FBINSTALLCOPYDIR%\%FIREBIRD_BASE_VER%_Uninstall_%RUN_TIMESTAMP%
    )
)
@if defined verbose @echo ADIRNAME is %ADIRNAME%
@mkdir %ADIRNAME% 2>nul

@if defined INSTALL (
    @if not defined NOARCHIVE (
        @echo %TAB% Copying Install to %ADIRNAME%
        @xcopy /e /i /y %FIREBIRD% %ADIRNAME%  > nul
        @if errorlevel 1 (
            @echo ERROR Failure executing xcopy /e /i /y %FIREBIRD% %ADIRNAME%
        )
    )
    @copy %FBINSTALLLOGDIR%\install%RUN_TIMESTAMP%-saved.inf %ADIRNAME%\install-saved.inf > nul
    @copy %FBINSTALLLOGDIR%\install%RUN_TIMESTAMP%.log %ADIRNAME%\install.log > nul
) else (
    @echo %TAB% Copying uninstall log to %ADIRNAME%
    @copy %FBINSTALLLOGDIR%\uninstall%RUN_TIMESTAMP%.log %ADIRNAME%\uninstall.log > nul
)
@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::===COPY_INSTALL======================

::=====================================
:CHECK_ENV
:: TODO - add more checks for the environment declared in SET_GLOBAL_ENV
if not exist %FBINST_EXEC% (
    echo %~0 failed
    echo Cannot find %FBINST_EXEC%
    echo Check the setting of FBINST_EXEC
    set _err=1
    exit /b %_err%
)
goto :EOF
::===CHECK_ENV=========================

::=====================================
:RESET_ERRORLEVEL
:: ERRORLEVEL is an internal variable. We can see its value with
:: 'echo %ERRORLEVEL%' but if we try to reset its value with
:: SET ERRORLEVEL=0 we just create an env var called ERRORLEVEL which will
:: not have the same value as the internal ERRORLEVEL. We have to execute an
:: arbitrary command that cannot fail if we want to really reset the internal
:: variable.
@set _err=
@ver > nul
@goto :EOF
::===RESET_ERRORLEVEL==================

::=====================================
:SET_VERBOSE_IF_DEBUG_ON
:: The script is designed to allow turning VERBOSE on at the sub-routine level
:: If we set DEBUG globally then we should turn on VERBOSE automatically when
:: we enter a sub-routine
@if defined DEBUG (
  @set VERBOSE=1
  @echo on
)
@goto :EOF
::===SET_VERBOSE_IF_DEBUG_ON================


::=====================================
:SET_VERBOSE
@set VERBOSE=1
@echo on
@goto :EOF
::===SET_VERBOSE================


::=====================================
:UNSET_VERBOSE
:: Unset VERBOSE before exiting each sub-routine.
:: and force echo off
@if NOT defined DEBUG (
  @set VERBOSE=
  @echo off
)
@goto :EOF
::===UNSET_VERBOSE================


::=====================================
:HELP
@echo off
@echo.
@more /e /t4 %~d0%~p0fbit-help.txt
@echo.
goto :EOF
::===HELP==============================

::=====================================
:ISS_ERROR
@echo.
@echo InnoSetup ErrorCode %1 from calling %~2 %~3 %~4 %~5 %~6 %~7 %~8 %~9
@echo.
@if "%1"=="1" (
  @echo Setup failed to initialize.
  @echo.
  @goto :EOF
)

@if "%1"=="2" (
  @echo The user clicked Cancel in the wizard before the actual installation
  @echo started, or chose 'No' on the opening 'This will install...' message box.
  @echo.
  @goto :EOF
)

@if "%1" == "3" (
  @echo A fatal error occurred while preparing to move to the next
  @echo installation phase (for example, from displaying the pre-installation
  @echo wizard pages to the actual installation process). This should never
  @echo happen except under the most unusual of circumstances, such as
  @echo running out of memory or Windows resources.
  @echo.
  @goto :EOF
)

@if "%1" == "4" (
  @echo A fatal error occurred during the actual installation process.
  @echo.
  @goto :EOF
)

@if "%1" == "5" (
  @echo The user clicked Cancel during the actual installation process,
  @echo or chose Abort at an Abort-Retry-Ignore box.
  @echo.
  @goto :EOF
)

@if "%1" == "6" (
  @echo The Setup process was forcefully terminated by the debugger
  @echo (Run | Terminate was used in the Compiler IDE).
  @echo.
  @goto :EOF
)

@if "%1" == "7" (
  @echo The Preparing to Install stage determined that Setup cannot proceed
  @echo with installation. (First introduced in Inno Setup 5.4.1.)
  @echo.
  @goto :EOF
)

@if "%1" == "8" (
  @echo The Preparing to Install stage determined that Setup cannot proceed
  @echo with installation, and that the system needs to be restarted in
  @echo order to correct the problem. (First introduced in Inno Setup 5.4.1.)
  @echo.
  @goto :EOF
)
@echo.
@goto :EOF
::======================================

::==============================================================================
:: GENERIC SUPPORT ROUTINES FROM HERE TO END  ==================================
::==============================================================================


::=====================================
:CHECKSTRING
@if defined DEBUG @echo Entering %0
@call :SET_VERBOSE_IF_DEBUG_ON
::@if defined VERBOSE echo %0 - %*
for %%v in ( %* )  do (
  if /I "%%v"=="" ( @goto :EOF )
)
:: === NOTE ====
:: We use the /x flag here for an exact match!
@if defined VERBOSE echo calling findstr /x /c:%1 /i %2
:: findstr /x /c:"servermode = superclassic" /i Firebird_4_0\firebird.conf
findstr /x /c:%1 /i %2

@call :RESET_ERRORLEVEL
@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::===CHECKSTRING===================



::=====================================
:GET_DATE
@if NOT defined DEBUG @echo off
@echo. | date | FIND "(mm" > NUL
@if errorlevel 1 ((set MD=0) & call :ParseDate DD MM) else ((set MD=1) & call :ParseDate MM DD)
@goto :EOF
:ParseDate
@for /f "tokens=1-3 delims=/.- " %%a in ("%DATE%") do (
@set %1=%%a
@set %2=%%b
@set YYYY=%%c
@goto:EOF)
::=====================================

::=====================================
:GET_TIME
@if NOT defined DEBUG @echo off
@for /f "tokens=1-4 delims=:. " %%a in ("%TIME%") do (
@set hh=%%a
@set nn=%%b
@set ss=%%c
@set ms=%%d
@goto :EOF)
::=====================================

::=====================================
:CLEAR_DT_VARS
@if NOT defined DEBUG @echo off
@set MM=
@set DD=
@set YYYY=
@set hh=
@set nn=
@set ss=
@set ms=
@if defined DEBUG @echo Leaving CLEAR_DT_VARS
@goto :EOF
::=====================================

::=====================================
:TIMESTAMP
@call :GET_DATE
@call :GET_TIME
@set TIMESTAMP=%YYYY%%MM%%DD%%hh%%nn%%ss%
@if defined DEBUG (@echo Timestamp set to %TIMESTAMP%)
@call :CLEAR_DT_VARS
@goto :EOF
::=====================================

::==============================================================================
:: MAIN is always the last routine
::==============================================================================

::=====================================
:MAIN
@if defined DEBUG @echo Entering %0 %*
@call :SET_GLOBAL_ENV

@if /I "%1"=="help" (
  @call :HELP
  @goto :EOF
)

:: sometimes it is useful to just tidy up!
@if /I "%1"=="clean" (
  @set NOUNINSTALL=
  call :RUN_UNINSTALLER SCRIPTED CLEAN
  goto :EOF
)

@call :RUN_INSTALLER %*
@if defined _err ( echo _err is %_err% - quitting. && @goto :EOF)

@call :RUN_UNINSTALLER %*


@if defined DEBUG @echo Leaving %0
@call :UNSET_VERBOSE
@goto :EOF
::===MAIN==============================



:: NOTHING BEYOND THIS POINT===========
:EOF
