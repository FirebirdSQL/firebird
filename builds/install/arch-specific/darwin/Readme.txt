The Firebird install process will create a new user: firebird. This is for added security. Please don't delete this user. The installer creates a Firebird framework in /Library/Frameworks. By default the install creates one super-user database user "SYSDBA" with a password "masterkey". You should change that password using gsec or isql according to the documentation.

All the standard command line executables are installed in /Library/Frameworks/Firebird.framework/Resources/bin. 

If you are interested in helping with the Firebird Project please contact us via the Firebird website at www.firebirdsql.org.

The release notes can be found in the doc directory.  More generic documentation for Firebird can be found on www.firebirdsql.org and the IBPhoenix web site at www.ibphoenix.com. There is also a yahoo group that can be subscribed to named "ib-support" if you have any problems.

Thanks to:
John Bellardo (Original MacOSX port for Firebird)
David Pugh (Firebird 1.5.3 Port)
Paul Beach & Alex Peshkov (Firebird 1.5.x,2.x & 3.x Ports)
Paul Beach (Firebird Installer)
Daniel Puckett (Firebird 2.x Launch Daemon for MacOSX 10.5+) 
Craig Altenburg (Improvements to the install scripts)
