/*  $Id: leto1.c,v 1.166.2.89 2013/12/27 11:22:03 alkresin Exp $  */

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

#define LETO_TIMEOUT      -1

typedef struct
{
   USHORT    uiConnection;
   ULONG     ulAreaID;
   LETOAREAP pArea;
} FINDAREASTRU;

static USHORT s_uiRddCount = 0;
static USHORT s_uiRddIdLETO = ( USHORT ) -1;
static RDDFUNCS letoSuper;

static PHB_ITEM s_pEvalItem = NULL;

extern USHORT uiConnCount;
extern LETOCONNECTION * letoConnPool;
extern LETOCONNECTION * pCurrentConn;

extern LETOCONNECTION * letoParseParam( const char * szParam, char * szFile );

void leto_writelog( const char * sFile, int n, const char * s, ... )
{
   char * sFileDef = "letodb.log";

   if( n )
   {
      HB_FHANDLE handle;
      if( hb_fsFile( (sFile)? sFile : sFileDef ) )
         handle = hb_fsOpen( (sFile)? sFile : sFileDef, FO_WRITE );
      else
         handle = hb_fsCreate( (sFile)? sFile : sFileDef, 0 );

      hb_fsSeek( handle,0, SEEK_END );
      hb_fsWrite( handle, s, (n) ? n : (int) strlen(s) );
      hb_fsWrite( handle, "\n\r", 2 );
      hb_fsClose( handle );
   }
   else
   {
      FILE * hFile = hb_fopen( (sFile)? sFile : sFileDef, "a" );

      va_list ap;
      if( hFile )
      {
         va_start( ap, s );
         vfprintf( hFile, s, ap );
         va_end( ap );
         fclose( hFile );
      }
   }
}

void funcEvalItem( void )
{
   if( s_pEvalItem )
      hb_vmEvalBlockOrMacro( s_pEvalItem );   
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

static int leto_SendRecv( LETOAREAP pArea, char * sData, ULONG ulLen, int iErr )
{
   long int lRet;
   char * ptr;

   lRet = leto_DataSendRecv( letoConnPool + pArea->pTable->uiConnection, sData, ulLen );
   if( !lRet )
      commonError( pArea, EG_DATATYPE, 1000, 0, NULL, 0, NULL );
   else if( *( ptr = leto_getRcvBuff() ) == '-' && iErr )
   {
      commonError( pArea, EG_DATATYPE, iErr, 0, ptr, 0, NULL );
      return 0;
   }
   return lRet;
}

BOOL leto_SetOptions( void )
{
   LetoSetCentury( (hb_setGetCentury())? 'T' : 'F' );
   LetoSetDateFormat( hb_setGetDateFormat() );
   LetoSetCdp( hb_cdp_page->id );
   LetoSetDeleted( (unsigned int)hb_setGetDeleted() );

   return 1;
}

void leto_SetAreaFlags( LETOAREAP pArea )
{
   pArea->area.fBof = pArea->pTable->fBof;
   pArea->area.fEof = pArea->pTable->fEof;
   pArea->area.fFound = pArea->pTable->fFound;
}

BOOL leto_CheckArea( LETOAREAP pArea )
{
   return pArea && ( (AREAP) pArea )->rddID == s_uiRddIdLETO;
}

BOOL leto_CheckAreaConn( AREAP pArea, LETOCONNECTION * pConnection )
{
   return leto_CheckArea( ( LETOAREAP ) pArea ) && ( letoConnPool + ( (LETOAREAP) pArea )->pTable->uiConnection == pConnection );
}

static ERRCODE leto_doClose( AREAP pArea, void * p )
{
   if( leto_CheckAreaConn( pArea, ( LETOCONNECTION * ) p ) )
   {
      ( ( LETOCONNECTION * ) p )->bCloseAll = 1;
      SELF_CLOSE( pArea );
   }
   return SUCCESS;
}

static int leto_CloseAll( LETOCONNECTION * pConnection )
{

   hb_rddIterateWorkAreas( leto_doClose, (void *) pConnection );

   if( pConnection->bCloseAll )
      return LetoCloseAll( pConnection );

   return 1;

}

void leto_ConnectionClose( LETOCONNECTION * pConnection )
{

   if( pConnection->pAddr )
   {
      leto_CloseAll( pConnection );
      LetoConnectionClose( pConnection );
   }

}

static int leto_ParseRec( LETOAREAP pArea, const char * szData, BOOL bCrypt )
{
   LETOTABLE * pTable = pArea->pTable;

   if( leto_ParseRecord( pTable, szData, (unsigned int) bCrypt ) )
   {
      leto_SetAreaFlags( pArea );
      return 1;
   }
   else
   {
      commonError( pArea, EG_DATATYPE, 1022, 0, NULL, 0, NULL );
      return 0;
   }

}

static BOOL leto_CheckError( LETOAREAP pArea )
{
   char * ptr = leto_getRcvBuff();
   if( *ptr == '-' )
   {
      ULONG uiGenCode, uiSubCode, uiOsCode, uiFlags;
      char * szFileName = NULL;

      if( ptr[4] == ':' )
      {
         sscanf( ptr+5, "%lu-%lu-%lu-%lu", &uiGenCode, &uiSubCode, &uiOsCode, &uiFlags );
         if( ( szFileName = strchr( ptr+5, '\t' ) ) != NULL)
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

static BOOL leto_checkLockError( LETOAREAP pArea )
{
   char * ptr = leto_getRcvBuff();
   if( *ptr == '-' )
   {
      if( *(ptr+3) != '4' )
         commonError( pArea, EG_DATATYPE, 1021, 0, ptr, 0, NULL );
      return TRUE;
   }
   return FALSE;
}

static void leto_ClearSeekBuf( LETOTAGINFO * pTagInfo )
{
   pTagInfo->Buffer.ulBufDataLen = 0;
   pTagInfo->uiRecInBuf = 0;
}

static void leto_ClearAllSeekBuf( LETOAREAP pArea )
{
   if( pArea->pTable->pTagInfo )
   {
      LETOTAGINFO * pTagInfo = pArea->pTable->pTagInfo;
      while( pTagInfo )
      {
         leto_ClearSeekBuf( pTagInfo );
         pTagInfo = pTagInfo->pNext;
      }
   }
}

static void leto_ClearBuffers( LETOAREAP pArea )
{
   LETOTABLE * pTable = pArea->pTable;
   if( ! pTable->ptrBuf )
      pTable->Buffer.ulBufDataLen = 0;
   else
      pTable->ptrBuf = NULL;
   leto_ClearAllSeekBuf( pArea );
}

static int leto_PutRec( LETOAREAP pArea, BOOL bCommit )
{

   leto_ClearAllSeekBuf( pArea );

   if( !LetoDbPutRecord( pArea->pTable, bCommit ) )
      return SUCCESS;
   else
   {
      if( LetoGetError() == 1000 )
         commonError( pArea, EG_DATATYPE, LetoGetError(), 0, NULL, 0, NULL );
      else
         leto_CheckError( pArea );
      return FAILURE;
   }
}

static void leto_CreateKeyExpr( LETOAREAP pArea, LETOTAGINFO * pTagInfo, const char * szData, PHB_ITEM pKeyExp )
{
   if( !pTagInfo->pExtra )
   {
      pTagInfo->pExtra = malloc( sizeof( LETOTAGEXTRAINFO ) );
      memset( pTagInfo->pExtra, 0, sizeof( LETOTAGEXTRAINFO ) );
   }
   if( strlen(szData) )
   {

      pTagInfo->nField = hb_rddFieldExpIndex( ( AREAP ) pArea, pTagInfo->KeyExpr );

      if( pKeyExp )
      {
         ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pKeyItem = hb_itemNew( pKeyExp );
      }
      else if ( SELF_COMPILE( ( AREAP ) pArea, pTagInfo->KeyExpr ) != FAILURE )
      {
         ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pKeyItem = pArea->area.valResult;
         pArea->area.valResult = NULL;
      }
   }
}

static void ScanIndexFields( LETOAREAP pArea, LETOTAGINFO * pTagInfo )
{

   USHORT uiCount = pArea->area.uiFieldCount, uiIndex, uiFound = 0;
   char *szKey = hb_strdup( pTagInfo->KeyExpr ), *pKey, *ptr;
   char szName[12];
   BOOL bFound;
   unsigned int uiLen;
   LETOTAGEXTRAINFO * pExtra = (LETOTAGEXTRAINFO *)pTagInfo->pExtra ;

   pExtra->puiFields = (USHORT *) malloc( uiCount*sizeof( USHORT ) );
   hb_strUpper( szKey, strlen(szKey ) );

   for( uiIndex = 0; uiIndex < uiCount; uiIndex ++ )
   {
      bFound = FALSE;
      szName[ 0 ] = '\0';
      SELF_FIELDNAME( (AREAP) pArea, uiIndex + 1, szName );
      uiLen = strlen( szName );
      pKey = szKey;
      while( strlen( pKey ) >= uiLen && ( ptr = strstr(pKey, szName) ) != NULL )
      {
         if( ( (ptr==szKey) || !HB_ISNEXTIDCHAR( *(ptr-1) ) ) && !HB_ISNEXTIDCHAR( *(ptr+uiLen) ) )
         {
            bFound = TRUE;
            break;
         }
         pKey = ptr + strlen(szName);
      }
      if( bFound )
         pExtra->puiFields[ uiFound ++ ] = uiIndex + 1;
   }

   hb_xfree( szKey );

   if( !uiFound )
   {
      free( pExtra->puiFields );
      pExtra->puiFields = NULL;
   }
   else if( uiFound < uiCount )
      pExtra->puiFields = (USHORT *) realloc( pExtra->puiFields, uiFound*sizeof( USHORT ) );
   pExtra->uiFCount = uiFound;
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
   LETOTABLE * pTable = pArea->pTable;

   HB_TRACE(HB_TR_DEBUG, ("letoGoBottom(%p)", pArea));

   pArea->lpdbPendingRel = NULL;
   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   LetoSetDeleted( (unsigned int) hb_setGetDeleted() );
   if( LetoDbGoBottom( pTable, ( (pArea->pTagCurrent)? pArea->pTagCurrent->TagName : NULL ) ) )
      return FAILURE;
   else
   {
      leto_SetAreaFlags( pArea );

      SELF_SKIPFILTER( ( AREAP ) pArea, -1 );

      if( pArea->area.lpdbRelations )
         return SELF_SYNCCHILDREN( ( AREAP ) pArea );
      else
         return SUCCESS;
   }
}

static void leto_GotoEof( LETOAREAP pArea )
{

   LetoDbGotoEof( pArea->pTable );
   pArea->area.fEof = pArea->area.fBof = TRUE;
   pArea->area.fFound = FALSE;
}

static ERRCODE letoGoTo( LETOAREAP pArea, ULONG ulRecNo )
{
   LETOTABLE * pTable = pArea->pTable;
   HB_TRACE(HB_TR_DEBUG, ("letoGoTo(%p, %lu)", pArea, ulRecNo));

   pArea->lpdbPendingRel = NULL;
   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   LetoSetDeleted( (unsigned int) hb_setGetDeleted() );
   if( LetoDbGoTo( pTable, (unsigned long) ulRecNo,
          ( (pArea->pTagCurrent)? pArea->pTagCurrent->TagName : NULL ) ) )
      return FAILURE;
   else
   {
      leto_SetAreaFlags( pArea );
      if( pArea->area.lpdbRelations )
         return SELF_SYNCCHILDREN( ( AREAP ) pArea );
      else
         return SUCCESS;
   }
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
   LETOTABLE * pTable = pArea->pTable;

   HB_TRACE(HB_TR_DEBUG, ("letoGoTop(%p)", pArea));

   pArea->lpdbPendingRel = NULL;
   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   LetoSetDeleted( (unsigned int) hb_setGetDeleted() );
   if( LetoDbGoTop( pTable, ( (pArea->pTagCurrent)? pArea->pTagCurrent->TagName : NULL ) ) )
      return FAILURE;
   else
   {
      leto_SetAreaFlags( pArea );

      SELF_SKIPFILTER( ( AREAP ) pArea, 1 );

      if( pArea->area.lpdbRelations )
         return SELF_SYNCCHILDREN( ( AREAP ) pArea );
      else
         return SUCCESS;
   }
}

static USHORT leto_KeyToStr( LETOAREAP pArea, char * szKey, char cType, PHB_ITEM pKey )
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

static char * AddKeyToBuf( char * szData, char * szKey, USHORT uiKeyLen, ULONG *pulLen )
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

static PHB_ITEM leto_KeyEval( LETOAREAP pArea, LETOTAGINFO * pTagInfo )
{
   PHB_ITEM pItem;
   if( pTagInfo->nField )
   {
      pItem = hb_stackReturnItem();
      SELF_GETVALUE( ( AREAP ) pArea, pTagInfo->nField, pItem );
   }
   else
      pItem = hb_vmEvalBlockOrMacro( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pKeyItem );
   return pItem;
}

static ERRCODE letoSeek( LETOAREAP pArea, BOOL bSoftSeek, PHB_ITEM pKey, BOOL bFindLast )
{
   LETOTABLE * pTable = pArea->pTable;
   char szKey[LETO_MAX_KEY+1];
   char szData[LETO_MAX_KEY+LETO_MAX_TAGNAME+56], * pData, cType;
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;
   USHORT uiKeyLen;
   ULONG ulLen, ulRecNo;
   LETOTAGINFO * pTagInfo = pArea->pTagCurrent;
   BOOL bSeekBuf = FALSE;
   BOOL bSeekLeto = TRUE;

   pArea->lpdbPendingRel = NULL;
   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   if( ! pTagInfo )
   {
      commonError( pArea, EG_NOORDER, 1201, 0, NULL, EF_CANDEFAULT, NULL );
      return FAILURE;
   }

   if( ( cType = leto_ItemType( pKey ) ) != pTagInfo->KeyType )
   {
      sprintf( szData,"goto;%lu;-3;%s;%c;\r\n", pTable->hTable,
           (pTagInfo)? pTagInfo->TagName:"",
           (char)( (hb_setGetDeleted())? 0x41 : 0x40 ) );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
   }
   else
   {
      uiKeyLen = leto_KeyToStr( pArea, szKey, cType, pKey );

      bSeekBuf = pTagInfo->uiBufSize && ! bSoftSeek && ! bFindLast &&
          ( cType != 'C' || uiKeyLen == (USHORT)pTagInfo->KeySize );

      if( bSeekBuf && pTagInfo->Buffer.ulBufDataLen && leto_HotBuffer( pTable, &pTagInfo->Buffer, pConnection->uiBufRefreshTime ) )
      {
         char *ptr = (char *) pTagInfo->Buffer.pBuffer;
         USHORT uiKeyLenBuf;

         for( ;; )
         {
            uiKeyLenBuf = ((unsigned char)*ptr) & 0xFF;
            ptr ++;

            if( ( uiKeyLen == uiKeyLenBuf ) && !memcmp( szKey, ptr, uiKeyLen) )
            {
               ptr += uiKeyLenBuf;
               if( HB_GET_LE_UINT24( ptr ) != 0 )
                  leto_ParseRec( pArea, ptr, TRUE );
               else
                  leto_GotoEof( pArea );
               bSeekBuf = FALSE;
               bSeekLeto = FALSE;
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
            (char)( ( (hb_setGetDeleted())? 0x41 : 0x40 ) |
            ( (bSoftSeek)? 0x10 : 0 ) | ( (bFindLast)? 0x20 : 0 ) ) );
         pData = AddKeyToBuf( pData, szKey, uiKeyLen, &ulLen );
         if ( !leto_SendRecv( pArea, pData, (ULONG) ulLen, 1021 ) ) return FAILURE;
      }
   }

   if( bSeekLeto )
   {
      pData = leto_firstchar();
      leto_ParseRec( pArea, pData, TRUE );
      ulRecNo = pTable->ulRecNo;
   }
   pTable->ptrBuf = NULL;

   if( pArea->area.dbfi.itmCobExpr != NULL && !pArea->area.dbfi.fOptimized )
   {
      if( SELF_SKIPFILTER( ( AREAP ) pArea, ( bFindLast ? -1 : 1 ) ) == SUCCESS )
      {
         PHB_ITEM pItem = leto_KeyEval( pArea, pTagInfo );

         if( itmCompare( pItem, pKey, FALSE ) != 0 && ! bSoftSeek )
            SELF_GOTO( ( AREAP ) pArea, 0 );
      }
   }

   if( bSeekLeto && !pArea->area.fEof && ( ulRecNo == pTable->ulRecNo ) )
   {
      leto_setSkipBuf( pTable, pData, 0, 0 );
   }

   if( bSeekBuf && (!pArea->area.fEof || ( !pConnection->bRefreshCount && pTable->ulRecCount ) ) )
   {
      ULONG ulRecLen = (!pArea->area.fEof ? HB_GET_LE_UINT24( pData ) : 0 ) + 3;
      ULONG ulDataLen = ulRecLen + uiKeyLen + 1;
      BYTE * ptr;

      if( !leto_HotBuffer( pTable, &pTagInfo->Buffer, pConnection->uiBufRefreshTime ) ||
          ( pTagInfo->uiRecInBuf >= pTagInfo->uiBufSize ) )
      {
         leto_ClearSeekBuf( pTagInfo );
      }

      leto_AllocBuf( &pTagInfo->Buffer, pTagInfo->Buffer.ulBufDataLen + ulDataLen, ulDataLen*5 );

      ptr = pTagInfo->Buffer.pBuffer + pTagInfo->Buffer.ulBufDataLen - ulDataLen;
      *ptr = (uiKeyLen & 0xFF);

      memcpy( ptr + 1, szKey, uiKeyLen );
      if( !pArea->area.fEof )
         memcpy( ptr + 1 + uiKeyLen, pData, ulRecLen );
      else
         HB_PUT_LE_UINT24( ptr + 1 + uiKeyLen, 0 );
         
      pTagInfo->uiRecInBuf ++;
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
   LETOTABLE * pTable = pArea->pTable;
   BYTE * ptrBuf = NULL;

   HB_TRACE(HB_TR_DEBUG, ("letoSkip(%p, %ld)", pArea, lToSkip));

   if( pTable->uiUpdated )
   {
      leto_PutRec( pArea, FALSE );
      ptrBuf = pTable->ptrBuf;
   }

   LetoSetDeleted( (unsigned int) hb_setGetDeleted() );
   if( !lToSkip )
   {
      if( ptrBuf )
         pTable->ptrBuf = NULL;
   }
   else if( LetoDbSkip( pTable, lToSkip, ( (pArea->pTagCurrent)? pArea->pTagCurrent->TagName : NULL ) ) )
   {
      commonError( pArea, EG_DATATYPE, LetoGetError(), 0, NULL, 0, NULL );
      return FAILURE;
   }
   else
      leto_SetAreaFlags( pArea );

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   else
      return SUCCESS;
}

static ERRCODE letoAddField( LETOAREAP pArea, LPDBFIELDINFO pFieldInfo )
{
   HB_TRACE(HB_TR_DEBUG, ("hb_dbfAddField(%p, %p)", pArea, pFieldInfo));

   /* Update field offset */
   //pTable->pFieldOffset[ pArea->area.uiFieldCount ] = pTable->uiRecordLen;
   //pTable->uiRecordLen += pFieldInfo->uiLen;
   return SUPER_ADDFIELD( ( AREAP ) pArea, pFieldInfo );
}

static ERRCODE letoAppend( LETOAREAP pArea, BOOL fUnLockAll )
{

   LETOTABLE * pTable = pArea->pTable;

   HB_TRACE(HB_TR_DEBUG, ("letoAppend(%p, %d)", pArea, (int) fUnLockAll));

   if( pTable->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }

   pArea->lpdbPendingRel = NULL;

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   pArea->area.fBof = pArea->area.fEof = pArea->area.fFound = 0;

   if( LetoDbAppend( pTable, fUnLockAll ) )
      return FAILURE;

   if( pArea->area.lpdbRelations )
      return SELF_SYNCCHILDREN( ( AREAP ) pArea );
   return SUCCESS;
}

#define  letoCreateFields          NULL

static ERRCODE letoDeleteRec( LETOAREAP pArea )
{

   LETOTABLE * pTable = pArea->pTable;

   HB_TRACE(HB_TR_DEBUG, ("letoDeleteRec(%p)", pArea));

   if( pTable->fShared && !pArea->fFLocked && !pTable->fRecLocked )
   {
      commonError( pArea, EG_UNLOCKED, EDBF_UNLOCKED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   else if( pTable->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }

//   sprintf( szData,"del;01;%d;%lu;\r\n", pTable->hTable, pTable->ulRecNo );
//   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   if( ( pTable->uiUpdated & 4 ) || !pTable->fDeleted )
   {
      pTable->fDeleted = 1;
      pTable->uiUpdated |= 4;
   }
   return SUCCESS;
}

static ERRCODE letoDeleted( LETOAREAP pArea, BOOL * pDeleted )
{
   HB_TRACE(HB_TR_DEBUG, ("letoDeleted(%p, %p)", pArea, pDeleted));

   *pDeleted = pArea->pTable->fDeleted;
   return SUCCESS;
}

#define  letoFieldCount            NULL
#define  letoFieldDisplay          NULL
#define  letoFieldInfo             NULL
#define  letoFieldName             NULL

static ERRCODE letoFlush( LETOAREAP pArea )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[32];
   HB_TRACE(HB_TR_DEBUG, ("letoFlush(%p)", pArea ));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   if( !letoConnPool[pTable->uiConnection].bTransActive )
   {
      sprintf( szData,"flush;%lu;\r\n", pTable->hTable );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
   }

   return SUCCESS;
}

static ERRCODE letoGetRec( LETOAREAP pArea, BYTE ** pBuffer )
{
   if( pArea->lpdbPendingRel )
   {
      if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
         return FAILURE;
   }
   if( pBuffer != NULL )
   {
      *pBuffer = pArea->pTable->pRecord;
   }
   return SUCCESS;
}

static ERRCODE leto_GetMemoValue( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem, USHORT uiType )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[32], *ptr;
   ULONG ulLen;

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"memo;%lu;get;%lu;%d;\r\n", pTable->hTable, pTable->ulRecNo, uiIndex+1 );
   else
      sprintf( szData,"memo;get;%lu;%lu;%d;\r\n", pTable->hTable, pTable->ulRecNo, uiIndex+1 );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) )
   {
      return FAILURE;
   }

   ptr = leto_DecryptText( letoConnPool + pTable->uiConnection, &ulLen );
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
   LETOTABLE * pTable = pArea->pTable;
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
            memcpy( pVal, pTable->pRecord + pTable->pFieldOffset[uiIndex], pField->uiLen );
            pVal[ pField->uiLen ] = '\0';
            hb_cdpnTranslate( pVal, pArea->area.cdPage, hb_cdp_page, pField->uiLen );
            hb_itemPutCL( pItem, pVal, pField->uiLen );
            hb_xfree( pVal );
#else
            char * pVal;
            HB_SIZE uiKeyLen = pField->uiLen;
            pVal = hb_cdpnDup( ( const char * ) pTable->pRecord + pTable->pFieldOffset[uiIndex], &uiKeyLen, pArea->area.cdPage, hb_cdp_page );
            hb_itemPutCLPtr( pItem, pVal, pField->uiLen );
#endif
         }
         else
            hb_itemPutCL( pItem, (char*) pTable->pRecord + pTable->pFieldOffset[uiIndex],
                       pField->uiLen );
         break;

      case HB_FT_LONG:
      case HB_FT_FLOAT:
      {
         HB_MAXINT lVal;
         double dVal;
         BOOL fDbl;

         fDbl = hb_strnToNum( (const char *) pTable->pRecord + pTable->pFieldOffset[ uiIndex ],
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
               hb_itemPutNILen( pItem, ( int ) HB_GET_LE_INT16( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ), 6 );
               break;
            case 4:
               hb_itemPutNIntLen( pItem, ( HB_LONG ) HB_GET_LE_INT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ), 10 );
               break;
            case 8:
#ifndef HB_LONG_LONG_OFF
               hb_itemPutNIntLen( pItem, ( HB_LONG ) HB_GET_LE_INT64( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ), 20 );
#else
               hb_itemPutNLen( pItem, ( double ) HB_GET_LE_INT64( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ), 20, 0 );
#endif
               break;
         }
         break;
      }
      case HB_FT_DOUBLE:
      {
         hb_itemPutNDLen( pItem, HB_GET_LE_DOUBLE( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ),
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
            HB_GET_LE_UINT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ),
            HB_GET_LE_UINT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] + 4 ) );
#else
         hb_itemPutTDT( pItem,
            HB_GET_LE_UINT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ),
            HB_GET_LE_UINT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] + 4 ) );
#endif
         break;
      }
      case HB_FT_DATE:
      {
         if( pField->uiLen == 3 )
         {
            hb_itemPutDL( pItem, HB_GET_LE_UINT24( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ) );
         }
         else if( pField->uiLen == 4 )
         {
            hb_itemPutDL( pItem, HB_GET_LE_UINT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ) );
         }
         else
         {
            char szBuffer[ 9 ];
            memcpy( szBuffer, pTable->pRecord + pTable->pFieldOffset[ uiIndex ], 8 );
            szBuffer[ 8 ] = 0;
            hb_itemPutDS( pItem, szBuffer );
            // hb_itemPutDS( pItem, ( char * ) pTable->pRecord + pTable->pFieldOffset[ uiIndex ] );
         }
         break;
      }
      case HB_FT_LOGICAL:
         hb_itemPutL( pItem, pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] == 'T' ||
                      pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] == 't' ||
                      pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] == 'Y' ||
                      pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] == 'y' );
         break;

      case HB_FT_MEMO:
      case HB_FT_BLOB:
      case HB_FT_PICTURE:
      case HB_FT_OLE:
      {
         if( ( pTable->uiUpdated & 1 ) || pArea->area.fEof || ( pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] == ' ' ) )
            hb_itemPutC( pItem, "" );
         else
         {
            if( leto_GetMemoValue( pArea, uiIndex, pItem, pField->uiType ) == FAILURE ) 
            {
               return FAILURE;
            }
         }
         hb_itemSetCMemo( pItem );
         break;
      }
      case HB_FT_ANY:
      {
         char * pData = ( char * ) pTable->pRecord + pTable->pFieldOffset[ uiIndex ];
         if( pField->uiLen == 3 )
         {
            hb_itemPutDL( pItem, HB_GET_LE_UINT24( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] ) );
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
                  leto_GetMemoValue( pArea, uiIndex, pItem, HB_FT_MEMO );
                  break;
               }
               default:
                  hb_itemPutCL( pItem, "", 0 );
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

static ERRCODE letoPutRec( LETOAREAP pArea, BYTE * pBuffer )
{
   LETOTABLE * pTable = pArea->pTable;

   if( pTable->fShared && !pArea->fFLocked && !pTable->fRecLocked )
   {
      commonError( pArea, EG_UNLOCKED, EDBF_UNLOCKED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   else if( pTable->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pBuffer != NULL )
   {
      if( pArea->lpdbPendingRel )
      {
         if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
            return FAILURE;
      }
      memcpy( pTable->pRecord, pBuffer, pTable->uiRecordLen );
      pTable->uiUpdated |= 2;
      memset( pTable->pFieldUpd, 1, pArea->area.uiFieldCount * sizeof( USHORT ) );
   }
   return SUCCESS;
}

static ERRCODE leto_PutMemoValue( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem, USHORT uiType )
{
   LETOTABLE * pTable = pArea->pTable;
   char *szData, *ptr;
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
   ULONG ulLenMemo = hb_itemGetCLen( pItem );
#else
   HB_SIZE ulLenMemo = hb_itemGetCLen( pItem );
   char * pBuff;
#endif
   ULONG ulLen;
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;

   if( pTable->uiUpdated && !pConnection->bTransActive )
      leto_PutRec( pArea, FALSE );

   pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] = (ulLenMemo)? '!' : ' ';
   ulLen = ulLenMemo;
   szData = (char*) hb_xgrab( ulLen + 36 );

   if( ( pTable->uiUpdated & 1 ) && pConnection->bTransActive )
   {
      pTable->uiUpdated &= ( !1 );
      if( LetoCheckServerVer( pConnection, 100 ) )
         sprintf( szData+4,"memo;%lu;add;%lu;%d;", pTable->hTable, pTable->ulRecNo, uiIndex+1 );
      else
         sprintf( szData+4,"memo;add;%lu;%lu;%d;", pTable->hTable, pTable->ulRecNo, uiIndex+1 );
   }
   else
   {
      if( LetoCheckServerVer( pConnection, 100 ) )
         sprintf( szData+4,"memo;%lu;put;%lu;%d;", pTable->hTable, pTable->ulRecNo, uiIndex+1 );
      else
         sprintf( szData+4,"memo;put;%lu;%lu;%d;", pTable->hTable, pTable->ulRecNo, uiIndex+1 );
   }
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

   if( pConnection->bTransActive )
   {
      leto_AddTransBuffer( pConnection, ptr, ulLen );
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
   LETOTABLE * pTable = pArea->pTable;
   LPFIELD pField;
   char szBuf[256];
   BOOL bTypeError = 0;
#if !defined (__XHARBOUR__) && defined(__HARBOUR__) && ( (__HARBOUR__ - 0) >= 0x020000 )
   char * pBuff;
#endif

   HB_TRACE(HB_TR_DEBUG, ("letoPutValue(%p, %hu, %p)", pArea, uiIndex, pItem));

   if( !uiIndex || uiIndex > pArea->area.uiFieldCount )
      return FAILURE;

   if( pTable->fShared && !pArea->fFLocked && !pTable->fRecLocked )
   {
      commonError( pArea, EG_UNLOCKED, EDBF_UNLOCKED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   else if( pTable->fReadonly )
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
            memcpy( pTable->pRecord + pTable->pFieldOffset[uiIndex],
                    hb_itemGetCPtr( pItem ), uiSize );
            if( pArea->area.cdPage != hb_cdp_page )
               hb_cdpnTranslate( (char *) pTable->pRecord + pTable->pFieldOffset[ uiIndex ], hb_cdp_page, pArea->area.cdPage, uiSize );
            memset( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] + uiSize,
                    ' ', pField->uiLen - uiSize );
#else
            HB_SIZE ulLen1 = hb_itemGetCLen( pItem );
            if( pArea->area.cdPage != hb_cdp_page )
            {
               pBuff = hb_cdpnDup( hb_itemGetCPtr( pItem ), &ulLen1, hb_cdp_page, pArea->area.cdPage );
               if( ulLen1 > (HB_SIZE)pField->uiLen )
                  ulLen1 = pField->uiLen;
               memcpy( pTable->pRecord + pTable->pFieldOffset[uiIndex], pBuff, ulLen1 );
               hb_xfree( pBuff );
            }
            else
            {
               if( ulLen1 > (HB_SIZE)pField->uiLen )
                  ulLen1 = pField->uiLen;
               memcpy( pTable->pRecord + pTable->pFieldOffset[uiIndex],
                      hb_itemGetCPtr( pItem ), ulLen1 );
            }
            memset( pTable->pRecord + pTable->pFieldOffset[ uiIndex ] + ulLen1,
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
               memcpy( pTable->pRecord + pTable->pFieldOffset[ uiIndex ],
                       szBuf, pField->uiLen );
            }
            else
            {
               memset( pTable->pRecord + pTable->pFieldOffset[ uiIndex ],
                       '*', pField->uiLen );
               commonError( pArea, EG_DATAWIDTH, EDBF_DATAWIDTH, 0, NULL, 0, hb_dynsymName( ( PHB_DYNS ) pField->sym ) );
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
               memset( pTable->pRecord + pTable->pFieldOffset[ uiIndex ],
                       '*', pField->uiLen );
               commonError( pArea, EG_DATAWIDTH, EDBF_DATAWIDTH, 0, NULL, 0, hb_dynsymName( ( PHB_DYNS ) pField->sym ) );
               return FAILURE;
            }
            else
            {
               switch( pField->uiLen )
               {
                  case 1:
                     pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] = ( signed char ) lVal;
                     break;
                  case 2:
                     HB_PUT_LE_UINT16( pTable->pRecord + pTable->pFieldOffset[ uiIndex ], ( unsigned short ) lVal );
                     break;
                  case 3:
                     HB_PUT_LE_UINT24( pTable->pRecord + pTable->pFieldOffset[ uiIndex ], ( UINT32 ) lVal );
                     break;
                  case 4:
                     HB_PUT_LE_UINT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ], ( UINT32 ) lVal );
                     break;
                  case 8:
#ifndef HB_LONG_LONG_OFF
                     HB_PUT_LE_UINT64( pTable->pRecord + pTable->pFieldOffset[ uiIndex ], ( UINT64 ) lVal );
#else
                     HB_PUT_LE_UINT64( pTable->pRecord + pTable->pFieldOffset[ uiIndex ], dVal );
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
            HB_PUT_LE_DOUBLE( pTable->pRecord + pTable->pFieldOffset[ uiIndex ],
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
               HB_PUT_LE_UINT24( pTable->pRecord + pTable->pFieldOffset[ uiIndex ],
                                 hb_itemGetDL( pItem ) );
            }
            else if( pField->uiLen == 4 )
            {
               HB_PUT_LE_UINT32( pTable->pRecord + pTable->pFieldOffset[ uiIndex ],
                                 hb_itemGetDL( pItem ) );
            }
            else
            {
               hb_itemGetDS( pItem, szBuf );
               memcpy( pTable->pRecord + pTable->pFieldOffset[ uiIndex ], szBuf, 8 );
            }
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_DATETIME:
      case HB_FT_DAYTIME:
      case HB_FT_MODTIME:
      {
         BYTE * ptr = pTable->pRecord + pTable->pFieldOffset[ uiIndex ];
#ifdef __XHARBOUR__
         if( HB_IS_DATE( pItem ) )
         {
            HB_PUT_LE_UINT32( ptr, hb_itemGetDL( pItem ) );
            HB_PUT_LE_UINT32( ptr + 4, hb_itemGetT( pItem ) );
         }
#else
         if( HB_IS_DATETIME( pItem ) )
         {
            long lDate, lTime;

            hb_itemGetTDT( pItem, &lDate, &lTime );
            HB_PUT_LE_UINT32( ptr, lDate );
            HB_PUT_LE_UINT32( ptr + 4, lTime );
         }
         else if( HB_IS_STRING( pItem ) )
         {
            LONG lJulian, lMillisec;
            hb_timeStampStrGetDT( hb_itemGetCPtr( pItem ), &lJulian, &lMillisec );
            HB_PUT_LE_UINT32( ptr, lJulian );
            HB_PUT_LE_UINT32( ptr + 4, lMillisec );
         }
#endif
         else
            bTypeError = 1;
         break;
      }
      case HB_FT_LOGICAL:
         if( HB_IS_LOGICAL( pItem ) )
         {
            pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] = hb_itemGetL( pItem ) ? 'T' : 'F';
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
            if( ( pTable->pRecord[ pTable->pFieldOffset[ uiIndex ] ] == ' ' ) && ( hb_itemGetCLen( pItem ) == 0 ) )
               return SUCCESS;
            else
               return leto_PutMemoValue( pArea, uiIndex, pItem, pField->uiType );
         }
         else
            bTypeError = 1;
         break;

      case HB_FT_ANY:
      {
         char * pData = ( char * ) pTable->pRecord + pTable->pFieldOffset[ uiIndex ];
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
               return leto_PutMemoValue( pArea, uiIndex, pItem, HB_FT_MEMO );
            }
         }
         break;
      }
   }

   if( bTypeError )
   {
      commonError( pArea, EG_DATATYPE, 1020, 0, NULL, 0, hb_dynsymName( ( PHB_DYNS ) pField->sym ) );
      return FAILURE;
   }

   pTable->uiUpdated |= 2;
   *(pTable->pFieldUpd+uiIndex) = 1;

   return SUCCESS;
}

static ERRCODE letoRecall( LETOAREAP pArea )
{
   LETOTABLE * pTable = pArea->pTable;

   HB_TRACE(HB_TR_DEBUG, ("letoRecall(%p)", pArea));

   if( pTable->fShared && !pArea->fFLocked && !pTable->fRecLocked )
   {
      commonError( pArea, EG_UNLOCKED, EDBF_UNLOCKED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   else if( pTable->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }

//   sprintf( szData,"del;02;%d;%lu;\r\n", pTable->hTable, pTable->ulRecNo );
//   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   if( ( pTable->uiUpdated & 4 ) || pTable->fDeleted )
   {
      pTable->fDeleted = 0;
      pTable->uiUpdated |= 4;
   }
   return SUCCESS;
}

static ERRCODE letoRecCount( LETOAREAP pArea, ULONG * pRecCount )
{
   HB_TRACE(HB_TR_DEBUG, ("letoRecCount(%p, %p)", pArea, pRecCount));

   if( LetoDbRecCount( pArea->pTable, (unsigned long *) pRecCount ) )
      return FAILURE;
   else
      return SUCCESS;
}

static BOOL leto_IsRecLocked( LETOAREAP pArea, ULONG ulRecNo )
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

static ERRCODE letoRecInfo( LETOAREAP pArea, PHB_ITEM pRecID, USHORT uiInfoType, PHB_ITEM pInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   ULONG ulRecNo = hb_itemGetNL( pRecID ), ulPrevRec = 0;
   ERRCODE uiRetVal = SUCCESS;

   HB_TRACE(HB_TR_DEBUG, ("letoRecInfo(%p, %p, %hu, %p)", pArea, pRecID, uiInfoType, pInfo));

   if( pArea->lpdbPendingRel )
   {
      if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
         return FAILURE;
   }

   if( ulRecNo == 0 )
   {
      ulRecNo = pTable->ulRecNo;
   }
   else if( ulRecNo != pTable->ulRecNo )
   {
      switch( uiInfoType )
      {
         case DBRI_DELETED:
         case DBRI_RAWRECORD:
         case DBRI_RAWMEMOS:
         case DBRI_RAWDATA:
            ulPrevRec = pTable->ulRecNo;
            uiRetVal = SELF_GOTO( ( AREAP ) pArea, ulRecNo );
            if( uiRetVal != SUCCESS )
               return uiRetVal;
            break;
      }
   }

   switch( uiInfoType )
   {
      case DBRI_DELETED:
      {
         hb_itemPutL( pInfo, pTable->fDeleted );
         break;
      }
      case DBRI_LOCKED:
      {
         if( ulRecNo != 0 && ulRecNo != pTable->ulRecNo )
         {
            if( !pTable->fShared || pArea->fFLocked || leto_IsRecLocked( pArea, ulRecNo ) )
            {
               hb_itemPutL( pInfo, TRUE );
            }
            else
            {
               char szData[32], * ptr;

               sprintf( szData,"islock;%lu;%lu;\r\n", pTable->hTable, ulRecNo );
               if ( !leto_SendRecv( pArea, szData, 0, 1021 ) || leto_checkLockError( pArea ) ) return FAILURE;
               ptr = leto_firstchar();
               LetoGetCmdItem( &ptr, szData ); ptr ++;
               hb_itemPutL( pInfo, szData[0] == 'T' );
            }
         }
         else
         {
            hb_itemPutL( pInfo, pTable->fRecLocked );
         }
         break;
      }
      case DBRI_RECSIZE:
         hb_itemPutNL( pInfo, pTable->uiRecordLen );
         break;

      case DBRI_RECNO:
         if( ulRecNo == 0 )
            uiRetVal = SELF_RECNO( ( AREAP ) pArea, &ulRecNo );
         hb_itemPutNL( pInfo, ulRecNo );
         break;

      case DBRI_UPDATED:
         hb_itemPutL( pInfo, ulRecNo == pTable->ulRecNo && pTable->uiUpdated );
         break;

      case DBRI_RAWRECORD:
         hb_itemPutCL( pInfo, ( char * ) pTable->pRecord, pTable->uiRecordLen );
         break;

      case DBRI_RAWMEMOS:
      case DBRI_RAWDATA:
      {
         USHORT uiFields;
         BYTE *pResult;
         ULONG ulLength, ulLen;

         ulLength = uiInfoType == DBRI_RAWDATA ? pTable->uiRecordLen : 0;
         pResult = ( BYTE * ) hb_xgrab( ulLength + 1 );
         if( ulLength )
         {
            memcpy( pResult, pTable->pRecord, ulLength );
         }

         if( pTable->uiMemoVersion )
         {
            for( uiFields = 0; uiFields < pArea->area.uiFieldCount; uiFields++ )
            {
               if( pArea->area.lpFields[ uiFields ].uiType == HB_FT_MEMO ||
                   pArea->area.lpFields[ uiFields ].uiType == HB_FT_PICTURE ||
                   pArea->area.lpFields[ uiFields ].uiType == HB_FT_BLOB ||
                   pArea->area.lpFields[ uiFields ].uiType == HB_FT_OLE )
               {
                  uiRetVal = SELF_GETVALUE( ( AREAP ) pArea, uiFields + 1, pInfo );
                  if( uiRetVal != SUCCESS )
                     break;
                  ulLen = hb_itemGetCLen( pInfo );
                  if( ulLen > 0 )
                  {
                     pResult = ( BYTE * ) hb_xrealloc( pResult, ulLength + ulLen + 1 );
                     memcpy( pResult + ulLength, hb_itemGetCPtr( pInfo ), ulLen );
                     ulLength += ulLen;
                  }
               }
            }
         }
         hb_itemPutCLPtr( pInfo, ( char * ) pResult, ulLength );
         break;
      }

      default:
         uiRetVal = SUPER_RECINFO( ( AREAP ) pArea, pRecID, uiInfoType, pInfo );
   }

   if( ulPrevRec != 0 )
   {
      if( SELF_GOTO( ( AREAP ) pArea, ulPrevRec ) != SUCCESS &&
          uiRetVal == SUCCESS )
         uiRetVal = FAILURE;
   }

   return uiRetVal;
}

static ERRCODE letoRecNo( LETOAREAP pArea, ULONG * ulRecNo )
{
   LETOTABLE * pTable = pArea->pTable;
   HB_TRACE(HB_TR_DEBUG, ("letoRecNo(%p, %p)", pArea, ulRecNo));

   if( pTable->uiUpdated & 1 )
      leto_PutRec( pArea, FALSE );

   if( pArea->lpdbPendingRel )
   {
      if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
         return FAILURE;
   }

   *ulRecNo = pTable->ulRecNo;

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
   /*
   if( uiFieldExtent )
   {
      pTable->pFieldOffset = ( USHORT * ) hb_xgrab( uiFieldExtent * sizeof( USHORT ) );
      memset( pTable->pFieldOffset, 0, uiFieldExtent * sizeof( USHORT ) );
      pTable->pFieldUpd = ( USHORT * ) hb_xgrab( uiFieldExtent * sizeof( USHORT ) );
      memset( pTable->pFieldUpd, 0, uiFieldExtent * sizeof( USHORT ) );
   }
   */
   return SUCCESS;
}

#define  letoAlias                 NULL

static void leto_FreeTag( LETOTAGINFO * pTagInfo )
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
      hb_xfree( pTagInfo->pTopScopeAsString );
   if( pTagInfo->pBottomScopeAsString )
      hb_xfree( pTagInfo->pBottomScopeAsString );
   if( pTagInfo->Buffer.pBuffer )
      free( pTagInfo->Buffer.pBuffer );

   if( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pKeyItem != NULL )
      hb_vmDestroyBlockOrMacro( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pKeyItem );
   if( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pTopScope )
      hb_itemRelease( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pTopScope );
   if( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pBottomScope )
      hb_itemRelease( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pBottomScope );
   if( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->puiFields )
      free( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->puiFields );

   free( pTagInfo->pExtra );
   free( pTagInfo );
}

static ERRCODE letoClose( LETOAREAP pArea )
{
   char szData[32];
   LETOTABLE * pTable = pArea->pTable;
   HB_TRACE(HB_TR_DEBUG, ("letoClose(%p)", pArea));

   if( pTable && pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   pArea->lpdbPendingRel = NULL;

   if( !LetoDbCloseTable( pTable ) )
   {
      /* Free all filenames */
      if( pArea->szDataFileName )
      {
         hb_xfree( pArea->szDataFileName );
         pArea->szDataFileName = NULL;
      }
      if( pTable->pTagInfo )
      {
         LETOTAGINFO * pTagInfo = pTable->pTagInfo, * pTagNext;
         do
         {
            pTagNext = pTagInfo->pNext;
            leto_FreeTag( pTagInfo );
            pTagInfo = pTagNext;
         }
         while( pTagInfo );
         pTable->pTagInfo = NULL;
      }
      if( pArea->pLocksPos )
      {
         hb_xfree( pArea->pLocksPos );
         pArea->pLocksPos  = NULL;
         pArea->ulLocksMax = pArea->ulLocksAlloc = 0;
      }
      if( pArea->pTable )
         SUPER_CLOSE( ( AREAP ) pArea );
      pArea->pTable = NULL;
   }
   else
   {
      commonError( pArea, EG_SYNTAX, LetoGetError(), 0, NULL, 0, NULL );
      return FAILURE;
   }
   return SUCCESS;
}

static LETOCONNECTION * leto_OpenConnection( LETOAREAP pArea, LPDBOPENINFO pOpenInfo, char * szFile, BOOL bCreate)
{
   LETOCONNECTION * pConnection;
   char szAddr[96], * ptr;
   int iPort;
   unsigned int uiLen;

   if( pOpenInfo->ulConnection > 0 && pOpenInfo->ulConnection <= (ULONG) uiConnCount )
   {
      if( letoConnPool[pOpenInfo->ulConnection-1].pAddr )
      {
         const char * szPath = leto_RemoveIpFromPath( pOpenInfo->abName );
         pConnection = letoConnPool + pOpenInfo->ulConnection - 1;
         if( ( ptr = strrchr( szPath, '/') ) != NULL ||
             ( ptr = strrchr( szPath, '\\') ) != NULL )
         {
            uiLen = ptr - szPath;
            memcpy( szFile, szPath, uiLen );
            szFile[uiLen] = '\0';
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
          leto_SetOptions() &&
          ( pConnection = LetoConnectionNew( szAddr, iPort, NULL, NULL, 0 ) ) == NULL )
      {
         commonError( pArea, EG_OPEN, (bCreate ? 1 : 102), 0, ( char * ) pOpenInfo->abName, 0, NULL );
         return NULL;
      }
   }
   return pConnection;
}

static ERRCODE letoCreate( LETOAREAP pArea, LPDBOPENINFO pCreateInfo )
{
   LETOTABLE * pTable;
   LETOCONNECTION * pConnection;
   LPFIELD pField;
   USHORT uiCount;
   unsigned int uiLen;
   char *szData;
   char szFile[_POSIX_PATH_MAX + 1], szTemp[14];
   char * ptr;
   char cType;

   HB_TRACE(HB_TR_DEBUG, ("letoCreate(%p, %p)", pArea, pCreateInfo));

   if( ( pConnection = leto_OpenConnection( pArea, pCreateInfo, szFile, TRUE ) ) == NULL )
      return FAILURE;

   uiLen = strlen( szFile );
   if( szFile[uiLen-1] != '/' && szFile[uiLen-1] != '\\' )
   {
      szFile[uiLen++] = szFile[0];
      szFile[uiLen] = '\0';
   }
   leto_getFileFromPath( pCreateInfo->abName, szFile+uiLen );

   uiLen = (unsigned int) pArea->area.uiFieldCount * 24 + 10;
   szData = (char *) hb_xgrab( uiLen );
   ptr = szData;
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
         case HB_FT_DATETIME:
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

   pTable = LetoDbCreateTable( pConnection, szFile, (char*)pCreateInfo->atomAlias, szData, pCreateInfo->uiArea );
   hb_xfree( szData );

   if ( !pTable )
   {
      if( LetoGetError() == 1 || LetoGetError() == 1000 )
         commonError( pArea, EG_DATATYPE, LetoGetError(), 0, NULL, 0, NULL );
      else
         leto_CheckError( pArea );
      return FAILURE;
   }

   pArea->pTable = pTable;
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

   return SUPER_CREATE( ( AREAP ) pArea, pCreateInfo );
}

static ERRCODE letoInfo( LETOAREAP pArea, USHORT uiIndex, PHB_ITEM pItem )
{
   LETOTABLE * pTable = pArea->pTable;
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
         hb_itemPutNL( pItem, pTable->uiRecordLen );
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
         hb_itemPutL( pItem, pTable->fReadonly );
         break;

      case DBI_SHARED:
         hb_itemPutL( pItem, pTable->fShared );
         break;

      case DBI_MEMOEXT:
         hb_itemPutC( pItem, pTable->szMemoExt );
         break;

      case DBI_MEMOTYPE:
         hb_itemPutNL( pItem, pTable->bMemoType );
         break;

      case DBI_MEMOVERSION:
         hb_itemPutNL( pItem, pTable->uiMemoVersion );
         break;

      case DBI_DB_VERSION     :   /* HOST driver Version */
      {
         hb_itemPutC( pItem, LetoGetServerVer( &(letoConnPool[pTable->uiConnection]) ) );
         break;
      }

      case DBI_RDD_VERSION    :   /* RDD version (current RDD) */
         hb_itemPutC( pItem, HB_LETO_VERSION_STRING );
         break;

      case DBI_TRIGGER:
      {
         char szData[32];
         char *ptr;

         if( HB_IS_LOGICAL( pItem ) )
         {
            sprintf( szData,"dbi;%lu;%i;%s;\r\n", pTable->hTable, uiIndex,
               ( hb_itemGetL( pItem ) ? ".T." : ".F.") );

         }
         else if( HB_IS_STRING( pItem ) )
         {
            sprintf( szData,"dbi;%lu;%i;%s;\r\n", pTable->hTable, uiIndex,
               hb_itemGetCPtr( pItem ) );
         }
         else
            sprintf( szData,"dbi;%lu;%i;\r\n", pTable->hTable, uiIndex );

         if ( !leto_SendRecv( pArea, szData, 0, 1021 ) )
            return FAILURE;

         ptr = leto_firstchar();
         LetoGetCmdItem( &ptr, szData ); ptr ++;

         hb_itemPutC( pItem, szData );
         break;
      }

      case DBI_LASTUPDATE:
/*
         if( pTable->uiUpdated )
         {
            int iYear, iMonth, iDay;
            hb_dateToday( &iYear, &iMonth, &iDay );
            hb_itemPutDL( pItem, hb_dateEncode( iYear, iMonth, iDay ) );
         }
         else
*/
         hb_itemPutDL( pItem, pTable->lLastUpdate );
         break;

      case DBI_BUFREFRESHTIME:
      {
         SHORT iBufRefreshTime = pTable->iBufRefreshTime;
         if( HB_IS_NUMERIC( pItem ) )
            pTable->iBufRefreshTime = hb_itemGetNI( pItem );
         hb_itemPutNI( pItem, iBufRefreshTime );
         break;
      }

      case DBI_CLEARBUFFER:
         pTable->ptrBuf = NULL;
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
   unsigned int uiFields, uiLen, uiCount;
   DBFIELDINFO dbFieldInfo;
   char szData[_POSIX_PATH_MAX + 16];
   char szAlias[HB_RDD_MAX_ALIAS_LEN + 1];
   char szFile[_POSIX_PATH_MAX + 1], szName[14], szTemp[14], * ptr;
   char cType;
   LETOTABLE * pTable;
   LETOFIELD * pField;

   HB_TRACE(HB_TR_DEBUG, ("letoOpen(%p)", pArea));

   if( pOpenInfo->abName[0] != '+' && pOpenInfo->abName[0] != '-' )
   {
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

      if( ( pConnection = leto_OpenConnection( pArea, pOpenInfo, szFile, FALSE ) ) == NULL )
         return FAILURE;

      uiLen = strlen( szFile );
      if( szFile[uiLen-1] != '/' && szFile[uiLen-1] != '\\' )
      {
         szFile[uiLen++] = szFile[0];
         szFile[uiLen] = '\0';
      }
      leto_getFileFromPath( pOpenInfo->abName, szFile+uiLen );

      pArea->szDataFileName = hb_strdup( (char *) ( pOpenInfo->abName ) );

      pTable = LetoDbOpenTable( pConnection, szFile, (char*)pOpenInfo->atomAlias, 
            pOpenInfo->fShared, pOpenInfo->fReadonly, 
            (pOpenInfo->cdpId)? (char*)pOpenInfo->cdpId:"", pOpenInfo->uiArea );
   }
   else
   {
      if( !pOpenInfo->atomAlias )
         return FAILURE;

      if( pOpenInfo->ulConnection > 0 && pOpenInfo->ulConnection <= (ULONG) uiConnCount )
         pTable->uiConnection = pOpenInfo->ulConnection - 1;
      else if( pCurrentConn )
         pTable->uiConnection = pCurrentConn - letoConnPool;
      else
         return FAILURE;
      pConnection = letoConnPool + pTable->uiConnection;

      if( ( ptr = strchr( szFile + 1, ';' ) ) == NULL )
         return NULL;
      ptr ++;
      pArea->szDataFileName = hb_strndup( (char *) ( pOpenInfo->abName + 1 ), ptr - pOpenInfo->abName - 2 );

      pTable = LetoDbOpenTable( pConnection, (char*) pOpenInfo->abName, NULL, 0, 0, "", 0 );
   }

   if( !pTable )
   {
      if( LetoGetError() == 103 )
         commonError( pArea, EG_OPEN, 103, 32, ptr, EF_CANDEFAULT, NULL );
      else
         commonError( pArea, EG_DATATYPE, 1021, 0, ptr, 0, NULL );
      return FAILURE;
   }
   pArea->pTable = pTable;
   hb_strLower( pArea->szDataFileName, strlen(pArea->szDataFileName) );

   pArea->pTagCurrent    = NULL;
   if( pOpenInfo->cdpId )
   {
      pArea->area.cdPage = hb_cdpFind( (char *) pOpenInfo->cdpId );
      if( !pArea->area.cdPage )
         pArea->area.cdPage = hb_cdp_page;
   }
   else
      pArea->area.cdPage = hb_cdp_page;

   pArea->pLocksPos  = NULL;
   pArea->ulLocksMax = pArea->ulLocksAlloc = 0;

   SELF_SETFIELDEXTENT( ( AREAP ) pArea, pTable->uiFieldExtent );
   uiFields = pTable->uiFieldExtent;
   for( uiCount = 0; uiCount < uiFields; uiCount++ )
   {
      memset( &dbFieldInfo, 0, sizeof( dbFieldInfo ) );
      pField = pTable->pFields + uiCount;
      dbFieldInfo.atomName = pField->szName;
      dbFieldInfo.uiLen = pField->uiLen;
      dbFieldInfo.uiDec = pField->uiDec;
      dbFieldInfo.uiTypeExtended = 0;
      dbFieldInfo.uiType = pField->uiType;
      if( SELF_ADDFIELD( ( AREAP ) pArea, &dbFieldInfo ) == FAILURE )
         return FAILURE;
   }

   if( SUPER_OPEN( ( AREAP ) pArea, pOpenInfo ) == FAILURE )
   {
      SELF_CLOSE( ( AREAP ) pArea );
      return FAILURE;
   }

   if( pTable->pTagInfo )
   {
      LETOTAGINFO * pTagInfo = pTable->pTagInfo;
      while( pTagInfo )
      {
         leto_CreateKeyExpr( pArea, pTagInfo, pTagInfo->KeyExpr, NULL );
         if( LetoCheckServerVer( pConnection, 100 ) )
            ScanIndexFields( pArea, pTagInfo );
         pTagInfo = pTagInfo->pNext;
      }
      if( hb_setGetAutOpen() )
      {
         //ParseTagInfo( pArea, pTable->szTags );
         if( hb_setGetAutOrder() )
         {
            pTagInfo = pTable->pTagInfo;
            while( pTagInfo )
            {
               if( pTagInfo->uiTag == (unsigned int)hb_setGetAutOrder() )
               {
                  pArea->pTagCurrent = pTagInfo;
                  break;
               }
               pTagInfo = pTagInfo->pNext;
            }
         }
      }
   }
   pArea->area.fBof = pTable->fBof;
   pArea->area.fEof = pTable->fEof;
   pArea->area.fFound = pTable->fFound;
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
   LETOTABLE * pTable = pArea->pTable;
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoPack(%p)", pArea));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   if( letoConnPool[pTable->uiConnection].bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pTable->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pTable->fShared )
   {
      commonError( pArea, EG_SHARED, EDBF_SHARED, 0, NULL, 0, NULL );
      return FAILURE;
   }
   sprintf( szData,"pack;%lu;\r\n", pTable->hTable );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   return SELF_GOTOP( ( AREAP ) pArea );
}

#define  letoPackRec               NULL

static char * leto_PutTransInfo( LETOAREAP pArea, LETOAREAP pAreaDst, LPDBTRANSINFO pTransInfo, char * pData )
{
   LETOTABLE * pTable = pArea->pTable;
   char * ptr;
   USHORT uiIndex;

   sprintf( pData,"%lu;%lu;%s;%c;%lu;%s;%s;%lu;%lu;%c;%c;%c;%c;%c;%c;%c;%d;%d;",
                  pTable->hTable, pTable->ulRecNo,
                  pArea->pTagCurrent ? pArea->pTagCurrent->TagName : "",
                  (char) ( (hb_setGetDeleted()) ? 'T' : 'F' ),
                  pAreaDst->pTable->hTable,
                  hb_itemGetCPtr( pTransInfo->dbsci.lpstrFor ),
                  hb_itemGetCPtr( pTransInfo->dbsci.lpstrWhile ),
                  hb_itemGetNL( pTransInfo->dbsci.lNext ),
                  hb_itemGetNL( pTransInfo->dbsci.itmRecID ),
                  hb_itemGetL( pTransInfo->dbsci.fRest ) ? 'T' : 'F',
                  pTransInfo->dbsci.fIgnoreFilter ? 'T' : 'F',
                  pTransInfo->dbsci.fIncludeDeleted ? 'T' : 'F',
                  pTransInfo->dbsci.fLast ? 'T' : 'F',
                  pTransInfo->dbsci.fIgnoreDuplicates ? 'T' : 'F',
                  pTransInfo->dbsci.fBackward ? 'T' : 'F',
                  pTransInfo->dbsci.fOptimized ? 'T' : 'F',
                  pTransInfo->uiFlags, pTransInfo->uiItemCount );

   ptr = pData + strlen( pData );
   for( uiIndex = 0; uiIndex < pTransInfo->uiItemCount; uiIndex++ )
   {
      sprintf(ptr, "%d,%d;",
                     pTransInfo->lpTransItems[uiIndex].uiSource,
                     pTransInfo->lpTransItems[uiIndex].uiDest );
      ptr += strlen( ptr );
   }

   return ptr;
}

static ERRCODE letoSort( LETOAREAP pArea, LPDBSORTINFO pSortInfo )
{
   LPDBTRANSINFO pTransInfo = &pSortInfo->dbtri;
   LETOAREAP pAreaDst = (LETOAREAP) pTransInfo->lpaDest;
   ERRCODE fResult;
   char *pData, *ptr;
   USHORT uiLen, uiIndex;

   if( !leto_CheckArea( pAreaDst ) ||
       ( pArea->pTable->uiConnection != pAreaDst->pTable->uiConnection ) ||
       ( pTransInfo->dbsci.itmCobFor && ! pTransInfo->dbsci.lpstrFor ) ||
       ( pTransInfo->dbsci.itmCobWhile && ! pTransInfo->dbsci.lpstrWhile ) )
   {
      return SUPER_SORT( (AREAP) pArea, pSortInfo );
   }

   uiLen = 92 + LETO_MAX_TAGNAME +
               hb_itemGetCLen( pTransInfo->dbsci.lpstrFor ) +
               hb_itemGetCLen( pTransInfo->dbsci.lpstrWhile ) +
               pTransInfo->uiItemCount*16 + pSortInfo->uiItemCount*16;

   pData = hb_xgrab( uiLen );

   strcpy( pData, "sort;");

   ptr = leto_PutTransInfo( pArea, pAreaDst, pTransInfo, pData + 5 );

   sprintf(ptr, "%d;", pSortInfo->uiItemCount);
   ptr += strlen(ptr);

   for( uiIndex = 0; uiIndex < pSortInfo->uiItemCount; uiIndex++ )
   {
      sprintf(ptr, "%d,%d;",
                     pSortInfo->lpdbsItem[uiIndex].uiField,
                     pSortInfo->lpdbsItem[uiIndex].uiFlags );
      ptr += strlen( ptr );
   }

   sprintf( ptr,"\r\n" );

   fResult = ( leto_SendRecv( pArea, pData, 0, 0 ) ? SUCCESS : FAILURE );
   hb_xfree( pData );
   return fResult;
}

static ERRCODE letoTrans( LETOAREAP pArea, LPDBTRANSINFO pTransInfo )
{
   LETOAREAP pAreaDst = (LETOAREAP) pTransInfo->lpaDest;
   ERRCODE fResult;
   char *pData, *ptr;
   USHORT uiLen;

   if( !leto_CheckArea( pAreaDst ) ||
       ( pArea->pTable->uiConnection != pAreaDst->pTable->uiConnection ) ||
       ( pTransInfo->dbsci.itmCobFor && ! pTransInfo->dbsci.lpstrFor ) ||
       ( pTransInfo->dbsci.itmCobWhile && ! pTransInfo->dbsci.lpstrWhile ) )
   {
      return SUPER_TRANS( (AREAP) pArea, pTransInfo );
   }

   uiLen = 82 + LETO_MAX_TAGNAME +
               hb_itemGetCLen( pTransInfo->dbsci.lpstrFor ) +
               hb_itemGetCLen( pTransInfo->dbsci.lpstrWhile ) +
               pTransInfo->uiItemCount*16;

   pData = hb_xgrab( uiLen );

   strcpy( pData, "trans;");

   ptr = leto_PutTransInfo( pArea, pAreaDst, pTransInfo, pData + 6 );

   sprintf( ptr,"\r\n" );

   fResult = ( leto_SendRecv( pArea, pData, 0, 0 ) ? SUCCESS : FAILURE );
   hb_xfree( pData );
   return fResult;
}

#define  letoTransRec              NULL

static ERRCODE letoZap( LETOAREAP pArea )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoZap(%p)", pArea));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   if( letoConnPool[pTable->uiConnection].bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pTable->fReadonly )
   {
      commonError( pArea, EG_READONLY, EDBF_READONLY, 0, NULL, 0, NULL );
      return FAILURE;
   }
   if( pTable->fShared )
   {
      commonError( pArea, EG_SHARED, EDBF_SHARED, 0, NULL, 0, NULL );
      return FAILURE;
   }

   sprintf( szData,"zap;%lu;\r\n", pTable->hTable );
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

   return SUPER_CLEARREL( ( AREAP ) pArea );
}

static ERRCODE letoSetRel( LETOAREAP pArea, LPDBRELINFO pRelInf )
{
   HB_TRACE(HB_TR_DEBUG, ("letoSetRel(%p, %p)", pArea, pRelInf) );

   return SUPER_SETREL( ( AREAP ) pArea, pRelInf );
}

static ERRCODE letoOrderListAdd( LETOAREAP pArea, LPDBORDERINFO pOrderInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[_POSIX_PATH_MAX + 16], * ptr, * ptr1;
   const char * szBagName;
   LETOTAGINFO * pTagInfo;
   unsigned int uiLen;
   int iRcvLen;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListAdd(%p, %p)", pArea, pOrderInfo));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   szBagName = leto_RemoveIpFromPath( hb_itemGetCPtr( pOrderInfo->atomBagName ) );
   strcpy( szData, szBagName );
   hb_strLower( szData, strlen(szData) );
   if( (ptr = strrchr(szData, '/')) == NULL )
      ptr = szData;
   else
      ptr ++;
   if( (ptr1 = strchr(ptr, '.')) == NULL )
      uiLen = strlen(ptr);
   else
      uiLen = ptr1 - ptr;

   pTagInfo = pTable->pTagInfo;
   while( pTagInfo )
   {
      if( (uiLen == strlen(pTagInfo->BagName)) && !memcmp( ptr, pTagInfo->BagName, uiLen ) )
      {
         pArea->pTagCurrent = pTagInfo;
         return letoGoTop( pArea );
      }
      pTagInfo = pTagInfo->pNext;
   }

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"open_i;%lu;%s;\r\n", pTable->hTable, szBagName );
   else
      sprintf( szData,"open;02;%lu;%s;\r\n", pTable->hTable, szBagName );
   if ( ( iRcvLen = leto_SendRecv( pArea, szData, 0, 0 ) ) == 0 || leto_CheckError( pArea) )
      return FAILURE;
   ptr = leto_firstchar();

   pTagInfo = pTable->pTagInfo;
   while( pTagInfo && pTagInfo->pNext )
      pTagInfo = pTagInfo->pNext;

   ptr = leto_ParseTagInfo( pTable, ptr );
   if( pTagInfo )
      pArea->pTagCurrent = pTagInfo->pNext;
   else
      pArea->pTagCurrent = pTable->pTagInfo;

   pTagInfo = pTable->pTagInfo;
   while( pTagInfo )
   {
      leto_CreateKeyExpr( pArea, pTagInfo, pTagInfo->KeyExpr, NULL );
      if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
         ScanIndexFields( pArea, pTagInfo );
      pTagInfo = pTagInfo->pNext;
   }

   if( (iRcvLen - 4) > (ptr - leto_getRcvBuff()) )
      leto_ParseRec( pArea, ptr, TRUE );

   return SUCCESS;
}

static ERRCODE letoOrderListClear( LETOAREAP pArea )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[32];
   LETOTAGINFO * pTagInfo = pTable->pTagInfo, * pTag1, * pTagPrev = NULL;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListClear(%p)", pArea));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   if( pTagInfo )
   {
      if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
         sprintf( szData,"ord;%lu;04;\r\n", pTable->hTable );
      else
         sprintf( szData,"ord;04;%lu;\r\n", pTable->hTable );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

      pArea->pTagCurrent = NULL;
      pTable->uiOrders = 0;

      do
      {
         if( pTable->uiDriver || ( pTagInfo->BagName && leto_BagCheck( pArea->szDataFileName, pTagInfo->BagName ) ) )
         {
            pTag1 = pTagInfo;
            if( pTagInfo == pTable->pTagInfo )
               pTagInfo = pTable->pTagInfo = pTagInfo->pNext;
            else
               pTagInfo = pTagPrev->pNext = pTagInfo->pNext;

            leto_FreeTag( pTag1 );
         }
         else
         {
            pTagPrev = pTagInfo;
            pTagInfo = pTagPrev->pNext;
            pTable->uiOrders ++;
         }
      }
      while( pTagInfo );

   }

   return SUCCESS;
}

static ERRCODE letoOrderListDelete( LETOAREAP pArea, LPDBORDERINFO pOrderInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[_POSIX_PATH_MAX + 16], * ptr, * ptr1;
   const char * szBagName;
   LETOTAGINFO * pTagInfo, * pTagNext;
   unsigned int uiLen;
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListDelete(%p, %p)", pArea, pOrderInfo));

   if( !hb_setGetAutOpen() && LetoCheckServerVer( pConnection, 210 ) && pOrderInfo->atomBagName )
   {
      szBagName = leto_RemoveIpFromPath( hb_itemGetCPtr( pOrderInfo->atomBagName ) );

      sprintf( szData,"ord;%lu;10;%s;\r\n", pTable->hTable, szBagName );
      if ( !leto_SendRecv( pArea, szData, 0, 0 ) )
         return FAILURE;

      strcpy( szData, szBagName );
      hb_strLower( szData, strlen(szData) );
      if( (ptr = strrchr(szData, '/')) == NULL )
         ptr = szData;
      else
         ptr ++;
      
      if( (ptr1 = strchr(ptr, '.')) == NULL )
         uiLen = strlen(ptr);
      else
         uiLen = ptr1 - ptr;

      pTagInfo = pTable->pTagInfo;
      while( pTagInfo )
      {
      
         if( (uiLen == strlen(pTagInfo->BagName)) && !memcmp( ptr, pTagInfo->BagName, uiLen ) )
         {
            pTagNext = pTagInfo->pNext;
            if( pTagInfo == pTable->pTagInfo )
               pTable->pTagInfo = pTagNext;
            leto_FreeTag( pTagInfo );
            pTagInfo = pTagNext;
         }
         else
            pTagInfo = pTagInfo->pNext;
      }
      pArea->pTagCurrent = pTable->pTagInfo;

   }
   return SUCCESS;
}

static ERRCODE letoOrderListFocus( LETOAREAP pArea, LPDBORDERINFO pOrderInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   char szTagName[12];

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListFocus(%p, %p)", pArea, pOrderInfo));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );
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
      LETOTAGINFO * pTagInfo = pTable->pTagInfo;
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
         unsigned int uiOrder =  hb_itemGetNI( pOrderInfo->itmOrder );
         if( !uiOrder || uiOrder > pTable->uiOrders )
         {
            pArea->pTagCurrent = NULL;
            return SUCCESS;
         }
         while( --uiOrder ) pTagInfo = pTagInfo->pNext;
      }

      pArea->pTagCurrent = pTagInfo;
      pTable->ptrBuf = NULL;
      if( !pTagInfo )
         return FAILURE;
   }
   return SUCCESS;
}

static ERRCODE letoOrderListRebuild( LETOAREAP pArea )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoOrderListRebuild(%p)", pArea));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
      sprintf( szData,"ord;%lu;03;\r\n", pTable->hTable );
   else
      sprintf( szData,"ord;03;%lu;\r\n", pTable->hTable );
   if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

   return SELF_GOTOP( ( AREAP ) pArea );
}

#define  letoOrderCondition        NULL

static ERRCODE letoOrderCreate( LETOAREAP pArea, LPDBORDERCREATEINFO pOrderInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   const char * szKey, * szFor, * szBagName;
   LETOTAGINFO * pTagInfo;
   unsigned int uiLen, uiLenab, uiFlags;
   PHB_ITEM pResult, pKeyExp;
   ERRCODE errCode;
   BYTE bType;
   int iRes;
   LPDBORDERCONDINFO lpdbOrdCondInfo;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderCreate(%p, %p)", pArea, pOrderInfo));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

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
   lpdbOrdCondInfo = pArea->area.lpdbOrdCondInfo;
   if( !lpdbOrdCondInfo || !lpdbOrdCondInfo->fCustom )
   {
      hb_itemRelease( pKeyExp );
      pKeyExp = NULL;
   }

   if( pOrderInfo->abBagName )
      szBagName = leto_RemoveIpFromPath( pOrderInfo->abBagName );
   else
      szBagName = NULL;

   szFor = ( lpdbOrdCondInfo && lpdbOrdCondInfo->abFor)? lpdbOrdCondInfo->abFor : "";
       
   if( lpdbOrdCondInfo && lpdbOrdCondInfo->itmCobEval )
   {
      s_pEvalItem = lpdbOrdCondInfo->itmCobEval;
      leto_setCallback( funcEvalItem );
   }

   uiFlags = pOrderInfo->fUnique ? LETO_INDEX_UNIQ:0;
   if( lpdbOrdCondInfo )
      uiFlags |= (lpdbOrdCondInfo->fAll ? LETO_INDEX_ALL:0) |
            (lpdbOrdCondInfo->fRest ? LETO_INDEX_REST:0) |
            (lpdbOrdCondInfo->fDescending ? LETO_INDEX_DESC:0) |
            (lpdbOrdCondInfo->fCustom ? LETO_INDEX_CUST:0) |
            (lpdbOrdCondInfo->fAdditive ? LETO_INDEX_ADD:0) |
            (lpdbOrdCondInfo->fTemporary ? LETO_INDEX_TEMP:0) |
            (lpdbOrdCondInfo->fUseFilter ? LETO_INDEX_FILT:0);

   iRes = LetoDbOrderCreate( pTable, (char*)szBagName, (char*)(pOrderInfo->atomBagName),
         (char*)szKey, bType, uiFlags, (char*)szFor,
         (lpdbOrdCondInfo? (char*) lpdbOrdCondInfo->abWhile : NULL),
         (lpdbOrdCondInfo ? lpdbOrdCondInfo->lNextCount : 0) );
   if( s_pEvalItem )
   {
      s_pEvalItem = NULL;
      leto_setCallback( NULL );
   }
   if( iRes )
   {
      if( LetoGetError() == 1000 )
         commonError( pArea, EG_DATATYPE, LetoGetError(), 0, NULL, 0, NULL );
      else
         leto_CheckError( pArea );
      return FAILURE;
   }

   pTagInfo = pTable->pTagInfo;
   while( pTagInfo->pNext ) pTagInfo = pTagInfo->pNext;

   leto_CreateKeyExpr( pArea, pTagInfo, szKey, pKeyExp );
   if( pKeyExp )
   {
      hb_itemRelease( pKeyExp );
   }

   ScanIndexFields( pArea, pTagInfo );

   pArea->pTagCurrent = pTagInfo;

   return SELF_GOTOP( ( AREAP ) pArea );
}

static ERRCODE letoOrderDestroy( LETOAREAP pArea, LPDBORDERINFO pOrderInfo )
{
   HB_TRACE(HB_TR_DEBUG, ("letoOrderDestroy(%p, %p)", pArea, pOrderInfo));
   HB_SYMBOL_UNUSED( pOrderInfo );

   if( pArea->pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   return SUCCESS;
}

static BOOL leto_SkipEval( LETOAREAP pArea, LETOTAGINFO * pTagInfo, BOOL fForward,
                          PHB_ITEM pEval )
{
   LETOTABLE * pTable = pArea->pTable;
   PHB_ITEM pKeyVal, pKeyRec;
   BOOL fRet;

   if( ! pTagInfo || hb_itemType( pEval ) != HB_IT_BLOCK )
   {
      if( SELF_SKIP( ( AREAP ) pArea, fForward ? 1 : -1 ) == FAILURE )
         return FALSE;
      return fForward ? !pArea->area.fEof : !pArea->area.fBof;
   }

   for( ;; )
   {
      SELF_SKIP( ( AREAP ) pArea, fForward ? 1 : -1 );
      if( fForward ? pArea->area.fEof : pArea->area.fBof )
         break;
      pKeyVal = leto_KeyEval( pArea, pTagInfo );
      pKeyRec = hb_itemPutNInt( NULL, pTable->ulRecNo );
      fRet = hb_itemGetL( hb_vmEvalBlockV( pEval, 2, pKeyVal, pKeyRec ) );
      hb_itemRelease( pKeyRec );
      if( fRet )
         break;
   }

   return fForward ? !pArea->area.fEof : !pArea->area.fBof;
}

static ERRCODE letoOrderInfo( LETOAREAP pArea, USHORT uiIndex, LPDBORDERINFO pOrderInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   LETOTAGINFO * pTagInfo = NULL;
   char szData[32];
   USHORT uiTag = 0;

   HB_TRACE(HB_TR_DEBUG, ("letoOrderInfo(%p, %hu, %p)", pArea, uiIndex, pOrderInfo));

   if( pOrderInfo->itmOrder )
   {
      if( ! pTable->pTagInfo )
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
            pTagInfo = pTable->pTagInfo;
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
         unsigned int uiOrder =  hb_itemGetNI( pOrderInfo->itmOrder );

         if( uiOrder && uiOrder <= pTable->uiOrders )
         {
            pTagInfo = pTable->pTagInfo;
            while( --uiOrder ) pTagInfo = pTagInfo->pNext;
            uiOrder ++;
         }
         else
            uiOrder = 0;
         uiTag = uiOrder;
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
         hb_itemClear( pOrderInfo->itmResult );

         if( pArea->lpdbPendingRel )
            SELF_FORCEREL( ( AREAP ) pArea );

         if( ! pArea->area.fEof && pTagInfo && ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pKeyItem )
         {
            PHB_ITEM pItem = leto_KeyEval( pArea, pTagInfo );
            hb_itemCopy( pOrderInfo->itmResult, pItem );
         }
         break;
      }

      case DBOI_KEYCOUNT:
      {
         char * ptr;
         ULONG ul;

         if( !pTagInfo )
            sprintf( szData,"rcou;%lu;\r\n", pTable->hTable );
         else
         {
            if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
               sprintf( szData,"ord;%lu;01;%s;\r\n", pTable->hTable, pTagInfo->TagName );
            else
               sprintf( szData,"ord;01;%lu;%s;\r\n", pTable->hTable, pTagInfo->TagName );
         }
         if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

         ptr = leto_firstchar();
         LetoGetCmdItem( &ptr, szData ); ptr ++;
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
            // TOFIX: pTagInfo->UsrAscend - inverted TOP and BOTTOM!!!

            char szData[LETO_MAX_KEY+LETO_MAX_TAGNAME+35];
            char szKey[LETO_MAX_KEY+1];
            BOOL bSetTop, bSetBottom;
            BOOL bClearTop, bClearBottom;

            bClearTop    = ( (uiIndex == DBOI_SCOPETOPCLEAR)    || (uiIndex == DBOI_SCOPECLEAR) );
            bClearBottom = ( (uiIndex == DBOI_SCOPEBOTTOMCLEAR) || (uiIndex == DBOI_SCOPECLEAR) );
            bSetTop    = ( bClearTop    || (uiIndex == DBOI_SCOPETOP)    || (uiIndex == DBOI_SCOPESET) );
            bSetBottom = ( bClearBottom || (uiIndex == DBOI_SCOPEBOTTOM) || (uiIndex == DBOI_SCOPESET) );

            if( !((LETOTAGEXTRAINFO *)(pTagInfo->pExtra))->pTopScope )
               ((LETOTAGEXTRAINFO *)(pTagInfo->pExtra))->pTopScope = hb_itemNew( NULL );
            if( !((LETOTAGEXTRAINFO *)(pTagInfo->pExtra))->pBottomScope )
               ((LETOTAGEXTRAINFO *)(pTagInfo->pExtra))->pBottomScope = hb_itemNew( NULL );

            if( pOrderInfo->itmResult )
            {
               if( bSetTop && ( uiIndex != DBOI_SCOPECLEAR ) )
                  hb_itemCopy( pOrderInfo->itmResult, ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pTopScope );
               else if( bSetBottom && ( uiIndex != DBOI_SCOPECLEAR ) )
                  hb_itemCopy( pOrderInfo->itmResult, ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pBottomScope );
               else
                  hb_itemClear( pOrderInfo->itmResult );
            }

            if( pOrderInfo->itmNewVal || bClearTop || bClearBottom )
            {
               USHORT uiKeyLen;
               ULONG ulLen;
               char *pData;
               BOOL bUpdateTop = FALSE;
               BOOL bUpdateBottom = FALSE;

               if( bClearTop || bClearBottom )
               {
                  uiKeyLen = 0;
                  szKey[0] = '\0';
               }
               else
               {
                  uiKeyLen = leto_KeyToStr( pArea, szKey, leto_ItemType( pOrderInfo->itmNewVal ), pOrderInfo->itmNewVal );
               }

               if( bSetTop )
               {
                  if( bClearTop )
                     bUpdateTop = ( pTagInfo->pTopScopeAsString != NULL );
                  else if( ! pTagInfo->pTopScopeAsString )
                     bUpdateTop = TRUE;
                  else
                     bUpdateTop = ( strcmp( szKey, pTagInfo->pTopScopeAsString ) != 0 );
               }
               if( bSetBottom )
               {
                  if( bClearBottom )
                     bUpdateBottom = ( pTagInfo->pBottomScopeAsString != NULL );
                  else if( ! pTagInfo->pBottomScopeAsString )
                     bUpdateBottom = TRUE;
                  else
                     bUpdateBottom = ( strcmp( szKey, pTagInfo->pBottomScopeAsString ) != 0 );
               }

               if( bUpdateTop || bUpdateBottom )
               {
                  pData = szData + 4;
                  sprintf( pData,"scop;%lu;%i;%s;", pTable->hTable, uiIndex, pTagInfo->TagName );
                  pData = AddKeyToBuf( pData, szKey, uiKeyLen, &ulLen );
                  if ( !leto_SendRecv( pArea, pData, (ULONG) ulLen, 1021 ) )
                     return FAILURE;

                  if( bSetTop )
                  {
                     if( bClearTop )
                     {
                        hb_xfree( pTagInfo->pTopScopeAsString );
                        pTagInfo->pTopScopeAsString = NULL;
                        pTagInfo->iTopScopeAsString = 0;
                        hb_itemClear(((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pTopScope);
                     }
                     else
                     {
                        if( ! pTagInfo->pTopScopeAsString )
                        {
                           pTagInfo->pTopScopeAsString = (char*) hb_xgrab( uiKeyLen + 1 );
                           pTagInfo->iTopScopeAsString = uiKeyLen + 1;
                        }
                        else if( uiKeyLen >= pTagInfo->iTopScopeAsString )
                        {
                           pTagInfo->pTopScopeAsString = (char*) hb_xrealloc( pTagInfo->pTopScopeAsString, uiKeyLen + 1 );
                           pTagInfo->iTopScopeAsString = uiKeyLen + 1;
                        }
                        memcpy( pTagInfo->pTopScopeAsString, szKey, uiKeyLen+1 );
                        hb_itemCopy( ((LETOTAGEXTRAINFO *)pTagInfo->pExtra)->pTopScope, pOrderInfo->itmNewVal );
                     }
                  }
                  if( bSetBottom )
                  {
                     if( bClearBottom )
                     {
                        hb_xfree( pTagInfo->pBottomScopeAsString );
                        pTagInfo->pBottomScopeAsString = NULL;
                        pTagInfo->iBottomScopeAsString = 0;
                        hb_itemClear(((LETOTAGEXTRAINFO *)(pTagInfo->pExtra))->pBottomScope);
                     }
                     else
                     {
                        if( ! pTagInfo->pBottomScopeAsString )
                        {
                           pTagInfo->pBottomScopeAsString = (char*) hb_xgrab( uiKeyLen + 1 );
                           pTagInfo->iBottomScopeAsString = uiKeyLen + 1;
                        }
                        else if( uiKeyLen >= pTagInfo->iBottomScopeAsString )
                        {
                           pTagInfo->pBottomScopeAsString = (char*) hb_xrealloc( pTagInfo->pBottomScopeAsString, uiKeyLen + 1 );
                           pTagInfo->iBottomScopeAsString = uiKeyLen + 1;
                        }
                        memcpy( pTagInfo->pBottomScopeAsString, szKey, uiKeyLen+1 );
                        hb_itemCopy( ((LETOTAGEXTRAINFO *)(pTagInfo->pExtra))->pBottomScope, pOrderInfo->itmNewVal );
                     }
                  }
               }
               leto_ClearSeekBuf( pTagInfo );
               pTable->ptrBuf = NULL;
            }

         }
         else if( pOrderInfo->itmResult )
            hb_itemClear( pOrderInfo->itmResult );

         break;
      }

      case DBOI_POSITION:
      case DBOI_KEYNORAW:
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
            if( pTable->uiUpdated )
               leto_PutRec( pArea, FALSE );

            if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
               sprintf( szData,"ord;%lu;%s;%s;%lu;\r\n", pTable->hTable,
                  ( uiIndex == DBOI_POSITION ? "05" : "09" ),
                  pTagInfo->TagName, hb_itemGetNL( pOrderInfo->itmNewVal ) );
            else
               sprintf( szData,"ord;05;%lu;%s;%lu;\r\n", pTable->hTable,
                  pTagInfo->TagName, hb_itemGetNL( pOrderInfo->itmNewVal ) );

            if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
            hb_itemPutL( pOrderInfo->itmResult, TRUE );

            ptr = leto_firstchar();
            leto_ParseRec( pArea, ptr, TRUE );
            pTable->ptrBuf = NULL;

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
            if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
               sprintf( szData,"ord;%lu;%s;%s;%lu;\r\n", pTable->hTable,
                  ( uiIndex == DBOI_POSITION ? "02" : "08" ),
                  pTagInfo->TagName, pTable->ulRecNo );
            else
               sprintf( szData,"ord;02;%lu;%s;%lu;\r\n", pTable->hTable,
                  (pTagInfo)? pTagInfo->TagName:"", pTable->ulRecNo );

            if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

            ptr = leto_firstchar();
            LetoGetCmdItem( &ptr, szData ); ptr ++;
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
         if( pTable->uiUpdated )
            leto_PutRec( pArea, FALSE );

         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
            sprintf( szData,"ord;%lu;06;%s;%lu;%c;%lu;\r\n", pTable->hTable,
               (pTagInfo)? pTagInfo->TagName:"",
               pTable->ulRecNo, (char)( (hb_setGetDeleted())? 0x41 : 0x40 ),
               pOrderInfo->itmNewVal && HB_IS_NUMERIC( pOrderInfo->itmNewVal ) ?
               hb_itemGetNL( pOrderInfo->itmNewVal ) : 1 );
         else
            sprintf( szData,"ord;06;%lu;%s;%lu;\r\n", pTable->hTable,
               (pTagInfo)? pTagInfo->TagName:"",
               pOrderInfo->itmNewVal && HB_IS_NUMERIC( pOrderInfo->itmNewVal ) ?
               hb_itemGetNL( pOrderInfo->itmNewVal ) : 1 );

         lRet = leto_SendRecv( pArea, szData, 0, 1021 );
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult, lRet );
         if( !lRet ) return FAILURE;

         ptr = leto_firstchar();
         leto_ParseRec( pArea, ptr, TRUE );
         pTable->ptrBuf = NULL;

         if( pArea->area.lpdbRelations )
            return SELF_SYNCCHILDREN( ( AREAP ) pArea );
         break;
      }

      case DBOI_SKIPEVAL:
      case DBOI_SKIPEVALBACK:
      {
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult,
            leto_SkipEval( pArea, pTagInfo, uiIndex == DBOI_SKIPEVAL, pOrderInfo->itmNewVal ) );
         break;
      }

      case DBOI_SKIPWILD:
      case DBOI_SKIPWILDBACK:
      case DBOI_SKIPREGEX:
      case DBOI_SKIPREGEXBACK:
      {
         pArea->lpdbPendingRel = NULL;
         if( pTable->uiUpdated )
            leto_PutRec( pArea, FALSE );

         if( pTagInfo && pOrderInfo->itmNewVal && HB_IS_STRING( pOrderInfo->itmNewVal ) )
         {
            char * ptr;
            sprintf( szData,"ord;%lu;07;%s;%lu;%c;%i;%" HB_PFS "u;%s;\r\n", pTable->hTable,
               pTagInfo->TagName,
               pTable->ulRecNo, (char)( (hb_setGetDeleted())? 0x41 : 0x40 ), uiIndex,
               hb_itemGetCLen( pOrderInfo->itmNewVal ),
               hb_itemGetCPtr( pOrderInfo->itmNewVal ) );

            if( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

            ptr = leto_firstchar();
            leto_ParseRec( pArea, ptr, TRUE );
            pTable->ptrBuf = NULL;

            if( pArea->area.lpdbRelations )
               SELF_SYNCCHILDREN( ( AREAP ) pArea );
         }
         else
         {
            SELF_SKIP( ( AREAP ) pArea, ( ( uiIndex == DBOI_SKIPWILD ) ||
                                          ( uiIndex == DBOI_SKIPREGEX ) ) ? 1 : -1 );
         }
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult, ! pArea->area.fEof && ! pArea->area.fBof );
         break;
      }

      case DBOI_BAGEXT:
      {
         if( pOrderInfo->itmResult )
            hb_itemClear( pOrderInfo->itmResult );
         else
            pOrderInfo->itmResult = hb_itemNew( NULL );
         hb_itemPutC( pOrderInfo->itmResult, (pTable->uiDriver)? ".NTX" : ".CDX" );
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
         BOOL bResult;

         if( ! pTagInfo )
         {
            bResult = FALSE;
         }
         else if( pOrderInfo->itmNewVal && HB_IS_LOGICAL( pOrderInfo->itmNewVal ) )
         {
            char * ptr;

            sprintf( szData, "dboi;%lu;%i;%s;%s;\r\n", pTable->hTable, uiIndex, pTagInfo->TagName,
                             hb_itemGetL(pOrderInfo->itmNewVal) ? "T" : "F" );

            if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;

            ptr = leto_firstchar();
            LetoGetCmdItem( &ptr, szData ); ptr ++;
            bResult = ( szData[0] == 'T' );
            if( uiIndex == DBOI_ISDESC )
               pTagInfo->UsrAscend = !bResult;
            else if( uiIndex == DBOI_CUSTOM )
               pTagInfo->Custom = bResult;
            else
               pTagInfo->UniqueKey = bResult;
         }
         else
         {
            if( uiIndex == DBOI_ISDESC )
               bResult = !pTagInfo->UsrAscend;
            else if( uiIndex == DBOI_CUSTOM )
               bResult = pTagInfo->Custom;
            else
               bResult = pTagInfo->UniqueKey;
         }
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult, bResult );
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
      case DBOI_BAGCOUNT:
      {
         unsigned int uiCount = 0;
         if( pTable->uiOrders )
         {
            char ** pBagNames = hb_xgrab( sizeof( char * ) * pTable->uiOrders );
            unsigned int ui;
            LETOTAGINFO * pTag = pTable->pTagInfo;
            BOOL bFound = FALSE;
            while( pTag )
            {
               if( pTag->BagName )
               {
                  for( ui = 0; ui < uiCount; ui++ )
                     if( ( pBagNames[ui] == pTag->BagName ) || !strcmp( pBagNames[ui], pTag->BagName ) )
                     {
                        bFound = TRUE;
                        break;
                     }
                  if( !bFound && uiCount < pTable->uiOrders )
                     pBagNames[ uiCount ++ ] = pTag->BagName;
               }
               pTag = pTag->pNext;
            }
            hb_xfree( pBagNames );
         }
         hb_itemPutNI( pOrderInfo->itmResult, uiCount );
         break;
      }
      case DBOI_BAGNUMBER:
      {
         const char * pBagName, * pLast = NULL;
         USHORT uiNumber = 0;
         LETOTAGINFO * pTag = pTable->pTagInfo;

         if( hb_itemGetCLen( pOrderInfo->atomBagName ) > 0 )
            pBagName = hb_itemGetCPtr( pOrderInfo->atomBagName );
         else if( pTagInfo )
            pBagName = pTagInfo->BagName;
         else
            pBagName = NULL;
         if( pBagName )
            while( pTag )
            {
               if( pTag->BagName )
               {
                  if( !pLast || !strcmp( pLast, pTag->BagName ) )
                  {
                     pLast = pTag->BagName;
                     ++uiNumber;
                  }
                  if( !strcmp( pBagName, pTag->BagName ) )
                     break;
               }
               pTag = pTag->pNext;
            }
         hb_itemPutNI( pOrderInfo->itmResult, uiNumber );
         break;
      }
      case DBOI_BAGORDER:
      {
         const char * pBagName;
         USHORT uiNumber = 0;
         LETOTAGINFO * pTag = pTable->pTagInfo;

         if( hb_itemGetCLen( pOrderInfo->atomBagName ) > 0 )
            pBagName = hb_itemGetCPtr( pOrderInfo->atomBagName );
         else if( pTagInfo )
            pBagName = pTagInfo->BagName;
         else
            pBagName = NULL;
         if( pBagName )
            while( pTag )
            {
               ++uiNumber;
               if( pTag->BagName && !strcmp( pBagName, pTag->BagName ) )
                  break;
               pTag = pTag->pNext;
            }
         hb_itemPutNI( pOrderInfo->itmResult, uiNumber );
         break;
      }
      case DBOI_KEYADD:
      case DBOI_KEYDELETE:
      {
         char * ptr;

         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
         {
            char szData1[LETO_MAX_KEY+LETO_MAX_TAGNAME+56];
            char szKey[LETO_MAX_KEY+1];
            char cType;
            char * pData = szData1 + 4;
            USHORT uiKeyLen;
            ULONG ulLen = 0;

            sprintf( pData,"dboi;%lu;%i;%s;%lu;", pTable->hTable, uiIndex,
                  (pTagInfo)? pTagInfo->TagName:"", pTable->ulRecNo );

            if( pOrderInfo->itmNewVal && ( cType = leto_ItemType( pOrderInfo->itmNewVal ) ) == pTagInfo->KeyType )
            {
               uiKeyLen = leto_KeyToStr( pArea, szKey, cType, pOrderInfo->itmNewVal );
               pData = AddKeyToBuf( pData, szKey, uiKeyLen, &ulLen );
            }
            else
               *( pData+strlen(pData) ) = '\0';

            if ( !leto_SendRecv( pArea, pData, ulLen, 1021 ) ) return FAILURE;
         }
         else
         {
            sprintf( szData,"dboi;%lu;%i;%s;%s;\r\n", pTable->hTable, uiIndex,
                  (pTagInfo)? pTagInfo->TagName:"",
                  (pOrderInfo->itmNewVal && HB_IS_STRING( pOrderInfo->itmNewVal )) ? hb_itemGetCPtr( pOrderInfo->itmNewVal ) : "" );

            if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
         }
         ptr = leto_firstchar();
         LetoGetCmdItem( &ptr, szData ); ptr ++;
         pOrderInfo->itmResult = hb_itemPutL( pOrderInfo->itmResult, szData[0] == 'T' );
         break;
      }
      case DBOI_ORDERCOUNT:
      {
         hb_itemPutNI( pOrderInfo->itmResult, pTable->uiOrders );
         break;
      }
   }

   return SUCCESS;
}

static ERRCODE letoClearFilter( LETOAREAP pArea )
{
   LETOTABLE * pTable = pArea->pTable;
   char szData[32];

   HB_TRACE(HB_TR_DEBUG, ("letoClearFilter(%p)", pArea));

#ifndef __BM
   if( pTable->hTable && pArea->area.dbfi.itmCobExpr )
#else
   if( pTable->hTable && ( pArea->area.dbfi.itmCobExpr || pArea->area.dbfi.lpvCargo ) )
#endif
   {
      sprintf( szData,"setf;%lu;;\r\n", pTable->hTable );
      if ( !leto_SendRecv( pArea, szData, 0, 1021 ) ) return FAILURE;
   }

#ifdef __BM
   pArea->area.dbfi.lpvCargo = NULL;
#endif
   return SUPER_CLEARFILTER( ( AREAP ) pArea );
}

#define  letoClearLocate           NULL
#define  letoClearScope            NULL
#define  letoCountScope            NULL
#define  letoFilterText            NULL
#define  letoScopeInfo             NULL

static ERRCODE letoSetFilter( LETOAREAP pArea, LPDBFILTERINFO pFilterInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   char * pData, * pData1;
   ULONG ulLen;

   HB_TRACE(HB_TR_DEBUG, ("letoSetFilter(%p, %p)", pArea, pFilterInfo));

   if( SUPER_SETFILTER( ( AREAP ) pArea, pFilterInfo ) == SUCCESS )
   {
      if( pFilterInfo->abFilterText && hb_itemGetCLen(pFilterInfo->abFilterText) )
      {
         ulLen = hb_itemGetCLen(pFilterInfo->abFilterText);
         pData = (char*) hb_xgrab( ulLen + 24 );
         sprintf( pData,"setf;%lu;", pTable->hTable );

         pData1 = pData + strlen(pData);
         memcpy( pData1, hb_itemGetCPtr(pFilterInfo->abFilterText), ulLen );
         pData1 += ulLen;

         memcpy( pData1, ";\r\n\0", 4 );

         if ( !leto_SendRecv( pArea, pData, 0, 0 ) )
         {
            hb_xfree( pData );
            return FAILURE;
         }
         hb_xfree( pData );

         if( *(leto_getRcvBuff()) == '-' )
            pArea->area.dbfi.fOptimized = FALSE;
         else
            pArea->area.dbfi.fOptimized = TRUE;

         leto_ClearBuffers( pArea );
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

static void leto_AddRecLock( LETOAREAP pArea, ULONG ulRecNo )
{
   if( ! leto_IsRecLocked( pArea, ulRecNo ) )
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
   LETOTABLE * pTable = pArea->pTable;
   char szData[32];
   LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;

   HB_TRACE(HB_TR_DEBUG, ("letoRawLock(%p, %hu, %lu)", pArea, uiAction, ulRecNo));

   if( pTable->uiUpdated )
      leto_PutRec( pArea, FALSE );

   if( pArea->lpdbPendingRel )
   {
      if( SELF_FORCEREL( ( AREAP ) pArea ) != SUCCESS )
         return FAILURE;
   }

   switch( uiAction )
   {
      case REC_LOCK:
         leto_ClearBuffers( pArea );
         if( !pTable->fShared || pArea->fFLocked || ( ulRecNo == pTable->ulRecNo ? pTable->fRecLocked : leto_IsRecLocked(pArea, ulRecNo) ) )
            return SUCCESS;

         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
            sprintf( szData,"lock;%lu;01;%lu;\r\n", pTable->hTable, ulRecNo );
         else
            sprintf( szData,"lock;01;%lu;%lu;\r\n", pTable->hTable, ulRecNo );
         if ( !leto_SendRecv( pArea, szData, 0, 0 )|| leto_checkLockError( pArea ) ) return FAILURE;
         if( ulRecNo == pTable->ulRecNo )
            pTable->fRecLocked = 1;
         break;

      case REC_UNLOCK:
         leto_ClearBuffers( pArea );
         if( pConnection->bTransActive )
         {
            commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
            return FAILURE;
         }
         if( !pTable->fShared || pArea->fFLocked || ( ulRecNo == pTable->ulRecNo ? !pTable->fRecLocked : !leto_IsRecLocked(pArea, ulRecNo) ) )
            return SUCCESS;

         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
            sprintf( szData,"unlock;%lu;01;%lu;\r\n", pTable->hTable, ulRecNo );
         else
            sprintf( szData,"unlock;01;%lu;%lu;\r\n", pTable->hTable, ulRecNo );
         if ( !leto_SendRecv( pArea, szData, 0, 0 ) || leto_checkLockError( pArea ) ) return FAILURE;
         if( ulRecNo == pTable->ulRecNo )
            pTable->fRecLocked = 0;
         break;

      case FILE_LOCK:
         leto_ClearBuffers( pArea );
         if( !pTable->fShared || pArea->fFLocked )
            return SUCCESS;

         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
            sprintf( szData,"lock;%lu;02;\r\n", pTable->hTable );
         else
            sprintf( szData,"lock;02;%lu;\r\n", pTable->hTable );
         if ( !leto_SendRecv( pArea, szData, 0, 0 ) || leto_checkLockError( pArea ) ) return FAILURE;
         pArea->fFLocked = TRUE;
         break;

      case FILE_UNLOCK:
         leto_ClearBuffers( pArea );
         if( pConnection->bTransActive )
         {
            commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
            return FAILURE;
         }
         if( !pTable->fShared )
            return SUCCESS;

         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
            sprintf( szData,"unlock;%lu;02;\r\n", pTable->hTable );
         else
            sprintf( szData,"unlock;02;%lu;\r\n", pTable->hTable );
         if ( !leto_SendRecv( pArea, szData, 0, 0 ) || leto_checkLockError( pArea ) ) return FAILURE;
         pArea->fFLocked = FALSE;
         pTable->fRecLocked = 0;
         pArea->ulLocksMax = 0;
         break;
   }
   return SUCCESS;
}

static ERRCODE letoLock( LETOAREAP pArea, LPDBLOCKINFO pLockInfo )
{
   LETOTABLE * pTable = pArea->pTable;
   ULONG ulRecNo = ( ULONG ) hb_itemGetNL( pLockInfo->itmRecID );
   USHORT uiAction;

   HB_TRACE(HB_TR_DEBUG, ("letoLock(%p, %p)", pArea, pLockInfo));

   if( !ulRecNo && pArea->lpdbPendingRel )
      SELF_FORCEREL( ( AREAP ) pArea );

   switch( pLockInfo->uiMethod )
   {
      case DBLM_EXCLUSIVE :
         ulRecNo = pTable->ulRecNo;
         uiAction = REC_LOCK;
         pArea->ulLocksMax = 0;
         break;

      case DBLM_MULTIPLE :
         if( pLockInfo->itmRecID == NULL )
            ulRecNo = pTable->ulRecNo;
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
      leto_AddRecLock( pArea, ulRecNo );

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

static ERRCODE letoDrop( LPRDDNODE pRDD, PHB_ITEM pItemTable, PHB_ITEM pItemIndex, HB_ULONG ulConnect )
{
   LETOCONNECTION * pConnection;
   char szTFileName[ _POSIX_PATH_MAX + 1 ];
   char szIFileName[ _POSIX_PATH_MAX + 1 ];
   char szData[ 2 * _POSIX_PATH_MAX + 16 ];
   const char * szTableFile, * szIndexFile;

   HB_SYMBOL_UNUSED( pRDD );
   HB_SYMBOL_UNUSED( ulConnect );

   szTableFile = hb_itemGetCPtr( pItemTable );
   szIndexFile = hb_itemGetCPtr( pItemIndex );

   pConnection = letoParseParam( szIndexFile, szIFileName );
   if( pConnection == NULL )
      pConnection = letoParseParam( szTableFile, szTFileName );
   else
      letoParseParam( szTableFile, szTFileName );

   if( pConnection != NULL )
   {
      sprintf( szData, "drop;%s;%s;\r\n", szTFileName, szIFileName );
      if ( leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         char * ptr = leto_firstchar();

         LetoGetCmdItem( &ptr, szData ); ptr ++;
         if( szData[0] == 'T' )
            return SUCCESS;
      }
   }

   return FAILURE;
}

static ERRCODE letoExists( LPRDDNODE pRDD, PHB_ITEM pItemTable, PHB_ITEM pItemIndex, HB_ULONG ulConnect )
{
   LETOCONNECTION * pConnection;
   char szTFileName[ _POSIX_PATH_MAX + 1 ];
   char szIFileName[ _POSIX_PATH_MAX + 1 ];
   char szData[ 2 * _POSIX_PATH_MAX + 16 ];
   const char * szTableFile, * szIndexFile;

   HB_SYMBOL_UNUSED( pRDD );
   HB_SYMBOL_UNUSED( ulConnect );

   szTableFile = hb_itemGetCPtr( pItemTable );
   szIndexFile = hb_itemGetCPtr( pItemIndex );

   pConnection = letoParseParam( szIndexFile, szIFileName );
   if( pConnection == NULL )
      pConnection = letoParseParam( szTableFile, szTFileName );
   else
      letoParseParam( szTableFile, szTFileName );

   if( pConnection != NULL )
   {
      sprintf( szData, "exists;%s;%s;\r\n", szTFileName, szIFileName );
      if ( leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         char * ptr = leto_firstchar();

         LetoGetCmdItem( &ptr, szData ); ptr ++;
         if( szData[0] == 'T' )
            return SUCCESS;
      }
   }

   return FAILURE;
}

static ERRCODE letoRename( LPRDDNODE pRDD, PHB_ITEM pItemTable, PHB_ITEM pItemIndex, PHB_ITEM pItemNew, HB_ULONG ulConnect )
{
   LETOCONNECTION * pConnection;
   char szTFileName[ _POSIX_PATH_MAX + 1 ];
   char szIFileName[ _POSIX_PATH_MAX + 1 ];
   char szFileNameNew[ _POSIX_PATH_MAX + 1 ];
   char szData[ 3 * _POSIX_PATH_MAX + 16 ];
   const char * szTableFile, * szIndexFile, * szNewFile;

   HB_SYMBOL_UNUSED( pRDD );
   HB_SYMBOL_UNUSED( ulConnect );

   szTableFile = hb_itemGetCPtr( pItemTable );
   szIndexFile = hb_itemGetCPtr( pItemIndex );
   szNewFile   = hb_itemGetCPtr( pItemNew );

   pConnection = letoParseParam( szIndexFile, szIFileName );
   if( pConnection == NULL )
      pConnection = letoParseParam( szTableFile, szTFileName );
   else
      letoParseParam( szTableFile, szTFileName );
   letoParseParam( szNewFile, szFileNameNew );

   if( pConnection != NULL )
   {
      sprintf( szData, "rename;%s;%s;%s;\r\n", szTFileName, szIFileName, szFileNameNew );
      if ( leto_DataSendRecv( pConnection, szData, 0 ) )
      {
         char * ptr = leto_firstchar();

         LetoGetCmdItem( &ptr, szData ); ptr ++;
         if( szData[0] == 'T' )
            return SUCCESS;
      }
   }

   return FAILURE;
}

static ERRCODE letoInit( LPRDDNODE pRDD )
{
   char szFile[_POSIX_PATH_MAX + 1], **pArgv;

   HB_SYMBOL_UNUSED( pRDD );

   LetoInit();

   pArgv = hb_cmdargARGV();
   if( pArgv )
      leto_getFileFromPath( *pArgv, szFile );
   else
      szFile[0] = '\0';

   LetoSetModName( szFile );

   return SUCCESS;
}

static ERRCODE letoExit( LPRDDNODE pRDD )
{
   USHORT i;
   HB_SYMBOL_UNUSED( pRDD );

   if( letoConnPool )
      for( i=0; i<uiConnCount; i++ )
         leto_ConnectionClose( letoConnPool + i );

   LetoExit( 0 );
   if( s_uiRddCount )
   {
      if( ! --s_uiRddCount )
      {
         LetoExit( 1 );
#ifdef __BORLANDC__
   #pragma option push -w-pro
#endif
         // letoApplicationExit();
#ifdef __BORLANDC__
   #pragma option pop
#endif
      }
   }

   /* free pRDD->lpvCargo if necessary */

   return SUCCESS;
}

static USHORT leto_MemoType( ULONG ulConnect )
{
  LETOCONNECTION * pConnection = ( ulConnect > 0 && ulConnect <= (ULONG) uiConnCount ) ?
     letoConnPool + ulConnect - 1 : pCurrentConn;
  USHORT uiMemoType;
  if( pConnection && pConnection->uiMemoType )
     uiMemoType = pConnection->uiMemoType;
  else if( pConnection )
     uiMemoType = pConnection->uiDriver == LETO_NTX ? DB_MEMO_DBT : DB_MEMO_FPT;
  else
     uiMemoType = DB_MEMO_FPT;
  return uiMemoType;
}

static ERRCODE letoRddInfo( LPRDDNODE pRDD, USHORT uiIndex, ULONG ulConnect, PHB_ITEM pItem )
{
   HB_TRACE(HB_TR_DEBUG, ("letoRddInfo(%p, %hu, %lu, %p)", pRDD, uiIndex, ulConnect, pItem));

   switch( uiIndex )
   {
      case RDDI_REMOTE:
         hb_itemPutL( pItem, TRUE );
         break;
      case RDDI_CONNECTION:
      {
         USHORT uiConnection = ( pCurrentConn ? pCurrentConn - letoConnPool + 1 : 0 );

         if( HB_IS_NUMBER( pItem ) )
         {
            USHORT uiNewConn = hb_itemGetNI( pItem );
            if( uiNewConn && uiNewConn <= uiConnCount )
               pCurrentConn = letoConnPool + uiNewConn - 1;
         }
         hb_itemPutNI( pItem, uiConnection );
         break;
      }
      case RDDI_ISDBF:
         hb_itemPutL( pItem, TRUE );
         break;

      case RDDI_CANPUTREC:
         hb_itemPutL( pItem, TRUE );
         break;

      case RDDI_TABLEEXT:
         hb_itemPutC( pItem, ".dbf" );
         break;

      case RDDI_MEMOEXT:
         hb_itemPutC( pItem, leto_MemoType( ulConnect ) == DB_MEMO_DBT ? ".dbt" : ".fpt" );
         break;

      case RDDI_MEMOTYPE:
         hb_itemPutNI( pItem, leto_MemoType( ulConnect ) );
         break;

      case RDDI_ORDEREXT:
      case RDDI_ORDBAGEXT:
      case RDDI_ORDSTRUCTEXT:
         hb_itemPutC( pItem, ".cdx" );
         break;

      case RDDI_REFRESHCOUNT:
         if( ulConnect > 0 && ulConnect <= (ULONG) uiConnCount )
         {
            LETOCONNECTION * pConnection = letoConnPool + ulConnect - 1;
            BOOL bSet = HB_IS_LOGICAL( pItem );
            BOOL bRefresh = hb_itemGetL( pItem );
            hb_itemPutL( pItem, pConnection->bRefreshCount );
            if( bSet )
               pConnection->bRefreshCount = bRefresh;
         }
         else
            hb_itemPutL( pItem, TRUE );
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

static void leto_RegisterRDD( USHORT * pusRddId )
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

   leto_RegisterRDD( &s_uiRddIdLETO );
}

HB_FUNC( LETO ) { ; }

static ERRCODE leto_UpdArea( AREAP pArea, void * p )
{
   if( leto_CheckAreaConn( pArea, ( LETOCONNECTION * ) p ) &&
       ( (LETOAREAP) pArea )->pTable->uiUpdated )
   {
      leto_PutRec( (LETOAREAP)pArea, FALSE );
   }
   return 0;
}

static ERRCODE leto_UnLockRec( AREAP pArea, void * p )
{
   if( leto_CheckAreaConn( pArea, ( LETOCONNECTION * ) p ) &&
       ( (LETOAREAP) pArea )->pTable->uiUpdated )
   {
      ( (LETOAREAP) pArea )->pTable->fRecLocked = 0;
   }
   return 0;
}

static ERRCODE leto_ClearUpd( AREAP pArea, void * p )
{
   if( leto_CheckAreaConn( pArea, ( LETOCONNECTION * ) p ) &&
       ( (LETOAREAP) pArea )->pTable->uiUpdated )
   {
      leto_SetUpdated( ( ( LETOAREAP ) pArea)->pTable, 0);
      letoGoTo( (LETOAREAP)pArea, ( ( LETOAREAP ) pArea)->pTable->ulRecNo );
   }
   return 0;
}

HB_FUNC( LETO_BEGINTRANSACTION )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;

   if( !leto_CheckArea( pArea ) || letoConnPool[pTable->uiConnection].bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
   }
   else
   {
      LETOCONNECTION * pConnection = letoConnPool + pTable->uiConnection;
      hb_rddIterateWorkAreas( leto_UpdArea, (void *) pConnection );
      pConnection->ulTransBlockLen = HB_ISNUM(1) ? hb_parnl(1) : 0;
      pConnection->bTransActive = TRUE;
   }
}

HB_FUNC( LETO_ROLLBACK )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;
   LETOCONNECTION * pConnection;

   if( !leto_CheckArea( pArea ) || !letoConnPool[pTable->uiConnection].bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
   }
   else
   {
      pConnection = letoConnPool + pTable->uiConnection;
      pConnection->bTransActive = FALSE;
      pConnection->ulTransDataLen = pConnection->ulRecsInTrans = 0;

      if( HB_ISNIL(1) || ( HB_ISLOG(1) && hb_parl(1) ) )
      {
         char szData[18], *pData;
         ULONG ulLen;

         memcpy( szData+4, "ta;", 3 );
         if( LetoCheckServerVer( letoConnPool+pTable->uiConnection, 100 ) )
         {
            ulLen = 8;
            HB_PUT_LE_UINT32( szData+7, 0 );
         }
         else
         {
            ulLen = 6;
            szData[7] = szData[8] = '\0';
         }
         szData[pConnection->uiTBufOffset-1] = (char) 0x41;
         pData = leto_AddLen( (szData+4), &ulLen, 1 );
         leto_SendRecv( pArea, pData, ulLen, 1021 );
         hb_rddIterateWorkAreas( leto_UnLockRec, (void *) pConnection );
         hb_rddIterateWorkAreas( leto_ClearUpd, (void *) pConnection );
      }
   }
}

static ERRCODE leto_FindArea( AREAP pArea, void * p )
{
   if( leto_CheckArea( ( LETOAREAP ) pArea ) &&
      ( ( LETOAREAP ) pArea)->pTable->uiConnection == (unsigned int)( ( FINDAREASTRU * ) p )->uiConnection &&
      ( ( LETOAREAP ) pArea)->pTable->hTable == ( ( FINDAREASTRU * ) p )->ulAreaID )
   {
      ( ( FINDAREASTRU * ) p )->pArea = ( LETOAREAP ) pArea;
   }
   return SUCCESS;
}

HB_FUNC( LETO_COMMITTRANSACTION )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;
   char * pData;
   ULONG ulLen;
   BOOL  bUnlockAll = (HB_ISLOG(1))? hb_parl(1) : TRUE;
   LETOCONNECTION * pConnection;

   if( !leto_CheckArea( pArea ) || !letoConnPool[pTable->uiConnection].bTransActive )
   {
      commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      return;
   }

   pConnection = letoConnPool + pTable->uiConnection;
   hb_rddIterateWorkAreas( leto_UpdArea, (void *) pConnection );
   pConnection->bTransActive = FALSE;

   if( pConnection->szTransBuffer && ( pConnection->ulTransDataLen > (ULONG)pConnection->uiTBufOffset ) )
   {
      ulLen = pConnection->ulTransDataLen - 4;
      if( LetoCheckServerVer( pConnection, 100 ) )
      {
         HB_PUT_LE_UINT32( pConnection->szTransBuffer+7, pConnection->ulRecsInTrans );
      }
      else
      {
         pConnection->szTransBuffer[7] = (unsigned char)(pConnection->ulRecsInTrans & 0xff);
         pConnection->ulRecsInTrans = pConnection->ulRecsInTrans >> 8;
         pConnection->szTransBuffer[8] = (unsigned char)(pConnection->ulRecsInTrans & 0xff);
      }
      pConnection->szTransBuffer[pConnection->uiTBufOffset-1] = (BYTE) ( (bUnlockAll)? 0x41 : 0x40 );
      pData = leto_AddLen( (char*) (pConnection->szTransBuffer+4), &ulLen, 1 );

      if( bUnlockAll )
         hb_rddIterateWorkAreas( leto_UnLockRec, (void *) pConnection );

      if ( !leto_SendRecv( pArea, pData, ulLen, 1021 ) )
      {
         hb_retl( 0 );
         return;
      }
      else
      {
         pData = leto_firstchar();
         if( *(pData + 3) == ';' )
         {

            int i, iCount;
            ULONG ulAreaID, ulRecNo;
            FINDAREASTRU sArea;

            pData += 4;
            sscanf( pData, "%d;", &iCount );
            pData = strchr( pData, ';' );
            if( pData && iCount )
            {
               for( i = 0; i < iCount; i ++ )
               {
                  pData ++;
                  sscanf( pData, "%lu,%lu;", &ulAreaID, &ulRecNo );
                  if( ulAreaID && ulRecNo )
                  {
                     sArea.uiConnection = pTable->uiConnection;
                     sArea.ulAreaID = ulAreaID;
                     sArea.pArea = NULL;
                     hb_rddIterateWorkAreas( leto_FindArea, (void *) &sArea );
                     if( sArea.pArea && sArea.pArea->pTable->ulRecNo == 0 )
                        sArea.pArea->pTable->ulRecNo = ulRecNo;
                  }
                  pData = strchr( pData, ';' );
                  if( pData == NULL )
                     break;
               }
            }
         }
      }

   }
   hb_retl( 1 );
   pConnection->ulTransDataLen = pConnection->ulRecsInTrans = 0;
}

HB_FUNC( LETO_INTRANSACTION )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   hb_retl( leto_CheckArea( pArea ) && letoConnPool[pArea->pTable->uiConnection].bTransActive );
}

static char * leto_AddScopeExp( LETOAREAP pArea, char * pData, int iIndex )
{
   PHB_ITEM pScope = hb_param( iIndex, HB_IT_ANY );
   USHORT uiKeyLen;
   char szKey[LETO_MAX_KEY+1];

   if( pScope )
   {
      uiKeyLen = leto_KeyToStr( pArea, szKey, leto_ItemType( pScope ), pScope );
   }
   else
   {
      uiKeyLen = 0;
      szKey[0] = '\0';
   }
   *pData++ = (char) (uiKeyLen) & 0xFF;
   memcpy( pData, szKey, uiKeyLen );
   pData += uiKeyLen;

   pScope = hb_param( iIndex + 1, HB_IT_ANY );
   if( pScope )
   {
      uiKeyLen = leto_KeyToStr( pArea, szKey, leto_ItemType( pScope ), pScope );
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
   return pData;
}

HB_FUNC( LETO_GROUPBY )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;
   char szData[LETO_MAX_KEY*2+LETO_MAX_EXP+LETO_MAX_TAGNAME+47], *pData;
   const char *szGroup = (HB_ISCHAR( 1 ) ? hb_parc(1) : NULL);
   const char *szField, *szFilter;
   ULONG ulLen;

   if ( leto_CheckArea( pArea ) && szGroup && HB_ISCHAR( 2 ) )
   {
      if( pTable->uiUpdated )
         leto_PutRec( pArea, FALSE );

      szField = hb_parc( 2 );
      if( strchr(szField, ',') == NULL && ! hb_rddFieldIndex( (AREAP) pArea, szField ) )
      {
         hb_reta( 0 );
         return;
      }
      szFilter = HB_ISCHAR( 3 ) ? hb_parc( 3 ) : "";

      pData = szData + 4;
      sprintf( pData,"group;%lu;%s;%s;%s;%s;%c;", pTable->hTable,
         (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
         szGroup, szField, szFilter,
         (char)( ( (hb_setGetDeleted())? 0x41 : 0x40 ) ) );
      pData = ( pData + strlen( pData ) );

      pData = leto_AddScopeExp( pArea, pData, 4 );

      ulLen = pData - szData - 4;
      pData = leto_AddLen( (szData+4), &ulLen, 1 );
      if ( leto_SendRecv( pArea, pData, (ULONG) ulLen, 1021 ) )
      {
         ULONG ulRow, ulIndex;
         int uiCount, uiIndex, uiGroupLen;
         PHB_ITEM pItem = hb_itemNew( NULL );
         PHB_ITEM pArray = hb_itemNew( NULL );
         PHB_ITEM pSubArray;
         char * ptr;

         char cGroupType;
         int iLen, iWidth, iDec;
         BOOL fDbl;
         HB_MAXINT lValue;
         double dValue;

         pData = leto_firstchar();

         sscanf( pData, "%lu;", &ulRow );
         pData = strchr( pData, ';' ) + 1;
         sscanf( pData, "%d;", &uiCount );
         pData = strchr( pData, ';' ) + 1;
         sscanf( pData, "%c;", &cGroupType );
         pData = strchr( pData, ';' ) + 1;

         hb_arrayNew( pArray, ulRow );
         for( ulIndex = 1; ulIndex <= ulRow; ulIndex ++ )
            hb_arrayNew( hb_arrayGetItemPtr( pArray, ulIndex ), uiCount + 1 );

         ulIndex = 1;
         while( ulIndex <= ulRow )
         {
            pSubArray = hb_arrayGetItemPtr( pArray, ulIndex );
            switch( cGroupType )
            {
               case 'C':
               {
                  uiGroupLen = HB_GET_LE_UINT16( pData );
                  pData += 2;
                  hb_arraySetCL( pSubArray, 1, pData, uiGroupLen );
                  pData += uiGroupLen;
                  break;
               }
               case 'N':
                  ptr = strchr( pData, ',' );
                  iLen = ptr - pData;
                  fDbl = hb_valStrnToNum( pData, iLen, &lValue, &dValue, &iDec, &iWidth );
                  if( fDbl )
                     hb_arraySetND( pSubArray, 1, dValue );
                  else
                     hb_arraySetNL( pSubArray, 1, lValue );
                  pData = ptr;
                  break;
               case 'D':
                  hb_arraySetDS( pSubArray, 1, pData );
                  pData += 8;
                  break;
               case 'L':
                  hb_arraySetL( pSubArray, 1, ( *pData=='T' ) );
                  pData ++;
                  break;
            }
            if( *pData != ',' )
               break;

            uiIndex = 1;
            while( uiIndex <= uiCount )
            {
               pData ++;
               ptr = strchr(pData, uiIndex == uiCount ? ';' : ',');
               if( ! ptr)
                  break;

               iLen = ptr - pData;
               fDbl = hb_valStrnToNum( pData, iLen, &lValue, &dValue, &iDec, &iWidth );
               if( fDbl )
                  hb_itemPutNDLen( hb_arrayGetItemPtr( pSubArray, uiIndex + 1 ), dValue, iWidth, iDec );
               else
                  hb_arraySetNL( pSubArray, uiIndex + 1, lValue );

               pData = ptr;
               uiIndex ++;
            }

            if( *pData != ';' )
               break;
            pData ++;
            ulIndex ++;
         }

         hb_itemRelease( pItem );
         hb_itemReturnForward( pArray );

      }
      else
         hb_reta( 0 );
   }
}

HB_FUNC( LETO_SUM )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;
   char szData[LETO_MAX_KEY*2+LETO_MAX_EXP+LETO_MAX_TAGNAME+35], *pData;
   const char *szField, *szFilter;
   ULONG ulLen;

   if ( leto_CheckArea( pArea ) && HB_ISCHAR( 1 ) )
   {
      if( pTable->uiUpdated )
         leto_PutRec( pArea, FALSE );

      szField = hb_parc( 1 );
      szFilter = HB_ISCHAR( 2 ) ? hb_parc( 2 ) : "";

      pData = szData + 4;
      sprintf( pData,"sum;%lu;%s;%s;%s;%c;", pTable->hTable,
         (pArea->pTagCurrent)? pArea->pTagCurrent->TagName:"",
         szField, szFilter,
         (char)( ( (hb_setGetDeleted())? 0x41 : 0x40 ) ) );
      pData = ( pData + strlen( pData ) );

      pData = leto_AddScopeExp( pArea, pData, 3 );

      ulLen = pData - szData - 4;
      pData = leto_AddLen( (szData+4), &ulLen, 1 );
      if ( leto_SendRecv( pArea, pData, (ULONG) ulLen, 1021 ) )
      {
         int iWidth, iDec, iLen;
         BOOL fDbl;
         HB_MAXINT lValue;
         double dValue;
         char *ptr;

         pData = leto_firstchar();

         if( strchr(pData, ',') == NULL)
         {
            ptr = strchr(pData, ';');
            iLen = (ptr ? ptr - pData : (int) strlen(pData) );
            fDbl = hb_valStrnToNum( pData, iLen, &lValue, &dValue, &iDec, &iWidth );
            if ( !fDbl )
               hb_retnintlen( lValue, iWidth );
            else
               hb_retnlen( dValue, iWidth, iDec );
         }
         else
         {
            PHB_ITEM pArray = hb_itemNew( NULL );
            PHB_ITEM pItem = hb_itemNew( NULL );

            hb_arrayNew( pArray, 0);

            while(1)
            {
               ptr = strchr(pData, ',');
               if( !ptr )
                  ptr = strchr(pData, ';');
               iLen = (ptr ? ptr - pData : (int) strlen(pData) );
               fDbl = hb_valStrnToNum( pData, iLen, &lValue, &dValue, &iDec, &iWidth );

               hb_arrayAddForward( pArray, (fDbl ? hb_itemPutNDLen( pItem, dValue, iWidth, iDec) : hb_itemPutNL( pItem, lValue ) ) );

               if( ! ptr || (*ptr == ';') )
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

HB_FUNC( LETO_COMMIT )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;

   if( leto_CheckArea( pArea ) )
   {
      if( letoConnPool[pTable->uiConnection].bTransActive )
      {
         commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
      }
      else if( pTable->uiUpdated )
      {
         leto_PutRec( pArea, TRUE );
         pArea->fFLocked = FALSE;
         pTable->fRecLocked = 0;
         pArea->ulLocksMax = 0;
      }
      else
      {
         char szData[32];
         sprintf( szData,"flush;%lu;\r\n", pTable->hTable );
         if ( !leto_SendRecv( pArea, szData, 0, 1021 ) )
         {
            commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
         }
         if( letoUnLock( pArea, NULL ) == FAILURE )
         {
            commonError( pArea, EG_SYNTAX, 1031, 0, NULL, 0, NULL );
         }
      }
   }
}

HB_FUNC( LETO_PARSEREC )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;

   if( leto_CheckArea( pArea ) && HB_ISCHAR( 1 ) )
   {
      leto_ParseRec( pArea, (char *) hb_parc( 1 ), TRUE );
      pTable->ptrBuf = NULL;
   }
   hb_ret();
}

HB_FUNC( LETO_PARSERECORDS )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   LETOTABLE * pTable = pArea->pTable;

   if( leto_CheckArea( pArea ) && HB_ISCHAR( 1 ) )
   {
      const char * pBuffer = hb_parc(1), *ptr;
      int iLenLen = (((int)*pBuffer) & 0xFF);
      ULONG ulDataLen;

      if( iLenLen < 10 )
      {
         pTable->BufDirection = 1;

         ulDataLen = leto_b2n( pBuffer+1, iLenLen );
         ptr = pBuffer + 2 + iLenLen;
         leto_ParseRec( pArea, ptr, TRUE );

         leto_setSkipBuf( pTable, ptr, ulDataLen, 0 );
         pTable->ptrBuf = pTable->Buffer.pBuffer;
      }

   }
   hb_ret();
}

HB_FUNC( LETO_CONNECT_ERR )
{
   hb_retni( LetoGetConnectRes() );
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

   hb_retl( LetoSetFastAppend( b ) );
}

LETOCONNECTION * leto_getConnection( int iParam )
{
   LETOCONNECTION * pConnection = NULL;
   if( HB_ISCHAR( iParam ) )
   {
      char szAddr[96];
      int iPort;
      if( leto_getIpFromPath( hb_parc( iParam ), szAddr, &iPort, NULL, FALSE ) )
          pConnection = leto_ConnectionFind( szAddr, iPort );
   }
   else if( HB_ISNUM( iParam ) )
   {
      USHORT uiConn = hb_parni( iParam );
      if( uiConn > 0 && uiConn <= uiConnCount )
         pConnection = letoConnPool + uiConn - 1;
   }
   else
      pConnection = pCurrentConn;
   return pConnection;
}

HB_FUNC( LETO_CLOSEALL )
{
   LETOCONNECTION * pConnection = leto_getConnection( 1 );

   if( pConnection )
   {
      hb_retl( leto_CloseAll( pConnection ) );
   }
   else
      hb_retl( 0 );
}

HB_FUNC( LETO_RECLOCKLIST )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   if( leto_CheckArea( pArea ) && HB_ISARRAY( 1 ) )
   {
      PHB_ITEM pArray = hb_param( 1, HB_IT_ARRAY );
      HB_SIZE nLen = hb_arrayLen( pArray ), nIndex;

      if( ! pArea->pLocksPos )
      {
         /* Allocate locks array for the table, if it isn't allocated yet */
         pArea->ulLocksAlloc = nLen;
         pArea->pLocksPos = (ULONG*) hb_xgrab( sizeof(ULONG) * pArea->ulLocksAlloc );
         pArea->ulLocksMax = 0;
      }
      else if( pArea->ulLocksMax + nLen > pArea->ulLocksAlloc )
      {
         pArea->ulLocksAlloc = pArea->ulLocksMax + nLen;
         pArea->pLocksPos = (ULONG*) hb_xrealloc( pArea->pLocksPos, sizeof(ULONG) * (pArea->ulLocksAlloc) );
      }

      for( nIndex = 1; nIndex <= nLen; nIndex++ )
      {
         leto_AddRecLock( pArea, hb_arrayGetNL( pArray, nIndex ) );
      }
   }
   hb_ret();
}

#ifdef __BM
HB_FUNC( LETO_SETBM )
{
   LETOAREAP pArea = (LETOAREAP) hb_rddGetCurrentWorkAreaPointer();
   pArea->area.dbfi.lpvCargo = (void *) 1;
   leto_ClearBuffers( pArea );
}
#endif

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
