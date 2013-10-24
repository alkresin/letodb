/*  $Id: server.prg,v 1.123 2010/08/20 09:17:58 alkresin Exp $  */

/*
 * Harbour Project source code:
 * Leto db server
 *
 * Copyright 2008 Alexander S. Kresin <alex / at / belacy.belgorod.su>
 * www - http://www.harbour-project.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA (or visit the web site http://www.gnu.org/).
 *
 * As a special exception, the Harbour Project gives permission for
 * additional uses of the text contained in its release of Harbour.
 *
 * The exception is that, if you link the Harbour libraries with other
 * files to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the Harbour library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released by the Harbour
 * Project under the name Harbour.  If you copy code from other
 * Harbour Project or Free Software Foundation releases into a copy of
 * Harbour, as the General Public License permits, the exception does
 * not apply to the code that you add in this way.  To avoid misleading
 * anyone as to the status of such modified files, you must delete
 * this exception notice from them.
 *
 * If you write modifications of your own for Harbour, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 *
 */

#include "hbclass.ch"
#include "dbstruct.ch"
#include "rddsys.ch"
#include "common.ch"
#include "dbinfo.ch"
#include "fileio.ch"
#include "error.ch"

#ifndef HB_HRB_BIND_DEFAULT
   #define HB_HRB_BIND_DEFAULT 0x0 
#endif
#define SOCKET_BUFFER_SIZE  8192

   MEMVAR oApp

#ifdef __LINUX__
   ANNOUNCE HB_GTSYS
   REQUEST HB_GT_STD_DEFAULT
   #define DEF_SEP      '/'
   #define DEF_CH_SEP   '\'
#else
#ifndef __CONSOLE__
#ifndef __XHARBOUR__
   ANNOUNCE HB_GTSYS
   REQUEST HB_GT_GUI_DEFAULT
#endif
#endif
   #define DEF_SEP      '\'
   #define DEF_CH_SEP   '/'
#endif

   REQUEST  ABS, ALLTRIM, AT, CHR, CTOD, DATE, DAY, DELETED, DESCEND, DTOC, DTOS, ;
      EMPTY, I2BIN, L2BIN, LEFT, LEN, LOWER, LTRIM, MAX, MIN, MONTH, PAD, PADC, ;
      PADL, PADR, RAT, RECNO, RIGHT, ROUND, RTRIM, SPACE, STOD, STR, STRZERO, ;
      SUBSTR, TIME, TRANSFORM, TRIM, UPPER, VAL, YEAR

   EXTERNAL LETO_ALIAS

/*
 Ads-specific functions
Request  CONTAINS, COLLATE, CTOT, CTOTS, LOWERW, NOW, REVERSE, STOTS, ;
    TODAY, TSTOD, UPPERW
*/

#ifdef __HB_EXT_CDP__
   #include "hbextcdp.ch"
#else
   #include "letocdp.ch"
#endif

#ifdef __CONSOLE__

PROCEDURE Main
   LOCAL nRes
   PUBLIC oApp := HApp():New()

   CLS
   IF ( nRes := leto_CreateMemArea( oApp:nPort % 10000 ) ) == - 1
      ? "Initialization Error ..."
      RETURN
   ENDIF
   @ 1, 5 SAY "Server listening ..."
   @ 2, 5 SAY "Press [ESC] to terminate the program"

   StartServer()

   RETURN

#endif

#ifdef __WIN_DAEMON__

PROCEDURE Main( cCommand )
   LOCAL nRes
   PUBLIC oApp := HApp():New()

   IF ( nRes := leto_CreateMemArea( oApp:nPort % 10000 ) ) == - 1
      MsgStop( "Initialization Error ..." )
      RETURN
   ENDIF
   IF cCommand != Nil .AND. Lower( cCommand ) == "stop"
      IF nRes > 0
         MsgStop( "Server isn't found..." )
      ELSE
         Leto_WriteMemArea( '0', 0, 1 )
      ENDIF
      RETURN
   ELSE
      IF nRes == 0
         MsgStop( "Server already running!" )
         RETURN
      ENDIF
   ENDIF

   StartServer()

   RETURN

#endif

#ifdef __LINUX_DAEMON__

PROCEDURE Main( cCommand )
   LOCAL nRes
   PUBLIC oApp := HApp():New()

   IF cCommand != Nil .AND. Lower( cCommand ) == "stop"

      IF ( nRes := leto_CreateMemArea( oApp:nPort % 10000 ) ) == - 1
         WrLog( "Initialization Error" )
         RETURN
      ENDIF
      IF nRes > 0
         WrLog( "Server isn't found" )
      ELSE
         Leto_WriteMemArea( '0', 0, 1 )
      ENDIF
      RETURN

   ELSE
      IF !leto_Daemon()
         WrLog( "Can't become a daemon" )
         RETURN
      ENDIF
      IF ( nRes := leto_CreateMemArea( oApp:nPort % 10000 ) ) == - 1
         WrLog( "Initialization Error" )
         RETURN
      ENDIF
      IF nRes == 0
         IF cCommand != Nil .AND. Lower( cCommand ) == "force"
            leto_SetAreaNew()
         ELSE
            WrLog( "Server already running!" )
            RETURN
         ENDIF
      ENDIF
   ENDIF

   StartServer()

   RETURN

#endif

PROCEDURE StartServer()

   REQUEST DBFNTX
   REQUEST DBFCDX

   hs_InitSet()

   leto_CreateData()
   WrLog( "Leto DB Server has been started." )

   IF File( "letoudf.hrb" )
#ifndef __XHARBOUR__
     hb_HrbLoad(HB_HRB_BIND_DEFAULT, "letoudf.hrb" )
#else
      __hrbLoad( "letoudf.hrb" )
#endif
      WrLog( "letoudf.hrb has been loaded." )
   ENDIF

   leto_Server( oApp:nPort )

   WrLog( "Server has been closed." )

   RETURN

FUNCTION hs_InitSet()

   rddSetDefault( "DBFCDX" )
   SET AUTOPEN ON

   RETURN Nil

STATIC FUNCTION FilePath( fname )
   LOCAL i

   RETURN iif( ( i := RAt( '\', fname ) ) = 0, ;
      iif( ( i := RAt( '/', fname ) ) = 0, "", Left( fname, i ) ), ;
      Left( fname, i ) )

STATIC FUNCTION GetExten( fname )
   LOCAL i

   IF ( i := RAt( '.', fname ) ) == 0
      RETURN ""
   ELSEIF Max( RAt( '/', fname ), RAt( '\', fname ) ) > i
      RETURN ""
   ENDIF

   RETURN Lower( SubStr( fname, i + 1 ) )

/*
STATIC FUNCTION IndexInfo( i, cName )

   RETURN OrdBagName( i ) + ";" + OrdName( i ) + ";" + cName + ";" + ;
      OrdFor( i ) + ";" + dbOrderInfo( DBOI_KEYTYPE, , i ) + ";" +   ;
      LTrim( Str( dbOrderInfo(DBOI_KEYSIZE,, i ) ) ) + ";"
*/

STATIC FUNCTION cValToChar( uValue )
   LOCAL cType := ValType( uValue )

   IF cType == "L"
      RETURN iif( uValue, "T", "F" )
   ELSEIF cType == "N"
      RETURN AllTrim( Str( uValue ) )
   ELSEIF cType == "D"
      RETURN Dtoc( uValue )
   ENDIF

   RETURN uValue

FUNCTION hs_opentable( nUserStru, cCommand )
   LOCAL nPos, cReply, cLen, cName, cFileName, cAlias, cRealAlias, cFlags
   LOCAL aStru, i, cIndex := ""
   LOCAL nTableStru, nIndexStru, nId, lShared, lReadonly, cdp, nArea
   LOCAL bOldError, lres := .T. , oError, nDriver, lNoSaveWA := leto_GetAppOptions( 11 )
   LOCAL cDataPath, lShareTables

   IF Empty( cName := GetCmdItem( cCommand,1,@nPos ) ) .OR. nPos == 0
      RETURN "-002"
   ENDIF
   nArea := Val( cName )
   IF Empty( cName := GetCmdItem( cCommand,nPos + 1,@nPos ) ) .OR. nPos == 0
      RETURN "-002"
   ENDIF
   cAlias := GetCmdItem( cCommand, nPos + 1, @nPos )
   IF nPos == 0; RETURN "-002"; ENDIF
   cFlags := GetCmdItem( cCommand, nPos + 1, @nPos )
   IF nPos == 0; RETURN "-002"; ENDIF

   lShared := ( Left( cFlags,1 ) == "T" )
   lReadonly := ( SubStr( cFlags,2,1 ) == "T" )

   cdp := GetCmdItem( cCommand, nPos + 1, @nPos )

   IF ( nDriver := leto_getDriver( Lower(cName ) ) ) == Nil
      nDriver := leto_GetAppOptions( 2 )
   ENDIF

   cName := StrTran( cName, DEF_CH_SEP, DEF_SEP )
   IF Empty( getExten( cName ) )
      cName += ".dbf"
   ENDIF

   IF ( nTableStru := leto_FindTable( cName, .T. ) ) == - 1 .OR. lNoSaveWA
      dbSelectArea()
      cRealAlias := "A" + PadL( Select(), 6, '0' )
      lShareTables := leto_GetAppOptions( 10 )
      cDataPath := leto_GetAppOptions( 1 )
      cFileName := cDataPath + cName
      bOldError := ErrorBlock( { |e|break( e ) } )
      BEGIN SEQUENCE
         leto_SetUserEnv( nUserStru )
         dbUseArea( .F. , iif( nDriver == 1,"DBFNTX",Nil ), ;
            cFileName, cRealAlias, ;
            ( lShareTables .AND. lShared ),   ;
            ( lShareTables .AND. lReadOnly ), ;
            Iif( !Empty(cdp ),cdp,Nil ) )
      RECOVER USING oError
         lres := .F.
      END SEQUENCE
      IF ! lres .AND. ( !lShareTables .AND. lReadonly )
          // file read only
          lres := .T.
          BEGIN SEQUENCE
             dbUseArea( .F. , iif( nDriver == 1,"DBFNTX",Nil ), ;
                cFileName, cRealAlias, ;
                ( lShared ),   ;
                ( lReadOnly ), ;
                Iif( !Empty(cdp ),cdp,Nil ) )
          RECOVER USING oError
             lres := .F.
          END SEQUENCE
      ENDIF
      ErrorBlock( bOldError )

      IF !lres
         RETURN ErrorStr(Iif( oError:genCode==EG_OPEN .AND. oError:osCode==32, "-004:","-003:" ), oError, cFileName)
      ENDIF
      IF nTableStru == -1
         nTableStru := leto_InitTable( Select(), cName, nDriver, lShared )
      ENDIF
   ELSE
      IF !leto_SetShared( nTableStru ) .OR. !lShared
         RETURN "-004:21-1023-0-0" + Chr(9) + cName
      ENDIF
      bOldError := ErrorBlock( { |e|break( e ) } )
      BEGIN SEQUENCE
        IF OrdCount() > 0
           OrdSetFocus( 1 )
        ENDIF
        dbGoTop()
      RECOVER USING oError
         lres := .F.
      END SEQUENCE
      ErrorBlock( bOldError )
      IF !lres
         RETURN ErrorStr("-004:", oError, cName)
      ENDIF
   ENDIF

   nId := Select() * 512 + nArea
   leto_InitArea( nUserStru, nTableStru, nId, cAlias )

   aStru := dbStruct()
   cReply := "+" + LTrim( Str( nId ) ) + ";" + ;
      iif( nDriver == 1, "1;", "0;" ) + LTrim( Str( Len(aStru ) ) ) + ";"
   FOR i := 1 TO Len( aStru )
      cReply += aStru[i,DBS_NAME] + ";" + aStru[i,DBS_TYPE] + ";" + ;
         LTrim( Str( aStru[i,DBS_LEN] ) ) + ";" + LTrim( Str( aStru[i,DBS_DEC] ) ) + ";"
   NEXT
   i := 1
   DO WHILE !Empty( cName := OrdKey( i ) )
      IF i == 1
         nIndexStru := leto_InitIndex( nId, nUserStru, OrdBagName( 1 ) )
      ENDIF
      cIndex += Leto_IndexInfo( i, cName )
      leto_addTag( nId, nUserStru, nIndexStru, Lower( OrdName(i ) ) )
      i ++
   ENDDO
   cReply += LTrim( Str( i - 1 ) ) + ";" + cIndex

   cReply += leto_rec( nId, nUserStru )
   cLen := leto_N2B( Len( cReply ) )

   RETURN Chr( Len( cLen ) ) + cLen + cReply

FUNCTION hs_openindex( nUserStru, cCommand )
   LOCAL cReply, nId, cBagName, nPos, nIndexStru
   LOCAL bOldError, oError, lres := .T. , nOrder := 1, cName, cIndex := ""
   LOCAL nCount := 0
   LOCAL cDataPath

   IF Empty( nId := Val( GetCmdItem( cCommand,1,@nPos ) ) )
      RETURN "-002"
   ENDIF
   IF Empty( cBagName := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      RETURN "-002"
   ENDIF
   IF leto_SelectArea( nId, nUserStru )
      cDataPath := leto_GetAppOptions( 1 )
      cBagName  := StrTran( cBagName, DEF_CH_SEP, DEF_SEP )
      IF DEF_SEP $ cBagName
         cBagName := cDataPath + iif( Left( cBagName,1 ) $ DEF_SEP, "", DEF_SEP ) + cBagName
      ELSE
         cBagName := cDataPath + FilePath( leto_TableName( nId,nUserStru ) ) + cBagName
      ENDIF
      bOldError := ErrorBlock( { |e|break( e ) } )
      BEGIN SEQUENCE
         OrdListAdd( cBagName )
         OrdSetFocus( OrdCount() )
         cBagName := OrdBagName()
      RECOVER USING oError
         lres := .F.
      END SEQUENCE
      ErrorBlock( bOldError )

      IF !lres
         RETURN ErrorStr("-003:", oError, cBagName)
      ENDIF

      cReply := "+"
      nOrder := 1
      DO WHILE !Empty( cName := OrdKey( nOrder ) )
         IF cBagName == OrdBagName( nOrder )
            nCount ++
            IF nCount == 1
               nIndexStru := leto_InitIndex( nId, nUserStru, OrdBagName( nOrder ), cBagName )
            ENDIF
            cIndex += Leto_IndexInfo( nOrder, cName )
            leto_addTag( nId, nUserStru, nIndexStru, Lower( OrdName(nOrder ) ) )
         ENDIF
         nOrder ++
      ENDDO
      cReply += LTrim( Str( nCount ) ) + ";" + cIndex
      RETURN cReply + leto_rec( nId, nUserStru )

   ENDIF

   RETURN "-003"

FUNCTION hs_OrderInfo( nUserStru, cCommand )
   LOCAL nDefine, cOrder, nOrder, xNewSetting, nPos, nId

   IF Empty( nId := Val( GetCmdItem( cCommand, 1, @nPos ) ) )
      RETURN "-003"
   ENDIF
   IF Empty( nDefine := Val( GetCmdItem( cCommand, nPos + 1, @nPos ) ) )
      RETURN "-004"
   ENDIF
   cOrder := GetCmdItem( cCommand, nPos + 1, @nPos )
   IF leto_SelectArea( nId, nUserStru )
      nOrder := OrdNumber( cOrder )
      IF Empty( xNewSetting := GetCmdItem( cCommand, nPos + 1, @nPos ) )
         RETURN "+" + cValToChar( DbOrderInfo( nDefine,, nOrder ) ) + ";"
      ELSEIF Ascan( { DBOI_ISDESC, DBOI_CUSTOM, DBOI_UNIQUE }, nDefine ) != 0
         RETURN "+" + cValToChar( DbOrderInfo( nDefine,, nOrder, xNewSetting == "T" ) ) + ";"
      ELSEIF Ascan( { DBOI_FINDREC, DBOI_FINDRECCONT, DBOI_KEYGOTO, DBOI_KEYGOTORAW, DBOI_POSITION, DBOI_RELKEYPOS, DBOI_SKIPUNIQUE }, nDefine ) != 0
         RETURN "+" + cValToChar( DbOrderInfo( nDefine,, nOrder, Val( xNewSetting ) ) ) + ";"
      ELSE
         RETURN "+" + cValToChar( DbOrderInfo( nDefine,, nOrder, xNewSetting ) ) + ";"
      ENDIF
   ENDIF

   RETURN "-010"

FUNCTION hs_createtable( nUserStru, cCommand )
   LOCAL cName, cAlias, cFileName, cExt, i, nLen, cLen, nPos, aStru, cReply
   LOCAL bOldError, oError, lres := .T. , nDriver, nTableStru, cRealAlias, nArea, nId
   LOCAL lAnyExt := leto_GetAppOptions( 4 ), cDataPath := leto_GetAppOptions( 1 )

   IF Empty( cName := GetCmdItem( cCommand,1,@nPos ) ) .OR. nPos == 0
      RETURN "-002"
   ENDIF
   nArea := Val( cName )

   IF Empty( cName := GetCmdItem( cCommand,nPos + 1,@nPos ) ) .OR. nPos == 0
      RETURN "-002"
   ENDIF
   cAlias := GetCmdItem( cCommand,nPos + 1,@nPos )
   IF nPos == 0
      RETURN "-002"
   ENDIF
   IF Empty( nLen := Val( GetCmdItem( cCommand,nPos + 1,@nPos ) ) )
      RETURN "-003"
   ENDIF

   IF !lAnyExt .AND. !Empty( cExt := GetExten( cName ) ) .AND. cExt != "dbf"
      RETURN "-004"
   ENDIF

   aStru := Array( nLen, 4 )
   FOR i := 1 TO nLen
      aStru[i,1] := GetCmdItem( cCommand, nPos + 1, @nPos )
      aStru[i,2] := GetCmdItem( cCommand, nPos + 1, @nPos )
      aStru[i,3] := Val( GetCmdItem( cCommand,nPos + 1,@nPos ) )
      aStru[i,4] := Val( GetCmdItem( cCommand,nPos + 1,@nPos ) )
   NEXT

   IF ( nDriver := leto_getDriver( Lower(cName ) ) ) == Nil
      nDriver := leto_GetAppOptions( 2 )
   ENDIF

   cName := StrTran( cName, DEF_CH_SEP, DEF_SEP )
   IF Empty( getExten( cName ) )
      cName += ".dbf"
   ENDIF

   dbSelectArea()
   cRealAlias := "A" + PadL( Select(), 6, '0' )

   cFileName := cDataPath + cName
   bOldError := ErrorBlock( { |e|break( e ) } )
   BEGIN SEQUENCE
      leto_SetUserEnv( nUserStru )
      dbCreate( cFileName, aStru, iif( nDriver == 1,"DBFNTX",Nil ), .T. , cRealAlias )
   RECOVER USING oError
      lres := .F.
   END SEQUENCE
   ErrorBlock( bOldError )

   IF !lres
      RETURN ErrorStr("-011:", oError, cFileName)
   ENDIF

   nTableStru := leto_InitTable( Select(), cName, nDriver, .F. )

   nId := Select() * 512 + nArea
   leto_InitArea( nUserStru, nTableStru, nId, cAlias )

   cReply := "+" + LTrim( Str( nId ) ) + ";" + ;
      iif( nDriver == 1, "1;", "0;" )

   cReply += leto_rec( nId, nUserStru )
   cLen := leto_N2B( Len( cReply ) )

   RETURN Chr( Len( cLen ) ) + cLen + cReply

FUNCTION hs_createindex( nUserStru, cCommand )
   LOCAL nId, nPos
   LOCAL cBagName, cTagName, cKey, lUnique, cFor, cWhile
   LOCAL bOldError, oError, lres := .T. , nIndexStru
   LOCAL cTemp
   LOCAL lAll, nRecno, nNext, nRecord, lRest, lDescend, lCustom, lAdditive
   LOCAL lAnyExt := leto_GetAppOptions( 4 ), cDataPath := leto_GetAppOptions( 1 )

   IF Empty( nId := Val( GetCmdItem( cCommand,1,@nPos ) ) )
      RETURN "-002"
   ENDIF
   cBagName := GetCmdItem( cCommand, nPos + 1, @nPos )
   cTagName := GetCmdItem( cCommand, nPos + 1, @nPos )
   IF Empty( cBagName ) .AND. Empty( cTagName )
      RETURN "-002"
   ENDIF
   IF Empty( cKey := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      RETURN "-002"
   ENDIF
   lUnique := ( GetCmdItem( cCommand,nPos + 1,@nPos ) == "T" )
   cFor := GetCmdItem( cCommand, nPos + 1, @nPos )
   cWhile := GetCmdItem( cCommand, nPos + 1, @nPos )

   IF !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      lAll := ( cTemp == "T" )
   ENDIF
   IF !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      nRecno := Val( cTemp )
   ENDIF
   IF !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      nNext := Val( cTemp )
   ENDIF
   IF !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      nRecord := Val( cTemp )
   ENDIF
   IF !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      lRest := ( cTemp == "T" )
   ENDIF
   IF !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      lDescend := ( cTemp == "T" )
   ENDIF
   IF !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      lCustom := ( cTemp == "T" )
   ENDIF
   IF nPos != 0 .AND. !Empty( cTemp := GetCmdItem( cCommand,nPos + 1,@nPos ) )
      lAdditive := ( cTemp == "T" )
   ENDIF

   IF !lAnyExt .AND. !Empty( cBagName ) .AND. ;
         !Empty( cTemp := GetExten( cBagName ) ) .AND. ;
         ( Len( cTemp ) < 3 .OR. !( cTemp $ "cdx;idx;ntx" ) )
      RETURN "-004"
   ENDIF

   IF leto_SelectArea( nId, nUserStru )
      IF !Empty( cBagName )
         cBagName := StrTran( cBagName, DEF_CH_SEP, DEF_SEP )
         IF DEF_SEP $ cBagName
            IF ! Empty( cDataPath ) .AND. cBagName <> DEF_SEP
               cBagName := DEF_SEP + cBagName
            ENDIF
            cBagName := cDataPath + iif( Left( cBagName,1 ) $ DEF_SEP, "", DEF_SEP ) + cBagName
         ELSE
            cBagName := cDataPath + FilePath( leto_TableName( nId,nUserStru ) ) + cBagName
         ENDIF
      ENDIF
      bOldError := ErrorBlock( { |e|break( e ) } )
      BEGIN SEQUENCE
         IF ! Empty( cFor ) .OR. ! Empty( cWhile ) .OR. lDescend != Nil .OR. ;
               lAll != Nil .OR. nRecno != Nil .OR. nNext != Nil .OR. nRecord != Nil
            ordCondSet( ;
               iif( !Empty( cFor ), cFor, ), ;
               iif( !Empty( cFor ), &( "{||" + cFor + "}" ), ), ;
               iif( lAll != Nil, lAll, ), ;
               iif( !Empty( cWhile ), &( "{||" + cWhile + "}" ), ), , , ;
               iif( !Empty( nRecno ), nRecno, ), ;
               iif( !Empty( nNext ), nNext, ), ;
               iif( !Empty( nRecord ), nRecord, ), ;
               iif( lRest != Nil, lRest, ), ;
               iif( lDescend != Nil, lDescend, ), ;
               iif( lAdditive != Nil, lAdditive, ), , , ;
               iif( lCustom != Nil, lCustom, ), , ;
               iif( ! Empty( cWhile ), cWhile, ) )
         ENDIF
         leto_SetUserEnv( nUserStru )
         OrdCreate( cBagName, cTagName, cKey, &( "{||" + cKey + "}" ), lUnique )
      RECOVER USING oError
         lres := .F.
      END SEQUENCE
      ErrorBlock( bOldError )

      IF !lres
         RETURN ErrorStr("-003:", oError, cBagName)
      ENDIF

      nIndexStru := leto_InitIndex( nId, nUserStru, OrdBagName( 0 ), cBagName )
      leto_addTag( nId, nUserStru, nIndexStru, Lower( cTagName ) )
      RETURN "++"
   ENDIF

   RETURN "-002"

#define EF_CANRETRY                     1
#define EF_CANSUBSTITUTE                2
#define EF_CANDEFAULT                   4

STATIC FUNCTION ErrorStr(cSign, oError, cFileName)
   LOCAL nFlags := 0
   IF oError:canDefault
      nFlags += EF_CANDEFAULT
   ENDIF
   IF oError:canRetry
      nFlags += EF_CANRETRY
   ENDIF
   IF oError:canSubstitute
      nFlags += EF_CANSUBSTITUTE
   ENDIF

   WrLog( Leto_ErrorMessage( oError ) )

   RETURN cSign + LTrim( Str( oError:genCode ) ) + "-" + LTrim( Str( oError:subCode ) ) ;
          + "-" + LTrim( Str( oError:osCode ) ) + "-" + LTrim( Str( nFlags ) ) ;
          + Chr(9) + cFileName

INIT PROCEDURE INITP
   hb_ipInit()

   RETURN

EXIT PROCEDURE EXITP

   leto_ReleaseData()
   hb_ipCleanup()
   leto_CloseMemArea()

   RETURN

CLASS HApp

   DATA nPort     INIT 2812
   DATA DataPath  INIT ""
   DATA LogFile   INIT ""
   DATA lLower    INIT .F.
   DATA lFileFunc INIT .F.
   DATA lAnyExt   INIT .F.
   DATA lShare    INIT .F.      // .T. - new mode, which allows share tables with other processes
   DATA lNoSaveWA INIT .F.      // .T. - new mode, which forces dbUseArea() each time "open table" is demanded
   DATA nDriver   INIT 0
   DATA lPass4M   INIT .F.
   DATA lPass4L   INIT .F.
   DATA lPass4D   INIT .F.
   DATA cPassName INIT "leto_users"
   DATA lCryptTraffic INIT .F.

   METHOD New()

ENDCLASS

METHOD New() CLASS HApp
   LOCAL cIniName := "letodb.ini"
   LOCAL aIni, i, j, cTemp, cPath, nDriver
   LOCAL nPort
   LOCAL nMaxVars, nMaxVarSize
   LOCAL nCacheRecords := 10

#ifdef __LINUX__

   IF File( cIniName )
      aIni := rdIni( cIniName )
   ELSEIF File( "/etc/" + cIniName )
      aIni := rdIni( "/etc/" + cIniName )
   ELSE
      RETURN Nil
   ENDIF
#else
   aIni := rdIni( cIniName )
#endif
   IF !Empty( aIni )
      FOR i := 1 TO Len( aIni )
         IF aIni[i,1] == "MAIN"
            FOR j := 1 TO Len( aIni[i,2] )
               IF aIni[i,2,j,1] == "PORT"
                  IF ( nPort := Val( aIni[i,2,j,2] ) ) >= 2000
                     ::nPort := nPort
                  ENDIF
               ELSEIF aIni[i,2,j,1] == "DATAPATH"
                  ::DataPath := StrTran( aIni[i,2,j,2], DEF_CH_SEP, DEF_SEP )
                  IF Right( ::DataPath, 1 ) $ DEF_SEP
                     ::DataPath := Left( ::DataPath, Len( ::DataPath ) - 1 )
                  ENDIF
               ELSEIF aIni[i,2,j,1] == "LOG"
                  IF GetExten( aIni[i,2,j,2] ) == "log"
                     ::LogFile := StrTran( aIni[i,2,j,2], DEF_CH_SEP, DEF_SEP )
                  ELSE
                     WrLog( "Wrong logfile extension" )
                  ENDIF
               ELSEIF aIni[i,2,j,1] == "LOWER_PATH"
                  ::lLower := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "ENABLEFILEFUNC"
                  ::lFileFunc := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "ENABLEANYEXT"
                  ::lAnyExt := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "SHARE_TABLES"
                  ::lShare := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "NO_SAVE_WA"
                  ::lNoSaveWA := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "DEFAULT_DRIVER"
                  ::nDriver := iif( Lower( aIni[i,2,j,2] ) == "ntx", 1, 0 )
               ELSEIF aIni[i,2,j,1] == "PASS_FOR_LOGIN"
                  ::lPass4L := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "PASS_FOR_MANAGE"
                  ::lPass4M := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "PASS_FOR_DATA"
                  ::lPass4D := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "PASS_FILE"
                  ::cPassName := aIni[i,2,j,2]
               ELSEIF aIni[i,2,j,1] == "CRYPT_TRAFFIC"
                  ::lCryptTraffic := ( aIni[i,2,j,2] == '1' )
               ELSEIF aIni[i,2,j,1] == "MAX_VARS_NUMBER"
                  nMaxVars := Val( aIni[i,2,j,2] )
               ELSEIF aIni[i,2,j,1] == "MAX_VAR_SIZE"
                  nMaxVarSize := Val( aIni[i,2,j,2] )
               ELSEIF aIni[i,2,j,1] == "CACHE_RECORDS"
                  IF ( nCacheRecords := Val( aIni[i,2,j,2] ) ) <= 0
                      nCacheRecords := 10
                  ENDIF
               ENDIF
            NEXT
         ELSEIF aIni[i,1] == "DATABASE"
            cPath := nDriver := Nil
            FOR j := 1 TO Len( aIni[i,2] )
               IF aIni[i,2,j,1] == "DATAPATH"
                  cPath := StrTran( aIni[i,2,j,2], DEF_CH_SEP, DEF_SEP )
                  IF Right( cPath, 1 ) $ DEF_SEP
                     cPath := Left( cPath, Len( cPath ) - 1 )
                  ENDIF
               ELSEIF aIni[i,2,j,1] == "DRIVER"
                  nDriver := iif( ( cTemp := Lower( aIni[i,2,j,2] ) ) == "cdx", ;
                     0, iif( cTemp == "ntx", 1, Nil ) )
               ENDIF
            NEXT
            IF cPath != Nil
               leto_AddDataBase( cPath, iif( nDriver == Nil,::nDriver,nDriver ) )
            ENDIF
         ENDIF
      NEXT
   ENDIF

   IF ::lLower
      SET( _SET_FILECASE, 1 )
      SET( _SET_DIRCASE, 1 )
   ENDIF
   IF ::lNoSaveWA
      ::lShare := .T.
   ENDIF

   leto_SetAppOptions( iif( Empty(::DataPath ),Nil,::DataPath ), ::nDriver, ::lFileFunc, ;
         ::lAnyExt, ::lPass4L, ::lPass4M, ::lPass4D, ::cPassName, ::lCryptTraffic, ;
         ::lShare, ::lNoSaveWA, nMaxVars, nMaxVarSize, nCacheRecords )

   RETURN Self

#ifdef __XHARBOUR__

PROCEDURE HB_GT_WIN (); RETURN

#endif
