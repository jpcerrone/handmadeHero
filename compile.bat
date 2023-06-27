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

set COMMON_FLAGS=/Febin\ /Fdbin\ /Fobin\ /nologo -W4 -Oi -Od -GR -EHa -wd4100 -wd4201
set COMMON_LIBS=User32.lib Gdi32.lib Ole32.lib Winmm.lib

set CURRENT_TIMESTAMP=%time:~0,2%%time:~3,2%%time:~6,2%

: todo extract dev build from both
: todo remove evth from bin folder (COMMIT BEFORE THIS)
cl -Zi .\gameCode.cpp -DDEV_BUILD %COMMON_LIBS% %COMMON_FLAGS% /LD /link /DLL /INCREMENTAL:NO /PDB:bin\gameCode_%CURRENT_TIMESTAMP%.pdb
cl -Zi .\win_main.cpp -DDEV_BUILD %COMMON_LIBS% %COMMON_FLAGS%