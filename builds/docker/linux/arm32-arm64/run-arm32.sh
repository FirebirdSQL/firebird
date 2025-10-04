#!/bin/sh
docker run --platform arm --rm --user `id -u`:`id -g` -v `pwd`/../../../..:/firebird -t asfernandes/firebird-builder:fb6-arm32-ng-v1
