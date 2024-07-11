#!/bin/sh
set -e

echo no | $ANDROID_HOME/cmdline-tools/latest/bin/avdmanager create avd --name firebird-builder --package "system-images;android-30;google_apis;x86_64"
