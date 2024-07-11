#!/bin/sh
export CROSS_BUILD_DIR=`realpath ../linux`
[ -z "$NDK_TOOLCHAIN" ] && export NDK_TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
export PATH=$NDK_TOOLCHAIN/bin:$PATH
