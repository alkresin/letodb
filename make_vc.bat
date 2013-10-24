@echo off
if "%1" == "clean" goto CLEAN
if "%1" == "CLEAN" goto CLEAN

if not exist lib md lib
if not exist obj md obj
if not exist obj\vc md obj\vc
:BUILD

rem @SET VSINSTALLDIR=C:\MSVS8
rem @SET VCINSTALLDIR=C:\MSVS8\VC
rem @set PATH=C:\MSVS8\VC\BIN;C:\MSVS8\VC\PlatformSDK\bin;C:\MSVS8\Common7\IDE;C:\MSVS8\Common7\Tools;C:\MSVS8\Common7\Tools\bin;C:\MSVS8\SDK\v2.0\bin;C:\MSVS8\VC\VCPackages;%PATH%
rem @set INCLUDE=C:\MSVS8\VC\include;C:\MSVS8\VC\PlatformSDK\include
rem @set LIB=C:\MSVS8\VC\lib;C:\MSVS8\VC\PlatformSDK\lib
rem @set LIBPATH=C:\MSVS8\VC\lib;C:\MSVS8\VC\PlatformSDK\lib

   nmake /I /Fmakefile.vc %1 %2 %3 > make_vc.log
if errorlevel 1 goto BUILD_ERR
rem copy lib\rddleto.lib %HB_PATH%\lib\rddleto.lib
:BUILD_OK

   goto EXIT

:BUILD_ERR

   notepad make_vc.log
   goto EXIT

:CLEAN
   del bin\*.exe
   del lib\*.lib
   del lib\*.bak
   del obj\vc\*.obj
   del obj\vc\*.c

   del make_vc.log

   goto EXIT

:EXIT

