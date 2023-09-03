#!/bin/sh
docker build \
	--pull \
	--build-arg ARG_BASE=arm32v7/ubuntu:22.04 \
	--build-arg ARG_TARGET_ARCH=arm-pc-linux-gnueabihf \
	--build-arg ARG_CTNF_CONFIG=crosstool-ng-config-arm32 \
	-t asfernandes/firebird-builder:fb5-arm32-ng-v1 .
