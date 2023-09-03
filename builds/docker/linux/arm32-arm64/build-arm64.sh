#!/bin/sh
docker build \
	--pull \
	--build-arg ARG_BASE=arm64v8/ubuntu:22.04 \
	--build-arg ARG_TARGET_ARCH=aarch64-pc-linux-gnu \
	--build-arg ARG_CTNF_CONFIG=crosstool-ng-config-arm64 \
	-t asfernandes/firebird-builder:fb5-arm64-ng-v1 .
