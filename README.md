# Granade - an Open Source Granular Synth

<img src="data\granadeUI.png" alt="Granular Synthesizer UI" width="500" />

[![Version](https://img.shields.io/badge/version-0.1.0-blue)](https://github.com/Audio-Project-Workgroup/granular_synth)
[![License](https://img.shields.io/badge/license-MIT-green)](https://github.com/Audio-Project-Workgroup/granular_synth/tree/readme?tab=License-1-ov-file)
[![Year](https://img.shields.io/badge/year-2025-brightgreen)](https://github.com/Audio-Project-Workgroup/granular_synth)
<!-- temporarily hide until we develop the ci pipeline
![Build Status](https://img.shields.io/github/workflow/status/Audio-Project-Workgroup/granular_synth/CI)
-->
![Linux](https://github.com/Audio-Project-Workgroup/granular_synth/workflows/Linux/badge.svg)
![MacOs](https://github.com/Audio-Project-Workgroup/granular_synth/workflows/macOS/badge.svg)
![Windows](https://github.com/Audio-Project-Workgroup/granular_synth/workflows/Windows/badge.svg)

Granade is a real-time granular synthesizer, available as a vst3 plugin and as a standalone application for mac, windows, and linux. Granade records audio input into a buffer and deconstructs the sound into multiple overlapping delayed, enveloped, and panned segments called grains, creating complex textures and new sonic landscapes. Several interactive parameters such as grain size, density, spread, and windowing, are provided for manipulating grain playback in real time. 

## Contents

- [Key Features](#key-features)
- [Installation](#installation)
	- [Download Releases](#download-releases)
	- [Building from Source](#building-from-source)
		- [Requirements](#install-requirements)
		- [Initialize local repo](#Clone-the-repository-and-initialize-submodules)
		- [Platform-specific-setup](#Platform-specific-setup)
		- [Verify build](#Verify-your-build)
- [Documentation](#documentation)
- [Quick Start Guide](#quick-start-guide)
- [FAQ](#faq)
- [Future Work](#future-work)
- [Release Notes](#release-notes)
- [Acknowledgments](#acknowledgments)

## Key Features

- Real-time granular synthesis
- Advanced grain control: size, density, spread, windowing, panning, and mix controls
- Cross-platform support (Windows, macOS, Linux)

## Installation

### Download Releases

The easiest way to get Granade is to download pre-built releases from our [GitHub Releases page](https://github.com/Audio-Project-Workgroup/granular_synth/releases).

<!--
Skip these notes
**NOTE TO WINDOWS USERS**
The relative locations of files/directories in the folder you downloaded is critical for the application to function.
If you want to move the executable to a more convenient location, either move the entire folder, or create an alias
to the executable. Appologies for not shipping an installer.

**NOTE TO LINUX USERS**
We distribute Granade as an AppImage on linux. We recommend using appimaged
(download: github.com/probonopd/go-appimage) to automate the integration of AppImages into the desktop environment. 
Once you have appimaged running, copy Granade to ~/Applications and appimaged will handle the rest.
-->

### Building from Source

#### Install requirements

For building from source, you'll need:

* GLFW: [source](https://github.com/glfw/glfw), [binaries](https://www.glfw.org/download.html)
<!--* ONNX: https://github.com/microsoft/onnxruntime (source), https://github.com/microsoft/onnxruntime/releases/tag/v1.20.1 (releases)-->
* OpenGL
* CMake (for vst)
* msvc (windows)
* clang (mac and linux)
<!--* pkg-config (mac and linux)-->

linux users will also need the development versions (ie with headers) of all packages JUCE requires for building VSTs (eg gtk, asound, ...).

#### Clone the repository and initialize submodules: 
``` bash
git clone --recurse-submodules https://github.com/Audio-Project-Workgroup/granular_synth	
```
This places the `https://github.com/juce-framework/JUCE` repository within `src/JUCE` directory.
#### Platform-specific setup:
	
##### Windows 

1. **Set up GLFW:**
	- Copy the `GLFW` folder from your downloaded GLFW package to `src\include`
	- From the `lib-vc[YEAR]` folder (corresponding to your version of Visual Studio), copy `glfw3_mt.lib` and `glfw3.lib` to `src\libs`

2. **Configure your shell to be able to call a C compiler from the command line:**
	- Open a Developer Command Prompt and run (e.g. for x64 architectures):
	```batch
	"C:\Program Files\Microsoft Visual Studio\[YEAR]\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
	```

3. **Build the native host application and the dynamic library plugin:**
	```batch
	cd src
	build
	```
	These will be located in a new directory called `build` (with names `Granade.exe` and `plugin.dll`)

4. **Build the VST3 plugin with JUCE:**
	```batch
	cd src
	build_juce
	```
	you will have to put the resulting vst3 bundle (located in `granular_synth\build\build_JUCE\Granade_artefacts\Debug\VST3`) somewhere where your DAW/plugin host can find it (@TODO: get JUCE to do the installation automatically)

<!--
Uncomment in the future in case that onnx is supported.
N. Set up ONNX Runtime:
	- copy the file `onnxruntime_c_api.h` from the ONNX include directory to `src\include`
	- copy `onnxruntime.lib` and `onnxruntime.pdb` to `src\libs`
	- copy `onnxruntime.dll` to the `build` directory (if this is your first build, that directory won't be there yet, so defer this step until after you run `build.bat`)

NOTE:
	on your first build, you will also have to copy `onnxruntime.dll` from your `build` folder to the folder containing your DAW's executable (TODO: install the onnxruntime dll globally with a custom name (to not interfere with the one that's installed by default)) 
-->

##### Mac and Linux

1. **Install the [required dependencies](#install-requirements) via your package manager**

2. **Set up environment paths (optional):**
	- on mac, you may need to set/modify your shell's include, library, and pkg-config paths. For example:
   ```bash
   # ~/.zshrc
   # replace /opt/homebrew with /usr/local on intel macs
   export CPATH=/opt/homebrew/include # append to this path if it exists already
   export LIBRARY_PATH=/opt/homebrew/lib # append to this path if it exists already
   export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
   ```

<!--
	on linux, onnx is not available via standard package managers. Download the binaries (recommended version is 1.20.1) and install them globally (the exact directories can be found in the libonnxruntime.pc file, located in `[onnxruntime folder]/lib/pkgconfig`) 
-->

3. **Build the native host application and the dynamic library plugin**

```bash
cd src
./build.sh
``` 
The compiled native host application and the dynamic library plugin will be located in a new directory called `build`

4. **Build the VST3 plugin with JUCE:**

```bash
cd src
./build_juce.sh
``` 
The `Granade.vst3` file will be created into a nested directory within the build directory (i.e. within `granular_synth\build\build_JUCE\Granade_artefacts\Debug\VST3`). 


#### Verify your build

##### Standalone application:

To test the standalone application, simply:
- in Windows: Run `build\Granade`
- in macOS/Linux: Run `./build/Granade`

##### VST3 plugin:

To test the VST3 plugin, make sure it is visible by your DAW by either moving it inside the default VST directory, or adding the parent directory of the VST3 bundle to your DAW's list of scanned directories.

## Documentation

Some files include comments explaining implementation details. Documentation coverage is currently limited but being expanded. For more technical information, visit [How Granade Works](data\docs\HOW_GRANADE_WORKS.md).

## Quick Start Guide

*Granade is a working granular synthesizer with room for future development. Some interface elements may not yet be fully functional.*

#### Audio Input Sources

- **Standalone:** Uses microphone input
- **VST:** Works as audio effect

#### Controls

- Left click and drag a knob or slider up or down to change its value.
- You can also navigate the ui with tab and backspace. Use + and - on a selected control to change parameter values.
- Pressing ESC opens a menu to select audio input and output devices with the mouse. Press ESC again to close.

#### Parameters

Parameter | Description | How to Use
:-- | :--: | --:
Density | Controls the number of playing grains | Turn left for fewer grains, and right for more.
Spread | Controls the random stereo widening applied to each grain | Fully left is dual mono, middle is unmodified, right is wider.
Offset | Controls the space between the read and write heads of the buffer | Turn left for less delay, and right for more.
Size | Controls the size of new grains | Left is shorter, right is longer.
Mix | Controls the dry/wet mix |Fully left is completely dry, fully right is completely wet.
Pan | Controls the pan of the wet signal | Left is left, right is right.
Window | Controls the amplitude window shape applied to each grain | Turn left for a smoother ramp, and right for a choppier sound
Level | Controls the overall amplitude of the output signal | Up for louder, down for quieter

#### Start Exploring

Feel free to experiment - Granade is designed for artistic exploration and creative discovery!

## FAQ

<details>
<summary><strong>Is Granade a cross-platform project? Does it support both standalone apps and VST3 plugins for each platform?</strong></summary>
<br>
Yes it is, and so it does! You can download <a href="https://github.com/Audio-Project-Workgroup/granular_synth/releases">precompiled binaries</a> for macOS (arm), Windows (x64), and Linux (x64). Other combinations of OS and architecture are supported via <a href="#building-from-source">building from source</a>.

</details>

<details>
<summary><strong>What makes Granade different from other granular synthesizers?</strong></summary>
<br>
While inspired by existing granular synths, Granade brings its own unique approach to granular synthesis. Therefore, it's not novel, but unique.
</details>

<details>
<summary><strong>Is Granade ready for production use?</strong></summary>
<br>
Granade is in a nascent stage of development, developed by a small team, and only tested on a handful of systems. As such, you may experience crashes, bugs, or be missing certain features. In any of these cases, please let us know by submitting an <a href="https://github.com/Audio-Project-Workgroup/granular_synth/issues">issue</a> (bug report or feature request) on github. While functional for experimentation and creative use, some features are incomplete. We recommend it for creative exploration rather than critical production work at this stage.
</details>

<details>
<summary><strong>Can I load my own audio files?</strong></summary>
<br>
File loading is supported in the codebase under the hood but the UI control is not yet implemented. Currently, the standalone application uses microphone input and the VST processes audio directly from your DAW tracks.
</details>

<details>
<summary><strong>Some controls in the interface don't seem to work. Is this a bug?</strong></summary>
<br>
No, this is expected. Granade is in its first release and not all visible controls are fully functional yet. For more details, see <a href="#future-work">Future work</a>.
</details>

<details>
<summary><strong>Does Granade use machine learning for audio processing?</strong></summary>
<br>
Not currently. The codebase includes ONNX runtime integration to support potential ML features, but the current audio processing pipeline is purely algorithmic. ML inclusion for granular synthesis is on our <a href="#future-work">future development</a> list.

</details>

<details>
<summary><strong>How can I speed up build times after the first compilation?</strong></summary>
<br>
After your first successful build, you can comment out the miniaudio compilation lines in your platform's build script to significantly reduce recompilation time:

*Windows (`build.bat`):*
```batch
REM compile miniaudio to static library
REM cl %CFLAGS% -c ..\src\miniaudio_impl.c
REM lib -OUT:miniaudio.lib miniaudio_impl.obj

```
*macOS/Linux (`build.sh`):*
```bash
# compile miniaudio to static library
# clang -c ../src/miniaudio_impl.c -o miniaudio.o
# ar rcs libminiaudio.a miniaudio.o
```
</details>

## Future Work

### Bugs/Issues
<!-- 
- [ ] for empty items
- [x] for accomplished items
 -->

This is a list of bugs and issues that require fixing:

- [ ] vst3 automatic installation into default VST3 directory
- [ ] make parameters visible to fl studio last tweaked automation menu
	- [ ] per-grain level
	- [ ] modulation
- [ ] linux hot reloading sometimes fails (recompiling the executable at runtime sometimes crashes the application)

### Improvements

This is a list of existing features that require curation and improvement:

- [ ] automatically detect if we need to build a miniaudio library, discarding the need of commenting out compilation instructions
- [ ] condense builds into a single script, with optional arguments
- Improve UI:
	- [ ] improve font rendering
	- [ ] tooltips when hovering (eg show numeric parameter value)
	- [ ] improve grain buffer state display
	- [ ] support modifying parameters with mouse wheel scrolling
- [ ] full state (de)serialization (not just parameters, also grain buffer)
- File Loading:
	- [ ] speed up reads
	- [ ] better samplerate conversion
- MIDI: 
	- [ ] cc channel - parameter remapping at runtime
- Enhance development practices:
	- [ ] static analysis
	- [ ] hook a fuzzer up to the input for resilience testing
	- [ ] remove plugin dependence on crt for maximum portability
- Optimize codebase:
	- [ ] profiling
	- [ ] simd everywhere
 
### Additions

This is a list of missing features for concrete Granade implementation, or envisioned features that aim to highly leverage Granade:

- Implement missing grain controls:
	- [ ] Pitch-Shift
	- [ ] Time-Stretch
	- [ ] Modulation
- Extend UI:
	- [ ] interface for selecting files to load at runtime.
	- [ ] interface for remapping midi cc channels with parameters.
- Extend File Loading: 
	- Support more audio file formats 
		- [ ] flac
		- [ ] ogg
		- [ ] mp3
		- [ ] ...
	- [ ] (ML depended) load in batches to pass to ML model
- [ ] save and restore plugin state

- [ ] screen reader support
- [ ] Integrate ML into the pipeline for grouping grains

## Release Notes

### 0.1.0 (Initial Release) - 1st June 2025
Initial release with real-time granular synthesis, cross-platform support, and VST3 plugin.

---

*For a complete changelog of all versions, see [CHANGELOG.md](data/docs/CHANGELOG.md)*


## Acknowledgments

- Born from connections made at [ADC 24](https://audio.dev/archive/adc24-bristol/), Granade exists thanks to the [Audio-Project-Workgroup](https://github.com/Audio-Project-Workgroup)'s initiative to unite developers with shared interests under the scope of music and IT. Special appreciation to all the people who contributed their time to it.
