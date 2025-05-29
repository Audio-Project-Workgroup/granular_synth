# How Granade works

Here are some notes on how Granade's code is organized. 

### Top-Level Directories

* `src` contains all the source code, including all the source code for the JUCE framework
* `data` contains audio files for the plugin to load
* building the project creates a new directory called `build`, which will contain the compiled executables, libraries, and plugins. See [Building](../../README.md#building-from-source)

### Translation Units

* `plugin.so/dll/dylib`: a dynamic library containing all of the plugin's audio-visual output logic. Compiled from `plugin.cpp`.
* `Granade.exe`: a simple host application for the dynamic library plugin, capable of hot-reloading. Compiled from `main.cpp`.
* `Granade.vst3`: a vst plugin that loads the dynamic library plugin. Compiled from `PluginProcessor.cpp` and `PluginEditor.cpp`.

### Misc.

* `common.h` defines the functions and data structures that the dynamic library plugin needs to interface with its host, as well as some shared code and definitions
* `platform.h` is a small OS abstraction layer used by the native host

## How it Works

* at startup, the host application (the native host, a DAW, the JUCE plugin host, etc.) opens a window with an OpenGL context, connects to audio input and output devices, and loads the dynamic libarary plugin.
* the dynamic library plugin exposes three functions: initializePluginState() initializes the plugin state, renderNewFrame() responds to keyboard/mouse input and outputs vertex data for the new video frame, and audioProcess() processes audio input and outputs samples to fill the audio output buffer.
* at runtime, the host calls the plugin functions. It passes them memory to store state, OS-dependent functions for things like file io and interlocked/atomic operations, and input information. It then copies the audio samples to the output device, and calls OpenGL to render the vertex data.
* all the logic for displaying an interactive UI, updating the state, and outputting audio is contained in the dynamic library. It is very small and only depends on the C-runtime-library, so it only takes a fraction of a second to recompile.
* in debug builds, the native host hot-reloads the the dynamic library plugin when it is recompiled. Thus you can edit the plugin, recompile it, and see the changes instantly without having to close and reopen the application window or otherwise manually retrigger a reload.
