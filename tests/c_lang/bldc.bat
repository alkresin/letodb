@echo off
set HRB_DIR=%HB_PATH%
set LETO_DIR=..\..\

bcc32 -O2 -d -I%HRB_DIR%\include;%LETO_DIR%\include -L%LETO_DIR%\lib %1.c ws2_32.lib leto.lib

del %1.obj
del %1.tds
