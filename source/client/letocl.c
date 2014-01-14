/* $Id: letocl.c,v 1.1.2.21 2014/01/09 18:36:26 ptsarenko Exp $ */

/*
 * Harbour Project source code:
 * Harbour Leto client
 *
 * Copyright 2013 Alexander S. Kresin <alex / at / kresin.ru>
 * www - http://www.kresin.ru
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

#if defined (_MSC_VER)
  #define _WINSOCKAPI_
#endif
#if defined(HB_OS_UNIX)
   #include <netinet/in.h>
   #include <arpa/inet.h>
#endif

#if !defined (SUCCESS)
#define SUCCESS            0
#define FAILURE            1
#endif

#define LETO_TIMEOUT      -1

#include "hbdefs.h"
#include "funcleto.h"
#include "rddleto.ch"
#include "letocl.h"

static char * s_szBuffer = NULL;
static unsigned long s_lBufferLen = 0;
static int s_iConnectRes = 1;
static int s_iError = 0;

static char * s_szModName = NULL;
static char * s_szDateFormat = NULL;
static char * s_szCdp = NULL;
static char s_cCentury = 'T';
static unsigned int s_uiDeleted = 0;

static unsigned int s_bFastAppend = 0;
static unsigned int s_uiAutOpen = 1;

static void( *pFunc )( void ) = NULL;

unsigned int uiConnCount = 0;
LETOCONNECTION * letoConnPool = NULL;
LETOCONNECTION * pCurrentConn = NULL;

void leto_ConnectionClose( LETOCONNECTION * pConnection );

char * leto_getRcvBuff( void )
{
   return s_szBuffer;
}

char * leto_firstchar( void )
{
   int iLenLen;

   if( ( iLenLen = (((int)*s_szBuffer) & 0xFF) ) < 10 )
      return (s_szBuffer+2+iLenLen);
   else
      return (s_szBuffer+1);
}

static long leto_dateEncStr( const char * szDate )
{
   int  iYear, iMonth, iDay;

   if( szDate )
   {
      iYear = ( ( ( int ) ( szDate[ 0 ] - '0' )   * 10 +
                  ( int ) ( szDate[ 1 ] - '0' ) ) * 10 +
                  ( int ) ( szDate[ 2 ] - '0' ) ) * 10 +
                  ( int ) ( szDate[ 3 ] - '0' );
      iMonth = ( szDate[ 4 ] - '0' ) * 10 + ( szDate[ 5 ] - '0' );
      iDay   = ( szDate[ 6 ] - '0' ) * 10 + ( szDate[ 7 ] - '0' );

      if( iYear >= 0 && iYear <= 9999 && iMonth >= 1 && iMonth <= 12 && iDay >= 1 )
      {
         static const int auiDayLimit[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

         if( iDay <= auiDayLimit[ iMonth - 1 ] ||
             ( iDay == 29 && iMonth == 2 &&
               ( iYear & 3 ) == 0 && ( iYear % 100 != 0 || iYear % 400 == 0 ) ) )
         {
            int iFactor = ( iMonth < 3 ) ? -1 : 0;

            return ( ( long )( iFactor + 4800 + iYear ) * 1461 / 4 ) +
                   ( ( long )( iMonth - 2 - ( iFactor * 12 ) ) * 367 ) / 12 -
                   ( ( long )( ( iFactor + 4900 + iYear ) / 100 ) * 3 / 4 ) +
                   ( long ) iDay - 32075;
         }
      }
   }

   return 0;
}

char * leto_AddKeyToBuf( char * szData, char * szKey, unsigned int uiKeyLen, unsigned long *pulLen )
{
   char * pData = ( szData + strlen( szData ) );
   *pData++ = (char) (uiKeyLen) & 0xFF;
   memcpy( pData,szKey,uiKeyLen );
   pData += uiKeyLen;
   *pData = '\0';

   *pulLen = pData - szData;
   pData = leto_AddLen( szData, pulLen, 1 );
   return pData;
}

unsigned int leto_IsRecLocked( LETOTABLE * pTable, unsigned long ulRecNo )
{
   unsigned long ul;

   if( pTable->pLocksPos )   
      for( ul = 0; ul < pTable->ulLocksMax; ul++ )
         if( pTable->pLocksPos[ ul ] == ulRecNo )
            return 1;
   return 0;
}

void leto_AddRecLock( LETOTABLE * pTable, unsigned long ulRecNo )
{
   if( ! leto_IsRecLocked( pTable, ulRecNo ) )
   {
      if( ! pTable->pLocksPos )
      {
         /* Allocate locks array for the table, if it isn't allocated yet */
         pTable->ulLocksAlloc = 10;
         pTable->pLocksPos = (ULONG*) malloc( sizeof(ULONG) * pTable->ulLocksAlloc );
         pTable->ulLocksMax = 0;
      }
      else if( pTable->ulLocksMax == pTable->ulLocksAlloc )
      {
         pTable->pLocksPos = (ULONG*) realloc( pTable->pLocksPos, sizeof(ULONG) * (pTable->ulLocksAlloc+10) );
         pTable->ulLocksAlloc += 10;
      }
      pTable->pLocksPos[ pTable->ulLocksMax++ ] = ulRecNo;
   }
}

void leto_setCallback( void( *fn )( void ) )
{
   pFunc = fn;
}

HB_EXPORT void LetoSetDateFormat( const char * szDateFormat )
{
   unsigned int uiLen;

   if( s_szDateFormat )
      free( s_szDateFormat );

   uiLen = strlen( szDateFormat );
   s_szDateFormat = (char*) malloc( uiLen+1 );
   memcpy( s_szDateFormat, szDateFormat, uiLen );
   s_szDateFormat[uiLen] = '\0';
}

HB_EXPORT void LetoSetCentury( char cCentury )
{
   s_cCentury = cCentury;
}

HB_EXPORT void LetoSetCdp( const char * szCdp )
{
   unsigned int uiLen;

   if( s_szCdp )
      free( s_szCdp );

   uiLen = strlen( szCdp );
   s_szCdp = (char*) malloc( uiLen+1 );
   memcpy( s_szCdp, szCdp, uiLen );
   s_szCdp[uiLen] = '\0';
}

HB_EXPORT void LetoSetDeleted( unsigned int uiDeleted )
{
   s_uiDeleted = uiDeleted;
}

HB_EXPORT void LetoSetAutOpen( unsigned int uiAutOpen )
{
   s_uiAutOpen = uiAutOpen;
}

HB_EXPORT void LetoSetModName( char * szModule )
{
   unsigned int uiLen;

   if( s_szModName )
      free( s_szModName );

   uiLen = strlen( szModule );
   s_szModName = (char*) malloc( uiLen+1 );
   memcpy( s_szModName, szModule, uiLen );
   s_szModName[uiLen] = '\0';
}

HB_EXPORT void LetoSetPath( LETOCONNECTION * pConnection, const char * szPath )
{
   if( pConnection->szPath )
      free( pConnection->szPath );
   pConnection->szPath = (char*) malloc( strlen( szPath ) + 1 );
   strcpy( pConnection->szPath, szPath );
}

HB_EXPORT int LetoGetConnectRes( void )
{
   return s_iConnectRes;
}

HB_EXPORT int LetoGetError( void )
{
   return s_iError;
}

void LetoSetError( int iErr )
{
   s_iError = iErr;
}

HB_EXPORT unsigned int LetoSetFastAppend( unsigned int uiFApp )
{
   unsigned int uiPrev = s_bFastAppend;

   s_bFastAppend = uiFApp;
   return uiPrev;
}

HB_EXPORT char * LetoGetServerVer( LETOCONNECTION * pConnection )
{
   return pConnection->szVersion;
}

HB_EXPORT int LetoGetCmdItem( char ** pptr, char * szDest )
{
   char * ptr = *pptr;

   if( ptr )
   {
      while( *ptr && *ptr != ';' ) ptr++;
      if( *ptr )
      {
         if( ptr > *pptr )
            memcpy( szDest, *pptr, ptr-*pptr );
         szDest[ptr-*pptr] = '\0';
         *pptr = ptr;
         return 1;
      }
      else
         return 0;
   }
   else
      return 0;
}

HB_EXPORT void LetoInit( void )
{
   if( !s_szBuffer )
   {
      hb_ipInit();

      letoConnPool = malloc( sizeof( LETOCONNECTION ) );
      uiConnCount = 1;
      memset( letoConnPool, 0, sizeof(LETOCONNECTION) * uiConnCount );

      s_szBuffer = (char*) malloc(HB_SENDRECV_BUFFER_SIZE);
      s_lBufferLen = HB_SENDRECV_BUFFER_SIZE;
   }
}

HB_EXPORT void LetoExit( unsigned int uiFull )
{
   unsigned int i;

   if( letoConnPool )
   {
      for( i=0; i<uiConnCount; i++ )
      {
         if( (letoConnPool + i)->pAddr )
            LetoConnectionClose( letoConnPool + i );
      }
      free( letoConnPool );
      letoConnPool = NULL;
   }
   uiConnCount = 0;

   if( uiFull )
   {
      if( s_szBuffer )
         free( s_szBuffer );
      s_szBuffer = NULL;
      s_lBufferLen = 0;
      hb_ipCleanup();
   }

}

long int leto_RecvFirst( LETOCONNECTION * pConnection )
{
   HB_SOCKET_T hSocket = pConnection->hSocket;
   int iRet;
   char szRet[LETO_MSGSIZE_LEN+1], * ptr;
   unsigned long int ulMsgLen;

   ptr = s_szBuffer;
   while( hb_ipDataReady( hSocket,2 ) == 0 )
   {
   }

   if( ( iRet = hb_ipRecv( hSocket, szRet, LETO_MSGSIZE_LEN ) ) < LETO_MSGSIZE_LEN )
      return 0;

   if( !strncmp( szRet, "Leto", 4 ) )
   {
      pConnection->uiProto = 1;
      pConnection->uiTBufOffset = 10;

      memcpy( s_szBuffer, szRet, 4 );
      ptr += 4;
      do
      {
         while( hb_ipDataReady( hSocket,2 ) == 0 )
         {
         }
         if( (iRet = hb_ipRecv( hSocket, ptr, HB_SENDRECV_BUFFER_SIZE-(ptr-s_szBuffer) )) <= 0 )
            break;
         else
         {
            ptr += iRet;
            if( *(ptr-1) == '\n' && *(ptr-2) == '\r' )
               break;
         }
      }
      while( ptr - s_szBuffer < HB_SENDRECV_BUFFER_SIZE );
   }
   else
   {
      pConnection->uiProto = 2;
      pConnection->uiTBufOffset = 12;

      ulMsgLen = HB_GET_LE_UINT32( szRet );
      if( !ulMsgLen || (ulMsgLen + 1 > s_lBufferLen) )
         return 0;
      do
      {
         while( hb_ipDataReady( hSocket, 2 ) == 0 )
         {
         }
         if( (iRet = hb_ipRecv( hSocket, ptr, ulMsgLen )) <= 0 )
            break;
         else
         {
            ptr += iRet;
            ulMsgLen -= iRet;
         }
      }
      while( ulMsgLen > 0 );
   }

   if( (((int)*s_szBuffer) & 0xFF) >= 10 && (ptr - s_szBuffer) > 2 && *(ptr-1) == '\n' && *(ptr-2) == '\r' )
      *(ptr-2) = '\0';
   else
      *ptr = '\0';

   return (long int) (ptr - s_szBuffer);
}

long int leto_Recv( LETOCONNECTION * pConnection )
{
   HB_SOCKET_T hSocket = pConnection->hSocket;
   char * ptr = s_szBuffer;
   int iRet;

   if( pConnection->uiProto == 1 )
   {
      int iLenLen = 0;
      char szRet[HB_SENDRECV_BUFFER_SIZE];
      long int lLen = 0;

      do
      {
         while( hb_ipDataReady( hSocket,2 ) == 0 )
         {
         }
         iRet = hb_ipRecv( hSocket, szRet, HB_SENDRECV_BUFFER_SIZE );
         if( iRet <= 0 )
            break;
         else
         {
            if( ((ULONG)((ptr - s_szBuffer) + iRet)) > s_lBufferLen )
            {
               char * szTemp;
               s_lBufferLen += HB_SENDRECV_BUFFER_SIZE;
               szTemp = (char*) malloc( s_lBufferLen );
               memcpy( szTemp, s_szBuffer, ptr-s_szBuffer );
               ptr = szTemp + (ptr-s_szBuffer);
               free( s_szBuffer );
               s_szBuffer = szTemp;
            }
            memcpy( ptr, szRet, iRet );
            ptr += iRet;
            if( lLen == 0 && ( iLenLen = (((int)*s_szBuffer) & 0xFF) ) < 10 && (ptr-s_szBuffer) > iLenLen )
               lLen = leto_b2n( s_szBuffer+1, iLenLen );
            if( ( lLen > 0 && lLen+iLenLen+1 <= ptr-s_szBuffer ) ||
                  ( iLenLen >= 10 && *(ptr-1) == '\n' && *(ptr-2) == '\r' ) )
               break;
         }
      }
      while(1);

      if( iLenLen >= 10 )
         *(ptr-2) = '\0';
      else
         *ptr = '\0';
   }
   else
   {
      int iStep = 0;
      char szRet[LETO_MSGSIZE_LEN+1];
      unsigned long int lMsgLen;

      while( hb_ipDataReady( hSocket, 2 ) == 0 )
      {
         if( pFunc && ( ( iStep ++ ) % 100 == 0 ) )
            pFunc();
      }
      iRet = hb_ipRecv( hSocket, szRet, LETO_MSGSIZE_LEN );
      if( iRet != LETO_MSGSIZE_LEN )
         return 0;

      lMsgLen = HB_GET_LE_UINT32( szRet );
      if( ! lMsgLen )
         return 0;

      if( lMsgLen + 1 > s_lBufferLen )
      {
         s_lBufferLen = lMsgLen + 1;
         s_szBuffer = (char*) realloc( s_szBuffer, s_lBufferLen );
         ptr = s_szBuffer;
      }

      do
      {
         while( hb_ipDataReady( hSocket, 2 ) == 0 )
         {
         }
         iRet = hb_ipRecv( hSocket, ptr, lMsgLen );
         if( iRet <= 0 )
         {
            //leto_writelog( NULL,0,"!ERROR! leto_Recv! broken message" );
            break;
         }
         else
         {
            ptr += iRet;
            lMsgLen -= iRet;
         }
      }
      while( lMsgLen > 0 );

      if( (((int)*s_szBuffer) & 0xFF) >= 10 && (ptr - s_szBuffer) > 2 && *(ptr-1) == '\n' && *(ptr-2) == '\r' )
         *(ptr-2) = '\0';
      else
         *ptr = '\0';
   }

   return (long int) (ptr - s_szBuffer);
}

long int leto_DataSendRecv( LETOCONNECTION * pConnection, const char * sData, unsigned long ulLen )
{
   unsigned int uiSizeLen = (pConnection->uiProto==1)? 0 : LETO_MSGSIZE_LEN;

   if( !ulLen )
      ulLen = strlen(sData);

   if( uiSizeLen + ulLen > s_lBufferLen )
   {
      s_lBufferLen = uiSizeLen + ulLen;
      s_szBuffer = (char*) realloc( s_szBuffer, s_lBufferLen );
   }

   HB_PUT_LE_UINT32( s_szBuffer, ulLen );
   memcpy( s_szBuffer + uiSizeLen, sData, ulLen );
   if ( hb_ipSend( pConnection->hSocket, s_szBuffer, uiSizeLen + ulLen, LETO_TIMEOUT ) <= 0 )
   {
      return 0;
   }
   return leto_Recv( pConnection );
}

static int leto_SendRecv( LETOTABLE * pTable, char * sData, unsigned long ulLen, int iErr )
{
   long int lRet;

   s_iError = 0;
   lRet = leto_DataSendRecv( letoConnPool + pTable->uiConnection, sData, ulLen );
   if( !lRet )
      s_iError = 1000;
   else if( *( leto_getRcvBuff() ) == '-' && iErr )
   {
      s_iError = iErr;
      return 0;
   }
   return lRet;
}

LETOCONNECTION * leto_getConnectionPool( void )
{
   return letoConnPool;
}

LETOCONNECTION * leto_ConnectionFind( const char * szAddr, int iPort )
{
   unsigned int i;

   for( i=0; i<uiConnCount; i++ )
   {
      if( letoConnPool[i].pAddr && letoConnPool[i].iPort == iPort &&
              !strcmp(letoConnPool[i].pAddr,szAddr) )
         return letoConnPool + i;
   }
   return NULL;
}

const char * leto_GetServerCdp( LETOCONNECTION * pConnection, const char *szCdp )
{
   PCDPSTRU pCdps = pConnection->pCdpTable;

   while( szCdp && pCdps )
   {
      if( !strcmp( szCdp, pCdps->szClientCdp ) )
      {
         szCdp = pCdps->szServerCdp;
         break;
      }
      pCdps = pCdps->pNext;
   }
   return szCdp;
}

int LetoCheckServerVer( LETOCONNECTION * pConnection, USHORT uiVer )
{
   return (int) ( pConnection->uiMajorVer*100 + pConnection->uiMinorVer ) >= uiVer;
}

const char * leto_RemoveIpFromPath(const char * szPath)
{
   const char * ptr;
   if( strlen(szPath) > 2 && szPath[0]=='/'  && szPath[1]=='/' && (ptr=strchr(szPath+2, '/')) != NULL )
   {
      szPath = ptr + 1;
   }
   return szPath;
}

int leto_getIpFromPath( const char * sSource, char * szAddr, int * piPort, char * szPath, BOOL bFile )
{
   const char * ptrPort, * ptr = sSource;
   char * ptrw;

   while( strlen(ptr) >= 2 && (ptr[0] != '/' || ptr[1] != '/'))
   {
      if( ( ptrPort = strchr( ptr, ',' ) ) == NULL)
         ptrPort = strchr( ptr, ';' );
      if( ptrPort )
         ptr = ptrPort + 1;
      else
         return 0;
   }
   ptr ++;
   if( ( ptrPort = strchr( ptr,':' ) ) == NULL )
   {
      return 0;
   }

   ptr ++;
   memcpy( szAddr, ptr, ptrPort-ptr );
   szAddr[ptrPort-ptr] = '\0';
   ptrPort ++;
   sscanf( ptrPort, "%d", piPort );

   if( szPath )
   {
      do ptrPort ++; while( *ptrPort>='0' && *ptrPort<='9' );
      ptr = ptrPort;
      ptrPort = sSource + strlen(sSource);
      if( bFile )
      {
         while( *ptrPort != '/' && *ptrPort != '\\' ) ptrPort --;
      }
      if( ptrPort < ptr )
      {
         return 0;
      }
      else if( ptrPort >= ptr )
      {
         ptrPort ++;
         if( *(ptr+2) == ':' )
            ptr ++;
         memcpy( szPath, ptr, ptrPort-ptr );
      }
      szPath[ptrPort-ptr] = '\0';
      if( ( ptrw = strchr( szPath, ',' ) ) != NULL || ( ptrw = strchr( szPath, ';' ) ) != NULL)
         ptrw[0] = '\0';
      ptr = szPath;
      while( ( ptr = strchr( ptr,'.' ) ) != NULL )
      {
         ptr ++;
         if( *ptr == '.' )
            return 0;
      }
   }
   return 1;
}

void leto_getFileFromPath( const char * sSource, char * szFile )
{
   const char * ptr = sSource + strlen(sSource) - 1;
   while( ptr >= sSource && *ptr != '/' && *ptr != '\\' ) ptr --;
   ptr ++;
   strcpy( szFile, ptr );
}

char * leto_DecryptBuf( LETOCONNECTION * pConnection, const char * ptr, ULONG * pulLen )
{
   if( *pulLen > pConnection->ulBufCryptLen )
   {
      if( !pConnection->ulBufCryptLen )
         pConnection->pBufCrypt = (char*) malloc( *pulLen );
      else
         pConnection->pBufCrypt = (char*) realloc( pConnection->pBufCrypt, *pulLen );
      pConnection->ulBufCryptLen = *pulLen;
   }
   leto_decrypt( ptr, *pulLen, pConnection->pBufCrypt, pulLen, LETO_PASSWORD );
   return pConnection->pBufCrypt;
}

char * leto_DecryptText( LETOCONNECTION * pConnection, ULONG * pulLen )
{
   char * ptr = leto_getRcvBuff();
   USHORT iLenLen = ((int)*ptr++) & 0xFF;

   if( !iLenLen || iLenLen >= 10 )
      *pulLen = 0;
   else
   {
     *pulLen = leto_b2n( ptr, iLenLen );
     ptr += iLenLen + 1;
     if( pConnection->bCrypt )
     {
        ptr = leto_DecryptBuf( pConnection, ptr, pulLen );
     }
   }
   return ptr;
}

unsigned int leto_IsBinaryField( unsigned int uiType, unsigned int uiLen )
{
   return ( ( uiType == HB_FT_MEMO || uiType == HB_FT_BLOB ||
              uiType == HB_FT_IMAGE || uiType == HB_FT_OLE ) && uiLen == 4 ) ||
          ( uiType == HB_FT_DATE && uiLen <= 4 ) ||
          uiType == HB_FT_DATETIME ||
          uiType == HB_FT_DAYTIME ||
          uiType == HB_FT_MODTIME ||
          uiType == HB_FT_ANY ||
          uiType == HB_FT_INTEGER ||
          uiType == HB_FT_DOUBLE;
}

void leto_SetBlankRecord( LETOTABLE * pTable, unsigned int uiAppend )
{
   unsigned int uiCount;
   LETOFIELD * pField;

   if( uiAppend )
      pTable->ulRecNo = 0;
   memset( pTable->pRecord, ' ', pTable->uiRecordLen );

   for( uiCount = 0, pField = pTable->pFields; uiCount < pTable->uiFieldExtent; uiCount++, pField++ )
      if( leto_IsBinaryField( pField->uiType, pField->uiLen ) )
         memset(pTable->pRecord + pTable->pFieldOffset[uiCount], 0, pField->uiLen );
}

void leto_AllocBuf( LETOBUFFER *pLetoBuf, unsigned long ulDataLen, ULONG ulAdd )
{
   if( !pLetoBuf->ulBufLen )
   {
      pLetoBuf->pBuffer = (BYTE *) malloc( ulDataLen + ulAdd );
      pLetoBuf->ulBufLen = ulDataLen + ulAdd;
   }
   else if( pLetoBuf->ulBufLen < ulDataLen )
   {
      pLetoBuf->pBuffer = (BYTE *) realloc( pLetoBuf->pBuffer, ulDataLen + ulAdd );
      pLetoBuf->ulBufLen = ulDataLen + ulAdd;
   }
   pLetoBuf->ulBufDataLen = ulDataLen;
   pLetoBuf->bSetDeleted = s_uiDeleted;
   pLetoBuf->ulDeciSec = leto_MilliSec()/10;
}

unsigned long leto_BufStore( LETOTABLE * pTable, char * pBuffer, const char * ptr, unsigned long ulDataLen )
{
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;
   unsigned long ulBufLen;

   if( pConnection->bCrypt )
   {
      char * pTemp = malloc( ulDataLen ), * pDecrypt;
      unsigned long ulAllLen = 0, ulRecLen, ulLen;
      ulBufLen = 0;
      while( ulAllLen < ulDataLen - 1 )
      {
         ulRecLen = ulLen = HB_GET_LE_UINT24( ptr + ulAllLen );
         pDecrypt = leto_DecryptBuf( pConnection, ptr + ulAllLen + 3, &ulLen );
         HB_PUT_LE_UINT24( pTemp + ulBufLen, ulLen );
         ulBufLen += 3;
         if( ulLen )
         {
            memcpy( pTemp + ulBufLen, pDecrypt, ulLen );
            ulBufLen += ulLen;
         }
         ulAllLen += ulRecLen + 3;
      }
      ulBufLen -= 2;
      memcpy( pBuffer, pTemp, ulBufLen );
      free( pTemp );
   }
   else
   {
      memcpy( pBuffer, ptr, ulDataLen );
      ulBufLen = ulDataLen;
   }
   return ulBufLen - ulDataLen;
}

unsigned long leto_BufRecNo( char * ptrBuf )
{
   unsigned long ulRecNo;
   sscanf( ptrBuf + 1, "%lu;", &ulRecNo );
   return ulRecNo;
}

unsigned int leto_HotBuffer( LETOTABLE * pTable, LETOBUFFER * pLetoBuf, unsigned int uiBufRefreshTime )
{
   if( pTable->iBufRefreshTime >= 0 )
      uiBufRefreshTime = pTable->iBufRefreshTime;
   return ( !pTable->fShared || ( uiBufRefreshTime == 0 ) ||
             ( ((unsigned int)(((leto_MilliSec()/10))-pLetoBuf->ulDeciSec)) < uiBufRefreshTime ) ) &&
          ( pLetoBuf->bSetDeleted == s_uiDeleted );
}

unsigned int leto_OutBuffer( LETOBUFFER * pLetoBuf, char * ptr )
{
   return ((unsigned long)(ptr - (char*)pLetoBuf->pBuffer)) >= pLetoBuf->ulBufDataLen-1;
}

void leto_setSkipBuf( LETOTABLE * pTable, const char * ptr, unsigned long ulDataLen, unsigned int uiRecInBuf )
{
   if( !ulDataLen )
      ulDataLen = HB_GET_LE_UINT24( ptr ) + 3;

   leto_AllocBuf( &pTable->Buffer, ulDataLen, 0 );
   if( ptr )
   {
      pTable->Buffer.ulBufDataLen += leto_BufStore( pTable, (char *) pTable->Buffer.pBuffer, ptr, ulDataLen );
   }
   pTable->uiRecInBuf = uiRecInBuf;
}

void leto_ClearSeekBuf( LETOTAGINFO * pTagInfo )
{
   pTagInfo->Buffer.ulBufDataLen = 0;
   pTagInfo->uiRecInBuf = 0;
}

static void leto_ClearAllSeekBuf( LETOTABLE * pTable )
{
   if( pTable->pTagInfo )
   {
      LETOTAGINFO * pTagInfo = pTable->pTagInfo;
      while( pTagInfo )
      {
         leto_ClearSeekBuf( pTagInfo );
         pTagInfo = pTagInfo->pNext;
      }
   }
}

void leto_ClearBuffers( LETOTABLE * pTable )
{
   if( ! pTable->ptrBuf )
      pTable->Buffer.ulBufDataLen = 0;
   else
      pTable->ptrBuf = NULL;
   leto_ClearAllSeekBuf( pTable );
}

static ULONG leto_TransBlockLen( LETOCONNECTION * pConnection, ULONG ulLen )
{
   return ( pConnection->ulTransBlockLen ? pConnection->ulTransBlockLen : (ulLen<256)? 1024 : ulLen*3 );
}

void leto_AddTransBuffer( LETOCONNECTION * pConnection, char * pData, ULONG ulLen )
{
   if( !pConnection->szTransBuffer )
   {
      pConnection->ulTransBuffLen = leto_TransBlockLen( pConnection, ulLen );
      pConnection->szTransBuffer = (BYTE*) malloc( pConnection->ulTransBuffLen );
      memcpy( pConnection->szTransBuffer+4, "ta;", 3 );
   }
   if( !pConnection->ulTransDataLen )
      pConnection->ulTransDataLen = pConnection->uiTBufOffset;
   if( pConnection->ulTransBuffLen - pConnection->ulTransDataLen <= ulLen )
   {
      pConnection->ulTransBuffLen = pConnection->ulTransDataLen + leto_TransBlockLen( pConnection, ulLen );
      pConnection->szTransBuffer = (BYTE*) realloc( pConnection->szTransBuffer, pConnection->ulTransBuffLen );
   }
   memcpy( pConnection->szTransBuffer+pConnection->ulTransDataLen, pData, ulLen );
   pConnection->ulTransDataLen += ulLen;
   pConnection->ulRecsInTrans ++;
}

HB_EXPORT void LetoDbFreeTag( LETOTAGINFO * pTagInfo )
{
   if( pTagInfo->BagName )
      free( pTagInfo->BagName );
   if( pTagInfo->TagName )
      free( pTagInfo->TagName );
   if( pTagInfo->KeyExpr )
      free( pTagInfo->KeyExpr );
   if( pTagInfo->ForExpr )
      free( pTagInfo->ForExpr );
   if( pTagInfo->pTopScopeAsString )
      free( pTagInfo->pTopScopeAsString );
   if( pTagInfo->pBottomScopeAsString )
      free( pTagInfo->pBottomScopeAsString );
   if( pTagInfo->Buffer.pBuffer )
      free( pTagInfo->Buffer.pBuffer );

   free( pTagInfo );
}

static unsigned int leto_checkLockError( void )
{
   char * ptr;

   if( *( ptr = leto_getRcvBuff() ) == '-' )
   {
      if( *(ptr+3) != '4' )
         s_iError = 1021;
      return 1;
   }
   return 0;
}

char * leto_ParseTagInfo( LETOTABLE * pTable, char * pBuffer )
{
   LETOTAGINFO * pTagInfo, * pTagNext;
   int iOrders, iCount, iLen, i;
   char szData[_POSIX_PATH_MAX];
   char * ptr = pBuffer;

   LetoGetCmdItem( &ptr, szData ); ptr ++;
   sscanf( szData, "%d" , &iOrders );  

   if( iOrders )
   {
      if( pTable->pTagInfo )
      {
         pTagInfo = pTable->pTagInfo;
         while( pTagInfo->pNext )
            pTagInfo = pTagInfo->pNext;
         pTagInfo->pNext = (LETOTAGINFO *) malloc( sizeof(LETOTAGINFO) );
         pTagInfo = pTagInfo->pNext;
      }
      else
      {
         pTagInfo = pTable->pTagInfo = (LETOTAGINFO *) malloc( sizeof(LETOTAGINFO) );
      }
      for( iCount = 1; iCount <= iOrders; iCount++ )
      {
         memset( pTagInfo, 0, sizeof(LETOTAGINFO) );

         pTagInfo->uiTag = pTable->uiOrders + iCount;
         LetoGetCmdItem( &ptr, szData ); ptr ++;
         if( ( iLen = strlen(szData) ) != 0 )
         {
            pTagInfo->BagName = (char*) malloc( iLen+1 );
            memcpy( pTagInfo->BagName, szData, iLen );
            pTagInfo->BagName[iLen] = '\0';
            for( i=0; i<iLen; i++ )
               pTagInfo->BagName[i] = HB_TOLOWER(pTagInfo->BagName[i]);
         }
         LetoGetCmdItem( &ptr, szData ); ptr ++;
         if( ( iLen = strlen(szData) ) != 0 )
         {
            if( iLen > LETO_MAX_TAGNAME )
               iLen = LETO_MAX_TAGNAME;
            pTagInfo->TagName = (char*) malloc( iLen+1 );
            memcpy( pTagInfo->TagName, szData, iLen );
            pTagInfo->TagName[iLen] = '\0';
            for( i=0; i<iLen; i++ )
               pTagInfo->TagName[i] = HB_TOLOWER(pTagInfo->TagName[i]);
         }
         LetoGetCmdItem( &ptr, szData ); ptr ++;        
         pTagInfo->KeyExpr = (char*) malloc( (iLen = strlen(szData))+1 );
         memcpy( pTagInfo->KeyExpr, szData, iLen );
         pTagInfo->KeyExpr[iLen] = '\0';

         LetoGetCmdItem( &ptr, szData ); ptr ++;
         if( ( iLen = strlen(szData) ) != 0 )
         {
            pTagInfo->ForExpr = (char*) malloc( iLen+1 );
            memcpy( pTagInfo->ForExpr, szData, iLen );
            pTagInfo->ForExpr[iLen] = '\0';
         }
         LetoGetCmdItem( &ptr, szData ); ptr ++;
         pTagInfo->KeyType = szData[0];

         LetoGetCmdItem( &ptr, szData ); ptr ++;
         sscanf( szData, "%d", &iLen );
         pTagInfo->KeySize = iLen;

         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
         {
            LetoGetCmdItem( &ptr, szData ); ptr ++;
            pTagInfo->UsrAscend = !( szData[0] == 'T' );

            LetoGetCmdItem( &ptr, szData ); ptr ++;
            pTagInfo->UniqueKey = ( szData[0] == 'T' );

            LetoGetCmdItem( &ptr, szData ); ptr ++;
            pTagInfo->Custom = ( szData[0] == 'T' );
         }
         if( iCount < iOrders )
         {
            pTagNext = (LETOTAGINFO *) malloc( sizeof(LETOTAGINFO) );
            pTagInfo->pNext = pTagNext;
            pTagInfo = pTagNext;
         }
      }
      pTable->uiOrders += iOrders;
   }
   return ptr;
}

void LetoDbGotoEof( LETOTABLE * pTable )
{

   if( !LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
   {
      unsigned long ulRecCount;
      LetoDbRecCount( pTable, &ulRecCount );
   }
   leto_SetBlankRecord( pTable, FALSE );
   pTable->ulRecNo = pTable->ulRecCount + 1;
   pTable->fEof = pTable->fBof = TRUE;
   pTable->fDeleted = pTable->fFound = pTable->fRecLocked = FALSE;
}

void leto_SetUpdated( LETOTABLE * pTable, USHORT uiUpdated )
{
   pTable->uiUpdated = uiUpdated;
   memset( pTable->pFieldUpd, 0, pTable->uiFieldExtent * sizeof( USHORT ) );
}

//#define ZIP_RECORD

int leto_ParseRecord( LETOTABLE * pTable, const char * szData, unsigned int uiCrypt )
{
   int iLenLen, iSpace;
   unsigned int uiCount, uiLen, uiFields;
   unsigned long ulLen;
   char * ptr, * ptrRec;
   char szTemp[24];
   LETOFIELD * pField;
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;

   ulLen = HB_GET_LE_UINT24( szData );
   ptr = (char *) szData + 3;

   if( uiCrypt && pConnection->bCrypt )
      ptr = leto_DecryptBuf( pConnection, ptr, &ulLen );

   LetoGetCmdItem( &ptr, szTemp ); ptr ++;

   pTable->fBof = ( (*szTemp) & LETO_FLG_BOF );
   pTable->fEof = ( (*szTemp) & LETO_FLG_EOF );
   pTable->fDeleted = ( *(szTemp) & LETO_FLG_DEL );
   pTable->fFound = ( *(szTemp) & LETO_FLG_FOUND );
   pTable->fRecLocked = ( *(szTemp) & LETO_FLG_LOCKED );
   pTable->ulRecNo = atol( szTemp+1 );

   if( pTable->fEof )
   {
      leto_SetBlankRecord( pTable, 0 );
   }

#ifdef ZIP_RECORD

   memcpy( pTable->pRecord, ptr + 1, pTable->uiRecordLen );   // skip delete flag

#else

   pField = pTable->pFields;
   uiFields = pTable->uiFieldExtent;
   for( uiCount = 0; uiCount < uiFields; uiCount++ )
   {
      ptrRec = (char*) pTable->pRecord + pTable->pFieldOffset[uiCount];
      iLenLen = ((unsigned char)*ptr) & 0xFF;

      if( pTable->fEof )
      {
         ptr ++;
      }
      else if( !iLenLen && !leto_IsBinaryField( pField->uiType, pField->uiLen ) )
      {
         if( pField->uiType == HB_FT_LOGICAL )
            *ptrRec = 'F';
         else
         {
            iSpace = pField->uiLen;
            while( iSpace-- )
               *ptrRec++ = ' ';
         }
         ptr ++;
         if( pField->uiType == HB_FT_STRING && pField->uiLen > 255 )
            ptr ++;
      }
      else
         switch( pField->uiType )
         {
            case HB_FT_DATE:
            {
               if( *ptr == '\0' && pField->uiLen == 8 )
               {
                  memset( ptrRec, ' ', 8 );
                  ptr ++;
               }
               else
               {
                  memcpy( ptrRec, ptr, pField->uiLen );
                  ptr += pField->uiLen;
               }
               break;
            }
            case HB_FT_STRING:
               ptr ++;
               if( pField->uiLen > 255 )
               {
                  uiLen = leto_b2n( ptr, iLenLen );
                  ptr += iLenLen;
                  iLenLen = (int) uiLen;
               }
               if( pField->uiLen < (unsigned int)iLenLen )
               {
                  s_iError = 1022;
                  return 0;
               }
               if( iLenLen > 0 )
                  memcpy( ptrRec, ptr, iLenLen );
               ptrRec += iLenLen;
               ptr += iLenLen;
               if( ( iSpace = (pField->uiLen - iLenLen) ) > 0 )
               {
                  while( iSpace-- )
                     *ptrRec++ = ' ';
               }
               break;

            case HB_FT_MEMO:
            case HB_FT_BLOB:
            case HB_FT_PICTURE:
            case HB_FT_OLE:
               *ptrRec = *ptr++;
               break;

            case HB_FT_LOGICAL:
               *ptrRec = *ptr++;
               break;

            case HB_FT_LONG:
            case HB_FT_FLOAT:
               if( pField->uiLen < (unsigned int)iLenLen )
               {
                  s_iError = 1022;
                  return 0;
               }
               ptr++;
               if( ( iSpace = (pField->uiLen - iLenLen) ) > 0 )
               {
                  while( iSpace-- )
                     *ptrRec++ = ' ';
               }
               memcpy( ptrRec, ptr, iLenLen );
               ptr += iLenLen;
               break;

            // binary fields
            case HB_FT_INTEGER:
            case HB_FT_CURRENCY:
            case HB_FT_DOUBLE:
            case HB_FT_DATETIME:
            case HB_FT_MODTIME:
            case HB_FT_DAYTIME:
               memcpy( ptrRec, ptr, pField->uiLen );
               ptr += pField->uiLen;
               break;

            case HB_FT_ANY:
               if( pField->uiLen == 3 || pField->uiLen == 4 )
               {
                  memcpy( ptrRec, ptr, pField->uiLen );
                  ptr += pField->uiLen;
               }
               else
               {
                  *ptrRec++ = *ptr;
                  switch( *ptr++ )
                  {
                     case 'D':
                        memcpy( ptrRec, ptr, 8 );
                        ptr += 8;
                        break;
                     case 'L':
                        *ptrRec = *ptr++;
                        break;
                     case 'N':
/* todo */
                        break;
                     case 'C':
                     {
                        uiLen = leto_b2n( ptr, 2 );
                        memcpy( ptrRec, ptr, uiLen + 2 );
                        ptr += uiLen + 2;
                        break;
                     }
                  }
               }
               break;
         }
      pField ++;
   }
   if( LetoCheckServerVer( pConnection, 100 ) )
      if( *ptr == ';' )
      {
         pTable->ulRecCount = atol( ptr+1 );
      }

   if( pConnection->bTransActive )
   {
      unsigned long ulIndex, ulAreaID, ulRecNo;
      char * ptrPar;

      ptr = (char*) pConnection->szTransBuffer + pConnection->uiTBufOffset;
      for( ulIndex = 0; ulIndex < pConnection->ulRecsInTrans; ulIndex ++ )
      {
         if( ( iLenLen = (((int)*ptr) & 0xFF) ) < 10 )
         {
            ulLen = leto_b2n( ptr+1, iLenLen );
            ptr += iLenLen + 1;
            ptrPar = strchr( ptr, ';' );
            ++ ptrPar;
            sscanf( ptrPar, "%lu;", &ulAreaID );
            if( ulAreaID == pTable->hTable && !strncmp( ptr, "upd;", 4 ) )
            {
               ptrPar = strchr( ptrPar, ';' );
               ++ ptrPar;
               ulRecNo = atol( ptrPar );
               if( ulRecNo == pTable->ulRecNo )
               {
                  int iUpd, i;
                  int n255 = ( pTable->uiFieldExtent > 255 ? 2 : 1 );
                  unsigned int uiField;

                  ptrPar = strchr( ptrPar, ';' );
                  ++ ptrPar;
                  iUpd = atol( ptrPar );
                  ptrPar = strchr( ptrPar, ';' );
                  ++ ptrPar;
                  if( *ptrPar == '1' )
                     pTable->fDeleted = TRUE;
                  else if( *ptrPar == '2')
                     pTable->fDeleted = FALSE;
                  ptrPar = strchr( ptrPar, ';' );
                  ++ ptrPar;

                  for( i=0; i<iUpd; i++ )
                  {
                     uiField = (unsigned int) leto_b2n( ptrPar, n255 ) - 1;
                     pField = pTable->pFields + uiField;
                     ptrPar += n255;

                     ptrRec = (char*) pTable->pRecord + pTable->pFieldOffset[uiField];

                     switch( pField->uiType )
                     {
                        case HB_FT_STRING:
                           if( pField->uiLen < 256 )
                           {
                              uiLen = ((unsigned char)*ptrPar) & 0xFF;
                              ptrPar ++;
                           }
                           else
                           {
                              iLenLen = ((unsigned char)*ptr) & 0xFF;
                              uiLen = leto_b2n( ptrPar, iLenLen );
                              ptrPar += iLenLen + 1;
                           }

                           memcpy( ptrRec, ptrPar, uiLen );
                           ptrPar += uiLen;
                           if( uiLen < pField->uiLen )
                              memset( ptrRec + uiLen, ' ', pField->uiLen - uiLen );

                           break;

                        case HB_FT_LOGICAL:
                           *ptrRec = *ptrPar++;
                           break;

                        case HB_FT_LONG:
                        case HB_FT_FLOAT:
                           uiLen = ((unsigned char)*ptrPar) & 0xFF;
                           ptrPar ++;

                           if( ( iSpace = (pField->uiLen - uiLen) ) > 0 )
                           {
                              while( iSpace-- )
                                 *ptrRec++ = ' ';
                           }
                           memcpy( ptrRec, ptrPar, uiLen );
                           ptrPar += uiLen;
                           break;

                        case HB_FT_INTEGER:
                        case HB_FT_CURRENCY:
                        case HB_FT_DOUBLE:
                        case HB_FT_DATETIME:
                        case HB_FT_MODTIME:
                        case HB_FT_DAYTIME:
                        case HB_FT_DATE:
                           memcpy( ptrRec, ptrPar, pField->uiLen );
                           ptrPar += pField->uiLen;
                     }

                  }
               }
            }
            ptr += ulLen;
         }
      }
   }

#endif

   return 1;
}

static char * leto_ReadMemoInfo( LETOTABLE * pTable, char * ptr )
{
   char szTemp[14];

   if( LetoGetCmdItem( &ptr, szTemp ) )
   {
      ptr ++;
      strcpy( pTable->szMemoExt, szTemp );
      pTable->bMemoType = ( *ptr - '0' );
      ptr += 2;
      LetoGetCmdItem( &ptr, szTemp ); ptr ++;
      sscanf( szTemp, "%d" , (int*) &pTable->uiMemoVersion );
   }

   return ptr;
}

static char * leto_SkipTagInfo( LETOCONNECTION * pConnection, char * ptr )
{
   char szData[24];
   int iOrders, i;

   LetoGetCmdItem( &ptr, szData ); ptr ++;
   sscanf( szData, "%d" , &iOrders );

   if( LetoCheckServerVer( pConnection, 100 ) )
      iOrders *= 9;
   else
      iOrders *= 6;
   for( i = 0; i < iOrders; i++ )
   {
      while( *ptr && *ptr != ';' ) ptr++;
      ptr++;
   }
   return ptr;
}

char * leto_NetName( void )
{
#if defined(HB_OS_UNIX) || ( defined(HB_OS_OS2) && defined(__GNUC__) )

   #define MAXGETHOSTNAME 256

   char szValue[MAXGETHOSTNAME + 1], *szRet;
   unsigned int uiLen;

   szValue[ 0 ] = '\0';
   gethostname( szValue, MAXGETHOSTNAME );

#elif defined(HB_OS_WIN_32) || defined( HB_OS_WIN )

   DWORD uiLen = MAX_COMPUTERNAME_LENGTH+1;
   char szValue[MAX_COMPUTERNAME_LENGTH+1], *szRet;

   szValue[ 0 ] = '\0';
   GetComputerName( szValue, &uiLen );

#else

   return NULL;

#endif

   uiLen = strlen( szValue );
   szRet = (char*) malloc( uiLen+1 );
   memcpy( szRet, szValue, uiLen );
   szRet[uiLen] = '\0';
   return szRet;

}

HB_EXPORT LETOCONNECTION * LetoConnectionNew( const char * szAddr, int iPort, const char * szUser, const char * szPass, int iTimeOut )
{
   HB_SOCKET_T hSocket;
   char szData[300];
   char szBuf[36];
   char szKey[LETO_MAX_KEYLENGTH+1];
   unsigned int i;
   unsigned long int ulLen; 

   for( i=0; i<uiConnCount; i++ )
   {
      if( !letoConnPool[i].pAddr )
         break;
   }
   if( i == uiConnCount )
   {
      USHORT uiCurrentConn = 0;
      if( pCurrentConn )
         uiCurrentConn = pCurrentConn - letoConnPool;
      letoConnPool = realloc( letoConnPool, sizeof( LETOCONNECTION ) * (++uiConnCount) );
      memset( letoConnPool+i, 0, sizeof( LETOCONNECTION ) );
      if( pCurrentConn )
         pCurrentConn = letoConnPool + uiCurrentConn;
   }
   if( iTimeOut == 0 )
      iTimeOut = 5000;
   hSocket = hb_ipConnect( szAddr, htons( iPort ), iTimeOut );
   if( !hb_iperrorcode() )    
   {
      LETOCONNECTION * pConnection = letoConnPool + i;

      pConnection->hSocket = hSocket;
      pConnection->iPort = iPort;
      pConnection->iTimeOut = iTimeOut;
      pConnection->pAddr = (char*) malloc( strlen(szAddr)+1 );
      memcpy( pConnection->pAddr, szAddr, strlen(szAddr) );
      pConnection->pAddr[strlen(szAddr)] = '\0';

      pConnection->bTransActive = FALSE;
      pConnection->szTransBuffer = NULL;
      pConnection->ulTransBuffLen = 0;
      pConnection->ulTransDataLen = 0;
      pConnection->ulRecsInTrans = 0;
      pConnection->ulTransBlockLen = 0;

      pConnection->bRefreshCount = TRUE;

      pConnection->pBufCrypt = NULL;
      pConnection->ulBufCryptLen = 0;

      pConnection->uiBufRefreshTime = 100;

      if( leto_RecvFirst( pConnection ) )
      {
         char * ptr, * pName;
         unsigned int uiSizeLen = (pConnection->uiProto==1)? 0 : LETO_MSGSIZE_LEN;

         ptr = strchr( s_szBuffer,';' );
         if( ptr )
         {
            memcpy( pConnection->szVersion, s_szBuffer, ptr-s_szBuffer );
            pConnection->szVersion[ptr-s_szBuffer] = '\0';
            ptr ++;
            if( *ptr++ == 'Y' )
               pConnection->bCrypt = 1;
            else
               pConnection->bCrypt = 0;
            if( *ptr == ';' )
               pConnection->cDopcode[0] = '\0';
            else
            {
               pConnection->cDopcode[0] = *ptr++;
               pConnection->cDopcode[1] = *ptr;
            }
         }
         else
         {
            memcpy( pConnection->szVersion, s_szBuffer, strlen(s_szBuffer) );
            pConnection->szVersion[strlen(s_szBuffer)] = '\0';
            pConnection->cDopcode[0] = '\0';
         }
         if( ( ptr = strstr( pConnection->szVersion, "v." ) ) != NULL )
            sscanf( ptr+2, "%d.%d", &pConnection->uiMajorVer, &pConnection->uiMinorVer);
         else
            pConnection->uiMajorVer = pConnection->uiMinorVer = 0;

         pName = leto_NetName();
         ptr = szData + uiSizeLen;
         sprintf( ptr, "intro;%s;%s;%s;", 
                        HB_LETO_VERSION_STRING, ( (pName) ? pName : "" ),
                        ( (s_szModName)? s_szModName : "client" ) );
         ptr += strlen( ptr );
         if( pConnection->cDopcode[0] && szUser )
         {
            if( ( ulLen = strlen(szUser) ) > LETO_MAX_USERNAME )
               ulLen = LETO_MAX_USERNAME;
            memcpy( ptr, szUser, ulLen );
            ptr += ulLen;
         }
         *ptr++ = ';';
         if( pConnection->cDopcode[0] && szPass )
         {
            unsigned int uiKeyLen = strlen(LETO_PASSWORD);

            if( uiKeyLen > LETO_MAX_KEYLENGTH )
               uiKeyLen = LETO_MAX_KEYLENGTH;
            if( ( ulLen = strlen(szPass) ) > LETO_MAX_USERNAME )
               ulLen = LETO_MAX_USERNAME;

            memcpy( szKey, LETO_PASSWORD, uiKeyLen );
            szKey[uiKeyLen] = '\0';
            szKey[0] = pConnection->cDopcode[0];
            szKey[1] = pConnection->cDopcode[1];
            leto_encrypt( szPass, ulLen, szBuf, &ulLen, szKey );
            leto_byte2hexchar( szBuf, (int)ulLen, ptr );
            ptr += ulLen * 2;
         }
         *ptr++ = ';';
         sprintf( ptr, "%s;%s;%c\r\n", leto_GetServerCdp( pConnection, ( (s_szCdp)? s_szCdp : "EN" ) ),
                  ( (s_szDateFormat)? s_szDateFormat : "dd/mm/yy" ), s_cCentury );
         if( pName )
            free( pName );
         if( LetoCheckServerVer( pConnection, 100 ) )
         {
            ulLen = strlen(szData+LETO_MSGSIZE_LEN);
            HB_PUT_LE_UINT32( szData, ulLen );
            if ( hb_ipSend( hSocket, szData, LETO_MSGSIZE_LEN+ulLen, LETO_TIMEOUT ) <= 0 )
            {
               s_iConnectRes = LETO_ERR_SEND;
               return NULL;
            }
         }
         else
         {
            if ( hb_ipSend( hSocket, szData, strlen(szData), -1 ) <= 0 )
            {
               s_iConnectRes = LETO_ERR_SEND;
               return NULL;
            }
         }
         if( !leto_Recv( pConnection ) )
         {
            if( pConnection->cDopcode[0] )
               s_iConnectRes = LETO_ERR_LOGIN;
            else
               s_iConnectRes = LETO_ERR_RECV;
            return NULL;
         }
         ptr = leto_firstchar();
         if( !strncmp( ptr, "ACC", 3 ) )
            s_iConnectRes = LETO_ERR_ACCESS;
         else
         {
            pConnection->szVerHarbour[0] = '\0';
            if( ( pName = strchr(ptr, ';') ) != NULL && (pName - ptr) >= 3 )
            {
               memcpy( pConnection->szAccess, ptr, (pName-ptr>3)? 3 : pName-ptr );
               if( ( pName = strchr(pName+1, ';') ) != NULL )
               {
                  memcpy( pConnection->szVerHarbour, ptr, pName-ptr );
                  pConnection->szVerHarbour[pName-ptr] = '\0';
                  if( LetoCheckServerVer( pConnection, 211 ) && *( ptr = pName + 1 ) != '\0' )
                  {
                     int iDriver, iMemoType;
                     sscanf(ptr, "%d;%d", &iDriver, &iMemoType );
                     pConnection->uiDriver = iDriver;
                     pConnection->uiMemoType = iMemoType;
                  }
               }
            }
            s_iConnectRes = 0;
         }
         return pConnection;
      }        
   }
   else
   {
      s_iConnectRes = LETO_ERR_SOCKET;
   }
   return NULL;
}

HB_EXPORT int LetoCloseAll( LETOCONNECTION * pConnection )
{
   char szData[16];

   if( LetoCheckServerVer( pConnection, 100 ) )
      sprintf( szData,"close_all;\r\n" );
   else
      sprintf( szData,"close;00;\r\n" );

   if( leto_DataSendRecv( pConnection, szData, 0 ) )
      return 1;
   else
      return 0;
}

HB_EXPORT void LetoConnectionClose( LETOCONNECTION * pConnection )
{
   hb_ipclose( pConnection->hSocket );
   pConnection->hSocket = 0;
   free( pConnection->pAddr );
   pConnection->pAddr = NULL;
   if( pConnection->szPath )
      free( pConnection->szPath );
   pConnection->szPath = NULL;
   if( pConnection->pCdpTable )
   {
      PCDPSTRU pNext = pConnection->pCdpTable, pCdps;
      while( pNext )
      {
         pCdps = pNext;
         free( pCdps->szClientCdp );
         free( pCdps->szServerCdp );
         pNext = pCdps->pNext;
         free( pCdps );
      }
      pConnection->pCdpTable = NULL;
   }

   if( pConnection->szTransBuffer )
   {
      free( pConnection->szTransBuffer );
      pConnection->szTransBuffer = NULL;
   }

   if( pConnection->pBufCrypt )
   {
      free( pConnection->pBufCrypt );
      pConnection->pBufCrypt = NULL;
   }
   pConnection->ulBufCryptLen = 0;

}

static char * leto_AddFields( LETOTABLE * pTable, unsigned int uiFields, char * szFields )
{
   unsigned int uiCount, uiLen;
   char * ptrStart, * ptr = szFields;
   LETOFIELD * pField;

   pTable->uiFieldExtent = uiFields;

   pTable->pFieldOffset = ( unsigned int * ) malloc( uiFields * sizeof( unsigned int ) );
   memset( pTable->pFieldOffset, 0, uiFields * sizeof( unsigned int ) );
   pTable->pFieldUpd = ( unsigned int * ) malloc( uiFields * sizeof( unsigned int ) );
   memset( pTable->pFieldUpd, 0, uiFields * sizeof( unsigned int ) );

   pTable->pFields = (LETOFIELD*) malloc( sizeof(LETOFIELD) * uiFields );
   pTable->uiRecordLen = 0;

   for( uiCount = 0; uiCount < uiFields; uiCount++ )
   {
      pField = pTable->pFields + uiCount;
      ptrStart = ptr;
      while( *ptr != ';' ) ptr++;
      uiLen = ( (uiLen = ptr - ptrStart ) > 10 )? 10 : uiLen;
      memcpy( pField->szName, ptrStart, uiLen );
      pField->szName[uiLen] = '\0';
      ptr++;

      switch( *ptr )
      {
         case 'C':
            pField->uiType = HB_FT_STRING;
            break;
         case 'N':
            pField->uiType = HB_FT_LONG;
            break;
         case 'L':
            pField->uiType = HB_FT_LOGICAL;
            break;
         case 'D':
            pField->uiType = HB_FT_DATE;
            break;
         case 'M':
            pField->uiType = HB_FT_MEMO;
            break;
         case 'W':
            pField->uiType = HB_FT_BLOB;
            break;
         case 'P':
            pField->uiType = HB_FT_IMAGE;
            break;
         case 'G':
            pField->uiType = HB_FT_OLE;
            break;
         case 'V':
            pField->uiType = HB_FT_ANY;
            break;
         case 'I':
         case '2':
         case '4':
            pField->uiType = HB_FT_INTEGER;
            break;
         case 'F':
            pField->uiType = HB_FT_FLOAT;
            break;
         case 'Y':
            pField->uiType = HB_FT_CURRENCY;
            break;
         case '8':
         case 'B':
            pField->uiType = HB_FT_DOUBLE;
            break;
         case '@':
            pField->uiType = HB_FT_MODTIME;
            break;
         case 'T':
            pField->uiType = HB_FT_TIMESTAMP;
            break;
      }

      while( *ptr != ';' ) ptr++; ptr++;
      pField->uiLen = atoi( ptr );
      pTable->pFieldOffset[uiCount] = pTable->uiRecordLen;
      pTable->uiRecordLen += pField->uiLen;
      while( *ptr != ';' ) ptr++; ptr++;
      pField->uiDec = atoi( ptr );
      while( *ptr != ';' ) ptr++; ptr++;
   }
   return ptr;
}

HB_EXPORT LETOTABLE * LetoDbCreateTable( LETOCONNECTION * pConnection, char * szFile, char * szAlias, char * szFields, unsigned int uiArea )
{
   LETOTABLE * pTable;
   char * szData, * ptr = szFields, szTemp[14];
   unsigned int ui, uiFields = 0;

   while( *ptr )
   {
      for( ui=0; ui<4; ui++ )
      {
         while( *ptr && *ptr++ != ';' );
         if( !(*ptr) && ( *(ptr-1) != ';' || ui < 3 ) )
         {
            s_iError = 1;
            return NULL;
         }
      }
      uiFields ++;
   }

   szData = (char *) malloc( (unsigned int) uiFields * 24 + 10 + _POSIX_PATH_MAX );

   if( LetoCheckServerVer( pConnection, 100 ) )
      sprintf( szData,"creat;%s;%s;%d;%s\r\n", szFile, szAlias, uiFields, szFields );
   else
      sprintf( szData,"creat;01;%d;%s;%s;%d;%s\r\n", uiArea, szFile, szAlias, uiFields, szFields );

   if( !leto_DataSendRecv( pConnection, szData, 0 ) )
   {
      s_iError = 1000;
      free( szData );
      return NULL;
   }
   free( szData );

   ptr = leto_getRcvBuff();
   if( *ptr == '-' )
   {
      if( ptr[4] == ':' )
         sscanf( ptr+5, "%u-%u-", &ui, &s_iError );
      else
         s_iError = 1021;
      return NULL;
   }

   pTable = (LETOTABLE*) malloc( sizeof(LETOTABLE) );
   memset( pTable, 0, sizeof( LETOTABLE ) );
   pTable->uiConnection = pConnection - letoConnPool;
   pTable->iBufRefreshTime = -1;
   pTable->uiSkipBuf = 0;
   pTable->fShared = pTable->fReadonly = 0;

   pTable->pLocksPos  = NULL;
   pTable->ulLocksMax = pTable->ulLocksAlloc = 0;

   leto_AddFields( pTable, uiFields, szFields );

   pTable->pRecord = ( unsigned char * ) malloc( pTable->uiRecordLen+1 );

   ptr = leto_firstchar();

   LetoGetCmdItem( &ptr, szTemp ); ptr ++;
   sscanf( szTemp, "%lu" , &(pTable->hTable) );
   if( *ptr == '1' )
     pTable->uiDriver = LETO_NTX;
   ptr ++; ptr++;

   leto_ParseRecord( pTable, ptr, 1 );

   /*
   if( LetoCheckServerVer( pConnection, 100 ) )
   {
      ptr += HB_GET_LE_UINT24( ptr ) + 3;
      leto_ReadMemoInfo( pTable, ptr );
   }
   */
   return pTable;
}

HB_EXPORT LETOTABLE * LetoDbOpenTable( LETOCONNECTION * pConnection, char * szFile, char * szAlias, int iShared, int iReadOnly, char * szCdp, unsigned int uiArea )
{

   char szData[_POSIX_PATH_MAX + 16], * ptr, szTemp[14];
   LETOTABLE * pTable;
   unsigned int uiFields;

   if( szFile[0] != '+' && szFile[0] != '-' )
   {
      if( LetoCheckServerVer( pConnection, 100 ) )
         sprintf( szData,"open;%s;%s;%c%c;%s;\r\n", szFile,
             szAlias, (iShared)? 'T':'F', (iReadOnly)? 'T':'F', szCdp );
      else
         sprintf( szData,"open;01;%d;%s;%s;%c%c;%s;\r\n", uiArea, szFile,
             szAlias, (iShared)? 'T':'F', (iReadOnly)? 'T':'F', szCdp );

      if( !leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         s_iError = 1000;
         return NULL;
      }
      ptr = leto_getRcvBuff();
      if( *ptr == '-' )
      {
         if( *(ptr+3) == '4' )
            s_iError = 103;
         else
            s_iError = 1021;
         return NULL;
      }

      pTable = (LETOTABLE*) malloc( sizeof(LETOTABLE) );
      memset( pTable, 0, sizeof( LETOTABLE ) );
      pTable->uiConnection = pConnection - letoConnPool;

      ptr = leto_firstchar();
   }
   else
   {
      if( ( ptr = strchr( szFile + 1, ';' ) ) == NULL )
         return NULL;
      pTable = (LETOTABLE*) malloc( sizeof(LETOTABLE) );
      ptr ++;
   }

   pTable->fShared = iShared;
   pTable->fReadonly = iReadOnly;
   pTable->iBufRefreshTime = -1;
   pTable->uiSkipBuf = 0;

   pTable->pLocksPos  = NULL;
   pTable->ulLocksMax = pTable->ulLocksAlloc = 0;

   LetoGetCmdItem( &ptr, szTemp ); ptr ++;
   sscanf( szTemp, "%lu" , &(pTable->hTable) );
   if( *ptr == '1' )
     pTable->uiDriver = LETO_NTX;
   ptr ++; ptr++;

   if( LetoCheckServerVer( pConnection, 100 ) )
   {
      // Read MEMOEXT, MEMOTYPE, MEMOVERSION
      ptr = leto_ReadMemoInfo( pTable, ptr );
      // for DBI_LASTUPDATE
      if( LetoCheckServerVer( pConnection, 208 ) )
      {
         LetoGetCmdItem( &ptr, szTemp ); ptr ++;
         pTable->lLastUpdate = leto_dateEncStr( szTemp );
      }
   }
   // Read number of fields
   LetoGetCmdItem( &ptr, szTemp ); ptr ++;
   sscanf( szTemp, "%d" , &uiFields );
   ptr = leto_AddFields( pTable, uiFields, ptr );

   pTable->pRecord = ( unsigned char * ) malloc( pTable->uiRecordLen+1 );

   if( s_uiAutOpen )
      ptr = leto_ParseTagInfo( pTable, ptr );
   else
      ptr = leto_SkipTagInfo( pConnection, ptr );

   leto_ParseRecord( pTable, ptr, 1 );

   return pTable;
}

HB_EXPORT unsigned int LetoDbCloseTable( LETOTABLE * pTable )
{
   LETOCONNECTION * pConnection;
   //if( pArea->uiUpdated )
   //   leto_PutRec( pArea, FALSE );

   if( !pTable )
      return 1;

   pConnection = letoConnPool + pTable->uiConnection;
   if( pConnection->bTransActive )
   {
      s_iError = 1031;
      return 1;
   }

   if( pTable->hTable )
   {
      char szData[32];  

      if( !pConnection->bCloseAll )
      {
         if( LetoCheckServerVer( pConnection, 100 ) )
            sprintf( szData,"close;%lu;\r\n", pTable->hTable );
         else
            sprintf( szData,"close;01;%lu;\r\n", pTable->hTable );
         if( !leto_SendRecv( pTable, szData, 0, 1021 ) ) return 1;
      }
      pTable->hTable = 0;
   }
   /* Free field offset array */
   if( pTable->pFieldOffset )
   {
      free( pTable->pFieldOffset );
      pTable->pFieldOffset = NULL;
   }
   if( pTable->pFieldUpd )
   {
      free( pTable->pFieldUpd );
      pTable->pFieldUpd = NULL;
   }
   /* Free buffer */
   if( pTable->pRecord )
   {
      free( pTable->pRecord );
      pTable->pRecord = NULL;
   }
   if( pTable->pFields )
   {
      free( pTable->pFields );
      pTable->pFields = NULL;
   }
   if( pTable->szTags )
   {
      free( pTable->szTags );
      pTable->szTags = NULL;
   }  
   if( pTable->Buffer.pBuffer )
   {
      free( pTable->Buffer.pBuffer );
      pTable->Buffer.pBuffer = NULL;
   }
   if( pTable->pLocksPos )
   {
      free( pTable->pLocksPos );
      pTable->pLocksPos  = NULL;
      pTable->ulLocksMax = pTable->ulLocksAlloc = 0;
   }
   if( pTable->pTagInfo )
   {
      LETOTAGINFO * pTagInfo = pTable->pTagInfo, * pTagNext;
      do
      {
         pTagNext = pTagInfo->pNext;
         pTagInfo->pNext = NULL;
         LetoDbFreeTag( pTagInfo );
         pTagInfo = pTagNext;
      }
      while( pTagInfo );
      pTable->pTagInfo = NULL;
   }

   free( pTable );
   return 0;
}

HB_EXPORT unsigned int LetoDbBof( LETOTABLE * pTable )
{
   return pTable->fBof;
}

HB_EXPORT unsigned int LetoDbEof( LETOTABLE * pTable )
{
   return pTable->fEof;
}

HB_EXPORT unsigned int LetoDbGetField( LETOTABLE * pTable, unsigned int uiIndex, char * szRet, unsigned int * uiLen )
{

   unsigned int uiFldLen;

   if( !uiIndex || uiIndex > pTable->uiFieldExtent )
      return 1;

   uiIndex --;
   if( !uiLen )
      uiFldLen = (pTable->pFields+uiIndex)->uiLen;
   else if( !*uiLen || *uiLen > (pTable->pFields+uiIndex)->uiLen )
      *uiLen = uiFldLen = (pTable->pFields+uiIndex)->uiLen;
   else
      uiFldLen = *uiLen;

   memcpy( szRet, pTable->pRecord + pTable->pFieldOffset[uiIndex], uiFldLen );
   szRet[ uiFldLen ] = '\0';

   return 0;
}

HB_EXPORT unsigned int LetoDbPutField( LETOTABLE * pTable, unsigned int uiIndex, char * szValue, unsigned int uiLen )
{
   LETOFIELD * pField;
   unsigned char * ptr, * ptrv;

   if( pTable->fReadonly )
      return 10;

   if( !uiIndex || uiIndex > pTable->uiFieldExtent )
      return 1;

   uiIndex --;
   pField = pTable->pFields+uiIndex;
   if( !uiLen || uiLen > pField->uiLen )
      return 2;

   ptr = pTable->pRecord + pTable->pFieldOffset[uiIndex];

   if( pField->uiType == HB_FT_DATE )
   {
      if( uiLen != pField->uiLen || leto_dateEncStr( szValue ) == 0 )
         return 2;
      memcpy( ptr, szValue, uiLen );
   }
   else if( pField->uiType == HB_FT_LOGICAL )
   {
      if( *szValue != 'T' && *szValue != 'F' )
         return 2;
      memcpy( ptr, szValue, uiLen );
   }
   else if( pField->uiType == HB_FT_LONG )
   {
      unsigned int uiDot = 0;
      ptrv = szValue;
      while( ((unsigned int)((char*)ptrv-szValue)) < uiLen && *ptrv == ' ' ) ptrv ++;
      if( ((unsigned int)((char*)ptrv-szValue)) < uiLen )
      {
         while( ((unsigned int)((char*)ptrv-szValue)) < uiLen )
         {
            if( *ptrv == '.' )
            {
               uiDot = 1;
               if( !pField->uiDec || szValue + uiLen - pField->uiDec - 1 != (char*)ptrv )
                  return 2;
            }
            else if( *ptrv < '0' || *ptrv > '9' )
               return 2;
            ptrv ++;
         }
         if( pField->uiDec && !uiDot )
            return 2;
      }
      if( uiLen < pField->uiLen )
      {
         memset( ptr, ' ', pField->uiLen - uiLen );
         ptr += (pField->uiLen - uiLen);
      }
      memcpy( ptr, szValue, uiLen );
   }
   else if( pField->uiType == HB_FT_STRING )
   {
      memcpy( ptr, szValue, uiLen );
      if(  uiLen < pField->uiLen )
      {
         ptr += uiLen;
         memset( ptr, ' ', pField->uiLen - uiLen );
      }
   }
   else if( pField->uiType == HB_FT_MEMO || pField->uiType == HB_FT_IMAGE || pField->uiType == HB_FT_BLOB || pField->uiType == HB_FT_OLE )
   {
   }

   pTable->uiUpdated |= 2;
   *(pTable->pFieldUpd+uiIndex) = 1;

   return 0;
}

HB_EXPORT unsigned int LetoDbRecNo( LETOTABLE * pTable, unsigned long * ulRecNo )
{
   *ulRecNo = pTable->ulRecNo;
   return 0;
}

HB_EXPORT unsigned int LetoDbRecCount( LETOTABLE * pTable, unsigned long * ulCount )
{
   char szData[32], * ptr;

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
   {
      if( letoConnPool[pTable->uiConnection].bRefreshCount || !pTable->ulRecCount )
      {
         sprintf( szData,"rcou;%lu;\r\n", pTable->hTable );
         if ( !leto_SendRecv( pTable, szData, 0, 1021 ) ) return 1;

         ptr = leto_firstchar();
         pTable->ulRecCount = *ulCount = (unsigned long) atol( ptr );
      }
      else
         *ulCount = pTable->ulRecCount;
   }
   else
   {
      sprintf( szData,"rcou;%lu;\r\n", pTable->hTable );
      if ( !leto_SendRecv( pTable, szData, 0, 1021 ) ) return 1;

      ptr = leto_firstchar();
      pTable->ulRecCount = *ulCount = (unsigned long) atol( ptr );
   }

   return 0;
}

HB_EXPORT unsigned int LetoDbFieldCount( LETOTABLE * pTable, unsigned int * uiCount )
{
   *uiCount = pTable->uiFieldExtent;
   return 0;
}

HB_EXPORT unsigned int LetoDbFieldName( LETOTABLE * pTable, unsigned int uiIndex, char * szName )
{
   if( !szName || !uiIndex || uiIndex > pTable->uiFieldExtent )
      return 1;

   strcpy( szName, (pTable->pFields+uiIndex-1)->szName );
   return 0;
}

HB_EXPORT unsigned int LetoDbFieldType( LETOTABLE * pTable, unsigned int uiIndex, unsigned int * uiType )
{
   if( !uiType || !uiIndex || uiIndex > pTable->uiFieldExtent )
      return 1;

   *uiType = (pTable->pFields+uiIndex-1)->uiType;
   return 0;
}

HB_EXPORT unsigned int LetoDbFieldLen( LETOTABLE * pTable, unsigned int uiIndex, unsigned int * uiLen )
{
   if( !uiLen || !uiIndex || uiIndex > pTable->uiFieldExtent )
      return 1;

   *uiLen = (pTable->pFields+uiIndex-1)->uiLen;
   return 0;
}

HB_EXPORT unsigned int LetoDbFieldDec( LETOTABLE * pTable, unsigned int uiIndex, unsigned int * uiDec )
{
   if( !uiDec || !uiIndex || uiIndex > pTable->uiFieldExtent )
      return 1;

   *uiDec = (pTable->pFields+uiIndex-1)->uiDec;
   return 0;
}

HB_EXPORT unsigned int LetoDbGoTo( LETOTABLE * pTable, unsigned long ulRecNo, char * szTag )
{
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;
   int iFound = 0;

   if( pTable->ulRecNo != ulRecNo && pTable->ptrBuf && leto_HotBuffer( pTable, &pTable->Buffer, pConnection->uiBufRefreshTime ) )
   {
      unsigned char * ptrBuf = pTable->Buffer.pBuffer;
      long lu = 0;
      unsigned long ulRecLen;

      do
      {
         ulRecLen = HB_GET_LE_UINT24( ptrBuf );
         if( !ulRecLen || ulRecLen >= 100000 )
            break;

         if( leto_BufRecNo( ptrBuf + 3 ) == ulRecNo )
         {
            pTable->ptrBuf = ptrBuf;
            pTable->uiRecInBuf = lu;

            leto_ParseRecord( pTable, (char*) pTable->ptrBuf, FALSE );
            iFound = 1;
            break;
         }
         ptrBuf += ulRecLen + 3;
         lu ++;
      }
      while( !leto_OutBuffer( &pTable->Buffer, ptrBuf ) );
   }

   if( !iFound )
   {
      if( !ulRecNo && !pConnection->bRefreshCount && pTable->ulRecCount )
      {
         LetoDbGotoEof( pTable );
      }
      else
      {
         char szData[32], * ptr;
         sprintf( szData,"goto;%lu;%lu;%s;%c;\r\n", pTable->hTable, ulRecNo,
              (szTag)? szTag : "", (char)( (s_uiDeleted)? 0x41 : 0x40 ) );
         if( !leto_SendRecv( pTable, szData, 0, 1021 ) ) return 1;

         ptr = leto_firstchar();
         leto_ParseRecord( pTable, ptr, TRUE );
         if( !pTable->fEof )
            leto_setSkipBuf( pTable, ptr, 0, 0 );
      }
      pTable->ptrBuf = NULL;
   }
   return 0;
}

HB_EXPORT unsigned int LetoDbGoBottom( LETOTABLE * pTable, char * szTag )
{
   char sData[32], *ptr;

   sprintf( sData,"goto;%lu;-2;%s;%c;\r\n", pTable->hTable,
         (szTag)? szTag : "", (char)( (s_uiDeleted)? 0x41 : 0x40 ) );
   if ( !leto_SendRecv( pTable, sData, 0, 1021 ) )
      return 1;

   ptr = leto_firstchar();
   leto_ParseRecord( pTable, ptr, TRUE );
   pTable->ptrBuf = NULL;
   if( !pTable->fEof )
      leto_setSkipBuf( pTable, ptr, 0, 0 );

   return 0;
}

HB_EXPORT unsigned int LetoDbGoTop( LETOTABLE * pTable, char * szTag )
{
   char sData[32], *ptr;

   sprintf( sData,"goto;%lu;-1;%s;%c;\r\n", pTable->hTable,
         (szTag)? szTag : "", (char)( (s_uiDeleted)? 0x41 : 0x40 ) );
   if ( !leto_SendRecv( pTable, sData, 0, 1021 ) )
      return 1;

   ptr = leto_firstchar();
   leto_ParseRecord( pTable, ptr, TRUE );
   pTable->ptrBuf = NULL;
   if( !pTable->fEof )
      leto_setSkipBuf( pTable, ptr, 0, 0 );

   return 0;
}

HB_EXPORT unsigned int LetoDbSkip( LETOTABLE * pTable, long lToSkip, char * szTag )
{
   char sData[60], * ptr;
   int iLenLen;
   ULONG ulDataLen, ulRecLen, ulRecNo = 0;
   LONG lu = 0;
   BYTE * ptrBuf = NULL;
   BOOL bCurRecInBuf = FALSE;
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;

   if( !lToSkip )
   {
      if( ptrBuf )
         pTable->ptrBuf = NULL;
      return 0;
   }
   else if( pTable->ptrBuf && leto_HotBuffer( pTable, &pTable->Buffer, pConnection->uiBufRefreshTime ) )
   {
      if( pTable->BufDirection == (lToSkip > 0 ? 1 : -1) )
      {
         while( lu < ( lToSkip > 0 ? lToSkip : -lToSkip ) )
         {
            ulRecLen = HB_GET_LE_UINT24( pTable->ptrBuf );
            if( ulRecLen >= 100000 )
            {
               s_iError = 1020;
               return 1;
            }
            if( !ulRecLen )
               break;
            pTable->ptrBuf += ulRecLen + 3;
            lu ++;
            if( leto_OutBuffer( &pTable->Buffer, (char *) pTable->ptrBuf ) )
            {
               lu = 0;
               break;
            }
         }
         pTable->uiRecInBuf += lu;
      }
      else if( pTable->uiRecInBuf >= (unsigned int)( lToSkip > 0 ? lToSkip : -lToSkip ) )
      {
         pTable->uiRecInBuf -= ( lToSkip > 0 ? lToSkip : -lToSkip );
         pTable->ptrBuf = pTable->Buffer.pBuffer;
         while( lu < (LONG)pTable->uiRecInBuf )
         {
            ulRecLen = HB_GET_LE_UINT24( pTable->ptrBuf );
            if( ulRecLen >= 100000 )
            {
               s_iError = 1020;
               return 1;
            }
            if( !ulRecLen )
               break;
            pTable->ptrBuf += ulRecLen + 3;
            lu ++;

            if( leto_OutBuffer( &pTable->Buffer, (char *) pTable->ptrBuf ) )
            {
               lu = 0;
               break;
            }
         }
         if( lu == (LONG)pTable->uiRecInBuf )
            lu = ( lToSkip > 0 ? lToSkip : -lToSkip );

      }
      if( lu == ( lToSkip > 0 ? lToSkip : -lToSkip ) )
      {
         ulRecLen = HB_GET_LE_UINT24( pTable->ptrBuf );
         if( ulRecLen )
         {
            if( ptrBuf )
            {
               HB_PUT_LE_UINT24( ptrBuf, 0 );
            }
            leto_ParseRecord( pTable, (char*) pTable->ptrBuf, FALSE );
            pTable->Buffer.uiShoots ++;
            return 0;
         }
      }
   }
   else if( ! ptrBuf && pTable->Buffer.ulBufDataLen > 4 )
   {
   // current record in buffer
      ulRecLen = HB_GET_LE_UINT24( pTable->Buffer.pBuffer );
      if( ulRecLen && leto_HotBuffer( pTable, &pTable->Buffer, pConnection->uiBufRefreshTime ) &&
          ( leto_BufRecNo( (char *) pTable->Buffer.pBuffer + 3 ) == pTable->ulRecNo ) )
      {
         bCurRecInBuf = TRUE;
         ulRecNo = pTable->ulRecNo;
      }
   }

   if( LetoCheckServerVer( pConnection, 100 ) )
   {
      sprintf( sData,"skip;%lu;%ld;%lu;%s;%c;", pTable->hTable, lToSkip,
         pTable->ulRecNo, (szTag)? szTag : "",
         (char)( (s_uiDeleted)? 0x41 : 0x40 ) );
      ptr = sData + strlen(sData);
      if( LetoCheckServerVer( pConnection, 206 ) && pTable->uiSkipBuf > 0 )
      {
         sprintf(ptr, "%d;", pTable->uiSkipBuf );
         ptr += strlen(ptr);
         pTable->uiSkipBuf = 0;
      }
      sprintf( ptr,"\r\n" );
   }
   else
      sprintf( sData,"skip;%lu;%ld;%lu;%s;%c;\r\n", pTable->hTable, lToSkip,
         pTable->ulRecNo, (szTag)? szTag : "",
         (char)( (s_uiDeleted)? 0x41 : 0x40 ) );

   if ( !leto_SendRecv( pTable, sData, 0, 1021 ) ) return 1;

   ptr = leto_getRcvBuff();
   if( ( iLenLen = (((int)*ptr) & 0xFF) ) >= 10 )
   {
      s_iError = 1020;
      return 1;
   }
   pTable->BufDirection = (lToSkip > 0 ? 1 : -1);

   ulDataLen = leto_b2n( ptr+1, iLenLen );
   ptr += 2 + iLenLen;
   leto_ParseRecord( pTable, ptr, TRUE );

   if( ulDataLen > 1 ) 
   {
      if( ! bCurRecInBuf || ( pTable->ulRecNo == ulRecNo ) )
      {
         leto_setSkipBuf( pTable, ptr, ulDataLen, 0 );
         pTable->ptrBuf = pTable->Buffer.pBuffer;
      }
      else
      {
         ulRecLen = HB_GET_LE_UINT24( pTable->Buffer.pBuffer );
         leto_setSkipBuf( pTable, NULL, ulDataLen + ulRecLen + 3, 1 );
         pTable->Buffer.ulBufDataLen += leto_BufStore( pTable, (char *) pTable->Buffer.pBuffer + ulRecLen + 3, ptr, ulDataLen );
         pTable->ptrBuf = pTable->Buffer.pBuffer + ulRecLen + 3;
      }
   }
   else
   {
      pTable->ptrBuf = NULL;
      pTable->uiRecInBuf = 0;
   }
   return 0;
}

HB_EXPORT unsigned int LetoDbSeek( LETOTABLE * pTable, char * szTag, char * szKey, unsigned int uiKeyLen, unsigned int bSoftSeek, unsigned int bFindLast )
{
   char szData[LETO_MAX_KEY+LETO_MAX_TAGNAME+56], * pData;
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;
   unsigned int ui, uiLen;
   unsigned long ulLen;
   LETOTAGINFO * pTagInfo = pTable->pTagInfo;
   unsigned int bSeekBuf;
   unsigned int bSeekLeto = 1;

   //if( pTable->uiUpdated )
   //   leto_PutRec( pArea, FALSE );

   uiLen = strlen( szTag );
   for( ui=0; ui<uiLen; ui++ )
      szData[ui] = HB_TOLOWER(szTag[ui]);
   szData[uiLen] = '\0';
   do
   {
      if( !strcmp( szData, pTagInfo->TagName ) )
         break;
      pTagInfo = pTagInfo->pNext;
   }
   while( pTagInfo );

   if( !pTagInfo )
   {
      s_iError = 1;
      return 1;
   }

   if( !szKey )
   {
      sprintf( szData,"goto;%lu;-3;%s;%c;\r\n", pTable->hTable, pTagInfo->TagName,
           (char)( (s_uiDeleted)? 0x41 : 0x40 ) );
      if( !leto_SendRecv( pTable, szData, 0, 1021 ) )
      {
         s_iError = 1021;
         return 1;
      }
   }
   else
   {
      if( !uiKeyLen )
         uiKeyLen = strlen( szKey );
      bSeekBuf = pTagInfo->uiBufSize && ! bSoftSeek && ! bFindLast &&
          ( pTagInfo->KeyType != 'C' || uiKeyLen == pTagInfo->KeySize );

      if( bSeekBuf && pTagInfo->Buffer.ulBufDataLen && leto_HotBuffer( pTable, &pTagInfo->Buffer, pConnection->uiBufRefreshTime ) )
      {
         char *ptr = (char *) pTagInfo->Buffer.pBuffer;
         unsigned int uiKeyLenBuf;

         for( ;; )
         {
            uiKeyLenBuf = ((unsigned char)*ptr) & 0xFF;
            ptr ++;

            if( ( uiKeyLen == uiKeyLenBuf ) && !memcmp( szKey, ptr, uiKeyLen) )
            {
               ptr += uiKeyLenBuf;
               if( HB_GET_LE_UINT24( ptr ) != 0 )
                  leto_ParseRecord( pTable, ptr, TRUE );
               else
                  LetoDbGotoEof( pTable );
               bSeekBuf = bSeekLeto = 0;
               pTagInfo->Buffer.uiShoots ++;
               break;
            }
            else
            {
               ptr += uiKeyLenBuf;
               ptr += HB_GET_LE_UINT24( ptr ) + 3;
               if( leto_OutBuffer( &pTagInfo->Buffer, ptr ) )
               {
                  break;
               }
            }
         }
      }

      if( bSeekLeto )
      {
         pData = szData + 4;
         sprintf( pData,"seek;%lu;%s;%c;", pTable->hTable, pTagInfo->TagName,
            (char)( ( (s_uiDeleted)? 0x41 : 0x40 ) |
            ( (bSoftSeek)? 0x10 : 0 ) | ( (bFindLast)? 0x20 : 0 ) ) );
         pData = leto_AddKeyToBuf( pData, szKey, uiKeyLen, &ulLen );
         if( !leto_SendRecv( pTable, pData, ulLen, 1021 ) )
         {
            s_iError = 1021;
            return 1;
         }
      }
   }
   if( bSeekLeto )
   {
      pData = leto_firstchar();
      leto_ParseRecord( pTable, pData, TRUE );
   }
   pTable->ptrBuf = NULL;

   if( bSeekLeto && !pTable->fEof )
      leto_setSkipBuf( pTable, pData, 0, 0 );

   if( bSeekBuf && (!pTable->fEof || ( !pConnection->bRefreshCount && pTable->ulRecCount ) ) )
   {
      unsigned long ulRecLen = (!pTable->fEof ? HB_GET_LE_UINT24( pData ) : 0 ) + 3;
      unsigned long ulDataLen = ulRecLen + uiKeyLen + 1;
      unsigned char * ptr;

      if( !leto_HotBuffer( pTable, &pTagInfo->Buffer, pConnection->uiBufRefreshTime ) ||
          ( pTagInfo->uiRecInBuf >= pTagInfo->uiBufSize ) )
      {
         leto_ClearSeekBuf( pTagInfo );
      }

      leto_AllocBuf( &pTagInfo->Buffer, pTagInfo->Buffer.ulBufDataLen + ulDataLen, ulDataLen*5 );

      ptr = pTagInfo->Buffer.pBuffer + pTagInfo->Buffer.ulBufDataLen - ulDataLen;
      *ptr = (uiKeyLen & 0xFF);

      memcpy( ptr + 1, szKey, uiKeyLen );
      if( !pTable->fEof )
         memcpy( ptr + 1 + uiKeyLen, pData, ulRecLen );
      else
         HB_PUT_LE_UINT24( ptr + 1 + uiKeyLen, 0 );
         
      pTagInfo->uiRecInBuf ++;
   }

   return 0;
}

HB_EXPORT unsigned int LetoDbClearFilter( LETOTABLE * pTable )
{
   char szData[32];

   sprintf( szData,"setf;%lu;;\r\n", pTable->hTable );
   if( !leto_SendRecv( pTable, szData, 0, 1021 ) )
   {
      s_iError = 1021;
      return 1;
   }

   return 0;
}

HB_EXPORT unsigned int LetoDbSetFilter( LETOTABLE * pTable, char * szFilter )
{
   char * pData;
   unsigned long ulLen;

   if( szFilter && ( ulLen = strlen( szFilter ) ) != 0 )
   {
      pData = (char*) malloc( ulLen + 24 );
      sprintf( pData,"setf;%lu;%s;\r\n", pTable->hTable, szFilter );

      if( !leto_SendRecv( pTable, pData, 0, 0 ) )
      {
         free( pData );
         s_iError = 1;
         return 1;
      }
      free( pData );

      leto_ClearBuffers( pTable );

      if( *(leto_getRcvBuff()) == '-' )
      {
         s_iError = 1000;
         return 1;
      }
   }
   else
   {
      s_iError = 2;
      return 1;
   }

   return 0;
}

HB_EXPORT unsigned int LetoDbCommit( LETOTABLE * pTable )
{
   char szData[32];

   if( letoConnPool[pTable->uiConnection].bTransActive )
   {
      s_iError = 1;
      return 1;
   }
   sprintf( szData,"flush;%lu;\r\n", pTable->hTable );
   if( !leto_SendRecv( pTable, szData, 0, 1021 ) )
   {
      s_iError = 1021;
      return 1;
   }

   return 0;
}

HB_EXPORT unsigned int LetoDbPutRecord( LETOTABLE * pTable, unsigned int bCommit )
{
   unsigned int ui, uiUpd = 0, uiFields;
   LETOFIELD * pField;
   char * szData, * pData, * ptr, * ptrEnd;
   unsigned int uiRealLen, uiLen;
   unsigned long ulLen;
   unsigned int bAppend = FALSE;
   int n255;
   int iRet = 0;
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;

   leto_ClearAllSeekBuf( pTable );

   if( ! pTable->ptrBuf )
      pTable->Buffer.ulBufDataLen = 0;

   uiFields = pTable->uiFieldExtent;
   for( ui = 0; ui < uiFields; ui++ )
      if( pTable->pFieldUpd[ui] )
         uiUpd ++;

   szData = (char*) malloc( pTable->uiRecordLen + uiUpd * 5 + 20 );

   pData = szData + 4;
   if( pTable->uiUpdated & 1 )
   {
      if( bCommit )
         sprintf( pData, "cmta;%lu;%d;%d;", pTable->hTable, (pTable->uiUpdated & 8)? 1 : 0, uiUpd );
      else
         sprintf( pData, "add;%lu;%d;%d;", pTable->hTable, (pTable->uiUpdated & 8)? 1 : 0, uiUpd );
      bAppend = TRUE;
   }
   else
   {
      if( bCommit )
         sprintf( (char*) pData, "cmtu;%lu;%lu;%d;", pTable->hTable, pTable->ulRecNo, uiUpd );
      else
         sprintf( (char*) pData, "upd;%lu;%lu;%d;", pTable->hTable, pTable->ulRecNo, uiUpd );
   }

   pData += strlen( pData );
   pData[0] = (pTable->uiUpdated & 4)? ((pTable->fDeleted)? '1' : '2') : '0';
   pData[1] = ';';
   pData += 2;

   n255 = ( uiFields > 255 ? 2 : 1 );
   for( ui = 0; ui < uiFields; ui++ )
   {
      if( pTable->pFieldUpd[ui] )
      {
         pField = pTable->pFields + ui;
         ptr = (char*) (pTable->pRecord + pTable->pFieldOffset[ui]);
         ptrEnd = ptr + pField->uiLen - 1;
         // Field number
         pData += leto_n2cb( pData, ui+1, n255 );

         switch( pField->uiType )
         {
            case HB_FT_STRING:
               while( ptrEnd > ptr && *ptrEnd == ' ' ) ptrEnd --;
               uiRealLen = ptrEnd - ptr + ( *ptrEnd == ' ' ? 0 : 1 );
               // Trimmed field length
               if( pField->uiLen < 256 )
               {
                  *pData = (BYTE) uiRealLen & 0xFF;
                  uiLen = 1;
               }
               else
               {
                  uiLen = leto_n2b( pData+1, uiRealLen );
                  *pData = (BYTE) uiLen & 0xFF;
                  uiLen ++;
               }
               pData += uiLen;
               memcpy( pData, ptr, uiRealLen );
               pData += uiRealLen;
               break;

            case HB_FT_LONG:
            case HB_FT_FLOAT:
               while( ptrEnd > ptr && *ptr == ' ' ) ptr ++;
               uiRealLen = ptrEnd - ptr + 1;
               *pData = (BYTE) uiRealLen & 0xFF;
               pData ++;
               memcpy( pData, ptr, uiRealLen );
               pData += uiRealLen;
               break;

            // binary fields
            case HB_FT_INTEGER:
            case HB_FT_CURRENCY:
            case HB_FT_DOUBLE:
            case HB_FT_DATETIME:
            case HB_FT_MODTIME:
            case HB_FT_DAYTIME:

            case HB_FT_DATE:
               memcpy( pData, ptr, pField->uiLen );
               pData += pField->uiLen;
               break;

            case HB_FT_LOGICAL:
               *pData++ = *ptr;
               break;

            case HB_FT_ANY:
               if( pField->uiLen == 3 || pField->uiLen == 4 )
               {
                  memcpy( pData, ptr, pField->uiLen );
                  pData += pField->uiLen;
               }
               else
               {
                  *pData++ = *ptr;
                  switch( *ptr++ )
                  {
                     case 'D':
                        memcpy( pData, ptr, 8 );
                        pData += 8;
                        break;
                     case 'L':
                        *pData++ = *ptr;
                        break;
                     case 'N':
                        break;
                     case 'C':
                        uiLen = leto_b2n( ptr, 2 );
                        memcpy( pData, ptr, uiLen + 2 );
                        pData += uiLen + 2;
                        break;
                  }
               }
               break;
         }
      }
   }
   ulLen = pData - szData - 4;
   pData = leto_AddLen( (szData+4), &ulLen, 1 );
   leto_SetUpdated( pTable, 0);

   if( pConnection->bTransActive )
   {
      leto_AddTransBuffer( pConnection, pData, ulLen );
   }
   else
   {
      if ( !leto_SendRecv( pTable, pData, ulLen, 0 ) )
      {
         s_iError = 1000;
         iRet = 1;
      }
      else if( *( ptr = leto_getRcvBuff() ) == '-' )
      {
         if( ptr[4] == ':' )
            sscanf( ptr+5, "%u-%u-", &ui, &s_iError );
         else
            s_iError = 1021;
         iRet = 1;
      }
      else if( bAppend )
      {
         ptr = leto_firstchar();
         sscanf( ptr, "%lu;", &(pTable->ulRecNo) );
      }
   }
   free( szData );

   return iRet;
}

HB_EXPORT unsigned int LetoDbAppend( LETOTABLE * pTable, unsigned int fUnLockAll )
{
   if( pTable->fReadonly )
      return 10;

   leto_SetUpdated( pTable, (fUnLockAll)? 9 : 1 );
   pTable->fBof = pTable->fEof = pTable->fFound = pTable->fDeleted = 0;
   pTable->ptrBuf = NULL;
   leto_SetBlankRecord( pTable, 1 );
   pTable->ulRecCount ++;

   if( s_bFastAppend || !LetoDbPutRecord( pTable, 0 ) )
      pTable->fRecLocked = 1;
   else
      return 1;

   return 0;
}

HB_EXPORT unsigned int LetoDbOrderCreate( LETOTABLE * pTable, char * szBagName, char * szTag, char * szKey, unsigned char bType, unsigned int uiFlags, char * szFor, char * szWhile, unsigned long ulNext )
{
   char szData[LETO_MAX_EXP*2], * ptr;
   LETOTAGINFO * pTagInfo;
   unsigned int ui, uiLen, uiLenab;

   if( !( uiFlags & ( LETO_INDEX_REST | LETO_INDEX_CUST | LETO_INDEX_FILT ) )
         && ( !szFor || !*szFor ) && ( !szWhile || !*szWhile ) && !ulNext )
      uiFlags |= LETO_INDEX_ALL;

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"creat_i;%lu;%s;%s;%s;%c;%s;%s;%s;%lu;%lu;%s;%s;%s;%s;%s;%s;%s;\r\n", pTable->hTable,
         (szBagName ? szBagName : ""), (szTag ? szTag : ""),
         szKey, ((uiFlags & LETO_INDEX_UNIQ)? 'T' : 'F'), (szFor ? szFor : ""),
         (szWhile ? szWhile : ""),
         ((uiFlags & LETO_INDEX_ALL) ? "T" : "F"),
         pTable->ulRecNo, ulNext, "",
         ((uiFlags & LETO_INDEX_REST) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_DESC) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_CUST) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_ADD) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_TEMP) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_FILT) ? "T" : "F")
         );
   else
      sprintf( szData,"creat;02;%lu;%s;%s;%s;%c;%s;%s;%s;%lu;%lu;%s;%s;%s;%s;%s;\r\n", pTable->hTable,
         (szBagName ? szBagName : ""), (szTag ? szTag : ""),
         szKey, ((uiFlags & LETO_INDEX_UNIQ)? 'T' : 'F'), (szFor ? szFor : ""),
         (szWhile ? szWhile : ""),
         ((uiFlags & LETO_INDEX_ALL) ? "T" : "F"),
         pTable->ulRecNo, ulNext, "",
         ((uiFlags & LETO_INDEX_REST) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_DESC) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_CUST) ? "T" : "F"),
         ((uiFlags & LETO_INDEX_ADD) ? "T" : "F")
         );

   if ( !leto_SendRecv( pTable, szData, 0, 0 ) )
   {
      s_iError = 1000;
      return 1;
   }
   else if( *( ptr = leto_getRcvBuff() ) == '-' )
   {
      if( ptr[4] == ':' )
         sscanf( ptr+5, "%u-%u-", &ui, &s_iError );
      else
         s_iError = 1021;
      return 1;
   }

   pTagInfo = (LETOTAGINFO *) malloc( sizeof(LETOTAGINFO) );
   memset( pTagInfo, 0, sizeof(LETOTAGINFO) );
   if( pTable->pTagInfo )
   {
      LETOTAGINFO * pTagNext = pTable->pTagInfo;
      while( pTagNext->pNext ) pTagNext = pTagNext->pNext;
      pTagNext->pNext = pTagInfo;
   }
   else
   {
      pTable->pTagInfo = pTagInfo;
   }

   if( ( uiLenab = (szBagName)? strlen((char*)szBagName) : 0 ) != 0 )
   {
      pTagInfo->BagName = (char*) malloc( uiLenab+1 );
      memcpy( pTagInfo->BagName, szBagName, uiLenab );
      pTagInfo->BagName[uiLenab] = '\0';
      for( ui=0; ui<uiLenab; ui++ )
         pTagInfo->BagName[ui] = HB_TOLOWER(pTagInfo->BagName[ui]);
   }
   if( ( uiLen = (szTag)? strlen(szTag) : 0 ) != 0 )
   {
      if( uiLen > LETO_MAX_TAGNAME )
         uiLen = LETO_MAX_TAGNAME;
      pTagInfo->TagName = (char*) malloc( uiLen+1 );
      memcpy( pTagInfo->TagName, szTag, uiLen );
      pTagInfo->TagName[uiLen] = '\0';
      for( ui=0; ui<uiLen; ui++ )
         pTagInfo->TagName[ui] = HB_TOLOWER(pTagInfo->TagName[ui]);
   }
   else if( uiLenab )
   {
      pTagInfo->TagName = (char*) malloc( uiLenab+1 );
      memcpy( pTagInfo->TagName, pTagInfo->BagName, uiLenab );
      pTagInfo->TagName[uiLenab] = '\0';
   }

   pTagInfo->KeyExpr = (char*) malloc( (uiLen=strlen(szKey))+1 );
   memcpy( pTagInfo->KeyExpr, szKey, uiLen );
   pTagInfo->KeyExpr[uiLen] = '\0';

   if( ( uiLen = (szFor)? strlen((char*)szFor) : 0 ) != 0 )
   {
      pTagInfo->ForExpr = (char*) malloc( uiLen+1 );
      memcpy( pTagInfo->ForExpr, szFor, uiLen );
      pTagInfo->ForExpr[uiLen] = '\0';
   }
   pTagInfo->KeyType = bType;
   pTagInfo->uiTag = pTable->uiOrders;

   pTagInfo->UsrAscend = !(uiFlags & LETO_INDEX_DESC);
   pTagInfo->UniqueKey = (uiFlags & LETO_INDEX_UNIQ);
   pTagInfo->Custom = (uiFlags & LETO_INDEX_CUST);

   pTable->uiOrders ++;

   return 0;
}

HB_EXPORT unsigned int LetoDbIsRecLocked( LETOTABLE * pTable, unsigned long ulRecNo, unsigned int * uiRes )
{
   s_iError = 0;
   if( ulRecNo != 0 && ulRecNo != pTable->ulRecNo )
   {
      if( !pTable->fShared || pTable->fFLocked || leto_IsRecLocked( pTable, ulRecNo ) )
         * uiRes = 1;
      else
      {
         char szData[32], * ptr;

         sprintf( szData,"islock;%lu;%lu;\r\n", pTable->hTable, ulRecNo );
         if( !leto_SendRecv( pTable, szData, 0, 1021 ) || leto_checkLockError() ) return 1;
         ptr = leto_firstchar();
         *uiRes = ( *ptr == 'T' );
      }
   }
   else
      * uiRes = pTable->fRecLocked;

   return 0;
}

HB_EXPORT unsigned int LetoDbRecLock( LETOTABLE * pTable, unsigned long ulRecNo )
{
   char szData[48];

   leto_ClearBuffers( pTable );
   if( !pTable->fShared || pTable->fFLocked || ( ulRecNo == pTable->ulRecNo ? pTable->fRecLocked : leto_IsRecLocked(pTable, ulRecNo) ) )
      return 0;

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"lock;%lu;01;%lu;\r\n", pTable->hTable, ulRecNo );
   else
      sprintf( szData,"lock;01;%lu;%lu;\r\n", pTable->hTable, ulRecNo );

   if ( !leto_SendRecv( pTable, szData, 0, 0 )|| leto_checkLockError() ) return 1;
   if( ulRecNo == pTable->ulRecNo )
      pTable->fRecLocked = 1;

   return 0;
}

HB_EXPORT unsigned int LetoDbRecUnLock( LETOTABLE * pTable, unsigned long ulRecNo )
{
   char szData[48];

   leto_ClearBuffers( pTable );
   if( (letoConnPool+pTable->uiConnection)->bTransActive )
   {
      s_iError = 1031;
      return 1;
   }
   if( !pTable->fShared || pTable->fFLocked || ( ulRecNo == pTable->ulRecNo ? !pTable->fRecLocked : !leto_IsRecLocked(pTable, ulRecNo) ) )
      return 0;

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"unlock;%lu;01;%lu;\r\n", pTable->hTable, ulRecNo );
   else
      sprintf( szData,"unlock;01;%lu;%lu;\r\n", pTable->hTable, ulRecNo );

   if ( !leto_SendRecv( pTable, szData, 0, 0 ) || leto_checkLockError() ) return 1;
   if( ulRecNo == pTable->ulRecNo )
      pTable->fRecLocked = 0;

   return 0;
}

HB_EXPORT unsigned int LetoDbFileLock( LETOTABLE * pTable )
{
   char szData[48];

   leto_ClearBuffers( pTable );

   if( !pTable->fShared || pTable->fFLocked )
      return SUCCESS;

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"lock;%lu;02;\r\n", pTable->hTable );
   else
      sprintf( szData,"lock;02;%lu;\r\n", pTable->hTable );

   if ( !leto_SendRecv( pTable, szData, 0, 0 )|| leto_checkLockError() ) return 1;

   pTable->fFLocked = 1;

   return 0;
}

HB_EXPORT unsigned int LetoDbFileUnLock( LETOTABLE * pTable )
{
   char szData[48];

   leto_ClearBuffers( pTable );
   if( (letoConnPool+pTable->uiConnection)->bTransActive )
   {
      s_iError = 1031;
      return 1;
   }

   if( !pTable->fShared )
      return SUCCESS;

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"unlock;%lu;02;\r\n", pTable->hTable );
   else
      sprintf( szData,"unlock;02;%lu;\r\n", pTable->hTable );

   if ( !leto_SendRecv( pTable, szData, 0, 0 ) || leto_checkLockError() ) return 1;

   pTable->fFLocked = pTable->fRecLocked = pTable->ulLocksMax = 0;

   return 0;
}

HB_EXPORT unsigned int LetoDbPack( LETOTABLE * pTable )
{
   char szData[32];

   if( letoConnPool[pTable->uiConnection].bTransActive || pTable->fReadonly || pTable->fShared )
   {
      s_iError = 1;
      return 1;
   }

   sprintf( szData,"pack;%lu;\r\n", pTable->hTable );
   if ( !leto_SendRecv( pTable, szData, 0, 1021 ) ) return 2;

   return 0;
}

HB_EXPORT unsigned int LetoDbZap( LETOTABLE * pTable )
{
   char szData[32];

   if( letoConnPool[pTable->uiConnection].bTransActive || pTable->fReadonly || pTable->fShared )
   {
      s_iError = 1;
      return 1;
   }

   sprintf( szData,"zap;%lu;\r\n", pTable->hTable );
   if ( !leto_SendRecv( pTable, szData, 0, 1021 ) ) return 2;

   return 0;
}

HB_EXPORT unsigned int LetoDbReindex( LETOTABLE * pTable )
{
   char szData[32];

   if( letoConnPool[pTable->uiConnection].bTransActive || pTable->fReadonly || pTable->fShared )
   {
      s_iError = 1;
      return 1;
   }

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"ord;%lu;03;\r\n", pTable->hTable );
   else
      sprintf( szData,"ord;03;%lu;\r\n", pTable->hTable );

   if ( !leto_SendRecv( pTable, szData, 0, 1021 ) ) return 2;

   return 0;
}

HB_EXPORT char * LetoMgGetInfo( LETOCONNECTION * pConnection )
{
   if( leto_DataSendRecv( pConnection, "mgmt;00;\r\n", 0 ) )
   {
      return leto_firstchar();
   }
   else
      return NULL;
}

HB_EXPORT char * LetoMgGetUsers( LETOCONNECTION * pConnection, const char * szTable )
{
   char szData[_POSIX_PATH_MAX + 1];

   if( szTable )
      sprintf( szData, "mgmt;01;%s;\r\n", szTable );
   else
      sprintf( szData, "mgmt;01;\r\n" );

   if( leto_DataSendRecv( pConnection, szData, 0 ) )
   {
      return leto_firstchar();
   }
   else
      return NULL;
}

HB_EXPORT char * LetoMgGetTables( LETOCONNECTION * pConnection, const char * szUser )
{
   char szData[64];

   if( szUser )
      sprintf( szData, "mgmt;02;%s;\r\n", szUser );
   else
      sprintf( szData, "mgmt;02;\r\n" );

   if( leto_DataSendRecv( pConnection, szData, 0 ) )
   {
      return leto_firstchar();
   }
   else
      return NULL;
}

HB_EXPORT void LetoMgKillUser( LETOCONNECTION * pConnection, const char * szUserId )
{
   char szData[32];

   sprintf( szData, "mgmt;09;%s;\r\n", szUserId );
   leto_DataSendRecv( pConnection, szData, 0 );

}

HB_EXPORT char * LetoMgGetTime( LETOCONNECTION * pConnection )
{
   if( leto_DataSendRecv( pConnection, "mgmt;03;\r\n", 0 ) )
   {
      return leto_firstchar();
   }
   else
      return NULL;
}

HB_EXPORT int LetoVarSet( LETOCONNECTION * pConnection, char * szGroup, char * szVar, char cType, char * szValue, unsigned int uiFlags, char ** pRetValue )
{
   unsigned long ul, ulLen = 24 + strlen(szGroup) + strlen(szVar);
   char * pData, * ptr, cFlag1 = ' ', cFlag2 = ' ';
   unsigned int uiRes;

   s_iError = -1;
   ptr = szGroup;
   while( *ptr )
      if( *ptr++ == ';' )
         return 0;
   ptr = szVar;
   while( *ptr )
      if( *ptr++ == ';' )
         return 0;

   if( cType == '1' || cType == '2' || cType == '3' )
      ulLen += strlen( szValue );
   else
      return 0;

   cFlag1 |= ( uiFlags & ( LETO_VCREAT | LETO_VOWN | LETO_VDENYWR | LETO_VDENYRD ) );
   if( pRetValue )
      cFlag2 |= LETO_VPREVIOUS;

   pData = ( char * ) malloc( ulLen );
   memcpy( pData, "var;set;", 8 );
   ptr = pData + 8;
   memcpy( ptr, szGroup, ulLen = strlen( szGroup ) );
   ptr += ulLen;
   *ptr++ = ';';
   memcpy( ptr, szVar, ulLen = strlen( szVar ) );
   ptr += ulLen;
   *ptr++ = ';';
   memcpy( ptr, szValue, ulLen = strlen( szValue ) );
   if( cType == '3' )
   {
      for( ul=0; ul < ulLen; ul++, ptr++ )
         if( *ptr == ';' )
            *ptr = '\1';
   }
   else
      ptr += ulLen;
   *ptr++ = ';';
   *ptr++ = cType;
   *ptr++ = cFlag1;
   *ptr++ = cFlag2;
   *ptr++ = ';'; *ptr++ = '\r'; *ptr++ = '\n';
   *ptr = '\0';

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar() - 1;
      uiRes = ( *ptr == '+' );
      ptr++;
   }
   else
      return 0;

   if( uiRes )
   {
      s_iError = 0;
      if( pRetValue )
      {
         pData = ptr;
         ptr += 2;
         while( *ptr != ';' )
         {
            if( *ptr == '\1' )
               *ptr = ';';
            ptr ++;
         }
         *pRetValue = ( char * ) malloc( ulLen = (ptr - pData) );
         memcpy( *pRetValue, pData, ulLen );
         *(*pRetValue + ulLen) = '\0';
      }
   }
   else
      sscanf( ptr+1, "%u", &s_iError );

   return uiRes;
}

HB_EXPORT char * LetoVarGet( LETOCONNECTION * pConnection, char * szGroup, char * szVar )
{
   unsigned long ulLen = 24 + strlen(szGroup) + strlen(szVar);
   char * pData, * ptr;
   unsigned int uiRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "var;get;%s;%s;\r\n", szGroup, szVar );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar() - 1;
      uiRes = ( *ptr == '+' );
      ptr++;
   }
   else
      return NULL;

   if( uiRes )
   {
      char * szRetValue = ( char * ) malloc( (ulLen = strlen(ptr)) + 1 );
      memcpy( szRetValue, ptr, ulLen );
      *(szRetValue + ulLen) = '\0';
      ptr = szRetValue + 2;
      while( * ptr && *ptr != ';' )
      {
         if( *ptr == '\1' )
            *ptr = ';';
         ptr ++;
      }
      *ptr = '\0';
      s_iError = 0;
      return szRetValue;
   }

   sscanf( ptr, "%u", &s_iError );
   return NULL;

}

HB_EXPORT long LetoVarIncr( LETOCONNECTION * pConnection, char * szGroup, char * szVar, unsigned int uiFlags )
{
   unsigned long ulLen = 24 + strlen(szGroup) + strlen(szVar);
   char *pData, *ptr, cFlag1 = ' ';
   long lValue;
   unsigned int uiRes;

   s_iError = -1;

   cFlag1 |= ( uiFlags & ( LETO_VCREAT | LETO_VOWN | LETO_VDENYWR | LETO_VDENYRD ) );
   if( uiFlags & LETO_VOWN )
      cFlag1 |= LETO_VOWN;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "var;inc;%s;%s;2%c!;\r\n", szGroup, szVar, cFlag1 );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar() - 1;
      uiRes = ( *ptr == '+' );
      ptr++;
   }
   else
      return 0;
   if( uiRes )
   {
      sscanf( ptr+2, "%ld", &lValue );
      s_iError = 0;
      return lValue;
   }

   sscanf( ptr, "%u", &s_iError );
   return 0;
}

HB_EXPORT long LetoVarDecr( LETOCONNECTION * pConnection, char * szGroup, char * szVar, unsigned int uiFlags )
{
   unsigned long ulLen = 24 + strlen(szGroup) + strlen(szVar);
   char *pData, *ptr, cFlag1 = ' ';
   long lValue;
   unsigned int uiRes;

   s_iError = -1;

   cFlag1 |= ( uiFlags & ( LETO_VCREAT | LETO_VOWN | LETO_VDENYWR | LETO_VDENYRD ) );
   if( uiFlags & LETO_VOWN )
      cFlag1 |= LETO_VOWN;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "var;dec;%s;%s;2%c!;\r\n", szGroup, szVar, cFlag1 );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar() - 1;
      uiRes = ( *ptr == '+' );
      ptr++;
   }
   else
      return 0;
   if( uiRes )
   {
      sscanf( ptr+2, "%ld", &lValue );
      s_iError = 0;
      return lValue;
   }

   sscanf( ptr, "%u", &s_iError );
   return 0;
}

HB_EXPORT int LetoVarDel( LETOCONNECTION * pConnection, char * szGroup, char * szVar )
{
   unsigned long ulLen = 24 + strlen(szGroup) + strlen(szVar);
   char * pData, * ptr;
   unsigned int uiRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "var;del;%s;%s;\r\n", szGroup, szVar );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar() - 1;
      uiRes = ( *ptr == '+' );
      ptr++;
   }
   else
      return 0;

   if( uiRes )
   {
      s_iError = 0;
      return 1;
   }

   sscanf( ptr, "%u", &s_iError );
   return 0;

}

HB_EXPORT char * LetoVarGetList( LETOCONNECTION * pConnection, const char * szGroup, unsigned int uiMaxLen )
{
   unsigned long ulLen = 32 + ( (szGroup)? strlen(szGroup) : 0 );
   char * pData, * ptr;
   unsigned int uiRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "var;list;%s;;%u;\r\n", (szGroup)? szGroup : "", uiMaxLen );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar() - 1;
      uiRes = ( *ptr == '+' );
      ptr++;
   }
   else
      return NULL;

   if( uiRes )
   {
      char * szRetValue = ( char * ) malloc( (ulLen = strlen(ptr)) + 1 );
      memcpy( szRetValue, ptr, ulLen );
      *(szRetValue + ulLen) = '\0';
      s_iError = 0;
      return szRetValue;
   }

   sscanf( ptr, "%u", &s_iError );
   return NULL;

}

HB_EXPORT int LetoFileExist( LETOCONNECTION * pConnection, char * szFile )
{
   unsigned long ulLen = 24 + strlen(szFile);
   char * pData, * ptr;
   unsigned int uiRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "file;01;%s;\r\n", szFile );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' && *ptr=='T' )
      {
         s_iError = 0;
         return 1;
      }
   }

   return 0;

}

HB_EXPORT int LetoFileErase( LETOCONNECTION * pConnection, char * szFile )
{
   unsigned long ulLen = 24 + strlen(szFile);
   char * pData, * ptr;
   unsigned int uiRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "file;02;%s;\r\n", szFile );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' && (*ptr=='T') )
      {
         s_iError = 0;
         return 1;
      }
      sscanf( ptr+2, "%u", &s_iError );
   }

   return 0;

}

HB_EXPORT int LetoFileRename( LETOCONNECTION * pConnection, char * szFile, char * szFileNew )
{
   unsigned long ulLen = 24 + strlen(szFile) + strlen(szFileNew);
   char * pData, * ptr;
   unsigned int uiRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "file;03;%s;%s;\r\n", szFile, szFileNew );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' && (*ptr=='T') )
      {
         s_iError = 0;
         return 1;
      }
      sscanf( ptr+2, "%u", &s_iError );
   }

   return 0;

}

HB_EXPORT char * LetoMemoRead( LETOCONNECTION * pConnection, char * szFile, unsigned long * ulMemoLen )
{
   unsigned long ulLen = 24 + strlen(szFile);
   char * pData, * ptr;
   unsigned long ulRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "file;04;%s;\r\n", szFile );

   *ulMemoLen = 0;
   ulRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( ulRes )
   {
      ptr = leto_DecryptText( pConnection, &ulLen );

      if( ulLen )
      {
         pData = ( char * ) malloc( ulLen );
         memcpy( pData, ptr, ulLen - 1 );
         *( pData+ulLen ) = '\0';
         s_iError = 0;
         *ulMemoLen = ulLen - 1;
         return pData;
      }
   }

   return NULL;

}

HB_EXPORT int LetoMemoWrite( LETOCONNECTION * pConnection, char * szFile, char * szValue, unsigned long ulLen )
{
   char * pData, * ptr;
   unsigned long ulRes;

   s_iError = -1;

   pData = ( char * ) malloc( 32 + strlen(szFile) + ulLen );
   sprintf( pData, "file;13;%s;;;", szFile );

   ulRes = strlen( pData );

   HB_PUT_LE_UINT32( pData+ulRes, ulLen );  ulRes += 4;
   memcpy( pData+ulRes, szValue, ulLen );
   ulRes += ulLen;

   pData[ulRes  ] = '\r';
   pData[ulRes+1] = '\n';
   pData[ulRes+2] = '0';
   ulRes += 2;

   ulRes = leto_DataSendRecv( pConnection, pData, ulRes );
   free( pData );
   if( ulRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' )
      {
         if( *ptr == 'T' )
         {
            s_iError = 0;
            return 1;
         }
         else
         {
            ptr += 2;
            sscanf( ptr, "%d;", &s_iError );
         }
      }
   }

   return 0;

}

HB_EXPORT char * LetoFileRead( LETOCONNECTION * pConnection, char * szFile, unsigned long ulStart, unsigned long * ulLen )
{
   char * pData, * ptr;
   unsigned long ulRes;

   s_iError = 0;

   pData = ( char * ) malloc( 64 + strlen(szFile) );
   sprintf( pData, "file;10;%s;%lu;%lu;\r\n", szFile, ulStart, *ulLen );

   ulRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if ( ulRes >= 5 )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' )
      {
         if( *ptr == 'T' )
         {
            ptr += 2;
            *ulLen = HB_GET_LE_UINT32( ptr );
            ptr += 4;

            if( *ulLen < ulRes )
            {
               pData = ( char * ) malloc( *ulLen );
               memcpy( pData, ptr, *ulLen );
               *( pData+*ulLen ) = '\0';
               return pData;
            }
            else
               s_iError = -2;
         }
         else
            sscanf( ptr+2, "%d;", &s_iError );
      }
   }
   else
      s_iError = -1;

   *ulLen = 0;
   return NULL;

}

HB_EXPORT int LetoFileWrite( LETOCONNECTION * pConnection, char * szFile, char * szValue, unsigned long ulStart, unsigned long ulLen )
{
   char * pData, * ptr;
   unsigned long ulRes;

   s_iError = 0;

   pData = ( char * ) malloc( 48 + strlen(szFile) + ulLen );
   sprintf( pData, "file;14;%s;%lu;%lu;", szFile, ulStart, ulLen );

   ulRes = strlen( pData );

   HB_PUT_LE_UINT32( pData+ulRes, ulLen );  ulRes += 4;
   memcpy( pData+ulRes, szValue, ulLen );
   ulRes += ulLen;

   pData[ulRes  ] = '\r';
   pData[ulRes+1] = '\n';
   pData[ulRes+2] = '0';
   ulRes += 2;

   ulRes = leto_DataSendRecv( pConnection, pData, ulRes );
   free( pData );
   if( ulRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' )
      {
         if( *ptr == 'T' )
            return 1;
         else
         {
            ptr += 2;
            sscanf( ptr, "%d;", &s_iError );
         }
      }
      else
         s_iError = -2;
   }
   else
      s_iError = -1;

   return 0;

}

HB_EXPORT long int LetoFileSize( LETOCONNECTION * pConnection, char * szFile )
{
   unsigned long ulLen = 24 + strlen(szFile);
   char * pData, * ptr;
   unsigned long ulRes;
  
   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "file;11;%s;\r\n", szFile );

   ulRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( ulRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' )
      {
         if( *ptr == 'T' )
         {
            sscanf( ptr+2, "%lu;", &ulLen );
            s_iError = 0;
            return ulLen;
         }
         else
            sscanf( ptr+2, "%d;", &s_iError );
      }
   }

   return -1;

}

HB_EXPORT char * LetoDirectory( LETOCONNECTION * pConnection, char * szDir, char * szAttr )
{
   unsigned long ulLen = 48 + strlen(szDir);
   char * pData, * ptr;
   unsigned long ulRes;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "file;12;%s;%s;\r\n", szDir, ((szAttr)? szAttr : "") );

   s_iError = -1;
   ulRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( ulRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' )
      {
         ptr += 2;
         ulLen = strlen( ptr );
         pData = ( char * ) malloc( ulLen + 1 );
         memcpy( pData, ptr, ulLen );
         *( pData+ulLen ) = '\0';
         s_iError = 0;
         return pData;
      }
      else
      {
         sscanf( ptr+2, "%d;", &s_iError );
         return NULL;
      }

   }

   return NULL;

}

HB_EXPORT int LetoDirMake( LETOCONNECTION * pConnection, char * szFile )
{
   unsigned long ulLen = 24 + strlen(szFile);
   char * pData, * ptr;
   unsigned int uiRes;

   s_iError = -1;

   pData = ( char * ) malloc( ulLen );
   sprintf( pData, "file;05;%s;\r\n", szFile );

   uiRes = leto_DataSendRecv( pConnection, pData, 0 );
   free( pData );
   if( uiRes )
   {
      ptr = leto_firstchar();
      if( *(ptr-1) == '+' && *ptr=='T' )
      {
         s_iError = 0;
         return 1;
      }
      sscanf( ptr+2, "%u", &s_iError );
   }

   return 0;

}
