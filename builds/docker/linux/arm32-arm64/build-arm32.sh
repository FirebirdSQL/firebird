#!/bin/sh
docker buildx build \
	--pull \
	--build-arg ARG_BASE=arm32v7/debian:bookworm \
	--build-arg ARG_TARGET_ARCH=arm-pc-linux-gnueabihf \
	--build-arg ARG_CTNF_CONFIG=crosstool-ng-config-arm32 \
	-t asfernandes/firebird-builder:fb6-arm32-ng-v1 .
