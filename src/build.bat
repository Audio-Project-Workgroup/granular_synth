@echo off

set BUILD_DEBUG=1

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
rem cl %CFLAGS% -c ..\src\miniaudio_impl.c
rem lib -OUT:miniaudio.lib miniaudio_impl.obj

REM preprocessor
::cl %CFLAGS% ..\src\preprocessor.cpp /link %LFLAGS%
::popd
:: TODO: this is incomprehensibly fucked
::..\build\preprocessor.exe plugin.h > generated.cpp 
::pushd ..\build

REM asset packer
cl %CFLAGS% ..\src\asset_packer.cpp /link %LFLAGS% -out:asset_packer.exe
IF NOT EXIST %DATA_DIR%\test_atlas.png (
   asset_packer.exe
)

REM compile plugin and host
cl %CFLAGS% -D"DATA_PATH=\"../data/\"" ..\src\plugin.cpp -Fmplugin.map -LD /link %LFLAGS% -PDB:plugin_%random%.pdb
REM -EXPORT:renderNewFrame -EXPORT:audioProcess -EXPORT:initializePluginState

set PLUGIN_STATUS=%ERRORLEVEL%

cl %CFLAGS% -D"PLUGIN_PATH=\"plugin.dll\"" ..\src\main.cpp logo.res -Fmmain.map /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup %LFLAGS% -LIBPATH:..\src\libs user32.lib gdi32.lib shell32.lib glfw3_mt.lib opengl32.lib miniaudio.lib /out:granade.exe
REM onnxruntime.lib

popd

exit /b %PLUGIN_STATUS%

