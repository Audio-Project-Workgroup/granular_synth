@echo off
setlocal enabledelayedexpansion

:: -----------------------------------------------------------------------------
:: default options

:: config options
set "config_debug=0"
set "config_logging=0"

:: target options
set "target_plugin=0"
set "target_exe=0"
set "target_vst=0"
set "target_all=1"

:: -----------------------------------------------------------------------------
:: parse cli arguments

:: NOTE: syntax is `key:value1+value2` to set variables `key_value1` and `key_value2` to 1
for %%a in (%*) do (
    for /f "tokens=1* delims=:" %%k in ("%%a") do (
    	set "key=%%k"
	if "!key!"=="target" set "target_plugin=0" && set "target_exe=0" && set "target_vst=0" && set "target_all=0"

	set "values=%%l"
    	for /f "tokens=1-4 delims=+" %%v in ("!values!") do (	    
	    if not "%%v"=="" call :set_var !key! %%v
	    if not "%%w"=="" call :set_var !key! %%w
	    if not "%%x"=="" call :set_var !key! %%x
	    if not "%%y"=="" call :set_var !key! %%y
	)
    )   
)

if "!target_all!"=="1" set "target_plugin=1" && set "target_exe=1" && set "target_vst=1"
if "!target_exe!"=="1" set "target_plugin=1"
if "!target_vst!"=="1" set "target_plugin=1"

if "!target_plugin!"=="1" echo [BUILD_PLUGIN]
if "!target_exe!"=="1" echo [BUILD_EXE]
if "!target_vst!"=="1" echo [BUILD_VST]

if "!config_debug!"=="1" echo [BUILD_DEBUG]
if "!config_logging!"=="1" echo [BUILD_LOGGING]

:: -----------------------------------------------------------------------------
:: set up directories, flags

set SRC_DIR=%CD%
set DATA_DIR=%CD%\..\data

set CFLAGS=
set CFLAGS=%CFLAGS% -nologo -W3 -wd"4244" -wd"4146"
if "!config_debug!"=="1" (
   set CFLAGS=%CFLAGS% -Zi
)
set CFLAGS=%CFLAGS% -I..\src\include
set CFLAGS=%CFLAGS% -I..\src\include\glad\include
set CFLAGS=%CFLAGS% -MT -EHa- -GR-
set CFLAGS=%CFLAGS% -DBUILD_DEBUG=!config_debug!
set CFLAGS=%CFLAGS% -DBUILD_LOGGING=!config_logging!

set LFLAGS=-incremental:no -opt:ref

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
set BUILD_DIR=%CD%

:: -----------------------------------------------------------------------------
:: build targets

:: produce logo icon file
if not exist logo.res (
    echo building logo resource...
    call rc /nologo /fo logo.res ..\data\logo.rc
)

:: delete old pdbs and rdis
del *.pdb > NUL 2> NUL
del *.rdi > NUL 2> NUL

:: compile miniaudio to static library
IF NOT EXIST miniaudio.lib (
    echo compiling miniaudio...
    cl %CFLAGS% -c ..\src\miniaudio_impl.c
    lib -OUT:miniaudio.lib miniaudio_impl.obj
)

:: preprocessor
rem cl %CFLAGS% ..\src\preprocessor.cpp /link %LFLAGS%
rem popd
rem :: TODO: this is incomprehensibly fucked
rem ..\build\preprocessor.exe plugin.h > generated.cpp 
rem pushd ..\build

:: asset packer
IF NOT EXIST %DATA_DIR%\test_atlas.png (
    echo compiling asset packer...
    cl %CFLAGS% ..\src\asset_packer.cpp /link %LFLAGS% -out:asset_packer.exe
    echo running asset packer...
    asset_packer.exe
)

:: TODO: try compiling common layer implementations to separate lib/obj and link
::       with all targets to speed up build times

:: plugin
:: TODO: allow linking the plugin to the exe and vst statically for release builds
if "!target_plugin!"=="1" (
    echo compiling plugin...
    cl %CFLAGS% -D"DATA_PATH=\"../data/\"" ..\src\plugin.cpp -Fmplugin.map -LD /link %LFLAGS% -PDB:plugin_%random%.pdb
)
set PLUGIN_STATUS=%ERRORLEVEL%

:: exe
if "!target_exe!"=="1" (
    if %PLUGIN_STATUS%==0 (
        echo compiling exe...
    	cl %CFLAGS% -D"PLUGIN_PATH=\"plugin.dll\"" ..\src\main.cpp logo.res -Fmmain.map /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup %LFLAGS% -LIBPATH:..\src\libs user32.lib gdi32.lib shell32.lib glfw3_mt.lib opengl32.lib miniaudio.lib /out:granade.exe
    	REM onnxruntime.lib
    ) else (
        echo ERROR: plugin build failed. skipping exe compilation
    )
)
set EXE_STATUS=%ERRORLEVEL%

:: vst
if "!target_vst!"=="1" (
    if %PLUGIN_STATUS%==0 (
        echo compiling vst...
	cmake -S %SRC_DIR% -B %BUILD_DIR%/build_JUCE -DBUILD_DEBUG=!config_debug! -DBUILD_LOGGING=!config_logging!
	cmake --build %BUILD_DIR%/build_JUCE
    ) else (
        echo ERROR: plugin build failed. skipping vst compilation
    )
)
set VST_STATUS=%ERRORLEVEL%

popd
goto end_script

::------------------------------------------------------------------------------
:: subroutine definitions

:set_var
set "key=%1"
set "value=%2"
set "var=!key!_!value!"
rem echo "set_var: !var!"
if defined !var! (
    set "!var!=1" && echo "set !var! to 1"
) else (
    echo "unknown key-value pair: !var!"
)
goto :eof

:end_script 
echo done

exit /b %ERRORLEVEL%

