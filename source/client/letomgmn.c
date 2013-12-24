/*  $Id: letomgmn.c,v 1.35 2010/08/19 15:43:07 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Harbour Leto management functions
 *
 * Copyright 2008 Pavel Tsarenko <tpe2 / at / mail.ru>
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

#include "hbapi.h"
#include "hbapiitm.h"
#include "rddleto.h"
#ifdef __XHARBOUR__
#include "hbstack.h"
#endif

#if defined( __XHARBOUR__ ) || ( defined( __HARBOUR__ ) && ( __HARBOUR__ - 0 <= 0x010100 ) )
   #define hb_snprintf snprintf
#endif

extern LETOCONNECTION * pCurrentConn;
extern USHORT uiConnCount;

BOOL leto_SetOptions( void );
void leto_ConnectionClose( LETOCONNECTION * pConnection );
LETOCONNECTION * leto_getConnectionPool( void );
LETOCONNECTION * leto_getConnection( int iParam );
char * leto_DecryptText( LETOCONNECTION * pConnection, ULONG * pulLen );
BOOL leto_CheckArea( LETOAREAP pArea );

#ifdef __XHARBOUR__

char * hb_itemSerialize( PHB_ITEM pItem, HB_BOOL fNumSize, HB_SIZE *pnSize );
PHB_ITEM hb_itemDeserialize( const char ** pBufferPtr, HB_SIZE * pnSize );

int hb_stor( int iParam )
{
   HB_TRACE(HB_TR_DEBUG, ("hb_stor(%d)", iParam));

   if( iParam == -1 )
   {
      hb_ret( );
      return 1;
   }
   else if( iParam >= 0 && iParam <= hb_pcount() )
   {
      PHB_ITEM pItem = hb_stackItemFromBase( iParam );

      if( HB_IS_BYREF( pItem ) )
      {
         hb_itemClear( hb_itemUnRef( pItem ) );
         return 1;
      }
   }

   return 0;
}
#endif

static LETOCONNECTION * letoParseFile( const char *szSource, char *szFile)
{
   LETOCONNECTION * pConnection = NULL;
   char szAddr[96];
   int iPort;

   if( leto_getIpFromPath( szSource, szAddr, &iPort, szFile, TRUE ) &&
       ( ( ( pConnection = leto_ConnectionFind( szAddr, iPort ) ) != NULL ) ||
         ( leto_SetOptions() && 
         ( ( pConnection = LetoConnectionNew( szAddr, iPort, NULL, NULL, 0 ) ) ) != NULL ) ) )
   {
      unsigned int uiLen = strlen( szFile );

      if( szFile[uiLen-1] != '/' && szFile[uiLen-1] != '\\' )
      {
         szFile[uiLen++] = szFile[0];
         szFile[uiLen] = '\0';
      }
      leto_getFileFromPath( szSource, szFile+uiLen );
   }
   return pConnection;
}

HB_FUNC( LETO_FERROR )
{
   hb_retni( LetoGetError() );
}

LETOCONNECTION * letoParseParam( const char * szParam, char * szFile )
{
   LETOCONNECTION * pConnection;

   if( strlen( szParam ) >= 2 && szParam[0] == '/' && szParam[1] == '/' )
   {
      pConnection = letoParseFile( szParam, szFile);
   }
   else
   {
      strcpy( szFile, szParam );
      pConnection = pCurrentConn;
   }
   return pConnection;
}

HB_FUNC( LETO_FILE )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
      hb_retl( LetoIsFileExist( pConnection, szFile ) );
   else
      hb_retl( FALSE );
}

HB_FUNC( LETO_FERASE )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {

      if( LetoFileErase( pConnection, szFile ) )
         hb_retni( 0 );
      else
         hb_retni( -1 );
   }
   else
      hb_retni( -1 );
}

HB_FUNC( LETO_FRENAME )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && HB_ISCHAR(2) && (pConnection = letoParseParam( hb_parc(1), szFile) ) != NULL )
   {
      if( LetoFileRename( pConnection, szFile, (char *)leto_RemoveIpFromPath( hb_parc(2) ) ) )
         hb_retni( 0 );
      else
         hb_retni( -1 );
   }
   else
      hb_retni( -1 );

}

HB_FUNC( LETO_MEMOREAD )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1], * ptr;
   unsigned long ulMemoLen;

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      if( ( ptr = LetoMemoRead( pConnection, szFile, &ulMemoLen ) ) != NULL )
      {
         hb_retclen( ptr, ulMemoLen );
         free( ptr );
      }
      else
         hb_retc("");
   }
   else
      hb_retc( "" );
}

// leto_FileRead( cFile, nStart, nLen, @cBuf )
HB_FUNC( LETO_FILEREAD )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1], *ptr;
   unsigned long ulLen = hb_parnl(3);

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && ulLen > 0 && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      if( ( ptr = LetoFileRead( pConnection, szFile, hb_parnl(2), &ulLen ) ) != NULL )
      {
         hb_storclen( ptr, ulLen, 4 );
         hb_retnl( ulLen );
         free( ptr );
         return;
      }
   }
   hb_retnl( -1 );
}

HB_FUNC( LETO_MEMOWRITE )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && HB_ISCHAR(2) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
      hb_retl( LetoMemoWrite( pConnection, szFile, (char*)hb_parc(2), hb_parclen(2) ) );
   else
      hb_retl( FALSE );
}

// leto_FileWrite( cFile, nStart, cBuf )
HB_FUNC( LETO_FILEWRITE )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];
   unsigned long ulBufLen = hb_parclen(3);

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && HB_ISCHAR(3) && ulBufLen > 0 && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
      hb_retl( LetoFileWrite( pConnection, szFile, (char*)hb_parc(3), hb_parnl(2), ulBufLen ) );
   else
      hb_retl( FALSE );
}

// leto_FileSize( cFile )
HB_FUNC( LETO_FILESIZE )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      hb_retnl( LetoFileSize( pConnection, szFile ) );
      return;
   }

   hb_retnl( -1 );
}

// leto_Directory( cPathSpec, cAttributes )
HB_FUNC( LETO_DIRECTORY )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1], * ptr, *pData ;
   PHB_ITEM aInfo;

   LetoSetError( -1 );
   aInfo = hb_itemArrayNew( 0 );

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      if( ( pData = ptr = LetoDirectory( pConnection, szFile, (char*)((HB_ISCHAR(1))? hb_parc(1) : NULL) ) )  != NULL )
      {
         USHORT uiItems;
         ULONG ulData;
         PHB_ITEM pSubarray = hb_itemNew( NULL );

         uiItems = 1;
         while( LetoGetCmdItem( &ptr, szFile ) )
         {
            ptr ++;
            if( uiItems == 1 )
               hb_arrayNew( pSubarray, 5 );

            switch( uiItems )
            {
               case 1:
               case 4:
               case 5:
                  hb_arraySetC( pSubarray, uiItems, szFile );
                  break;
               case 2:
                  sscanf( szFile, "%lu", &ulData );
                  hb_arraySetNInt( pSubarray, 2, ulData );
                  break;
               case 3:
                  sscanf( szFile, "%lu", &ulData );
                  hb_arraySetDL( pSubarray, 3, ulData );
                  break;
            }
            uiItems ++;
            if( uiItems > 5 )
            {
               uiItems = 1;
               hb_arrayAddForward( aInfo, pSubarray );
            }
         }
         free( pData );
         hb_itemRelease( pSubarray );
      }
   }
   hb_itemReturnRelease( aInfo );
}

HB_FUNC( LETO_MAKEDIR )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   LetoSetError( -10 );
   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      if( LetoMakeDir( pConnection, szFile ) )
         hb_retni( 0 );
      else
         hb_retni( -1 );
   }
   else
      hb_retni( -1 );
}

HB_FUNC( LETO_CONNECT )
{
   LETOCONNECTION * pConnection;
   char szAddr[96];
   char szPath[_POSIX_PATH_MAX + 1];
   int iPort;
   const char * szUser = (HB_ISNIL(2)) ? NULL : hb_parc(2);
   const char * szPass = (HB_ISNIL(3)) ? NULL : hb_parc(3);
   int iTimeOut = hb_parni(4);

   szPath[0] = '\0';
   if( HB_ISCHAR(1) && leto_getIpFromPath( hb_parc(1), szAddr, &iPort, szPath, FALSE ) &&
       ( ( ( pConnection = leto_ConnectionFind( szAddr, iPort ) ) != NULL ) ||
         ( leto_SetOptions() &&
         ( ( pConnection = LetoConnectionNew( szAddr, iPort, szUser, szPass, iTimeOut ) ) ) != NULL ) ) )
   {
      pCurrentConn = pConnection;
      if( HB_ISNUM(5) )
         pConnection->uiBufRefreshTime = hb_parni(5);

      if( strlen(szPath) > 1 && pConnection->szPath == NULL )
         LetoSetPath( pConnection, szPath );
      hb_retni( pConnection - leto_getConnectionPool() + 1 );
   }
   else
   {
      pCurrentConn = NULL;
      hb_retni( -1 );
   }
}

HB_FUNC( LETO_DISCONNECT )
{
   LETOCONNECTION * pConnection = leto_getConnection( 1 );

   if( pConnection )
   {
      leto_ConnectionClose( pConnection );
      if( pConnection == pCurrentConn )
         pCurrentConn = NULL;
   }
   hb_ret( );
}

HB_FUNC( LETO_SETCURRENTCONNECTION )
{
   USHORT uiConn = hb_parni(1);

   if( uiConn && uiConn <= uiConnCount )
      pCurrentConn = leto_getConnectionPool() + uiConn - 1;
   else
      pCurrentConn = NULL;
   hb_ret( );
}

HB_FUNC( LETO_GETCURRENTCONNECTION )
{
   if( pCurrentConn )
      hb_retni( pCurrentConn - leto_getConnectionPool() + 1 );
   else
      hb_retni( -1 );
}

HB_FUNC( LETO_GETSERVERVERSION )
{
   LETOCONNECTION * pConnection = leto_getConnection( 2 );
   if( pConnection )
      hb_retc( HB_ISNIL( 1 ) ? LetoGetServerVer( pConnection ) : pConnection->szVerHarbour );
   else
      hb_retc( "" );
}

HB_FUNC( LETO_PATH )
{
   LETOCONNECTION * pConnection = leto_getConnection( 2 );
   if( pConnection )
   {
      hb_retc( pConnection->szPath ? pConnection->szPath : "");
      if( HB_ISCHAR(1) )
      {
         LetoSetPath( pConnection, hb_parc( 1 ) );
      }
   }
   else
      hb_retc( "" );
}

HB_FUNC( LETO_MGGETINFO )
{
   char * ptr;

   if( pCurrentConn )
   {
      if( ( ptr = LetoMgGetInfo( pCurrentConn ) ) != NULL )
      {
         PHB_ITEM temp;
         PHB_ITEM aInfo;
         char szData[_POSIX_PATH_MAX + 1];
         int i;

         if( *(ptr-1) == '+' )
         {
            aInfo = hb_itemArrayNew( 17 );
            for( i=1; i<=17; i++ )
            {
               if( !LetoGetCmdItem( &ptr, szData ) )
                  break;

               ptr ++;
               temp = hb_itemPutCL( NULL, szData, strlen(szData) );
               hb_itemArrayPut( aInfo, i, temp );
               hb_itemRelease( temp );
            }

            hb_itemReturn( aInfo );
            hb_itemRelease( aInfo );
         }
      }
   }
}

HB_FUNC( LETO_MGGETUSERS )
{
   char szData[64], *ptr;

   if( pCurrentConn )
   {
      if( ( ptr = LetoMgGetUsers( pCurrentConn, ( (HB_ISNIL(1))? NULL : hb_parc(1) ) ) ) != NULL )
      {
         int iUsers, i, j;
         PHB_ITEM pArray, pArrayItm;

         if( *(ptr-1) == '+' )
         {
            if( !LetoGetCmdItem( &ptr, szData ) ) return; ptr ++;
            sscanf( szData, "%d", &iUsers );
            pArray = hb_itemArrayNew( iUsers );
            for ( i = 1; i <= iUsers; i++ )
            {
               pArrayItm = hb_arrayGetItemPtr( pArray, i );
               hb_arrayNew( pArrayItm, 5 );
               for( j=1; j<=5; j++ )
               {
                  if( !LetoGetCmdItem( &ptr, szData ) ) return; ptr ++;
                  hb_itemPutC( hb_arrayGetItemPtr( pArrayItm, j ), szData );
               }
            }
            hb_itemReturn( pArray );
            hb_itemRelease( pArray );
         }
      }
   }
}

HB_FUNC( LETO_MGGETTABLES )
{
   char szData[_POSIX_PATH_MAX + 1], * ptr;

   if( pCurrentConn )
   {
      if( ( ptr = LetoMgGetTables( pCurrentConn, ( (HB_ISNIL(1))? NULL : hb_parc(1) ) ) ) != NULL )
      {
         int iTables, i, j;
         PHB_ITEM pArray, pArrayItm;

         if( *(ptr-1) == '+' )
         {
            if( !LetoGetCmdItem( &ptr, szData ) ) return; ptr ++;
            sscanf( szData, "%d", &iTables );
            pArray = hb_itemArrayNew( iTables );
            for ( i = 1; i <= iTables; i++ )
            {
               pArrayItm = hb_arrayGetItemPtr( pArray, i );
               hb_arrayNew( pArrayItm, 2 );
               for( j=1; j<=2; j++ )
               {
                  if( !LetoGetCmdItem( &ptr, szData ) ) return; ptr ++;
                  hb_itemPutC( hb_arrayGetItemPtr( pArrayItm, j ), szData );
               }
            }
            hb_itemReturn( pArray );
            hb_itemRelease( pArray );
         }
      }
   }
}

HB_FUNC( LETO_MGKILL )
{
   char szData[32];

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) )
         LetoMgKillUser( pCurrentConn, hb_parc(1) );
   }
}

HB_FUNC( LETO_MGGETTIME )
{
   char * ptr;

   if( pCurrentConn )
   {
      if( ( ptr = LetoMgGetTime( pCurrentConn ) ) != NULL )
      {
         PHB_ITEM temp;
         PHB_ITEM aInfo;
         char szData[32];
         int i;

         if( *(ptr-1) == '+' )
         {
            aInfo = hb_itemArrayNew( 2 );
            for( i = 1; i <= 2; i++ )
            {
               if( !LetoGetCmdItem( &ptr, szData ) )
               {
                  hb_itemReturn( aInfo );
                  hb_itemRelease( aInfo );
                  return;
               }
               ptr ++;
               if( i == 1 )
               {
                  int iOvf;
                  ULONG ulDate = hb_strValInt( szData, &iOvf );
                  temp = hb_itemPutDL( NULL, ulDate );
               }
               else
               {
                  temp = hb_itemPutND( NULL, hb_strVal( szData, 10 ) );
               }
               hb_itemArrayPut( aInfo, i, temp );
               hb_itemRelease( temp );
            }

            hb_itemReturn( aInfo );
            hb_itemRelease( aInfo );
         }
      }
   }
}

HB_FUNC( LETO_SETSKIPBUFFER )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();

   if( leto_CheckArea( pArea ) )
   {
      if( !HB_ISNIL(1) )
      {
         LETOCONNECTION * pConnection = leto_getConnectionPool() + pArea->pTable->uiConnection;
         if( LetoCheckServerVer( pConnection, 206 ) )
         {
            pArea->uiSkipBuf = hb_parni(1);
         }
         else
         {
            char szData[32];
            hb_snprintf( szData, 32, "set;02;%lu;%d;\r\n", pArea->pTable->hTable, hb_parni(1) );
            leto_DataSendRecv( pConnection, szData, 0 );
         }
      }
      hb_retni( pArea->Buffer.uiShoots );
   }
   else
      hb_retni( 0 );
}

HB_FUNC( LETO_SETSEEKBUFFER )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();

   if( leto_CheckArea( pArea ) && pArea->pTagCurrent )
   {
      if( HB_ISNUM(1) )
      {
         USHORT uiNum = hb_parni( 1 );
         pArea->pTagCurrent->uiBufSize = uiNum;
         if( uiNum == 0 )
            pArea->pTagCurrent->Buffer.ulBufDataLen = 0;
      }
      hb_retni( pArea->pTagCurrent->Buffer.uiShoots );
   }
   else
      hb_retni( 0 );
}

HB_FUNC( LETO_USERADD )
{
   char szData[96];
   char szPass[54];
   const char * szAccess = ( HB_ISNIL(3) )? "" : hb_parc(3);
   ULONG ulLen;
   char szKey[LETO_MAX_KEYLENGTH+1];

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         if( ( ulLen = hb_parclen(2) ) > 0 )
         {
            USHORT uiKeyLen = strlen(LETO_PASSWORD);

            if( uiKeyLen > LETO_MAX_KEYLENGTH )
               uiKeyLen = LETO_MAX_KEYLENGTH;

            memcpy( szKey, LETO_PASSWORD, uiKeyLen );
            szKey[uiKeyLen] = '\0';
            if( pCurrentConn->cDopcode[0] )
            {
               szKey[0] = pCurrentConn->cDopcode[0];
               szKey[1] = pCurrentConn->cDopcode[1];
            }
            leto_encrypt( hb_parc(2), ulLen, szData, &ulLen, szKey );
            leto_byte2hexchar( szData, (int)ulLen, szPass );
            szPass[ulLen*2] = '\0';
         }

         hb_snprintf( szData, 96, "admin;uadd;%s;%s;%s;\r\n", hb_parc(1), szPass, szAccess );
         if( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
         {
            char * ptr = leto_firstchar();
            hb_retl( *ptr == '+' );
            return;
         }
      }
   }
   hb_retl( 0 );
}

HB_FUNC( LETO_USERPASSWD )
{
   char szData[96];
   char szPass[54];
   ULONG ulLen;
   char szKey[LETO_MAX_KEYLENGTH+1];

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         if( ( ulLen = hb_parclen(2) ) > 0 )
         {
            USHORT uiKeyLen = strlen(LETO_PASSWORD);

            if( uiKeyLen > LETO_MAX_KEYLENGTH )
               uiKeyLen = LETO_MAX_KEYLENGTH;

            memcpy( szKey, LETO_PASSWORD, uiKeyLen );
            szKey[uiKeyLen] = '\0';
            if( pCurrentConn->cDopcode[0] )
            {
               szKey[0] = pCurrentConn->cDopcode[0];
               szKey[1] = pCurrentConn->cDopcode[1];
            }
            leto_encrypt( hb_parc(2), ulLen, szData, &ulLen, szKey );
            leto_byte2hexchar( szData, (int)ulLen, szPass );
            szPass[ulLen*2] = '\0';
         }

         hb_snprintf( szData, 96, "admin;upsw;%s;%s;\r\n", hb_parc(1), szPass );
         if( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
         {
            char * ptr = leto_firstchar();
            hb_retl( *ptr == '+' );
            return;
         }
      }
   }
   hb_retl( 0 );
}

HB_FUNC( LETO_USERRIGHTS )
{
   char szData[96];

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         hb_snprintf( szData, 96, "admin;uacc;%s;%s;\r\n", hb_parc(1), hb_parc(2) );
         if( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
         {
            char * ptr = leto_firstchar();
            hb_retl( *ptr == '+' );
            return;
         }
      }
   }
   hb_retl( 0 );
}

HB_FUNC( LETO_USERFLUSH )
{
   char szData[24];

   if( pCurrentConn )
   {
      hb_snprintf( szData, 24, "admin;flush;\r\n" );
      if( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
      {
         char * ptr = leto_firstchar();
         hb_retl( *ptr == '+' );
         return;
      }
   }
   hb_retl( 0 );
}

HB_FUNC( LETO_USERGETRIGHTS )
{

   if( pCurrentConn )
   {
      hb_retclen( pCurrentConn->szAccess,3 );
   }
}

HB_FUNC( LETO_LOCKCONN )
{
   char szData[24];

   if( HB_ISLOG(1) && pCurrentConn )
   {
      hb_snprintf( szData, 24, "admin;lockc;%c;\r\n", ( hb_parl(1) ? 'T' : 'F' ) );
      if( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
      {
         char * ptr = leto_firstchar();
         hb_retl( *ptr == '+' );
         return;
      }
   }
   hb_retl( 0 );
}

HB_FUNC( LETO_LOCKLOCK )
{
   char szData[24];

   if( pCurrentConn )
   {
      USHORT uiSecs = HB_ISNUM(2) ? hb_parni(2) : 30;
      hb_snprintf( szData, 24, "admin;lockl;%c;%d;\r\n", ( HB_ISLOG(1) ? ( hb_parl(1) ? 'T' : 'F' ) : '?'), uiSecs );
      if( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
      {
         char * ptr = leto_firstchar();
         hb_retl( *ptr == '+' );
         return;
      }
   }
   hb_retl( 0 );
}

/*
 * LETO_VARSET( cGroupName, cVarName, xValue[, nFlags[, @xRetValue]] ) --> lSuccess
 */
HB_FUNC( LETO_VARSET )
{
   char *ptr;
   char szValue[32], cType;
   LONG lValue;
   unsigned int uiRes = 0;
   USHORT uiFlags = (HB_ISNIL(4))? 0 : hb_parni(4);
   BOOL bPrev = HB_ISBYREF( 5 );
   char ** pRetValue;

   LetoSetError( -10 );
   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) && !HB_ISNIL(3) )
      {
         if( HB_ISLOG(3) )
         {
            cType = '1';
            *szValue = ( hb_parl(3) )? '1' : '0';
            *(szValue+1) = '\0';
         }
         else if( HB_ISNUM(3) )
         {
            cType = '2';
            hb_snprintf( szValue, 32, "%ld", hb_parnl(3) );
         }
         else if( HB_ISCHAR(3) )
            cType = '3';
         else
         {
            hb_retl( 0 );
            return;
         }
         uiRes = LetoVarSet( pCurrentConn, (char*)hb_parc(1), (char*)hb_parc(2), cType, 
            ((cType=='3')? (char*)hb_parc(3) : szValue), uiFlags, ((bPrev)? pRetValue : NULL) );

         if( uiRes && bPrev )
         {
            ptr = *pRetValue;
            cType = *ptr;
            ptr += 2;
            if( cType == '1' )
               hb_storl( *ptr == '1', 5 );
            else if( cType == '2' )
            {
               sscanf( ptr, "%ld", &lValue );
               hb_stornl( lValue, 5 );
            }
            else if( cType == '3' )
            {
               hb_storclen( ptr, strlen( ptr ), 5 );
            }
            else
            {
                hb_stor( 5 );
            }
            free( *pRetValue );
         }
      }
   }
   hb_retl( uiRes );
}

/*
 * LETO_VARGET( cGroupName, cVarName ) --> xValue
 */
HB_FUNC( LETO_VARGET )
{
   char *pData;
   char cType;
   LONG lValue;

   LetoSetError( -10 );
   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         if( ( pData = LetoVarGet( pCurrentConn, (char*)hb_parc(1), (char*)hb_parc(2) ) ) != NULL )
         {
            cType = *pData;
            if( cType == '1' )
               hb_retl( *(pData+2) == '1' );
            else if( cType == '2' )
            {
               sscanf( pData+2, "%ld", &lValue );
               hb_retnl( lValue );
            }
            else
            {
               hb_retc( pData+2 );
            }
            free( pData );
            return;
         }
      }
   }
   hb_ret();
}

/*
 * LETO_VARINCR( cGroupName, cVarName[, nFlags ) --> nValue
 */
HB_FUNC( LETO_VARINCR )
{
   LONG lValue;

   LetoSetError( -10 );
   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         lValue = LetoVarIncr( pCurrentConn, (char*)hb_parc(1), (char*)hb_parc(2),
               (HB_ISNIL(3))? 0 : hb_parni(3) );

         if( !LetoGetError() )
         {
            hb_retnl( lValue );
            return;
         }
      }
   }
   hb_ret();
}

/*
 * LETO_VARDECR( cGroupName, cVarName[, nFlags ) --> nValue
 */
HB_FUNC( LETO_VARDECR )
{
   LONG lValue;

   LetoSetError( -10 );
   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         lValue = LetoVarDecr( pCurrentConn, (char*)hb_parc(1), (char*)hb_parc(2),
               (HB_ISNIL(3))? 0 : hb_parni(3) );

         if( !LetoGetError() )
         {
            hb_retnl( lValue );
            return;
         }
      }
   }
   hb_ret();
}

HB_FUNC( LETO_VARDEL )
{
   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         hb_retl( LetoVarDel( pCurrentConn, (char*)hb_parc(1), (char*)hb_parc(2) ) );
         return;
      }
   }
   hb_retl( 0 );
}

HB_FUNC( LETO_VARGETLIST )
{
   char *pData, *ptr;
   const char *pGroup = (HB_ISNIL(1))? NULL : hb_parc(1);
   char szData[24], cType;
   LONG lValue;
   USHORT uiItems = 0;
   USHORT uiMaxLen = (HB_ISNUM(2))? hb_parni(2) : 0;

   LetoSetError( -10 );
   if( pCurrentConn )
   {
      if( !pGroup || uiMaxLen > 999 )
         uiMaxLen = 0;
      if( ( ptr = pData = LetoVarGetList( pCurrentConn, pGroup, uiMaxLen ) ) != NULL )
      {
         PHB_ITEM temp;
         PHB_ITEM aInfo, aVar;

         while( LetoGetCmdItem( &ptr, szData ) )
         {
            ptr ++;
            uiItems ++;
         }
         if( pGroup && uiMaxLen )
            uiItems /= 3;
         aInfo = hb_itemArrayNew( uiItems );
         ptr = pData;
         uiItems = 1;
         while( LetoGetCmdItem( &ptr, szData ) )
         {
            ptr ++;
            if( pGroup && uiMaxLen )
            {
               aVar = hb_arrayGetItemPtr( aInfo, uiItems );
               hb_arrayNew( aVar, 2 );

               hb_itemPutC( hb_arrayGetItemPtr( aVar, 1 ), szData );

               if( !LetoGetCmdItem( &ptr, szData ) ) return; ptr ++;
               cType = *szData;

               if( !LetoGetCmdItem( &ptr, szData ) ) return; ptr ++;
               switch( cType )
               {
                  case '1':
                  {
                     hb_itemPutL( hb_arrayGetItemPtr( aVar, 2 ), ( *szData == 1 ) );
                     break;
                  }
                  case '2':
                  {
                     sscanf( szData, "%ld", &lValue );
                     hb_itemPutNL( hb_arrayGetItemPtr( aVar, 2 ), lValue );
                     break;
                  }
                  case '3':
                  {
                     hb_itemPutC( hb_arrayGetItemPtr( aVar, 2 ), szData );
                     break;
                  }
               }
            }
            else
            {
               temp = hb_itemPutCL( NULL, szData, strlen(szData) );
               hb_itemArrayPut( aInfo, uiItems, temp );
               hb_itemRelease( temp );
            }
            uiItems ++;
         }
         hb_itemReturn( aInfo );
         hb_itemRelease( aInfo );
         free( pData );
         return;
      }
   }
   hb_ret();
}
 
HB_FUNC( LETO_GETLOCALIP )
{
   char szIP[24];

   *szIP = '\0';
   if( pCurrentConn )
   {
      hb_getLocalIP( pCurrentConn->hSocket, szIP );
   }
   hb_retc( szIP );
}

HB_FUNC( LETO_ADDCDPTRANSLATE )
{
   if( pCurrentConn && HB_ISCHAR(1) && HB_ISCHAR(2) )
   {
      PCDPSTRU pCdps;
      if( pCurrentConn->pCdpTable )
      {
         pCdps = pCurrentConn->pCdpTable;
         while( pCdps->pNext )
           pCdps = pCdps->pNext;
         pCdps = malloc( sizeof( CDPSTRU ) );
      }
      else
         pCdps = pCurrentConn->pCdpTable = hb_xgrab( sizeof( CDPSTRU ) );
      pCdps->szClientCdp = (char*) malloc( hb_parclen(1) + 1 );
      strcpy( pCdps->szClientCdp, hb_parc( 1 ) );
      pCdps->szServerCdp = (char*) malloc( hb_parclen(2) + 1 );
      strcpy( pCdps->szServerCdp, hb_parc( 2 ) );
      pCdps->pNext = NULL;
   }
}

HB_FUNC( LETO_UDFEXIST )
{
   LETOCONNECTION * pConnection;
   char szFuncName[ HB_SYMBOL_NAME_LEN + 1 ];

   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFuncName ) ) != NULL )
   {
      char szData[ HB_SYMBOL_NAME_LEN + 15], *ptr;

      ptr = (szFuncName[0] == '/') ? szFuncName + 1 : szFuncName;
      hb_snprintf( szData, HB_SYMBOL_NAME_LEN + 15, "udf_fun;03;%s;\r\n", ptr );
      if ( !leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         hb_retl( FALSE );
      }
      else
      {
         ptr = leto_firstchar();

         LetoGetCmdItem( &ptr, szData ); ptr ++;
         hb_retl( ( szData[0] == 'T') );
      }
   }
   else
      hb_retl( FALSE );
}

HB_FUNC( LETO_UDF )
{
   LETOCONNECTION * pConnection;
   char szFuncName[ HB_SYMBOL_NAME_LEN + 1 ];

   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFuncName ) ) != NULL )
   {
      char *szData = hb_xgrab( hb_parclen(1) + hb_parclen(2) + 12 );

      sprintf( szData,"usr;01;%s;%s;\r\n", szFuncName+1, hb_parc(2) );

      if ( !leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         hb_ret( );
      }
      else
      {
         char * ptr = leto_firstchar();
         ULONG ulLen = HB_GET_LE_UINT32( ptr );

         if( ulLen )
            hb_retclen( ptr + sizeof( ULONG ), ulLen );
         else
            hb_ret( );
      }
      hb_xfree(szData);
   }
   else
      hb_ret( );
}
