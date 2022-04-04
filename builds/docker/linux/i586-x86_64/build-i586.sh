#!/bin/sh
docker build \
	--build-arg BASE=i386/ubuntu:vivid \
	--build-arg BUILD_ARCH=i586-pc-linux-gnu \
	--build-arg LIBDIR=/lib/i386-linux-gnu \
	-t asfernandes/firebird-builder:i586-5 .
