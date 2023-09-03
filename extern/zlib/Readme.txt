The zlib.exe is a SFX archive with set of include files required to build 
Firebird with zlib support and compiled zlib DLL's for both win32 and x64 
architectures.

The source code of zlib library was downloaded from 

  http://zlib.net/zlib1213.zip

It was built with MSVC17 compilers using commands specified at win32/Makefile.msc:

win32: 
nmake -f win32/Makefile.msc 

x64:
nmake -f win32/Makefile.msc 

Note, ASM files is not uses in build as it was not updated for a long time, not
officially supported and 32-bit build crashes in simplest test, see:

https://github.com/madler/zlib/issues/41
https://github.com/madler/zlib/issues/200
https://github.com/madler/zlib/issues/223
https://github.com/madler/zlib/issues/249
