#!/bin/sh
docker run --platform amd64 --rm \
	-e CTNG_UID=${CTNG_UID:-$(id -u)} \
	-e CTNG_GID=${CTNG_GID:-$(id -g)} \
	-v `pwd`/../../../..:/firebird \
	-v `pwd`/../../../../gen:/home/ctng/firebird-build/gen \
	--entrypoint /entry-src-bundle.sh \
	-t firebirdsql/firebird-builder-linux:fb6-x64-ng-v2
