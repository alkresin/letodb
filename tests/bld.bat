@echo off

set HRB_DIR=%HB_PATH%
set LETO_DIR=..\

%HRB_DIR%\bin\harbour %1.prg -n -i%HRB_DIR%\include -i%LETO_DIR%\include -dRDD_LETO %2 %3

bcc32 -O2 -d -I%HRB_DIR%\include -L%HRB_DIR%\lib %1.c hbdebug.lib hbvm.lib hbrtl.lib gtwin.lib hbcpage.lib hblang.lib hbrdd.lib hbmacro.lib hbpp.lib rddntx.lib rddfpt.lib hbsix.lib hbcommon.lib ws2_32.lib %LETO_DIR%\lib\rddleto.lib

del %1.obj
del %1.c
del *.tds