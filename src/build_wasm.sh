#!/usr/bin/env bash

SRC_DIR=$PWD
DATA_DIR=$SRC_DIR/../data

CFLAGS="-Wall -Wextra -c"
WASM_CFLAGS="--target=wasm32 -nostdlib"
WASM_LFLAGS="--no-entry --export-all --import-memory"

mkdir -p ../build
pushd ../build > /dev/null

BUILD_DIR=$PWD

clang $CFLAGS $WASM_CFLAGS $SRC_DIR/wasm_glue.cpp -o wasm_glue.o
wasm-ld $WASM_LFLAGS wasm_glue.o -o wasm_glue.wasm
wasm2wat wasm_glue.wasm -o wasm_glue.wat

popd > /dev/null
