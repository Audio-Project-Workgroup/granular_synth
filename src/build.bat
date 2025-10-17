@echo off
setlocal enabledelayedexpansion

:: -----------------------------------------------------------------------------
:: default options

:: build options
set BUILD_DEBUG=1
set BUILD_LOGGING=0

:: target options
set "target_plugin=1"
set "target_exe=1"
set "target_vst=0"
set "target_all=0"
echo "target_plugin: !target_plugin!"
echo "target_exe: !target_exe!"
echo "target_vst: !target_vst!"
echo "target_all: !target_all!"

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
if "!target_exe!"=="1" if "!target_plugin!"=="0" set "target_plugin=1"
if "!target_vst!"=="1" if "!target_plugin!"=="0" set "target_plugin=1"
echo "target_plugin: !target_plugin!"
echo "target_exe: !target_exe!"
echo "target_vst: !target_vst!"
echo "target_all: !target_all!"

if %BUILD_DEBUG%==1 echo [BUILD_DEBUG]
if %BUILD_LOGGING%==1 echo [BUILD_LOGGING]

:: -----------------------------------------------------------------------------
:: set up directories, flags

set SRC_DIR=%CD%
set DATA_DIR=%CD%\..\data

set CFLAGS=
set CFLAGS=%CFLAGS% -nologo -W3 -wd"4244" -wd"4146"
if %BUILD_DEBUG%==1 (
   set CFLAGS=%CFLAGS% -Zi
)
set CFLAGS=%CFLAGS% -I..\src\include
set CFLAGS=%CFLAGS% -I..\src\include\glad\include
set CFLAGS=%CFLAGS% -MT -EHa- -GR-
rem set CFLAGS=-DBUILD_DEBUG=0 -I..\src\include -nologo -Zi -W4 -wd"4201" -wd"4100" -wd"4146" -wd"4310" -wd"4244" -wd"4505" -MT -EHa- -GR-
set CFLAGS=%CFLAGS% -DBUILD_DEBUG=%BUILD_DEBUG%
set CFLAGS=%CFLAGS% -DBUILD_LOGGING=%BUILD_LOGGING%
rem set CFLAGS=%CFLAGS% -DBUILD_DEBUG=0

set LFLAGS=-incremental:no -opt:ref

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
set BUILD_DIR=%CD%

:: produce logo icon file
call rc /nologo /fo logo.res ..\data\logo.rc

:: delete old pdbs and rdis
del *.pdb > NUL 2> NUL
del *.rdi > NUL 2> NUL

REM compile miniaudio to static library
IF NOT EXIST miniaudio.lib (
    cl %CFLAGS% -c ..\src\miniaudio_impl.c
    lib -OUT:miniaudio.lib miniaudio_impl.obj
)

REM preprocessor
::cl %CFLAGS% ..\src\preprocessor.cpp /link %LFLAGS%
::popd
:: TODO: this is incomprehensibly fucked
::..\build\preprocessor.exe plugin.h > generated.cpp 
::pushd ..\build

REM asset packer
IF NOT EXIST %DATA_DIR%\test_atlas.png (
    cl %CFLAGS% ..\src\asset_packer.cpp /link %LFLAGS% -out:asset_packer.exe
    asset_packer.exe
)

:: TODO: allow linking the plugin to the exe and vst statically for release builds
set PLUGIN_STATUS=0
if "!target_plugin!"=="1" (
    echo compiling plugin...
    cl %CFLAGS% -D"DATA_PATH=\"../data/\"" ..\src\plugin.cpp -Fmplugin.map -LD /link %LFLAGS% -PDB:plugin_%random%.pdb
    REM -EXPORT:renderNewFrame -EXPORT:audioProcess -EXPORT:initializePluginState
    set PLUGIN_STATUS=%ERRORLEVEL%
)

set EXE_STATUS=0
if "!target_exe!"=="1" (
    if %PLUGIN_STATUS%==0 (
        echo compiling exe...
    	cl %CFLAGS% -D"PLUGIN_PATH=\"plugin.dll\"" ..\src\main.cpp logo.res -Fmmain.map /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup %LFLAGS% -LIBPATH:..\src\libs user32.lib gdi32.lib shell32.lib glfw3_mt.lib opengl32.lib miniaudio.lib /out:granade.exe
    	REM onnxruntime.lib
    ) else (
        echo ERROR: plugin build failed. skipping exe compilation
    )
)

set VST_STATUS=0
if "!target_vst!"=="1" (
    if %PLUGIN_STATUS%==0 (
        echo compiling vst...
	cmake -S . -B ../build/build_JUCE
	cmake --build ../build/build_JUCE
    ) else (
        echo ERROR: plugin build failed. skipping vst compilation
    )
)

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

rem :compile_link
rem set cflags=%1
rem set lflags=%2
rem set sources=%3
rem set outname=%4
rem echo %cflags%
rem echo %lflags%
rem echo %sources%
rem echo %outname%
rem rem cl %cflags% %sources% -link %lflags% -out:%outname%
rem goto :eof

:end_script 
echo done

rem exit /b %PLUGIN_STATUS%

