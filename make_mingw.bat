@echo off
if "%1" == "clean" goto CLEAN
if "%1" == "CLEAN" goto CLEAN

:BUILD

if not exist lib md lib
if not exist obj md obj
if not exist obj\w32 md obj\w32

   rem set path=c:\softools\mingw\bin
   mingw32-make.exe -f makefile.gcc >make_mingw.log
   if errorlevel 1 goto BUILD_ERR

:BUILD_OK

   goto EXIT

:BUILD_ERR

   goto EXIT

:CLEAN
   del lib\*.a
   del obj\w32\*.c
   del obj\w32\*.o

   goto EXIT

:EXIT
