@echo off
if "%1" == "clean" goto CLEAN
if "%1" == "CLEAN" goto CLEAN

if not exist lib2 md lib2
if not exist obj2 md obj2
if not exist obj2\b32 md obj2\b32
:BUILD

make -l EXE_OBJ_DIR=obj2\b32\bin OBJ_DIR=obj2\b32 -fmakefile.bc2 %1 %2 %3 > make_b2.log
if errorlevel 1 goto BUILD_ERR
rem copy lib\rddleto.lib %HB_PATH%\lib\rddleto.lib
:BUILD_OK

   goto EXIT

:BUILD_ERR

   notepad make_b2.log
   goto EXIT

:CLEAN
   del bin\letodb2.exe
   del bin\*.tds
   del lib2\*.lib
   del lib2\*.bak
   del obj2\b32\*.obj
   del obj2\b32\*.c

   del make_b32.log

   goto EXIT

:EXIT

