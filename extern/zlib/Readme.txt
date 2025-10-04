The zlib.exe is a SFX archive with set of include files required to build
Firebird with zlib support and compiled zlib DLL's for Windows x86, x64 and arm64
architectures.

The source code of zlib library was downloaded from

  https://www.zlib.net/zlib131.zip

It was built with VS 2022 compilers using commands specified at win32/Makefile.msc:

Open VsDevCmd prompt for the given architectures (-arch=amd64, -arch=x86 or -arch=arm64) and run:
  nmake -f win32/Makefile.msc

Note, ASM files are not used in build as they were not updated for a long time, not
officially supported and 32-bit build crashes in simplest test, see:

https://github.com/madler/zlib/issues/41
https://github.com/madler/zlib/issues/200
https://github.com/madler/zlib/issues/223
https://github.com/madler/zlib/issues/249
