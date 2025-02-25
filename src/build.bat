@echo off

set CFLAGS=-I..\src\include -nologo -Zi -W4 -wd"4201" -wd"4100" -wd"4146" -wd"4310" -wd"4244" -MT -EHa- -GR-
set LFLAGS=-incremental:no -opt:ref

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM delete old pdbs and rdis
del *.pdb > NUL 2> NUL
del *.rdi > NUL 2> NUL

REM compile miniaudio to static library
REM cl %CFLAGS% -c ..\src\miniaudio_impl.c
REM lib -OUT:miniaudio.lib miniaudio_impl.obj

REM compile plugin and host
REM /PDB:plugin_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%%time:~9,2%.pdb
cl %CFLAGS% ..\src\plugin.cpp -Fmplugin.map -LD /link %LFLAGS% -PDB:plugin_%random%.pdb -EXPORT:renderNewFrame -EXPORT:audioProcess

set PLUGIN_STATUS=%ERRORLEVEL%

cl %CFLAGS% -D"PLUGIN_PATH=\"%cd:\=\\%\\plugin.dll\"" ..\src\main.cpp -Fmmain.map /link %LFLAGS% -LIBPATH:..\src\libs user32.lib gdi32.lib shell32.lib glfw3_mt.lib opengl32.lib miniaudio.lib onnxruntime.lib

popd

exit /b %PLUGIN_STATUS%

