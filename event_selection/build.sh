#!/bin/sh
set -e

rm -rf build

# Build aarch64
docker build --progress=plain --no-cache --build-arg ARCH=aarch64 --tag acap .
CONTAINER_ID=$(docker create acap)
docker cp "$CONTAINER_ID":/opt/app ./build
docker rm "$CONTAINER_ID"
mv build/*.eap .
rm -rf build

# Build armv7hf
docker build --progress=plain --no-cache --tag acap .
CONTAINER_ID=$(docker create acap)
docker cp "$CONTAINER_ID":/opt/app ./build
docker rm "$CONTAINER_ID"
mv build/*.eap .
rm -rf build
