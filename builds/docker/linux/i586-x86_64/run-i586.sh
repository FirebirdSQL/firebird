#!/bin/sh
docker run --rm --user `id -u`:`id -g` -v `pwd`/../../../..:/firebird -t asfernandes/firebird-builder:i586-5
