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

* GLFW: https://github.com/glfw/glfw (to build from source), https://www.glfw.org/download.html (for the precompiled binaries)
* OpenGL (mac and linux)
* clang (mac and linux)
* msvc (windows)

## Building

### Windows 

* clone the repo with `git clone https://github.com/Audio-Project-Workgroup/granular_synth`
* download the `JUCE` submodule with:
```
cd granular-synth
git submodule update --init --recursive
``` 
This places the `https://github.com/juce-framework/JUCE` repository within `src/JUCE`.
* from your downloaded GLFW folder, copy the `GLFW` folder to `src\include`, and from the `lib-vc[YEAR]` folder (corresponding to your version of Visual Studio), copy `glfw3_mt.lib` and `glfw3.lib` to `src\libs`
* configure your shell to be able to call a C compiler from the command line. This can be done by calling `vcvarsall.bat x64`, which you can find in `C:\Program Files\Microsoft Visual Studio\[YEAR]\Community\VC\Auxiliary\Build`, or something similar. 
* navigate to the src directory
* open `build.bat` and uncomment the two lines to compile miniaudio to a static library (by removing the REM command from the statements), as follows:
```
REM compile miniaudio to static library
cl %CFLAGS% -c ..\src\miniaudio_impl.c
lib -OUT:miniaudio.lib miniaudio_impl.obj
```
* run `build` to build the native host application and the dynamic library plugin. These will be located in a new directory called `build` (with names `main.exe` and `plugin.dll`)
  * recomment the two lines in `build.bat` for subsequent builds
* open `PluginProcessor.cpp`, and change two variables (NOTE: these are temporary measures that should be fixed soon):
  * change the local variable `fileDirectory` in the `juceReadEntireFile` function to your local data directory
  * change the local variable `pluginDirectory` in the `prepareToPlay` function to your local build directory
* run `build_juce` to build the vst3 plugin with JUCE
  * you will have to put the resulting vst3 bundle somewhere where your DAW/plugin host can find it

### Mac(untested) and Linux

* navigate to the src directory
* open `build.sh` and uncomment the two lines to compile miniaudio to a static library
* run `./build.sh` to build the native host application and the dynamic library plugin. These will be located in a new directory called build
  * append your local build directory path to `DYLD_LIBRARY_PATH` (mac) or `LD_LIBRARY_PATH` (linux)
  * recomment those two lines in `build.sh` for subsequent builds
* open `PluginProcessor.cpp`, and change the local variable `fileDirectory` in the `juceReadEntireFile` function to your local data directory (NOTE: this is a temporary measure that should be fixed soon)
* run `./build_juce.sh` to build the vst3 plugin with JUCE and install it

## RUN
* Test the installation by running the application executable from within the build directory.
 - main.exe # for Windows
 - test # for Linux
* Test the VST3 plugin by importing the vst3 file wihin your DAW.

## Issues

* vst3 installation must be done manually on windows
* JUCE requires a local path to the data directory to load files
* loading the dynamic library plugin requires modifying shell environment variables on mac and linux (and requires a local path on windows)