#!/bin/bash

CFLAGS="-g -Wall -Wno-writable-strings -Wno-missing-braces -fno-exceptions -fno-rtti -std=gnu++11"

mkdir -p ../build
pushd ../build

#clang -c ../src/miniaudio_impl.c -o miniaudio.o
#ar rcs libminiaudio.a miniaudio.o

clang $CFLAGS ../src/plugin.cpp -o plugin.so -shared -fPIC -lm
clang $CFLAGS ../src/main.cpp -o test -L. -lGL -lglfw -lminiaudio -ldl -lpthread -lm

popd
