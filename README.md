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
- [Todo List](#todo-list)
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

	4. **Build the VST3 plugin with JUCE:**

	```bash
	cd src
	./build_juce.sh
	``` 

## RUN
Test the installation for the application and for the VST plugin.

* To run the application find the executable within the build directory. The file names are `main.exe` for Windows and `test` for Linux. (TODO: real names)
* To test the VST3 plugin, find the `Granular Synth Test.vst3` file, and import it in your DAW.

## Issues

* vst3 automatic installation doesn't work anymore
* must put onnxruntime dlls everywhere on windows. We don't want to make users do that, so we should put a dll with a custom name (to not conflict with the default) in System32.
* should automatically detect if we need to build a miniaudio library, and not need to comment and uncomment crap
* should condense builds into a single script, with optional arguments
