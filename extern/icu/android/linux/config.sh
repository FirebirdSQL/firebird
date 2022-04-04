#!/bin/sh
../source/runConfigureICU Linux --prefix=$PWD/prebuilt \
    CFLAGS="-Os" \
    CXXFLAGS="--std=c++17" \
    --enable-static \
    --enable-shared=no \
    --enable-extras=no \
    --enable-strict=no \
    --enable-icuio=no \
    --enable-layout=no \
    --enable-layoutex=no \
    --enable-tools \
    --enable-tests=no \
    --enable-samples=no \
    --enable-dyload \
    --with-data-packaging=archive
