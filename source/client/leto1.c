/*  $Id: leto1.c,v 1.149 2010/08/20 15:33:42 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Harbour Leto RDD
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

#define HB_OS_WIN_32_USED
#define HB_SET_IMPORT
#define SUPERTABLE ( &letoSuper )
#define MAX_STR_LEN 255

#if defined (_MSC_VER)
  #define _WINSOCKAPI_
#endif
#include "hbapi.h"
#include "hbinit.h"
#include "hbapiitm.h"
#include "hbapierr.h"
#include "hbdbferr.h"
#include "hbapilng.h"
#include "hbdate.h"
#include "hbset.h"
#include "hbstack.h"
#include "hbvm.h"
#include "rddsys.ch"
#include "rddleto.h"
#include <ctype.h>
#if defined(HB_OS_UNIX)
   #include <netinet/in.h>
   #include <arpa/inet.h>
#endif

#if !defined (SUCCESS)
#define SUCCESS            0
#define FAILURE            1
#endif

#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 ) 
   #define HB_MAXINT    HB_LONG
#endif

static USHORT s_uiRddCount = 0;
static USHORT s_uiRddIdLETO = ( USHORT ) -1;
static RDDFUNCS letoSuper;
static char * s_szBuffer = NULL;
static long int s_lBufferLen = 0;

USHORT uiConnCount = 0;
static LETOCONNECTION * letoConnPool = NULL;
LETOCONNECTION * pCurrentConn = NULL;
static SHORT s_iConnectRes = 1;

static BOOL bTransActive = 0;
static BYTE * szTransBuffer = NULL;
static ULONG ulTransBuffLen;
static ULONG ulTransDataLen = 0;
static USHORT uiRecsInTrans = 0;

static BOOL s_bFastAppend = 0;

char * leto_NetName( void );

char * leto_firstchar( void )
{
   int iLenLen;

   if( ( iLenLen = (((int)*s_szBuffer) & 0xFF) ) < 10 )
      return (s_szBuffer+2+iLenLen);
   else
      return (s_szBuffer+1);
}

static BYTE leto_ItemType( PHB_ITEM pItem )
{
   switch( hb_itemType( pItem ) )
   {
      case HB_IT_STRING:
      case HB_IT_STRING | HB_IT_MEMO:
         return 'C';

      case HB_IT_INTEGER:
      case HB_IT_LONG:
      case HB_IT_DOUBLE:
         return 'N';

      case HB_IT_DATE:
         return 'D';

      case HB_IT_LOGICAL:
         return 'L';

      default:
         return 'U';
   }
}

static ERRCODE commonError( LETOAREAP pArea, USHORT uiGenCode, USHORT uiSubCode,
                            USHORT uiOsCode, char * szFileName, USHORT uiFlags,
                            const char * szOperation )
{
   ERRCODE errCode;
   PHB_ITEM pError;

   pError = hb_errNew();
   hb_errPutGenCode( pError, uiGenCode );
   hb_errPutSubCode( pError, uiSubCode );
   hb_errPutDescription( pError, hb_langDGetErrorDesc( uiGenCode ) );
   if( szOperation )
      hb_errPutOperation( pError, szOperation );
   if( uiOsCode )
      hb_errPutOsCode( pError, uiOsCode );
   if( szFileName )
      hb_errPutFileName( pError, szFileName );
   if ( uiFlags )
      hb_errPutFlags( pError, uiFlags );
   errCode = SUPER_ERROR( ( AREAP ) pArea, pError );
   hb_itemRelease( pError );

   return errCode;
}

static long int leto_Recv( HB_SOCKET_T hSocket )
{
   int iRet, iLenLen = 0;
   char szRet[HB_SENDRECV_BUFFER_SIZE], * ptr;
   long int lLen = 0;

   // leto_writelog("recv",0);
   ptr = s_szBuffer;
   do
   {
      while( hb_ipDataReady( hSocket,2 ) == 0 )
      {
      }
      iRet = hb_ipRecv( hSocket, szRet, HB_SENDRECV_BUFFER_SIZE );
      // leto_writelog(szRet,iRet);
      if( iRet <= 0 )
         break;
      else
      {
         if( (ptr - s_szBuffer) + iRet > s_lBufferLen )
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

   // leto_writelog( s_szBuffer,ptr - s_szBuffer );
   return (long int) (ptr - s_szBuffer);
}

long int leto_DataSendRecv( LETOCONNECTION * pConnection, const char * sData, ULONG ulLen )
{
   if ( hb_ipSend( pConnection->hSocket, sData, (ulLen)? ulLen:strlen(sData), -1 ) <= 0 )
   {
      return 0;
   }
   return leto_Recv( pConnection->hSocket );
}

static int leto_SendRecv( LETOAREAP pArea, char * sData, ULONG ulLen, int iErr )
{
   long int lRet;
   // leto_writelog(sData,(ulLen)? ulLen:0);
   lRet = leto_DataSendRecv( letoConnPool + pArea->uiConnection, sData, ulLen );
   if( !lRet )
      commonError( pArea, EG_DATATYPE, 1000, 0, NULL, 0, NULL );
   else if( *s_szBuffer == '-' && iErr )
   {
      commonError( pArea, EG_DATATYPE, iErr, 0, s_szBuffer, 0, NULL );
      return 0;
   }
   return lRet;
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

LETOCONNECTION * leto_getConnectionPool( void )
{
   return letoConnPool;
}

LETOCONNECTION * leto_ConnectionFind( const char * szAddr, int iPort )
{
   int i;

   for( i=0; i<uiConnCount; i++ )
   {
      if( letoConnPool[i].pAddr && letoConnPool[i].iPort == iPort &&
              !strcmp(letoConnPool[i].pAddr,szAddr) )
         return letoConnPool + i;
   }
   return NULL;
}

static const char * leto_GetServerCdp( LETOCONNECTION * pConnection, const char *szCdp )
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

BOOL letoCheckServerVer( LETOCONNECTION * pConnection, USHORT uiVer )
{
   return (USHORT) ( pConnection->uiMajorVer*100 + pConnection->uiMinorVer ) >= uiVer;
}

LETOCONNECTION * leto_ConnectionNew( const char * szAddr, int iPort, const char * szUser, const char * szPass, int iTimeOut )
{
   HB_SOCKET_T hSocket;
   char szData[300];
   char szBuf[36];
   char szKey[LETO_MAX_KEYLENGTH+1];
   int i;
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
      letoConnPool = hb_xrealloc( letoConnPool, sizeof( LETOCONNECTION ) * (++uiConnCount) );
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
      pConnection->pAddr = (char*) hb_xgrab( strlen(szAddr)+1 );
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

      if( leto_Recv( hSocket ) )
      {
         char * ptr, * pName, **pArgv;
         char szFile[_POSIX_PATH_MAX + 1];

         // leto_writelog( s_szBuffer,0 );
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

         pName = leto_NetName();
         pArgv = hb_cmdargARGV();
         leto_getFileFromPath( *pArgv, szFile );
         sprintf( szData, "intro;%s;%s;%s;", 
            HB_RDD_LETO_VERSION_STRING, (pName)? pName:"", szFile );
         ptr = szData + strlen( szData );
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
            USHORT uiKeyLen = strlen(LETO_PASSWORD);

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
         sprintf( ptr, "%s;%s;%c\r\n",hb_cdp_page->id, hb_setGetDateFormat(), (hb_setGetCentury())? 'T' : 'F' );
         if( pName )
            hb_xfree( pName );
         // leto_writelog( szData,0 );
         if ( hb_ipSend( hSocket, szData, strlen(szData), -1 ) <= 0 )
         {
            s_iConnectRes = LETO_ERR_SEND;
            return NULL;
         }           
         if( !leto_Recv( hSocket ) )
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

BOOL leto_CheckArea( LETOAREAP pArea )
{
   return pArea && ( (AREAP) pArea )->rddID == s_uiRddIdLETO;
}

BOOL leto_CheckAreaConn( AREAP pArea, LETOCONNECTION * pConnection )
{
   return leto_CheckArea( ( LETOAREAP ) pArea ) && ( letoConnPool + ( (LETOAREAP) pArea )->uiConnection == pConnection );
}

static ERRCODE leto_doClose( AREAP pArea, void * p )
{
   if( leto_CheckAreaConn( pArea, ( LETOCONNECTION * ) p ) )
   {
      SELF_CLOSE( pArea );
   }
   return SUCCESS;
}

BOOL leto_CloseAll( LETOCONNECTION * pConnection )
{
   char szData[16];

   pConnection->bCloseAll = 1;
   /* hb_rddCloseAll(); */
   hb_rddIterateWorkAreas( leto_doClose, (void *) pConnection );
   pConnection->bCloseAll = 0;

   sprintf( szData,"close_all;\r\n" );
   if( leto_DataSendRecv( pConnection, szData, 0 ) )
      return TRUE;
   else
      return FALSE;
}

static ERRCODE leto_CheckAreas( AREAP pArea, void * p )
{
   if( leto_CheckAreaConn( pArea, ( LETOCONNECTION * ) p ) )
   {
      ( ( LETOCONNECTION * ) p )->bCloseAll = 1;
   }
   return SUCCESS;
}

void leto_ConnectionClose( LETOCONNECTION * pConnection )
{

   if( pConnection->pAddr )
   {
      hb_ipclose( pConnection->hSocket );
      pConnection->hSocket = 0;
      hb_xfree( pConnection->pAddr );
      pConnection->pAddr = NULL;
      if( pConnection->szPath )
         hb_xfree( pConnection->szPath );
      pConnection->szPath = NULL;

      if( pConnection->pCdpTable )
      {
         PCDPSTRU pNext = pConnection->pCdpTable, pCdps;
         while( pNext )
         {
            pCdps = pNext;
            hb_xfree( pCdps->szClientCdp );
            hb_xfree( pCdps->szServerCdp );
            pNext = pCdps->pNext;
            hb_xfree( pCdps );
         }
         pConnection->pCdpTable = NULL;
      }

      if( pConnection->szTransBuffer )
      {
         hb_xfree( pConnection->szTransBuffer );
         pConnection->szTransBuffer = NULL;
      }

      if( pConnection->pBufCrypt )
      {
         hb_xfree( pConnection->pBufCrypt );
         pConnection->pBufCrypt = NULL;
      }
      pConnection->ulBufCryptLen = 0;

   }

}

int leto_getCmdItem( char ** pptr, char * szDest )
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

static BOOL letoIsBinaryField( USHORT uiType, USHORT uiLen )
{
   return ( ( uiType == HB_FT_MEMO || uiType == HB_FT_BLOB ||
              uiType == HB_FT_PICTURE || uiType == HB_FT_OLE ) && uiLen == 4 ) ||
          ( uiType == HB_FT_DATE && uiLen <= 4 ) ||
          uiType == HB_FT_DATETIME ||
          uiType == HB_FT_DAYTIME ||
          uiType == HB_FT_MODTIME ||
          uiType == HB_FT_ANY ||
          uiType == HB_FT_INTEGER ||
          uiType == HB_FT_DOUBLE;
}

static void letoSetBlankRecord( LETOAREAP pArea, BOOL bAppend )
{
   USHORT uiCount;
   LPFIELD pField;

   if( bAppend )
      pArea->ulRecNo = 0;
   memset( pArea->pRecord, ' ', pArea->uiRecordLen );

   for( uiCount = 0, pField = pArea->area.lpFields; uiCount < pArea->area.uiFieldCount; uiCount++, pField++ )
   {
      USHORT uiLen = pField->uiLen;

      if( letoIsBinaryField( pField->uiType, uiLen ) )
      {
         memset(pArea->pRecord + pArea->pFieldOffset[uiCount], 0, uiLen);
      }
   }
}

static char * leto_DecryptBuf( LETOCONNECTION * pConnection, const char * ptr, ULONG * pulLen )
{
   if( *pulLen > pConnection->ulBufCryptLen )
   {
      if( !pConnection->ulBufCryptLen )
         pConnection->pBufCrypt = (char*) hb_xgrab( *pulLen );
      else
         pConnection->pBufCrypt = (char*) hb_xrealloc( pConnection->pBufCrypt, *pulLen );
      pConnection->ulBufCryptLen = *pulLen;
   }
   leto_decrypt( ptr, *pulLen, pConnection->pBufCrypt, pulLen, LETO_PASSWORD );
   return pConnection->pBufCrypt;
}

static int leto_ParseRec( LETOAREAP pArea, char * szData )
{
   int iLenLen, iSpace;
   char * ptr, * ptrRec;
   char szTemp[24];
   USHORT uiCount, uiLen;
   LPFIELD pField;
   ULONG ulLen;
   LETOCONNECTION * pConnection = letoConnPool + pArea->uiConnection;

   ulLen = HB_GET_LE_UINT24( szData );
   ptr = (char *) szData + 3;

   if( pConnection->bCrypt )
   {
      ptr = leto_DecryptBuf( pConnection, ptr, &ulLen );
   }

   leto_getCmdItem( &ptr, szTemp ); ptr ++;

   pArea->area.fBof = ( (*szTemp) & LETO_FLG_BOF );
   pArea->area.fEof = ( (*szTemp) & LETO_FLG_EOF );
   pArea->fDeleted = ( *(szTemp) & LETO_FLG_DEL );
   pArea->area.fFound = ( *(szTemp) & LETO_FLG_FOUND );
   pArea->fRecLocked = ( *(szTemp) & LETO_FLG_LOCKED );
   sscanf( szTemp+1, "%lu" , &(pArea->ulRecNo) );

   if( pArea->area.fEof )
   {
      letoSetBlankRecord( pArea, FALSE );
   }
   pField = pArea->area.lpFields;
   for( uiCount = 0; uiCount < pArea->area.uiFieldCount; uiCount++ )
   {
      ptrRec = (char*) pArea->pRecord + pArea->pFieldOffset[uiCount];
      iLenLen = ((unsigned char)*ptr) & 0xFF;

      if( pArea->area.fEof )
      {
         ptr ++;
      }
      else if( !iLenLen && !letoIsBinaryField( pField->uiType, pField->uiLen ) )
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
               if( pField->uiLen < iLenLen )
               {
                  commonError( pArea, EG_DATATYPE, 1022, 0, NULL, 0, NULL );
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
               if( pField->uiLen < iLenLen )
               {
                  commonError( pArea, EG_DATATYPE, 1022, 0, NULL, 0, hb_dynsymName( ( PHB_DYNS ) pField->sym ) );
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

   return 1;
}

static BOOL leto_checkerror( LETOAREAP pArea )
{
   if( *s_szBuffer == '-' )
   {
      ULONG uiGenCode, uiSubCode, uiOsCode, uiFlags;
      char * szFileName = NULL;

      if( s_szBuffer[4] == ':' )
      {
         sscanf( s_szBuffer+5, "%lu-%lu-%lu-%lu", &uiGenCode, &uiSubCode, &uiOsCode, &uiFlags );
         if( ( szFileName = strchr( s_szBuffer+5, '\t' ) ) != NULL)
            szFileName ++;
      }
      else
      {
         uiGenCode = EG_DATATYPE;
         uiSubCode = 1021;
         uiOsCode = 0;
         uiFlags = 0;
      }

      commonError( pArea, (USHORT) uiGenCode, (USHORT) uiSubCode, (USHORT) uiOsCode,
                   szFileName, (USHORT) uiFlags, NULL );
      return TRUE;
   }
   else
      return FALSE;
}

static int leto_PutRec( LETOAREAP pArea )
{
   USHORT ui, uiUpd = 0;
   LPFIELD pField;
   char * szData, * pData, * ptr, * ptrEnd;
   USHORT uiRealLen, uiLen;
   ULONG ulLen;
   BOOL bAppend = FALSE;
   int n255;
   int iRet = 1;

   for( ui = 0; ui < pArea->area.uiFieldCount; ui++ )
      if( pArea->pFieldUpd[ui] )
         uiUpd ++;

   szData = (char*) malloc( pArea->uiRecordLen + uiUpd * 5 + 20 );

   pData = szData + 4;
   if( pArea->uiUpdated & 1 )
   {
      sprintf( pData, "add;%lu;%d;%d;", pArea->hTable,
               (pArea->uiUpdated & 8)? 1 : 0, uiUpd );
      bAppend = TRUE;
   }
   else
      sprintf( (char*) pData, "upd;%lu;%lu;%d;", pArea->hTable, pArea->ulRecNo, uiUpd );

   pData += strlen( pData );
   pData[0] = (pArea->uiUpdated & 4)? ((pArea->fDeleted)? '1' : '2') : '0';
   pData[1] = ';';
   pData += 2;

   n255 = ( pArea->area.uiFieldCount > 255 ? 2 : 1 );
   for( ui = 0; ui < pArea->area.uiFieldCount; ui++ )
   {
      if( pArea->pFieldUpd[ui] )
      {
         pField = pArea->area.lpFields + ui;
         ptr = (char*) (pArea->pRecord + pArea->pFieldOffset[ui]);
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
   pArea->uiUpdated = 0;
   memset( pArea->pFieldUpd, 0, pArea->area.uiFieldCount * sizeof( USHORT ) );

   if( bTransActive )
   {
      if( !szTransBuffer )
      {
         ulTransBuffLen = (ulLen<256)? 1024 : ulLen*3;
         szTransBuffer = (BYTE*) hb_xgrab( ulTransBuffLen );
         memcpy( szTransBuffer+4, "ta;", 3 );
      }
      if( !ulTransDataLen )
         ulTransDataLen = 10;
      if( ulTransBuffLen - ulTransDataLen <= ulLen )
      {
         ulTransBuffLen = ulTransDataLen + ( (ulLen<256)? 1024 : ulLen*3 );
         szTransBuffer = (BYTE*) hb_xrealloc( szTransBuffer, ulTransBuffLen );
      }
      memcpy( szTransBuffer+ulTransDataLen, pData, ulLen );
      ulTransDataLen += ulLen;
      uiRecsInTrans ++;
   }
   else
   {
      if ( !leto_SendRecv( pArea, pData, ulLen, 0 ) || leto_checkerror( pArea ) )
      {
         iRet = FAILURE;
      }
      else if( bAppend )
      {
         pData = leto_firstchar();
         sscanf( pData, "%lu;", &(pArea->ulRecNo) );
      }
   }
   free( szData );
   return iRet;
}

static void leto_CreateKeyExpr( LETOAREAP pArea, LETOTAGINFO * pTagInfo, const char * szData)
{
   unsigned int uiLen;
   if( ( uiLen = strlen(szData) ) != 0 )
   {
      pTagInfo->KeyExpr = (char*) hb_xgrab( uiLen+1 );
      memcpy( pTagInfo->KeyExpr, szData, uiLen );
      pTagInfo->KeyExpr[uiLen] = '\0';

      if ( SELF_COMPILE( ( AREAP ) pArea, pTagInfo->KeyExpr ) != FAILURE )
      {
         pTagInfo->pKeyItem = pArea->area.valResult;
         pArea->area.valResult = NULL;
      }
   }
}

static char * SkipTagInfo( char * pBuffer )
{
   char szData[24];
   char * ptr = pBuffer;
   int iOrders, i;

   leto_getCmdItem( &ptr, szData ); ptr ++;
   sscanf( szData, "%d" , &iOrders );  
   for( i = 0; i < iOrders*6; i++ )
   {
      while( *ptr && *ptr != ';' ) ptr++;
      ptr++;
   }
   return ptr;
}

static char * ParseTagInfo( LETOAREAP pArea, char * pBuffer )
{
   LETOTAGINFO * pTagInfo, * pTagNext;
   USHORT uiCount;
   int iOrders, iLen;
   char szData[_POSIX_PATH_MAX];
   char * ptr = pBuffer;

   leto_getCmdItem( &ptr, szData ); ptr ++;
   sscanf( szData, "%d" , &iOrders );  

   if( iOrders )
   {
      if( pArea->pTagInfo )
      {
         pTagInfo = pArea->pTagInfo;
         while( pTagInfo->pNext )
            pTagInfo = pTagInfo->pNext;
         pTagInfo->pNext = (LETOTAGINFO *) hb_xgrab( sizeof(LETOTAGINFO) );
         pTagInfo = pTagInfo->pNext;
      }
      else
      {
         pTagInfo = pArea->pTagInfo = (LETOTAGINFO *) hb_xgrab( sizeof(LETOTAGINFO) );
      }
      for( uiCount = 1; uiCount <= iOrders; uiCount++ )
      {
         memset( pTagInfo, 0, sizeof(LETOTAGINFO) );

         pTagInfo->uiTag = pArea->iOrders + uiCount;
         leto_getCmdItem( &ptr, szData ); ptr ++;
         if( ( iLen = strlen(szData) ) != 0 )
         {
            pTagInfo->BagName = (char*) hb_xgrab( iLen+1 );
            memcpy( pTagInfo->BagName, szData, iLen );
            pTagInfo->BagName[iLen] = '\0';
            hb_strLower( pTagInfo->BagName, iLen );
         }
         leto_getCmdItem( &ptr, szData ); ptr ++;
         if( ( iLen = strlen(szData) ) != 0 )
         {
            pTagInfo->TagName = (char*) hb_xgrab( iLen+1 );
            memcpy( pTagInfo->TagName, szData, iLen );
            pTagInfo->TagName[iLen] = '\0';
            hb_strLower( pTagInfo->TagName, iLen );
         }
         leto_getCmdItem( &ptr, szData ); ptr ++;
         leto_CreateKeyExpr( pArea, pTagInfo, szData );
         leto_getCmdItem( &ptr, szData ); ptr ++;
         if( ( iLen = strlen(szData) ) != 0 )
         {
            pTagInfo->ForExpr = (char*) hb_xgrab( iLen+1 );
            memcpy( pTagInfo->ForExpr, szData, iLen );
            pTagInfo->ForExpr[iLen] = '\0';
         }
         leto_getCmdItem( &ptr, szData ); ptr ++;
         pTagInfo->KeyType = szData[0];

         leto_getCmdItem( &ptr, szData ); ptr ++;
         sscanf( szData, "%d", &iLen );
         pTagInfo->KeySize = iLen;

         if( uiCount < iOrders )
         {
            pTagNext = (LETOTAGINFO *) hb_xgrab( sizeof(LETOTAGINFO) );
            pTagInfo->pNext = pTagNext;
            pTagInfo = pTagNext;
         }
      }
      pArea->iOrders += iOrders;
   }
   return ptr;
}

/*
 * -- LETO RDD METHODS --
 */

static ERRCODE letoBof( LETOAREAP pArea, BOOL * pBof )
{
   HB_TRACE(HB_TR_DEBUG, ("letoBof(%p, %p)", pArea, pBof));

   /* resolve any pending relations */
   if( pArea->lpdbPendingRel )
      SELF_FORCEREL( ( AREAP ) pArea );

   *pBof = pArea->area.fBof;

   return SUCCESS;
}

static ERRCODE letoEof( LETOAREAP pArea, BOOL * pEof )
{
   HB_TRACE(HB_TR_DEBUG, ("letoEof(%p, %p)", pArea, pEof));

   /* resolve any pending relations */
   if( pArea->lpdbPendingRel )
      SELF_FORCEREL( ( AREAP ) pArea );

   *pEof = pArea->area.fEof;

   return SUCCESS;
}

static ERRCODE letoFound( LETOAREAP pArea, BOOL * pFound )
{
   /* resolve any pending relations */
   if( pArea->lpdbPendingRel )
      SELF_FORCEREL( ( AREAP ) pArea );

   *pFound = pArea->area.fFound;

   return SUCCESS;
}

static ERRCODE letoGoBottom( LETOAREAP pArea )
{
   char sData[32], * ptr;
   HB_TRACE(HB_TR_DEBUG, ("letoGoBottom(%p)", pArea));

   pArea->lpdbPendingRel = NULL;
   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   sprintf( sData,"goto;%lu;-2;%s;%c;\r\n", pArea->hTable,
            (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
            (char)( (hb_setGetDeleted())? 0x41 : 0x40 ) );
   if ( !leto_SendRecv( pArea, sData, 0, 1021 ) ) return FAILURE;

   ptr = leto_firstchar();
   leto_ParseRec( pArea, ptr );
   pArea->ptrBuf = NULL;

   SELF_SKIPFILTER( ( AREAP ) pArea, -1 );

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   else
      return SUCCESS;
}

static ERRCODE letoGoTo( LETOAREAP pArea, ULONG ulRecNo )
{
   char sData[32], * ptr;
   HB_TRACE(HB_TR_DEBUG, ("letoGoTo(%p, %lu)", pArea, ulRecNo));

   pArea->lpdbPendingRel = NULL;
   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   sprintf( sData,"goto;%lu;%lu;%s;%c;\r\n", pArea->hTable, ulRecNo,
        (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
        (char)( (hb_setGetDeleted())? 0x41 : 0x40 ) );
   if ( !leto_SendRecv( pArea, sData, 0, 1021 ) ) return FAILURE;

   ptr = leto_firstchar();
   leto_ParseRec( pArea, ptr );
   pArea->ptrBuf = NULL;

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   else
      return SUCCESS;
}

static ERRCODE letoGoToId( LETOAREAP pArea, PHB_ITEM pItem )
{
   ULONG ulRecNo;

   HB_TRACE(HB_TR_DEBUG, ("letoGoToId(%p, %p)", pArea, pItem));

   if( HB_IS_NUMERIC( pItem ) )
   {
      ulRecNo = hb_itemGetNL( pItem );
      return SELF_GOTO( ( AREAP ) pArea, ulRecNo );
   }
   else
   {
      commonError( pArea, EG_DATATYPE, 1020, 0, NULL, 0, NULL );
      return FAILURE;
   }
}

static ERRCODE letoGoTop( LETOAREAP pArea )
{
   char sData[32], * ptr;
   HB_TRACE(HB_TR_DEBUG, ("letoGoTop(%p)", pArea));

   pArea->lpdbPendingRel = NULL;
   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   sprintf( sData,"goto;%lu;-1;%s;%c;\r\n", pArea->hTable,
           (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
           (char)( (hb_setGetDeleted())? 0x41 : 0x40 ) );
   if ( !leto_SendRecv( pArea, sData, 0, 1021 ) )
      return FAILURE;

   ptr = leto_firstchar();
   leto_ParseRec( pArea, ptr );
   pArea->ptrBuf = NULL;

   SELF_SKIPFILTER( ( AREAP ) pArea, 1 );

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   else
      return SUCCESS;
}

static USHORT letoKeyToStr( LETOAREAP pArea, char * szKey, char cType, PHB_ITEM pKey )
{
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
   USHORT uiKeyLen;
#else
   HB_SIZE uiKeyLen;
#endif

   if( cType == 'N' )
   {
      char * sNum = hb_itemStr( pKey, NULL, NULL );
      uiKeyLen = strlen(sNum);
      memcpy( szKey, sNum, uiKeyLen );
      hb_xfree( sNum );
   }
   else if( cType == 'D' )
   {
      hb_itemGetDS( pKey, szKey );
      uiKeyLen = 8;
   }
   else if( cType == 'L' )
   {
      szKey[0] = (hb_itemGetL(pKey))? 'T':'F';
      uiKeyLen = 1;
   }
   else
   {
      uiKeyLen = hb_itemGetCLen(pKey);
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
      memcpy( szKey, hb_itemGetCPtr(pKey), uiKeyLen );
      if( pArea->area.cdPage != hb_cdp_page )
         hb_cdpnTranslate( szKey, hb_cdp_page, pArea->area.cdPage, uiKeyLen );
#else
      if( pArea->area.cdPage != hb_cdp_page )
      {
         char * pBuff = hb_cdpnDup( hb_itemGetCPtr(pKey), &uiKeyLen, hb_cdp_page, pArea->area.cdPage );
         memcpy( szKey, pBuff, uiKeyLen );
         hb_xfree( pBuff );
      }
      else
         memcpy( szKey, hb_itemGetCPtr(pKey), uiKeyLen );
#endif
   }
   szKey[uiKeyLen] = '\0';
   return uiKeyLen;
}

static int itmCompare( PHB_ITEM pValue, PHB_ITEM pItem, BOOL bExact )
{
   int iRet = 1;

   if( HB_IS_STRING( pValue ) && HB_IS_STRING( pItem ) )
   {
      iRet = hb_itemStrCmp( pValue, pItem, bExact );
   }
   else if( HB_IS_DATE( pValue ) && HB_IS_DATE( pItem ) )
   {
#ifdef __XHARBOUR__
      if( pItem->item.asDate.value == pValue->item.asDate.value &&
          pItem->item.asDate.time == pValue->item.asDate.time )
#else
#if defined(HB_VER_SVNID) && ( (HB_VER_SVNID - 0) >= 10669 )
      if( hb_itemGetTD( pItem ) == hb_itemGetTD( pValue ) )
#else
      if( hb_itemGetDL( pItem ) == hb_itemGetDL( pValue ) )
#endif
#endif
      {
         iRet = 0;
      }
#ifdef __XHARBOUR__
      else if( pValue->item.asDate.value < pItem->item.asDate.value )
#else
#if defined(HB_VER_SVNID) && ( (HB_VER_SVNID - 0) >= 10669 )
      else if( hb_itemGetTD( pValue ) < hb_itemGetTD( pItem ) )
#else
      else if( hb_itemGetDL( pValue ) < hb_itemGetDL( pItem ) )
#endif
#endif
      {
         iRet = -1;
      }
   }
   else if( HB_IS_NUMINT( pValue ) && HB_IS_NUMINT( pItem ) )
   {
      HB_LONG l1 = hb_itemGetNInt( pValue );
      HB_LONG l2 = hb_itemGetNInt( pItem );
      if( l1 == l2 )
      {
         iRet = 0;
      }
      else if( l1 < l2 )
      {
         iRet = -1;
      }
   }
   else if( HB_IS_NUMBER( pValue ) && HB_IS_NUMBER( pItem ) )
   {
      double d1 = hb_itemGetND( pValue );
      double d2 = hb_itemGetND( pItem );
      if( d1 == d2 )
      {
         iRet = 0;
      }
      else if( d1 < d2 )
      {
         iRet = -1;
      }
   }
   else if( HB_IS_LOGICAL( pValue ) && HB_IS_LOGICAL( pItem ) )
   {
      BOOL b1 = hb_itemGetL( pValue );
      BOOL b2 = hb_itemGetL( pItem );
      if( b1 == b2 )
      {
         iRet = 0;
      }
      else if(! b1 )
      {
         iRet = -1;
      }
   }

   return iRet;
}

static ERRCODE letoSeek( LETOAREAP pArea, BOOL bSoftSeek, PHB_ITEM pKey, BOOL bFindLast )
{
   char szKey[LETO_MAX_KEY+1];
   char szData[LETO_MAX_KEY+LETO_MAX_TAGNAME+56], * pData, cType;
   USHORT uiKeyLen;
   ULONG ulLen;

   pArea->lpdbPendingRel = NULL;
   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   if( ! pArea->pTagCurrent )
   {
      commonError( pArea, EG_NOORDER, 1201, 0, NULL, EF_CANDEFAULT, NULL );
      return FAILURE;
   }

   if( ( cType = leto_ItemType( pKey ) ) != pArea->pTagCurrent->KeyType )
   {
      sprintf( szData,"goto;%lu;-3;%s;%c;\r\n", pArea->hTable,
           (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
           (char)( (hb_setGetDeleted())? 0x41 : 0x40 ) );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
   }
   else
   {
      uiKeyLen = letoKeyToStr( pArea, szKey, cType, pKey );
      /*
      sprintf( szData,"seek;%d;%s;%s;%c%c;\r\n", pArea->hTable, szKey,
         (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
         (bSoftSeek)? 'T':'F', (bFindLast)? 'T':'F'  );
      */
      pData = szData + 4;
      sprintf( pData,"seek;%lu;%s;%c;", pArea->hTable,
         (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
         (char)( ( (hb_setGetDeleted())? 0x41 : 0x40 ) |
         ( (bSoftSeek)? 0x10 : 0 ) | ( (bFindLast)? 0x20 : 0 ) ) );
      pData = ( pData + strlen( pData ) );
      *pData++ = (char) (uiKeyLen) & 0xFF;
      memcpy( pData,szKey,uiKeyLen );
      pData += uiKeyLen;
      *pData = '\0';

      ulLen = pData - szData - 4;
      pData = leto_AddLen( (szData+4), &ulLen, 1 );
      if ( !leto_SendRecv( pArea, pData, (ULONG) ulLen, 1021 ) ) return FAILURE;
   }

   pData = leto_firstchar();
   leto_ParseRec( pArea, pData );
   pArea->ptrBuf = NULL;

   if( pArea->area.dbfi.itmCobExpr != NULL && !pArea->area.dbfi.fOptimized )
   {
      if( SELF_SKIPFILTER( ( AREAP ) pArea, ( bFindLast ? -1 : 1 ) ) == SUCCESS )
      {
         PHB_ITEM pItem = hb_vmEvalBlockOrMacro( pArea->pTagCurrent->pKeyItem );

         if( itmCompare( pItem, pKey, FALSE ) != 0 && ! bSoftSeek )
            SELF_GOTO( ( AREAP ) pArea, 0 );
      }
   }

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   else
      return SUCCESS;
}

#define  letoSkip                     NULL

static ERRCODE letoSkipFilter( LETOAREAP pArea, LONG lUpDown )
{
   HB_TRACE(HB_TR_DEBUG, ("letoSkipFilter(%p, %ld)", pArea, lUpDown));

   if( pArea->area.dbfi.itmCobExpr != NULL && !pArea->area.dbfi.fOptimized )
      return SUPER_SKIPFILTER( ( AREAP ) pArea, lUpDown );
   return SUCCESS;
}

static ERRCODE letoSkipRaw( LETOAREAP pArea, LONG lToSkip )
{
   char sData[50], * ptr;
   int iLenLen;
   ULONG ulDataLen, ulRecLen;
   LONG lu = 0;
   BYTE * ptrBuf = NULL;

   HB_TRACE(HB_TR_DEBUG, ("letoSkip(%p, %ld)", pArea, lToSkip));

   // sprintf( sData,"Skip-1 %lu %ld",pArea->ulRecNo,lToSkip );
   // leto_writelog( sData,0 );
   if( pArea->uiUpdated )
   {
      leto_PutRec( pArea );
      ptrBuf = pArea->ptrBuf;
      // leto_writelog("Skip-1A",0);
   }

   if( !lToSkip )
   {
      if( ptrBuf )
         pArea->ptrBuf = NULL;
      if( pArea->area.lpdbRelations )
         return SELF_SYNCCHILDREN( ( AREAP ) pArea );
      return SUCCESS;
   }
   else if( ( !pArea->fShared || ( ((ULONG)(hb_dateSeconds()*100))-pArea->ulDeciSec < BUFF_REFRESH_TIME ) ) && pArea->ptrBuf )
   {
      if( ( lToSkip > 0 && pArea->area.fEof ) || ( lToSkip < 0 && pArea->area.fBof ) )
         return SUCCESS;
      /*
      else if( lToSkip > 0 && pArea->area.fBof )
         lToSkip ++;
      */
      if( pArea->BufDirection == (lToSkip > 0 ? 1 : -1) )
      {
         // leto_writelog("letoSkip-2",0);
         while( lu < ( lToSkip > 0 ? lToSkip : -lToSkip ) )
         {
            // leto_writelog("letoSkip-2A",0);
            ulRecLen = (((int)*pArea->ptrBuf) & 0xFF) + ((((int)*(pArea->ptrBuf+1)) << 8) & 0xFF00) + ((((int)*(pArea->ptrBuf+2)) << 16) & 0xFF0000);
            if( ulRecLen >= 100000 )
            {
               commonError( pArea, EG_DATATYPE, 1020, 0, NULL, 0, NULL );
               return FAILURE;
            }
            if( !ulRecLen )
               break;
            pArea->ptrBuf += ulRecLen + 3;
            lu ++;
            // sprintf( sData,"check2: %lu", (ULONG)pArea->ptrBuf );
            // leto_writelog(sData,0);
            if( ((ULONG)(pArea->ptrBuf - pArea->pBuffer)) >= pArea->ulBufDataLen-1 )
            {
               lu = 0;
               break;
            }
         }
         pArea->uiRecInBuf += lu;
      }
      else if( pArea->uiRecInBuf >= ( lToSkip > 0 ? lToSkip : -lToSkip ) )
      {
         pArea->uiRecInBuf -= ( lToSkip > 0 ? lToSkip : -lToSkip );
         pArea->ptrBuf = pArea->pBuffer;
         // sprintf( sData,"letoSkip-3 %d %d", ( lToSkip > 0 ? lToSkip : -lToSkip ), pArea->uiRecInBuf );
         // leto_writelog(sData,0);
         while( lu < (LONG)pArea->uiRecInBuf )
         {
            // leto_writelog("letoSkip-3A",0);
            ulRecLen = (((int)*pArea->ptrBuf) & 0xFF) + ((((int)*(pArea->ptrBuf+1)) << 8) & 0xFF00) + ((((int)*(pArea->ptrBuf+2)) << 16) & 0xFF0000);
            if( ulRecLen >= 100000 )
            {
               commonError( pArea, EG_DATATYPE, 1020, 0, NULL, 0, NULL );
               return FAILURE;
            }
            if( !ulRecLen )
               break;
            pArea->ptrBuf += ulRecLen + 3;
            lu ++;
            // sprintf( sData,"check3: %lu", (ULONG)pArea->ptrBuf );
            // leto_writelog(sData,0);

            if( ((ULONG)(pArea->ptrBuf - pArea->pBuffer)) >= pArea->ulBufDataLen-1 )
            {
               lu = 0;
               break;
            }
         }
         if( lu == (LONG)pArea->uiRecInBuf )
            lu = ( lToSkip > 0 ? lToSkip : -lToSkip );
         // sprintf( sData,"letoSkip-3C %d", lu );
         // leto_writelog(sData,0);

      }
      if( lu == ( lToSkip > 0 ? lToSkip : -lToSkip ) )
      {
         // sprintf( sData,"letoSkip-4 %d", pArea->uiRecInBuf );
         // leto_writelog(sData,0);
         ulRecLen = (((int)*pArea->ptrBuf) & 0xFF) + ((((int)*(pArea->ptrBuf+1)) << 8) & 0xFF00) + ((((int)*(pArea->ptrBuf+2)) << 16) & 0xFF0000);
         if( ulRecLen )
         {
            // leto_writelog("letoSkip-4A",0);
            if( ptrBuf )
            {
               *ptrBuf = *(ptrBuf+1) = *(ptrBuf+2) = 0;
               // sprintf( sData,"nul: %lu", (ULONG)ptrBuf );
               // leto_writelog(sData,0);
            }
            leto_ParseRec( pArea, (char*) pArea->ptrBuf );

            if( pArea->area.lpdbRelations )
               return SELF_SYNCCHILDREN( ( AREAP ) pArea );
            else
               return SUCCESS;
         }
      }
   }

   // leto_writelog("letoSkip-5",0);
   sprintf( sData,"skip;%lu;%ld;%lu;%s;%c;\r\n", pArea->hTable, lToSkip,
      pArea->ulRecNo, (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
      (char)( (hb_setGetDeleted())? 0x41 : 0x40 ) );
   if ( !leto_SendRecv( pArea, sData, 0, 1021 ) ) return FAILURE;
   // sprintf( sData,"skip %lu", pArea->ulRecNo );
   // leto_writelog( sData,0 );

   if( ( iLenLen = (((int)*s_szBuffer) & 0xFF) ) >= 10 )
   {
      commonError( pArea, EG_DATATYPE, 1020, 0, NULL, 0, NULL );
      return FAILURE;
   }
   pArea->ulDeciSec = (ULONG) ( hb_dateSeconds() * 100 );
   pArea->BufDirection = (lToSkip > 0 ? 1 : -1);

   ulDataLen = leto_b2n( s_szBuffer+1, iLenLen );
   ptr = s_szBuffer + 2 + iLenLen;
   leto_ParseRec( pArea, ptr );

   if( ulDataLen > 1 ) 
   {
      if( pArea->ulBufLen && pArea->ulBufLen < ulDataLen )
      {
         hb_xfree( pArea->pBuffer );
         pArea->ulBufLen = 0;
      }
      if( !pArea->ulBufLen )
      {
         pArea->pBuffer = (BYTE *) hb_xgrab( ulDataLen );
         pArea->ulBufLen = ulDataLen;
      }
      pArea->ulBufDataLen = ulDataLen;
      memcpy( pArea->pBuffer, ptr, ulDataLen );
      pArea->ptrBuf = pArea->pBuffer;
   }
   else
      pArea->ptrBuf = NULL;
   pArea->uiRecInBuf = 0;
   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   else
      return SUCCESS;
}

static ERRCODE letoAddField( LETOAREAP pArea, LPDBFIELDINFO pFieldInfo )
{
   HB_TRACE(HB_TR_DEBUG, ("hb_dbfAddField(%p, %p)", pArea, pFieldInfo));

   /* Update field offset */
   pArea->pFieldOffset[ pArea->area.uiFieldCount ] = pArea->uiRecordLen;
   pArea->uiRecordLen += pFieldInfo->uiLen;
   return SUPER_ADDFIELD( ( AREAP ) pArea, pFieldInfo );
}

static ERRCODE letoAppend( LETOAREAP pArea, BOOL fUnLockAll )
{
   HB_TRACE(HB_TR_DEBUG, ("letoAppend(%p, %d)", pArea, (int) fUnLockAll));

   if( pArea->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }

   pArea->lpdbPendingRel = NULL;

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   pArea->uiUpdated = (fUnLockAll)? 9 : 1;
   memset( pArea->pFieldUpd, 0, sizeof(USHORT)*pArea->area.uiFieldCount );
   pArea->area.fBof = pArea->area.fEof = pArea->area.fFound = pArea->fDeleted = 0;
   pArea->ptrBuf = NULL;
   letoSetBlankRecord( pArea, TRUE );
   if( s_bFastAppend || leto_PutRec( pArea ) )
      pArea->fRecLocked = 1;

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   return SUCCESS;
}

#define  letoCreateFields          NULL

static ERRCODE letoDeleteRec( LETOAREAP pArea )
{
   // char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoDeleteRec(%p)", pArea));

   if( pArea->fShared && !pArea->fFLocked && !pArea->fRecLocked )
   {
      commonError( pArea, EG_UNLOCKED, EDBF_UNLOCKED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   else if( pArea->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }

//   sprintf( szData,"del;01;%d;%lu;\r\n", pArea->hTable, pArea->ulRecNo );
//   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   if( ( pArea->uiUpdated & 4 ) || !pArea->fDeleted )
   {
      pArea->fDeleted = 1;
      pArea->uiUpdated |= 4;
   }
   // leto_writelog("Delete",0);
   return SUCCESS;
}

static ERRCODE letoDeleted( LETOAREAP pArea, BOOL * pDeleted )
{
   HB_TRACE(HB_TR_DEBUG, ("letoDeleted(%p, %p)", pArea, pDeleted));

   *pDeleted = pArea->fDeleted;
   return SUCCESS;
}

#define  letoFieldCount            NULL
#define  letoFieldDisplay          NULL
#define  letoFieldInfo             NULL
#define  letoFieldName             NULL

static ERRCODE letoFlush( LETOAREAP pArea )
{
   char szData[32];
   HB_TRACE(HB_TR_DEBUG, ("letoFlush(%p)", pArea ));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   if( !bTransActive )
   {
      sprintf( szData,"flush;%lu;\r\n", pArea->hTable );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
   }

   return SUCCESS;
}

#define  letoGetRec                NULL

char * letoDecryptText( LETOCONNECTION * pConnection, ULONG * pulLen )
{
   char * ptr = s_szBuffer;
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

static ERRCODE letoGetMemoValue( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem, USHORT uiType )
{
   char szData[32], *ptr;
   ULONG ulLen;

   sprintf( szData,"memo;get;%lu;%lu;%d;\r\n", pArea->hTable, pArea->ulRecNo, uiIndex+1 );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) )
   {
      return FAILURE;
   }

   ptr = letoDecryptText( letoConnPool + pArea->uiConnection, &ulLen );
   if( ! ulLen)
      hb_itemPutC( pItem, "" );
   else
   {
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
     if( uiType == HB_FT_MEMO && pArea->area.cdPage != hb_cdp_page )
        hb_cdpnTranslate( ptr, pArea->area.cdPage, hb_cdp_page, ulLen-1 );
     hb_itemPutCL( pItem, ptr, ulLen-1 );
#else
     if( uiType == HB_FT_MEMO && pArea->area.cdPage != hb_cdp_page )
     {
        char * pBuff;
        HB_SIZE ulItemLen = ulLen-1;
        pBuff = hb_cdpnDup( ptr, &ulItemLen, pArea->area.cdPage, hb_cdp_page );
        hb_itemPutCL( pItem, pBuff, ulItemLen );
        hb_xfree( pBuff );
     }
     else
        hb_itemPutCL( pItem, ptr, ulLen-1 );
#endif
   }
   return SUCCESS;
}

static ERRCODE letoGetValue( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem )
{
   LPFIELD pField;

   HB_TRACE(HB_TR_DEBUG, ("letoGetValue(%p, %hu, %p)", pArea, uiIndex, pItem));

   if( !uiIndex || uiIndex > pArea->area.uiFieldCount )
   {
      return FAILURE;
   }
   if( pArea->lpdbPendingRel )
   {
      if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
         return FAILURE;
   }

   uiIndex--;
   pField = pArea->area.lpFields + uiIndex;

   switch( pField->uiType )
   {
      case HB_FT_STRING:
         if( pArea->area.cdPage != hb_cdp_page )
         {
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
            char * pVal = ( char * ) hb_xgrab( pField->uiLen + 1 );
            memcpy( pVal, pArea->pRecord + pArea->pFieldOffset[uiIndex], pField->uiLen );
            pVal[ pField->uiLen ] = '\0';
            hb_cdpnTranslate( pVal, pArea->area.cdPage, hb_cdp_page, pField->uiLen );
            hb_itemPutCL( pItem, pVal, pField->uiLen );
            hb_xfree( pVal );
#else
            char * pVal;
            HB_SIZE uiKeyLen = pField->uiLen;
            pVal = hb_cdpnDup( ( const char * ) pArea->pRecord + pArea->pFieldOffset[uiIndex], &uiKeyLen, pArea->area.cdPage, hb_cdp_page );
            hb_itemPutCLPtr( pItem, pVal, pField->uiLen );
#endif
         }
         else
            hb_itemPutCL( pItem, (char*) pArea->pRecord + pArea->pFieldOffset[uiIndex],
                       pField->uiLen );
         break;

      case HB_FT_LONG:
      case HB_FT_FLOAT:
      {
         HB_MAXINT lVal;
         double dVal;
         BOOL fDbl;

         fDbl = hb_strnToNum( (const char *) pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                              pField->uiLen, &lVal, &dVal );
         if( pField->uiDec )
            hb_itemPutNDLen( pItem, fDbl ? dVal : ( double ) lVal,
                             ( int ) ( pField->uiLen - pField->uiDec - 1 ),
                             ( int ) pField->uiDec );
         else if( fDbl )
            hb_itemPutNDLen( pItem, dVal, ( int ) pField->uiLen, 0 );
         else
            hb_itemPutNIntLen( pItem, lVal, ( int ) pField->uiLen );
         break;
      }
      case HB_FT_INTEGER:
      {
         switch( pField->uiLen )
         {
            case 2:
               hb_itemPutNILen( pItem, ( int ) HB_GET_LE_INT16( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ), 6 );
               break;
            case 4:
               hb_itemPutNIntLen( pItem, ( HB_LONG ) HB_GET_LE_INT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ), 10 );
               break;
            case 8:
#ifndef HB_LONG_LONG_OFF
               hb_itemPutNIntLen( pItem, ( HB_LONG ) HB_GET_LE_INT64( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ), 20 );
#else
               hb_itemPutNLen( pItem, ( double ) HB_GET_LE_INT64( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ), 20, 0 );
#endif
               break;
         }
         break;
      }
      case HB_FT_DOUBLE:
      {
         hb_itemPutNDLen( pItem, HB_GET_LE_DOUBLE( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ),
            20 - ( pField->uiDec > 0 ? ( pField->uiDec + 1 ) : 0 ),
            ( int ) pField->uiDec );
         break;
      }
      case HB_FT_CURRENCY:
      {
/* to do */
         break;
      }
      case HB_FT_DATETIME:
      case HB_FT_MODTIME:
      case HB_FT_DAYTIME:
      {
#ifdef __XHARBOUR__
         hb_itemPutDTL( pItem,
            HB_GET_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ),
            HB_GET_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] + 4 ) );
#else
   #if ( defined( HARBOUR_VER_AFTER_101 ) )
         hb_itemPutTDT( pItem,
            HB_GET_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ),
            HB_GET_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] + 4 ) );
/*
         hb_itemPutC( pItem, hb_timeStampStr( szBuffer,
            HB_GET_LE_INT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ),
            HB_GET_LE_INT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] + 4 ) ) );
*/
   #else
         char szBuffer[24];
         hb_itemPutC( pItem, hb_dateTimeStampStr( szBuffer,
            HB_GET_LE_INT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ),
            HB_GET_LE_INT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] + 4 ) ) );
   #endif
#endif
         break;
      }
      case HB_FT_DATE:
      {
         if( pField->uiLen == 3 )
         {
            hb_itemPutDL( pItem, HB_GET_LE_UINT24( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ) );
         }
         else if( pField->uiLen == 4 )
         {
            hb_itemPutDL( pItem, HB_GET_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ) );
         }
         else
         {
            char szBuffer[ 9 ];
            memcpy( szBuffer, pArea->pRecord + pArea->pFieldOffset[ uiIndex ], 8 );
            szBuffer[ 8 ] = 0;
            hb_itemPutDS( pItem, szBuffer );
            // hb_itemPutDS( pItem, ( char * ) pArea->pRecord + pArea->pFieldOffset[ uiIndex ] );
         }
         break;
      }
      case HB_FT_LOGICAL:
         hb_itemPutL( pItem, pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] == 'T' ||
                      pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] == 't' ||
                      pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] == 'Y' ||
                      pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] == 'y' );
         break;

      case HB_FT_MEMO:
      case HB_FT_BLOB:
      case HB_FT_PICTURE:
      case HB_FT_OLE:
      {
         if( pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] == ' ' )
            hb_itemPutC( pItem, "" );
         else
         {
            if( letoGetMemoValue( pArea, uiIndex, pItem, pField->uiType ) == FAILURE ) 
            {
               return FAILURE;
            }
         }
         hb_itemSetCMemo( pItem );
         break;
      }
      case HB_FT_ANY:
      {
         char * pData = ( char * ) pArea->pRecord + pArea->pFieldOffset[ uiIndex ];
         if( pField->uiLen == 3 )
         {
            hb_itemPutDL( pItem, HB_GET_LE_UINT24( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] ) );
         }
         else if( pField->uiLen == 4 )
         {
            hb_itemPutNL( pItem, HB_GET_LE_UINT32( pData ) );
         }
         else
         {
            switch( *pData++ )
            {
               case 'D':
               {
                  char szBuffer[ 9 ];
                  memcpy( szBuffer, pData, 8 );
                  szBuffer[ 8 ] = 0;
                  hb_itemPutDS( pItem, szBuffer );
                  break;
               }
               case 'L':
               {
                  hb_itemPutL( pItem, (*pData == 'T') );
                  break;
               }
               case 'N':
               {
/* to do */
                  break;
               }
               case 'C':
               {
                  USHORT uiLen = (USHORT) leto_b2n( pData, 2 );
                  hb_itemPutCL( pItem, (pData+2), uiLen );
                  break;
               }
               case '!':
               {
                  letoGetMemoValue( pArea, uiIndex, pItem, HB_FT_MEMO );
                  break;
               }
            }
         }
         break;
      }
   }
   return SUCCESS;
}

#define  letoGetVarLen             NULL
#define  letoGoCold                NULL
#define  letoGoCold                NULL
#define  letoGoHot                 NULL
#define  letoPutRec                NULL

static ERRCODE letoPutMemoValue( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem, USHORT uiType )
{
   char *szData, *ptr;
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
   ULONG ulLenMemo = hb_itemGetCLen( pItem );
#else
   HB_SIZE ulLenMemo = hb_itemGetCLen( pItem );
   char * pBuff;
#endif
   ULONG ulLen;

   if( pArea->uiUpdated && !bTransActive )
      leto_PutRec( pArea );

   pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] = (ulLenMemo)? '!' : ' ';
   ulLen = ulLenMemo;
   szData = (char*) hb_xgrab( ulLen + 36 );

   if( ( pArea->uiUpdated & 1 ) && bTransActive )
   {
      pArea->uiUpdated &= ( !1 );
      sprintf( szData+4,"memo;add;%lu;%lu;%d;", pArea->hTable, pArea->ulRecNo, uiIndex+1 );
   }
   else
      sprintf( szData+4,"memo;put;%lu;%lu;%d;", pArea->hTable, pArea->ulRecNo, uiIndex+1 );
   ptr = szData + 4 + strlen( szData+4 );
   ptr = leto_AddLen( ptr, &ulLen, 0 );
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
   memcpy( ptr, hb_itemGetCPtr( pItem ), ulLenMemo );
   *( ptr+ulLenMemo ) = '\0';

   if( uiType == HB_FT_MEMO && pArea->area.cdPage != hb_cdp_page )
      hb_cdpnTranslate( ptr, hb_cdp_page, pArea->area.cdPage, ulLenMemo );
#else
   if( uiType == HB_FT_MEMO && pArea->area.cdPage != hb_cdp_page )
   {
      pBuff = hb_cdpnDup( hb_itemGetCPtr( pItem ), &ulLenMemo, hb_cdp_page, pArea->area.cdPage );
      memcpy( ptr, pBuff, ulLenMemo );
      hb_xfree( pBuff );
   }
   else
      memcpy( ptr, hb_itemGetCPtr( pItem ), ulLenMemo );
   *( ptr+ulLenMemo ) = '\0';
#endif
   ulLen = ptr + ulLenMemo - ( szData + 4 );
   ptr = leto_AddLen( (szData+4), &ulLen, 1 );

   if( bTransActive )
   {
      if( !szTransBuffer )
      {
         ulTransBuffLen = (ulLen<256)? 1024 : ulLen*3;
         szTransBuffer = (BYTE*) hb_xgrab( ulTransBuffLen );
         memcpy( szTransBuffer+4, "ta;", 3 );
      }
      if( !ulTransDataLen )
         ulTransDataLen = 10;
      if( ulTransBuffLen - ulTransDataLen <= ulLen )
      {
         ulTransBuffLen = ulTransDataLen + ( (ulLen<256)? 1024 : ulLen*3 );
         szTransBuffer = (BYTE*) hb_xrealloc( szTransBuffer, ulTransBuffLen );
      }
      memcpy( szTransBuffer+ulTransDataLen, ptr, ulLen );
      ulTransDataLen += ulLen;
      uiRecsInTrans ++;
   }
   else
   {
      if ( !leto_SendRecv( pArea, ptr, ulLen, 1021 ) )
      {
         hb_xfree( szData );
         return FAILURE;
      }
   }
   hb_xfree( szData );
   return SUCCESS;
}

static ERRCODE letoPutValue( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem )
{
   LPFIELD pField;
   char szBuf[256];
   BOOL bTypeError = 0;
#if !defined (__XHARBOUR__) && defined(__HARBOUR__) && ( (__HARBOUR__ - 0) >= 0x020000 )
   char * pBuff;
#endif

   HB_TRACE(HB_TR_DEBUG, ("letoPutValue(%p, %hu, %p)", pArea, uiIndex, pItem));

   if( !uiIndex || uiIndex > pArea->area.uiFieldCount )
      return FAILURE;

   if( pArea->fShared && !pArea->fFLocked && !pArea->fRecLocked )
   {
      commonError( pArea, EG_UNLOCKED, EDBF_UNLOCKED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   else if( pArea->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }

   uiIndex --;
   pField = pArea->area.lpFields + uiIndex;

   switch( pField->uiType )
   {
      case HB_FT_STRING:
         if( HB_IS_MEMO( pItem ) || HB_IS_STRING( pItem ) )
         {
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
            USHORT uiSize = ( USHORT ) hb_itemGetCLen( pItem );
            if( uiSize > pField->uiLen )
               uiSize = pField->uiLen;
            memcpy( pArea->pRecord + pArea->pFieldOffset[uiIndex],
                    hb_itemGetCPtr( pItem ), uiSize );
            if( pArea->area.cdPage != hb_cdp_page )
               hb_cdpnTranslate( (char *) pArea->pRecord + pArea->pFieldOffset[ uiIndex ], hb_cdp_page, pArea->area.cdPage, uiSize );
            memset( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] + uiSize,
                    ' ', pField->uiLen - uiSize );
#else
            HB_SIZE ulLen1 = hb_itemGetCLen( pItem );
            if( pArea->area.cdPage != hb_cdp_page )
            {
               pBuff = hb_cdpnDup( hb_itemGetCPtr( pItem ), &ulLen1, hb_cdp_page, pArea->area.cdPage );
               if( ulLen1 > (HB_SIZE)pField->uiLen )
                  ulLen1 = pField->uiLen;
               memcpy( pArea->pRecord + pArea->pFieldOffset[uiIndex], pBuff, ulLen1 );
               hb_xfree( pBuff );
            }
            else
            {
               if( ulLen1 > (HB_SIZE)pField->uiLen )
                  ulLen1 = pField->uiLen;
               memcpy( pArea->pRecord + pArea->pFieldOffset[uiIndex],
                      hb_itemGetCPtr( pItem ), ulLen1 );
            }
            memset( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] + ulLen1,
                    ' ', pField->uiLen - ulLen1 );
#endif
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_LONG:
      case HB_FT_FLOAT:
         if( HB_IS_NUMBER( pItem ) )
         {
            if( hb_itemStrBuf( szBuf, pItem, pField->uiLen, pField->uiDec ) )
            {
               memcpy( pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                       szBuf, pField->uiLen );
            }
            else
            {
               memset( pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                       '*', pField->uiLen );
               commonError( pArea, EG_DATAWIDTH, EDBF_DATAWIDTH, 0, NULL, 0, NULL );
               return FAILURE;
            }
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_INTEGER:
         if( HB_IS_NUMBER( pItem ) )
         {
            HB_LONG lVal;
            double dVal;
            int iSize;

            if( pField->uiDec )
            {
               dVal = hb_numDecConv( hb_itemGetND( pItem ), - ( int ) pField->uiDec );
               lVal = ( HB_LONG ) dVal;
               if( ! HB_DBL_LIM_INT64( dVal ) )
                  iSize = 99;
               else
#ifndef HB_LONG_LONG_OFF
                  iSize = HB_LIM_INT8( lVal ) ? 1 :
                        ( HB_LIM_INT16( lVal ) ? 2 :
                        ( HB_LIM_INT24( lVal ) ? 3 :
                        ( HB_LIM_INT32( lVal ) ? 4 : 8 ) ) );
#else
                  iSize = HB_DBL_LIM_INT8( dVal ) ? 1 :
                        ( HB_DBL_LIM_INT16( dVal ) ? 2 :
                        ( HB_DBL_LIM_INT24( dVal ) ? 3 :
                        ( HB_DBL_LIM_INT32( dVal ) ? 4 : 8 ) ) );
#endif
            }
            else if( HB_IS_DOUBLE( pItem ) )
            {
               dVal = hb_itemGetND( pItem );
               lVal = ( HB_LONG ) dVal;
               if( ! HB_DBL_LIM_INT64( dVal ) )
                  iSize = 99;
               else
#ifndef HB_LONG_LONG_OFF
                  iSize = HB_LIM_INT8( lVal ) ? 1 :
                        ( HB_LIM_INT16( lVal ) ? 2 :
                        ( HB_LIM_INT24( lVal ) ? 3 :
                        ( HB_LIM_INT32( lVal ) ? 4 : 8 ) ) );
#else
                  iSize = HB_DBL_LIM_INT8( dVal ) ? 1 :
                        ( HB_DBL_LIM_INT16( dVal ) ? 2 :
                        ( HB_DBL_LIM_INT24( dVal ) ? 3 :
                        ( HB_DBL_LIM_INT32( dVal ) ? 4 : 8 ) ) );
#endif
            }
            else
            {
               lVal = ( HB_LONG ) hb_itemGetNInt( pItem );
#ifdef HB_LONG_LONG_OFF
               dVal = ( double ) lVal;
#endif
               iSize = HB_LIM_INT8( lVal ) ? 1 :
                     ( HB_LIM_INT16( lVal ) ? 2 :
                     ( HB_LIM_INT24( lVal ) ? 3 :
                     ( HB_LIM_INT32( lVal ) ? 4 : 8 ) ) );
            }

            if( iSize > pField->uiLen )
            {
               memset( pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                       '*', pField->uiLen );
               commonError( pArea, EG_DATAWIDTH, EDBF_DATAWIDTH, 0, NULL, 0, NULL );
               return FAILURE;
            }
            else
            {
               switch( pField->uiLen )
               {
                  case 1:
                     pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] = ( signed char ) lVal;
                     break;
                  case 2:
                     HB_PUT_LE_UINT16( pArea->pRecord + pArea->pFieldOffset[ uiIndex ], ( HB_U16 ) lVal );
                     break;
                  case 3:
                     HB_PUT_LE_UINT24( pArea->pRecord + pArea->pFieldOffset[ uiIndex ], ( HB_U32 ) lVal );
                     break;
                  case 4:
                     HB_PUT_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ], ( HB_U32 ) lVal );
                     break;
                  case 8:
#ifndef HB_LONG_LONG_OFF
                     HB_PUT_LE_UINT64( pArea->pRecord + pArea->pFieldOffset[ uiIndex ], ( UINT64 ) lVal );
#else
                     HB_PUT_LE_UINT64( pArea->pRecord + pArea->pFieldOffset[ uiIndex ], dVal );
#endif
                     break;
                  default:
                     bTypeError = 1;
                     break;
               }
            }
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_DOUBLE:
         if( HB_IS_NUMBER( pItem ) )
         {
            HB_PUT_LE_DOUBLE( pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                              hb_itemGetND( pItem ) );
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_DATE:
         if( HB_IS_DATE( pItem ) )
         {
            if( pField->uiLen == 3 )
            {
               HB_PUT_LE_UINT24( pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                                 hb_itemGetDL( pItem ) );
            }
            else if( pField->uiLen == 4 )
            {
               HB_PUT_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                                 hb_itemGetDL( pItem ) );
            }
            else
            {
               hb_itemGetDS( pItem, szBuf );
               memcpy( pArea->pRecord + pArea->pFieldOffset[ uiIndex ], szBuf, 8 );
            }
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_DAYTIME:
      case HB_FT_MODTIME:
#ifdef __XHARBOUR__
         if( HB_IS_DATE( pItem ) )
         {
            HB_PUT_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ],
                              hb_itemGetDL( pItem ) );
            HB_PUT_LE_UINT32( pArea->pRecord + pArea->pFieldOffset[ uiIndex ] + 4,
                              hb_itemGetT( pItem ) );
         }
#else
         if( HB_IS_STRING( pItem ) )
         {
            LONG lJulian, lMillisec;
            BYTE * ptr = pArea->pRecord + pArea->pFieldOffset[ uiIndex ];
    #if ( defined( HARBOUR_VER_AFTER_101 ) )
            hb_timeStampStrGetDT( hb_itemGetCPtr( pItem ), &lJulian, &lMillisec );
    #else
            hb_dateTimeStampStrGet( hb_itemGetCPtr( pItem ), &lJulian, &lMillisec );
    #endif
            HB_PUT_LE_UINT32( ptr, lJulian );
            ptr += 4;
            HB_PUT_LE_UINT32( ptr, lMillisec );
         }
#endif
         else
            bTypeError = 1;
         break;

      case HB_FT_LOGICAL:
         if( HB_IS_LOGICAL( pItem ) )
         {
            pArea->pRecord[ pArea->pFieldOffset[ uiIndex ] ] = hb_itemGetL( pItem ) ? 'T' : 'F';
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_MEMO:
      case HB_FT_BLOB:
      case HB_FT_PICTURE:
      case HB_FT_OLE:
         if( HB_IS_MEMO( pItem ) || HB_IS_STRING( pItem ) )
         {
            return letoPutMemoValue( pArea, uiIndex, pItem, pField->uiType );
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_ANY:
      {
         char * pData = ( char * ) pArea->pRecord + pArea->pFieldOffset[ uiIndex ];
         if( pField->uiLen == 3 )
         {
            if( HB_IS_DATE( pItem ) )
            {
               HB_PUT_LE_UINT24( pData, hb_itemGetDL( pItem ) );
            }
            else
            {
               bTypeError = 1;
            }
         }
         else if( pField->uiLen == 4 )
         {
            if( HB_IS_NUMBER( pItem ) )
            {
               HB_PUT_LE_UINT32( pData, hb_itemGetNL( pItem ) );
            }
            else
            {
               bTypeError = 1;
            }
         }
         else if( HB_IS_DATE( pItem ) )
         {
            hb_itemGetDS( pItem, szBuf );
            *pData++ = 'D';
            memcpy( pData, szBuf, 8 );
         }
         else if( HB_IS_LOGICAL( pItem ) )
         {
            *pData++ = 'L';
            *pData = hb_itemGetL( pItem ) ? 'T' : 'F';
         }
         else if( HB_IS_NUMBER( pItem ) )
         {
            *pData = 'N';
         }
         else if( HB_IS_STRING( pItem ) )
         {
            ULONG ulLen = hb_itemGetCLen( pItem );
            if( ulLen <= (ULONG) pField->uiLen - 3)
            {
               *pData++ = 'C';
               *pData++ = (char) ulLen & 0xFF;
               *pData++ = (char) (ulLen >> 8) & 0xFF;
               if( pArea->area.cdPage != hb_cdp_page )
               {
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
                  memcpy( pData, hb_itemGetCPtr( pItem ), ulLen );
                  hb_cdpnTranslate( pData, hb_cdp_page, pArea->area.cdPage, ulLen );
#else
                  HB_SIZE ulLen1 = ulLen;
                  pBuff = hb_cdpnDup( hb_itemGetCPtr( pItem ), &ulLen1, hb_cdp_page, pArea->area.cdPage );
                  memcpy( pData, pBuff, ulLen1 );
                  hb_xfree( pBuff );
#endif
               }
               else
                  memcpy( pData, hb_itemGetCPtr( pItem ), ulLen );
            }
            else
            {
               *pData = '!';
               return letoPutMemoValue( pArea, uiIndex, pItem, HB_FT_MEMO );
            }
         }
         break;
      }
   }

   if( bTypeError )
   {
      commonError( pArea, EG_DATATYPE, 1020, 0, NULL, 0, NULL );
      return FAILURE;
   }

   pArea->uiUpdated |= 2;
   *(pArea->pFieldUpd+uiIndex) = 1;

   return SUCCESS;
}

static ERRCODE letoRecall( LETOAREAP pArea )
{
   // char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoRecall(%p)", pArea));

   if( pArea->fShared && !pArea->fFLocked && !pArea->fRecLocked )
   {
      commonError( pArea, EG_UNLOCKED, EDBF_UNLOCKED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   else if( pArea->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }

//   sprintf( szData,"del;02;%d;%lu;\r\n", pArea->hTable, pArea->ulRecNo );
//   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   if( ( pArea->uiUpdated & 4 ) || pArea->fDeleted )
   {
      pArea->fDeleted = 0;
      pArea->uiUpdated |= 4;
   }
   return SUCCESS;
}

static ERRCODE letoRecCount( LETOAREAP pArea, ULONG * pRecCount )
{
   char szData[32], * ptr;
   HB_TRACE(HB_TR_DEBUG, ("letoRecCount(%p, %p)", pArea, pRecCount));

   sprintf( szData,"rcou;%lu;\r\n", pArea->hTable );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   ptr = leto_firstchar();
   /*
   leto_getCmdItem( &ptr, szData ); ptr ++;
   sscanf( szData, "%lu", pRecCount );
   */
   sscanf( ptr, "%lu;", pRecCount );

   return SUCCESS;
}

static ERRCODE letoRecInfo( LETOAREAP pArea, PHB_ITEM pRecID, USHORT uiInfoType, PHB_ITEM pInfo )
{
   ULONG ulRecNo = hb_itemGetNL( pRecID );
   ERRCODE uiRetVal = SUCCESS;

   HB_TRACE(HB_TR_DEBUG, ("letoRecInfo(%p, %p, %hu, %p)", pArea, pRecID, uiInfoType, pInfo));

   switch( uiInfoType )
   {
      case DBRI_DELETED:
      {
         hb_itemPutL( pInfo, pArea->fDeleted );
         break;
      }
      case DBRI_LOCKED:
      {
         if( ulRecNo != 0 && ulRecNo != pArea->ulRecNo )
         {
            if( !pArea->fShared || pArea->fFLocked )
            {
               hb_itemPutL( pInfo, TRUE );
            }
            else
            {
               char szData[32], * ptr;

               sprintf( szData,"islock;%lu;%lu;\r\n", pArea->hTable, ulRecNo );
               if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
               if( *s_szBuffer == '-' )
               {
                  if( *(s_szBuffer+3) != '4' )
                     commonError( pArea, EG_DATATYPE, 1021, 0, s_szBuffer, 0, NULL );
                  return FAILURE;
               }
               ptr = leto_firstchar();
               leto_getCmdItem( &ptr, szData ); ptr ++;
               hb_itemPutL( pInfo, szData[0] == 'T' );
            }
         }
         else
         {
            hb_itemPutL( pInfo, pArea->fRecLocked );
         }
         break;
      }
      case DBRI_RECSIZE:
         hb_itemPutNL( pInfo, pArea->uiRecordLen );
         break;

      case DBRI_RECNO:
         if( ulRecNo == 0 )
            uiRetVal = SELF_RECNO( ( AREAP ) pArea, &ulRecNo );
         hb_itemPutNL( pInfo, ulRecNo );
         break;

      case DBRI_UPDATED:
         /* TODO: this will not work properly with current ADS RDD */
         hb_itemPutL( pInfo, FALSE );
         break;

      default:
         return SUPER_RECINFO( ( AREAP ) pArea, pRecID, uiInfoType, pInfo );
   }
   return uiRetVal;
}


static ERRCODE letoRecNo( LETOAREAP pArea, ULONG * ulRecNo )
{
   HB_TRACE(HB_TR_DEBUG, ("letoRecNo(%p, %p)", pArea, ulRecNo));

   if( pArea->uiUpdated & 1 )
      leto_PutRec( pArea );

   if( pArea->lpdbPendingRel )
   {
      if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
         return FAILURE;
   }

   *ulRecNo = pArea->ulRecNo;

   return SUCCESS;
}

static ERRCODE letoRecId( LETOAREAP pArea, PHB_ITEM pRecNo )
{
   ERRCODE errCode;
   ULONG ulRecNo;

   HB_TRACE(HB_TR_DEBUG, ("letoRecId(%p, %p)", pArea, pRecNo));

   errCode = SELF_RECNO( ( AREAP ) pArea, &ulRecNo );
   hb_itemPutNL( pRecNo, ulRecNo );
   return errCode;
}

static ERRCODE letoSetFieldExtent( LETOAREAP pArea, USHORT uiFieldExtent )
{
   HB_TRACE(HB_TR_DEBUG, ("hb_letoSetFieldExtent(%p, %hu)", pArea, uiFieldExtent));

   if( SUPER_SETFIELDEXTENT( ( AREAP ) pArea, uiFieldExtent ) == FAILURE )
      return FAILURE;

   /* Alloc field offsets array */
   if( uiFieldExtent )
   {
      pArea->pFieldOffset = ( USHORT * ) hb_xgrab( uiFieldExtent * sizeof( USHORT ) );
      memset( pArea->pFieldOffset, 0, uiFieldExtent * sizeof( USHORT ) );
      pArea->pFieldUpd = ( USHORT * ) hb_xgrab( uiFieldExtent * sizeof( USHORT ) );
      memset( pArea->pFieldUpd, 0, uiFieldExtent * sizeof( USHORT ) );
   }

   return SUCCESS;
}

#define  letoAlias                 NULL

static ERRCODE letoClose( LETOAREAP pArea )
{
   char szData[32];
   LETOCONNECTION * pConnection = letoConnPool + pArea->uiConnection;
   HB_TRACE(HB_TR_DEBUG, ("letoClose(%p)", pArea));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   if( bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      return FAILURE;
   }

   pArea->lpdbPendingRel = NULL;

   SUPER_CLOSE( ( AREAP ) pArea );

   if( pArea->hTable )
   {
      if( !pConnection->bCloseAll )
      {
         sprintf( szData,"close;01;%lu;\r\n", pArea->hTable );
         if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
      }
      pArea->hTable = 0;
   }
   /* Free field offset array */
   if( pArea->pFieldOffset )
   {
      hb_xfree( pArea->pFieldOffset );
      pArea->pFieldOffset = NULL;
   }
   if( pArea->pFieldUpd )
   {
      hb_xfree( pArea->pFieldUpd );
      pArea->pFieldUpd = NULL;
   }
   /* Free buffer */
   if( pArea->pRecord )
   {
      hb_xfree( pArea->pRecord );
      pArea->pRecord = NULL;
   }
   if( pArea->pBuffer )
   {
      hb_xfree( pArea->pBuffer );
      pArea->pBuffer = NULL;
   }
   /* Free all filenames */
   if( pArea->szDataFileName )
   {
      hb_xfree( pArea->szDataFileName );
      pArea->szDataFileName = NULL;
   }
   if( pArea->pTagInfo )
   {
      LETOTAGINFO * pTagInfo = pArea->pTagInfo, * pTagNext;
      do
      {
         if( pTagInfo->BagName )
            hb_xfree( pTagInfo->BagName );
         if( pTagInfo->TagName )
            hb_xfree( pTagInfo->TagName );
         if( pTagInfo->KeyExpr )
            hb_xfree( pTagInfo->KeyExpr );
         if( pTagInfo->pKeyItem != NULL )
            hb_vmDestroyBlockOrMacro( pTagInfo->pKeyItem );
         if( pTagInfo->ForExpr )
            hb_xfree( pTagInfo->ForExpr );
         if( pTagInfo->pTopScope )
            hb_itemRelease( pTagInfo->pTopScope );
         if( pTagInfo->pBottomScope )
            hb_itemRelease( pTagInfo->pBottomScope );
         pTagNext = pTagInfo->pNext;
         hb_xfree( pTagInfo );
         pTagInfo = pTagNext;
      }
      while( pTagInfo );
      pArea->pTagInfo = NULL;
   }
   if( pArea->pLocksPos )
   {
      hb_xfree( pArea->pLocksPos );
      pArea->pLocksPos  = NULL;
      pArea->ulLocksMax = pArea->ulLocksAlloc = 0;
   }
   return SUCCESS;
}

const char * letoRemoveIpFromPath(const char * szPath)
{
   const char * ptr;
   if( strlen(szPath) > 2 && szPath[0]=='/'  && szPath[1]=='/' && (ptr=strchr(szPath+2, '/')) != NULL )
   {
      szPath = ptr + 1;
   }
   return szPath;
}

static LETOCONNECTION * letoOpenConnection( LETOAREAP pArea, LPDBOPENINFO pOpenInfo, char * szFile, BOOL bCreate)
{
   LETOCONNECTION * pConnection;
   char szAddr[96], * ptr;
   int iPort;
   unsigned int uiLen;

   if( pOpenInfo->ulConnection > 0 && pOpenInfo->ulConnection <= (ULONG) uiConnCount )
   {
      if( letoConnPool[pOpenInfo->ulConnection-1].pAddr )
      {
         const char * szPath = letoRemoveIpFromPath( pOpenInfo->abName );
         pConnection = letoConnPool + pOpenInfo->ulConnection - 1;
         if( ( ptr = strrchr( szPath, '/') ) != NULL ||
             ( ptr = strrchr( szPath, '\\') ) != NULL )
         {
            memcpy( szFile, szPath, ptr - szPath );
         }
         else if( pConnection->szPath )
         {
            strcpy(szFile, pConnection->szPath );
         }
         else
         {
            strcpy(szFile, (bCreate ? hb_setGetDefault() : hb_setGetPath() ) );
         }
         uiLen = strlen(szFile);
         if( szFile[uiLen] != '/' && szFile[uiLen] != '\\' )
         {
            szFile[uiLen] = '/';
            szFile[uiLen+1] = '\0';
         }
      }
      else
         pConnection = NULL;
   }
   else
   {
      if( !leto_getIpFromPath( pOpenInfo->abName, szAddr, &iPort, szFile, TRUE ) &&
          !leto_getIpFromPath( (bCreate ? hb_setGetDefault() : hb_setGetPath() ), szAddr, &iPort, szFile, FALSE ) )
      {
         if( pCurrentConn )
         {
            ptr = strrchr( pOpenInfo->abName, '/');
            if( ptr == NULL )
               ptr = strrchr( pOpenInfo->abName, '\\');
            if( ptr )
            {
               uiLen = ptr - (char *) pOpenInfo->abName + 1;
               memcpy( szFile, pOpenInfo->abName, uiLen );
               szFile[uiLen] = '\0';
            }
            else if( pCurrentConn->szPath )
            {
               ptr = strchr( pCurrentConn->szPath, ',');
               if( ptr == NULL )
                  ptr = strchr( pCurrentConn->szPath, ';');
               if( ptr == NULL )
                  ptr = pCurrentConn->szPath + strlen( pCurrentConn->szPath );
               uiLen = ptr - pCurrentConn->szPath;
               memcpy( szFile, pCurrentConn->szPath, uiLen );
               if( szFile[uiLen] != '/' && szFile[uiLen] != '\\' )
               {
                  szFile[uiLen] = '/';
                  szFile[uiLen+1] = '\0';
               }
            }
            else
            {
               szFile[0] = '\0';
            }
            return pCurrentConn;
         }
         else
         {
            commonError( pArea, EG_OPEN, (bCreate ? 1 : 101), 0, ( char * ) pOpenInfo->abName, 0, NULL );
            return NULL;
         }
      }
      if( ( pConnection = leto_ConnectionFind( szAddr, iPort ) ) == NULL &&
          ( pConnection = leto_ConnectionNew( szAddr, iPort, NULL, NULL, 0 ) ) == NULL )
      {
         commonError( pArea, EG_OPEN, (bCreate ? 1 : 102), 0, ( char * ) pOpenInfo->abName, 0, NULL );
         return NULL;
      }
   }
   return pConnection;
}

static ERRCODE letoCreate( LETOAREAP pArea, LPDBOPENINFO pCreateInfo )
{
   LETOCONNECTION * pConnection;
   LPFIELD pField;
   USHORT uiCount;
   unsigned int uiLen;
   char *szData;
   char szFile[_POSIX_PATH_MAX + 1], szTemp[14];
   char * ptr;
   char cType;

   HB_TRACE(HB_TR_DEBUG, ("letoCreate(%p, %p)", pArea, pCreateInfo));

   if( ( pConnection = letoOpenConnection( pArea, pCreateInfo, szFile, TRUE ) ) == NULL )
      return FAILURE;

   pArea->uiConnection = pConnection - letoConnPool;
   uiLen = strlen( szFile );
   if( szFile[uiLen-1] != '/' && szFile[uiLen-1] != '\\' )
   {
      szFile[uiLen++] = szFile[0];
      szFile[uiLen] = '\0';
   }
   leto_getFileFromPath( pCreateInfo->abName, szFile+uiLen );

   uiLen = (unsigned int) pArea->area.uiFieldCount * 24 + 10 + _POSIX_PATH_MAX;
   szData = (char *) hb_xgrab( uiLen );
   ptr = szData;
   sprintf( szData,"creat;01;%d;%s;%s;%d;", pCreateInfo->uiArea, szFile, pCreateInfo->atomAlias, pArea->area.uiFieldCount );
   ptr += strlen( szData );

   pField = pArea->area.lpFields;
   for( uiCount = 0; uiCount < pArea->area.uiFieldCount; uiCount++ )
   {
      switch( pField->uiType )
      {
         case HB_FT_DATE:
            cType = 'D';
            break;
         case HB_FT_STRING:
            cType = 'C';
            break;
         case HB_FT_MEMO:
            cType = 'M';
            break;
         case HB_FT_BLOB:
            cType = 'W';
            break;
         case HB_FT_PICTURE:
            cType = 'P';
            break;
         case HB_FT_OLE:
            cType = 'G';
            break;
         case HB_FT_LOGICAL:
            cType = 'L';
            break;
         case HB_FT_LONG:
            cType = 'N';
            break;
         case HB_FT_FLOAT:
            cType = 'F';
            break;
         case HB_FT_INTEGER:
            if( pField->uiLen == 2 )
               cType = '2';
            else
               cType = 'I';
            break;
         case HB_FT_DOUBLE:
            cType = '8';
            break;
         case HB_FT_MODTIME:
            cType = '@';
            break;
         case HB_FT_DAYTIME:
            cType = 'T';
            break;
         case HB_FT_ANY:
            cType = 'V';
            break;
         default:
            return FAILURE;
      }
      sprintf( ptr,"%s;%c;%d;%d;", hb_dynsymName((PHB_DYNS)pField->sym), cType, pField->uiLen, pField->uiDec );
      ptr = szData + strlen( szData );

      pField++;
   }
   sprintf( ptr,"\r\n" );

   if ( !leto_SendRecv( pArea, szData, 0, 0 ) )
   {
      hb_xfree( szData );
      return FAILURE;
   }

   hb_xfree( szData );
   if( leto_checkerror( pArea ) )
   {
      return FAILURE;
   }

   pArea->pRecord = ( BYTE * ) hb_xgrab( pArea->uiRecordLen+1 );
   if( pCreateInfo->cdpId )
   {
      pArea->area.cdPage = hb_cdpFind( (char *) pCreateInfo->cdpId );
      if( !pArea->area.cdPage )
         pArea->area.cdPage = hb_cdp_page;
   }
   else
      pArea->area.cdPage = hb_cdp_page;

   pArea->pLocksPos      = NULL;
   pArea->ulLocksMax = pArea->ulLocksAlloc = 0;

   ptr = leto_firstchar();
   leto_getCmdItem( &ptr, szTemp ); ptr ++;
   sscanf( szTemp, "%lu" , &(pArea->hTable) );
   if( *ptr == '1' )
     pArea->uiDriver = 1;
   ptr ++; ptr++;

   leto_ParseRec( pArea, ptr );

   return SUPER_CREATE( ( AREAP ) pArea, pCreateInfo );
}

static ERRCODE letoInfo( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem )
{
   HB_TRACE(HB_TR_DEBUG, ("letoInfo(%p, %hu, %p)", pArea, uiIndex, pItem));

   switch( uiIndex )
   {
      case DBI_ISDBF:
      case DBI_CANPUTREC:
         hb_itemPutL( pItem, TRUE );
         break;

      case DBI_GETHEADERSIZE:
         hb_itemPutNL( pItem, 32 + pArea->area.uiFieldCount * 32 + 2 );
         break;

      case DBI_GETRECSIZE:
         hb_itemPutNL( pItem, pArea->uiRecordLen );
         break;

      case DBI_GETLOCKARRAY:
      {
         ULONG ulCount;

         hb_arrayNew( pItem, pArea->ulLocksMax );
         for( ulCount = 0; ulCount < pArea->ulLocksMax; ulCount++ )
         {
            hb_arraySetNL( pItem, ulCount + 1, pArea->pLocksPos[ ulCount ] );
         }
         break;
      }

      case DBI_TABLEEXT:
         hb_itemPutC( pItem, "dbf" );
         break;

      case DBI_FULLPATH :
         hb_itemPutCL( pItem, pArea->szDataFileName, strlen(pArea->szDataFileName) );
         break;

      case DBI_ISFLOCK:
         hb_itemPutL( pItem, pArea->fFLocked );
         break;

      case DBI_ISREADONLY:
         hb_itemPutL( pItem, pArea->fReadonly );
         break;

      case DBI_SHARED:
         hb_itemPutL( pItem, pArea->fShared );
         break;

      case DBI_MEMOEXT:
         hb_itemPutC( pItem, "fpt" );
         break;

      case DBI_DB_VERSION     :   /* HOST driver Version */
      {
         hb_itemPutC( pItem, letoConnPool[pArea->uiConnection].szVersion );
         break;
      }

      case DBI_RDD_VERSION    :   /* RDD version (current RDD) */
         hb_itemPutC( pItem, HB_RDD_LETO_VERSION_STRING );
         break;

      default:
         return SUPER_INFO( ( AREAP ) pArea, uiIndex, pItem );
   }
   return SUCCESS;
}

static ERRCODE letoNewArea( LETOAREAP pArea )
{
   ERRCODE errCode;

   HB_TRACE(HB_TR_DEBUG, ("letoNewArea(%p)", pArea));

   errCode = SUPER_NEW( ( AREAP ) pArea );
   if( errCode == SUCCESS )
   {
   }
   return errCode;
}

static ERRCODE letoOpen( LETOAREAP pArea, LPDBOPENINFO pOpenInfo )
{
   LETOCONNECTION * pConnection;
   unsigned int uiFields, uiLen;
   USHORT uiCount;
   DBFIELDINFO dbFieldInfo;
   char szData[_POSIX_PATH_MAX + 16];
   char szAlias[HB_RDD_MAX_ALIAS_LEN + 1];
   char szFile[_POSIX_PATH_MAX + 1], szName[14], szTemp[14], * ptr;
   char cType;

   HB_TRACE(HB_TR_DEBUG, ("letoOpen(%p)", pArea));

   if( !pOpenInfo->atomAlias )
   {
      leto_getFileFromPath( pOpenInfo->abName, szFile );
      ptr = szFile + strlen(szFile) - 1;
      while( ptr > szFile && *ptr != '.' ) ptr --;
      if( *ptr == '.' )
         *ptr = '\0';
      hb_strncpyUpperTrim( szAlias, szFile, HB_RDD_MAX_ALIAS_LEN );
      pOpenInfo->atomAlias = szAlias;
   }

   if( ( pConnection = letoOpenConnection( pArea, pOpenInfo, szFile, FALSE ) ) == NULL )
      return FAILURE;

   pArea->uiConnection = pConnection - letoConnPool;
   uiLen = strlen( szFile );
   if( szFile[uiLen-1] != '/' && szFile[uiLen-1] != '\\' )
   {
      szFile[uiLen++] = szFile[0];
      szFile[uiLen] = '\0';
   }
   leto_getFileFromPath( pOpenInfo->abName, szFile+uiLen );

   sprintf( szData,"open;01;%d;%s;%s;%c%c;%s;\r\n", pOpenInfo->uiArea, szFile,
       pOpenInfo->atomAlias,
       (pOpenInfo->fShared)? 'T':'F',(pOpenInfo->fReadonly)? 'T':'F',
       (pOpenInfo->cdpId)? (char*)pOpenInfo->cdpId:"" );
   if ( !leto_SendRecv( pArea, szData, 0, 0 ) )
      return FAILURE;

   if( *s_szBuffer == '-' )
   {
      if( *(s_szBuffer+3) == '4' )
         commonError( pArea, EG_OPEN, 103, 32, s_szBuffer, EF_CANDEFAULT, NULL );
      else
         commonError( pArea, EG_DATATYPE, 1021, 0, s_szBuffer, 0, NULL );
      return FAILURE;
   }

   ptr = leto_firstchar();
   // leto_writelog(ptr,0);
   // Read table ID
   leto_getCmdItem( &ptr, szTemp ); ptr ++;
   sscanf( szTemp, "%lu" , &(pArea->hTable) );
   /*
   {
      char s[100];
      sprintf(s,"%s %lu",szFile,pArea->hTable );
      leto_writelog(s,0);
   }
   */

   pArea->szDataFileName = hb_strdup( (char *) ( pOpenInfo->abName ) );
   hb_strLower( pArea->szDataFileName, strlen(pArea->szDataFileName) );
   pArea->pTagCurrent    = NULL;
   pArea->iOrders        = 0;
   pArea->fShared        = pOpenInfo->fShared;
   pArea->fReadonly      = pOpenInfo->fReadonly;
   if( pOpenInfo->cdpId )
   {
      pArea->area.cdPage = hb_cdpFind( (char *) pOpenInfo->cdpId );
      if( !pArea->area.cdPage )
         pArea->area.cdPage = hb_cdp_page;
   }
   else
      pArea->area.cdPage = hb_cdp_page;

   pArea->pLocksPos      = NULL;
   pArea->ulLocksMax = pArea->ulLocksAlloc = 0;

   if( *ptr == '1' )
     pArea->uiDriver = 1;
   ptr ++; ptr++;
   // Read number of fields
   leto_getCmdItem( &ptr, szTemp ); ptr ++;
   sscanf( szTemp, "%d" , &uiFields );

   SELF_SETFIELDEXTENT( ( AREAP ) pArea, uiFields );

   pArea->uiRecordLen = 0;

   for( uiCount = 1; uiCount <= (USHORT)uiFields; uiCount++ )
   {
      memset( &dbFieldInfo, 0, sizeof( dbFieldInfo ) );
      // Read field name
      leto_getCmdItem( &ptr, szName ); ptr ++;
      dbFieldInfo.atomName = szName;
      // Read field type
      leto_getCmdItem( &ptr, szTemp ); ptr ++;
      cType = *szTemp;
      // Read field length
      leto_getCmdItem( &ptr, szTemp ); ptr ++;
      sscanf( szTemp, "%d" , &uiLen );
      dbFieldInfo.uiLen = uiLen;
      // Read field decimals
      leto_getCmdItem( &ptr, szTemp ); ptr ++;
      sscanf( szTemp, "%d" , &uiLen );
      dbFieldInfo.uiDec = uiLen;

      dbFieldInfo.uiTypeExtended = 0;
      switch( cType )
      {
         case 'C':
            dbFieldInfo.uiType = HB_FT_STRING;
            break;
         case 'N':
            dbFieldInfo.uiType = HB_FT_LONG;
            break;
         case 'L':
            dbFieldInfo.uiType = HB_FT_LOGICAL;
            break;
         case 'D':
            dbFieldInfo.uiType = HB_FT_DATE;
            break;
         case 'M':
            dbFieldInfo.uiType = HB_FT_MEMO;
            break;
         case 'W':
            dbFieldInfo.uiType = HB_FT_BLOB;
            break;
         case 'P':
            dbFieldInfo.uiType = HB_FT_PICTURE;
            break;
         case 'G':
            dbFieldInfo.uiType = HB_FT_OLE;
            break;
         case 'V':
            dbFieldInfo.uiType = HB_FT_ANY;
            break;
         case 'I':
         case '2':
         case '4':
            dbFieldInfo.uiType = HB_FT_INTEGER;
            break;
         case 'F':
            dbFieldInfo.uiType = HB_FT_FLOAT;
            break;
         case 'Y':
            dbFieldInfo.uiType = HB_FT_CURRENCY;
            break;
         case '8':
         case 'B':
            dbFieldInfo.uiType = HB_FT_DOUBLE;
            break;
         case '@':
            dbFieldInfo.uiType = HB_FT_MODTIME;
            break;
         case 'T':
            dbFieldInfo.uiType = HB_FT_DAYTIME;
            break;
      }

      if( SELF_ADDFIELD( ( AREAP ) pArea, &dbFieldInfo ) == FAILURE )
      {
         return FAILURE;
      }
   }
   pArea->pRecord = ( BYTE * ) hb_xgrab( pArea->uiRecordLen+1 );

   // pOpenInfo->atomAlias = "TEST1";
   // If successful call SUPER_OPEN to finish system jobs
   if( SUPER_OPEN( ( AREAP ) pArea, pOpenInfo ) == FAILURE )
   {
      SELF_CLOSE( ( AREAP ) pArea );
      return FAILURE;
   }
   if( hb_setGetAutOpen() )
   {
      ptr = ParseTagInfo( pArea, ptr );
      if( hb_setGetAutOrder() )
      {
         LETOTAGINFO * pTagInfo = pArea->pTagInfo;
         while( pTagInfo )
         {
            if( pTagInfo->uiTag == hb_setGetAutOrder() )
            {
               pArea->pTagCurrent = pTagInfo;
               break;
            }
            pTagInfo = pTagInfo->pNext;
         }
      }
   }
   else
   {
      ptr = SkipTagInfo( ptr );
   }
   leto_ParseRec( pArea, ptr );

   return SUCCESS;
}

#define  letoRelease               NULL

static ERRCODE letoStructSize( LETOAREAP pArea, USHORT * StructSize )
{
   HB_SYMBOL_UNUSED( pArea );

   * StructSize = sizeof( LETOAREA );

   return SUCCESS;
}

static ERRCODE letoSysName( LETOAREAP pArea, BYTE * pBuffer )
{
   HB_TRACE(HB_TR_DEBUG, ("letoSysName(%p, %p)", pArea, pBuffer));

   HB_SYMBOL_UNUSED( pArea );
   hb_strncpy( ( char * ) pBuffer, "LETO", HB_RDD_MAX_DRIVERNAME_LEN );
   return SUCCESS;
}

#define  letoEval                  NULL

static ERRCODE letoPack( LETOAREAP pArea )
{
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoPack(%p)", pArea));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   if( bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pArea->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pArea->fShared )
   {
      commonError( pArea, EG_SHARED, EDBF_SHARED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   sprintf( szData,"pack;%lu;\r\n", pArea->hTable );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   return SELF_GOTOP( ( AREAP ) pArea );
}

#define  letoPackRec               NULL
#define  letoSort                  NULL
#define  letoTrans                 NULL
#define  letoTransRec              NULL

static ERRCODE letoZap( LETOAREAP pArea )
{
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoZap(%p)", pArea));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   if( bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pArea->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pArea->fShared )
   {
      commonError( pArea, EG_SHARED, EDBF_SHARED, 0, NULL, 0, NULL );
      return FAILURE;
   }

   sprintf( szData,"zap;%lu;\r\n", pArea->hTable );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   return SELF_GOTOP( ( AREAP ) pArea );
}

static ERRCODE letoChildEnd( LETOAREAP pArea, LPDBRELINFO pRelInfo )
{
   ERRCODE uiError;

   HB_TRACE(HB_TR_DEBUG, ("letoChildEnd(%p, %p)", pArea, pRelInfo));

   if( pArea->lpdbPendingRel == pRelInfo )
      uiError = SELF_FORCEREL( ( AREAP ) pArea );
   else
      uiError = SUCCESS;
   SUPER_CHILDEND( ( AREAP ) pArea, pRelInfo );
   return uiError;
}

static ERRCODE letoChildStart( LETOAREAP pArea, LPDBRELINFO pRelInfo )
{
   HB_TRACE(HB_TR_DEBUG, ("letoChildStart(%p, %p)", pArea, pRelInfo));

   if( SELF_CHILDSYNC( ( AREAP ) pArea, pRelInfo ) != SUCCESS )
      return FAILURE;
   return SUPER_CHILDSTART( ( AREAP ) pArea, pRelInfo );
}

static ERRCODE letoChildSync( LETOAREAP pArea, LPDBRELINFO pRelInfo )
{
   HB_TRACE(HB_TR_DEBUG, ("letoChildSync(%p, %p)", pArea, pRelInfo));

   pArea->lpdbPendingRel = pRelInfo;

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );

   return SUCCESS;
}

static ERRCODE letoForceRel( LETOAREAP pArea )
{
   HB_TRACE(HB_TR_DEBUG, ("letoForceRel(%p)", pArea));

   if( pArea->lpdbPendingRel )
   {
      LPDBRELINFO lpdbPendingRel;

      lpdbPendingRel = pArea->lpdbPendingRel;
      pArea->lpdbPendingRel = NULL;

      return SELF_RELEVAL( ( AREAP ) pArea, lpdbPendingRel );
   }
   return SUCCESS;
}

#define letoSyncChildren                         NULL
#define letoRelArea                              NULL
#define letoRelEval                              NULL
#define letoRelText                              NULL

static ERRCODE letoClearRel( LETOAREAP pArea )
{
   HB_TRACE(HB_TR_DEBUG, ("letoClearRel(%p)", pArea) );
/*
   if( pArea->area.lpdbRelations )
   {
      char szData[20];

      sprintf( szData,"rel;02;%lu;\r\n", pArea->hTable );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
   }
*/
   return SUPER_CLEARREL( ( AREAP ) pArea );
}

static ERRCODE letoSetRel( LETOAREAP pArea, LPDBRELINFO pRelInf )
{
/*
   char szData[LETO_MAX_EXP+34];
   LETOAREAP pChildArea = ( LETOAREAP ) pRelInf->lpaChild;
*/
   HB_TRACE(HB_TR_DEBUG, ("letoSetRel(%p, %p)", pArea, pRelInf) );
/*
   sprintf( szData,"rel;01;%lu;%lu;%s;%c;\r\n", pArea->hTable, pChildArea->hTable, hb_itemGetCPtr( pRelInf->abKey ), (pRelInf->isScoped ? 'T' : 'F') );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
*/
   return SUPER_SETREL( ( AREAP ) pArea, pRelInf );
}

static ERRCODE letoOrderListAdd( LETOAREAP pArea, LPDBORDERINFO pOrderInfo )
{
   char szData[_POSIX_PATH_MAX + 16], * ptr, * ptr1;
   const char * szBagName;
   LETOTAGINFO * pTagInfo;
   unsigned int uiLen;
   int iRcvLen;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListAdd(%p, %p)", pArea, pOrderInfo));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   szBagName = letoRemoveIpFromPath( hb_itemGetCPtr( pOrderInfo->atomBagName ) );
   strcpy( szData, szBagName );
   hb_strLower( szData, strlen(szData) );
   pTagInfo = pArea->pTagInfo;
   while( pTagInfo )
   {
      if( (ptr = strrchr(szData, '/')) == NULL )
         ptr = szData;
      else
         ptr ++;

      if( (ptr1 = strchr(ptr, '.')) == NULL )
         uiLen = strlen(ptr);
      else
         uiLen = ptr1 - ptr;

      if( (uiLen == strlen(pTagInfo->BagName)) && !memcmp( ptr, pTagInfo->BagName, uiLen ) )
      {
         pArea->pTagCurrent = pTagInfo;
         return letoGoTop( pArea );
      }
      pTagInfo = pTagInfo->pNext;
   }

   sprintf( szData,"open;02;%lu;%s;\r\n", pArea->hTable, szBagName );
   if ( ( iRcvLen = leto_SendRecv( pArea, szData, 0, 0 ) ) == 0 || leto_checkerror( pArea) )
      return FAILURE;
   ptr = leto_firstchar();

   pTagInfo = pArea->pTagInfo;
   while( pTagInfo && pTagInfo->pNext )
      pTagInfo = pTagInfo->pNext;

   ptr = ParseTagInfo( pArea, ptr );
   if( pTagInfo )
      pArea->pTagCurrent = pTagInfo->pNext;
   else
      pArea->pTagCurrent = pArea->pTagInfo;

   if( (iRcvLen - 4) > (ptr - s_szBuffer) )
      leto_ParseRec( pArea, ptr );

   return SUCCESS;
}

static ERRCODE letoOrderListClear( LETOAREAP pArea )
{
   char szData[32];
   LETOTAGINFO * pTagInfo = pArea->pTagInfo, * pTag1, * pTagPrev = NULL;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListClear(%p)", pArea));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   if( pTagInfo )
   {
      sprintf( szData,"ord;04;%lu;\r\n", pArea->hTable );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

      pArea->pTagCurrent = NULL;
      pArea->iOrders = 0;

      do
      {
         if( pArea->uiDriver || ( pTagInfo->BagName && leto_BagCheck( pArea->szDataFileName, pTagInfo->BagName ) ) )
         {
            pTag1 = pTagInfo;
            if( pTagInfo == pArea->pTagInfo )
               pTagInfo = pArea->pTagInfo = pTagInfo->pNext;
            else
               pTagInfo = pTagPrev->pNext = pTagInfo->pNext;

            if( pTag1->BagName )
               hb_xfree( pTag1->BagName );
            if( pTag1->TagName )
               hb_xfree( pTag1->TagName );
            if( pTag1->KeyExpr )
               hb_xfree( pTag1->KeyExpr );
            if( pTag1->pKeyItem != NULL )
               hb_vmDestroyBlockOrMacro( pTag1->pKeyItem );
            if( pTag1->ForExpr )
               hb_xfree( pTag1->ForExpr );
            if( pTag1->pTopScope )
               hb_itemRelease( pTag1->pTopScope );
            if( pTag1->pBottomScope )
               hb_itemRelease( pTag1->pBottomScope );
            hb_xfree( pTag1 );
         }
         else
         {
            pTagPrev = pTagInfo;
            pTagInfo = pTagPrev->pNext;
            pArea->iOrders ++;
         }
      }
      while( pTagInfo );

   }

   return SUCCESS;
}

#define letoOrderListDelete                      NULL

static ERRCODE letoOrderListFocus( LETOAREAP pArea, LPDBORDERINFO pOrderInfo )
{
   char szTagName[12];

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListFocus(%p, %p)", pArea, pOrderInfo));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );
   if( !pArea->pTagCurrent )
      pOrderInfo->itmResult = hb_itemPutCL( pOrderInfo->itmResult, "", 0 );
   else
   {
      USHORT uiLen = strlen( pArea->pTagCurrent->TagName );
      hb_strncpyUpper( szTagName, pArea->pTagCurrent->TagName, uiLen );
      pOrderInfo->itmResult = hb_itemPutCL( pOrderInfo->itmResult, 
            szTagName, uiLen );
   }
   if( pOrderInfo->itmOrder )
   {
      LETOTAGINFO * pTagInfo = pArea->pTagInfo;
      if( HB_IS_STRING( pOrderInfo->itmOrder ) )
      {
         strncpy( szTagName, hb_itemGetCPtr( pOrderInfo->itmOrder ), 11 );
         hb_strLower( szTagName, strlen(szTagName) );
         if( ! *szTagName )
         {
            pArea->pTagCurrent = NULL;
            return SUCCESS;
         }
         do
         {
            if( !strcmp( szTagName, pTagInfo->TagName ) )
               break;
            pTagInfo = pTagInfo->pNext;
         }
         while( pTagInfo );
      }
      else if( HB_IS_NUMERIC( pOrderInfo->itmOrder ) )
      {
         int iOrder =  hb_itemGetNI( pOrderInfo->itmOrder );
         if( !iOrder || iOrder > pArea->iOrders )
         {
            pArea->pTagCurrent = NULL;
            return SUCCESS;
         }
         while( --iOrder ) pTagInfo = pTagInfo->pNext;
      }

      pArea->pTagCurrent = pTagInfo;
      pArea->ptrBuf = NULL;
      if( !pTagInfo )
         return FAILURE;
   }
   return SUCCESS;
}

static ERRCODE letoOrderListRebuild( LETOAREAP pArea )
{
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListRebuild(%p)", pArea));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   sprintf( szData,"ord;03;%lu;\r\n", pArea->hTable );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   return SELF_GOTOP( ( AREAP ) pArea );
}

#define  letoOrderCondition        NULL

static ERRCODE letoOrderCreate( LETOAREAP pArea, LPDBORDERCREATEINFO pOrderInfo )
{
   char szData[LETO_MAX_EXP*2];
   const char * szKey, * szFor, * szBagName;
   LETOTAGINFO * pTagInfo;
   USHORT uiLen, uiLenab;
   PHB_ITEM pResult, pKeyExp;
   ERRCODE errCode;
   BYTE bType;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderCreate(%p, %p)", pArea, pOrderInfo));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   szKey = hb_itemGetCPtr( pOrderInfo->abExpr );

   if( pOrderInfo->itmCobExpr )
      pKeyExp = hb_itemNew( pOrderInfo->itmCobExpr );
   else /* Otherwise, try compiling the key expression string */
   {
      errCode = SELF_COMPILE( ( AREAP ) pArea, szKey );
      if( errCode != SUCCESS )
         return errCode;
      pKeyExp = pArea->area.valResult;
      pArea->area.valResult = NULL;
   }
   errCode = SELF_EVALBLOCK( ( AREAP ) pArea, pKeyExp );
   if( errCode != SUCCESS )
   {
      hb_vmDestroyBlockOrMacro( pKeyExp );
      return errCode;
   }
   pResult = pArea->area.valResult;
   pArea->area.valResult = NULL;

   bType = leto_ItemType( pResult );

   hb_itemRelease( pResult );
   hb_itemRelease( pKeyExp );

   if( pOrderInfo->abBagName )
      szBagName = letoRemoveIpFromPath( pOrderInfo->abBagName );
   else
      szBagName = NULL;

   szFor = (pArea->area.lpdbOrdCondInfo && pArea->area.lpdbOrdCondInfo->abFor)? pArea->area.lpdbOrdCondInfo->abFor : "";
   sprintf( szData,"creat;02;%lu;%s;%s;%s;%c;%s;%s;%s;%lu;%lu;%s;%s;%s;%s;%s;\r\n", pArea->hTable,
      (szBagName ? szBagName : ""),
      (pOrderInfo->atomBagName)? (char*)pOrderInfo->atomBagName : "",
      szKey, (pOrderInfo->fUnique)? 'T' : 'F', szFor,
      (pArea->area.lpdbOrdCondInfo && pArea->area.lpdbOrdCondInfo->abWhile)? (char*)pArea->area.lpdbOrdCondInfo->abWhile : "",
      (pArea->area.lpdbOrdCondInfo ? (pArea->area.lpdbOrdCondInfo->fAll ? "T" : "F") : ""),
      pArea->ulRecNo,
      (pArea->area.lpdbOrdCondInfo ? pArea->area.lpdbOrdCondInfo->lNextCount : 0),
      "",
      (pArea->area.lpdbOrdCondInfo ? (pArea->area.lpdbOrdCondInfo->fRest ? "T" : "F") : ""),
      (pArea->area.lpdbOrdCondInfo ? (pArea->area.lpdbOrdCondInfo->fDescending ? "T" : "F") : ""),
      (pArea->area.lpdbOrdCondInfo ? (pArea->area.lpdbOrdCondInfo->fCustom ? "T" : "F") : ""),
      (pArea->area.lpdbOrdCondInfo ? (pArea->area.lpdbOrdCondInfo->fAdditive ? "T" : "F") : "")
      );
       
   if ( !leto_SendRecv( pArea, szData, 0, 0 ) || leto_checkerror( pArea ) )
      return FAILURE;

   pTagInfo = (LETOTAGINFO *) hb_xgrab( sizeof(LETOTAGINFO) );
   memset( pTagInfo, 0, sizeof(LETOTAGINFO) );
   if( pArea->pTagInfo )
   {
      LETOTAGINFO * pTagNext = pArea->pTagInfo;
      while( pTagNext->pNext ) pTagNext = pTagNext->pNext;
      pTagNext->pNext = pTagInfo;
   }
   else
      pArea->pTagInfo = pTagInfo;

   if( ( uiLenab = (szBagName)? strlen((char*)szBagName) : 0 ) != 0 )
   {
      pTagInfo->BagName = (char*) hb_xgrab( uiLenab+1 );
      memcpy( pTagInfo->BagName, szBagName, uiLenab );
      pTagInfo->BagName[uiLenab] = '\0';
      hb_strLower( pTagInfo->BagName, uiLenab );
   }
   if( ( uiLen = (pOrderInfo->atomBagName)? strlen((char*)pOrderInfo->atomBagName) : 0 ) != 0 )
   {
      pTagInfo->TagName = (char*) hb_xgrab( uiLen+1 );
      memcpy( pTagInfo->TagName, pOrderInfo->atomBagName, uiLen );
      pTagInfo->TagName[uiLen] = '\0';
      hb_strLower( pTagInfo->TagName, uiLen );
   }
   else if( uiLenab )
   {
      pTagInfo->TagName = (char*) hb_xgrab( uiLenab+1 );
      memcpy( pTagInfo->TagName, pTagInfo->BagName, uiLenab );
      pTagInfo->TagName[uiLenab] = '\0';
   }
   if( !pTagInfo->TagName )
   {
   }
   leto_CreateKeyExpr( pArea, pTagInfo, szKey );
   if( ( uiLen = strlen( szFor ) ) != 0 )
   {
      pTagInfo->ForExpr = (char*) hb_xgrab( uiLen+1 );
      memcpy( pTagInfo->ForExpr, szFor, uiLen );
      pTagInfo->ForExpr[uiLen] = '\0';
   }
   pTagInfo->KeyType = bType;
   pTagInfo->uiTag = pArea->iOrders;

   pArea->iOrders ++;
   pArea->pTagCurrent = pTagInfo;

   return SELF_GOTOP( ( AREAP ) pArea );
}

static ERRCODE letoOrderDestroy( LETOAREAP pArea, LPDBORDERINFO pOrderInfo )
{
   HB_TRACE(HB_TR_DEBUG, ("letoOrderDestroy(%p, %p)", pArea, pOrderInfo));
   HB_SYMBOL_UNUSED( pOrderInfo );

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   return SUCCESS;
}

static ERRCODE letoOrderInfo( LETOAREAP pArea, USHORT uiIndex, LPDBORDERINFO pOrderInfo )
{
   LETOTAGINFO * pTagInfo = NULL;
   char szData[32];
   USHORT uiTag = 0;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderInfo(%p, %hu, %p)", pArea, uiIndex, pOrderInfo));

   if( pOrderInfo->itmOrder )
   {
      if( ! pArea->pTagInfo )
      {
         // index not enabled
      }
      else if( HB_IS_STRING( pOrderInfo->itmOrder ) )
      {
         char szTagName[12];

         strncpy( szTagName, hb_itemGetCPtr( pOrderInfo->itmOrder ), 11 );
         hb_strLower( szTagName, strlen(szTagName) );
         if( *szTagName )
         {
            pTagInfo = pArea->pTagInfo;
            do
            {
               ++uiTag;
               if( !strcmp( szTagName, pTagInfo->TagName ) )
                  break;
               pTagInfo = pTagInfo->pNext;
            }
            while( pTagInfo );
         }
      }
      else if( HB_IS_NUMERIC( pOrderInfo->itmOrder ) )
      {
         int iOrder =  hb_itemGetNI( pOrderInfo->itmOrder );

         if( iOrder && iOrder <= pArea->iOrders )
         {
            pTagInfo = pArea->pTagInfo;
            while( --iOrder ) pTagInfo = pTagInfo->pNext;
            iOrder ++;
         }
         else
            iOrder = 0;
         uiTag = iOrder;
      }
   }
   else
   {
      pTagInfo = pArea->pTagCurrent;
      if( pTagInfo )
         uiTag = pTagInfo->uiTag;
   }

   switch( uiIndex )
   {
      case DBOI_CONDITION:
         if( pTagInfo )
            hb_itemPutC( pOrderInfo->itmResult, pTagInfo->ForExpr );
         else
            hb_itemPutC( pOrderInfo->itmResult, "" );
         break;

      case DBOI_EXPRESSION:
         if( pTagInfo )
            hb_itemPutC( pOrderInfo->itmResult, pTagInfo->KeyExpr );
         else
            hb_itemPutC( pOrderInfo->itmResult, "" );
         break;

      case DBOI_KEYTYPE:
         if( pTagInfo )
         {
            char s[2];
            s[0] = pTagInfo->KeyType;
            s[1] = '\0';
            hb_itemPutC( pOrderInfo->itmResult, s );
         }
         else
            hb_itemPutC( pOrderInfo->itmResult, "" );
         break;

      case DBOI_KEYSIZE:
         hb_itemPutNL( pOrderInfo->itmResult, pTagInfo ? pTagInfo->KeySize : 0 );
         break;

      case DBOI_KEYVAL:
      {
         PHB_ITEM pItem;
         hb_itemClear( pOrderInfo->itmResult );

         if( pArea->lpdbPendingRel )
            SELF_FORCEREL( ( AREAP ) pArea );

         if( ! pArea->area.fEof && pTagInfo && pTagInfo->pKeyItem )
         {
            pItem = hb_vmEvalBlockOrMacro( pTagInfo->pKeyItem );
            hb_itemCopy( pOrderInfo->itmResult, pItem );
         }
         break;
      }

      case DBOI_KEYCOUNT:
      {
         char * ptr;
         ULONG ul;

         if( !pTagInfo )
            sprintf( szData,"rcou;%lu;\r\n", pArea->hTable );
         else
            sprintf( szData,"ord;01;%lu;%s;\r\n", pArea->hTable,
               (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"" );

         if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

         ptr = leto_firstchar();
         leto_getCmdItem( &ptr, szData ); ptr ++;
         sscanf( szData, "%lu" , &ul );
         hb_itemPutNL( pOrderInfo->itmResult, ul );

         break;
      }

      case DBOI_SCOPETOP:
      case DBOI_SCOPEBOTTOM:
      case DBOI_SCOPESET:
      case DBOI_SCOPETOPCLEAR:
      case DBOI_SCOPEBOTTOMCLEAR:
      case DBOI_SCOPECLEAR:
      {
         if( pTagInfo )
         {
            char szData[LETO_MAX_KEY+LETO_MAX_TAGNAME+35], szKey[LETO_MAX_KEY+1];

            if( pTagInfo->pTopScope && ((uiIndex == DBOI_SCOPETOP) || (uiIndex == DBOI_SCOPESET) || (uiIndex == DBOI_SCOPETOPCLEAR) || (uiIndex == DBOI_SCOPECLEAR)) )
               hb_itemCopy( pOrderInfo->itmResult, pTagInfo->pTopScope );
            else if( pTagInfo->pBottomScope && ((uiIndex == DBOI_SCOPEBOTTOM) || (uiIndex == DBOI_SCOPESET) || (uiIndex == DBOI_SCOPEBOTTOMCLEAR) || (uiIndex == DBOI_SCOPECLEAR)) )
               hb_itemCopy( pOrderInfo->itmResult, pTagInfo->pBottomScope );
            else
               hb_itemClear( pOrderInfo->itmResult );

            if( pOrderInfo->itmNewVal )
            {
               if( (uiIndex == DBOI_SCOPETOP) || (uiIndex == DBOI_SCOPESET) )
               {
                  if( pTagInfo->pTopScope == NULL )
                     pTagInfo->pTopScope = hb_itemNew( NULL );
                  hb_itemCopy( pTagInfo->pTopScope, pOrderInfo->itmNewVal );
               }
               if( (uiIndex == DBOI_SCOPEBOTTOM) || (uiIndex == DBOI_SCOPESET) )
               {
                  if( pTagInfo->pBottomScope == NULL )
                     pTagInfo->pBottomScope = hb_itemNew( NULL );
                  hb_itemCopy( pTagInfo->pBottomScope, pOrderInfo->itmNewVal );
               }
            }

            if( ((uiIndex == DBOI_SCOPETOPCLEAR) || (uiIndex == DBOI_SCOPECLEAR)) && pTagInfo->pTopScope )
               hb_itemClear(pTagInfo->pTopScope);
            if( ((uiIndex == DBOI_SCOPEBOTTOMCLEAR) || (uiIndex == DBOI_SCOPECLEAR)) && pTagInfo->pBottomScope )
               hb_itemClear(pTagInfo->pBottomScope);

            if( pOrderInfo->itmNewVal || (uiIndex == DBOI_SCOPETOPCLEAR) || (uiIndex == DBOI_SCOPEBOTTOMCLEAR) || (uiIndex == DBOI_SCOPECLEAR) )
            {
               USHORT uiKeyLen;
               ULONG ulLen;
               char *pData;

               if( pOrderInfo->itmNewVal )
               {
                  uiKeyLen = letoKeyToStr( pArea, szKey, leto_ItemType( pOrderInfo->itmNewVal ), pOrderInfo->itmNewVal );
               }
               else
               {
                  uiKeyLen = 0;
                  szKey[0] = '\0';
               }
               pData = szData + 4;
               sprintf( pData,"scop;%lu;%i;%s;", pArea->hTable, uiIndex,
                  (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"");
               pData = ( pData + strlen( pData ) );
               *pData++ = (char) (uiKeyLen) & 0xFF;
               memcpy( pData,szKey,uiKeyLen );
               pData += uiKeyLen;
               *pData = '\0';

               ulLen = pData - szData - 4;
               pData = leto_AddLen( (szData+4), &ulLen, 1 );
               if ( !leto_SendRecv( pArea, pData, (ULONG) ulLen, 1021 ) ) return FAILURE;
            }

         }
         else
            hb_itemClear( pOrderInfo->itmResult );

         break;
      }

      case DBOI_POSITION:
      {
         char * ptr;
         ULONG ul;

         if( pOrderInfo->itmNewVal )   // ordkeygoto
         {
            if( !pTagInfo || !HB_IS_NUMBER( pOrderInfo->itmNewVal ) )
            {
               hb_itemPutL( pOrderInfo->itmResult, FALSE );
               break;
            }

            pArea->lpdbPendingRel = NULL;
            if( pArea->uiUpdated )
               leto_PutRec( pArea );

            sprintf( szData,"ord;05;%lu;%s;%lu;\r\n", pArea->hTable,
               (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"", hb_itemGetNL( pOrderInfo->itmNewVal ) );

            if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
            hb_itemPutL( pOrderInfo->itmResult, TRUE );

            ptr = leto_firstchar();
            leto_ParseRec( pArea, ptr );
            pArea->ptrBuf = NULL;

            if( pArea->area.lpdbRelations )
               return SELF_SYNCCHILDREN( ( AREAP ) pArea );
            break;
         }
         else     // ordkeyno
         {
            if( !pTagInfo )
            {
               hb_itemPutNL( pOrderInfo->itmResult, FALSE );
               break;
            }
            sprintf( szData,"ord;02;%lu;%s;%lu;\r\n", pArea->hTable,
                  (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"", pArea->ulRecNo );

            if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

            ptr = leto_firstchar();
            leto_getCmdItem( &ptr, szData ); ptr ++;
            sscanf( szData, "%lu" , &ul );
            hb_itemPutNL( pOrderInfo->itmResult, ul );
         }
         break;
      }

      case DBOI_SKIPUNIQUE:
      {
         int lRet;
         char * ptr;

         pArea->lpdbPendingRel = NULL;
         if( pArea->uiUpdated )
            leto_PutRec( pArea );

         sprintf( szData,"ord;06;%lu;%s;%lu;\r\n", pArea->hTable,
            (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
            pOrderInfo->itmNewVal && HB_IS_NUMERIC( pOrderInfo->itmNewVal ) ?
                     hb_itemGetNL( pOrderInfo->itmNewVal ) : 1 );

         lRet = leto_SendRecv( pArea, szData, 0, 1021 );
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult, lRet );
         if( !lRet ) return FAILURE;

         ptr = leto_firstchar();
         leto_ParseRec( pArea, ptr );
         pArea->ptrBuf = NULL;

         if( pArea->area.lpdbRelations )
            return SELF_SYNCCHILDREN( ( AREAP ) pArea );
         break;
      }

      case DBOI_BAGEXT:
      {
         if( pOrderInfo->itmResult )
            hb_itemClear( pOrderInfo->itmResult );
         else
            pOrderInfo->itmResult = hb_itemNew( NULL );
         hb_itemPutC( pOrderInfo->itmResult, (pArea->uiDriver)? ".NTX" : ".CDX" );
         break;
      }

      case DBOI_NUMBER:
      {
         hb_itemPutNI( pOrderInfo->itmResult, pTagInfo ? uiTag : 0 );
         break;
      }
      
      case DBOI_ISDESC:
      case DBOI_CUSTOM:
      case DBOI_UNIQUE:
      {
         char * ptr;

         sprintf( szData,"dboi;%lu;%i;%s;%s;\r\n", pArea->hTable, uiIndex,
               (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
               (pOrderInfo->itmNewVal && HB_IS_LOGICAL( pOrderInfo->itmNewVal )) ? ((pOrderInfo->itmNewVal) ? "T" : "F") : "" );

         if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

         ptr = leto_firstchar();
         leto_getCmdItem( &ptr, szData ); ptr ++;
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult, szData[0] == 'T' );

         break;
      }
      case DBOI_NAME:
      {
         pOrderInfo->itmResult = hb_itemPutC( pOrderInfo->itmResult, pTagInfo ? pTagInfo->TagName : "" );
         break;
      }      
      case DBOI_BAGNAME:
      {
         pOrderInfo->itmResult = hb_itemPutC( pOrderInfo->itmResult, pTagInfo ? pTagInfo->BagName : "" );
         break;
      }
      case DBOI_KEYADD:
      case DBOI_KEYDELETE:
      {
         char * ptr;

         sprintf( szData,"dboi;%lu;%i;%s;%s;\r\n", pArea->hTable, uiIndex,
               (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
               (pOrderInfo->itmNewVal && HB_IS_STRING( pOrderInfo->itmNewVal )) ? hb_itemGetCPtr( pOrderInfo->itmNewVal ) : "" );

         if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

         ptr = leto_firstchar();
         leto_getCmdItem( &ptr, szData ); ptr ++;
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult, szData[0] == 'T' );
         break;
      }
      case DBOI_ORDERCOUNT:
      {
         hb_itemPutNI( pOrderInfo->itmResult, pArea->iOrders );
         break;
      }
   }

   return SUCCESS;
}

static ERRCODE letoClearFilter( LETOAREAP pArea )
{
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoClearFilter(%p)", pArea));

   if( pArea->hTable && pArea->area.dbfi.itmCobExpr )
   {
      sprintf( szData,"setf;%lu;;\r\n", pArea->hTable );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
   }

   return SUPER_CLEARFILTER( ( AREAP ) pArea );
}

#define  letoClearLocate           NULL
#define  letoClearScope            NULL
#define  letoCountScope            NULL
#define  letoFilterText            NULL
#define  letoScopeInfo             NULL

static ERRCODE letoSetFilter( LETOAREAP pArea, LPDBFILTERINFO pFilterInfo )
{
   char * pData, * pData1;
   ULONG ulLen;

   HB_TRACE(HB_TR_DEBUG, ("letoSetFilter(%p, %p)", pArea, pFilterInfo));

   if( SUPER_SETFILTER( ( AREAP ) pArea, pFilterInfo ) == SUCCESS )
   {
      if( pFilterInfo->abFilterText && hb_itemGetCLen(pFilterInfo->abFilterText) )
      {
         ulLen = hb_itemGetCLen(pFilterInfo->abFilterText);
         pData = (char*) malloc( ulLen + 24 );
         sprintf( pData,"setf;%lu;", pArea->hTable );

         pData1 = pData + strlen(pData);
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
         memcpy( pData1, hb_itemGetCPtr(pFilterInfo->abFilterText), ulLen );
         if( pArea->area.cdPage != hb_cdp_page )
            hb_cdpnTranslate( pData1, hb_cdp_page, pArea->area.cdPage, ulLen );
         pData1 += ulLen;
#else
         if( pArea->area.cdPage != hb_cdp_page )
         {
            ULONG ulLen1 = ulLen;
            char * pBuff = hb_cdpnDup( hb_itemGetCPtr(pFilterInfo->abFilterText), &ulLen1, hb_cdp_page, pArea->area.cdPage );
            memcpy( pData1, pBuff, ulLen1 );
            hb_xfree( pBuff );
            pData1 += ulLen1;
         }
         else
         {
            memcpy( pData1, hb_itemGetCPtr(pFilterInfo->abFilterText), ulLen );
            pData1 += ulLen;
         }
#endif
         memcpy( pData1, ";\r\n\0", 4 );

         if ( !leto_SendRecv( pArea, pData, 0, 0 ) )
         {
            free( pData );
            return FAILURE;
         }
         free( pData );

         if( *s_szBuffer == '-' )
            pArea->area.dbfi.fOptimized = FALSE;
         else
            pArea->area.dbfi.fOptimized = TRUE;
      }
      else
         pArea->area.dbfi.fOptimized = FALSE;

      return SUCCESS;
   }
   else
      return FAILURE;
}

#define  letoSetLocate             NULL
#define  letoSetScope              NULL
#define  letoSkipScope             NULL
#define  letoLocate                NULL
#define  letoCompile               NULL
#define  letoError                 NULL
#define  letoEvalBlock             NULL

static BOOL letoIsRecLocked( LETOAREAP pArea, ULONG ulRecNo )
{
   ULONG ul;
   if( pArea->pLocksPos )   
   {
      for( ul = 0; ul < pArea->ulLocksMax; ul++ )
      {
         if( pArea->pLocksPos[ ul ] == ulRecNo )
         {
            return TRUE;
         }
      }
   }
   return FALSE;
}

static void letoAddRecLock( LETOAREAP pArea, ULONG ulRecNo )
{
   if( ! letoIsRecLocked( pArea, ulRecNo ) )
   {
      if( ! pArea->pLocksPos )
      {
         /* Allocate locks array for the table, if it isn't allocated yet */
         pArea->ulLocksAlloc = 10;
         pArea->pLocksPos = (ULONG*) hb_xgrab( sizeof(ULONG) * pArea->ulLocksAlloc );
         pArea->ulLocksMax = 0;
      }
      else if( pArea->ulLocksMax == pArea->ulLocksAlloc )
      {
         pArea->pLocksPos = (ULONG*) hb_xrealloc( pArea->pLocksPos, sizeof(ULONG) * (pArea->ulLocksAlloc+10) );
         pArea->ulLocksAlloc += 10;
      }
      pArea->pLocksPos[ pArea->ulLocksMax++ ] = ulRecNo;
   }
}

static ERRCODE letoRawLock( LETOAREAP pArea, USHORT uiAction, ULONG ulRecNo )
{
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoRawLock(%p, %hu, %lu)", pArea, uiAction, ulRecNo));

   if( pArea->uiUpdated )
      leto_PutRec( pArea );

   if( pArea->lpdbPendingRel )
   {
      if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
         return FAILURE;
   }

   switch( uiAction )
   {
      case REC_LOCK:
         pArea->ptrBuf = NULL;
         if( !pArea->fShared || pArea->fFLocked || ( ulRecNo == pArea->ulRecNo ?
            pArea->fRecLocked : letoIsRecLocked(pArea, ulRecNo) ) )
            return SUCCESS;

         sprintf( szData,"lock;01;%lu;%lu;\r\n", pArea->hTable, ulRecNo );
         if ( !leto_SendRecv( pArea, szData, 0, 0 ) ) return FAILURE;
         if( *s_szBuffer == '-' )
         {
            if( *(s_szBuffer+3) != '4' )
               commonError( pArea, EG_DATATYPE, 1021, 0, s_szBuffer, 0, NULL );
            return FAILURE;
         }
         if( ulRecNo == pArea->ulRecNo )
            pArea->fRecLocked = 1;
         break;

      case REC_UNLOCK:
         pArea->ptrBuf = NULL;
         if( bTransActive )
         {
            commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
            return FAILURE;
         }
         if( !pArea->fShared || pArea->fFLocked || !pArea->fRecLocked )
            return SUCCESS;

         sprintf( szData,"unlock;01;%lu;%lu;\r\n", pArea->hTable, ulRecNo );
         if ( !leto_SendRecv( pArea, szData, 0, 0 ) ) return FAILURE;
         if( *s_szBuffer == '-' )
         {
            if( *(s_szBuffer+3) != '4' )
               commonError( pArea, EG_DATATYPE, 1021, 0, s_szBuffer, 0, NULL );
            return FAILURE;
         }
         pArea->fRecLocked = 0;
         break;

      case FILE_LOCK:
         pArea->ptrBuf = NULL;
         if( !pArea->fShared || pArea->fFLocked )
            return SUCCESS;

         sprintf( szData,"lock;02;%lu;\r\n", pArea->hTable );
         if ( !leto_SendRecv( pArea, szData, 0, 0 ) ) return FAILURE;
         if( *s_szBuffer == '-' )
         {
            if( *(s_szBuffer+3) != '4' )
               commonError( pArea, EG_DATATYPE, 1021, 0, s_szBuffer, 0, NULL );
            return FAILURE;
         }
         pArea->fFLocked = TRUE;
         break;

      case FILE_UNLOCK:
         pArea->ptrBuf = NULL;
         if( bTransActive )
         {
            commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
            return FAILURE;
         }
         if( !pArea->fShared )
            return SUCCESS;

         sprintf( szData,"unlock;02;%lu;\r\n", pArea->hTable );
         if ( !leto_SendRecv( pArea, szData, 0, 0 ) ) return FAILURE;
         if( *s_szBuffer == '-' )
         {
            if( *(s_szBuffer+3) != '4' )
               commonError( pArea, EG_DATATYPE, 1021, 0, s_szBuffer, 0, NULL );
            return FAILURE;
         }
         pArea->fFLocked = FALSE;
         pArea->fRecLocked = 0;
         pArea->ulLocksMax = 0;
         break;
   }
   return SUCCESS;
}

static ERRCODE letoLock( LETOAREAP pArea, LPDBLOCKINFO pLockInfo )
{
   ULONG ulRecNo = ( ULONG ) hb_itemGetNL( pLockInfo->itmRecID );
   USHORT uiAction;

   HB_TRACE(HB_TR_DEBUG, ("letoLock(%p, %p)", pArea, pLockInfo));

   if( !ulRecNo && pArea->lpdbPendingRel )
      SELF_FORCEREL( ( AREAP ) pArea );

   switch( pLockInfo->uiMethod )
   {
      case DBLM_EXCLUSIVE :
         ulRecNo = pArea->ulRecNo;
         uiAction = REC_LOCK;
         pArea->ulLocksMax = 0;
         break;

      case DBLM_MULTIPLE :
         if( pLockInfo->itmRecID == NULL )
            ulRecNo = pArea->ulRecNo;
         uiAction = REC_LOCK;
         break;

      case DBLM_FILE :
         uiAction = FILE_LOCK;
         pArea->ulLocksMax = 0;
         break;

      default  :
         /* This should probably throw a real error... */
         pLockInfo->fResult = FALSE;
         return FAILURE;
   }

   pLockInfo->fResult = SELF_RAWLOCK( ( AREAP ) pArea, uiAction,
                                      ulRecNo ) == SUCCESS;

   if( pLockInfo->fResult && ( pLockInfo->uiMethod == DBLM_EXCLUSIVE || pLockInfo->uiMethod == DBLM_MULTIPLE ) )
      letoAddRecLock( pArea, ulRecNo );

   return SUCCESS;
}

static ERRCODE letoUnLock( LETOAREAP pArea, PHB_ITEM pRecNo )
{
   ULONG ulRecNo = hb_itemGetNL( pRecNo );
   ERRCODE iRes;

   HB_TRACE(HB_TR_DEBUG, ("letoUnLock(%p, %p)", pArea, pRecNo));

   iRes = SELF_RAWLOCK( ( AREAP ) pArea,
                        (ulRecNo)? REC_UNLOCK : FILE_UNLOCK, ulRecNo );
   if( iRes == SUCCESS && ulRecNo && pArea->pLocksPos )
   {
      ULONG ul;
      for( ul = 0; ul < pArea->ulLocksMax; ul++ )
      {
         if( pArea->pLocksPos[ ul ] == ulRecNo )
         {
            ULONG * pList = pArea->pLocksPos + ul;
            if(ul < pArea->ulLocksMax - 1)
               memmove( pList, pList + 1, ( pArea->ulLocksMax - ul - 1 ) *
                        sizeof( ULONG ) );
            pArea->ulLocksMax --;
            break;
         }
      }
   }

   return iRes;
}

#define  letoCloseMemFile          NULL
#define  letoCreateMemFile         NULL
#define  letoGetValueFile          NULL

#define  letoOpenMemFile           NULL
#define  letoPutValueFile          NULL
#define  letoReadDBHeader          NULL
#define  letoWriteDBHeader         NULL
#define  letoDrop                  NULL
#define  letoExists                NULL
#define  letoRename                NULL

static ERRCODE letoInit( LPRDDNODE pRDD )
{
   HB_SYMBOL_UNUSED( pRDD );

   hb_ipInit();

   letoConnPool = hb_xgrab( sizeof( LETOCONNECTION ) );
   uiConnCount = 1;
   memset( letoConnPool, 0, sizeof(LETOCONNECTION) * uiConnCount );

   s_szBuffer = (char*) malloc(HB_SENDRECV_BUFFER_SIZE);
   s_lBufferLen = HB_SENDRECV_BUFFER_SIZE;
   return SUCCESS;
}

static ERRCODE letoExit( LPRDDNODE pRDD )
{
   USHORT i;
   HB_SYMBOL_UNUSED( pRDD );

   for( i=0; i<uiConnCount; i++ )
      leto_ConnectionClose( letoConnPool + i );

   if( szTransBuffer )
   {
      hb_xfree( szTransBuffer );
      szTransBuffer = NULL;
   }

   if( s_uiRddCount )
   {
      if( ! --s_uiRddCount )
      {
         free( s_szBuffer );
#ifdef __BORLANDC__
   #pragma option push -w-pro
#endif
         // letoApplicationExit();
#ifdef __BORLANDC__
   #pragma option pop
#endif
         hb_ipCleanup();
      }
   }

   /* free pRDD->lpvCargo if necessary */

   return SUCCESS;
}


static ERRCODE letoRddInfo( LPRDDNODE pRDD, USHORT uiIndex, ULONG ulConnect, PHB_ITEM pItem )
{
   HB_TRACE(HB_TR_DEBUG, ("letoRddInfo(%p, %hu, %lu, %p)", pRDD, uiIndex, ulConnect, pItem));

   switch( uiIndex )
   {
      case RDDI_REMOTE:
         hb_itemPutL( pItem, TRUE );
         break;
/*
      case RDDI_CONNECTION:
      {     
         ADSHANDLE hOldConnection = adsConnectHandle;

         adsConnectHandle = HB_ADS_GETCONNECTION( pItem );
         HB_ADS_PUTCONNECTION( pItem, hOldConnection );
         break;
      }
*/
      case RDDI_ISDBF:
         hb_itemPutL( pItem, TRUE );
         break;

      case RDDI_CANPUTREC:
         hb_itemPutL( pItem, TRUE );
         break;

      case RDDI_TABLEEXT:
         hb_itemPutC( pItem, "dbf" );
         break;

      case RDDI_MEMOEXT:
         hb_itemPutC( pItem, "fpt" );
         break;

      case RDDI_ORDEREXT:
      case RDDI_ORDBAGEXT:
      case RDDI_ORDSTRUCTEXT:
         hb_itemPutC( pItem, "cdx" );
         break;

      default:
         return SUPER_RDDINFO( pRDD, uiIndex, ulConnect, pItem );
   }

   return SUCCESS;
}

#define  letoWhoCares              NULL

static const RDDFUNCS letoTable = { ( DBENTRYP_BP ) letoBof,
                                   ( DBENTRYP_BP ) letoEof,
                                   ( DBENTRYP_BP ) letoFound,
                                   ( DBENTRYP_V ) letoGoBottom,
                                   ( DBENTRYP_UL ) letoGoTo,
                                   ( DBENTRYP_I ) letoGoToId,
                                   ( DBENTRYP_V ) letoGoTop,
                                   ( DBENTRYP_BIB ) letoSeek,
                                   ( DBENTRYP_L ) letoSkip,
                                   ( DBENTRYP_L ) letoSkipFilter,
                                   ( DBENTRYP_L ) letoSkipRaw,
                                   ( DBENTRYP_VF ) letoAddField,
                                   ( DBENTRYP_B ) letoAppend,
                                   ( DBENTRYP_I ) letoCreateFields,
                                   ( DBENTRYP_V ) letoDeleteRec,
                                   ( DBENTRYP_BP ) letoDeleted,
                                   ( DBENTRYP_SP ) letoFieldCount,
                                   ( DBENTRYP_VF ) letoFieldDisplay,
                                   ( DBENTRYP_SSI ) letoFieldInfo,
#if defined( __OLDRDD__ )
                                   ( DBENTRYP_SVP ) letoFieldName,
                                   ( DBENTRYP_V ) letoFlush,
                                   ( DBENTRYP_PP ) letoGetRec,
                                   ( DBENTRYP_SI ) letoGetValue,
                                   ( DBENTRYP_SVL ) letoGetVarLen,
                                   ( DBENTRYP_V ) letoGoCold,
                                   ( DBENTRYP_V ) letoGoHot,
                                   ( DBENTRYP_P ) letoPutRec,
                                   ( DBENTRYP_SI ) letoPutValue,
                                   ( DBENTRYP_V ) letoRecall,
                                   ( DBENTRYP_ULP ) letoRecCount,
                                   ( DBENTRYP_ISI ) letoRecInfo,
                                   ( DBENTRYP_ULP ) letoRecNo,
                                   ( DBENTRYP_I ) letoRecId,
                                   ( DBENTRYP_S ) letoSetFieldExtent,
                                   ( DBENTRYP_P ) letoAlias,
                                   ( DBENTRYP_V ) letoClose,
                                   ( DBENTRYP_VP ) letoCreate,
                                   ( DBENTRYP_SI ) letoInfo,
                                   ( DBENTRYP_V ) letoNewArea,
                                   ( DBENTRYP_VP ) letoOpen,
                                   ( DBENTRYP_V ) letoRelease,
                                   ( DBENTRYP_SP ) letoStructSize,
                                   ( DBENTRYP_P ) letoSysName,
                                   ( DBENTRYP_VEI ) letoEval,
                                   ( DBENTRYP_V ) letoPack,
                                   ( DBENTRYP_LSP ) letoPackRec,
                                   ( DBENTRYP_VS ) letoSort,
                                   ( DBENTRYP_VT ) letoTrans,
                                   ( DBENTRYP_VT ) letoTransRec,
                                   ( DBENTRYP_V ) letoZap,
                                   ( DBENTRYP_VR ) letoChildEnd,
                                   ( DBENTRYP_VR ) letoChildStart,
                                   ( DBENTRYP_VR ) letoChildSync,
                                   ( DBENTRYP_V ) letoSyncChildren,
                                   ( DBENTRYP_V ) letoClearRel,
                                   ( DBENTRYP_V ) letoForceRel,
                                   ( DBENTRYP_SVP ) letoRelArea,
                                   ( DBENTRYP_VR ) letoRelEval,
                                   ( DBENTRYP_SI ) letoRelText,
                                   ( DBENTRYP_VR ) letoSetRel,
                                   ( DBENTRYP_OI ) letoOrderListAdd,
                                   ( DBENTRYP_V ) letoOrderListClear,
                                   ( DBENTRYP_OI ) letoOrderListDelete,
                                   ( DBENTRYP_OI ) letoOrderListFocus,
                                   ( DBENTRYP_V ) letoOrderListRebuild,
                                   ( DBENTRYP_VOI ) letoOrderCondition,
                                   ( DBENTRYP_VOC ) letoOrderCreate,
                                   ( DBENTRYP_OI ) letoOrderDestroy,
                                   ( DBENTRYP_OII ) letoOrderInfo,
                                   ( DBENTRYP_V ) letoClearFilter,
                                   ( DBENTRYP_V ) letoClearLocate,
                                   ( DBENTRYP_V ) letoClearScope,
                                   ( DBENTRYP_VPLP ) letoCountScope,
                                   ( DBENTRYP_I ) letoFilterText,
                                   ( DBENTRYP_SI ) letoScopeInfo,
                                   ( DBENTRYP_VFI ) letoSetFilter,
                                   ( DBENTRYP_VLO ) letoSetLocate,
                                   ( DBENTRYP_VOS ) letoSetScope,
                                   ( DBENTRYP_VPL ) letoSkipScope,
                                   ( DBENTRYP_B ) letoLocate,
                                   ( DBENTRYP_P ) letoCompile,
                                   ( DBENTRYP_I ) letoError,
                                   ( DBENTRYP_I ) letoEvalBlock,
                                   ( DBENTRYP_VSP ) letoRawLock,
                                   ( DBENTRYP_VL ) letoLock,
                                   ( DBENTRYP_I ) letoUnLock,
                                   ( DBENTRYP_V ) letoCloseMemFile,
                                   ( DBENTRYP_VP ) letoCreateMemFile,
                                   ( DBENTRYP_SVPB ) letoGetValueFile,
                                   ( DBENTRYP_VP ) letoOpenMemFile,
                                   ( DBENTRYP_SVPB ) letoPutValueFile,
#else
                                   ( DBENTRYP_SCP ) letoFieldName,
                                   ( DBENTRYP_V ) letoFlush,
                                   ( DBENTRYP_PP ) letoGetRec,
                                   ( DBENTRYP_SI ) letoGetValue,
                                   ( DBENTRYP_SVL ) letoGetVarLen,
                                   ( DBENTRYP_V ) letoGoCold,
                                   ( DBENTRYP_V ) letoGoHot,
                                   ( DBENTRYP_P ) letoPutRec,
                                   ( DBENTRYP_SI ) letoPutValue,
                                   ( DBENTRYP_V ) letoRecall,
                                   ( DBENTRYP_ULP ) letoRecCount,
                                   ( DBENTRYP_ISI ) letoRecInfo,
                                   ( DBENTRYP_ULP ) letoRecNo,
                                   ( DBENTRYP_I ) letoRecId,
                                   ( DBENTRYP_S ) letoSetFieldExtent,
                                   ( DBENTRYP_CP ) letoAlias,
                                   ( DBENTRYP_V ) letoClose,
                                   ( DBENTRYP_VO ) letoCreate,
                                   ( DBENTRYP_SI ) letoInfo,
                                   ( DBENTRYP_V ) letoNewArea,
                                   ( DBENTRYP_VO ) letoOpen,
                                   ( DBENTRYP_V ) letoRelease,
                                   ( DBENTRYP_SP ) letoStructSize,
                                   ( DBENTRYP_CP ) letoSysName,
                                   ( DBENTRYP_VEI ) letoEval,
                                   ( DBENTRYP_V ) letoPack,
                                   ( DBENTRYP_LSP ) letoPackRec,
                                   ( DBENTRYP_VS ) letoSort,
                                   ( DBENTRYP_VT ) letoTrans,
                                   ( DBENTRYP_VT ) letoTransRec,
                                   ( DBENTRYP_V ) letoZap,
                                   ( DBENTRYP_VR ) letoChildEnd,
                                   ( DBENTRYP_VR ) letoChildStart,
                                   ( DBENTRYP_VR ) letoChildSync,
                                   ( DBENTRYP_V ) letoSyncChildren,
                                   ( DBENTRYP_V ) letoClearRel,
                                   ( DBENTRYP_V ) letoForceRel,
                                   ( DBENTRYP_SSP ) letoRelArea,
                                   ( DBENTRYP_VR ) letoRelEval,
                                   ( DBENTRYP_SI ) letoRelText,
                                   ( DBENTRYP_VR ) letoSetRel,
                                   ( DBENTRYP_VOI ) letoOrderListAdd,
                                   ( DBENTRYP_V ) letoOrderListClear,
                                   ( DBENTRYP_VOI ) letoOrderListDelete,
                                   ( DBENTRYP_VOI ) letoOrderListFocus,
                                   ( DBENTRYP_V ) letoOrderListRebuild,
                                   ( DBENTRYP_VOO ) letoOrderCondition,
                                   ( DBENTRYP_VOC ) letoOrderCreate,
                                   ( DBENTRYP_VOI ) letoOrderDestroy,
                                   ( DBENTRYP_SVOI ) letoOrderInfo,
                                   ( DBENTRYP_V ) letoClearFilter,
                                   ( DBENTRYP_V ) letoClearLocate,
                                   ( DBENTRYP_V ) letoClearScope,
                                   ( DBENTRYP_VPLP ) letoCountScope,
                                   ( DBENTRYP_I ) letoFilterText,
                                   ( DBENTRYP_SI ) letoScopeInfo,
                                   ( DBENTRYP_VFI ) letoSetFilter,
                                   ( DBENTRYP_VLO ) letoSetLocate,
                                   ( DBENTRYP_VOS ) letoSetScope,
                                   ( DBENTRYP_VPL ) letoSkipScope,
                                   ( DBENTRYP_B ) letoLocate,
                                   ( DBENTRYP_CC ) letoCompile,
                                   ( DBENTRYP_I ) letoError,
                                   ( DBENTRYP_I ) letoEvalBlock,
                                   ( DBENTRYP_VSP ) letoRawLock,
                                   ( DBENTRYP_VL ) letoLock,
                                   ( DBENTRYP_I ) letoUnLock,
                                   ( DBENTRYP_V ) letoCloseMemFile,
                                   ( DBENTRYP_VO ) letoCreateMemFile,
                                   ( DBENTRYP_SCCS ) letoGetValueFile,
                                   ( DBENTRYP_VO ) letoOpenMemFile,
                                   ( DBENTRYP_SCCS ) letoPutValueFile,
#endif
                                   ( DBENTRYP_V ) letoReadDBHeader,
                                   ( DBENTRYP_V ) letoWriteDBHeader,
                                   ( DBENTRYP_R ) letoInit,
                                   ( DBENTRYP_R ) letoExit,
                                   ( DBENTRYP_RVVL ) letoDrop,
                                   ( DBENTRYP_RVVL ) letoExists,
#if ( __HARBOUR__ - 0 >= 0x020000 )
                                   ( DBENTRYP_RVVVL ) letoRename,
#endif
                                   ( DBENTRYP_RSLV ) letoRddInfo,
                                   ( DBENTRYP_SVP ) letoWhoCares
                                 };

static void letoRegisterRDD( USHORT * pusRddId )
{
   RDDFUNCS * pTable;
   USHORT * uiCount, uiRddId;

   uiCount = ( USHORT * ) hb_parptr( 1 );
   pTable = ( RDDFUNCS * ) hb_parptr( 2 );
   uiRddId = hb_parni( 4 );

   if( pTable )
   {
      ERRCODE errCode;

      if( uiCount )
         * uiCount = RDDFUNCSCOUNT;
      errCode = hb_rddInherit( pTable, &letoTable, &letoSuper, NULL );
      if ( errCode == SUCCESS )
      {
         /*
          * we successfully register our RDD so now we can initialize it
          * You may think that this place is RDD init statement, Druzus
          */
         *pusRddId = uiRddId;
         ++s_uiRddCount;
      }
      hb_retni( errCode );
   }
   else
   {
       hb_retni( FAILURE );
   }
}

HB_FUNC_STATIC( LETO_GETFUNCTABLE )
{
   HB_TRACE(HB_TR_DEBUG, ("LETO_GETFUNCTABLE()"));

   letoRegisterRDD( &s_uiRddIdLETO );
}

HB_FUNC( LETO ) { ; }

static ERRCODE letoUpdArea( AREAP pArea, void * p )
{
   HB_SYMBOL_UNUSED( p );

   if( pArea->rddID == s_uiRddIdLETO && ((LETOAREAP)pArea)->uiUpdated )
      leto_PutRec( (LETOAREAP)pArea );
   return 0;
}

static ERRCODE letoUnLockRec( AREAP pArea, void * p )
{
   HB_SYMBOL_UNUSED( p );

   if( pArea->rddID == s_uiRddIdLETO && ((LETOAREAP)pArea)->uiUpdated )
      ( (LETOAREAP)pArea )->fRecLocked = 0;
   return 0;
}

HB_FUNC( LETO_BEGINTRANSACTION )
{

   if( bTransActive )
   {
      commonError( (LETOAREAP) hb_rddGetCurrentWorkAreaPointer(), EG_SYNTAX, 1031, 0, NULL, 0, NULL );
   }
   else
   {
      hb_rddIterateWorkAreas( letoUpdArea, NULL );
      bTransActive = TRUE;
   }
}

HB_FUNC( LETO_ROLLBACK )
{

   if( !bTransActive )
   {
      commonError( (LETOAREAP) hb_rddGetCurrentWorkAreaPointer(), EG_SYNTAX, 1031, 0, NULL, 0, NULL );
   }
   else
   {
      bTransActive = FALSE;
      ulTransDataLen = uiRecsInTrans = 0;

      if( HB_ISNIL(1) || ( HB_ISLOG(1) && hb_parl(1) ) )
      {
         char szData[16], *pData;
         ULONG ulLen = 6;

         memcpy( szData+4, "ta;", 3 );
         szData[7] = szData[8] = '\0';
         szData[9] = (char) 0x41;
         pData = leto_AddLen( (szData+4), &ulLen, 1 );
         leto_SendRecv( (LETOAREAP) hb_rddGetCurrentWorkAreaPointer(), pData, ulLen, 1021 );
         hb_rddIterateWorkAreas( letoUnLockRec, NULL );
      }
   }
}

HB_FUNC( LETO_COMMITTRANSACTION )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   char * pData;
   ULONG ulLen;
   BOOL  bUnlockAll = (HB_ISLOG(1))? hb_parl(1) : TRUE;

   if( !bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      return;
   }

   hb_rddIterateWorkAreas( letoUpdArea, NULL );
   bTransActive = FALSE;

   if( szTransBuffer && ulTransDataLen > 10 )
   {
      ulLen = ulTransDataLen - 4;
      szTransBuffer[7] = (unsigned char)(uiRecsInTrans & 0xff);
      uiRecsInTrans = uiRecsInTrans >> 8;
      szTransBuffer[8] = (unsigned char)(uiRecsInTrans & 0xff);
      szTransBuffer[9] = (BYTE) ( (bUnlockAll)? 0x41 : 0x40 );
      pData = leto_AddLen( (char*) (szTransBuffer+4), &ulLen, 1 );

      if( bUnlockAll )
         hb_rddIterateWorkAreas( letoUnLockRec, NULL );

      if ( !leto_SendRecv( pArea, pData, ulLen, 1021 ) )
      {
         leto_writelog( pData, ulLen );
         hb_retl( 0 );
         return;
      }
   }
   hb_retl( 1 );
   ulTransDataLen = uiRecsInTrans = 0;
}

HB_FUNC( LETO_INTRANSACTION )
{
   hb_retl( bTransActive );
}

HB_FUNC( LETO_SUM )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   char szData[LETO_MAX_KEY*2+LETO_MAX_EXP+LETO_MAX_TAGNAME+35], *pData;
   char szKey[LETO_MAX_KEY+1];
   const char *szField, *szFilter;
   PHB_ITEM pScope;
   ULONG ulLen;
   USHORT uiKeyLen;

   if ( pArea && HB_ISCHAR( 1 ) )
   {
      szField = hb_parc( 1 );
      if( strchr(szField, ',') == NULL && ! hb_rddFieldIndex( (AREAP) pArea, szField ) )
      {
         hb_retni(0);
         return;
      }

      szFilter = HB_ISCHAR( 2 ) ? hb_parc( 2 ) : "";

      pData = szData + 4;
      sprintf( pData,"sum;%lu;%s;%s;%s;%c;", pArea->hTable,
         (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
         szField, szFilter,
         (char)( ( (hb_setGetDeleted())? 0x41 : 0x40 ) ) );
      pData = ( pData + strlen( pData ) );

      pScope = hb_param( 3, HB_IT_ANY );
      if( pScope )
      {
         uiKeyLen = letoKeyToStr( pArea, szKey, leto_ItemType( pScope ), pScope );
      }
      else
      {
         uiKeyLen = 0;
         szKey[0] = '\0';
      }
      *pData++ = (char) (uiKeyLen) & 0xFF;
      memcpy( pData, szKey, uiKeyLen );
      pData += uiKeyLen;

      pScope = hb_param( 4, HB_IT_ANY );
      if( pScope )
      {
         uiKeyLen = letoKeyToStr( pArea, szKey, leto_ItemType( pScope ), pScope );
      }
      else
      {
         uiKeyLen = 0;
         szKey[0] = '\0';
      }
      *pData++ = (char) (uiKeyLen) & 0xFF;
      memcpy( pData, szKey, uiKeyLen );
      pData += uiKeyLen;

      *pData = '\0';

      ulLen = pData - szData - 4;
      pData = leto_AddLen( (szData+4), &ulLen, 1 );
      if ( leto_SendRecv( pArea, pData, (ULONG) ulLen, 1021 ) )
      {
         int iWidth, iDec;
         BOOL fDbl;
         HB_MAXINT lValue;
         double dValue;

         pData = leto_firstchar();

         if( strchr(pData, ',') == NULL)
         {
            fDbl = hb_valStrnToNum( pData, strlen(pData), &lValue, &dValue , &iDec, &iWidth );
            if ( !fDbl )
               hb_retnintlen( lValue, iWidth );
            else
               hb_retnlen( dValue, iWidth, iDec );
         }
         else
         {
            PHB_ITEM pArray = hb_itemNew( NULL );
            PHB_ITEM pItem = hb_itemNew( NULL );
            char *ptr;
            int iLen;

            hb_arrayNew( pArray, 0);

            while(1)
            {
               ptr = strchr(pData, ',');
               iLen = (ptr ? ptr - pData : strlen(pData) );
               fDbl = hb_valStrnToNum( pData, iLen, &lValue, &dValue , &iDec, &iWidth );

               hb_arrayAddForward( pArray, (fDbl ? hb_itemPutNDLen( pItem, dValue, iWidth, iDec) : hb_itemPutNL( pItem, lValue ) ) );

               if( ! ptr)
                  break;
               pData = ptr + 1;
            }
            hb_itemRelease( pItem );
            hb_itemReturnForward( pArray );

         }

      }
      else
      {
         hb_retni(0);
      }

   }
   else
   {
      hb_retni(0);
   }
}

HB_FUNC( LETO_CONNECT_ERR )
{
   hb_retni( s_iConnectRes );
}

HB_FUNC( LETO_ISFLTOPTIM )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();

   if( leto_CheckArea( pArea ) )
#ifndef __BM
      hb_retl( pArea->area.dbfi.fOptimized );
#else
      hb_retl( pArea->area.dbfi.fOptimized || pArea->area.dbfi.lpvCargo );
#endif
   else
      hb_retl(0);
}

HB_FUNC( LETO_SETFASTAPPEND )
{
   BOOL b = ( HB_ISNIL(1) )? 1 : hb_parl( 1 );
   hb_retl( s_bFastAppend );
   s_bFastAppend = b;
}

HB_FUNC( LETO_CLOSEALL )
{
   LETOCONNECTION * pConnection = NULL;
   char szAddr[96];
   int iPort;

   if( HB_ISCHAR(1) )
   {
      if( leto_getIpFromPath( hb_parc(1), szAddr, &iPort, NULL, FALSE ) )
          pConnection = leto_ConnectionFind( szAddr, iPort );
   }
   else
      pConnection = pCurrentConn;
   if( pConnection )
   {
      pConnection->bCloseAll = 1;
      hb_rddCloseAll();
      pConnection->bCloseAll = 0;

      sprintf( szAddr,"close;00;\r\n" );
      if( leto_DataSendRecv( pConnection, szAddr, 0 ) )
         hb_retl( 1 );
      else
         hb_retl( 0 );
   }
   else
      hb_retl( 0 );
}


#define __PRG_SOURCE__ __FILE__

#ifdef HB_PCODE_VER
   #undef HB_PRG_PCODE_VER
   #define HB_PRG_PCODE_VER HB_PCODE_VER
#endif

static void hb_letoRddInit( void * cargo )
{
   HB_SYMBOL_UNUSED( cargo );

   // HB_LETO_SET_INIT();

   if( hb_rddRegister( "LETO", RDT_FULL ) > 1 )
   {
      hb_errInternal( HB_EI_RDDINVALID, NULL, NULL, NULL );
   }
}

HB_INIT_SYMBOLS_BEGIN( leto1__InitSymbols )
{ "LETO",                 {HB_FS_PUBLIC|HB_FS_LOCAL}, {HB_FUNCNAME( LETO )}, NULL },
{ "LETO_GETFUNCTABLE",    {HB_FS_PUBLIC|HB_FS_LOCAL}, {HB_FUNCNAME( LETO_GETFUNCTABLE )}, NULL },
HB_INIT_SYMBOLS_END( leto1__InitSymbols )

HB_CALL_ON_STARTUP_BEGIN( _hb_leto_rdd_init_ )
   hb_vmAtInit( hb_letoRddInit, NULL );
HB_CALL_ON_STARTUP_END( _hb_leto_rdd_init_ )

#if defined( HB_PRAGMA_STARTUP )
   #pragma startup leto1__InitSymbols
   #pragma startup _hb_leto_rdd_init_
#elif defined( HB_MSC_STARTUP )  // support for old Harbour version
   #if defined( HB_OS_WIN_64 )
      #pragma section( HB_MSC_START_SEGMENT, long, read )
   #endif
   #pragma data_seg( HB_MSC_START_SEGMENT )
   static HB_$INITSYM hb_vm_auto_leto1__InitSymbols = leto1__InitSymbols;
   static HB_$INITSYM hb_vm_auto_leto_rdd_init = _hb_leto_rdd_init_;
   #pragma data_seg()
#elif defined( HB_DATASEG_STARTUP )
   #define HB_DATASEG_BODY    HB_DATASEG_FUNC( leto1__InitSymbols ) \
                                 HB_DATASEG_FUNC( _hb_leto_rdd_init_ )
   #include "hbiniseg.h"
#endif
