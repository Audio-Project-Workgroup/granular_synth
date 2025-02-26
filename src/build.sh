#!/bin/bash

SRC_DIR=$PWD
#-I$SRC_DIR/include

CFLAGS="-g -Wall -Wno-writable-strings -Wno-missing-braces -fno-exceptions -fno-rtti -std=gnu++11"
MAC_GL_FLAGS="-framework OpenGL"
MAC_PLUGIN_FLAGS="-dynamiclib"
LINUX_GL_FLAGS="-lGL"
LINUX_PLUGIN_FLAGS="-shared -fPIC"


if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    GL_FLAGS=$LINUX_GL_FLAGS
    PLUGIN_NAME="plugin.so"
    PLUGIN_FLAGS=$LINUX_PLUGIN_FLAGS
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CFLAGS+=" -Wno-deprecated-declarations"
    GL_FLAGS=$MAC_GL_FLAGS
    PLUGIN_NAME="plugin.dylib"
    PLUGIN_FLAGS=$MAC_PLUGIN_FLAGS
else
    echo "ERROR: unsupported os: $OSTYPE"
    exit 1
fi

mkdir -p ../build
pushd ../build > /dev/null

# compile miniaudio to static library
#clang -c ../src/miniaudio_impl.c -o miniaudio.o
#ar rcs libminiaudio.a miniaudio.o

# compile plugin and host
clang $CFLAGS ../src/plugin.cpp -o $PLUGIN_NAME $PLUGIN_FLAGS -lm

PLUGIN_STATUS=$?

BUILD_PATH="$PWD"
PLUGIN_PATH="$BUILD_PATH/$PLUGIN_NAME"
echo $BUILD_PATH

clang $CFLAGS -D"PLUGIN_PATH=\"$PWD/$PLUGIN_NAME\"" ../src/main.cpp -o test -L. $GL_FLAGS -lglfw -lminiaudio -ldl -lpthread -lm $(pkg-config --libs --cflags libonnxruntime)
#-L$SRC_DIR/libs -lonnxruntime

popd > /dev/null

exit $PLUGIN_STATUS
