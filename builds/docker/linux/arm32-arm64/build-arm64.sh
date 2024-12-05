#!/bin/sh
docker buildx build \
	--pull \
	--build-arg ARG_BASE=arm64v8/debian:bookworm \
	--build-arg ARG_TARGET_ARCH=aarch64-pc-linux-gnu \
	--build-arg ARG_CTNF_CONFIG=crosstool-ng-config-arm64 \
	-t asfernandes/firebird-builder:fb6-arm64-ng-v1 .
