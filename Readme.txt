      /* $Id: Readme.txt,v 1.27 2010/08/13 16:43:27 ptsarenko Exp $ */

      Leto DB Server is a multiplatform database server or a database 
 management system, chiefly intended for client programs, written on Harbour,
 be able to work with dbf/cdx files, located on a remote server.

Contents
--------

1. Directory structure
2. Building binaries
   2.1 Borland Win32 C compiler
   2.2 Linux GNU C compiler
   2.3 xHarbour Builder
   2.4 Mingw C compiler
   2.5 Building letodb with hbmk
3. Running and stopping server
4. Server configuration
   4.1 letodb.ini
   4.2 Authentication
5. Connecting to the server from client programs
6. Variables management
7. Functions list
8. Management utility


      1. Directory structure

      bin/          -    server executable file
      doc/          -    documentation
      include/      -    source header files
      lib/          -    rdd library
      source/
          client/   -    rdd sources
          common/   -    some common source files
          client/   -    server sources
      tests/        -    test programs, samples
      utils/
          manage/   -    server management utilities


      2. Building binaries

      2.1 Borland Win32 C compiler

      An environment variable HB_PATH, which defines a path to the Harbour
 directory, must be set before compiling. This can be done, for example, 
 by adding a line in a make_b32.bat:

          SET HB_PATH=c:\harbour

 If you use xHarbour, uncomment a line 'XHARBOUR = yes' in makefile.bc.
 Then run the make_b32.bat and you will get server executable file letodb.exe in a bin/
 directory and rdd library rddleto.lib in a lib/ directory.

      2.2 Linux GNU C compiler

      An environment variable HB_ROOT, which defines a path to the Harbour
 directory, must be set before compiling. Or just change the value of HRB_DIR
 in the Makefile.linux.

 Then run the make_linux.sh and you will get server executable file letodb in a bin/
 directory and rdd library librddleto.a in a lib/ directory.

      2.3 xHarbour Builder

      Run the make_xhb.bat to build binaries with this compiler. Probably,
  you will need to change the path to your xHarbour Builder copy in the
  make_xhb.bat. Default value is:

          set XHB_PATH=c:\xhb

      2.4 Mingw C compiler

      An environment variable HB_PATH, which defines a path to the Harbour
 directory, must be set before compiling. This can be done, for example, 
 by adding a line in a make_mingw.bat:

          SET HB_PATH=c:\harbour

 If you use xHarbour, uncomment a line 'XHARBOUR = yes' in makefile.gcc.
 Then run the make_mingw.bat and you will get server executable file letodb.exe in a bin/
 directory and rdd library librddleto.a in a lib/ directory.

      2.5 Building letodb with hbmk

      Now there is a possibility to build letodb with a Harbour make utility, provided by
 Viktor Szakats. The command line syntax is:

      hbmk2 [-hb10|-xhb] rddleto.hbp letodb.hbp [-target=tests/test_ta.prg]

 Optional parameters: -hb10 - Harbour version is 1.0 or 1.0.1,
                      -xhb - xHarbour,
                      -target="blank"\test_ta.prg - build the test, too.

      3. Running and stopping server

      Just run it:
      
      letodb.exe                    ( under Windows )
      ./letodb                      ( under Linux )

      To shutdown the server, run the same executable with a 'stop' parameter:

      letodb.exe stop               ( under Windows )
      ./letodb stop                 ( under Linux )

      4. Server configuration

      4.1 letodb.ini

      You may provide configuration file letodb.ini if you isn't satisfied with
 default parameters values. Currently following parameters exists ( default
 values are designated ):

      Port = 2812              -    server port number;
      DataPath =               -    path to a data directory on a server;
      Log = letodb.log         -    path and name of a log file;
      Default_Driver = CDX     -    default RDD to open files on server ( CDX/NTX );
      Lower_Path = 0           -    if 1, convert all paths to lower case;
      EnableFileFunc = 0       -    if 1, using of file functions ( leto_file(),
                                    leto_ferase(), leto_frename() is enabled;
      EnableAnyExt = 0         -    if 1, creating of data tables and indexes with
                                    any extention, other than standard ( dbf,cdx,ntx )
                                    is enabled; 
      Pass_for_Login = 0       -    if 1, user authentication is necessary to
                                    login to the server;
      Pass_for_Manage = 0      -    if 1, user authentication is necessary to
                                    use management functions ( Leto_mggetinfo(), etc. );
      Pass_for_Data = 0        -    if 1, user authentication is necessary to
                                    have write access to the data;
      Pass_File = "leto_users" -    the path and name of users info file;
      Crypt_Traffic = 0        -    if 1, the data passes to the network encrypted;
      Share_Tables  = 0        -    if 0 (default, this mode server was the only from the
                                    start of a letodb project), the letodb opens all
                                    tables in an exclusive mode, what allows to increase
                                    the speed. If 1 (new mode, added since June 11, 2009),
                                    tables are opened in the same mode as client
                                    applications opens them, exclusive or shared, what
                                    allows the letodb to work in coexistence with other
                                    types of applications.
      Cache_Records            -    The number of records to read into the cache
      Max_Vars_Number = 10000  -    Maximum number of shared variables
      Max_Var_Size = 10000     -    Maximim size of a text variable

      It is possible to define [DATABASE] structure if you need to have a
 directory, where files are opened via other RDD:

      [DATABASE]
      DataPath =               -    (mandatory option)
      Driver = CDX             -    ( CDX/NTX )

      You can define as many [DATABASE] sections, as needed.

      In Windows environment the letodb.ini must be placed in a directory, from
 where server is started.
      In Linux the program looks for it in the directory from where the server
 is started and, if unsuccessfully, in the /etc directory.

      4.2 Authentication

      To turn authentication subsystem on you need to set one of the following 
 letodb.ini parameters to 1: Pass_for_Login, Pass_for_Manage, Pass_for_Data. But before
 you need to create, at least, one user with admin rights, because when authentication
 subsystem works, only authenticated users with admin rights are able to add/change users
 and passwords.
      To add a user, you need to include a call of LETO_USERADD() in your client side
 program, for example:

      LETO_USERADD( "admin", "secret:)", "YYY" )

 where "YYY" is a string, which gives rights to admin, manage and write access. You may
 also use the utils/manager/console.prg program to set or change authentication data.

 To connect to a server with an authentication data ( username and password ) you need to
 use LETO_CONNECT() function.

      5. Connecting to the server from client programs

      To be able to connect to the server you need to link the rddleto.lib (Windows)
 or librddleto.a (Linux) to your aplication and add at start of a main source file
 two lines:

      REQUEST LETO
      RDDSETDEFAULT( "LETO" )

      To open a dbf file on a server, you need to write in a SET PATH TO
 statement or in the USE command directly a path to the server in a standard
 form //ip_address:port/data_path/file_name.

      If a 'DataPath' parameter of a server configuration file is set to a non
 empty value, you need to designate not the full path to a file on the server,
 but only a relative ( relatively the 'DataPath' ).
      For example, if the you need to open a file test.dbf, which is located on
 a server 192.168.5.22 in a directory /data/mydir and the 'DataPath' parameter
 value ( of a letodb.ini server configuration file ) is '/data', the syntax
 will be the following:

      USE "//192.168.5.22:2812/mydir/test"

      If the server doesn't run or you write a wrong path, you'll get open error.
 It is possible to check accessibility of a server before opening files by using
 the leto_Connect( cAddress ) function, which returns -1 in case of unsuccessful
 attempt:

      IF leto_Connect( "//192.168.5.22:2812/mydir/" ) == -1
         Alert( "Can't connect to server ..." )
      ENDIF

      6. Variables management

      Letodb allows to manage variables, which are shared between applications, connected
 to the server. Variables are separated into groups and may be of logical, integer or 
 character type. You can set a variable, get it, delete or increment/decrement
 ( numerical, of course ). All operations on variables are performed consecutively by
 one thread, so the variables may work as semaphores. See the functions list for a syntax
 of appropriate functions and tests/test_var.prg for a sample.
      Letodb.ini may contain lines to determine the maximum number of variables and a
 maximum size of a text variable.

      7. Functions list

      Below is a full ( at least, for the moment I write it ) list of functions,
 available for using in client applications with RDD LETO linked.

      LETO_CONNECT( cAddress [, cUserName, cPassword ] )   --> nConnection, -1 if failed
      LETO_CONNECT_ERR()                                   --> nError
      LETO_DISCONNECT()
      LETO_SETCURRENTCONNECTION( nConnection )
      LETO_GETCURRENTCONNECTION()                          --> nConnection
      LETO_GETSERVERVERSION()                              --> cVersion
      LETO_GETLOCALIP()                                    --> IP address of client station

      LETO_BEGINTRANSACTION()
      LETO_ROLLBACK()
      LETO_COMMITTRANSACTION( [ lUnlockAll ] )             --> lSuccess
      LETO_INTRANSACTION()                                 --> lTransactionActive

      LETO_SUM( cFieldNames , [ cFilter ], [xScope], [xScopeBottom] )
                                                           --> nSumma if one field passed, or
                                                               {nSumma1, nSumma2, ...} for several fields
      LETO_ISFLTOPTIM()                                    --> lFilterOptimized
      LETO_SETSKIPBUFFER( nSkip )
      LETO_SETFASTAPPEND( lFastAppend )                    --> lFastAppend (previous value)

      LETO_FILE( cFileName )                               --> lFileExists
      LETO_FERASE( cFileName )                             --> -1 if failed
      LETO_FRENAME( cFileName, cFileNewName )              --> -1 if failed
      LETO_MEMOREAD( cFileName )                           --> cStr
      LETO_FERROR()                                        --> nError

      LETO_MGGETINFO()
      LETO_MGGETUSERS()
      LETO_MGGETTABLES()
      LETO_MGGETTIME()
      LETO_MGKILL()

      LETO_USERADD( cUserName, cPass [, cRights ] )        --> lSuccess
      LETO_USERPASSWD( cUserName, cPass )                  --> lSuccess
      LETO_USERRIGHTS( cUserName, cRights )                --> lSuccess
      LETO_USERFLUSH()                                     --> lSuccess
      LETO_USERGETRIGHTS()                                 --> cRights

      LETO_VARSET( cGroupName, cVarName, xValue[, nFlags[, @xRetValue]] ) --> lSuccess
      LETO_VARGET( cGroupName, cVarName )                  --> xValue
      LETO_VARINCR( cGroupName, cVarName )                 --> nValue
      LETO_VARDECR( cGroupName, cVarName )                 --> nValue
      LETO_VARDEL( cGroupName, cVarName )                  --> lSuccess
      LETO_VARGETLIST( [cGroupName [, nMaxLen]] )          --> aList

      8. Management utility

      There are two management utilities, GUI and console, the sources are in
 utils/manage directory.

      GUI utility, manage.prg, is made with the HwGUI library. If you have HwGUI,
 just write in the line 'set HWGUI_INSTALL=' in utils/manage/bld.bat a path
 to your HwGUI directory and run the bld.bat, it will build manage.exe for you.
   
      For those, who doesn't use HwGUI, there is a console mode utility,
 console.prg. Build a console.exe with a make/bat files, which you use to build
 Harbour single-file programs, you just need to add rddleto.lib to the libraries
 list. Run the console.exe with a server name or ip and port as a parameter:

      console.exe server_name:nPort
      console.exe ip_address:nPort

 The server_name and ip_address in a command line must be without leading
 slashes ( '//' ), because Clipper/Harbour interprets them in a special way.

