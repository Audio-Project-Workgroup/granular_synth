# Granade - an Open Source Granular Synth

<img src="data\granadeUI.png" alt="Granular Synthesizer UI" width="500" />

[![Version](https://img.shields.io/badge/version-0.1.0-blue)](https://github.com/Audio-Project-Workgroup/granular_synth)
![Build Status](https://img.shields.io/github/workflow/status/Audio-Project-Workgroup/granular_synth/CI)
[![License](https://img.shields.io/badge/license-MIT-green)](https://github.com/Audio-Project-Workgroup/granular_synth/tree/readme?tab=License-1-ov-file)
[![Year](https://img.shields.io/badge/year-2025-brightgreen)](https://github.com/Audio-Project-Workgroup/granular_synth)

The **Granade** is a granular synth that deconstructs sound into microscopic *"grains"* and manipulates them in real-time to create complex textures and sonic landscapes. By controlling parameters such as grain size, density, pitch, and position, you can transform any audio source into an entirely new sonic experience. 

Granade provides both a standalone application and a VST3 plugin.

## Contents

- [Key Features](#key-features)
- [Installation](#installation)
	- [Download Releases](#download-releases)
	- [Requirements](#requirements)
	- [Building from Source](#building-from-source)
- [Quick Start Guide](#quick-start-guide)
- [FAQ](#faq)
- [Future Work](#future-work)
- [Release Notes](#release-notes)
- [Acknowledgments](#acknowledgments)

## Key Features

- Real-time granular synthesis
- Advanced grain control: size, density, position, pitch, envelope, and spatial distribution
- Cross-platform support (Windows, macOS, Linux)

## Installation

### Download Releases

The easiest way to get Granade is to download pre-built releases from our [GitHub Releases page](https://github.com/Audio-Project-Workgroup/granular_synth/releases).

### Requirements

For building from source, you'll need:

* GLFW: https://github.com/glfw/glfw (to build from source), https://www.glfw.org/download.html (for the precompiled binaries)
<!--* ONNX: https://github.com/microsoft/onnxruntime (source), https://github.com/microsoft/onnxruntime/releases/tag/v1.20.1 (releases)-->
* OpenGL
* CMake (for vst)
* msvc (windows)
* clang (mac and linux)
* pkg-config (mac and linux)

### Building from Source

1. Clone the repository and initialize submodules:
	```bash
	git clone --recurse-submodules https://github.com/Audio-Project-Workgroup/granular_synth
	```
	This places the `https://github.com/juce-framework/JUCE` repository within `src/JUCE` directory.

2. Platform-specific setup:
	
	#### Windows 

	1. **Set up GLFW:**
		- Copy the `GLFW` folder from your downloaded GLFW package to `src\include`
		- From the `lib-vc[YEAR]` folder (corresponding to your version of Visual Studio), copy `glfw3_mt.lib` and `glfw3.lib` to `src\libs`
	
	2. **Configure your shell to be able to call a C compiler from the command line:**
		- Open a Developer Command Prompt or run (i.e. for x64 architectures):
		```batch
		"C:\Program Files\Microsoft Visual Studio\[YEAR]\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
		```

	3. **Build the native host application and the dynamic library plugin:**
		```
		cd src
		.\build.bat
		```
		These will be located in a new directory called `build` (with names `main.exe` and `plugin.dll`)

	4. **Build the VST3 plugin with JUCE:**
		```batch
		cd src
		.\build_juce.bat
		```
		you will have to put the resulting vst3 bundle somewhere where your DAW/plugin host can find it (@TODO: get JUCE to do the installation auomatically)

	<!-- Uncomment in the future in case that onnx is supported.
	N. Set up ONNX Runtime:
		- copy the file `onnxruntime_c_api.h` from the ONNX include directory to `src\include`
		- copy `onnxruntime.lib` and `onnxruntime.pdb` to `src\libs`
		- copy `onnxruntime.dll` to the `build` directory (if this is your first build, that directory won't be there yet, so defer this step until after you run `build.bat`)

	NOTE:
		on your first build, you will also have to copy `onnxruntime.dll` from your `build` folder to the folder containing your DAW's executable (TODO: install the onnxruntime dll globally with a custom name (to not interfere with the one that's installed by default)) -->

	#### Mac and Linux
	
	1. **Install the the [required dependencies](#requirements) via your package manager

	2. **Set up environment paths (optional):**

	* on mac, you may need to set/modify your shell's include, library, and pkg-config paths. For example:
	```
	# ~/.zshrc
	# replace /opt/homebrew with /usr/local on intel macs
	export CPATH=/opt/homebrew/include # append to this path if it exists already
	export LIBRARY_PATH=/opt/homebrew/lib # append to this path if it exists already
	export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
	```
	<!--
	* on linux, onnx is not available via standard package managers. Download the binaries (recommended version is 1.20.1) and install them globally (the exact directories can be found in the libonnxruntime.pc file, located in `[onnxruntime folder]/lib/pkgconfig`) -->

	3. **Build the native host application and the dynamic library plugin**
	
	```bash
	cd src
	./build.sh
	``` 
	The compiled native host application and the dynamic library plugin will be located in a new directory called build

	Test the installation by running the executable:

	- in Windows: Run `build\Granade .exe`
	- in macOS/Linux: Run `./build/Granade`

	4. **Build the VST3 plugin with JUCE:**

	```bash
	cd src
	./build_juce.sh
	``` 
	The `Granade.vst3` file will be created into a nested directory within the build directory (i.e. within `granular_synth\build\build_JUCE\Granade_artefacts\Debug\VST3` for Windows builds). *Make sure it is visible by your DAW by either moving it inside the default VST directory, or adding the parent directory in the VST3 plugins scanned directories of your DAW.*

## Quick Start Guide

*Granade is in active development. Some features shown may not be fully implemented yet.*

#### Audio Input Sources

- **Standalone:** Uses microphone input
- **VST:** Works as audio effect

#### Controls
Parameter | Description
:--- | ---:
Spread or width | How much opened in stereo are the grains played
Offset | How many samples between the grain buffer read and write indices (not applicable to file-backed grains)disc
Size | How many samples a grain plays (not applicable to file-backed grains)
Density |	How many grains are playing at once, or how long until a new grain starts playing 
Mix | Blend between the original signal and the signal with with the granulator. Being 0% the original signal only and 100% only the grains. (Efficient for separate the effect from the signal and easier to mix) 
Pan | Panning of the out signal
Window | Control over a windowing function applied to grain samples
Level | The level the grains are played independently

#### Start Exploring

Feel free to experiment - Granade is destined for artistic exploration and creative discovery!

## FAQ

<details>
<summary><strong>Is Granade a cross-platform project? Does it support both standalone apps and VST3 plugins for each platform?</strong></summary>
<br>
Yes it is. And so it does!
</details>

<details>
<summary><strong>What makes Granade different from other granular synthesizers?</strong></summary>
<br>
While inspired by existing granular synths, Granade brings its own unique approach  to granular synthesis.
</details>

<details>
<summary><strong>Is Granade ready for production use?</strong></summary>
<br>
Granade is currently in active development. While functional for experimentation and creative use, some features are incomplete. We recommend it for creative exploration rather than critical production work at this stage.
</details>

<details>
<summary><strong>Can I load my own audio files?</strong></summary>
<br>
File loading is supported in the codebase under the hood but the UI control is not yet implemented. Currently, the standalone application uses microphone input and the VST processes audio directly from your DAW tracks.
</details>

<details>
<summary><strong>Some controls in the interface don't seem to work. Is this a bug?</strong></summary>
<br>
No, this is expected. Granade is in active development and not all visible controls are fully functional yet. For more details, see <a href="#future-work">Future work</a>.
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
	- [ ] allow specification of horizontal vs. vertical slider, or knob for draggable elements
	- [ ] collapsable element groups, sized relative to children
	- [ ] tooltips when hovering (eg show numeric parameter value)
	- [ ] display the state of grains and the grain buffer (wip)
- [ ] full state (de)serialization (not just parameters, also grain buffer)
- File Loading: (on hold):
	- [ ] speed up reads (ie SIMD)
	- [ ] better samplerate conversion (ie FFT method (TODO: speed up fft and czt))
- MIDI: 
	- [ ] cc channel - parameter remapping at runtime
- Misc:
	- [ ] speed up fft (aligned loads/stores, different radix?)
	- [ ] work queues?
	- allocate hardware ring buffer?
- Enhance development practices:
	- [ ] static analysis
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
- Extend File Loading (on hold): 
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

- Born from connections made at [ADC 24](https://audio.dev/archive/adc24-bristol/), Granade exists thanks to the Audio-Project-Workgroup's initiative to unite developers with shared interests. Special appreciation to all who contributed their time into it.