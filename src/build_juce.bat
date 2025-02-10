call "build.bat"

cmake -S . -B ../build/build_JUCE
::-DJUCE_BUILD_EXTRAS=ON
cmake --build ../build/build_JUCE
::--verbose --target AudioPluginHost
