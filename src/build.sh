#!/bin/bash

SRC_DIR=$PWD
DATA_DIR=$SRC_DIR/../data

CFLAGS="-g -Wall -Wno-writable-strings -Wno-missing-braces -Wno-unused-function -fno-exceptions -fno-rtti -march=native -std=gnu++11 -DBUILD_DEBUG=1"
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

BUILD_DIR=$PWD
# compile miniaudio to static library
#clang -c ../src/miniaudio_impl.c -o miniaudio.o
#ar rcs libminiaudio.a miniaudio.o

# preprocessor
#clang $CFLAGS ../src/preprocessor.cpp -o preprocessor
popd > /dev/null
#../build/preprocessor plugin.h > generated.cpp
pushd ../build > /dev/null

# compile plugin and host
clang $CFLAGS -D"DATA_PATH=\"../data/\"" ../src/plugin.cpp -o $PLUGIN_NAME $PLUGIN_FLAGS -lm

PLUGIN_STATUS=$?

clang $CFLAGS -D"PLUGIN_PATH=\"$PLUGIN_NAME\"" ../src/main.cpp -o test -L. $GL_FLAGS -lglfw -lminiaudio -ldl -lpthread -lm
#$(pkg-config --libs --cflags libonnxruntime)
#-L$SRC_DIR/libs -lonnxruntime

if [[ "$OSTYPE" == "darwin"* ]]; then
    mkdir -p Granade.app
    pushd Granade.app > /dev/null

    mkdir -p Contents
    pushd Contents > /dev/null

    cp -r $DATA_DIR data
    cp $DATA_DIR/Info.plist Info.plist

    mkdir -p Frameworks

    mkdir -p Resources
    pushd Resources > /dev/null

    cp $DATA_DIR/icon.icns Granade.icns

    popd > /dev/null # Resources -> Contents

    mkdir -p MacOS
    pushd MacOS > /dev/null

    cp $BUILD_DIR/test Granade
    cp $BUILD_DIR/plugin.dylib plugin.dylib

    # copy dependencies to app bundle
    cp -Rn $(otool -L Granade | grep -oE "/(.+?)/OpenGL.framework") ../Frameworks
    cp -Rn $(otool -L Granade | grep -oE "/(.+?)/glfw/lib/") ../Frameworks

    # tell the executable to use the libraries in the bundle instead
    install_name_tool -change /opt/homebrew/opt/glfw/lib/libglfw.3.dylib @rpath/libglfw.3.dylib Granade
    install_name_tool -add_rpath @executable_path/../Frameworks Granade

    popd > /dev/null # MacOS -> Contents

    popd > /dev/null # Contents -> Granade.app

    popd > /dev/null # Granade.app -> build
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "TODO: create AppImage bundle on linux"
fi

popd > /dev/null # build -> src

exit $PLUGIN_STATUS
