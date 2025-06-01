cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

project(Granade VERSION 1.0.0)
set(PLUGIN_NAME "Granade")

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
  COMPANY_NAME ${PLUGIN_NAME}
  PRODUCT_NAME ${PLUGIN_NAME}
)

get_filename_component(BASE_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)
set(BUILD_DIR "${BASE_DIR}/build")
set(DATA_DIR "${BASE_DIR}/data")

# set(ONNX_MODEL_PATH ${BASE_DIR}/data/test_model.onnx)

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

  set(DYNAMIC_PLUGIN_NAME "plugin.dll")  

  # set(ONNX_LIBRARY_DIR "${BASE_DIR}/src/libs")
  # set(ONNX_INCLUDE_DIR "${BASE_DIR}/src/include")
  # set(ONNX_LIBRARY "${ONNX_LIBRARY_DIR}/onnxruntime.lib")
  # set(ONNX_DLL "${BUILD_DIR}/onnxruntime.dll")
  
  message("juce fucks up automatically installing plugins on windows. you have to do it yourself :(")
  #juce_enable_copy_plugin_step(${PROJECT_NAME})
elseif(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")

  set(DYNAMIC_PLUGIN_NAME "plugin.dylib")

  #set(ONNX_LIBRARY "${ONNX_LIBRARY_DIR}/libonnxruntime.dylib")

  # find_package(onnxruntime REQUIRED)
  # if(TARGET onnxruntime::onnxruntime)
  #   set(ONNX_LIBRARY "onnxruntime::onnxruntime")
  #   set(ONNX_INCLUDE_DIR ${onnxruntime_INCLUDE_DIRS})
  # else()
  #   message("onnxruntime not found")	
  # endif()

  # set(JUCE_VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST")
  # message("${JUCE_VST3_COPY_DIR}")
  # set(JUCE_COPY_PLUGIN_AFTER_BUILD ON)
  juce_enable_copy_plugin_step(${PROJECT_NAME})
elseif(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")

  set(DYNAMIC_PLUGIN_NAME "plugin.so")
  
  #set(ONNX_LIBRARY "${ONNX_LIBRARY_DIR}/libonnxruntime.so")

  # find_package(onnxruntime REQUIRED)
  # if(TARGET onnxruntime::onnxruntime)
  #   set(ONNX_LIBRARY "onnxruntime::onnxruntime")
  #   set(ONNX_INCLUDE_DIR ${onnxruntime_INCLUDE_DIRS})
  # else()
  #   message("onnxruntime not found")	
  # endif()  

  # set(JUCE_COPY_PLUGIN_AFTER_BUILD ON)
  juce_enable_copy_plugin_step(${PROJECT_NAME})
endif()

set(DYNAMIC_PLUGIN_PATH "${BUILD_DIR}/${DYNAMIC_PLUGIN_NAME}")    
# message("${BUILD_DIR}")
# message("${DATA_DIR}")
# message("${DYNAMIC_PLUGIN_PATH}")
# message("${ONNX_LIBRARY_DIR}")
# message("${ONNX_INCLUDE_DIR}")
# message("${ONNX_LIBRARY}")
# message("${ONNX_MODEL_PATH}")

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    # copy plugin dynamic library to vst output directory
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>"
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${DYNAMIC_PLUGIN_PATH}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>"
    
    # copy data folder to the vst bundle
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${DATA_DIR}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/VST3/${PLUGIN_NAME}.vst3/Contents/data"
  )

  if(UNIX)
    add_custom_command(
      TARGET ${PROJECT_NAME}
      POST_BUILD
      
      # copy script for bundling dependencies on linux
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/bundle_dependencies.sh"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}_VST3>"
    )
  endif()

target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE
  BUILD_DEBUG=1  
  "DYNAMIC_PLUGIN_FILE_NAME=\"${DYNAMIC_PLUGIN_NAME}\""
  # "ONNX_MODEL_PATH=\"${ONNX_MODEL_PATH}\""
)

target_include_directories(
  ${PROJECT_NAME}
  PRIVATE
  # ${ONNX_INCLUDE_DIR}
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
  # ${ONNX_LIBRARY}
)

# we shouldn't have to copy the onnx dll all over the place anymore, if we start using it again

# set(VST3_HELPER_DIRECTORY "${CMAKE_BINARY_DIR}/Debug")
# set(VST3_OUTPUT_DIR "${CMAKE_BINARY_DIR}/GranularSynthTest_artefacts/${CMAKE_BUILD_TYPE}/VST3/Granular Synth Test.vst3")

#if(WIN32)
#  set(VST3_BIN_DIR "${VST3_OUTPUT_DIR}/Contents/x86_64-win")
#  message("copying ${ONNX_DLL} to ${VST3_BIN_DIR}") 
#  add_custom_command(
#    TARGET ${PROJECT_NAME}
#    POST_BUILD
#    # copy to build output directory
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#    "${ONNX_DLL}"
#    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
#    # copy to VST3 component directory
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#    "${ONNX_DLL}"
#    "${VST3_BIN_DIR}"
#    # copy to juce_vst3_helper.exe directory
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#    "${ONNX_DLL}"
#    "${VST3_HELPER_DIRECTORY}"
#  )
#endif()
