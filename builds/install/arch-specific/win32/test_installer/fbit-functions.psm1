#region Helper functions #############

function spacer() {
  param ( [string]$message )
  if ( $message ) {
    Write-Output ''
    Write-Output "=================== $message ==================="
    Write-Output ''
  } else {
    Write-Output '------------------------------------------------------------------------------------------'
  }
}


function Pause( [string] $_msg = "Press any key to continue..." ) {
  Set-PSDebug -Trace 2

  $null = Read-Host $_msg
# Write-Host -NoNewline $Message

# 	$Ignore =
# 		16,  # Shift (left or right)
# 		17,  # Ctrl (left or right)
# 		18,  # Alt (left or right)
# 		20,  # Caps lock
# 		91,  # Windows key (left)
# 		92,  # Windows key (right)
# 		93,  # Menu key
# 		144, # Num lock
# 		145, # Scroll lock
# 		166, # Back
# 		167, # Forward
# 		168, # Refresh
# 		169, # Stop
# 		170, # Search
# 		171, # Favorites
# 		172, # Start/Home
# 		173, # Mute
# 		174, # Volume Down
# 		175, # Volume Up
# 		176, # Next Track
# 		177, # Previous Track
# 		178, # Stop Media
# 		179, # Play
# 		180, # Mail
# 		181, # Select Media
# 		182, # Application 1
# 		183  # Application 2

# 	While ($KeyInfo.VirtualKeyCode -Eq $Null -Or $Ignore -Contains $KeyInfo.VirtualKeyCode) {
# 		$KeyInfo = $Host.UI.RawUI.ReadKey("NoEcho, IncludeKeyDown")
# 	}

# 	Write-Host

  Set-PSDebug -Trace 0
}


function RunTimeStamp() {
  Get-Date -UFormat '%Y-%m-%d_%H-%M-%S'
}


<#
.SYNOPSIS
Shorten the prompt

.DESCRIPTION
The path to the test installer script can get too long

.PARAMETER reset
Restore the full path

#>
function prompt( [switch]$reset ) {


  $local:identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $local:principal = [Security.Principal.WindowsPrincipal] $local:identity
  $local:adminRole = [Security.Principal.WindowsBuiltInRole]::Administrator

  $( if (Test-Path variable:/PSDebugContext) { '[DBG]: ' }
    elseif ($principal.IsInRole($adminRole)) { '[ADMIN]: ' }
    else { '' }
  ) + $( if ( $reset -eq $true ) {
      'PS ' + $(Get-Location) + $( if ( $NestedPromptLevel -ge 1 ) { '>>' } ) + '> '
    } else {
      'FBIT ' + '> '
    } )

}


function check_file_exists( [string]$_apath, [switch]$_isdir ) {
  if ( $_isdir ) {
    return ( (Test-Path -Path $_apath ) -ne ""  )
  } else {
    return ( ( Test-Path -Path $_apath -PathType Leaf  ) -ne "" )
  }
}


function Invoke-Command ($_commandPath, $_commandArguments, $_commandTitle, $_outfile ) {
  Try {
    $local:pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $local:pinfo.FileName = "$_commandPath"
    $local:pinfo.RedirectStandardError = $true
    $local:pinfo.RedirectStandardOutput = $true
    $local:pinfo.UseShellExecute = $false
    $local:pinfo.Arguments = "$_commandArguments"
    $global:p = New-Object System.Diagnostics.Process
    $global:p.StartInfo = $pinfo
    $global:p.Start() | Out-Null
    $local:result_object = [pscustomobject]@{
      commandTitle = $_commandTitle
      stdout       = $p.StandardOutput.ReadToEnd()
      stderr       = $p.StandardError.ReadToEnd()
      ExitCode     = $p.ExitCode
    }
    $global:p.WaitForExit()
    if ( "$_outfile" -ne '' ) {
      $local:result_object.stdout > $_outfile
    } else {
      Write-Verbose $local:result_object.stdout
    }
    return $local:result_object.ExitCode
  } catch {
    Write-Output $local:result_object.stderr
    return $local:result_object.ExitCode

  }

}


#endregion end of helper functions #############


<#
.SYNOPSIS
Indicate if the (non-)existence of a file is a good or a bad thing.
.DESCRIPTION
When installing Firebird we expect certain files to exist.
When uninstalling we do not expect files to exist.

.PARAMETER afile
The file to check for

.PARAMETER str_if_true
The string to output if file exists. Defaults to 'good'

.PARAMETER str_if_false
The string to output if the file does not exist

.PARAMETER status_true_is_fail
When installing set status_true_is_fail to false.
When uninstalling set status_true_is_fail to true.

.PARAMETER isdir
Set isdir if testing for a directory.

.EXAMPLE
An example

.NOTES
General notes
#>
function check_file_status( $afile, [boolean]$status_true_is_fail, [boolean]$isdir
  , [string]$str_if_true = 'good', [string]$str_if_false = 'bad'
) {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"
  $local:retval = check_file_exists $afile $isdir
  if ( $local:retval -eq $true ) {
    Write-Output "$TAB $afile exists - $str_if_true"
  } else {
    Write-Output "$TAB $afile not found - $str_if_false"
  }

  if ( $status_true_is_fail -eq $true -and $local:retval -eq $true ) {
    Write-Verbose "$TAB $status_true_is_fail -eq $true -and $local:retval -eq $true "
    $ErrorCount += 1
  }

  if ( $status_true_is_fail -eq $false -and $local:retval -eq $false ) {
    Write-Verbose "$TAB $status_true_is_fail -eq $true -and $local:retval -eq $true "
    $ErrorCount += 1
  }
  Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"
}


function check_server_arch_configured() {
  Write-Debug "Entering function $($MyInvocation.MyCommand.Name)"

  if ( $global:classicserver ) { $local:str_to_test = 'servermode = classic' }
  if ( $global:superclassic ) { $local:str_to_test = 'servermode = superclassic' }
  if ( $global:superserver ) { $local:str_to_test = 'servermode = super' }

  # FIXME What if the fb.conf does not exist?
  $local:found = (Select-String -Path "$FIREBIRD/firebird.conf" -Pattern ^$local:str_to_test)

  if ( ! $local:found.Length -gt 0 ) {
    $ErrorCount += 1
    Write-Verbose $TAB $local:str_to_test not set in $FIREBIRD/firebird.conf

  }

  Write-Debug "Leaving function $($MyInvocation.MyCommand.Name)"
}



<#
.SYNOPSIS
Compare two strings

.DESCRIPTION
Long description

.PARAMETER expected
The result expected

.PARAMETER actual
The actual result

.PARAMETER equals_is_true
Set True if the actual result should equal the expected result
Or Set True if actualt result should not equal the expected result

.NOTES
General notes
#>
function check_result ( [string]$expected, [string]$actual, [boolean]$equals_is_true) {

  #   Write-Verbose "if ( ($expected -eq $actual ) -and $equals_is_true ){ return $true }"
  if ( ( "$expected" -eq "$actual" ) -and $equals_is_true ) { return $true }

  #   Write-Verbose "if ( ($expected -eq $actual ) -and !$equals_is_true ){ return $false }"
  if ( ( "$expected" -eq "$actual" ) -and ! $equals_is_true ) { return $false }

  #   Write-Verbose "if ( ($expected -ne $actual ) -and $equals_is_true ){ return $false }"
  if ( ( "$expected" -ne "$actual" ) -and $equals_is_true ) { return $false }

  #   Write-Verbose "if ( ($expected -ne $actual ) -and !$equals_is_true ){ return $true }"
  if ( ( "$expected" -ne "$actual" ) -and ! $equals_is_true ) { return $true }
}

function check_file_output( $afile, $apattern ) {
  $local:aretval = Select-String -Path $afile -Pattern $apattern -SimpleMatch -Quiet
  return $local:aretval

}

function print_output( $astring, [boolean]$found, [boolean]$found_is_fail
  , [string]$str_if_true = "GOOD", [string]$str_if_false = "BAD" ) {

  # If we find the string (ie result is not empty)
  if ( $found ) {
    if ( $found_is_fail ) {
      Write-Host -ForegroundColor Red "${TAB}$astring  - $str_if_false"
      $global:ErrorCount += 1
    } else {
      Write-Host -ForegroundColor Green "${TAB}$astring - $str_if_true"
    }
    # We did not find the string
  } else {
    if ( $found_is_fail ) {
      Write-Host -ForegroundColor Green "${TAB}$astring - $str_if_true"
    } else {
      Write-Host -ForegroundColor Red "${TAB}$astring - $str_if_false"
      $global:ErrorCount += 1
    }
  }

}



<#
.SYNOPSIS
Execute SQL via isql.exe

.NOTES
This function assumes that the script to execute exists in $env:Temp\infile.txt
and the output will be stored in $env:Temp\outfile.txt
#>
function Exec_SQL( [string]$db = "localhost:employee",
  [string]$username = "sysdba", [string]$pw = "masterkey" ) {

  # Always reset outfile otherwise output is appended.
  Write-Output "" > $env:Temp\outfile.txt

  $local:retval = Invoke-Command "$global:FIREBIRD\isql.exe" " -user $username -password $pw -z `
  -i $env:Temp/infile.txt -o $env:Temp/outfile.txt -m -m2 $db"

}
