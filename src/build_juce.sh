#!/bin/bash

LIBRARY_PATH=$(./build.sh)
echo $LIBRARY_PATH

cmake -S . -B ../build/build_JUCE # -DJUCE_BUILD_EXTRAS=ON
cmake --build ../build/build_JUCE # --target AudioPluginHost
