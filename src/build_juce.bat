call "build.bat"

REM cmake -S . -B ../build/build_JUCE
REM cmake --build ../build/build_JUCE
cmake -S . -B ../build/build_JUCE
REM -DJUCE_BUILD_EXTRAS=ON
cmake --build ../build/build_JUCE
REM --target AudioPluginHost
