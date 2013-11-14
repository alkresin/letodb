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

static int iFError = 0;
extern LETOCONNECTION * pCurrentConn;

extern int leto_getIpFromPath( const char * sSource, char * szAddr, int * piPort, char * szPath, BOOL lFile );
extern void leto_getFileFromPath( const char * sSource, char * szFile );
extern LETOCONNECTION * leto_ConnectionFind( const char * szAddr, int iPort );
extern LETOCONNECTION * leto_ConnectionNew( const char * szAddr, int iPort, const char * szUser, const char * szPass );
extern void leto_ConnectionClose( LETOCONNECTION * pConnection );
extern LETOCONNECTION * leto_getConnectionPool( void );
extern long int leto_DataSendRecv( LETOCONNECTION * pConnection, const char * sData, ULONG ulLen );
char * leto_firstchar( void );
int leto_getCmdItem( char ** pptr, char * szDest );

static LETOCONNECTION * letoParseFile( const char *szSource, char *szFile)
{
   LETOCONNECTION * pConnection = NULL;
   char szAddr[96];
   int iPort;

   if( leto_getIpFromPath( szSource, szAddr, &iPort, szFile, TRUE ) &&
       ( ( ( pConnection = leto_ConnectionFind( szAddr, iPort ) ) != NULL ) ||
         ( ( pConnection = leto_ConnectionNew( szAddr, iPort, NULL, NULL ) ) ) != NULL ) )
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
   hb_retni( iFError );
}

static LETOCONNECTION * letoParseParam( char * szParam, char * szFile )
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

   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      char szData[_POSIX_PATH_MAX + 16];

      sprintf( szData,"file;01;%s;\r\n", szFile );

      if ( !leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         hb_retl( FALSE );
      }
      else
      {
         char * ptr = leto_firstchar();
         leto_getCmdItem( &ptr, szData ); ptr ++;

         hb_retl( szData[0] == 'T' );
      }
   }
   else
      hb_retl( FALSE );
}

HB_FUNC( LETO_FERASE )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      char szData[_POSIX_PATH_MAX + 16];

      sprintf( szData,"file;02;%s;\r\n", szFile );

      if ( !leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         iFError = -1;
         hb_retni( -1 );
      }
      else
      {
         char * ptr = leto_firstchar();

         leto_getCmdItem( &ptr, szData ); ptr ++;
         hb_retni( ( szData[0] == 'T') ? 0 : -1 );
         leto_getCmdItem( &ptr, szData ); ptr ++;
         sscanf( szData, "%d", &iFError );
      }
   }
   else
   {
      iFError = -1;
      hb_retni( -1 );
   }
}

HB_FUNC( LETO_FRENAME )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   if( HB_ISCHAR(1) && HB_ISCHAR(2) && (pConnection = letoParseParam( hb_parc(1), szFile) ) != NULL )
   {
      char szData[_POSIX_PATH_MAX + 16];

      sprintf( szData,"file;03;%s;%s;\r\n", szFile, hb_parc(2) );

      if ( !leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         iFError = -1;
         hb_retni( -1 );
      }
      else
      {
         char * ptr = leto_firstchar();

         leto_getCmdItem( &ptr, szData ); ptr ++;
         hb_retni( ( szData[0] == 'T') ? 0 : -1 );
         leto_getCmdItem( &ptr, szData ); ptr ++;
         sscanf( szData, "%d", &iFError );
      }
   }
   else
   {
      iFError = -1;
      hb_retni( -1 );
   }
}

HB_FUNC( LETO_MEMOREAD )
{
   LETOCONNECTION * pConnection;
   char szFile[_POSIX_PATH_MAX + 1];

   if( HB_ISCHAR(1) && ( pConnection = letoParseParam( hb_parc(1), szFile ) ) != NULL )
   {
      char szData[_POSIX_PATH_MAX + 16];

      sprintf( szData,"file;04;%s;\r\n", szFile );

      if ( !leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         hb_retc( "" );
      }
      else
      {
         char * ptr = leto_firstchar();
         ULONG ulLen;

         ulLen = HB_GET_LE_UINT32( ptr );
         if( ulLen )
            hb_retclen( ptr + sizeof( ULONG ), ulLen );
         else
            hb_retc("");
      }
   }
   else
      hb_retc( "" );
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
         hb_retc( "" );
      }
      else
      {
         char * ptr = leto_firstchar();
         ULONG ulLen = HB_GET_LE_UINT32( ptr );

         if( ulLen )
            hb_retclen( ptr + sizeof( ULONG ), ulLen );
         else
            hb_retc("");
      }
      hb_xfree(szData);
   }
   else
      hb_retc( "" );
}

HB_FUNC( LETO_CONNECT )
{
   LETOCONNECTION * pConnection;
   char szAddr[96];
   int iPort;
   const char * szUser = (HB_ISNIL(2)) ? NULL : hb_parc(2);
   const char * szPass = (HB_ISNIL(3)) ? NULL : hb_parc(3);

   if( HB_ISCHAR(1) && leto_getIpFromPath( hb_parc(1), szAddr, &iPort, NULL, FALSE ) &&
       ( ( ( pConnection = leto_ConnectionFind( szAddr, iPort ) ) != NULL ) ||
         ( ( pConnection = leto_ConnectionNew( szAddr, iPort, szUser, szPass ) ) ) != NULL ) )
   {
      pCurrentConn = pConnection;
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
   if( pCurrentConn )
      leto_ConnectionClose( pCurrentConn );
   pCurrentConn = NULL;
}

HB_FUNC( LETO_SETCURRENTCONNECTION )
{
   USHORT uiConn = hb_parni(1);

   if( uiConn < MAX_CONNECTIONS_NUMBER )
      pCurrentConn = leto_getConnectionPool() + uiConn - 1;
   else
      pCurrentConn = NULL;
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
   if( pCurrentConn )
      hb_retc( hb_pcount() == 0 ? pCurrentConn->szVersion : pCurrentConn->szVerHarbour );
   else
      hb_retc( "" );
}

HB_FUNC( LETO_PATH )
{
   if( pCurrentConn )
   {
     hb_retc( pCurrentConn->szPath ? pCurrentConn->szPath : "");
     if( HB_ISCHAR(1) )
     {
        if( pCurrentConn->szPath )
           hb_xfree( pCurrentConn->szPath );
        pCurrentConn->szPath = (char*) hb_xgrab( hb_parclen(1) + 1 );
        strcpy( pCurrentConn->szPath, hb_parc( 1 ) );
     }
   }
   else
      hb_retc( "" );
}

HB_FUNC( LETO_MGGETINFO )
{
   if( pCurrentConn )
   {
      if( leto_DataSendRecv( pCurrentConn, "mgmt;00;\r\n", 0 ) )
      {
         PHB_ITEM temp;
         PHB_ITEM aInfo;
         char szData[_POSIX_PATH_MAX + 1];
         char * ptr = leto_firstchar();
         int i;

         if( *(ptr-1) == '+' )
         {
            aInfo = hb_itemArrayNew( 17 );
            for( i=1; i<=17; i++ )
            {
               if( !leto_getCmdItem( &ptr, szData ) )
               {
                  hb_itemReturn( aInfo );
                  hb_itemRelease( aInfo );
                  return;
               }
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
   char szData[_POSIX_PATH_MAX + 1];

   if( pCurrentConn )
   {
      if( HB_ISNIL(1) )
         sprintf( szData, "mgmt;01;\r\n" );
      else
         sprintf( szData, "mgmt;01;%s;\r\n", hb_parc(1) );
      if ( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
      {
         int iUsers, i, j;
         char * ptr = leto_firstchar();
         PHB_ITEM pArray, pArrayItm;

         if( *(ptr-1) == '+' )
         {
            if( !leto_getCmdItem( &ptr, szData ) ) return; ptr ++;
            sscanf( szData, "%d", &iUsers );
            pArray = hb_itemArrayNew( iUsers );
            for ( i = 1; i <= iUsers; i++ )
            {
               pArrayItm = hb_arrayGetItemPtr( pArray, i );
               hb_arrayNew( pArrayItm, 5 );
               for( j=1; j<=5; j++ )
               {
                  if( !leto_getCmdItem( &ptr, szData ) ) return; ptr ++;
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
   char szData[_POSIX_PATH_MAX + 1];

   if( pCurrentConn )
   {
      if( HB_ISNIL(1) )
         sprintf( szData, "mgmt;02;\r\n" );
      else
         sprintf( szData, "mgmt;02;%s;\r\n", hb_parc(1) );
      if ( leto_DataSendRecv( pCurrentConn, szData, 0 ) )
      {
         int iTables, i, j;
         char * ptr = leto_firstchar();
         PHB_ITEM pArray, pArrayItm;

         if( *(ptr-1) == '+' )
         {
            if( !leto_getCmdItem( &ptr, szData ) ) return; ptr ++;
            sscanf( szData, "%d", &iTables );
            pArray = hb_itemArrayNew( iTables );
            for ( i = 1; i <= iTables; i++ )
            {
               pArrayItm = hb_arrayGetItemPtr( pArray, i );
               hb_arrayNew( pArrayItm, 2 );
               for( j=1; j<=2; j++ )
               {
                  if( !leto_getCmdItem( &ptr, szData ) ) return; ptr ++;
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
      {
         sprintf( szData, "mgmt;09;%s;\r\n", hb_parc(1) );
         leto_DataSendRecv( pCurrentConn, szData, 0 );
      }
   }
}

HB_FUNC( LETO_MGGETTIME )
{
   if( pCurrentConn )
   {
      if( leto_DataSendRecv( pCurrentConn, "mgmt;03;\r\n", 0 ) )
      {
         PHB_ITEM temp;
         PHB_ITEM aInfo;
         char szData[32];
         char * ptr = leto_firstchar();
         int i;

         if( *(ptr-1) == '+' )
         {
            aInfo = hb_itemArrayNew( 2 );
            for( i = 1; i <= 2; i++ )
            {
               if( !leto_getCmdItem( &ptr, szData ) )
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
   char szData[32];

   if( pArea && !HB_ISNIL(1) )
   {

      sprintf( szData, "set;02;%lu;%d;\r\n", pArea->hTable, hb_parni(1) );
      leto_DataSendRecv( pArea->pConnection, szData, 0 );
   }
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

         sprintf( szData, "admin;uadd;%s;%s;%s;\r\n", hb_parc(1), szPass, szAccess );
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

         sprintf( szData, "admin;upsw;%s;%s;\r\n", hb_parc(1), szPass );
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
         sprintf( szData, "admin;uacc;%s;%s;\r\n", hb_parc(1), hb_parc(2) );
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
      sprintf( szData, "admin;flush;\r\n" );
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

HB_FUNC( LETO_VARSET )
{
   char *pData, *ptr;
   char szLong[32], cType, cFlag1 = ' ', cFlag2 = ' ';
   ULONG ulLen;
   LONG lValue;
   BOOL bRes = 0;
   USHORT uiFlags = (HB_ISNIL(4))? 0 : hb_parni(4);
   BOOL bPrev = HB_ISBYREF( 5 );

   iFError = 0;
   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) && !HB_ISNIL(3) )
      {
         ulLen = 24 + hb_parclen(1) + hb_parclen(2);
         if( HB_ISLOG(3) )
         {
            ulLen += 2;
            cType = '1';
         }
         else if( HB_ISNUM(3) )
         {
            sprintf( szLong, "%ld", hb_parnl(3) );
            ulLen += strlen( szLong );
            cType = '2';
         }
         else if( HB_ISCHAR(3) )
         {
            ulLen += hb_parclen(3);
            cType = '3';
         }
         else
         {
            hb_retl( 0 );
            return;
         }
         ptr = hb_parc(1);
         while( *ptr )
            if( *ptr++ == ';' )
            {
               hb_retl( 0 );
               return;
            }
         ptr = hb_parc(2);
         while( *ptr )
            if( *ptr++ == ';' )
            {
               hb_retl( 0 );
               return;
            }
         if( hb_parclen(1) > 15 || hb_parclen(2) > 15 )
         {
            hb_retl( 0 );
            return;
         }

         pData = ( char * ) malloc( ulLen );
         sprintf( pData, "var;set;%s;%s;", hb_parc(1), hb_parc(2) );
         ptr = pData + strlen( pData );
         if( cType == '1' )
            *ptr++ = ( ( hb_parl(3) )? '1' : '0' );
         else if( cType == '2' )
         {
            memcpy( ptr, szLong, strlen( szLong ) );
            ptr += strlen( szLong );
         }
         else
         {
            ULONG ul;
            memcpy( ptr, hb_parc(3), ulLen = hb_parclen(3) );
            for( ul=0; ul < ulLen; ul++, ptr++ )
               if( *ptr == ';' )
                  *ptr = '\1';
         }
         cFlag1 |= ( uiFlags & ( LETO_VCREAT | LETO_VOWN | LETO_VDENYWR | LETO_VDENYRD ) );
         if( bPrev )
            cFlag2 |= LETO_VPREVIOUS;
         *ptr++ = ';';
         *ptr++ = cType;
         *ptr++ = cFlag1;
         *ptr++ = cFlag2;
         *ptr++ = ';'; *ptr++ = '\r'; *ptr++ = '\n';
         *ptr++ = '\0';

         if( leto_DataSendRecv( pCurrentConn, pData, 0 ) )
         {
            ptr = leto_firstchar() - 1;
            bRes = ( *ptr == '+' );
         }
         free( pData );
         if( bRes && bPrev )
         {
            if( cType == '1' )
               hb_storl( *(ptr+3) == '1', 5 );
            else if( cType == '2' )
            {
               sscanf( ptr+3, "%ld", &lValue );
               hb_stornl( lValue, 5 );
            }
            else
            {
               ptr += 3;
               pData = ptr;
               while( *ptr != ';' )
               {
                  if( *ptr == '\1' )
                     *ptr = ';';
                  ptr ++;
               }
               hb_storclen( pData, ptr-pData, 5 );
            }
         }
         else if( !bRes )
            sscanf( ptr+1, "%u", &iFError );
      }
   }
   hb_retl( bRes );
}

HB_FUNC( LETO_VARGET )
{
   char *pData, *ptr;
   char cType;
   LONG lValue;

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         pData = ( char * ) malloc( 16 + hb_parclen(1) + hb_parclen(2) );
         sprintf( pData, "var;get;%s;%s;\r\n", hb_parc(1), hb_parc(2) );

         if( leto_DataSendRecv( pCurrentConn, pData, 0 ) )
         {
            free( pData );
            ptr = leto_firstchar() - 1;
            if( *ptr == '+' )
            {
               cType = *(ptr+1);
               if( cType == '1' )
                  hb_retl( *(ptr+3) == '1' );
               else if( cType == '2' )
               {
                  sscanf( ptr+3, "%ld", &lValue );
                  hb_retnl( lValue );
               }
               else
               {
                  ptr += 3;
                  pData = ptr;
                  while( *ptr != ';' )
                  {
                     if( *ptr == '\1' )
                        *ptr = ';';
                     ptr ++;
                  }
                  hb_retclen( pData, ptr-pData );
               }
               return;
            }
            else
               sscanf( ptr+1, "%u", &iFError );
         }
         else
            free( pData );
      }
   }
   hb_ret();
}

HB_FUNC( LETO_VARINCR )
{
   char *pData, *ptr;
   char cType, cFlag1 = ' ';
   LONG lValue;
   USHORT uiFlags = (HB_ISNIL(3))? 0 : hb_parni(3);

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         cFlag1 |= ( uiFlags & ( LETO_VCREAT | LETO_VOWN | LETO_VDENYWR | LETO_VDENYRD ) );
         if( uiFlags & LETO_VOWN )
            cFlag1 |= LETO_VOWN;
         pData = ( char * ) malloc( 24 + hb_parclen(1) + hb_parclen(2) );
         sprintf( pData, "var;inc;%s;%s;2%c!;\r\n", hb_parc(1), hb_parc(2), cFlag1 );

         if( leto_DataSendRecv( pCurrentConn, pData, 0 ) )
         {
            free( pData );
            ptr = leto_firstchar() - 1;
            if( *ptr == '+' )
            {
               sscanf( ptr+3, "%ld", &lValue );
               hb_retnl( lValue );
               return;
            }
            else
               sscanf( ptr+1, "%u", &iFError );
         }
         else
            free( pData );
      }
   }
   hb_ret();
}

HB_FUNC( LETO_VARDECR )
{
   char *pData, *ptr;
   char cType, cFlag1 = ' ';
   LONG lValue;
   USHORT uiFlags = (HB_ISNIL(3))? 0 : hb_parni(3);

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         cFlag1 |= ( uiFlags & ( LETO_VCREAT | LETO_VOWN | LETO_VDENYWR | LETO_VDENYRD ) );
         if( uiFlags & LETO_VOWN )
            cFlag1 |= LETO_VOWN;
         pData = ( char * ) malloc( 24 + hb_parclen(1) + hb_parclen(2) );
         sprintf( pData, "var;dec;%s;%s;2%c!;\r\n", hb_parc(1), hb_parc(2), cFlag1 );

         if( leto_DataSendRecv( pCurrentConn, pData, 0 ) )
         {
            free( pData );
            ptr = leto_firstchar() - 1;
            if( *ptr == '+' )
            {
               sscanf( ptr+3, "%ld", &lValue );
               hb_retnl( lValue );
               return;
            }
            else
               sscanf( ptr+1, "%u", &iFError );
         }
         else
            free( pData );
      }
   }
   hb_ret();
}

HB_FUNC( LETO_VARDEL )
{
   char *pData, *ptr;
   char cType;
   LONG lValue;

   if( pCurrentConn )
   {
      if( !HB_ISNIL(1) && !HB_ISNIL(2) )
      {
         pData = ( char * ) malloc( 16 + hb_parclen(1) + hb_parclen(2) );
         sprintf( pData, "var;del;%s;%s;\r\n", hb_parc(1), hb_parc(2) );

         if( leto_DataSendRecv( pCurrentConn, pData, 0 ) )
         {
            free( pData );
            ptr = leto_firstchar() - 1;
            if( *ptr == '+' )
            {
               hb_retl( 1 );
               return;
            }
            else
               sscanf( ptr+1, "%u", &iFError );
         }
         else
            free( pData );
      }
   }
   hb_retl( 0 );
}

HB_FUNC( LETO_VARGETLIST )
{
   char *pData, *ptr;
   char *pGroup = (HB_ISNIL(1))? NULL : hb_parc(1);
   char szData[24], cType;
   LONG lValue;
   USHORT uiItems = 0, ui;
   USHORT uiMaxLen = (HB_ISNUM(2))? hb_parni(2) : 0;

   if( pCurrentConn )
   {
      if( !pGroup || uiMaxLen > 999 )
         uiMaxLen = 0;
      pData = ( char * ) malloc( 32 + ( (pGroup)? hb_parclen(1) : 0 ) );
      sprintf( pData, "var;list;%s;;%u;\r\n", (pGroup)? pGroup : "", uiMaxLen );
      if( leto_DataSendRecv( pCurrentConn, pData, 0 ) )
      {
         PHB_ITEM temp;
         PHB_ITEM aInfo, aVar;

         free( pData );
         ptr = pData = leto_firstchar();
         if( *(ptr-1) == '+' )
         {
            while( leto_getCmdItem( &ptr, szData ) )
            {
               ptr ++;
               uiItems ++;
            }
            if( pGroup && uiMaxLen )
               uiItems /= 3;
            aInfo = hb_itemArrayNew( uiItems );
            ptr = pData;
            uiItems = 1;
            while( leto_getCmdItem( &ptr, szData ) )
            {              
               ptr ++;
               if( pGroup && uiMaxLen )
               {
                  aVar = hb_arrayGetItemPtr( aInfo, uiItems );
                  hb_arrayNew( aVar, 2 );

                  hb_itemPutC( hb_arrayGetItemPtr( aVar, 1 ), szData );

                  if( !leto_getCmdItem( &ptr, szData ) ) return; ptr ++;
                  cType = *szData;

                  if( !leto_getCmdItem( &ptr, szData ) ) return; ptr ++;
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
            return;
         }
         else
            sscanf( ptr, "%u", &iFError );
      }
      else
         free( pData );
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
