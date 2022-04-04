#!/bin/sh
trap exit INT TERM
/build.sh &
wait $!
