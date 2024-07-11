#!/bin/sh
set -e

$ANDROID_HOME/cmdline-tools/latest/bin/avdmanager delete avd --name firebird-builder
