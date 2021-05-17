MakeVersion=gen/Make.Version
Build=`grep ^BuildNum ${MakeVersion}|awk '{print $3;}'`
Version=`grep ^FirebirdVersion ${MakeVersion}|awk '{print $3;}'`
Release="Firebird-${Version}.${Build}-0.arm.tar.gz"
Debug="Firebird-withDebugInfo-${Version}.${Build}-0.arm.tar.gz"
Stripped=strip
aStrip=${NDK}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-strip
fbRootDir=`pwd`

cd gen/Release
rm -rf ${Stripped}
cp ${fbRootDir}/builds/install/arch-specific/android/AfterUntar.sh firebird
echo .
echo .
echo "Compress with deb-info"
tar cvfz ../${Debug} firebird

echo .
echo .
echo "Copy binaries"
mkdir ${Stripped}
tar cf - firebird | (cd ${Stripped}; tar xvf -)


cd ${Stripped}
echo .
echo .
echo "Strip"
for file in `find firebird -executable -type f -print`
do
	${aStrip} ${file}
done

echo .
echo .
echo "Compress release"
tar cvfz ../../${Release} firebird
cd ..
