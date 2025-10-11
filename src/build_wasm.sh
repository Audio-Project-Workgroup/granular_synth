#!/usr/bin/env bash

# TODO: enable/disble these with cli flags (e.g. build --debug --logging)
BUILD_DEBUG=0
BUILD_LOGGING=1

SRC_DIR=$PWD
DATA_DIR=$SRC_DIR/../data

MEMORY_PAGE_COUNT=256
MEMORY_SIZE=$[$MEMORY_PAGE_COUNT*64*1024]

CFLAGS="-Wall -Wextra -Wno-missing-braces -Wno-unused-function -Wno-unused-parameter -Wno-writable-strings -fPIC -DBUILD_DEBUG=$BUILD_DEBUG -DBUILD_LOGGING=$BUILD_LOGGING -c"
WASM_CFLAGS="--target=wasm32 -nostdlib -matomics -mbulk-memory"
WASM_LFLAGS="--no-entry --export-all --export-table --import-memory --shared-memory --initial-memory=$MEMORY_SIZE --max-memory=$MEMORY_SIZE -z stack-size=$[8 * 1024 * 1024]"
if [[ $BUILD_DEBUG == 1 ]]; then
    CFLAGS+=" -g -Og"
else
    CFLAGS+=" -O3"
fi
echo $CFLAGS
echo $WASM_CFLAGS
echo $WASM_LFLAGS

mkdir -p ../build
pushd ../build > /dev/null

BUILD_DIR=$PWD

# compile code to object file
clang $CFLAGS $WASM_CFLAGS $SRC_DIR/wasm_glue.cpp -o wasm_glue.o

# link object file, procuding wasm file
wasm-ld $WASM_LFLAGS wasm_glue.o -o wasm_glue.wasm

# generate human-readable code from wasm file
wasm2wat wasm_glue.wasm --enable-threads -o wasm_glue.wat

# copy wasm to project root directory
cp wasm_glue.wasm ../wasm_glue.wasm

popd > /dev/null
