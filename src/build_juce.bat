call "build.bat"

if %ERRORLEVEL% equ 0 (
  echo plugin build succeeded
  cmake -S . -B ../build/build_JUCE
  ::-DJUCE_BUILD_EXTRAS=ON
  cmake --build ../build/build_JUCE
  ::--verbose --target AudioPluginHost
  echo ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
) else (
  echo ERROR: plugin build failed. skipping vst compilation  
)
