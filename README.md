# Granular Synthesizer

## Code Structure

* the native host application is compiled from main.cpp
* the dynamic library plugin is compiled from plugin.cpp
* the JUCE vst3 plugin is compiled from PluginProcessor.cpp and PluginEditor.cpp
* common.h contains the code that the dynamic library plugin needs to interface with the native host and JUCE
* platform.h is a small OS abstraction layer used by the native host (windows functions are untested)

## How it Works

* at startup, the host application (the native host, a DAW, the JUCE plugin host, etc.) opens a window with an OpenGL context, connects to an output audio playback device, and loads the dynamic libarary plugin.
* the dynamic library pugin exposes two functions: renderNewFrame() outputs vertex data for the new video frame, and audioProcess() outputs samples to fill the audio output buffer.
* at runtime, the host calls the plugin functions. It passes them a function to open and read files, memory to store state and load files into, and a description of the current keyboard and mouse input state. It then copies the audio samples to the output device, and renders the vertex data with OpenGL.
* all the logic for displaying an interactive UI, updating the state, and outputting audio is contained in the dynamic library, which is very small and only depends on the C-runtime-library, so it only takes a fraction of a second to recompile.
* the native host hot-reloads the the dynamic library plugin as it is recompiled. Thus you can edit the plugin, recompile it, and see the changes instantly without having to close and reopen the application window or otherwise manually retrigger a reload.

## Requirements

* GLFW: https://github.com/glfw/glfw
* clang (mac and linux)

## Building

### Windows(untested)

* navigate to the src directory
* open main.cpp and uncomment the line with #define MINIAUDIO_IMPLEMENTATION
* run build.bat to build the native host application and the dynamic library plugin. These will be located in a new directory called build
* run build_juce.bat to build the vst3 plugin with JUCE and install it

### Mac(untested) and Linux

* navigate to the src directory
* open build.sh and uncomment the two commented lines
* run build.sh to build the native host application and the dynamic library plugin. These will be located in a new directory called build
  * append your local build directory path to DYLD_LIBRARY_PATH (mac) or LD_LIBRARY_PATH (linux)
  * recomment those two lines in build.sh for subsequent builds
* run build_juce.sh to build the vst3 plugin with JUCE and install it

## Issues

* Windows build.bat does not have a way of building miniaudio as a static library
* JUCE requires a local path to the data directory to load files
* loading the dynamic library plugin requires modifying shell environment variables on mac and linux (maybe windows too?)