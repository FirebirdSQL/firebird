#!/bin/sh
export CROSS_BUILD_DIR=`realpath ../linux`
export ANDROID_NDK=${NDK}
export ANDROID_TOOLCHAIN=${NDK_TOOLCHAIN}
export PATH=$ANDROID_TOOLCHAIN/bin:$PATH
