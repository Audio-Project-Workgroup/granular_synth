set CFLAGS=-Zi -W4 -MT -EHa- -GR-
set LFLAGS=-incremental:no -opt:ref

pushd ..\build

cl %CFLAGS% ..\src\plugin.cpp -Fmplugin.map -LD /link %LFLAGS% -EXPORT:renderNewFrame -EXPORT:audioProcess
cl %CFLAGS% ..\src\main.cpp -Fmmain.map /link %LFLAGS%
