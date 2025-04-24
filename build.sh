#!/bin/bash

set -e  # Exit on error

BUILD_DIR=build

if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm ../src
make
