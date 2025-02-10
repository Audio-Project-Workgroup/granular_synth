cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

project(GranularSynthTest VERSION 0.0.1)

add_subdirectory(JUCE)

juce_add_plugin(
  ${PROJECT_NAME}
  IS_SYNTH FALSE #TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  NEEDS_CURL TRUE # on linux, juce tries to link with curl for some reason
  NEEDS_WEB_BROWSER TRUE # on linux, juce cant find gtk.h unless this flag is set
  PLUGIN_MANUFACTURER_CODE Test
  PLUGIN_CODE Test
  FORMATS VST3
  PRODUCT_NAME "Granular Synth Test"
)

get_filename_component(BASE_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)
set(BUILD_DIR ${BASE_DIR}/build/)

set(ONNX_MODEL_PATH ${BASE_DIR}/data/test_model.onnx)

set(ONNX_LIBRARY_DIR "${BASE_DIR}/src/libs")
set(ONNX_INCLUDE_DIR "${BASE_DIR}/src/include")
if(WIN32)
  set(ONNX_LIBRARY "${ONNX_LIBRARY_DIR}/onnxruntime.lib")
  set(ONNX_DLL "${BUILD_DIR}/onnxruntime.dll")
  
  message("juce fucks up automatically installing plugins on windows. you have to do it yourself :(")
  #set(JUCE_VST3_COPY_DIR ${USER_ROOT}AppData/Local/Programs/Common/VST3)
  #set(JUCE_COPY_PLUGIN_AFTER_BUILD ON)
elseif(APPLE)
  set(ONNX_LIBRARY "${ONNX_LIBRARY_DIR}/libonnxruntime.dylib")	
  set(JUCE_COPY_PLUGIN_AFTER_BUILD ON)
elseif(UNIX)
  set(ONNX_LIBRARY "${ONNX_LIBRARY_DIR}/libonnxruntime.so")
  set(JUCE_COPY_PLUGIN_AFTER_BUILD ON)	
endif()

if(WIN32)
  string(REPLACE "/" "\\\\" BUILD_DIR_NATIVE "${BUILD_DIR}")
else()
  set(BUILD_DIR_NATIVE ${BUILD_DIR})
endif()
message("${BUILD_DIR}")
message("${BUILD_DIR_NATIVE}")
message("${ONNX_LIBRARY_DIR}")
message("${ONNX_INCLUDE_DIR}")
message("${ONNX_LIBRARY}")
message("${ONNX_MODEL_PATH}")

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
  "BUILD_DIR=\"${BUILD_DIR}\""
  "BUILD_DIR_NATIVE=\"${BUILD_DIR_NATIVE}\""
  "ONNX_MODEL_PATH=\"${ONNX_MODEL_PATH}\"")

target_include_directories(
  ${PROJECT_NAME}
  PRIVATE
  ${ONNX_INCLUDE_DIR}
)

target_sources(
  ${PROJECT_NAME}
  PRIVATE
  PluginEditor.cpp
  PluginProcessor.cpp
)

#set(CMAKE_VERBOSE_MAKEFILE ON)
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
  juce_audio_utils
  juce_opengl
  ${ONNX_LIBRARY}
)

set(VST3_HELPER_DIRECTORY "${CMAKE_BINARY_DIR}/Debug")
set(VST3_HELPER_PATH "${VST3_HELPER_DIRECTORY}/juce_vst3_helper.exe")
set(VST3_OUTPUT_DIR "${CMAKE_BINARY_DIR}/GranularSynthTest_artefacts/${CMAKE_BUILD_TYPE}/VST3/Granular Synth Test.vst3")
set(MODULEINFO_RESOURCES_DIR "${VST3_OUTPUT_DIR}/Contents/Resources")
set(MODULEINFO_PATH "${MODULEINFO_RESOURCES_DIR}/moduleinfo.json")

if(WIN32)
  set(VST3_BIN_DIR "${VST3_OUTPUT_DIR}/Contents/x86_64-win")
  message("copying ${ONNX_DLL} to ${VST3_BIN_DIR}") 
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    # copy to build output directory
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNX_DLL}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    # copy to VST3 component directory
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNX_DLL}"
    "${VST3_BIN_DIR}"
    # copy to juce_vst3_helper.exe directory
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNX_DLL}"
    "${VST3_HELPER_DIRECTORY}"
  )
endif()
