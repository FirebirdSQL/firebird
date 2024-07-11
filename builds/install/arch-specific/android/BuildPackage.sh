#!/bin/sh
set -e

arch=${1}

MakeVersion=gen/Make.Version
Build=`grep ^BuildNum ${MakeVersion}|awk '{print $3;}'`
Version=`grep ^FirebirdVersion ${MakeVersion}|awk '{print $3;}'`
PackageVersion=`grep ^PackageVersion ${MakeVersion}|awk '{print $3;}'`
Release="Firebird-${Version}.${Build}-${PackageVersion}-android-initial-${arch}.tar.gz"
Debug="Firebird-${Version}.${Build}-${PackageVersion}-android-initial-${arch}-withDebugSymbols.tar.gz"
fbRootDir=`pwd`

runTar()
{
	tarfile=${1}
	tar cvfz ${tarfile} --exclude '*.a' --exclude '*.fdb' --exclude '*.msg' firebird
}

cd gen/Release
cp ${fbRootDir}/builds/install/arch-specific/android/AfterUntar.sh firebird
chmod +x firebird/AfterUntar.sh
cp ${fbRootDir}/src/dbs/security.sql firebird
cp ${fbRootDir}/examples/empbuild/employe2.sql firebird
unzip -o ../../extern/icu/icudt.zip -d firebird
tar -C firebird --wildcards --strip-components 1 -xvf ../../extern/icu/icu_android.tar.xz ${arch}/*
echo .
echo .
echo "Compress with deb-info"
runTar ../${Debug}
