cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

project(GranularSynthTest VERSION 0.0.1)

if(WIN32)
	message("juce fucks up automatically installing plugins on windows. you have to do it yourself :(")
	#set(JUCE_VST3_COPY_DIR ${USER_ROOT}AppData/Local/Programs/Common/VST3)
	#set(JUCE_COPY_PLUGIN_AFTER_BUILD ON)	
else()
	set(JUCE_COPY_PLUGIN_AFTER_BUILD ON)	
endif()

add_subdirectory(JUCE)

juce_add_plugin(${PROJECT_NAME}
	IS_SYNTH TRUE
	NEEDS_MIDI_INPUT TRUE
	NEEDS_MIDI_OUTPUT FALSE
	NEEDS_CURL TRUE			# on linux, juce tries to link with curl for some reason
	NEEDS_WEB_BROWSER TRUE  	# on linux, juce cant find gtk.h unless this flag is set
	PLUGIN_MANUFACTURER_CODE Test
	PLUGIN_CODE Test
	FORMATS VST3
	PRODUCT_NAME "Granular Synth Test")

target_sources(${PROJECT_NAME}
	PRIVATE
	PluginEditor.cpp
	PluginProcessor.cpp)

target_link_libraries(${PROJECT_NAME}
	PRIVATE	
	juce_audio_utils
	juce_opengl)
