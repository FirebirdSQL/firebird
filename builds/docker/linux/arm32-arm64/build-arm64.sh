#!/bin/sh
ARG_NATIVE_ARCH=linux/arm64
docker buildx build \
	--progress=plain \
	--platform $ARG_NATIVE_ARCH \
	--pull \
	--build-arg ARG_NATIVE_ARCH=$ARG_NATIVE_ARCH \
	--build-arg ARG_BASE=arm64v8/debian:bookworm \
	--build-arg ARG_TARGET_ARCH=aarch64-pc-linux-gnu \
	--build-arg ARG_CTNF_CONFIG=crosstool-ng-config-arm64 \
	-t firebirdsql/firebird-builder-linux:fb6-arm64-ng-v2 .
