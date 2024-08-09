<#
.SYNOPSIS

fbit - test the firebird binary installer on windows

.DESCRIPTION

fbit {PARAM [...]]

   Parameters may be passed in any order except for one special case:

      fbit -realclean

    will clean up from previous broken installs. -realclean must the first
    parameter and subsequent parameters are ignored. Note: - It will run
    silently and will force cleanup, even if the uninstaller no longer exists.

 By default fbit installs Firebird according to the parameters passed and then immediately
 uninstalls it. A copy of the install is made, along with the install and uninstall logs.

 REQUIREMENTS
 ============

 Some knowledge of InnoSetup will be useful. See $FIREBIRD/doc/installation_scripted.txt
 for more info.

 FBIT Specific Parameters
 ========================
 Param Name      Value Passed to fbit            Comment
 ----------      --------------------            -------
 -help           -                               Displays this screen
 -dryrun         -                               Show what will be done. No changes are made
 -noarchive      -                               Disables copying of install dir to %USERPROFILE%\fbit
                                                 Logs and inf files are always saved.
 -nouninstall    -                               Disable automatic uninstall for this test run
 -scripted       -                               Sets VERYSILENT, SP and nomsg
 -testname       NameOfTestRun                   Optional. No spaces allowed. Used for storing test run details.

 -fbinst_exec    Path and filename of Installer  Will be dynamically determined if not set.

 -config         A file name                     Pass parameters in a config file.
                                                 Overrides params passed on the command-line.

 The following parameters are set by default. They are unset
 automatically when a conflicting parameter is passed:

 DefaultParam    Default Value set by fbit       Unset by
 -------------   -------------------------       ----------
 -interactive    True                            -scripted
 -installtype    ServerInstall                   -client or -devinst
 -service_task   True                            -apptask
 -superserver    True                            -classicserver or -superclassic

 Firebird Installer specific Parameters
 ======================================
 Param Name      Value passed to installer       Action when set
 ----------      -------------------------       ---------------
 -copygdslib     CopyFbClientAsGds32Task         Copy fbclient to <SYS> and rename to gds32
 -force          FORCE                           Force installation
 -noautostart    -                               Does not set AutoStartTask
 -nocopyfblib    -                               Does not copy fbclient to <SYS>
 -password       /SYSDBAPASSWORD=%PASSWORD%      Changes SYSDBA password from masterkey
                                                 If -password is not passed on the command-line and
                                                 ISC_PASSWORD is set then ISC_PASSWORD will be used.
 -legacy_auth    EnableLegacyClientAuth          Adds support for legacy authentication

 Installation Tasks
 ==================
 Param Name      Value passed to /TASKS          Comment
 -------------   ----------------------          ---------------
 -apptask        UseApplicationTask              Will not install as a service
 -classicserver  UseClassicServerTask            Will configure classic server
 -superclassic   UseSuperClassicTask             Will configure super classic

 Installation Types
 ==================
 Param Name      Value passed to /TYPE           Comment
 ------------    ---------------------           -------
 -client         ClientInstall                   Minimal working client install
 -devinst        DeveloperInstall                Everything but the server.
 -server_install ServerInstall

 Uninstallation
 ==============
 Param Name      Value passed to uninstaller     Comment
 --------------  ---------------------------     -------
 -clean          CLEAN                           Completely remove an existing firebird install.
                                                 Assumes installed version of Firebird matches
                                                 $firebirdrootdir\$firebird_base_ver set in fbit script.

 -realclean                                      Calls uninstaller if it exists.
                                                 Deletes the $firebirdrootdir\$firebird_base_ver dir
                                                 Removes the list of firebird shared dll's in the registry.
                                                 BEWARE - this should only be used when normal CLEAN is no
                                                 longer working correctly.

 -realclean -force                               Do not prompt during clean up. (Currently not implemented.)

 Generic InnoSetup parameters
 ============================
 Param Name      Value passed to installer       Comment
 ----------      -------------------------       -------
 -nomsg          SUPPRESSMSGBOXES                Suppress message boxes
 -nocancel       NOCANCEL                        Prevents user cancelling install
 -silent         SILENT
 -sp             SP-                             Disables the 'This will install...' prompt
 -verysilent     VERYSILENT


.PARAMETER help
Outputs this help screen and quits.

.INPUTS

None.

.OUTPUTS

None.

.EXAMPLE

   fbit -scripted

(Run a scripted server install followed immediately by a scripted uninstall.
 Adding -verbose is recommended.)

.EXAMPLE

  fbit -realclean

  (Clean up previous firebird install)

.EXAMPLE

  fbit -scripted -client

(Test scripted install of firebird client)

#>


[CmdletBinding(DefaultParameterSetName = "StandardParams" )]
param(

  # Load a configuration file
  [string]$config,

  [Parameter(Mandatory = $true, ParameterSetName = "classic")]
  [switch]$classicserver,

  [Parameter(Mandatory = $true, ParameterSetName = "superclassic")]
  [switch]$superclassic,

  # is the default. No need to set.
  [switch]$superserver,
  # use scripted install instead of default interactive
  [switch]$scripted,
  # uninstall an existing install. (install is the default.)
  [switch]$uninstall,
  # Ignore errors as far as possible. Used for -realclean
  [switch]$force,
  # install firebird. This is the default.
  [switch]$install,
  # Remove as many traces of Firebird as possible from the system.
  [switch]$realclean,
  # Print generated command to screen before execution.
  [switch]$show_final_command,
  # Just install. Default behaviour is to install and uninstall.
  [switch]$nouninstall,
  # verify current install
  [switch]$check_install,
  # verify current install
  [switch]$check_uninstall,

  # parameters specific to this script
  [string]$inno_params_from_cmdline,
  [string]$fbinst_exec,
  [string]$testname = "fbit_test",

  # ### Params to control InnoSetup

  [switch]$apptask,
  [switch]$clean,
  [switch]$client_install,
  [string]$components = "",
  [switch]$copygdslib,
  [switch]$dev_install,
  [switch]$noarchive,
  [switch]$noautostart,
  [switch]$nocancel,
  [switch]$nocopyfblib,
  [switch]$nomsg,
  [string]$password = "masterkey",
  [switch]$server_install,
  [switch]$silent,
  [switch]$sp,
  # install Firebird as a service. This is the default. See also apptask.
  [switch]$service_task,
  [string]$task_list = "",
  [switch]$verysilent,

  # General parameters. Do not add anything after help

  [switch]$dryrun,
  [switch]$help



)


# Vars available to all functions in script
$TAB = '  '
$ErrorCount = 0
[string]$action = ""
# Full Install/Uninstall command that will be executed
[string]$finalcmd
# The actual list of commands that will be passed to the installer/uninstaller
[string]$full_cmdline = ""
# Override timestamp
[string]$run_timestamp

[boolean]$IsVerbose = $false
[boolean]$IsDebug = $false

[string]$uninst_cmdline = ""
[string]$inno_silent = ""


[System.ConsoleColor]$fgcol = "Black"
[System.ConsoleColor]$fgred = "Red"
[System.ConsoleColor]$fggreen = "Green"

Import-Module .\fbit-functions.psm1

<#
 TO DO list

#>


<#
.SYNOPSIS
Load a simple config file and assign key-value pairs to $key variables

.DESCRIPTION


.EXAMPLE
# Comment lines may start with # or //
# Variables are strings by default
avar="AValue"
# Booleans must be passed as 0 or 1 and are converted to boolean types
aboolean=$true
# Switches may be set by passing the switch name prefixed with a hyphen
  -aswitch
# Or just passed 'as is'
  aswitch

.NOTES
General notes
#>
function LoadConfig( [string] $_conffile ) {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"
  Write-Debug "_conffile is $_conffile"
  foreach ($i in $(Get-Content $_conffile)) {
    # Trim any lead/trailing whitespace
    # Include value in quotes if you need trailing whitespace!
    $local:line = $i.trim()
    if ($local:line -ne '') {
      if ( -not ($line.startswith( '#' ) -or $line.startswith( '//' )) ) {
        Write-Verbose "Processing $local:line"

        $local:avalue = $null
        $local:akey = $null

        $local:akey = $i.split('=')[0]
        if ( $null -ne $local:akey ) { $local:akey = $local:akey.trim() }

        # Check if key is a switch.
        if ( $local:akey.StartsWith('-') ) {
          Write-Debug "$local:akey is a switch"
          Set-Variable -Scope global -Name $local:akey.TrimStart('-') -Value $true
          Set-Variable -Scope Script -Name $local:akey.TrimStart('-') -Value $true
          continue
        }

        try {
          $local:avalue = $i.split('=', 2)[1]
        } catch {
          Write-Debug "Setting $local:akey to true"
          Set-Variable -Scope global -Name $local:akey -Value $true
          Set-Variable -Scope Script -Name $local:akey -Value $true
          continue
        }

        if ( $null -ne $local:avalue ) { $local:avalue = $local:avalue.trim() }
        Write-Debug "akey is $local:akey and avalue is $local:avalue"
        # Test avalue and change type to boolean if necessary
        switch ( $local:avalue ) {
          { $_ -eq "1" } {
            Write-Debug "Setting $local:akey to true"
            Set-Variable -Scope Global -Name $local:akey -Value $true
            Set-Variable -Scope Script -Name $local:akey -Value $true
          }
          { $_ -eq "0" } {
            Write-Debug "Setting $local:akey to false"
            Set-Variable -Scope Global -Name $local:akey -Value $false
            Set-Variable -Scope Script -Name $local:akey -Value $false          }
          { $_ -eq $null } {
            Write-Debug "Setting $local:akey to true"
            Set-Variable -Scope Global -Name $local:akey -Value $true
            Set-Variable -Scope Script -Name $local:akey -Value $true
          }
          Default {
            Set-Variable -Scope global -Name $local:akey -Value $local:avalue
            Set-Variable -Scope Script -Name $local:akey -Value $local:avalue
          }
        }
      } else {
        Write-Debug "NOT processing commented $local:line"

      }
    }
  }
#  Set-PSDebug -Off
#  Get-ChildItem Variable:
#  Pause "In $($MyInvocation.MyCommand.Name)  called from line $($MyInvocation.ScriptLineNumber)"
  Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"
}


function print_vars( [string]$_action ) {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"

  $local:varfile = "$fbinstalllogdir/${testname}-${_action}-$run_timestamp.vars"
  if ( $script:IsDebug ) {
    Get-ChildItem Variable:
    Pause "In $($MyInvocation.MyCommand.Name)  called from line $($MyInvocation.ScriptLineNumber)"
  }
  if ( $script:IsVerbose ) {
    spacer > $local:varfile
    if ( check_file_exists "fb_build_vars_${env:PROCESSOR_ARCHITECTURE}.txt" ) {
      spacer "Firebird Build Environment" >> $local:varfile
      Get-Content fb_build_vars_${env:PROCESSOR_ARCHITECTURE}.txt >> $local:varfile
      spacer "Firebird Build Environment END" >> $local:varfile
      spacer >> $local:varfile
    }
    spacer 'Global Vars' >> $local:varfile
    Get-Variable -Scope global >> $local:varfile
    spacer 'Global Vars END' >> $local:varfile
    spacer >> $local:varfile
    spacer 'Env Vars' >> $local:varfile
    env | grep '^FB' >> $local:varfile
    env | grep '^ISC' >> $local:varfile
    spacer 'Env Vars END' >> $local:varfile
    spacer 'Script Vars' >> $local:varfile
    Get-Variable -Scope script | Format-Table -AutoSize -Wrap >> $local:varfile
    spacer 'Script Vars END' >> $local:varfile
    spacer >> $local:varfile
    spacer 'Local Vars' >> $local:varfile
    Get-Variable -Scope local >> $local:varfile
    spacer 'Local Vars END' >> $local:varfile
    spacer >> $local:varfile
  }
  Write-Debug "Leaving $($MyInvocation.MyCommand.Name)"

}

function show-help() {
  Get-Help $PSCommandPath -ShowWindow
}





<#
.SYNOPSIS
Set defaults based on lack of supplied arguments
#>
function check_params() {

  if ( $(Test-Path variable:$script:MyInvocation.BoundParameters['Verbose'] ) ) {
    $script:IsVerbose = $script:MyInvocation.BoundParameters['Verbose'].IsPresent
  }

  if ( $(Test-Path variable:$script:MyInvocation.BoundParameters['Debug'] ) ){
    $script:IsDebug = $script:MyInvocation.BoundParameters['Debug'].IsPresent
  }

  if ( $script:config ) {
    Write-Output "script:config is $script:config"
    LoadConfig $script:config
  } else {
    Write-Output "$($MyInvocation.MyCommand.Name) Not calling script:config $script:config"
  }

  if ( $script:IsDebug ) {
    Get-ChildItem Variable:
    Pause "In $($MyInvocation.MyCommand.Name) called from line $($MyInvocation.ScriptLineNumber ) After testing script:config $script:config"
  }

  # ### fbit related param checks. These control what gets executed. ### #
  # By default the script is designed to install, check install and uninstall.
  # Those defaults are over-ridden by "realclean", "uninstall", and "check_install"

  # If realclean is set then we must try to uninstall/cleanup.
  # fbit will exit after "clean" or "realclean" has finished.
  if ( $script:realclean ) { $script:uninstall = $true }

  # Default to install if uninstall not set. uninstall is run automatically after.
  # Pass nouninstall if the intention is to keep the installation for further testing
  $script:install = !$script:uninstall

  # By default we install unless uninstall has been set.
  # If realclean has been set then we really clean up and then exit.
  # If check_install has been set then we check the install and exit.
  # If check_uninstall has been set then we check the install and exit.
  if ( $script:install ) { $script:action = "install" }
  if ( $script:uninstall ) { $script:action = "uninstall" }
  if ( $script:realclean) { $script:action = "realclean" }
  if ( $script:check_install -and !$realclean ) { $script:action = "check_install" }
  if ( $script:check_uninstall ) { $script:action = "check_uninstall" }


  # ### InnoSetup related param checks ### #

  $script:interactive = !$script:scripted


  # default to superserver if neither of classic nor superclassic are set
  $script:superserver = !($script:classicserver -or $script:superclassic)

  # default to server install if neither of dev nor client install are set
  $script:server_install = !($script:dev_install -or $script:client_install )

  # Install Firebird as a service if $apptask not set
  $script:service_task = !($script:apptask)

  # If ISC_PASSWORD is set and password defaults to masterkey then set password
  # to ISC_PASSWORD.
  # If password != masterkey then we over-ride the value of $env:ISC_PASSWORD
  if ( $env:ISC_PASSWORD -and ($script:password -eq "masterkey" ) ) {
    $script:password = $env:ISC_PASSWORD
  }

  # ### END of InnoSetup related param checks ### #

  if ( $script:IsDebug ) {
    Get-ChildItem Variable:
    Pause "In $($MyInvocation.MyCommand.Name) called from line $($MyInvocation.ScriptLineNumber ) Before leaving function."
  }
}



<#
.SYNOPSIS
Check registry for shared dlls

.NOTES
Currently checks for all references to firebirdin the list of sharedDlls.
Perhaps this should be limited to include a firebird major version check.
#>
function check_shared_dlls() {

  $local:shareddlls = (reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\SharedDLLs)
  $local:count = (Select-String -InputObject "$local:shareddlls" -Pattern "firebird" -AllMatches).Matches.Count

  if ( $local:count -gt 0 ) {
    $script:ErrorCount += 1
    Write-Output "$script:TAB Shared firebird dlls found in HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\SharedDLLs"
  } else {
    Write-Output "$script:TAB No shared firebird dlls found in registry"
  }

}


<#
.SYNOPSIS
Make a copy of the install
#>
function copy_install() { # ( [string]$_sourceFolder, [string]$_targetFolder ) {

  $_source_folder = $script:FIREBIRD
  $_target_folder = $script:copy_install_target

  Write-Verbose "Saving Firebird Installation to $_target_folder"

  if ( ! ( Test-Path -Path $_target_folder ) ) { mkdir $_target_folder }
  Copy-Item -Path $_source_folder\* -Destination $_target_folder -Recurse -Force | Out-Null
  if ( Test-Path $script:install_log_file -PathType leaf ) {
    Copy-Item -Path $script:install_log_file -Destination $_target_folder -Force | Out-Null
  }
  if ( Test-Path $script:install_inf_file -PathType leaf ) {
    Copy-Item -Path $script:install_inf_file -Destination $_target_folder -Force | Out-Null
  }

}

<#
.SYNOPSIS
Check that the sec db has been initialised
.DESCRIPTION
Check that the sec db has been initialised.
Also check if masterkey is used for the SYSDBA password.

.NOTES

 if user has specified a password and it is not masterkey then
 verify that
  - the password works
  - that masterkey does not work.


#>
function check_server_sec_db() {

  # Check that the security db exists
  $local:retval = check_file_exists "$script:FIREBIRD\security${script:fbmajver}.fdb"
  if ( !$local:retval ) {
    $script:fgcol = ( $script:action | Select-String -Pattern "un" -NotMatch -Quiet ) ?  $script:fgred : $script:fggreen
    Write-Host -ForegroundColor $script:fgcol "${TAB}$script:FIREBIRD\security${script:fbmajver}.fdb not found."
    return
  }

  # Now check that isql is available
  $local:retval = check_file_exists "$script:FIREBIRD/isql.exe"
  if ( !$local:retval ) {
    $script:fgcol = ( $script:action | Select-String -Pattern "un" -NotMatch -Quiet ) ?  $script:fgred : $script:fggreen
    Write-Host -ForegroundColor $script:fgcol "${TAB}$FIREBIRD\isql.exe does not exist. Cannot check security db"
    return
  }

  #Write-Output "${TAB}Test the password we used during the install..."

  Write-Output "exit;" > $env:Temp\infile.txt

  foreach ( $apw in "$script:password", "masterkey", "masterke" ) {
    Exec_SQL "localhost:employee" "SYSDBA" "$apw"
    if ( $apw -eq "$script:password") {
      $local:found_str_is_fail = $true
    } else {
      $local:found_str_is_fail = $false
    }
    $local:teststr = "Your user name and password are not defined. Ask your database administrator to set up a Firebird login."
    $local:retval = $(check_file_output "$env:Temp\outfile.txt" "$local:teststr")

    print_output "${TAB}Password test with $apw " $local:retval $local:found_str_is_fail "PASSED" "FAILED"

  }

}

<#
.SYNOPSIS
Use the $FIREBIRD variable to check the current firebird installation

.DESCRIPTION
Long description

.EXAMPLE
An example

.NOTES
General notes
#>
function check_firebird_installed() {

  $local:retval = check_file_exists $script:FIREBIRD -_isdir
  if ( ! $local:retval ) {
    $script:fgcol = ( $script:action -eq "check_install" ) ?  $script:fgred : $script:fggreen
    Write-Host -ForegroundColor $script:fgcol "${TAB}$script:FIREBIRD directory does not exist."
    return
  }

  $local:retval = check_file_exists "${script:FIREBIRD}\firebird.exe"
  if ( $local:retval ) {
    $script:fgcol = ( $script:action -eq "check_install" ) ?  $script:fggreen : $script:fgred
    Write-Host -ForegroundColor $script:fgcol "${TAB}Firebird Server appears to be installed."
    $script:server_installed = $true
  } else {
    $script:fgcol = ( $script:action -eq "check_install" ) ? $script:fgred : $script:fggreen
    Write-Host -ForegroundColor $script:fgcol "${TAB}Firebird Server does NOT appear to be installed."
  }


  $local:retval = check_file_exists "$script:FIREBIRD/isql.exe"
  if ( $local:retval ) {
    $script:fgcol = ( $script:action -eq "check_install" ) ?  $script:fggreen : $script:fgred
    Write-Host -ForegroundColor $script:fgcol "${TAB}Firebird Dev Tools appears to be installed."
    $script:devtools_installed = $true
  } else {
    $script:fgcol = ( $script:action -eq "check_install" ) ? $script:fgred : $script:fggreen
    Write-Host -ForegroundColor $script:fgcol "${TAB}Firebird Dev Tools do NOT appear to be installed."
  }
  $local:retval = check_file_exists "$script:FIREBIRD/fbclient.dll"
  if ( $local:retval ) {
    $script:fgcol = ( $script:action -eq "check_install" ) ?  $script:fggreen : $script:fgred
    Write-Host -ForegroundColor $script:fgcol "${TAB}Firebird Client appears to be installed."
    $script:client_installed = $true
  } else {
    $script:fgcol = ( $script:action -eq "check_install" ) ? $script:fgred : $script:fggreen
    Write-Host -ForegroundColor $script:fgcol "${TAB}Firebird Client does NOT appear to be installed."
  }

}


function check_service_installed( [string]$_servicename ) {

  $local:ExpectedServiceName = "FirebirdServerDefaultInstance"

  $local:ActualServiceName = Get-Service -ErrorAction ignore -Name $local:ExpectedServiceName | Select-Object -ExpandProperty Name
  if (check_result $local:ExpectedServiceName $local:ActualServiceName $true ) {
    $script:fgcol = ( $script:action -eq "check_install" ) ?  $script:fggreen : $script:fgred
    Write-Host  -ForegroundColor $script:fgcol "${TAB}$local:ExpectedServiceName is installed. "
  } else {
    $script:fgcol = ( $script:action -eq "check_uninstall" ) ?  $script:fggreen : $script:fgred
    Write-Host  -ForegroundColor $script:fgcol "${TAB}$local:ExpectedServiceName is NOT installed. "
  }
}


function load_fb_build_env() {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"

  # Build_All.bat should generate fb_build_vars_${env:PROCESSOR_ARCHITECTURE}.txt
  $local:fbbuild_vars_file = Get-Content "fb_build_vars_${env:PROCESSOR_ARCHITECTURE}.txt"

  $script:fbbuild_vars = @{}
#  $local:fbbuild_vars_file | Sort-Object -Property key | ForEach-Object {
  $local:fbbuild_vars_file | ForEach-Object {
    $s = $_ -split "="
    $s[1] = $s[1].Trim()
    $script:fbbuild_vars.Add($s[0], $s[1] )
  }
  if ( $script:IsDebug ) {
    Get-ChildItem Variable:
    Pause "In $($MyInvocation.MyCommand.Name) called from line $($MyInvocation.ScriptLineNumber) "
  }

  # Check that we have fbmajver and fix it if not.
  # Normally this should not happen if we have just built firebird
  if ( ! $script:fbbuild_vars.ContainsKey('FB_MAJOR_VER') ) {
    # Try to locate build_no.h
    $local:fb_root_path= $script:fbbuild_vars.'FB_ROOT_PATH'
    if ( Test-Path "${local:fb_root_path}\src\jrd\build_no.h" -PathType Leaf ) {

      foreach ($aline in $(Get-Content ${local:fb_root_path}\src\jrd\build_no.h | grep "#define FB_")) {
        $s = $aline.split(" ")
        $s[2] = $s[2].Trim('"')
        $script:fbbuild_vars.Add($s[1], $s[2] )
      }
      $script:fbmajver = $script:fbbuild_vars.'FB_MAJOR_VER'
      $script:fbpackageNum = $script:fbbuild_vars.ContainsKey('FB_PACKAGE_NUMBER') ? $script:fbbuild_vars.'FB_PACKAGE_NUMBER' : 0

      $script:fbbuild_vars.'FBBUILD_FILE_ID' = $script:fbbuild_vars.'FB_MAJOR_VER' + "." +
                                               $script:fbbuild_vars.'FB_MINOR_VER' + "." +
                                               $script:fbbuild_vars.'FB_REV_NO' + "." +
                                               $script:fbbuild_vars.'FB_BUILD_NO' + "-" +
                                               $script:fbpackageNum + "-" +
                                               $script:fbbuild_vars.'FB_TARGET_PLATFORM'
    }
  }

  $script:fbbuild_vars = [System.Collections.SortedList] $script:fbbuild_vars

  $script:fbbuild_vars.GetEnumerator() | ForEach-Object {
    Write-Verbose "$($_.Key): $($_.Value)"
  }

  Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"

}


<#
.SYNOPSIS
Analyse the installation and environment. Set some variables.
#>
function check_environment() {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"

  # If passed via command line
  if ( $script:fbinst_exec -ne "" ) {
    Write-Debug "Determine build env from $script:fbinst_exec"
#    Set-PSDebug -Trace 2
    $path_file = @{}
    $path_file = $script:fbinst_exec -split "\\", -2
    $path_file = $path_file[1] -split ".exe" , 2
    $script:FirebirdInstallVer = $path_file[0]
    $_astr = $script:FirebirdInstallVer -split "\."
    $_astr = $_astr[0] -split "-"
    $script:fbmajver = $_astr[1]
#    Set-PSDebug -Trace 0
  } else {
    Write-Debug "Determine build env dynamically"
    load_fb_build_env
    switch ( $script:fbmajver ) {
      "3" {
        $local:fb_file_id = $script:fbbuild_vars.'FBBUILD_FILE_ID' -replace "-", "_"
      }
      Default {
        $local:fb_file_id = $script:fbbuild_vars.'FBBUILD_FILE_ID'
      }
    }

    if ( $local:fb_file_id -eq "" ) {
      Write-Output "Unable to set fbinst_exec."
    } else {
      $script:FirebirdInstallVer = "Firebird-${local:fb_file_id}"
      $script:fbinst_exec = -join ($script:fbbuild_vars.'FB_ROOT_PATH', "\builds\install_images\", $script:FirebirdInstallVer , '.exe' )
      Write-Verbose "Setting fbinst_exec to $script:fbinst_exec"
    }

  }
  if ( $script:IsDebug ) {
    Pause "In $($MyInvocation.MyCommand.Name) called from line $($MyInvocation.ScriptLineNumber)"
  }

#  Set-PSDebug -Trace 2
  $script:fbinstalllogdir = "$env:userprofile\fbit-tests\logs"
  $script:fbinstallcopydir = "$env:userprofile\fbit-tests\install_archive"


  #This is just the default root directory for all versions of firebird
  $script:firebirdrootdir = "$env:ProgramFiles\Firebird"
  $script:firebird_base_ver = "Firebird_" + $script:fbmajver + "_0"

  # ### FIXME - allow assigning a non-default dir
  $script:FIREBIRD = "$script:firebirdrootdir\$script:firebird_base_ver"

  $script:uninstallexe = "${script:FIREBIRD}\unins000.exe"

  if ( ! $(Test-Path variable:run_timestamp) ) {
    $script:run_timestamp = runtimestamp
  }

  $script:install_inf_file = "${script:fbinstalllogdir}\${script:testname}-install-$script:FirebirdInstallVer-$script:run_timestamp-saved.inf"
  $script:install_log_file = "$script:fbinstalllogdir\${script:testname}-install-$script:FirebirdInstallVer-$script:run_timestamp.log"
  $script:uninstall_log_file = "$script:fbinstalllogdir\${script:testname}-uninstall-$script:FirebirdInstallVer-$script:run_timestamp.log"
  $script:copy_install_target = "$script:fbinstallcopydir\$script:testname-install-$script:FirebirdInstallVer-$script:run_timestamp"

  $script:boiler_plate_install = " /DIR=`"$script:FIREBIRD`" /LOG=`"$script:install_log_file`" /SAVEINF=`"$script:install_inf_file`" "
  $script:boiler_plate_uninstall = " /LOG=`"$script:uninstall_log_file`" "

  if ( $script:IsDebug ) {
    # Set-PSDebug -Trace 0
    Get-ChildItem Variable:
    Pause "In $($MyInvocation.MyCommand.Name)  called from line $($MyInvocation.ScriptLineNumber)"
  }
  Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"

}


function check_innosetup_params() {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"

  if ( ! $script:dryrun ) {
    $local:patharray = "$script:fbinstalllogdir", "$script:fbinstallcopydir"
    foreach ($apath in $local:patharray) {
      if ( check_file_exists "$apath" $true ) {
        if ($? -eq $false) { mkdir -Path "$apath" }
      }
    }
  }

  if ( $script:scripted ) {
    $script:inno_verysilent = " /VERYSILENT "
    $script:inno_sp = " /SP- "
    $script:inno_nomsg = " /SUPPRESSMSGBOXES "
  }


  if ( $script:uninstall) {
    if ( $script:realclean ) { $script:clean = $true }
  } else {

    # check params for install
    if ( $script:client_install) {
      $script:task_list = ""
      $script:inno_installtype = "ClientInstall"
      $script:inno_devinst = ""
      $script:inno_server_install = ""
      $script:inno_classicserver = ""
      $script:inno_superclassic = ""
      $script:inno_superserver = ""
    }

    if ( $script:dev_install) {
      $script:task_list = ""
      $script:inno_installtype = "DeveloperInstall"
      $script:inno_client = ""
      $script:inno_server_install = ""
      $script:inno_classicserver = ""
      $script:inno_superclassic = ""
      $script:inno_superserver = ""
    }

    # FIXME - CODE REVIEW REQUIRED
    if ( $script:server_install ) {

      $script:inno_installtype = "ServerInstall"
      $script:inno_client = ""
      $script:inno_devinst = ""

      if ( $cscript:lassicserver ) {
        $script:task_list = " UseClassicServerTask "
        $script:inno_classicserver = 1
        $script:inno_superserver = ""
        $script:inno_superclassic = ""
      } else {
        if ( $script:superclassic ) {
          $script:task_list = " UseSuperClassicTask "
          $script:inno_superserver = ""
          $script:inno_classicserver = ""
          $script:inno_superclassic = 1
        } else {
          $script:task_list = " UseSuperServerTask "
          $script:inno_superclassic = ""
          $script:inno_classicserver = ""
          $script:inno_superserver = 1
        }
      }

      $script:inno_sysdbapassword = " /SYSDBAPASSWORD=`"$script:password`" "

    } # end if ( $server_install )

    # Now start building our task list

    # At this stage, if task_list is not defined then we are not doing a server install
    if ( $script:task_list ) {
      if ( $script:apptask ) {
        $script:task_list += ", UseApplicationTask "
        $script:inno_installtype = "CustomInstall"
        $script:inno_service_task = ""
      } else {
        $script:task_list += ", UseServiceTask "
      }

      if ( ! $script:noautostart ) {
        $script:task_list += ", AutoStartTask "
        #           $script:inno_installtype="CustomInstall"
      }

      # ### FIXME - we need to integrate /MERGE_TASKS and use it here
      if ( ! $script:nocopyfblib ) {
        $script:task_list += ", CopyFbClientToSysTask "
        #           $script:inno_installtype="CustomInstall"
      }

      if ( $script:copygdslib ) {
        $script:task_list += ", CopyFbClientAsGds32Task"
        $script:inno_installtype = "CustomInstall"
      }

      if ( ! $(Test-Path variable:legacy_auth)          ) {
        $script:task_list += ", EnableLegacyClientAuth"
        $script:inno_installtype = "CustomInstall"
      }
    }

  } # End check innosetup params for install

  spacer
  Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"
}


function clean_registry() {

  # Loop through HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall
  # and search for "The Firebird Project" and uninstall all ms runtimes


  return
}


function build_inno_cmd() {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"

  # set up the command line variables we will use



  # ### Params shared by install and uninstall actions ###

  if ( $script:force ) {
    $script:inno_params_from_cmdline += " /FORCE "
    $script:uninst_cmdline += " /FORCE "
  }

  if ( $script:inno_nomsg ) {
    $script:inno_params_from_cmdline += $script:inno_nomsg
    $script:uninst_cmdline += $script:inno_nomsg
  }

  if ( $script:inno_sp ) {
    $script:inno_params_from_cmdline += $script:inno_sp
    $script:uninst_cmdline += $script:inno_sp
  }

  if ( $script:inno_verysilent ) {
    $script:inno_params_from_cmdline += $script:inno_verysilent
    $script:uninst_cmdline += $script:inno_verysilent
  }

  # ### Params used exclusively by the uninstall action ###
  if ( $script:clean ) {
    $script:uninst_cmdline += " /CLEAN "
  }

  if ( $script:inno_silent ) { $script:inno_params_from_cmdline += " /SILENT " }

  if ( $script:nocancel ) { $script:inno_params_from_cmdline += " /NOCANCEL " }

  # ### Params used exclusively for install action ###

  if ( $script:install ) {
    # Setting PASSWORD is only relevant for a server install
    if ( $script:password -ne "masterkey" ) {
        $script:inno_params_from_cmdline += " $script:inno_sysdbapassword "
        $script:inno_installtype = "CustomInstall"
    }

    if ( $script:inno_installtype -eq "CustomInstall" ) {

      if ( $script:client_install ) { $script:inno_components = "ClientComponent" }
      if ( $script:dev_install ) { $script:inno_components = "DevAdminComponent,ClientComponent" }
      if ( $script:server_install ) { $script:inno_components = "ServerComponent,DevAdminComponent,ClientComponent" }

    } else {
      $script:inno_components = "ServerComponent,DevAdminComponent,ClientComponent"
    }

    if ( $script:TASK_LIST ) {
      $script:full_cmdline = " /TYPE=`"$script:inno_installtype`" /TASKS=`"$script:task_list`" /COMPONENTS=`"$script:inno_components`" $script:inno_params_from_cmdline "
    } else {
      $script:full_cmdline = "/TYPE=`"$script:inno_installtype`" /COMPONENTS=`"$script:inno_components`" $script:inno_params_from_cmdline "
    }
  }

  # Always add on the boiler plate log and inf output to the command
  $script:full_cmdline += $script:boiler_plate_install
  $script:uninst_cmdline += $script:boiler_plate_uninstall

  Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"

}


function dry_run( [string]$_action, [string]$_exec ) {

  Write-Output "Dry run - not executing $_action with $_exec"

}

function run_check_install() {


  if ( $script:action |  Select-String -Pattern "un" -SimpleMatch -Quiet ) {
    $script:action = "check_uninstall"
    Write-Output "Checking uninstallation..."
  } else {
    $script:action = "check_install"
    Write-Output "Checking installation..."
  }
  # What is the most reliable way?
  check_firebird_installed

  check_service_installed
  check_server_sec_db

}

function run_installer() {
  [OutputType([int])]
  [cmdletbinding()]
  Param()
  begin {
    Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"
    $local:retval = 0
  }

# We use BEGIN..PROCESS..END here so that we can mask the output of copyinstall and
# allow the function to return an integer instean of an object.
  process {
    build_inno_cmd
    print_vars "$script:action"

    if ( ! ( check_file_exists "$script:fbinst_exec" ) ) {
      Write-Error "fbinst_exec does not exist. Quitting."
      $local:retval = 1
      return
    }

    Write-Verbose "Cmdline to execute is $script:fbinst_exec $script:full_cmdline"

    if ( $dryrun ) {
      dry_run $script:action $script:fbinst_exec
    } else {

      $errorActionPreference = "Stop"
      $env:SEE_MASK_NOZONECHECKS = 1

      $local:retval = Invoke-Command "$script:fbinst_exec" "$script:full_cmdline" "Running Firebird Installer"

      if ( $local:retval -ne 0 ) {
        iss_error ( $local:retval )
      } else {
        Write-Verbose "Completed Firebird Installation"

        if ( ! $script:noarchive  ) {
          # Assign output to null here or else returned $local:retval below becomes an object, not an int
          $null = copy_install

        }
      }
    }
  }
  end {
    Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"
    return $local:retval
  }
}


function run_uninstaller() {

  $script:action = "uninstall"
  build_inno_cmd
  print_vars "$script:action"

  Write-Verbose "uninst_cmdline is $script:uninst_cmdline"

  if ( $dryrun ) {
    dry_run $script:action $script:uninstallexe
  } else {

    $local:retval = check_file_exists "$script:uninstallexe"
    if ( ! $local:retval ) {
      Write-Output "$script:uninstallexe does not exist. Quitting."
      Write-Output ""
      return #$local:retval
    }

    $errorActionPreference = "Stop"
    $local:retval = Invoke-Command "$script:uninstallexe" "$script:uninst_cmdline" "UnInstalling Firebird"
    if ( $local:retval -ne 0 ) {
      iss_error ( $local:retval )
    } else {
      Write-Verbose "Firebird should now be uninstalled"

      run_check_install

    }

    # Copy uninstall log to copy_install_target if it exists.
    if ( Test-Path -Path $script:copy_install_target ) {
      if ( Test-Path $script:uninstall_log_file -PathType leaf ) {
        Copy-Item -Path $script:uninstall_log_file -Destination $script:copy_install_target | Out-Null
      }
    }

  }

 # return $local:retval

}
function run_cleanup() {

  if ( $script:dryrun ) {
    Write-Output "Dry run - not executing $($MyInvocation.MyCommand.Name )"
    Write-Output "Not uninstalling Firebird"
    Write-Output "Not deleting the $env:firebirdrootdir\$env:firebird_base_ver dir"
    Write-Output "Not removing the list of firebird shared dll's in the registry."
    Write-Output ""
  } else {
    Write-Verbose "Cleaning up existing Firebird installation"
    run_uninstaller

    if ( $script:realclean ) {
      $local:retval = check_file_exists $script:firebirdrootdir\$script:firebird_base_ver dir $true
      if ( $local:retval ) {
        Write-Verbose "Removing all files in $script:firebirdrootdir\$script:firebird_base_ver dir"
        Remove-Item "$script:firebirdrootdir\$script:firebird_base_ver" -Recurse -Include "*.*" -Confirm
      }
      Write-Verbose "Removing fbclient and gds32 from $env:SystemRoot\System32"
      Remove-Item "$env:SystemRoot\System32" -Include "fbclient.dll", "gds32.dll" -Confirm
      Write-Verbose "Removing fbclient and gds32 from $env:SystemRoot\SysWOW64"
      Remove-Item "$env:SystemRoot\SysWOW64" -Include "fbclient.dll", "gds32.dll" -Confirm

      Write-Verbose "Clean up listing of Shared DLLs in registry"
      $RegKey = "SOFTWARE\Microsoft\Windows\CurrentVersion\SharedDLLs"
      foreach ( $avalue in "C:\Program\*.*", "${script:firebirdrootdir}\${script:firebird_base_ver}\*.*", `
        "*\fbclient.dll", "*\gds32.dll", "*GDS32.DLL" ) {
          Remove-ItemProperty HKLM:$RegKey -Name $avalue -Confirm
      }
    }
    Write-Verbose "Completed cleanup  of Firebird installation"
  }

}


function iss_error( [Int32]$_err_code = 0 ) {

  switch ($_err_code) {
    1 { Write-Output "Setup failed to initialize." }
    2 {
      Write-Output "The user clicked Cancel in the wizard before the actual installation
                    started, or chose 'No' on the opening 'This will install...' message box."
    }
    3 {
      Write-Output "A fatal error occurred while preparing to move to the next
                    installation phase (for example, from displaying the pre-installation
                    wizard pages to the actual installation process). This should never
                    happen except under the most unusual of circumstances, such as
                    running out of memory or Windows resources."
    }
    4 { Write-Output "A fatal error occurred during the actual installation process." }
    5 {
      Write-Output "The user clicked Cancel during the actual installation process,
                    or chose Abort at an Abort-Retry-Ignore box."
    }
    6 {
      Write-Output "The Setup process was forcefully terminated by the debugger
                    (Run | Terminate was used in the Compiler IDE)."
    }
    7 {
      Write-Output "The Preparing to Install stage determined that Setup cannot proceed
                    with installation. (First introduced in Inno Setup 5.4.1.)"
    }
    8 {
      Write-Output "The Preparing to Install stage determined that Setup cannot proceed
                    with installation, and that the system needs to be restarted in
                    order to correct the problem. (First introduced in Inno Setup 5.4.1.)"
    }
    Default {}
  }

}

function main() {
  begin{
#    $script:DebugPreference = 'Continue'
#    $script:VerbosePreference = 'Continue'

    Set-StrictMode -Version 3.0
  }

  process {
    if ( $script:help ) {
      show-help
    } else {
      prompt
      spacer
      check_params
      check_environment
      check_innosetup_params

      switch ($script:action) {
        "realclean" { run_cleanup }
        "check_install" { run_check_install }
        "uninstall" { run_uninstaller }
        "check_uninstall" { run_check_install }
        Default {
          $local:retval = $( run_installer | Select-Object -Last 1 )
          if ( $local:retval -ne 0 ) {
            Write-Error "run_installer returned $local:retval"
            return
          }
          run_check_install
          if ( ! $script:nouninstall ) {
            run_uninstaller
          }
        }
      }
    }
  }

  end{
    spacer " Finished "

    # this attempt to 'reset' the prompt fails :-(
    #prompt -reset

  }
}

main $args

