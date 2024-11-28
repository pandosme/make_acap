#!/bin/sh
rm -rf build
docker build  --progress=plain --no-cache --build-arg ARCH=aarch64 --tag acap .
docker cp $(docker create acap):/opt/app ./build
mv build/*.eap .
rm -rf build
docker build  --progress=plain --no-cache --tag acap .
docker cp $(docker create acap):/opt/app ./build
mv build/*.eap .
rm -rf build

