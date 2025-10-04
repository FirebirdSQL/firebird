#!/bin/sh
docker run --platform arm64 --rm --user `id -u`:`id -g` -v `pwd`/../../../..:/firebird -t asfernandes/firebird-builder:fb6-arm64-ng-v1
