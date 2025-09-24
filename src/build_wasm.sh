#!/usr/bin/env bash

SRC_DIR=$PWD
DATA_DIR=$SRC_DIR/../data

MEMORY_PAGE_COUNT=3
MEMORY_SIZE=$[$MEMORY_PAGE_COUNT*64*1024]

CFLAGS="-Wall -Wextra -Wno-missing-braces -Wno-unused-function -Wno-unused-parameter -Wno-writable-strings -c"
WASM_CFLAGS="--target=wasm32 -nostdlib -matomics -mbulk-memory"
WASM_LFLAGS="--no-entry --export-all --import-memory --shared-memory --initial-memory=$MEMORY_SIZE --max-memory=$MEMORY_SIZE"

mkdir -p ../build
pushd ../build > /dev/null

BUILD_DIR=$PWD

clang $CFLAGS $WASM_CFLAGS $SRC_DIR/wasm_glue.cpp -o wasm_glue.o
wasm-ld $WASM_LFLAGS wasm_glue.o -o wasm_glue.wasm
wasm2wat wasm_glue.wasm --enable-threads -o wasm_glue.wat
cp wasm_glue.wasm ../wasm_glue.wasm

popd > /dev/null
