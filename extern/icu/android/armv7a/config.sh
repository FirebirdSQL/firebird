#!/bin/sh

. ../env.sh

../source/configure --prefix=$(pwd)/prebuilt \
    --host=arm-linux-androideabi \
    --enable-static=no \
    --enable-shared \
    --enable-extras=no \
    --enable-strict=no \
    --enable-icuio=no \
    --enable-layout=no \
    --enable-layoutex=no \
    --enable-tools=no \
    --enable-tests=no \
    --enable-samples=no \
    --enable-renaming \
    --enable-dyload \
    --with-cross-build=$CROSS_BUILD_DIR \
    CFLAGS='-Os -march=armv7-a -mfloat-abi=softfp -mfpu=neon' \
    CXXFLAGS='--std=c++17 -march=armv7-a -mfloat-abi=softfp -mfpu=neon' \
    LDFLAGS='-static-libstdc++ -march=armv7-a -Wl,--fix-cortex-a8' \
    CC=arm-linux-androideabi-clang \
    CXX=arm-linux-androideabi-clang++ \
    AR=arm-linux-androideabi-ar \
    RANLIB=arm-linux-androideabi-ranlib \
    --with-data-packaging=archive
