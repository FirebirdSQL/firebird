#!/bin/sh
docker run --platform amd64 --rm --user `id -u`:`id -g` -v `pwd`/../../../..:/firebird -t --entrypoint /entry-src-bundle.sh asfernandes/firebird-builder:fb6-x64-ng-v1
