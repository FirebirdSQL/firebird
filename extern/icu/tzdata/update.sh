#!/bin/sh

# Change version.txt before run this script.

THIS_DIR=`readlink -f $0`
THIS_DIR=`dirname $THIS_DIR`

TMP_DIR=`mktemp -d`
VERSION=`cat $THIS_DIR/version.txt`
BASE_URL=https://github.com/unicode-org/icu-data/raw/main/tzdata/icunew/$VERSION/44

echo Building update-ids...
cd $THIS_DIR
g++ -o $TMP_DIR/update-ids update-ids.cpp -licui18n -licuuc

echo Downloading and updating little-endian files...
mkdir $TMP_DIR/le
cd $TMP_DIR/le
curl -OLs $BASE_URL/le/metaZones.res
curl -OLs $BASE_URL/le/timezoneTypes.res
curl -OLs $BASE_URL/le/windowsZones.res
curl -OLs $BASE_URL/le/zoneinfo64.res
ICU_TIMEZONE_FILES_DIR=. $TMP_DIR/update-ids ids.dat $THIS_DIR/../../../src/common/TimeZones.h $THIS_DIR/../../../src/include/firebird/TimeZones.h
rm $THIS_DIR/le.zip
zip $THIS_DIR/le.zip *.res ids.dat

echo Downloading and updating big-endian files...
mkdir $TMP_DIR/be
cd $TMP_DIR/be
curl -OLs $BASE_URL/be/metaZones.res
curl -OLs $BASE_URL/be/timezoneTypes.res
curl -OLs $BASE_URL/be/windowsZones.res
curl -OLs $BASE_URL/be/zoneinfo64.res
cp $TMP_DIR/le/ids.dat .
rm $THIS_DIR/be.zip
zip $THIS_DIR/be.zip *.res ids.dat

rm -r $TMP_DIR
