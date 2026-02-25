#!/bin/sh
set -e

exec /entry-common.sh setarch $SET_ARCH /build.sh
