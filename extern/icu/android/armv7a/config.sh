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
    LDFLAGS='-static-libstdc++ -march=armv7-a -Wl,--fix-cortex-a8 -Wl,-rpath=\$$ORIGIN' \
    CC=armv7a-linux-androideabi29-clang \
    CXX=armv7a-linux-androideabi29-clang++ \
    AR=llvm-ar \
    RANLIB=llvm-ranlib \
    --with-data-packaging=archive
