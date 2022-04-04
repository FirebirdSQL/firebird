## Firebird Binary Installer Test Harness - FBIT


### Quick Start

 * DO NOT run this script in a production environment.
 * Using a snapshotted VM is recommended for initial use.
 * Open a command prompt and execute

   ``fbit.bat HELP``

   to see what parameters you can pass. Or jump to [examples](#Examples)


### Introduction

The executable Firebird Binary Installer is designed to be as simple as possible, allowing users of any level to more or less click through to have a working install. Options are kept as simple as possible so that the installation will work, no matter which options have been chosen. In addition the installer can be run from a batch script. This enables developers to embed it within their own deployment application.

Over the years bug reports have been made in the tracker but these are often untestable.

There have also been feature requests made and most have not made it into the installer. This is for two reasons:

  * More options can be visually confusing during an interactive install, and certainly make a click through install more difficult for users as they are forced to make decision even though they may know nothing about Firebird.

  * More options means more installation configurations are possible and there has not been a reliable method of testing these combinations

Currently this test harness consists of a single script. It is hoped that using it will make it easier to add new features as well as verify and reliably reproduce problems so that they can be fixed. Other scripts are planned for addition at a later date.


#### Intended Audience

This test harness is for anyone who wishes to integrate a scripted install of Firebird into their own installer or deployment program.

It is also intended to help developers to create reproduceable bug reports if they do experience installation problems.

##### WARNING

> This script is not intended for use in a production environment.
> IT IS DELIBERATELY DESTRUCTIVE AS IT WILL DELETE THE INSTALLED VERSION OF FIREBIRD ON COMPLETION.

However it only installs and uninstalls Firebird using the features of the installer. It does not do anything that can not already be done with the installer itself.

It should also be noted that this script is not intended to be an example of best practice as far as creating a scripted install of Firebird is concerned. The script is way too complicated for that as it needs to support as many possible installation configurations as possible.


#### Assumptions

The script has only been tested on Windows 10 20H2 as an Administrator with UAC turned off.

The script can be run multiple times with different configurations. It is intended that the results of different install configurations can be compared with a tool such as BeyondCompare.

It is preferable that Firebird is not already installed. However, it doesn't matter as testing with an existing version of Firebird installed is itself a valid test of the installer. Such a test should fail and the results can be used for future analysis.

The script is designed to:

  - execute a combination of installation options,
  - make a copy of the new install
  - save the install log and the install configuration into the copy of the install
  - immediately uninstall the package
  - save the uninstall log with the copy of the installation
  - Generate some visual indications of the outcome of the installation and uninstallation

It is **NOT** designed for testing Firebird after the install has completed. However, if you wish to do so be sure to pass NOUNINSTALL otherwise the script will uninstall Firebird immediately.


#### System Requirements

  * The test script is designed to run on Windows systems that are currently supported by Microsoft. It should run on earlier versions too but they may lack features that have been introduced into newer releases of Windows.
  * A version of grep needs to be on the path. Other posix tools _may_ be required at a later date.
  * A Firebird Binary installer package is required. Any version of Firebird _should_ work but testing has only been done with Firebird 4.

  There are no particular installation requirements. The script is intended to be run at command prompt.


#### Configuration

There are some hard coded variables near the beginning of the script. These need to be changed to suit your setup. In particular, FBINST_EXEC must point to the Firebird Binary Installer you wish to test. See the section :SET_GLOBAL_ENV in the script.

#### Examples

Here is a typical configuration to run the regular interactive installer:

  ``fbit.bat``

This will produce the same installation, but entirely scripted:

  ``fbit.bat SCRIPTED``

Uninstallation is automatic and by default will leave files such as firebird.conf in place. There is a 'clean' option to test complete removal of a Firebird install:

  ``fbit.bat SCRIPTED CLEAN``

This sort of command will override the defaults to install Firebird SuperClassic as an application with no automatic start:

  ``fbit.bat SCRIPTED SUPERCLASSIC APPTASK NOAUTOSTART``

All install runs are archived into %USERPROFILE%\fbit-tests with a unique name. You can add your own test identifier by passing TESTNAME name


After a full firebird build from the same console prompt where run_all.bat was run this command will test the freshly built package:

  ``..\install\arch-specific\win32\test_installer\fbit.bat SCRIPTED CLEAN TESTNAME SmokeTestAfterBuild``



#### Functionality Not Yet Implemented

 * A detailed explanation of InnoSetup error codes will be added.
 * Some more verifications will be added
   - test that server is running
   - SYSDBA can connect to employee db locally and via localhost



#### Known issues and problems

 1/ The current focus of FBIT is to test Firebird specific features of the installer. It is not intended as a generic test tool for all InnoSetup based installers. Many scriptable options of InnoSetup based installers have not been implemented and perhaps may never be implemented.

  MERGE_TASKS is currently not possible as it requires use of the ! exclamation mark to unset a task. However FBIT uses Delayed Variable Expansion which strips all instances of '!' during assignment.

  The /HELP option is not implemented and probably never will.

  /CURRENTUSER and /ALLUSERS may be implemented at a later date.

  The various options around RESTART, CLOSE APPS, RESTARTCLOSEDAPPS have not been implemented mainly because the installer does not use them. They may never be implemented in the test harness.

  /LOADINF hasn't been implemented but could be.

  /DIR, /TYPE, /TASKS, /COMPONENTS, /LOG and /SAVEINF are implemented internally in FBIT. It doesn't make sense to expose them.

  /GROUP and /NOICONS could be implemented but currently are not.

  /LANG= probably will be implemented in fbit but is not currently available.

  A full list of the standard InnoSetup commandline params as of December 2020 is available in a text file accompanying this package.

 2/ Parameter passing is not 100% reliable.

 3/ Uninstall error codes from InnoSetup are not reliable.

 4/ Cancelling an interactive uninstall is not detected. Verification of the cancelled uninstall will subsequently detect errors that are not in fact errors.

 5/ Installation type always becomes CustomInstall.

 6/ It is not possible to choose the installation language used by scripted installs.

 7/ Running an interactive install with this script is not well tested.



#### When things go wrong

What could possibly go wrong?



### Future Enhancements

Possible features that will be added to the harness are:

 * Diagnostic report of an existing Firebird installation along with details of O/S and H/W
 * A test suite that runs through all known/reasonable combination of options
 * A script to uninstall MS runtimes that have been deployed by the installer.
 * A means to scrub the registry of all references to previous Firebird installs.
