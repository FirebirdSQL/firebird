#!/bin/sh
set -e

trap exit INT TERM
/build.sh &
wait $!
