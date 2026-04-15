#!/bin/sh
mkdir -p `pwd`/../../../../gen
docker run --platform i386 --rm \
	-e CTNG_UID=${CTNG_UID:-$(id -u)} \
	-e CTNG_GID=${CTNG_GID:-$(id -g)} \
	-v `pwd`/../../../..:/firebird:ro \
	-v `pwd`/../../../../gen:/home/ctng/firebird-build/gen \
	-t firebirdsql/firebird-builder-linux:fb6-x86-ng-v2
