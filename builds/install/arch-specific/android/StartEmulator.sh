#!/bin/sh
set -e

$ANDROID_HOME/emulator/emulator -no-window -avd firebird-builder -port 5554 &
$ANDROID_HOME/platform-tools/adb wait-for-device -s emulator-5554
$ANDROID_HOME/platform-tools/adb -s emulator-5554 root
