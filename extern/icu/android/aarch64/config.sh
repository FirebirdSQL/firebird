#!/bin/sh

. ../env.sh

../source/configure --prefix=$(pwd)/prebuilt \
    --host=aarch64-android-linux \
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
    CFLAGS='-Os' \
    CXXFLAGS='--std=c++17' \
    LDFLAGS='-static-libstdc++' \
    CC=aarch64-linux-android24-clang \
    CXX=aarch64-linux-android24-clang++ \
    AR=aarch64-linux-android-ar \
    RANLIB=aarch64-linux-android-ranlib \
    --with-data-packaging=archive
