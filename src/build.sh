#!/bin/bash

## -----------------------------------------------------------------------------
## default options

# config options
config_debug=0
config_logging=0

# target options
target_plugin=0
target_exe=0
target_vst=0
target_web=0
target_all=1

echo "initial values:"
echo "config_debug: $config_debug"
echo "config_logging: $config_logging"
echo "target_plugin: $target_plugin"
echo "target_exe: $target_exe"
echo "target_vst: $target_vst"
echo "target_web: $target_web"
echo "target_all: $target_all"

## -----------------------------------------------------------------------------
## parse cli arguments

for arg in "$@"; do
    echo "$arg"
    key=$(echo $arg | cut -d ":" -f 1)
    values=$(echo $arg | cut -d ":" -f 2)
    if [[ "$key" == "target" ]]; then
	target_plugin=0
	target_exe=0
	target_vst=0
	target_web=0
	target_all=0
    fi

    if [[ "$key" == "config" ]]; then
	config_debug=0
	config_logging=0
    fi

    OLD_IFS=$IFS
    IFS='+' read -ra values_array <<< "$values"
    for value in "${values_array[@]}"; do
	var="${key}_${value}"
	if [[ -v $var ]]; then
	    declare "$var"=1
	else
	    echo "unknown variable ${key}_${value}"
	fi
    done
    IFS=$OLD_IFS
done

if [[ $target_exe == 1 ]]; then
    target_plugin=1
fi
if [[ $target_vst == 1 ]]; then
    target_plugin=1
fi
if [[ $target_all == 1 ]]; then
    target_plugin=1
    target_exe=1
    target_vst=1
    target_web=1
fi

echo "after parsing:"
echo "config_debug: $config_debug"
echo "config_logging: $config_logging"
echo "target_plugin: $target_plugin"
echo "target_exe: $target_exe"
echo "target_vst: $target_vst"
echo "target_web: $target_web"
echo "target_all: $target_all"

## -----------------------------------------------------------------------------
## set up directories, flags

SRC_DIR=$PWD
DATA_DIR=$SRC_DIR/../data

CFLAGS="-Wall -Wno-writable-strings -Wno-missing-braces -Wno-unused-function"
if [[ $config_debug == 1 ]]; then
    CFLAGS+=" -g"
fi
CFLAGS+=" -I$SRC_DIR/include -I$SRC_DIR/include/glad/include"
CFLAGS+=" -fno-exceptions -fno-rtti -march=native -std=gnu++11"
CFLAGS+=" -DBUILD_DEBUG=$config_debug -DBUILD_LOGGING=$config_logging"
#CFLAGS="-g -Wall -Wno-writable-strings -Wno-missing-braces -Wno-unused-function -fno-exceptions -fno-rtti -march=native -std=gnu++11 -DBUILD_DEBUG=0"
MAC_GL_FLAGS="-framework OpenGL"
MAC_PLUGIN_FLAGS="-dynamiclib"
LINUX_GL_FLAGS="-lGL"
LINUX_PLUGIN_FLAGS="-shared -fPIC"

echo $CFLAGS

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    #CFLAGS+= " -Wl,rpath,'$ORIGIN/../lib' -Wl,--enable-new-dtags"
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

## -----------------------------------------------------------------------------
## build targets

# compile miniaudio to static library
if [ ! -f libminiaudio.a ]; then
    echo "compiling miniaudio..."
    clang -c ../src/miniaudio_impl.c -o miniaudio.o
    ar rcs libminiaudio.a miniaudio.o
fi

# preprocessor
#clang $CFLAGS ../src/preprocessor.cpp -o preprocessor
#popd > /dev/null
#../build/preprocessor plugin.h > generated.cpp
#pushd ../build > /dev/null

# asset packer
if [ ! -f $DATA_DIR/test_atlas.png ]; then
    echo "compiling asset packer..."
    clang $CFLAGS $SRC_DIR/asset_packer.cpp $LFLAGS -lm -o asset_packer
    echo "running asset packer..."
    ./asset_packer
fi

# TODO: try compiling common layer implementations to separate lib/obj and link
#       with all targets to speed up build times

# plugin
# TODO: allow linking the plugin to the exe and vst statically for release builds
if [[ $target_plugin == 1 ]]; then
    echo "compiling plugin..."
    clang $CFLAGS -D"DATA_PATH=\"../data/\"" ../src/plugin.cpp -o $PLUGIN_NAME $PLUGIN_FLAGS -lm
fi
PLUGIN_STATUS=$?

# exe
if [[ $target_exe == 1 ]]; then
    if [[ $PLUGIN_STATUS == 0 ]]; then
	echo "compiling exe..."
	clang $CFLAGS -D"PLUGIN_PATH=\"$PLUGIN_NAME\"" ../src/main.cpp -o granade -L. $GL_FLAGS -lglfw -lminiaudio -ldl -lpthread -lm
	#$(pkg-config --libs --cflags libonnxruntime)
	#-L$SRC_DIR/libs -lonnxruntime
    else
	echo "ERROR: plugin build failed. skipping exe compilation"
    fi
fi
EXE_STATUS=$?

if [[ $target_vst == 1 ]]; then
    if [[ $PLUGIN_STATUS == 0 ]]; then
	echo "compiling vst"
	cmake -S $SRC_DIR -B $BUILD_DIR/build_JUCE -DBUILD_DEBUG=$config_debug -DBUILD_LOGGING=$config_logging
	cmake --build $BUILD_DIR/build_JUCE
    else
	echo "ERROR: plugin build failed. skipping vst compilation"
    fi
fi
VST_STATUS=$?

# create application bundle (.app on mac, .AppImage on linux), with nonstandard dependencies included
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

    cp $BUILD_DIR/granade Granade
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
    mkdir -p Granade.AppDir
    pushd Granade.AppDir > /dev/null    

    cp $DATA_DIR/Granade.desktop Granade.desktop

    cp $DATA_DIR/PNG/DENSITYPOMEGRANATE_BUTTON.png Granade.png

    cp $DATA_DIR/AppRun.sh AppRun.sh
    ln -sf ./AppRun.sh AppRun

    mkdir -p usr
    pushd usr > /dev/null

    cp -r $DATA_DIR data

    # TODO: is this necessary?
    mkdir -p share/icons/hicolor/512x512
    pushd share/icons/hicolor/512x512 > /dev/null
    cp $DATA_DIR/PNG/DENSITYPOMEGRANATE_BUTTON.png Granade.png
    popd > /dev/null # share/icons/hicolor/512x512 -> usr

    mkdir -p lib
    pushd lib > /dev/null    
    popd > /dev/null # lib -> usr    

    mkdir -p bin
    pushd bin > /dev/null

    cp $BUILD_DIR/granade Granade
    cp $BUILD_DIR/plugin.so plugin.so    

    # NOTE: copy dependencies (OpenGL, glfw, X11)
    DEPENDENCIES=$(ldd Granade | grep -oE "/(.+?)\\.so\\.[0-9]+" | grep -vE "(libc|libm|libdl|libpthread|ld-linux-)")
    for DEPENDENCY in $DEPENDENCIES
    do
	ABSOLUTE_SYMLINK=$(realpath "$DEPENDENCY")
	cp -P $DEPENDENCY ../lib
	cp $ABSOLUTE_SYMLINK ../lib
    done
    
    popd > /dev/null # bin -> usr    

    popd > /dev/null # usr -> Granade.AppDir

    popd > /dev/null # Granade.AppDir -> build

    # NOTE: uncomment if you want to make an appimage (need appimagetool)
    # appimagetool Granade.AppDir
    # cp Granade-*.AppImage ~/Applications
fi

popd > /dev/null # build -> src

exit $PLUGIN_STATUS
