#!/bin/bash
set -e

echo "Building fildel in Docker..."
docker build -t fildel-builder .

echo "Extracting binary..."
docker create --name extract fildel-builder
docker cp extract:/build/fildel ./fildel-linux-amd64
docker rm extract

echo "Done! Binary: ./fildel-linux-amd64"
