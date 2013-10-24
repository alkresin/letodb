@echo off
if "%1" == "clean" goto CLEAN
if "%1" == "CLEAN" goto CLEAN

set XHB_PATH=c:\xhb

if not exist lib md lib
if not exist obj md obj
if not exist obj\xhb md obj\xhb

%XHB_PATH%\bin\xhb.exe -oobj\xhb\server.c -m -n -q -gc0 -D__WIN_DAEMON__  -Iinclude  -I%XHB_PATH%\include -I%XHB_PATH%\include\w32 source\server\server.prg
%XHB_PATH%\bin\xhb.exe -oobj\xhb\errorsys.c -m -n -q -gc0 -Iinclude  -I%XHB_PATH%\include -I%XHB_PATH%\include\w32 source\server\errorsys.prg
%XHB_PATH%\bin\xhb.exe -oobj\xhb\common.c -m -n -q -gc0 -Iinclude  -I%XHB_PATH%\include -I%XHB_PATH%\include\w32 source\common\common.prg
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\common_c.obj /MT -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\common\common_c.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\blowfish.obj /MT -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\common\blowfish.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\hbip.obj /MT -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\common\hbip.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\leto_win.obj /MT -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\server\leto_win.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\errint.obj /MT -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\server\errint.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\letofunc.obj /MT -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\server\letofunc.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\letoacc.obj /MT -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\server\letoacc.c

%XHB_PATH%\bin\xlink.exe -NOEXPOBJ -MAP -FORCE:MULTIPLE -NOIMPLIB -subsystem:windows -LIBPATH:%XHB_PATH%\lib -LIBPATH:%XHB_PATH%\c_lib -LIBPATH:%XHB_PATH%\c_lib\win  obj\xhb\server.obj obj\xhb\errorsys.obj obj\xhb\common.obj obj\xhb\common_c.obj obj\xhb\blowfish.obj obj\xhb\hbip.obj obj\xhb\leto_win.obj obj\xhb\errint.obj obj\xhb\letofunc.obj obj\xhb\letoacc.obj OptG.lib xhb.lib dbf.lib ntx.lib cdx.lib ct3comm.lib crtmt.lib kernel32.lib user32.lib winspool.lib ole32.lib oleaut32.lib odbc32.lib odbccp32.lib uuid.lib wsock32.lib ws2_32.lib wininet.lib advapi32.lib shlwapi.lib msimg32.lib mpr.lib comctl32.lib comdlg32.lib gdi32.lib shell32.lib winmm.lib lz32.lib Netapi32.lib -out:bin\letodb.exe

%XHB_PATH%\bin\xcc.exe -Foobj\xhb\leto1.obj -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\client\leto1.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\letomgmn.obj -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\client\letomgmn.c
%XHB_PATH%\bin\xcc.exe -Foobj\xhb\net.obj -Iinclude -I%XHB_PATH%\include -I%XHB_PATH%\c_include -I%XHB_PATH%\c_include\win -I%XHB_PATH%\c_include\msvc source\common\net.c

%XHB_PATH%\bin\xlib.exe  -out:lib\rddleto.lib obj\xhb\leto1.obj obj\xhb\letomgmn.obj obj\xhb\hbip.obj obj\xhb\common_c.obj obj\xhb\blowfish.obj obj\xhb\net.obj

goto EXIT

:CLEAN
   del bin\*.exe
   del lib\*.lib
   del lib\*.bak
   del obj\xhb\*.obj
   del obj\xhb\*.c

:EXIT
