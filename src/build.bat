@echo off

set CFLAGS=-DBUILD_DEBUG=0 -I..\src\include -nologo -Zi -W4 -wd"4201" -wd"4100" -wd"4146" -wd"4310" -wd"4244" -wd"4505" -MT -EHa- -GR-
set LFLAGS=-incremental:no -opt:ref

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM produce logo icon file
call rc /nologo /fo logo.res ..\data\logo.rc

REM delete old pdbs and rdis
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

REM compile plugin and host
cl %CFLAGS% -D"DATA_PATH=\"../data\"" ..\src\plugin.cpp -Fmplugin.map -LD /link %LFLAGS% -PDB:plugin_%random%.pdb -EXPORT:renderNewFrame -EXPORT:audioProcess -EXPORT:initializePluginState

set PLUGIN_STATUS=%ERRORLEVEL%

cl %CFLAGS% -D"PLUGIN_PATH=\"plugin.dll\"" ..\src\main.cpp logo.res -Fmmain.map /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup %LFLAGS% -LIBPATH:..\src\libs user32.lib gdi32.lib shell32.lib glfw3_mt.lib opengl32.lib miniaudio.lib /out:granade.exe
REM onnxruntime.lib

popd

exit /b %PLUGIN_STATUS%

