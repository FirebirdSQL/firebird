#!/bin/sh
set -e

docker build -t fb-icu-android:v5 .

cid=$(docker create fb-icu-android:v5 true)
docker cp $cid:/out/icu-android.tar.xz ../icu_android.tar.xz
docker rm $cid

docker image rm fb-icu-android:v5
