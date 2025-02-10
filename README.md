# Granular Synthesizer

## Code Structure

### Top-Level Directories

* `src` contains all the source code, including all the source code for the JUCE framework
* `data` contains audio files for the plugin to load
* building the project creates a new directory called `build`, which will contain the compiled executables, libraries, and plugins. See [Building](#building)

### Translation Units(TODO: real names)

* `plugin.so/dll/dylib`: a dynamic library containing all of the plugin's audio-visual output logic. Compiled from `plugin.cpp`.
* `test/main.exe`: a simple host application for the dynamic library plugin, capable of hot-reloading. Compiled from `main.cpp`.
* `Granular Synth Test.vst3`: a vst plugin that loads the dynamic library plugin. Compiled from `PluginProcessor.cpp` and `PluginEditor.cpp`.

### Misc.

* `common.h` defines the functions and data structures that the dynamic library plugin needs to interface with the native host and JUCE, as well as some shared code and definitions
* `platform.h` is a small OS abstraction layer used by the native host

## How it Works

* at startup, the host application (the native host, a DAW, the JUCE plugin host, etc.) opens a window with an OpenGL context, connects to an output audio playback device, and loads the dynamic libarary plugin.
* the dynamic library pugin exposes two functions: renderNewFrame() outputs vertex data for the new video frame, and audioProcess() outputs samples to fill the audio output buffer.
* at runtime, the host calls the plugin functions. It passes them a function to open and read files, memory to store state and load files into, and a description of the current keyboard and mouse input state. It then copies the audio samples to the output device, and renders the vertex data with OpenGL.
* all the logic for displaying an interactive UI, updating the state, and outputting audio is contained in the dynamic library, which is very small and only depends on the C-runtime-library, so it only takes a fraction of a second to recompile.
* the native host hot-reloads the the dynamic library plugin as it is recompiled. Thus you can edit the plugin, recompile it, and see the changes instantly without having to close and reopen the application window or otherwise manually retrigger a reload.

## Requirements

* GLFW: https://github.com/glfw/glfw (to build from source), https://www.glfw.org/download.html (for the precompiled binaries)
* ONNX: https://github.com/microsoft/onnxruntime (source), https://github.com/microsoft/onnxruntime/releases/tag/v1.20.1 (releases)
* OpenGL (mac and linux)
* clang (mac and linux)
* msvc (windows)

## Building

* clone the repo with `git clone https://github.com/Audio-Project-Workgroup/granular_synth`
* download the `JUCE` submodule with:
```
cd granular-synth
git submodule update --init --recursive
``` 
This places the `https://github.com/juce-framework/JUCE` repository within `src/JUCE` directory.

### Windows 

* from your downloaded GLFW folder:
  * copy the `GLFW` folder to `src\include`
  * from the `lib-vc[YEAR]` folder (corresponding to your version of Visual Studio), copy `glfw3_mt.lib` and `glfw3.lib` to `src\libs`
* from your downloaded onnxruntime folder:
  * from the `include` directory, copy the file `onnxruntime_c_api.h` to `src\include`
  * from the `lib` directory, copy the files `onnxruntime.lib` and `onnxruntime.pdb` to `src\libs`, and copy the file `onnxruntime.dll` to the `build` directory (if this is your first build, that directory won't be there yet, so defer this step until after you run `build.bat`)
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
* run `build_juce` to build the vst3 plugin with JUCE
  * you will have to put the resulting vst3 bundle somewhere where your DAW/plugin host can find it (TODO: get JUCE to do the installation auomatically)
  * on your first build, you will also have to copy `onnxruntime.dll` from your `build` folder to the folder containing your DAW's executable (TODO: install the onnxruntime dll globally with a custom name (to not interfere with the one that's installed by default))

### Mac and Linux (TODO: update for building with ONNX)

* navigate to the src directory
* open `build.sh` and uncomment the two lines to compile miniaudio to a static library
* run `./build.sh` to build the native host application and the dynamic library plugin. These will be located in a new directory called build
  * (NOTE: this may no longer be necessary) append your local build directory path to `DYLD_LIBRARY_PATH` (mac) or `LD_LIBRARY_PATH` (linux)
  * recomment those two lines in `build.sh` for subsequent builds
* run `./build_juce.sh` to build the vst3 plugin with JUCE and install it

## RUN
Test the installation for the application and for the VST plugin.

* To run the application find the executable within the build directory. The file names are `main.exe` for Windows and `test` for Linux. (TODO: real names)
* To test the VST3 plugin, find the `Granular Synth Test.vst3` file, and import it in your DAW.

## Issues

* vst3 installation must be done manually on windows
* must put onnxruntime dlls everywhere on windows. We don't want to make users do that, so we should put a dll with a custom name (to not conflict with the default) in System32.
* (FIXED) double-free bug when closing vst plugin
* (NOTE: probably fixed) loading the dynamic library plugin requires modifying shell environment variables on mac and linux (and requires a local path on windows)