Firebird Database Server $MAJOR.$MINOR.$RELEASE
===============================================


This document is a guide to installing this package of
Firebird $MAJOR.$MINOR on the Windows platform. These notes refer
to the installation package itself, rather than
Firebird $MAJOR.$MINOR in general. In addition, these notes are
primarily aimed at users of the binary installer.

It is assumed that readers of this document are already
familiar with Firebird. If you are evaluating Firebird $MAJOR.$MINOR
as part of a migration from some older Firebird version you are advised
to review the Firebird $MAJOR.$MINOR documentation to understand
the changes made between your version and $MAJOR.$MINOR.


Contents
--------

o Before installation
o Deployment of gds32.dll
o Installation of the Guardian
o Re-installation of Firebird $MAJOR.$MINOR
o Known installation problems
o Uninstallation
o Installation from a batch file


Before installation
-------------------

It is recommended that you UNINSTALL all previous
versions of Firebird or InterBase before installing
this package. It is especially important to verify that
fbclient.dll and gds32.dll are removed from <system32>.
See the UNINSTALL section below for more info on this.


Installation of the Guardian
----------------------------

We are hoping to phase out the Guardian. It doesn't 
work with the Classic server and the binary installer 
does not offer it at install time if Classic is 
chosen. If SuperServer or SuperClassic are chosen 
it is offered but not selected by default.


Re-installation of Firebird
---------------------------

The binary installer does its best to detect and
preserve a previous install. If the installer detects
firebird.conf or security5.fdb it will not offer the
option to set the SYSDBA username and password.


Known installation problems
---------------------------------

o It is only possible to use the binary installer
  to install the default instance of Firebird $MAJOR.$MINOR. If
  you wish to install additional, named instances you
  should manually install them with the zipped install
  image.

o Unfortunately, the installer cannot reliably detect
  if a previous version of Firebird Classic server
  is running.

o The service installer (instsvc) uses the same
  default instance name for 32-bit and 64-bit
  installations. This is by design. Services exist
  in a single name space.

o Be sure to install as an administrator. ie, if 
  using the binary installer right click and choose 
  'Run as administrator'. Otherwise the installer 
  may be unable to start the Firebird service at
  the end of installation.

o Libraries deployed by instclient may fail to load if
  the MS runtime libraries have not been installed. 
  This may be a problem if installing on older Windows
  platforms.


Uninstallation
--------------

o It is preferred that this package be uninstalled
  correctly using the uninstallation application
  supplied. This can be called from the Control Panel.
  Alternatively it can be uninstalled by running
  unins000.exe directly from the installation
  directory.

o If Firebird is running as an application (instead of
  as a service) it is recommended that you manually
  stop the server before running the uninstaller. This
  is because the uninstaller cannot stop a running
  application. If a server is running during the
  uninstall the uninstall will complete with errors.
  You will have to delete the remnants by hand.

o Uninstallation leaves six files in the install
  directory:

  - databases.conf
  - firebird.conf
  - fbtrace.conf
  - replication.conf
  - firebird.log
  - security5.fdb

  This is intentional. These files are all
  potentially modifiable by users and may be required
  if Firebird is re-installed in the future. They can
  be deleted if no longer required.

o A new feature of the uninstaller is an option to
  run it with the /CLEAN parameter. This will check
  the shared file count of each of the above files. If
  possible it will delete them.


Installation from a batch file
------------------------------

The setup program can be run from a batch file.
Please see this document:

    installation_scripted.txt

for full details.

