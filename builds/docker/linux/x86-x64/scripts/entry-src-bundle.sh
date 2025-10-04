#!/bin/sh
set -e

trap exit INT TERM
setarch $SET_ARCH /src-bundle.sh &
wait $!
