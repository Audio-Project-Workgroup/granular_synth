#!/bin/bash

./build.sh
cmake -S . -B ../build/build_JUCE
cmake --build ../build/build_JUCE
