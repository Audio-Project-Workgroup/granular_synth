#!/bin/bash

echo "WARNING: this script has been deprecated. run `./build.sh target:vst` instead"

./build.sh

PLUGIN_STATUS=$?

if [ $PLUGIN_STATUS -eq 0 ]; then
    cmake -S . -B ../build/build_JUCE #-DJUCE_BUILD_EXTRAS=ON
    cmake --build ../build/build_JUCE #--target AudioPluginHost
else
    echo ERROR: plugin compilation failed. build terminated.
fi
