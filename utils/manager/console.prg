/*  $Id: console.prg,v 1.13 2010/01/21 10:18:26 alkresin Exp $  */

/*
 * Harbour Project source code:
 * Console management utility for Leto db server
 *
 * Copyright 2008 Alexander S.Kresin <alex@belacy.belgorod.su>
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
#include "rddleto.ch"

Static CRLF

Function Main( cAddress, cUser, cPasswd )
Local arr, nKey := 49, nRes

   IF cAddress == Nil
      ? "Server address is absent ..."
      Return Nil
   ENDIF

   CRLF := Chr(13)+Chr(10)
   hb_IPInit()

   cAddress := "//" + cAddress + "/"
   ? "Connecting to " + cAddress
   IF ( leto_Connect( cAddress, cUser, cPasswd ) ) == -1
      nRes := leto_Connect_Err()
      IF nRes == LETO_ERR_LOGIN
         ? "Login failed"
      ELSEIF nRes == LETO_ERR_RECV
         ? "Recv Error"
      ELSEIF nRes == LETO_ERR_SEND
         ? "Send Error"
      ELSE
         ? "No connection"
      ENDIF

      Return Nil
   ENDIF
   ? "Connected to " + leto_GetServerVersion()

   DO WHILE nKey != 27

      ?
      IF nKey == 49
         BasicInfo()
      ELSEIF nKey == 50
         UsersInfo()
      ELSEIF nKey == 51
         TablesInfo()
      ELSEIF nKey == 57
         Admin()
      ENDIF
      ?
      ? "Press ESC to quit, 1 - connection info, 2 - users list, 3 - tables, 9 - administration"
      nKey := Inkey(0)
   ENDDO

Return Nil

Static Function BasicInfo()
Local aInfo, nSec, nDay, nHour, oldc, nTransAll, nTransBad

   IF ( aInfo := leto_MgGetInfo() ) == Nil
      Return .F.
   ENDIF

   oldc := SETCOLOR( "GR+/N" )

   ? "Users   current:" + Padl( aInfo[1],12 )
   ?? "   Max: " + Padl( aInfo[2],12 )
   ? "Tables  current:" + Padl( aInfo[3],12 )
   ?? "   Max: " + Padl( aInfo[4],12 )
   nSec := Val( aInfo[5] )
   nDay := Int(nSec/86400)
   nHour := Int((nSec%86400)/3600)
   ? "Time elapsed:   " + Padl(Ltrim(Str(nDay)) + Iif(nDay==1," day "," days ") + ;
               Ltrim(Str(nHour))+Iif(nHour==1," hour "," hours ") + ;
               Ltrim(Str(Int((nSec%3600)/60))) + " min",15)
   ? "Operations:     " + Padl( aInfo[6],12)
   ? "KBytes sent:    " + Padl(Int( Val( aInfo[7] ) /1024 ),12)
   ? "KBytes read:    " + Padl(Int( Val( aInfo[8] ) /1024 ),12)
   IF !Empty( aInfo[14] )
      nTransAll := Val( aInfo[14] )
      nTransBad := nTransAll - Val( aInfo[15] )
      ? "Transactions All:" + Str( nTransAll,11)
      ?? "   Bad: " + Str( nTransBad,12 )
   ENDIF
   ? "Waiting current:" + Padl( aInfo[13],12)
   ?? "   Max: " + Padl( aInfo[12],12 )

   SETCOLOR( oldc )

Return .T.


Static Function UsersInfo()
Local aInfo, cData, nPos := 1, i, cItem, nUsers, oldc

   IF ( aInfo := leto_MgGetUsers() ) == Nil
      Return .F.
   ENDIF

   oldc := SETCOLOR( "BG+/N" )
   nUsers := Len( aInfo )
   FOR i := 1 TO nUsers
      ? Padr( aInfo[i,2],15 )
      ?? Padr( aInfo[i,3],18 )
      ?? Padr( aInfo[i,4],18 )
      ?? Padl(Ltrim(Str(Int((Val(aInfo[i,5])%86400)/3600))),2,'0') ;
         +":"+ Padl(Ltrim(Str(Int((Val(aInfo[i,5])%3600)/60))),2,'0') +":"+ ;
         Padl(Ltrim(Str(Int(Val(aInfo[i,5])%60))),2,'0')
   NEXT

   SETCOLOR( oldc )
Return .T.

Static Function TablesInfo()
Local aInfo, cData, nPos := 1, i, cItem, nTables, oldc

   IF ( aInfo := leto_MgGetTables() ) == Nil
      Return .F.
   ENDIF

   oldc := SETCOLOR( "G+/N" )
   nTables := Len( aInfo )
   FOR i := 1 TO nTables
      ? aInfo[i,2]
   NEXT

   SETCOLOR( oldc )
Return .T.

Static Function Admin()
Local nKey := 0, arr

   DO WHILE nKey != 48
      ? "1 Add user"
      ? "2 Change password"
      ? "3 Change access rights"
      ? "4 Flush changes"
      ? "0 Exit"
      ?
      nKey := Inkey(0)
      IF nKey == 49
         IF( arr := GetUser( .t.,.t. ) ) != Nil
            IF leto_useradd( arr[1], arr[2], arr[3] )
               EchoColor( "User is added", "GR+/N" )
            ELSE
               EchoColor( "User is not added", "R+/N" )
            ENDIF
         ELSE
            EchoColor( "Operation canceled", "R+/N" )
         ENDIF
      ELSEIF nKey == 50
         IF( arr := GetUser( .t.,.f. ) ) != Nil
            IF leto_userpasswd( arr[1], arr[2] )
               EchoColor( "Password is changed", "GR+/N" )
            ELSE
               EchoColor( "Password is not changed", "R+/N" )
            ENDIF
         ELSE
            EchoColor( "Operation canceled", "R+/N" )
         ENDIF
      ELSEIF nKey == 51
         IF( arr := GetUser( .f.,.t. ) ) != Nil
            IF leto_userrights( arr[1], arr[3] )
               EchoColor( "Rights are changed", "GR+/N" )
            ELSE
               EchoColor( "Rights are not changed", "R+/N" )
            ENDIF
         ELSE
            EchoColor( "Operation canceled", "R+/N" )
         ENDIF
      ELSEIF nKey == 52
         leto_userflush()
         EchoColor( "OK", "GR+/N" )
      ENDIF
      ?
   ENDDO

Return .T.

Static Function GetUser( lPass, lRights )
Local cUser, cPass, cRights, i

   ACCEPT "User name :" TO cUser
   IF Empty(cUser)
      Return Nil
   ELSEIF Len( cUser ) > 15
      EchoColor( "Too long", "R+/N" )
      Return Nil
   ENDIF

   IF lPass
      ACCEPT "Password  :" TO cPass
      IF Empty(cPass)
         Return Nil
      ELSEIF Len( cPass ) > 15
         EchoColor( "Too long", "R+/N" )
         Return Nil
      ENDIF
   ENDIF

   IF lRights
      ACCEPT "Access rights - Admin,Manage,Wright (default - NNN) :" TO cRights
      IF Len( cRights ) > 3
         EchoColor( "Too long", "R+/N" )
         Return Nil
      ELSE
         FOR i := 1 TO Len(cRights)
            IF !( Substr(cRights,i,1) $ "NY" )
               EchoColor( "Only 'N' and 'Y' chars are permitted", "R+/N" )
               Return Nil
            ENDIF
         NEXT
      ENDIF
   ENDIF

Return { cUser, cPass, cRights }

Function EchoColor( cText, cColor )
Local oldc := SetColor( cColor )

   ? cText
   SetColor( oldc )
Return Nil

FUNCTION CutPath( fname )
LOCAL i
RETURN IIF( ( i := RAT( '\', fname ) ) = 0, ;
           IIF( ( i := RAT( '/', fname ) ) = 0, fname, SUBSTR( fname, i+1 ) ), ;
           SUBSTR( fname, i+1 ) )

Static Function WrLog( cText,cFile )
LOCAL nHand

  IF cFile == Nil
     cFile := "a.log"
  ENDIF
   
  IF !File( cFile )           
     nHand := Fcreate( cFile )
  ELSE                                     
     nHand := Fopen( cFile,1)
  ENDIF
  IF Ferror() != 0                           
     Return .F.
  ENDIF

  Fseek(nHand, 0,2 )
  Fwrite( nHand, cText + Chr(13) + Chr(10) )
  Fclose( nHand )

Return .T.

EXIT PROCEDURE EXIPROC

   leto_DisConnect()
   hb_IPCleanup()
RETURN
