#!/bin/sh
ARG_NATIVE_ARCH=linux/arm
docker buildx build \
	--progress=plain \
	--platform $ARG_NATIVE_ARCH \
	--pull \
	--build-arg ARG_NATIVE_ARCH=$ARG_NATIVE_ARCH \
	--build-arg ARG_BASE=arm32v7/debian:bookworm \
	--build-arg ARG_TARGET_ARCH=arm-pc-linux-gnueabihf \
	--build-arg ARG_CTNF_CONFIG=crosstool-ng-config-arm32 \
	-t firebirdsql/firebird-builder-linux:fb6-arm32-ng-v2 .
