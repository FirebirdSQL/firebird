#!/bin/sh
set -e

docker push firebirdsql/firebird-builder-linux:fb6-arm32-ng-v2
docker push firebirdsql/firebird-builder-linux:fb6-arm64-ng-v2
