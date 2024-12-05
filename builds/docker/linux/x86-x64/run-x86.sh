#!/bin/sh
docker run --platform i386 --rm --user `id -u`:`id -g` -v `pwd`/../../../..:/firebird -t asfernandes/firebird-builder:fb6-x86-ng-v1
