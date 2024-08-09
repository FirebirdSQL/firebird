# Setup Command Line Parameters

(Taken from InnoSetup Help v. 6.1.2 Deember 2020)

The Setup program accepts optional command line parameters. These can be useful to system administrators, and to other programs calling the Setup program.


**/HELP, /?**

Shows a summary of this information. Ignored if the UseSetupLdr [Setup] section directive was set to no.


**/SP-**

Disables the This will install... Do you wish to continue? prompt at the beginning of Setup. Of course, this will have no effect if the DisableStartupPrompt [Setup] section directive was set to yes.


**/SILENT, /VERYSILENT**

Instructs Setup to be silent or very silent. When Setup is silent the wizard and the background window are not displayed but the installation progress window is. When a setup is very silent this installation progress window is not displayed. Everything else is normal so for example error messages during installation are displayed and the startup prompt is (if you haven't disabled it with DisableStartupPrompt or the '/SP-' command line option explained above).

If a restart is necessary and the '/NORESTART' command isn't used (see below) and Setup is silent, it will display a Reboot now? message box. If it's very silent it will reboot without asking.


**/SUPPRESSMSGBOXES**

Instructs Setup to suppress message boxes. Only has an effect when combined with '/SILENT' or '/VERYSILENT'.

The default response in situations where there's a choice is:

- Yes in a 'Keep newer file?' situation.
- No in a 'File exists, confirm overwrite.' situation.
- Abort in Abort/Retry situations.
- Cancel in Retry/Cancel situations.
- Yes (=continue) in a DiskSpaceWarning/DirExists/DirDoesntExist/NoUninstallWarning/ExitSetupMessage/ConfirmUninstall situation.
- Yes (=restart) in a FinishedRestartMessage/UninstalledAndNeedsRestart situation.
- The recommended choice in a PrivilegesRequiredOverridesAllowed=dialog situation.

5 message boxes are not suppressible:

- The About Setup message box.
- The Exit Setup? message box.
- The FileNotInDir2 message box displayed when Setup requires a new disk to be inserted and the disk was not found.
- Any (error) message box displayed before Setup (or Uninstall) could read the command line parameters.
- Any task dialog or message box displayed by [Code] support functions TaskDialogMsgBox and MsgBox.

**/ALLUSERS**

Instructs Setup to install in administrative install mode. Only has an effect when the [Setup] section directive PrivilegesRequiredOverridesAllowed allows the commandline override.

**/CURRENTUSER**

Instructs Setup to install in non administrative install mode. Only has an effect when the [Setup] section directive PrivilegesRequiredOverridesAllowed allows the commandline override.

**/LOG**

Causes Setup to create a log file in the user's TEMP directory detailing file installation and [Run] actions taken during the installation process. This can be a helpful debugging aid. For example, if you suspect a file isn't being replaced when you believe it should be (or vice versa), the log file will tell you if the file was really skipped, and why.

The log file is created with a unique name based on the current date. (It will not overwrite or append to existing files.)

The information contained in the log file is technical in nature and therefore not intended to be understandable by end users. Nor is it designed to be machine-parsable; the format of the file is subject to change without notice.

**/LOG="filename"**

Same as /LOG, except it allows you to specify a fixed path/filename to use for the log file. If a file with the specified name already exists it will be overwritten. If the file cannot be created, Setup will abort with an error message.

**/NOCANCEL**

Prevents the user from cancelling during the installation process, by disabling the Cancel button and ignoring clicks on the close button. Useful along with '/SILENT' or '/VERYSILENT'.

**/NORESTART**

Prevents Setup from restarting the system following a successful installation, or after a Preparing to Install failure that requests a restart. Typically used along with /SILENT or /VERYSILENT.

**/RESTARTEXITCODE=exit code**

Specifies a custom exit code that Setup is to return when the system needs to be restarted following a successful installation. (By default, 0 is returned in this case.) Typically used along with /NORESTART. See also: Setup Exit Codes

**/CLOSEAPPLICATIONS**

Instructs Setup to close applications using files that need to be updated by Setup if possible.

**/NOCLOSEAPPLICATIONS**

Prevents Setup from closing applications using files that need to be updated by Setup. If /CLOSEAPPLICATIONS was also used, this command line parameter is ignored.

**/FORCECLOSEAPPLICATIONS**

Instructs Setup to force close when closing applications.

**/NOFORCECLOSEAPPLICATIONS**

Prevents Setup from force closing when closing applications. If /FORCECLOSEAPPLICATIONS was also used, this command line parameter is ignored.

**/LOGCLOSEAPPLICATIONS**

Instructs Setup to create extra logging when closing applications for debugging purposes.

**/RESTARTAPPLICATIONS**

Instructs Setup to restart applications if possible.

**/NORESTARTAPPLICATIONS**

Prevents Setup from restarting applications. If /RESTARTAPPLICATIONS was also used, this command line parameter is ignored.

**/LOADINF="filename"**

Instructs Setup to load the settings from the specified file after having checked the command line. This file can be prepared using the '/SAVEINF=' command as explained below.

Don't forget to use quotes if the filename contains spaces.

**/SAVEINF="filename"**

Instructs Setup to save installation settings to the specified file.

Don't forget to use quotes if the filename contains spaces.

**/LANG=language**

Specifies the language to use. language specifies the internal name of the language as specified in a [Languages] section entry.

When a valid /LANG parameter is used, the Select Language dialog will be suppressed.

**/DIR="x:\dirname"**

Overrides the default directory name displayed on the Select Destination Location wizard page. A fully qualified pathname must be specified. May include an "expand:" prefix which instructs Setup to expand any constants in the name. For example: '/DIR=expand:{autopf}\My Program'.

**/GROUP="folder name"**

Overrides the default folder name displayed on the Select Start Menu Folder wizard page. May include an "expand:" prefix, see '/DIR='. If the [Setup] section directive DisableProgramGroupPage was set to yes, this command line parameter is ignored.

**/NOICONS**

Instructs Setup to initially check the Don't create a Start Menu folder check box on the Select Start Menu Folder wizard page.

**/TYPE=type name**

Overrides the default setup type.

If the specified type exists and isn't a custom type, then any /COMPONENTS parameter will be ignored.

**/COMPONENTS="comma separated list of component names"**

Overrides the default component settings. Using this command line parameter causes Setup to automatically select a custom type. If no custom type is defined, this parameter is ignored.

Only the specified components will be selected; the rest will be deselected.

If a component name is prefixed with a "*" character, any child components will be selected as well (except for those that include the dontinheritcheck flag). If a component name is prefixed with a "!" character, the component will be deselected.

This parameter does not change the state of components that include the fixed flag.

Example:

Deselect all components, then select the "help" and "plugins" components:

```
/COMPONENTS="help,plugins"
```

Example:

Deselect all components, then select a parent component and all of its children with the exception of one:

```
/COMPONENTS="*parent,!parent\child" /TASKS="comma separated list of task names"
```

Specifies a list of tasks that should be initially selected.

Only the specified tasks will be selected; the rest will be deselected. Use the /MERGETASKS parameter instead if you want to keep the default set of tasks and only select/deselect some of them.

If a task name is prefixed with a "*" character, any child tasks will be selected as well (except for those that include the dontinheritcheck flag). If a task name is prefixed with a "!" character, the task will be deselected.

Example:

Deselect all tasks, then select the "desktopicon" and "fileassoc" tasks:

```
/TASKS="desktopicon,fileassoc"
```

Example:

Deselect all tasks, then select a parent task and all of its children with the exception of one:

```
/TASKS="*parent,!parent\child" /MERGETASKS="comma separated list of task names"
```

Like the /TASKS parameter, except the specified tasks will be merged with the set of tasks that would have otherwise been selected by default.

If UsePreviousTasks is set to yes, the specified tasks will be selected/deselected after any previous tasks are restored.

Example:

Keep the default set of selected tasks, but additionally select the "desktopicon" and "fileassoc" tasks:

```
/MERGETASKS="desktopicon,fileassoc"
```

Example:

Keep the default set of selected tasks, but deselect the "desktopicon" task:

```
/MERGETASKS="!desktopicon" /PASSWORD=password
```

Specifies the password to use. If the [Setup] section directive Password was not set, this command line parameter is ignored.

When an invalid password is specified, this command line parameter is also ignored.
