#!/bin/sh
docker build \
	--build-arg BASE=ubuntu:raring \
	--build-arg BUILD_ARCH=x86_64-pc-linux-gnu \
	--build-arg LIBDIR=/usr/lib/x86_64-linux-gnu \
	-t asfernandes/firebird-builder:x86_64-5 .
