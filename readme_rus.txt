/* $Id: readme_rus.txt,v 1.6 2010/08/13 16:43:27 ptsarenko Exp $ */

      Leto DB Server - �� ����������ଥ��� �ࢥ� ��, ��� ����, 
 � �᭮���� �।�����祭�� ��� ������᪨� �ணࠬ�, ����ᠭ��� �� �몥 Harbour,
 ��� ����㯠 � 䠩��� dbf/cdx, �ᯮ������� �� 㤠������ �ࢥ�.

����ঠ���
--------

1. ������� ��⠫����
2. ���ઠ letodb
   2.1 ��������� Borland Win32 C
   2.2 ��������� Linux GNU C
   2.3 xHarbour Builder
   2.4 ��������� Mingw C
   2.5 ���ઠ letodb � ������� hbmk
3. ����� � ��⠭���� �ࢥ�
4. ���䨣��஢���� �ࢥ�
   4.1 letodb.ini
   4.2 ���ਧ��� ���짮��⥫��
5. ���������� � �ࢥ஬ ������᪨� �ணࠬ�
6. ��ࠢ����� ��६���묨
7. ���᮪ �㭪権
8. �⨫�� �ࠢ����� �ࢥ஬


      1. ������� ��⠫����

      bin/          -    �ᯮ��塞� 䠩� �ࢥ�
      doc/          -    ���㬥����
      include/      -    䠩�� ����������
      lib/          -    ������⥪� rdd
      source/
          client/   -    ��室�� ⥪��� rdd
          common/   -    ��騥 ��室�� ⥪��� ��� �ࢥ� � rdd
          client/   -    ��室�� ⥪��� �ࢥ�
      tests/        -    ��⮢� �ணࠬ��, �ਬ���
      utils/
          manage/   -    ��室�� ⥪��� �⨫��� �ࠢ����� �ࢥ஬


      2. ���ઠ letodb

      2.1 ��������� Borland Win32 C

      ��६����� ���㦥��� HB_PATH, � ���ன ��⠭���������� ���� � ��⠫���
 Harbour, ������ ���� ��⠭������ ��। �������樥�. �� ����� ᤥ����,
 � �ਬ���, ������� ��ம�� � make_b32.bat:

          SET HB_PATH=c:\harbour

 �᫨ �� �ᯮ���� xHarbour, ᭨��� �������਩ � ��ப� 'XHARBOUR = yes' � makefile.bc.
 ��⥬ ������� make_b32.bat, � �ᯮ��塞� 䠩� �ࢥ� letodb.exe �㤥� ᮧ��� � ��⠫��� bin/,
 � ������⥪� rdd rddleto.lib - � ��⠫��� lib/.

      2.2 ��������� Linux GNU C

      ��६����� ���㦥��� HB_ROOT, � ���ன ��⠭���������� ���� � ��⠫���
 Harbour, ������ ���� ��⠭������ ��। �������樥�. ��� ���� �������
 ���祭�� HRB_DIR � 䠩�� Makefile.linux.

 ��⥬ ������� make_linux.sh, � �ᯮ��塞� 䠩� �ࢥ� letodb �㤥� ᮧ���
 � ��⠫��� bin/, � ������⥪� rdd librddleto.a - � ��⠫��� lib/.

      2.3 xHarbour Builder

      ������� make_xhb.bat ��� ᡮન letodb �⨬ ��������஬. ��������,
  ����室��� �㤥� �������� ���� � xHarbour Builder � make_xhb.bat.
  ���祭�� ��� � xHarbour Builder �� 㬮�砭��:

          set XHB_PATH=c:\xhb

      2.4 ��������� Mingw C

      ��६����� ���㦥��� HB_PATH, � ���ன ��⠭���������� ���� � ��⠫���
 Harbour,  ������ ���� ��⠭������ ��। �������樥�. ��� ����� ᤥ����,
 ��⠢�� ��ப� � 䠩� make_mingw.bat:

          SET HB_PATH=c:\harbour

 �᫨ �� �ᯮ���� xHarbour, ᭨��� �������਩ � ��ப� 'XHARBOUR = yes'
 � 䠩�� makefile.gcc. ��⥬ ������� the make_mingw.bat, � �ᯮ��塞� 䠩�
 �ࢥ� letodb.exe �㤥� ᮧ��� � ��⠫��� bin/, � ������⥪� rdd 
 librddleto.a - � ��⠫��� lib/.

      2.5 ���ઠ letodb � ������� hbmk

      ����� ������ ����������� ᮡ��� letodb � ������� �⨫��� ᡮન Harbour,
 ����ᠭ��� Viktor Szakats. ���⠪�� ��������� ��ப� ��� ᡮન:

      hbmk2 [-hb10|-xhb] rddleto.hbp letodb.hbp [-target=tests/test_ta.prg]

 �������⥫�� ��ࠬ����: -hb10 - Harbour version is 1.0 or 1.0.1,
                      -xhb - xHarbour,
                      -target="blank"\test_ta.prg - ᡮઠ ��⮢�� �ਬ�஢.

      3. ����� � ��⠭���� �ࢥ�

      ���� �������:
      
      letodb.exe                    ( ��� Windows )
      ./letodb                      ( ��� Linux )

      ��� ��⠭���� �ࢥ� ������� �� �� �ᯮ��塞� 䠩� � ��ࠬ��஬
 'stop':

      letodb.exe stop               ( ��� Windows )
      ./letodb stop                 ( ��� Linux )

      4. ���䨣��஢���� �ࢥ�

      4.1 letodb.ini

      �᫨ ��� �� ���ࠨ���� ��ࠬ���� �� 㬮�砭��, �� ����� ��⠭�����
 ���祭�� ��६����� ���䨣��樨 � 䠩�� letodb.ini. � �����饥 �६�
 �������� ᫥���騥 ��ࠬ���� ���䨣��樨 ( 㪠���� ���祭�� �� 㬮�砭�� ):

      Port = 2812              -    ���� �ࢥ�;
      DataPath =               -    ���� � ���� ������ �� �ࢥ�;
      Log = letodb.log         -    ���� � ��� ���-䠩��;
      Default_Driver = CDX     -    RDD �� 㬮�砭�� ��� ������ 䠩��� �� �ࢥ� ( CDX/NTX );
      Lower_Path = 0           -    �᫨ 1, �८�ࠧ����� �� ��� � ������� ॣ�����;
      EnableFileFunc = 0       -    �᫨ 1, ࠧ�襭� �ᯮ�짮����� 䠩����� �㭪権
                                    ( leto_file(), leto_ferase(), leto_frename();
      EnableAnyExt = 0         -    �᫨ 1, ࠧ�襭� ᮧ����� ⠡��� ������ � �����ᮢ � ���७���,
                                    �⫨�� �� �⠭���⭮�� ( dbf,cdx,ntx );
      Pass_for_Login = 0       -    �᫨ 1, ����室��� ���ਧ��� ���짮��⥫�
                                    ��� ᮥ������� � �ࢥ஬;
      Pass_for_Manage = 0      -    �᫨ 1, ����室��� ���ਧ��� ���짮��⥫� ���
                                    �ᯮ�짮����� �㭪権 �ࠢ����� �ࢥ஬
                                    ( Leto_mggetinfo(), etc. );
      Pass_for_Data = 0        -    �᫨ 1, ����室��� ���ਧ��� ���짮��⥫� ���
                                    ����䨪�樨 ������;
      Pass_File = "leto_users" -    ���� � ��� 䠩�� ���ଠ樨 ���짮��⥫��;
      Crypt_Traffic = 0        -    �᫨ 1, � �����, ��।������ �� ��, ��������;
      Share_Tables  = 0        -    �᫨ 0 (�� 㬮�砭��, ��� ०�� ������� � ������
                                    ���� �஥�� letodb), letodb ���뢠�� �� ⠡����
                                    � �������쭮� ०���, �� �������� 㢥�����
                                    �ந�����⥫쭮���. �᫨ 1 (���� ०��, �������� ��᫥ 11.06.2009),
                                    ⠡���� ���뢠���� � ⮬ ०���, � ����� �� ���뢠��
                                    ������᪮� �ਫ������, �������쭮� ��� ०��� ࠧ�������, ��
                                    �������� letodb ࠡ���� ᮢ���⭮ � ��㣨�� �ਫ�����ﬨ.
      Cache_Records            -    ���-�� ����ᥩ, �⠥��� �� ���� ࠧ (� ��� ������)
      Max_Vars_Number = 10000  -    ���ᨬ��쭮� ������⢮ ࠧ���塞�� ��६�����
      Max_Var_Size = 10000     -    ���ᨬ���� ࠧ��� ⥪�⮢�� ��६�����

      �������� ��।����� ᥪ�� [DATABASE], �᫨ �� ��� 㪠���� ��⠫�� ��,
 � ���஬ ⠡���� ������ ���뢠���� ��㣨� RDD:

      [DATABASE]
      DataPath =               -    (��易⥫쭠� ����)
      Driver = CDX             -    ( CDX/NTX )

      ����� ��।����� �⮫쪮 ᥪ権 [DATABASE], ᪮�쪮 ����室���.

      � Windows 䠩� letodb.ini ������ ���� ࠧ��饭 � ⮬ ��⠫���, �
 ���஬ ��室���� �ࢥ� letodb.
      � Linux �ࢥ� ��� ��� 䠩� � ⮬ ��⠫���, ��㤠 �� ���⮢��,
 ���, �� ��㤠�, � ��⠫��� /etc.

      4.2 ���ਧ��� ���짮��⥫��

      �⮡� ������� �����⥬� ���ਧ�樨, ����室��� ��⠭����� ���� �� ᫥����� ��ࠬ��஢
 letodb.ini � 1: Pass_for_Login, Pass_for_Manage, Pass_for_Data. �� ��। �⨬
 ����室��� ᮧ����, ��� ������, ������ ���짮��⥫� � �ࠢ��� �����������, ��᪮��� �����
 ��⥬� ���ਧ�樨 ࠡ�⠥�, ⮫쪮 ���ਧ������ ���짮��⥫� � �ࠢ��� ����������� �����
 ���������/�������� ���짮��⥫�� � ��஫�.
      �⮡� �������� ���짮��⥫�, ����室��� ������� �맮� �㭪樨 LETO_USERADD() � ���������
 �ணࠬ��, � �ਬ���:

      LETO_USERADD( "admin", "secret:)", "YYY" )

 ��� "YYY" �� ��ப�, ����� ���� �ࠢ� ���������஢����, �ࠢ����� � �ࠢ� �� ������. �� �����
 ⠪�� �ᯮ�짮���� �ணࠬ�� utils/manager/console.prg, �⮡� ��⠭����� ��� �������� ����� ���ਧ�樨.

 ��� ᮥ������� � �ࢥ஬ � ����묨 ���ਧ�樨 ( ������ ���짮��⥫� � ��஫��) ����室���
 �맢��� �㭪�� LETO_CONNECT().

      5. ���������� � �ࢥ஬ ������᪨� �ணࠬ�

      �⮡� ᪮��������� � �ࢥ஬, �०�� �ᥣ� ����室��� �ਫ�������� rddleto.lib (Windows)
 ��� librddleto.a (Linux) � ᢮��� �ਫ������, � �������� � ��砫� ᢮�� �ணࠬ�� 
 ��� ��ப�:

      REQUEST LETO
      RDDSETDEFAULT( "LETO" )

      ��� ������ 䠩�� dbf �� �ࢥ� ����室��� ��⠢��� � ������ SET PATH TO,
 ��� � ������� USE ���� � �ࢥ�� � �⠭���⭮� �ଥ:
 //ip_address:port/data_path/file_name.

      �᫨ ����� ��ࠬ��� 'DataPath' � ���䨣��樮���� 䠩�� �ࢥ�, � �� �����
 �����⮥ ���祭��, ����室��� 㪠�뢠�� �� ����� ���� � 䠩�� �� �ࢥ�,
 � ���� �⭮�⥫�� ( �⭮�⥫쭮 ���祭�� 'DataPath' ).
      ���ਬ��, �᫨ ����室��� ������ 䠩� test.dbf, ����� �ᯮ����� ��
 �ࢥ� 192.168.5.22 � ��⠫��� /data/mydir � ���祭�� ��ࠬ��� 'DataPath'
 ( � 䠩�� ���䨣��樨 �ࢥ� letodb.ini ) '/data', ᨭ⠪�� ������ ����
 ⠪��:

      USE "//192.168.5.22:2812/mydir/test"

      �᫨ �ࢥ� �� ����饭 ��� �� 㪠���� ������ ����, �㤥� ᣥ���஢��� �訡�� ������.
 �������� �஢���� ����㯭���� �ࢥ� ��। ����⨥� 䠩��� �맮��� �㭪樨
 leto_Connect( cAddress ), ����� ��୥� -1 � ��砥 ��㤠筮� ����⪨:

      IF leto_Connect( "//192.168.5.22:2812/mydir/" ) == -1
         Alert( "Can't connect to server ..." )
      ENDIF

      6. ��ࠢ����� ��६���묨

      Letodb �������� ᮧ������ ��६����, ����� ࠧ�������� ����� �ਫ�����ﬨ,
 ����� ᮥ�������� � �ࢥ஬. ��६���� ����� ࠧ���� �� ��㯯�, � ��� ����� ����� �����᪨�,
 楫� ��� ᨬ����� ⨯. ����� ��⠭����� ���祭�� ��६�����, ������� ���, 㤠����, ��� �맢���
 ���६���/���६��� ( �᫮��� ��६�����, ����⢥��� ). �� ����樨 � ��६���묨 �믮�������
 ��᫥����⥫쭮 � ����� ��⮪�, ⠪ �� ��६���� ����� �ᯮ�짮������ ��� ᥬ����. ��. ᯨ᮪
 �㭪権 ��� ᨭ⠪�� �㦭�� �㭪樨, � �ਬ�� tests/test_var.prg.
      ���� Letodb.ini ����� ᮤ�ঠ�� ��ப� ��� ��।������ ���ᨬ��쭮�� ������⢠ ��६�����
 � ���ᨬ��쭮� ����� ⥪�⮢�� ��६�����.

      7. ���᮪ �㭪権

      ���� �ਢ���� ����� ( �� ������ ����ᠭ�� ) ᯨ᮪ �㭪権,
 ����㯭�� ��� �ᯮ�짮����� � �����᪮� �ਫ������ � �ਫ��������� RDD LETO.

      LETO_CONNECT( cAddress [, cUserName, cPassword ] )   --> nConnection, -1 �� ��㤠�
      LETO_CONNECT_ERR()                                   --> nError
      LETO_DISCONNECT()
      LETO_SETCURRENTCONNECTION( nConnection )
      LETO_GETCURRENTCONNECTION()                          --> nConnection
      LETO_GETSERVERVERSION()                              --> cVersion
      LETO_GETLOCALIP()                                    --> IP ���� ������

      LETO_BEGINTRANSACTION()
      LETO_ROLLBACK()
      LETO_COMMITTRANSACTION( [ lUnlockAll ] )             --> lSuccess
      LETO_INTRANSACTION()                                 --> lTransactionActive

      LETO_SUM( cFieldNames , [ cFilter ], [xScope], [xScopeBottom] )
                                                           --> nSumma - �᫨ ���।��� ���� ����, ���
                                                               {nSumma1, nSumma2, ...} ��� ��᪮�쪨� �����
      LETO_ISFLTOPTIM()                                    --> lFilterOptimized
      LETO_SETSKIPBUFFER( nSkip )
      LETO_SETFASTAPPEND( lFastAppend )                    --> lFastAppend (�।��饥 ���祭��)

      LETO_FILE( cFileName )                               --> lFileExists
      LETO_FERASE( cFileName )                             --> -1 �� ��㤠�
      LETO_FRENAME( cFileName, cFileNewName )              --> -1 �� ��㤠�
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

      8. �⨫�� �ࠢ����� �ࢥ஬

      ����� ᮡ��� ��� �⨫��� �ࠢ����� �ࢥ஬, GUI � ���᮫���. ��室�� ⥪���
 ��室���� � ��⠫��� utils/manage directory.

      �⨫�� GUI, manage.prg, ᮧ���� � ������� ������⥪� HwGUI. �᫨ � ��� ��⠭������ HwGUI,
 ��⠢�� ��ப� 'set HWGUI_INSTALL=' � 䠩� utils/manage/bld.bat ���� � 
 ��⠫��� HwGUI � ������� bld.bat. �⨫�� manage.exe �㤥� ᮡ࠭�.
   
      ��� ��, �� �� �ᯮ���� HwGUI, ���� �⨫�� �ࠢ����� � ०��� ���᮫�,
 console.prg. ������ console.exe � ������� 䠩�� make/bat, ����� �� �ᯮ����
 ��� ᡮન �ணࠬ�� Harbour, ����饩 �� ������ �����. ����室��� ⮫쪮 ��������
 rddleto.lib � ᯨ�� ������⥪. �������e console.exe � ������ �ࢥ� ��� ip ���ᮬ
 � ����஬ ���� � ����⢥ ��ࠬ���:

      console.exe server_name:nPort
      console.exe ip_address:nPort

 server_name � ip_address � ��������� ��ப� ������ ���� ��� ������ ᫥襩
 ( '//' ), ��᪮��� Clipper/Harbour �ᯮ���� �� ��� ᢮�� �㦤.
