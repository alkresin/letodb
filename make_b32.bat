@echo off
if "%1" == "clean" goto CLEAN
if "%1" == "CLEAN" goto CLEAN

if not exist lib md lib
if not exist obj md obj
if not exist obj\b32 md obj\b32
:BUILD

make -l EXE_OBJ_DIR=obj\b32\bin OBJ_DIR=obj\b32 -fmakefile.bc %1 %2 %3 > make_b32.log
if errorlevel 1 goto BUILD_ERR
copy lib\rddleto.lib %HB_PATH%\lib\rddleto.lib
:BUILD_OK

   goto EXIT

:BUILD_ERR

   notepad make_b32.log
   goto EXIT

:CLEAN
   del bin\*.exe
   del bin\*.tds
   del lib\*.lib
   del lib\*.bak
   del obj\b32\*.obj
   del obj\b32\*.c

   del make_b32.log

   goto EXIT

:EXIT

