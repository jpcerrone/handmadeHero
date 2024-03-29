: GCC
: g++ .\win_main.cpp -lgdi32 -lole32 -lavrt -l:libksguid.a -o win_main.exe -DDEV_BUILD -g -O0
: .\win_main.exe

: - VS compiler switches:
:   - `-WX`, `-W4`: enable warning level 4 and treat warnings as errors
:   - `-wd`: turn off some warnings
:   - `-MT`: static link C runtime library
:   - `-Oi`: generates intrinsic functions.
:   - `-Od`: disable optimization
:   - `-GR-`: disable run-time type information, we don't need this
:   - `-EHa-`: disable exception-handling
:   - `-nologo`: don't print compiler info
:   - `-FC`: full Path of Source Code File in Diagnostics

set COMMON_COMPILER_FLAGS=-DDEV_BUILD /Febin\ /Fdbin\ /Fobin\ /nologo -W4 -Oi -Od -GR -EHa -wd4100
set COMMON_LINKER_FLAGS=User32.lib Gdi32.lib Ole32.lib Winmm.lib

cl -Zi .\gameCode.cpp %COMMON_COMPILER_FLAGS% %COMMON_LINKER_FLAGS%
cl -Zi .\win_main.cpp %COMMON_COMPILER_FLAGS% %COMMON_LINKER_FLAGS%
