#!/bin/sh

set -eu

cmake \
    -B build \
    -G Ninja \
    -D CMAKE_C_COMPILER=clang \
    -D CMAKE_CXX_COMPILER=clang++ \
    -D CMAKE_BUILD_TYPE=Debug \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=1 \
    -D GROSS_ENABLE_UNIT_TESTS=1 \
    -D GROSS_ENABLE_INSTRUMENTS=1

cmake --build build
cmake --build build --target check-gross
