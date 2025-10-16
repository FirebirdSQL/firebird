#!/bin/sh
set -e

arch=${1}

OS=`uname -s`

case $OS in
	Darwin)
		NDK_TOOLCHAIN_NAME=darwin-x86_64
		TAR_OPTS="--numeric-owner --uid=0 --gid=0"
		FIND_EXEC_OPTS="-perm +0111" ;;
	Linux)
		NDK_TOOLCHAIN_NAME=linux-x86_64
		TAR_OPTS="--numeric-owner --owner=0 --group=0"
		FIND_EXEC_OPTS="-executable" ;;
esac

[ -z "$NDK_TOOLCHAIN" ] && NDK_TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/$NDK_TOOLCHAIN_NAME
aStrip=${NDK_TOOLCHAIN}/bin/llvm-strip

MakeVersion=gen/Make.Version
Build=`grep ^BuildNum ${MakeVersion}|awk '{print $3;}'`
Version=`grep ^FirebirdVersion ${MakeVersion}|awk '{print $3;}'`
PackageVersion=`grep ^PackageVersion ${MakeVersion}|awk '{print $3;}'`
InitialBaseName="Firebird-${Version}.${Build}-${PackageVersion}-android-initial-${arch}"
InitialDebugTar="$InitialBaseName-withDebugSymbols.tar"
InitialDebugTarGz="$InitialDebugTar.gz"
Stripped=strip

FinalRelease="Firebird-${Version}.${Build}-${PackageVersion}-android-${arch}.tar.gz"
FinalDebug="Firebird-${Version}.${Build}-${PackageVersion}-android-${arch}-withDebugSymbols.tar.gz"

[ -z "$AndroidDevicePort" ] && AndroidDevicePort=5554
AndroidDeviceName=emulator-$AndroidDevicePort
AndroidDir=/data/$InitialBaseName

mkdir -p gen/Release
(cd gen; gunzip --force -k $InitialDebugTarGz)
(cd gen/Release; tar xvzf ../$InitialDebugTarGz)

$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName shell "rm -rf $AndroidDir"
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName shell "mkdir $AndroidDir"
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName push gen/$InitialDebugTar $AndroidDir/
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName shell "(cd $AndroidDir && tar xvf $InitialDebugTar)"
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName shell "(cd $AndroidDir/firebird && ./common_test --log_level=error && ./libEngine14_test --log_level=error && ./isql_test --log_level=error)"
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName shell "(cd $AndroidDir/firebird && ./AfterUntar.sh)"
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName pull $AndroidDir/firebird/firebird.msg gen/Release/firebird/
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName pull $AndroidDir/firebird/security6.fdb gen/Release/firebird/
#$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName pull $AndroidDir/firebird/examples/empbuild/employe2.fdb gen/Release/firebird/examples/empbuild/
$ANDROID_HOME/platform-tools/adb -s $AndroidDeviceName shell "(rm -rf $AndroidDir)"

rm gen/$InitialDebugTar
cd gen/Release
rm -rf ${Stripped}

TAR_OPTS="$TAR_OPTS --exclude *_test --exclude security.sql --exclude employe2.sql --exclude build_file --exclude AfterUntar.sh"

tar $TAR_OPTS -czvf ../$FinalDebug firebird

mkdir ${Stripped}
tar cf - firebird | (cd ${Stripped}; tar xvf -)
cd ${Stripped}
echo .
echo .
echo "Strip"
for file in `find firebird -type f $FIND_EXEC_OPTS -not -name "*.sh" -print`
do
	${aStrip} ${file}
done

tar $TAR_OPTS -czvf ../../$FinalRelease firebird
