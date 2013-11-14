/*  $Id: letofunc.c,v 1.182 2010/08/20 09:30:01 alkresin Exp $  */

/*
 * Harbour Project source code:
 * Leto db server functions
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

#include "hbapi.h"
#include "hbapiitm.h"
#include "hbvm.h"
#include "hbxvm.h"
#ifdef __XHARBOUR__
   #include "hbfast.h"
#else
   #include "hbapicls.h"
#endif
#include "hbdate.h"
#include "srvleto.h"
#include "hbapierr.h"
#include "hbset.h"

#if defined(HB_OS_UNIX)
  #include <unistd.h>
  #include <sys/time.h>
  #include <sys/timeb.h>
#endif

#if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
   #define HB_SOCKET_T SOCKET
#else
   #define HB_SOCKET_T int
#endif

#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
   #define HB_MAXINT    HB_LONG
#endif

#define INIT_USERS_ALLOC   150
#define USERS_REALLOC       20
#define INIT_TABLES_ALLOC 1000
#define TABLES_REALLOC     100
#define INIT_INDEX_ALLOC     4
#define INDEX_REALLOC        4
#define INIT_AREAS_ALLOC  1000
#define AREAS_REALLOC      100

#ifndef HB_OS_PATH_DELIM_CHR
   #define HB_OS_PATH_DELIM_CHR OS_PATH_DELIMITER
#endif

#if !defined (SUCCESS)
#define SUCCESS            0
#define FAILURE            1
#endif

#ifdef __OLDRDD__
   #define PDBFAREA( x ) pArea->x
   #define leto_ClearFilter( pArea ) \
      pArea->dbfi.itmCobExpr = NULL; \
      pArea->dbfi.fFilter = FALSE;
#else
   #define PDBFAREA( x ) pArea->area.x
   #define leto_ClearFilter( pArea ) \
      pArea->area.dbfi.itmCobExpr = NULL; \
      pArea->area.dbfi.fFilter = FALSE;
#endif

#define LETO_MAX_TAGNAME 10
#define LETO_MAX_EXP 256

static DATABASE * s_pDB = NULL;

PTABLESTRU s_tables = NULL;
USHORT uiTablesAlloc = 0;                // Number of allocated table structures
static USHORT uiTablesMax   = 0;         // Higher index of table structure, which was busy
static USHORT uiTablesCurr  = 0;         // Current number of opened tables

static USHORT uiIndexMax   = 0;         // Higher index of index structure, which was busy
static USHORT uiIndexCurr  = 0;         // Current number of opened indexes

PUSERSTRU s_users = NULL;
static USHORT uiUsersAlloc = 0;         // Number of allocated user structures
USHORT uiUsersMax   = 0;                // Higher index of user structure, which was busy
USHORT uiUsersCurr  = 0;                // Current number of users

USHORT uiAnswerSent = 0;

ULONG ulOperations = 0;
ULONG ulBytesRead  = 0;
ULONG ulBytesSent  = 0;
static ULONG ulTransAll   = 0;
static ULONG ulTransOK    = 0;

static long   lStartDate;
static double dStartsec;
double dMaxWait    = 0;
double dMaxDayWait = 0;
long   ulToday     = 0;
double dSumWait[6];
unsigned int uiSumWait[6];

const char * szOk   = "++++";
const char * szErr2 = "-002";
const char * szErr3 = "-003";
const char * szErr4 = "-004";
const char * szErr101 = "-101";
const char * szErrAcc = "-ACC";
const char * szNull = "(null)";

const char * szErrUnlocked = "-004:38-1022-0-5";

#define ELETO_UNLOCKED 201

static char * pDataPath = NULL;
static USHORT uiDriverDef = 0;
static BOOL   bShareTables = 0;
static BOOL   bNoSaveWA = 0;
static BOOL   bFileFunc = 0;
static BOOL   bAnyExt = 0;
BOOL   bPass4L = 0;
BOOL   bPass4M = 0;
BOOL   bPass4D = 0;
BOOL   bCryptTraf = 0;
char * pAccPath  = NULL;
static BYTE * pBufCrypt = NULL;
static ULONG  ulBufCryptLen = 0;
ULONG  ulVarsMax = 10000;
USHORT uiVarLenMax = 10000;
USHORT uiVarsOwnMax = 50;
USHORT uiCacheRecords = 10;

PUSERSTRU pUStru_Curr = NULL;
ULONG  ulAreaID_Curr  = 0;

BOOL bHrbError = 0;

extern char * leto_memoread( const char * szFilename, ULONG *pulLen );

extern void leto_acc_read( const char * szFilename );
extern void leto_acc_release( void );
extern char * leto_acc_find( const char * szUser, const char * szPass );
extern BOOL leto_acc_add( const char * szUser, const char * szPass, const char * szAccess );
extern void leto_acc_flush( const char * szFilename );
extern void leto_vars_release( void );
extern void leto_varsown_release( PUSERSTRU pUStru );
extern void leto_SendAnswer( PUSERSTRU pUStru, const char* szData, ULONG ulLen );

void leto_CloseTable( USHORT nTableStru );
static int leto_IsRecLocked( PAREASTRU pAStru, ULONG ulRecNo );
static void leto_SetScope(DBFAREAP pArea, LETOTAG *pTag, BOOL bTop, BOOL bSet);


HB_FUNC( GET_CURR_USER )
{
   char sTemp[128];

   if( pUStru_Curr )
      sprintf( sTemp, "User: %s %s %s", pUStru_Curr->szAddr, pUStru_Curr->szNetname, pUStru_Curr->szExename );
   else
      sTemp[0] = '\0';
   hb_retc( sTemp );
}

HB_FUNC( SETHRBERROR )
{
   bHrbError = 1;
}

PAREASTRU leto_FindArea( PUSERSTRU pUStru, ULONG ulAreaID )
{
   PAREASTRU pAStru = pUStru->pAStru;
   USHORT ui;

   ulAreaID_Curr = ulAreaID;
   if( pAStru )
      for( ui=0; ui<pUStru->uiAreasAlloc; ui++,pAStru++ )
         if( pAStru->ulAreaNum == ulAreaID )
            return pAStru;
   return NULL;
}

PAREASTRU leto_FindAlias( PUSERSTRU pUStru, char * szAlias )
{
   PAREASTRU pAStru = pUStru->pAStru;
   USHORT ui;

   hb_strLower( szAlias, strlen(szAlias) );

   if( pAStru )
      for( ui=0; ui<pUStru->uiAreasAlloc; ui++,pAStru++ )
         if( ! strcmp( pAStru->szAlias, szAlias ) )
            return pAStru;
   return NULL;
}

DBFAREAP leto_SelectArea( ULONG ulAreaID, USHORT nUserStru, PAREASTRU * ppAStru )
{
   DBFAREAP pArea;

   hb_rddSelectWorkAreaNumber( ulAreaID/512 );
   pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();
   ulAreaID_Curr = ulAreaID;
   if( pArea )
   {
      hb_cdpSelect( PDBFAREA( cdPage ) );
      if( ppAStru )
      {
         PUSERSTRU pUStru = s_users + nUserStru;
         *ppAStru = leto_FindArea( pUStru, ulAreaID );
      }
   }
   return pArea;

}

ULONG leto_MilliSec( void )
{
#if defined(HB_OS_WIN_32) || defined( HB_OS_WIN )
   SYSTEMTIME st;
   GetLocalTime( &st );
   return (st.wMinute * 60 + st.wSecond) * 1000 + st.wMilliseconds;
#elif ( defined( HB_OS_LINUX ) || defined( HB_OS_BSD ) ) && !defined( __WATCOMC__ )
   struct timeval tv;
   gettimeofday( &tv, NULL );
   return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#else
   struct timeb tb;
   ftime( &tb );
   return tb.time * 1000 + tb.millitm;
#endif
}

void leto_profile( int iMode, int i1, ULONG ** parr1, ULONG ** parr2 )
{
   static ULONG arr1[10];
   static ULONG arr2[10];
   static ULONG arr3[10];
   ULONG ul = leto_MilliSec();

   switch( iMode )
   {
      case 0:
         memset( arr1,0,sizeof(ULONG)*10 );
         memset( arr2,0,sizeof(ULONG)*10 );
         memset( arr3,0,sizeof(ULONG)*10 );
         break;
      case 1:
         arr3[i1] = ul;
         break;
      case 2:
         if( arr3[i1] != 0 && ul >= arr3[i1] )
         {
            arr1[i1] += ul - arr3[i1];
            arr2[i1] ++;
            arr3[i1] = 0;
         }
         break;
      case 3:
         *parr1 = arr1;
         *parr2 = arr2;
         break;
   }
}

long leto_Date( void )
{
   int iYear, iMonth, iDay;
   hb_dateToday( &iYear, &iMonth, &iDay );
   return hb_dateEncode( iYear, iMonth, iDay );
}

HB_FUNC( LETO_READMEMAREA )
{
   char * pBuffer = (char*) hb_xgrab( hb_parni(2)+1 );
   if( !leto_ReadMemArea( pBuffer, hb_parni(1), hb_parni(2) ) )
   {
      hb_xfree( pBuffer );
      hb_ret();
   }
   else
      hb_retc_buffer( pBuffer );
}

HB_FUNC( LETO_WRITEMEMAREA )
{
   leto_WriteMemArea( hb_parc(1), hb_parni(2), hb_parni(3) );
}

HB_FUNC( GETCMDITEM )
{
   PHB_ITEM pText = hb_param( 1, HB_IT_STRING );
   PHB_ITEM pStart = hb_param( 2, HB_IT_NUMERIC );

   if( pText && pStart )
   {
      ULONG ulSize = hb_itemGetCLen( pText );
      ULONG ulStart = pStart ? hb_itemGetNL( pStart ) : 1;
      ULONG ulEnd = ulSize, ulAt;
      const char *Text = hb_itemGetCPtr( pText );

      if ( ulStart > ulSize )
      {
         hb_retc( NULL );
         if( HB_ISBYREF( 3 ) ) hb_storni( 0, 3 );
      }
      else
      {
         ulAt = hb_strAt( ";", 1, Text + ulStart - 1, ulEnd - ulStart + 1 );
         if ( ulAt > 0)
            ulAt += ( ulStart - 1 );

         if( HB_ISBYREF( 3 ) )
            hb_storni( ulAt, 3 );

         if ( ulAt != 0)
         {
            ULONG ulLen = ulAt - ulStart;

            if( ulStart ) ulStart--;

            if( ulStart < ulSize )
            {
               if( ulLen > ulSize - ulStart )
               {
                  ulLen = ulSize - ulStart;
               }

               if( ulLen > 0 )
               {
                  if( ulLen == ulSize )
                     hb_itemReturn( pText );
                  else
                     hb_retclen( Text + ulStart, ulLen );
               }
               else
                  hb_retc( NULL );
            }
            else
               hb_retc( NULL );
         }
         else
            hb_retc( NULL );
      }
   }
   else
   {
      if( HB_ISBYREF( 3 ) )
         hb_storni( 0, 3 );
      hb_retc( NULL );
   }
}

BOOL leto_checkExpr( const char* pExpr, int iLen )
{
   PHB_ITEM pItem = hb_itemPutCL( NULL, pExpr, iLen );
   const char * szType;

   hb_xvmSeqBegin();
#ifndef __XHARBOUR__
   szType = hb_macroGetType( pItem );
#else
   szType = hb_macroGetType( pItem, 64 ); // HB_SM_RT_MACRO
#endif
   hb_xvmSeqEnd();
   hb_itemClear( pItem );
   hb_itemRelease( pItem );
   if( bHrbError )
      return FALSE;
   else
      return szType[0] == 'L';
}

void leto_IndexInfo( char * szRet, int iNumber, const char * szKey )
{
   int iLen;
   DBORDERINFO pOrderInfo;
   AREAP pArea = hb_rddGetCurrentWorkAreaPointer();

   if( pArea )
   {
      memset( &pOrderInfo, 0, sizeof( DBORDERINFO ) );
      pOrderInfo.itmOrder = hb_itemPutNI( NULL, iNumber );
      pOrderInfo.itmResult = hb_itemPutC( NULL, "" );
      SELF_ORDINFO( pArea, DBOI_BAGNAME, &pOrderInfo );
      strcpy(szRet, hb_itemGetCPtr( pOrderInfo.itmResult ) );
      iLen = strlen(szRet);
      szRet[ iLen ++ ] = ';';
      hb_itemClear( pOrderInfo.itmResult );

      hb_itemPutC( pOrderInfo.itmResult, "" );
      SELF_ORDINFO( pArea, DBOI_NAME, &pOrderInfo );
      strcpy(szRet + iLen, hb_itemGetCPtr( pOrderInfo.itmResult ) );
      iLen = strlen(szRet);
      szRet[ iLen ++ ] = ';';
      hb_itemClear( pOrderInfo.itmResult );

      strcpy(szRet + iLen, szKey );
      iLen = strlen(szRet);
      szRet[ iLen ++ ] = ';';

      hb_itemPutC( pOrderInfo.itmResult, "" );
      SELF_ORDINFO( pArea, DBOI_CONDITION, &pOrderInfo );
      strcpy(szRet + iLen, hb_itemGetCPtr( pOrderInfo.itmResult ) );
      iLen = strlen(szRet);
      szRet[ iLen ++ ] = ';';
      hb_itemClear( pOrderInfo.itmResult );

      hb_itemPutC( pOrderInfo.itmResult, "" );
      SELF_ORDINFO( pArea, DBOI_KEYTYPE, &pOrderInfo );
      strcpy(szRet + iLen, hb_itemGetCPtr( pOrderInfo.itmResult ) );
      iLen = strlen(szRet);
      szRet[ iLen ++ ] = ';';
      hb_itemClear( pOrderInfo.itmResult );

      hb_itemPutNI( pOrderInfo.itmResult, 0 );
      SELF_ORDINFO( pArea, DBOI_KEYSIZE, &pOrderInfo );
      sprintf( szRet + iLen, "%lu;", hb_itemGetNL(pOrderInfo.itmResult) );

      hb_itemRelease( pOrderInfo.itmOrder );
      hb_itemRelease( pOrderInfo.itmResult );

   }
   else
   {
      szRet[0] = 0;
   }
}

HB_FUNC( LETO_INDEXINFO )
{
   char szRet[_POSIX_PATH_MAX + LETO_MAX_TAGNAME + LETO_MAX_EXP*2 + 20];
   leto_IndexInfo( szRet, hb_parni(1), hb_parc(2) );
   hb_retc( szRet );
}

#define MAXDEEP  5

BOOL leto_ParseFilter( const char * pNew, ULONG ulFltLen )
{
   int iMode = 0;
   BOOL bLastCond=0;
   int iDeep = 0;
   int piDeep[MAXDEEP];
   const char *ppDeep[MAXDEEP];
   const char * ptr, * ptr1;
   char c, cQuo=' ';

   for( ptr=ptr1=pNew; ulFltLen; ptr++,ulFltLen-- )
   {
      c = *ptr;
      if( iMode == 0 )
      {
         if( c == '\"' || c == '\'' )
         {
            iMode = 1;        // mode 1 - a string
            cQuo = c;
         }
         else if( ( c >= 'A' && c <= 'Z' ) || c == '_' || ( c >= 'a' && c <= 'z' ) )
         {
            iMode = 2;        // mode 2 - an identifier
            ppDeep[iDeep] = ptr;
         }
         else if( c >= '0' && c <= '9' )
         {
            iMode = 3;        // mode 3 - a number
         }
         else if( c == '(' )  // parenthesis, but not of a function
         {
            piDeep[iDeep] = 1;
            ppDeep[iDeep] = ptr+1;
            if( ++iDeep >= MAXDEEP )
            {
               return FALSE;
            }
            piDeep[iDeep] = 0;
            ppDeep[iDeep] = NULL;
         }
         else if( ( c == '>' ) && ( ptr > pNew ) && ( *(ptr-1) == '-' ) )
         {
            return FALSE;
         }
      }
      else if( iMode == 1 )   // We are inside a string
      {
         if( c == cQuo )
            iMode = 0;
      }
      else if( iMode == 2 )   // We are inside an identifier
      {
         if( c == '(' )
         {
            piDeep[iDeep] = 2;
            if( ++iDeep >= MAXDEEP )
            {
               return FALSE;
            }
            piDeep[iDeep] = 0;
            ppDeep[iDeep] = NULL;
            iMode = 0;
         }
         else if( !( ( c >= 'A' && c <= 'Z' ) || c == '_' || ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == ' ' ) )
         {
            iMode = 0;
         }
      }
      else if( iMode == 3 )   // We are inside a number
      {
         if( !( ( c >= '0' && c <= '9' ) || ( c =='.' && ulFltLen > 1 && *(ptr+1)>='0' && *(ptr+1)<='9' ) ) )
            iMode = 0;
      }
      if( iMode != 1 )
      {
         if( c == ')' )
         {
            if( --iDeep < 0 )
            {
               return FALSE;
            }
            if( bLastCond && ptr1 > ppDeep[iDeep] )
            {
               if( !leto_checkExpr( ptr1, ptr-ptr1 ) )
               {
                  return FALSE;
               }
               ptr1 = ptr + 1;
               bLastCond = 0;
            }
         }
         else if( iMode != 3 && c == '.'  && (
               ( ulFltLen >= 5 && *(ptr+4) == '.' && ( *(ptr+1)=='A' || *(ptr+1)=='a' ) && ( *(ptr+2)=='N' || *(ptr+2)=='n' ) && ( *(ptr+3)=='D' || *(ptr+3)=='d' ) )
               || ( ulFltLen >= 4 && *(ptr+3) == '.' && ( *(ptr+1)=='O' || *(ptr+1)=='o' ) && ( *(ptr+2)=='R' || *(ptr+2)=='r' ) ) ) )
         {
            // ptr stays at a beginning of .and. or .or.
            if( iDeep && ( ppDeep[iDeep-1] > ptr1 ) )
            {
               ptr1 = ppDeep[iDeep-1];
               bLastCond = 1;
            }
            if( bLastCond )
            {
               if( iDeep && piDeep[iDeep-1] == 2 )
                  while( *ptr1 != '(' ) ptr1 ++;
               if( *ptr1 == '(' )
                  ptr1 ++;
               if( ( ptr > (ptr1+1) ) && !leto_checkExpr( ptr1, ptr-ptr1 ) )
               {
                  return FALSE;
               }
            }
            ptr += 3;
            ulFltLen -= 3;
            if( ulFltLen > 0 && *ptr != '.' )
            {
               ptr ++;
               ulFltLen --;
            }
            ptr1 = ptr + 1;
            bLastCond = 1;
         }
      }
   }
   ptr --;
   if( (ptr1 > pNew) && ((ptr - ptr1) > 2) && !leto_checkExpr( ptr1, ptr-ptr1+1 ) )
   {
      return FALSE;
   }

   return TRUE;
}

HB_FUNC( LETO_SETAPPOPTIONS )
{
   USHORT uiLen;

   if( !HB_ISNIL(1) )
   {
      uiLen = (USHORT) hb_parclen(1);
      pDataPath = (char*) hb_xgrab( uiLen+1 );
      memcpy( pDataPath, hb_parc(1), uiLen );
      pDataPath[uiLen] = '\0';
   }

   uiDriverDef = hb_parni(2);
   bFileFunc = hb_parl(3);
   bAnyExt   = hb_parl(4);
   bPass4L   = hb_parl(5);
   bPass4M   = hb_parl(6);
   bPass4D   = hb_parl(7);

   if( !HB_ISNIL(8) )
   {
      uiLen = (USHORT) hb_parclen(8);
      pAccPath = (char*) hb_xgrab( uiLen+1 );
      memcpy( pAccPath, hb_parc(8), uiLen );
      pAccPath[uiLen] = '\0';
      leto_acc_read( pAccPath );
   }
   bCryptTraf = hb_parl(9);
   bShareTables = hb_parl(10);
   bNoSaveWA = hb_parl(11);

   if( HB_ISNUM(12) )
      ulVarsMax = hb_parnl(12);
   if( HB_ISNUM(13) )
      uiVarLenMax = hb_parnl(13);
   if( HB_ISNUM(14) )
      uiCacheRecords = hb_parnl(14);
}

HB_FUNC( LETO_GETAPPOPTIONS )
{
   USHORT uiNum = hb_parni(1);

   switch( uiNum )
   {
      case 1:
         hb_retc( pDataPath );
         break;
      case 2:
         hb_retni( uiDriverDef );
         break;
      case 4:
         hb_retl( bAnyExt );
         break;
      case 10:
         hb_retl( bShareTables );
         break;
      case 11:
         hb_retl( bNoSaveWA );
         break;
   }
}

HB_FUNC( LETO_SETSHARED )
{
   PTABLESTRU pTStru = s_tables + hb_parni( 1 );
   BOOL bRes = pTStru->bShared;
   if( HB_ISLOG(2) )
      pTStru->bShared = hb_parl( 2 );

   hb_retl( bRes );
}

HB_FUNC( LETO_TABLENAME )
{
   ULONG ulAreaID = (ULONG) hb_parnl(1);
   USHORT nUserStru = (HB_ISNIL(2))? 0 : (USHORT) hb_parni( 2 );
   PAREASTRU pAStru;
   PUSERSTRU pUStru = s_users + nUserStru;

   if( ( nUserStru < uiUsersMax ) && ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) != NULL )
      hb_retc( (char*) pAStru->pTStru->szTable );
   else
      hb_retc( "" );

}

char * leto_RealAlias( PUSERSTRU pUStru, char * szClientAlias )
{
   PAREASTRU pAStru = leto_FindAlias( pUStru, szClientAlias );
   if( pAStru )
   {
      AREAP pArea = hb_rddGetWorkAreaPointer( pAStru->ulAreaNum/512 );

      if( pArea )
      {
         char * szAlias = hb_xgrab( HB_RDD_MAX_ALIAS_LEN + 1 );

         SELF_ALIAS( pArea, ( BYTE * ) szAlias );
         return szAlias;
      }

   }
   return "";
}

HB_FUNC( LETO_ALIAS )
{
   USHORT nUserStru = (USHORT) hb_parni( 1 );
   if( HB_ISNUM(1) && HB_ISCHAR(2) && ( nUserStru < uiUsersMax ) )
   {
      hb_retc_buffer( leto_RealAlias( s_users + nUserStru, hb_parc(2) ) );
   }
   else
      hb_retc( "" );
}

HB_FUNC( LETO_SETFILTER )
{
   ULONG ulAreaID = (ULONG) hb_parnl(1);
   USHORT nUserStru = (HB_ISNIL(2))? 0 : (USHORT) hb_parni( 2 );
   PAREASTRU pAStru;
   PUSERSTRU pUStru = s_users + nUserStru;
   PHB_ITEM pItem = (HB_ISNIL(3))? NULL : hb_param( 3, HB_IT_BLOCK );

   if( ( nUserStru < uiUsersMax ) && ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) != NULL )
   {
      if( pAStru->itmFltExpr )
      {
         hb_itemClear( pAStru->itmFltExpr );
         hb_itemRelease( pAStru->itmFltExpr );
      }
      pAStru->itmFltExpr = (pItem)? hb_itemNew( pItem ) : NULL;
      hb_retl( 1 );
   }
   else
      hb_retl( 0 );
}

LETOTAG * leto_FindTag( PAREASTRU pAStru, const char * szOrder )
{
   LETOTAG *pTag = pAStru->pTag, *pTagCurrent = NULL;

   while( pTag )
   {
      if( ! strcmp( pTag->szTagName, szOrder) )
      {
         pTagCurrent = pTag;
         break;
      }
      else
         pTag = pTag->pNext;
   }

   return pTagCurrent;
}

HB_FUNC( LETO_ADDTAG )
{
   ULONG ulAreaID = (ULONG) hb_parnl(1);
   USHORT nUserStru = (HB_ISNIL(2))? 0 : (USHORT) hb_parni( 2 );
   PAREASTRU pAStru;
   PUSERSTRU pUStru = s_users + nUserStru;
   USHORT nIndexStru = hb_parni(3);
   const char * szOrder = hb_parc(4);
   ULONG ulLen = hb_parclen(4);

   if( ( nUserStru < uiUsersMax ) && ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) != NULL )
   {
      LETOTAG * pTag, * pTagNext;

      if( ulLen > 11 )
      {
         ulLen = 11;
         // szOrder[11] = '\0';
      }
      // hb_strLower( szOrder, uiLen );
      if( !leto_FindTag( pAStru, szOrder ) )
      {
         pTag = (LETOTAG*) hb_xgrab( sizeof(LETOTAG) );
         memset( pTag, 0, sizeof(LETOTAG) );
         pTag->pIStru = pAStru->pTStru->pIStru + nIndexStru;
         memcpy( pTag->szTagName, szOrder, ulLen );

         if( pAStru->pTag )
         {
            pTagNext = pAStru->pTag;
            while( pTagNext->pNext )
               pTagNext = pTagNext->pNext;
            pTagNext->pNext = pTag;
         }
         else
            pAStru->pTag = pTag;
      }
      hb_retl( 1 );
   }
   else
      hb_retl( 0 );
}

HB_FUNC( LETO_N2B )
{
   unsigned long int n = (unsigned long int)hb_parnl(1);
   int i = 0;
   unsigned char s[8];

   do
   {
      s[i] = (unsigned char)(n & 0xff);
      n = n >> 8;
      i ++;
   }
   while( n );
   s[i] = '\0';
   hb_retclen( (char*)s,i );
}

HB_FUNC( LETO_B2N )
{
   ULONG n = 0;
   int i = 0;
   BYTE *s = (BYTE*) hb_parc(1);

   do
   {
      n += (unsigned long int)(s[i] << (8*i));
      i ++;
   }
   while( s[i] );
   hb_retnl( n );
}

int leto_GetParam( const char *szData, const char **pp2, const char **pp3, const char **pp4, const char **pp5 )
{
   const char * ptr;
   int iRes = 0;

   if( ( ptr = strchr( szData, ';' ) ) != NULL )
   {
      iRes ++;
      if( pp2 )
      {
         *pp2 = ++ptr;
         if( ( ptr = strchr( *pp2, ';' ) ) != NULL )
         {
            iRes ++;
            if( pp3 )
            {
               *pp3 = ++ptr;
               if( ( ptr = strchr( *pp3, ';' ) ) != NULL )
               {
                  iRes ++;
                  if( pp4 )
                  {
                     *pp4 = ++ptr;
                     if( ( ptr = strchr( *pp4, ';' ) ) != NULL )
                     {
                        iRes ++;
                        if( pp5 )
                        {
                           *pp5 = ++ptr;
                           if( ( ptr = strchr( *pp5, ';' ) ) != NULL )
                           {
                              iRes ++;
                              ptr ++;
                              if( ( ptr = strchr( ptr, ';' ) ) != NULL )
                              {
                                 iRes ++;
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   return iRes;
}

HB_FUNC ( LETO_SELECTAREA )
{
   ULONG ulAreaID = (ULONG) hb_parnl(1);
   USHORT nUserStru = (HB_ISNIL(2))? 0 : (USHORT) hb_parni( 2 );
   PAREASTRU pAStru = NULL;

   if( leto_SelectArea( ulAreaID, nUserStru, &pAStru ) && pAStru )
      hb_retl( 1 );
   else
      hb_retl( 0 );
}


#define LETO_FLG_BOF        1
#define LETO_FLG_EOF        2
#define LETO_FLG_LOCKED     4
#define LETO_FLG_DEL        8
#define LETO_FLG_FOUND     16

#define SHIFT_FOR_LEN       3

// 3 bytes - len
//   1 byte  - flags
//   ? bytes - RECNO + ';'
//     //for each field
//       //if eof()
//          1 byte = '\0'
//       //else
//          HB_FT_STRING:   if(FieldLen>255 ? 2 : 1) bytes + field
//          HB_FT_LOGICAL:  field
//          HB_FT_MEMO:     1 byte
//          binary fields:  field
//          OTHER:          1 byte + field
static ULONG leto_rec( PAREASTRU pAStru, char * szData )
{
   char * pData, * ptr, * ptrEnd;
   BOOL bRecLocked;
   USHORT ui;
   USHORT uiLen;
   ULONG ulRealLen;
   LPFIELD pField;
   PHB_ITEM pItem;
   DBFAREAP pArea = leto_SelectArea( pAStru->ulAreaNum, 0, NULL );

   if( bCryptTraf )
   {
      ULONG ulLen = pArea->uiRecordLen + PDBFAREA( uiFieldCount ) * 3 + 24;
      if( ulLen > ulBufCryptLen )
      {
         if( !ulBufCryptLen )
            pBufCrypt = (BYTE*) hb_xgrab( ulLen );
         else
            pBufCrypt = (BYTE*) hb_xrealloc( pBufCrypt, ulLen );
         ulBufCryptLen = ulLen;
      }
      pData = ( char * ) pBufCrypt;
   }
   else
      pData = szData + SHIFT_FOR_LEN;

   pItem = hb_itemNew( NULL );
   if( SELF_GETVALUE( (AREAP)pArea, 1, pItem ) != SUCCESS )
   {
      ulRealLen = 0;
      hb_itemClear( pItem );
   }
   else
   {
      hb_itemClear( pItem );
      bRecLocked = ( leto_IsRecLocked( pAStru, pArea->ulRecNo ) == 1 );
      *pData = 0x40 + ((PDBFAREA( fBof ))? LETO_FLG_BOF:0) +
                      ((PDBFAREA( fEof ))? LETO_FLG_EOF:0) +
                      ((bRecLocked)? LETO_FLG_LOCKED:0) +
                      ((pArea->fDeleted)? LETO_FLG_DEL:0) +
                      ((PDBFAREA( fFound ))? LETO_FLG_FOUND:0);
      pData ++;
      sprintf( (char*) pData, "%lu;", pArea->ulRecNo );
      pData += strlen( (char*) pData );

      for( ui = 0; ui < PDBFAREA( uiFieldCount ); ui++ )
      {
         pField = PDBFAREA( lpFields ) + ui;
         ptr = ( char * ) pArea->pRecord + pArea->pFieldOffset[ui];
         ptrEnd = ptr + pField->uiLen - 1;
         if( PDBFAREA( fEof ) )
         {
            *pData = '\0';
            pData ++;
         }
         else
         {
            switch( pField->uiType )
            {
               case HB_FT_STRING:
                  while( ptrEnd > ptr && *ptrEnd == ' ' ) ptrEnd --;
                  ulRealLen = ptrEnd - ptr + 1;
                  if( ulRealLen == 1 && *ptr == ' ' )
                     ulRealLen = 0;
                  // Trimmed field length
                  if( pField->uiLen < 256 )
                  {
                     pData[0] = (BYTE) ulRealLen & 0xFF;
                     uiLen = 1;
                  }
                  else
                  {
                     uiLen = leto_n2b( pData+1, ulRealLen );
                     pData[0] = (BYTE) uiLen & 0xFF;
                     uiLen ++;
                  }
                  pData += uiLen;
                  if( ulRealLen > 0 )
                     memcpy( pData, ptr, ulRealLen );
                  pData += ulRealLen;
                  break;

               case HB_FT_LONG:
               case HB_FT_FLOAT:
                  while( ptrEnd > ptr && *ptr == ' ' ) ptr ++;
                  ulRealLen = ptrEnd - ptr + 1;
                  while( ptrEnd > ptr && ( *ptrEnd == '0' || *ptrEnd == '.' ) ) ptrEnd --;
                  if( *ptrEnd == '0' || *ptrEnd == '.' )
                     *pData++ = '\0';
                  else
                  {
                     *pData++ = (BYTE) ulRealLen & 0xFF;
                     memcpy( pData, ptr, ulRealLen );
                     pData += ulRealLen;
                  }
                  break;

               case HB_FT_DATE:
               {
                  if( *ptr <= ' ' && pField->uiLen == 8 )
                     *pData++ = '\0';
                  else
                  {
                     memcpy( pData, ptr, pField->uiLen );
                     pData += pField->uiLen;
                  }
                  break;
               }
               case HB_FT_LOGICAL:
                  *pData++ = *ptr;
                  break;
               case HB_FT_MEMO:
               case HB_FT_BLOB:
               case HB_FT_PICTURE:
               case HB_FT_OLE:
                  SELF_GETVALUE( (AREAP)pArea, ui+1, pItem );
                  if( hb_itemGetCLen( pItem ) )
                     *pData++ = '!';
                  else
                     *pData++ = '\0';
                  hb_itemClear( pItem );
                  break;

               // binary fields
               case HB_FT_INTEGER:
               case HB_FT_CURRENCY:
               case HB_FT_DOUBLE:
               case HB_FT_MODTIME:
               case HB_FT_DAYTIME:
                  memcpy( pData, ptr, pField->uiLen );
                  pData += pField->uiLen;
                  break;

               case HB_FT_ANY:
                  SELF_GETVALUE( (AREAP)pArea, ui+1, pItem );
                  if( pField->uiLen == 3 || pField->uiLen == 4 )
                  {
                     memcpy( pData, ptr, pField->uiLen );
                     pData += pField->uiLen;
                  }
                  else if( HB_IS_LOGICAL( pItem ) )
                  {
                     *pData++ = 'L';
                     *pData++ = (hb_itemGetL( pItem ) ? 'T' : 'F' );
                  }
                  else if( HB_IS_DATE( pItem ) )
                  {
                     *pData++ = 'D';
                     hb_itemGetDS( pItem, (char *) pData);
                     pData += 8;
                  }
                  else if( HB_IS_STRING( pItem ) )
                  {
                     uiLen = (USHORT) hb_itemGetCLen( pItem );
                     if( uiLen <= pField->uiLen - 3 )
                     {
                        *pData++ = 'C';
                        *pData++ = (BYTE) uiLen & 0xFF;
                        *pData++ = (BYTE) (uiLen >> 8) & 0xFF;
                        memcpy( pData, hb_itemGetCPtr( pItem ), uiLen );
                        pData += uiLen;
                     }
                     else
                     {
                        *pData++ = '!';
                     }
                  }
                  else if( HB_IS_NUMERIC( pItem ) )
                  {
                     char * szString = hb_itemStr( pItem, NULL, NULL );
                     char * szTemp;
                     *pData++ = 'N';

                     if( szString )
                     {
                        szTemp = szString;
                        while( HB_ISSPACE( *szTemp ) )
                           szTemp ++;
                        uiLen = strlen( szTemp );
                        memcpy( pData, szTemp, uiLen );
                        pData += uiLen;
                        hb_xfree( szString );
                     }
                     *pData++ = '\0';
                  }
                  else
                  {
                     *pData++ = 'U';
                  }
                  hb_itemClear( pItem );
                  break;
            }
         }
      }

      if( bCryptTraf )
      {
         ulRealLen = pData - ( char * ) pBufCrypt;
         leto_encrypt( ( const char * ) pBufCrypt, ulRealLen, szData + SHIFT_FOR_LEN, &ulRealLen, LETO_PASSWORD );
      }
      else
         ulRealLen = pData - szData - SHIFT_FOR_LEN;

      *szData = (BYTE) (ulRealLen & 0xff);
      *(szData+1) = (BYTE) ((ulRealLen >> 8) & 0xff);
      *(szData+2) = (BYTE) ((ulRealLen >> 16) & 0xff);
      ulRealLen += SHIFT_FOR_LEN;
   }
   hb_itemRelease( pItem );

   return ulRealLen;
}

HB_FUNC( LETO_REC )
{
   DBFAREAP pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();
   char * szData;
   USHORT uiRealLen;
   ULONG ulAreaID = (ULONG) hb_parnl(1);
   USHORT nUserStru = (HB_ISNIL(2))? 0 : (USHORT) hb_parni( 2 );
   PAREASTRU pAStru;
   PUSERSTRU pUStru = s_users + nUserStru;

   if( ( nUserStru < uiUsersMax ) && ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) != NULL )
   {
      szData = (char*) malloc( pArea->uiRecordLen + PDBFAREA( uiFieldCount ) * 3 + 24 );

      uiRealLen = (USHORT) leto_rec( pAStru, szData );

      hb_retclen( szData, uiRealLen );
      free( szData );
   }
   else
      hb_retc( "" );
}

static void leto_UnlockAllRec( PAREASTRU pAStru )
{
   PTABLESTRU pTStru = pAStru->pTStru;
   DBFAREAP pArea = NULL;
   ULONG ul, ul2;
   ULONG * pLockA, * pLockT;

   if( bNoSaveWA )
   {
      hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
      pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();
      SELF_UNLOCK( (AREAP)pArea, NULL );
   }
   else if( pAStru->pLocksPos )
   {
      PHB_ITEM pItem = NULL;

      if( bShareTables )
      {
         hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
         pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();
         pItem = hb_itemPutNL( NULL, 0 );
      }
      for( ul=0,pLockA=pAStru->pLocksPos; ul<pAStru->ulLocksMax; ul++,pLockA++ )
      {
        if( bShareTables )
        {
           hb_itemPutNL( pItem, *pLockA );
           SELF_UNLOCK( (AREAP)pArea, pItem );
        }
        for( ul2=0,pLockT=pTStru->pLocksPos; ul2<pTStru->ulLocksMax; ul2++,pLockT++ )
           if( *pLockT == *pLockA )
           {
              pTStru->ulLocksMax--;
              (*pLockT) = *( pTStru->pLocksPos + pTStru->ulLocksMax );
              break;
           }
      }
      pAStru->ulLocksMax = 0;

      if( bShareTables )
         hb_itemRelease( pItem );
   }
}

void leto_CloseIndex( PINDEXSTRU pIStru )
{

   if( pIStru->BagName )
   {
      hb_xfree( pIStru->BagName );
      pIStru->BagName = NULL;
   }
   if( pIStru->szFullName )
   {
      hb_xfree( pIStru->szFullName );
      pIStru->szFullName = NULL;
   }
   pIStru->uiAreas = 0;
   uiIndexCurr --;
}

HB_FUNC( LETO_INITINDEX )
{
   ULONG ulAreaID = (ULONG) hb_parnl(1);
   USHORT nUserStru = (HB_ISNIL(2))? 0 : (USHORT) hb_parni( 2 );
   PAREASTRU pAStru;
   PUSERSTRU pUStru = s_users + nUserStru;

   if( ( nUserStru < uiUsersMax ) && ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) != NULL )
   {
      PTABLESTRU pTStru;
      PINDEXSTRU pIStru;
      char * szBagName;
      USHORT ui = 0;
      USHORT uiLen;
      BOOL bRes = 0;

      pTStru = pAStru->pTStru;
      pIStru = pTStru->pIStru;

      uiLen = (USHORT) hb_parclen(3);
      szBagName = (char*) hb_xgrab( uiLen + 1 );
      memcpy( szBagName, hb_parc(3), uiLen );
      szBagName[uiLen] = '\0';
      hb_strLower( szBagName, (ULONG)uiLen );

      while( ui < pTStru->uiIndexAlloc )
      {
        if( pIStru->BagName && !strcmp( szBagName, pIStru->BagName ) )
        {
           LETOTAG * pTag = pAStru->pTag;
           while( pTag )
           {
              if( pTag->pIStru == pIStru )
              {
                 bRes = 1;
                 break;
              }
              pTag = pTag->pNext;
           }
           if( !bRes )
              pIStru->uiAreas ++;
           hb_xfree( szBagName );
           hb_retni( ui );
           return;
        }
        pIStru ++;
        ui ++;
      }

      ui = 0;
      pIStru = pTStru->pIStru;
      while( ui < pTStru->uiIndexAlloc && pIStru->BagName )
      {
        pIStru ++;
        ui ++;
      }
      if( ui == pTStru->uiIndexAlloc )
      {
         pTStru->pIStru = (INDEXSTRU*) hb_xrealloc( pTStru->pIStru, sizeof(INDEXSTRU) * ( pTStru->uiIndexAlloc+INDEX_REALLOC ) );
         memset( pTStru->pIStru+pTStru->uiIndexAlloc, 0, sizeof(INDEXSTRU) * INDEX_REALLOC );
         pIStru = pTStru->pIStru + pTStru->uiIndexAlloc;
         pTStru->uiIndexAlloc += INDEX_REALLOC;
      }
      pIStru->uiAreas = 1;
      pIStru->BagName = szBagName;
      pIStru->bCompound = ( !pTStru->uiDriver && !leto_BagCheck( ( const char * ) pTStru->szTable, szBagName ) );
      if( !HB_ISNIL(4) )
      {
         uiLen = (USHORT) hb_parclen(4);
         szBagName = (char*) hb_xgrab( uiLen + 1 );
         memcpy( szBagName, hb_parc(4), uiLen );
         szBagName[uiLen] = '\0';
         pIStru->szFullName = szBagName;
      }
      hb_retni( ui );
      uiIndexCurr ++;
      if( uiIndexCurr > uiIndexMax )
         uiIndexMax = uiIndexCurr;
   }
   else
      hb_retni( -1 );
}

int leto_FindTable( const char *szPar, BOOL bSelect )
{
   PTABLESTRU pTStru = s_tables;
   char szFile[_POSIX_PATH_MAX + 1];
   USHORT ui = 0;
   USHORT uiLen = (USHORT) strlen(szPar);

   memcpy( szFile, szPar, uiLen );
   szFile[uiLen] = '\0';
   hb_strLower( (char*)szFile, (ULONG)uiLen );

   while( ui < uiTablesAlloc )
   {
      if( pTStru->szTable && !strcmp( (char*)pTStru->szTable,szFile ) )
      {
         if( bSelect )
            hb_rddSelectWorkAreaNumber( pTStru->uiAreaNum );
         return ui;
      }
      pTStru ++;
      ui ++;
   }
   return -1;
}

HB_FUNC( LETO_FINDTABLE )
{
   hb_retni( leto_FindTable( hb_parc(1), hb_parl(2) ) );
}

void leto_CloseTable( USHORT nTableStru )
{
   PTABLESTRU pTStru = s_tables + nTableStru;
   PINDEXSTRU pIStru = pTStru->pIStru;
   USHORT ui;

   if( !bNoSaveWA )
   {
      hb_rddSelectWorkAreaNumber( pTStru->uiAreaNum );
      hb_rddReleaseCurrentArea();
   }

   if( pTStru->szTable )
   {
      hb_xfree( pTStru->szTable );
      pTStru->szTable = NULL;
   }
   if( pTStru->pLocksPos )
   {
      hb_xfree( pTStru->pLocksPos );
      pTStru->pLocksPos = NULL;
   }
   pTStru->bLocked = FALSE;
   ui = 0;
   if( pIStru )
   {
      while( ui < pTStru->uiIndexAlloc )
      {
         if( pIStru->BagName )
            leto_CloseIndex( pIStru );
         ui ++;
         pIStru ++;
      }
      hb_xfree( pTStru->pIStru );
      pTStru->pIStru = NULL;
   }

   pTStru->uiAreaNum = pTStru->uiAreas = 0;
   uiTablesCurr --;
}

int leto_InitTable( USHORT uiAreaNum, const char * szName, USHORT uiDriver, BOOL bShared )
{
   PTABLESTRU pTStru = s_tables;
   USHORT ui = 0, uiRet;
   USHORT uiLen;

   while( ui < uiTablesAlloc && pTStru->szTable )
   {
     pTStru ++;
     ui ++;
   }
   if( ui == uiTablesAlloc )
   {
      s_tables = (TABLESTRU*) hb_xrealloc( s_tables, sizeof(TABLESTRU) * ( uiTablesAlloc+TABLES_REALLOC ) );
      memset( s_tables+uiTablesAlloc, 0, sizeof(TABLESTRU) * TABLES_REALLOC );
      pTStru = s_tables + uiTablesAlloc;
      uiTablesAlloc += TABLES_REALLOC;
   }
   pTStru->uiAreaNum = uiAreaNum;
   pTStru->uiAreas = 0;
   uiLen = (USHORT) strlen(szName);
   pTStru->szTable = (BYTE*) hb_xgrab( uiLen + 1 );
   memcpy( pTStru->szTable, szName, uiLen );
   pTStru->szTable[uiLen] = '\0';
   hb_strLower( (char*)pTStru->szTable, (ULONG)uiLen );
   pTStru->uiDriver = uiDriver;
   pTStru->bShared = bShared;

   pTStru->uiIndexAlloc = INIT_INDEX_ALLOC;
   pTStru->pIStru = (INDEXSTRU*) hb_xgrab( sizeof(INDEXSTRU) * pTStru->uiIndexAlloc );
   memset( pTStru->pIStru, 0, sizeof(INDEXSTRU) * pTStru->uiIndexAlloc );

   uiRet = ui;
   if( ++ui > uiTablesMax )
      uiTablesMax = ui;
   uiTablesCurr ++;
   return uiRet;

}

HB_FUNC( LETO_INITTABLE )
{
   hb_retni( leto_InitTable( hb_parni(1), hb_parc(2), hb_parni(3), hb_parl(4) ) );
}

void leto_CloseArea( USHORT nUserStru, USHORT nAreaStru )
{
   PUSERSTRU pUStru = s_users + nUserStru;
   PAREASTRU pAStru = pUStru->pAStru + nAreaStru;

   if( bNoSaveWA )
   {
      hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
      hb_rddReleaseCurrentArea();
   }

   pAStru->pTStru->uiAreas --;
   if( !pAStru->pTStru->uiAreas )
   {
      /* If this was a last area for a table pAStru->pTStru, we need to close
         this table */
      leto_CloseTable( pAStru->pTStru - s_tables );
   }
   else
   {
      if( !bNoSaveWA )
         leto_UnlockAllRec( pAStru );
      if( pAStru->pTStru->bLocked && pAStru->bLocked )
      {
         pAStru->pTStru->bLocked = pAStru->bLocked = FALSE;
      }
   }
   pAStru->ulAreaNum = 0;
   pAStru->pTStru = NULL;
   if( pAStru->pLocksPos )
   {
      hb_xfree( pAStru->pLocksPos );
      pAStru->pLocksPos = NULL;
   }
   if( pAStru->pTag )
   {
      LETOTAG * pTag = pAStru->pTag, * pTagNext;
      do
      {
         /* if( pTag->szTagName )
            hb_xfree( pTag->szTagName ); */
         if( pTag->pTopScope )
            hb_itemRelease( pTag->pTopScope );
         if( pTag->pBottomScope )
            hb_itemRelease( pTag->pBottomScope );
         pTagNext = pTag->pNext;
         hb_xfree( pTag );
         pTag = pTagNext;
      }
      while( pTag );
      pAStru->pTag = NULL;
   }
   if( pAStru->itmFltExpr )
   {
      hb_itemClear( pAStru->itmFltExpr );
      hb_itemRelease( pAStru->itmFltExpr );
      pAStru->itmFltExpr = NULL;
   }
}

int leto_InitArea( int nUserStru, int nTableStru, ULONG ulId, const char * szAlias )
{
   PUSERSTRU pUStru = s_users + nUserStru;
   PTABLESTRU pTStru = s_tables + nTableStru;
   PAREASTRU pAStru;
   USHORT ui = 0;

   pAStru = pUStru->pAStru;
   while( ui < pUStru->uiAreasAlloc && pAStru->ulAreaNum )
   {
     pAStru ++;
     ui ++;
   }
   if( ui == pUStru->uiAreasAlloc )
   {
      pUStru->pAStru = (AREASTRU*) hb_xrealloc( pUStru->pAStru, sizeof(AREASTRU) * ( pUStru->uiAreasAlloc+AREAS_REALLOC ) );
      memset( pUStru->pAStru+pUStru->uiAreasAlloc, 0, sizeof(AREASTRU) * AREAS_REALLOC );
      pAStru = pUStru->pAStru + pUStru->uiAreasAlloc;
      pUStru->uiAreasAlloc += AREAS_REALLOC;
   }
   pAStru->ulAreaNum = ulId;
   pAStru->pTStru = pTStru;
   strcpy( pAStru->szAlias, szAlias );
   hb_strLower( pAStru->szAlias, strlen(pAStru->szAlias) );
   pTStru->uiAreas ++;

   return ui;
}

HB_FUNC( LETO_INITAREA )
{
   hb_retni( leto_InitArea( hb_parni(1), hb_parni(2), hb_parnl(3), hb_parc(4) ) );
}

void leto_CloseUS( PUSERSTRU pUStru )
{

   hb_ip_rfd_clr( pUStru->hSocket );
   hb_ipclose( pUStru->hSocket );
   pUStru->hSocket = 0;

   if( pUStru->pBuffer )
   {
      hb_xfree( pUStru->pBuffer );
      pUStru->pBuffer = NULL;
   }
   if( pUStru->szAddr )
   {
      hb_xfree( pUStru->szAddr );
      pUStru->szAddr = NULL;
   }
   if( pUStru->szVersion )
   {
      hb_xfree( pUStru->szVersion );
      pUStru->szVersion = NULL;
   }
   if( pUStru->szNetname )
   {
      hb_xfree( pUStru->szNetname );
      pUStru->szNetname = NULL;
   }
   if( pUStru->szExename )
   {
      hb_xfree( pUStru->szExename );
      pUStru->szExename = NULL;
   }
   if( pUStru->szDateFormat )
   {
      hb_xfree( pUStru->szDateFormat );
      pUStru->szDateFormat = NULL;
   }
   if( pUStru->pAStru )
   {
      USHORT ui;
      PAREASTRU pAStru = pUStru->pAStru;
      for( ui=0; ui<pUStru->uiAreasAlloc; ui++,pAStru++ )
         if( pAStru->pTStru )
            leto_CloseArea( pUStru-s_users, ui );
      hb_xfree( pUStru->pAStru );
      pUStru->pAStru = NULL;
   }

   leto_varsown_release( pUStru );

   uiUsersCurr --;
}

PUSERSTRU leto_InitUS( HB_SOCKET_T hSocket )
{
   PUSERSTRU pUStru = s_users;
   USHORT ui = 0;

   while( ui < uiUsersAlloc && pUStru->pAStru )
   {
     pUStru ++;
     ui ++;
   }
   if( ui == uiUsersAlloc )
   {
      s_users = (USERSTRU*) hb_xrealloc( s_users, sizeof(USERSTRU) * ( uiUsersAlloc+USERS_REALLOC ) );
      memset( s_users+uiUsersAlloc, 0, sizeof(USERSTRU) * USERS_REALLOC );
      pUStru = s_users + uiUsersAlloc;
      uiUsersAlloc += USERS_REALLOC;
   }
   pUStru->hSocket = hSocket;
   pUStru->ulBufferLen = HB_SENDRECV_BUFFER_SIZE;
   pUStru->pBuffer = (BYTE*) hb_xgrab( pUStru->ulBufferLen );

   pUStru->uiAreasAlloc = INIT_AREAS_ALLOC;
   pUStru->pAStru = (AREASTRU*) hb_xgrab( sizeof(AREASTRU) * pUStru->uiAreasAlloc );
   memset( pUStru->pAStru, 0, sizeof(AREASTRU) * pUStru->uiAreasAlloc );

   if( ++ui > uiUsersMax )
      uiUsersMax = ui;
   uiUsersCurr ++;

   return pUStru;
}

HB_FUNC( LETO_ADDDATABASE )
{
   DATABASE * pDBNext, * pDB = (DATABASE*) hb_xgrab( sizeof(DATABASE) );
   USHORT uiLen = (USHORT) hb_parclen(1);

   memset( pDB, 0, sizeof(DATABASE) );
   pDB->szPath = (char*) hb_xgrab( uiLen+1 );
   memcpy( pDB->szPath, hb_parc(1), uiLen );
   pDB->szPath[uiLen] = '\0';
   hb_strLower( pDB->szPath, uiLen );

   pDB->uiDriver = hb_parni(2);

   if( !s_pDB )
      s_pDB = pDB;
   else
   {
      pDBNext = s_pDB;
      while( pDBNext->pNext )
         pDBNext = pDBNext->pNext;
      pDBNext->pNext = pDB;
   }

}

USHORT leto_getDriver( const char * szPath )
{
   unsigned int iLen;
   DATABASE * pDB = s_pDB;

   while( pDB )
   {
      iLen = strlen( pDB->szPath );
      if( ( iLen <= strlen( szPath ) ) && ( !strncmp( pDB->szPath,szPath,iLen ) ) )
      {
         return pDB->uiDriver;
      }
      pDB = pDB->pNext;
   }
   return 0;
}

HB_FUNC( LETO_GETDRIVER )
{
   USHORT uiDriver = leto_getDriver( hb_parc(1) );
   if( uiDriver )
      hb_retni( uiDriver );
   else
      hb_ret();
}

HB_FUNC( LETO_CREATEDATA )
{
   uiUsersAlloc = INIT_USERS_ALLOC;
   s_users = (USERSTRU*) hb_xgrab( sizeof(USERSTRU) * uiUsersAlloc );
   memset( s_users, 0, sizeof(USERSTRU) * uiUsersAlloc );

   uiTablesAlloc = INIT_TABLES_ALLOC;
   s_tables = (TABLESTRU*) hb_xgrab( sizeof(TABLESTRU) * uiTablesAlloc );
   memset( s_tables, 0, sizeof(TABLESTRU) * uiTablesAlloc );

   lStartDate = leto_Date();
   dStartsec  = hb_dateSeconds();
   memset( dSumWait, 0, sizeof(dSumWait) );
   memset( uiSumWait, 0, sizeof(uiSumWait) );
}

HB_FUNC( LETO_RELEASEDATA )
{
   PUSERSTRU pUStru = s_users;
   PTABLESTRU pTStru = s_tables;
   DATABASE * pDBNext, * pDB = s_pDB;
   USHORT ui = 0;

   if( pUStru )
   {
      while( ui < uiUsersAlloc )
      {
         if( pUStru->pAStru )
            leto_CloseUS( pUStru );
         ui ++;
         pUStru ++;
      }

      hb_xfree( s_users );
      s_users = NULL;
   }
   ui = 0;
   if( pTStru )
   {
      while( ui < uiTablesAlloc )
      {
         if( pTStru->szTable )
            leto_CloseTable( ui );
         ui ++;
         pTStru ++;
      }

      hb_xfree( s_tables );
      s_tables = NULL;
   }
   if( pDataPath )
   {
      hb_xfree( pDataPath );
      pDataPath = NULL;
   }
   if( pBufCrypt )
   {
      hb_xfree( pBufCrypt );
      pBufCrypt = NULL;
      ulBufCryptLen = 0;
   }
   if( pDB )
      while( pDB )
      {
         pDBNext = pDB->pNext;
         if( pDB->szPath )
            hb_xfree( pDB->szPath );
         hb_xfree( pDB );
         pDB = pDBNext;
      }
   leto_acc_flush( pAccPath );
   if( pAccPath )
   {
      hb_xfree( pAccPath );
      pAccPath = NULL;
   }
   leto_acc_release();
   leto_vars_release();

}

static void leto_GotoIf( DBFAREAP pArea, ULONG ulRecNo )
{
   PHB_ITEM pItem = hb_itemPutNL( NULL, 0 );

   SELF_RECID( (AREAP)pArea, pItem );
   if( (ULONG)hb_itemGetNL( pItem ) != ulRecNo )
   {
      hb_itemPutNL( pItem, ulRecNo );
      SELF_GOTOID( (AREAP)pArea, pItem );
   }
   hb_itemRelease( pItem );

}

static void leto_NativeSep( char * szFile)
{
   char * ptrEnd;
#if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
   for( ptrEnd = szFile; *ptrEnd; ptrEnd++ )
      if( *ptrEnd == '/' )
         *ptrEnd = '\\';
#else
   for( ptrEnd = szFile; *ptrEnd; ptrEnd++ )
      if( *ptrEnd == '\\' )
         *ptrEnd = '/';
#endif
}

void leto_filef( PUSERSTRU pUStru, const char* szData )
{

   char * ptr;
   char * ptrEnd;
   char szData1[16];
   char szFile[_POSIX_PATH_MAX + 1];
   USHORT uiLen;

   if( !bFileFunc )
   {
      leto_SendAnswer( pUStru, "+F;100;", 7 );
   }
   else if( ( *(szData+2) != ';' ) || ( *szData != '0' ) ||
           ( ptr = strchr( szData+3, ';' ) ) == NULL )
   {
      leto_SendAnswer( pUStru, szErr2, 4 );
   }
   else
   {
      ULONG ulLen = 0;
      char * pBuffer = NULL;

      if ( pDataPath )
      {
         uiLen = strlen( pDataPath );
         memcpy( szFile, pDataPath, uiLen );
      }
      else
         uiLen = 0;
      memcpy( szFile+uiLen, szData+3, ptr - szData - 3 );
      szFile[uiLen+(ptr-szData-3)] = '\0';
      leto_NativeSep( szFile );
      szData1[0] = '+';
      ptr ++;
      switch( *(szData+1) )
      {
         case '1':
            szData1[1] = ( hb_spFile( szFile, NULL ) )? 'T' : 'F';
            szData1[2] = ';';
            break;

         case '2':
            if( hb_fsDelete( szFile ) )
               sprintf( szData1+1, "T;0;" );
            else
               sprintf( szData1+1, "F;%d;",hb_fsError() );
            break;

         case '3':
         {
            ptrEnd = strchr( ptr, ';' );
            if( !ptrEnd )
               strcpy( szData1, szErr2 );
            else
            {
               char szDest[_POSIX_PATH_MAX + 1];
               *ptrEnd = '\0';

               leto_NativeSep( ptr );
               memcpy( szDest, pDataPath, uiLen );
               szDest[uiLen++] = HB_OS_PATH_DELIM_CHR;
               strcpy( (szDest+uiLen), ( ( *ptr == HB_OS_PATH_DELIM_CHR) ? ptr + 1 : ptr ) );
               if ( hb_fsRename( szFile, szDest ) )
                  sprintf( szData1+1, "T;0;" );
               else
                  sprintf( szData1+1, "F;%d;",hb_fsError() );
            }
            break;
         }

         case '4':
         {
            pBuffer = leto_memoread( szFile, &ulLen );
            if( pBuffer )
            {
               pBuffer = hb_xrealloc( pBuffer, ulLen + 1 + sizeof( ULONG ));
               memmove( pBuffer + 1 + sizeof( ULONG ), pBuffer, ulLen );
               pBuffer[0] = '+';
               HB_PUT_LE_UINT32( pBuffer + 1, ulLen );
               ulLen += 1 + sizeof( ULONG );
            }
            else
            {
               memcpy( szData1 + 1, (char * ) &ulLen, sizeof( ULONG ) );
               ulLen = 5;
            }
            break;
         }

         default:
            strcpy( szData1, szErr2 );
            break;
      }
      if( pBuffer )
      {
         leto_SendAnswer( pUStru, pBuffer, ulLen );
         hb_xfree( pBuffer );
      }
      else
      {
         if( ulLen == 0)
            ulLen = strlen( szData1 );
         leto_SendAnswer( pUStru, szData1, ulLen );
      }
   }
   uiAnswerSent = 1;
}

/*
   leto_IsRecLocked()
   Returns 0, if the record isn't locked, 1, if it is locked in given area
   and -1 if it is locked by other area.
 */
static int leto_IsRecLocked( PAREASTRU pAStru, ULONG ulRecNo )
{
   PTABLESTRU pTStru = pAStru->pTStru;
   ULONG ul;
   ULONG * pLock;

   if( bNoSaveWA )
   {
      DBFAREAP pArea;
      PHB_ITEM pInfo = hb_itemNew( NULL );
      PHB_ITEM pRecNo = hb_itemPutNL( NULL, ulRecNo );
      BOOL bLocked;

      hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
      pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();
      SELF_RECINFO( (AREAP)pArea, pRecNo, DBRI_LOCKED, pInfo );
      bLocked = hb_itemGetL( pInfo );
      hb_itemRelease( pRecNo );
      hb_itemRelease( pInfo );
      return bLocked;
   }
   else
   {
      if( pAStru->pLocksPos )
         for( ul=0,pLock=pAStru->pLocksPos; ul<pAStru->ulLocksMax; ul++,pLock++ )
         {
            if( *pLock == ulRecNo )
            {
               return 1;
            }
         }
      if( pTStru->pLocksPos )
         for( ul=0,pLock=pTStru->pLocksPos; ul<pTStru->ulLocksMax; ul++,pLock++ )
         {
            if( *pLock == ulRecNo )
            {
               return -1;
            }
         }
   }
   return 0;

}

void leto_IsRecLockedUS( PUSERSTRU pUStru, const char* szData )
{
   const char * ptr;
   ULONG ulAreaID;
   PAREASTRU pAStru;

   if( ( ptr = strchr( szData, ';' ) ) == NULL )
      leto_SendAnswer( pUStru, szErr2, 4 );
   else
   {
      sscanf( szData, "%lu;", &ulAreaID );
      if( ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) == NULL )
         leto_SendAnswer( pUStru, szErr3, 4 );
      else
      {
         ULONG ulRecNo;
         ptr ++;
         sscanf( ptr, "%lu;", &ulRecNo );
         if ( leto_IsRecLocked( pAStru, ulRecNo ) )
           leto_SendAnswer( pUStru, "+T;", 3 );
         else
           leto_SendAnswer( pUStru, "+F;", 3 );
      }
   }
   uiAnswerSent = 1;
}

static BOOL leto_RecLock( PAREASTRU pAStru, ULONG ulRecNo )
{
   ULONG * pLock;
   ULONG ul;
   BOOL  bLocked = FALSE;
   PTABLESTRU pTStru = pAStru->pTStru;

   if( bNoSaveWA )
   {
      /* Simply lock the record with the standard RDD method */
      DBFAREAP pArea;
      DBLOCKINFO dbLockInfo;
      dbLockInfo.fResult = FALSE;
      dbLockInfo.itmRecID = hb_itemPutNL( NULL, ulRecNo );
      dbLockInfo.uiMethod = DBLM_MULTIPLE;

      hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
      pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();

      SELF_LOCK( (AREAP)pArea, &dbLockInfo );
      hb_itemRelease( dbLockInfo.itmRecID );

      return dbLockInfo.fResult;
   }
   else
   {
      /* Add a record number (ulRecNo) to the area's and table's
         locks lists (pAStru->pLocksPos) */
      /* Firstly, scanning the table's locks list */
      if( !pTStru->pLocksPos )
      {
         /* Allocate locks array for the table, if it isn't allocated yet */
         pTStru->ulLocksAlloc = 50;
         pTStru->pLocksPos = (ULONG*) hb_xgrab( sizeof(ULONG) * pTStru->ulLocksAlloc );
         pTStru->ulLocksMax = 0;
      }
      for( ul=0,pLock=pTStru->pLocksPos; ul<pTStru->ulLocksMax; ul++,pLock++ )
      {
         if( *pLock == ulRecNo )
         {
            /* The record is locked already, we need to determine,
               is it locked by current user/area, or no */
            bLocked = TRUE;
            break;
         }
      }

      if( !bLocked )
      {
         /* if the record isn't locked, lock it! */
         if( bShareTables )
         {
            /* if we work in ShareTables mode, set the real lock with the standard RDD method */
            DBFAREAP pArea;
            DBLOCKINFO dbLockInfo;
            dbLockInfo.fResult = FALSE;
            dbLockInfo.itmRecID = hb_itemPutNL( NULL, ulRecNo );
            dbLockInfo.uiMethod = DBLM_MULTIPLE;

            hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
            pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();

            SELF_LOCK( (AREAP)pArea, &dbLockInfo );
            hb_itemRelease( dbLockInfo.itmRecID );
            if( !dbLockInfo.fResult )
               return FALSE;
         }
         if( pTStru->ulLocksMax == pTStru->ulLocksAlloc )
         {
            pTStru->pLocksPos = (ULONG*) hb_xrealloc( pTStru->pLocksPos, sizeof(ULONG) * (pTStru->ulLocksAlloc+50) );
            pTStru->ulLocksAlloc += 50;
         }
         *( pTStru->pLocksPos + pTStru->ulLocksMax ) = ulRecNo;
         pTStru->ulLocksMax++;
      }

      /* Secondly, scanning the area's locks list */
      if( !pAStru->pLocksPos )
      {
         /* Allocate locks array for an area, if it isn't allocated yet */
         pAStru->ulLocksAlloc = 50;
         pAStru->pLocksPos = (ULONG*) hb_xgrab( sizeof(ULONG) * pAStru->ulLocksAlloc );
         pAStru->ulLocksMax = 0;
      }
      for( ul=0,pLock=pAStru->pLocksPos; ul<pAStru->ulLocksMax; ul++,pLock++ )
      {
         if( *pLock == ulRecNo )
         {
            return TRUE;
         }
      }
      if( bLocked )
         /* The record is locked by another user/area, so
            we return an error */
         return FALSE;
      else
      {
         if( pAStru->ulLocksMax == pAStru->ulLocksAlloc )
         {
            pAStru->pLocksPos = (ULONG*) hb_xrealloc( pAStru->pLocksPos, sizeof(ULONG) * (pAStru->ulLocksAlloc+50) );
            pAStru->ulLocksAlloc += 50;
         }
         *( pAStru->pLocksPos + pAStru->ulLocksMax ) = ulRecNo;
         pAStru->ulLocksMax++;
         return TRUE;
      }
   }
}

void leto_Lock( PUSERSTRU pUStru, const char* szData )
{
   const char * ptr;
   ULONG ulAreaID;
   PAREASTRU pAStru;
   PTABLESTRU pTStru;

   if( ( *szData != '0' ) || ( *(szData+2) != ';' ) ||
               ( ( ptr = strchr( szData+3, ';' ) ) == NULL ) )
      leto_SendAnswer( pUStru, szErr2, 4 );
   else
   {
      sscanf( szData+3, "%lu;", &ulAreaID );
      if( ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) == NULL )
         leto_SendAnswer( pUStru, szErr3, 4 );
      else if( pAStru->pTStru->bLocked && !pAStru->bLocked )
         leto_SendAnswer( pUStru, szErr4, 4 );
      else
      {
         switch( *(szData+1) )
         {
            case '1':
            {
               ULONG ulRecNo;
               ptr ++;
               sscanf( ptr, "%lu;", &ulRecNo );
               if ( leto_RecLock( pAStru, ulRecNo ) )
                  leto_SendAnswer( pUStru, szOk, 4 );
               else
                  leto_SendAnswer( pUStru, szErr4, 4 );
               break;
            }
            case '2':
            {
               pTStru = pAStru->pTStru;
               if( bNoSaveWA || ( !pAStru->bLocked && bShareTables ) )
               {
                  /* Lock the file with the standard RDD method */
                  DBFAREAP pArea;
                  DBLOCKINFO dbLockInfo;
                  dbLockInfo.fResult = FALSE;
                  dbLockInfo.itmRecID = NULL;
                  dbLockInfo.uiMethod = DBLM_FILE;

                  hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
                  pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();

                  SELF_LOCK( (AREAP)pArea, &dbLockInfo );
                  if( !dbLockInfo.fResult )
                  {
                     leto_SendAnswer( pUStru, szErr2, 4 );
                     break;
                  }
               }
               pTStru->bLocked = pAStru->bLocked = TRUE;
               leto_SendAnswer( pUStru, szOk, 4 );
               break;
            }
            default:
            {
               leto_SendAnswer( pUStru, szErr2, 4 );
            }
         }
      }
   }
   uiAnswerSent = 1;
}

void leto_Unlock( PUSERSTRU pUStru, const char* szData )
{
   const char * ptr;
   ULONG ulAreaID;
   PAREASTRU pAStru;
   PTABLESTRU pTStru;

   if( ( *szData != '0' ) || ( *(szData+2) != ';' ) ||
               ( ( ptr = strchr( szData+3, ';' ) ) == NULL ) )
      leto_SendAnswer( pUStru, szErr2, 4 );
   else
   {
      sscanf( szData+3, "%lu;", &ulAreaID );
      if( ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) == NULL )
         leto_SendAnswer( pUStru, szErr3, 4 );
      else
      {
         ptr ++;
         pTStru = pAStru->pTStru;
         switch( *(szData+1) )
         {
            case '1':
            /* Delete a record number (ulRecNo) from the area's and table's
               locks lists (pAStru->pLocksPos) */
            {
               ULONG * pLock;
               ULONG ul, ulRecNo;
               BOOL  bLocked = FALSE;

               sscanf( ptr, "%lu;", &ulRecNo );

               if( bNoSaveWA )
               {
                  /* Simply unlock the record with the standard RDD method */
                  DBFAREAP pArea;
                  PHB_ITEM pItem = hb_itemPutNL( NULL, ulRecNo );

                  hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
                  pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();

                  SELF_UNLOCK( (AREAP)pArea, pItem );
                  hb_itemRelease( pItem );
               }
               else
               {
                  /* Firstly, scanning the area's locks list */
                  if( pAStru->pLocksPos )
                     for( ul=0,pLock=pAStru->pLocksPos; ul<pAStru->ulLocksMax; ul++,pLock++ )
                     {
                        if( *pLock == ulRecNo )
                        {
                           bLocked = TRUE;
                           pAStru->ulLocksMax--;
                           *pLock = *( pAStru->pLocksPos + pAStru->ulLocksMax );
                           break;
                        }
                     }
                  if( bLocked )
                     /* The record is locked, so we unlock it */
                  {
                     /* Secondly, scanning the table's locks list if the record was
                        locked by the current user/area */
                     if( pTStru->pLocksPos )
                     {
                        for( ul=0,pLock=pTStru->pLocksPos; ul<pTStru->ulLocksMax; ul++,pLock++ )
                        {
                           if( *pLock == ulRecNo )
                           {
                              pTStru->ulLocksMax--;
                              (*pLock) = *( pTStru->pLocksPos + pTStru->ulLocksMax );
                              break;
                           }
                        }
                     }
                     if( bShareTables )
                     {
                        /* if we work in ShareTables mode, unlock with the standard RDD method */
                        DBFAREAP pArea;
                        PHB_ITEM pItem = hb_itemPutNL( NULL, ulRecNo );

                        hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
                        pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();

                        SELF_UNLOCK( (AREAP)pArea, pItem );
                        hb_itemRelease( pItem );
                     }
                  }
               }
               leto_SendAnswer( pUStru, szOk, 4 );
               break;
            }
            case '2':
            {
               /* Unlock table */
               leto_UnlockAllRec( pAStru );
               if( bNoSaveWA || ( pAStru->bLocked && bShareTables ) )
               {
                  DBFAREAP pArea;
                  hb_rddSelectWorkAreaNumber( pAStru->ulAreaNum/512 );
                  pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();
                  SELF_UNLOCK( (AREAP)pArea, NULL );
               }
               if( pAStru->bLocked || !pTStru->bLocked )
               {
                  pTStru->bLocked = pAStru->bLocked = FALSE;
                  leto_SendAnswer( pUStru, szOk, 4 );
               }
               else
               {
                  leto_SendAnswer( pUStru, szErr4, 4 );
               }
               break;
            }
            default:
            {
               leto_SendAnswer( pUStru, szErr2, 4 );
            }
         }
      }
   }
   uiAnswerSent = 1;
}

static int UpdateRec( PUSERSTRU pUStru, const char* szData, BOOL bAppend, ULONG * pRecNo, TRANSACTSTRU * pTA )
{
   DBFAREAP pArea;
   ULONG ulAreaID, ulRecNo;
   int   iUpd, iRes = 0;
   BOOL  bDelete = FALSE;
   BOOL  bRecall = FALSE;
   const char * pp1, * pp2, * pp3, * ptr;
   int nParam = leto_GetParam( szData, &pp1, &pp2, &pp3, &ptr );

   if( nParam < 4 )
      return 2;
   else
   {
      sscanf( szData, "%lu;", &ulAreaID );    // Get an area ID
      sscanf( pp1, "%lu;", &ulRecNo );        // Get a rec number or bUnlockAll(for append)
      sscanf( pp2, "%d;", &iUpd );    // Get a number of updated fields
      if( *pp3 == '1' )
         bDelete = TRUE;
      else if( *pp3 == '2' )
         bRecall = TRUE;
   }
   if( ulAreaID )
   {
      pArea = leto_SelectArea( ulAreaID, 0, NULL );
      if( pArea )
      {
         PAREASTRU pAStru = leto_FindArea( pUStru, ulAreaID );
         if( pTA )
            pTA->pArea = pArea;
         else
            hb_xvmSeqBegin();
         if( bAppend )
         {
            if( !pAStru->bLocked && pAStru->pTStru->bLocked )
            {
               if( !pTA )
                  hb_xvmSeqEnd();
               return ELETO_UNLOCKED;
            }
            if( pTA )
               pTA->bAppend = 1;
            else
            {
               PHB_ITEM pItem = hb_itemPutNL( NULL, 0 );

               if( ulRecNo )
               {
                  leto_UnlockAllRec( pAStru );
               }

               hb_rddSetNetErr( FALSE );
               SELF_APPEND( (AREAP)pArea, FALSE );
               SELF_RECID( (AREAP)pArea, pItem );
               ulRecNo = (ULONG)hb_itemGetNL( pItem );
               if( pRecNo )
                  *pRecNo = ulRecNo;
               hb_itemRelease( pItem );
               if( bHrbError )
                  iRes = 101;
               else
                  leto_RecLock( pAStru, ulRecNo );
            }
         }
         else
         {
            if( ( !pTA || ulRecNo ) && pAStru->pTStru->bShared && !pAStru->bLocked && leto_IsRecLocked( pAStru, ulRecNo ) != 1 )
            {
               /*  The table is opened in shared mode, but the record isn't locked */
               if( !pTA )
                  hb_xvmSeqEnd();
               return ELETO_UNLOCKED;
            }
            else if( !pTA )
               leto_GotoIf( pArea, ulRecNo );
            else
               pTA->ulRecNo = ulRecNo;
         }
         if( bDelete || bRecall )
         {
            if( pTA )
               pTA->uiFlag = ( (bDelete)? 1 : 0 ) | ( (bRecall)? 2 : 0 );
            else
            {
               BOOL b;
               SELF_DELETED( (AREAP)pArea, &b );
               if( bDelete && !b )
                  SELF_DELETE( (AREAP)pArea );
               else if( bRecall && b )
                  SELF_RECALL( (AREAP)pArea );
               if( bHrbError )
                  iRes = 102;
            }
         }
         if( !iRes && iUpd )
         {
            int i;
            LPFIELD pField;
            USHORT uiField, uiRealLen, uiLenLen;
            int n255;
            PHB_ITEM pItem = hb_itemNew( NULL );
#ifndef __XHARBOUR__
            char szBuffer[24];
#endif

            if( pTA )
            {
               pTA->uiItems  = iUpd;
               pTA->puiIndex = (USHORT*) malloc( sizeof(USHORT)*iUpd );
               pTA->pItems   = (PHB_ITEM*) malloc( sizeof(PHB_ITEM)*iUpd );
            }

            n255 = ( PDBFAREA( uiFieldCount ) > 255 ? 2 : 1 );

            for( i=0; i<iUpd; i++ )
            {
               /*  Firstly, calculate the updated field number ( uiField ) */
               uiField = (USHORT) leto_b2n( ptr, n255 );
               ptr += n255;

               if( !uiField || uiField > PDBFAREA( uiFieldCount ) )
               {
                  iRes = 3;
                  break;
               }
               pField = PDBFAREA( lpFields ) + uiField - 1;
               switch( pField->uiType )
               {
                  case HB_FT_STRING:
                     uiLenLen = ((int)*ptr) & 0xFF;
                     ptr ++;
                     if( pField->uiLen > 255 )
                     {
                        if( uiLenLen > 6 )
                        {
                           iRes = 3;
                           break;
                        }
                        uiRealLen = (USHORT) leto_b2n( ptr, uiLenLen );
                        ptr += uiLenLen;
                     }
                     else
                        uiRealLen = uiLenLen;
                     if( uiRealLen > pField->uiLen )
                     {
                        iRes = 3;
                        break;
                     }
                     hb_itemPutCL( pItem, (char*)ptr, uiRealLen );
                     ptr += uiRealLen;
                     break;

                  case HB_FT_LONG:
                  case HB_FT_FLOAT:
                  {
                     HB_MAXINT lVal;
                     double dVal;
                     BOOL fDbl;

                     uiRealLen = ((int)*ptr) & 0xFF;
                     if( uiRealLen > pField->uiLen )
                     {
                        iRes = 3;
                        break;
                     }
                     ptr ++;
                     fDbl = hb_strnToNum( (const char *) ptr,
                                          uiRealLen, &lVal, &dVal );
                     if( pField->uiDec )
                        hb_itemPutNDLen( pItem, fDbl ? dVal : ( double ) lVal,
                                         ( int ) ( pField->uiLen - pField->uiDec - 1 ),
                                         ( int ) pField->uiDec );
                     else if( fDbl )
                        hb_itemPutNDLen( pItem, dVal, ( int ) pField->uiLen, 0 );
                     else
                        hb_itemPutNIntLen( pItem, lVal, ( int ) pField->uiLen );
                     ptr += uiRealLen;
                     break;
                  }
                  case HB_FT_INTEGER:
                  {
                     switch( pField->uiLen )
                     {
                        case 2:
                           hb_itemPutNILen( pItem, ( int ) HB_GET_LE_INT16( ptr ), 6 );
                           break;
                        case 4:
                           hb_itemPutNIntLen( pItem, ( HB_LONG ) HB_GET_LE_INT32( ptr ), 10 );
                           break;
                        case 8:
#ifndef HB_LONG_LONG_OFF
                           hb_itemPutNIntLen( pItem, ( HB_LONG ) HB_GET_LE_INT64( ptr ), 20 );
#else
                           hb_itemPutNLen( pItem, ( double ) HB_GET_LE_INT64( ptr ), 20, 0 );
#endif
                        break;
                     }
                     ptr += pField->uiLen;
                     break;
                  }

                  case HB_FT_DOUBLE:
                  {
                     hb_itemPutNDLen( pItem, HB_GET_LE_DOUBLE( ptr ),
                        20 - ( pField->uiDec > 0 ? ( pField->uiDec + 1 ) : 0 ),
                        ( int ) pField->uiDec );
                     ptr += pField->uiLen;
                     break;
                  }

                  case HB_FT_CURRENCY:
                  {
/* to do */
                     ptr += pField->uiLen;
                     break;
                  }
                  case HB_FT_MODTIME:
                  case HB_FT_DAYTIME:
                  {
#ifdef __XHARBOUR__
                     hb_itemPutDTL( pItem,
                        HB_GET_LE_UINT32( ptr ),
                        HB_GET_LE_UINT32( ptr + 4 ) );
#else
   #if ( defined( HARBOUR_VER_AFTER_101 ) )
                     hb_itemPutC( pItem, hb_timeStampStr( szBuffer,
                        HB_GET_LE_INT32( ptr ),
                        HB_GET_LE_INT32( ptr + 4 ) ) );
   #else
                     hb_itemPutC( pItem, hb_dateTimeStampStr( szBuffer,
                        HB_GET_LE_INT32( ptr ),
                        HB_GET_LE_INT32( ptr + 4 ) ) );
   #endif
#endif
                     ptr += pField->uiLen;
                     break;
                  }

                  case HB_FT_DATE:
                     if( pField->uiLen == 8 )
                     {
                        char szBuffer[ 9 ];
                        memcpy( szBuffer, ptr, 8 );
                        szBuffer[ 8 ] = 0;
                        hb_itemPutDS( pItem, szBuffer );
                        ptr += 8;
                     }
                     else if( pField->uiLen == 4 )
                     {
                        hb_itemPutDL( pItem, HB_GET_LE_UINT32( ptr ) );
                        ptr += 4;
                     }
                     else  /*  pField->uiLen == 3 */
                     {
                        hb_itemPutDL( pItem, HB_GET_LE_UINT24( ptr ) );
                        ptr += 3;
                     }
                     break;

                  case HB_FT_LOGICAL:
                     hb_itemPutL( pItem, ( *ptr == 'T' ) );
                     ptr ++;
                     break;

                  case HB_FT_ANY:
                  {
                     if( pField->uiLen == 3 )
                     {
                        hb_itemPutDL( pItem, HB_GET_LE_UINT24( ptr ) );
                        ptr += 3;
                     }
                     else if( pField->uiLen == 4 )
                     {
                        hb_itemPutNL( pItem, HB_GET_LE_UINT32( ptr ) );
                        ptr += 4;
                     }
                     else
                     {
                        switch( *ptr++ )
                        {
                           case 'D':
                           {
                              char szBuffer[ 9 ];
                              memcpy( szBuffer, ptr, 8 );
                              szBuffer[ 8 ] = 0;
                              hb_itemPutDS( pItem, szBuffer );
                              ptr += 8;
                              break;
                           }
                           case 'L':
                           {
                              hb_itemPutL( pItem, (*ptr == 'T') );
                              ptr += 8;
                              break;
                           }
                           case 'N':
                           {
                              /* to do */
                              break;
                           }
                           case 'C':
                           {
                              USHORT uiLen = (USHORT) leto_b2n( ptr, 2 );
                              ptr += 2;
                              hb_itemPutCL( pItem, (char*)ptr, uiLen );
                              ptr += uiLen;
                              break;
                           }
                        }
                     }
                     break;
                  }
               }
               if( iRes )
                  break;
               if( pTA )
               {
                  pTA->puiIndex[i] = uiField;
                  pTA->pItems[i]   = hb_itemNew( pItem );
                  hb_itemClear( pItem );
               }
               else
               {
                  SELF_PUTVALUE( (AREAP)pArea, uiField, pItem );
                  hb_itemClear( pItem );
                  if( bHrbError )
                  {
                     iRes = 102;
                     break;
                  }
               }
            }
            hb_itemRelease( pItem );
         }
         if( !pTA )
            hb_xvmSeqEnd();
         return iRes;
      }
      else
         return 3;
   }
   else
      return 2;
}

void leto_UpdateRec( PUSERSTRU pUStru, const char* szData, BOOL bAppend )
{
   const char * pData;
   char szData1[24];
   ULONG ulRecNo, ulLen = 4;
   int iRes;

   if( ( bPass4D && !( pUStru->szAccess[0] & 4 ) ) )
      pData = szErrAcc;
   else
   {
      iRes = UpdateRec( pUStru, szData, bAppend, &ulRecNo, NULL );
      if( !iRes )
      {
         if( bAppend )
         {
            sprintf( szData1, "+%lu;", ulRecNo );
            pData = szData1;
            ulLen = strlen( pData );
         }
         else
            pData = szOk;
      }
      else if( iRes == 2 )
         pData = szErr2;
      else if( iRes == 3 )
         pData = szErr3;
      else if( iRes == 4 )
         pData = szErr4;
      else if( iRes == ELETO_UNLOCKED )
         pData = szErrUnlocked;
      else if( iRes > 100 )
      {
         sprintf( szData1, "-%d;", iRes );
         pData = szData1;
      }
      else  // ???
         pData = szErr2;
   }

   leto_SendAnswer( pUStru, pData, ulLen );
   uiAnswerSent = 1;
}

static int leto_SetFocus( AREAP pArea, const char * szOrder )
{
   DBORDERINFO pInfo;
   int iOrder;

   memset( &pInfo, 0, sizeof( DBORDERINFO ) );
   pInfo.itmOrder = NULL;
   pInfo.atomBagName = NULL;
   pInfo.itmResult = hb_itemPutC( NULL, "" );
   SELF_ORDLSTFOCUS( (AREAP) pArea, &pInfo );

   if( strcmp( (char*)szOrder, hb_itemGetCPtr( pInfo.itmResult ) ) )
   {
      hb_itemRelease( pInfo.itmResult );

      pInfo.itmOrder = hb_itemPutC( NULL,(char*)szOrder );
      pInfo.itmResult = hb_itemPutC( NULL, "" );
      SELF_ORDLSTFOCUS( pArea, &pInfo );
      hb_itemRelease( pInfo.itmOrder );
   }
   hb_itemRelease( pInfo.itmResult );

   pInfo.itmOrder = NULL;
   pInfo.itmResult = hb_itemPutNI( NULL, 0 );
   SELF_ORDINFO( pArea, DBOI_NUMBER, &pInfo );
   iOrder = hb_itemGetNI( pInfo.itmResult );
   hb_itemRelease( pInfo.itmResult );

   return iOrder;
}

static PHB_ITEM leto_KeyToItem( DBFAREAP pArea, const char *ptr, int iKeyLen, char *pOrder )
{
   BYTE KeyType;
   PHB_ITEM pKey = NULL;
   DBORDERINFO pInfo;

   memset( &pInfo, 0, sizeof( DBORDERINFO ) );
   if( pOrder )
      pInfo.itmOrder = hb_itemPutC( NULL, pOrder );
   pInfo.itmResult = hb_itemPutC( NULL, "" );
   SELF_ORDINFO( (AREAP)pArea, DBOI_KEYTYPE, &pInfo );
   KeyType = *( hb_itemGetCPtr( pInfo.itmResult ) );
   if( pOrder )
   {
      hb_itemClear( pInfo.itmOrder );
      hb_itemRelease( pInfo.itmOrder );
   }
   hb_itemClear( pInfo.itmResult );
   hb_itemRelease( pInfo.itmResult );

   switch( KeyType )
   {
      case 'N':
      {
         int iWidth, iDec;
         BOOL fDbl;
         HB_MAXINT lValue;
         double dValue;

         fDbl = hb_valStrnToNum( ptr, iKeyLen, &lValue, &dValue , &iDec, &iWidth );
         if ( !fDbl )
            pKey = hb_itemPutNIntLen( NULL, lValue, iWidth );
         else
            pKey = hb_itemPutNLen( NULL, dValue, iWidth, iDec );
         break;
      }
      case 'D':
      {
         char szBuffer[ 9 ];
         memcpy( szBuffer, ptr, 8 );
         szBuffer[ 8 ] = 0;
         pKey = hb_itemPutDS( NULL, szBuffer );
         break;
      }
      case 'L':
         pKey = hb_itemPutL( NULL, *ptr=='T' );
         break;
      case 'C':
         pKey = hb_itemPutCL( NULL, ptr, (ULONG)iKeyLen );
         break;
      default:
         break;
   }

   return pKey;
}

static void hb_setSetDeleted( BOOL bDeleted )
{
#if ( defined( __XHARBOUR__ ) ) && ( ! defined(HB_VER_CVSID) || ( HB_VER_CVSID < 6575 ) )
   HB_SET_STRUCT *hb_set = hb_GetSetStructPtr();
   hb_set->HB_SET_DELETED = bDeleted;
#else
  #if defined( HARBOUR_VER_AFTER_101 )
   PHB_ITEM pItem = hb_itemNew( NULL );
   hb_itemPutL( pItem, bDeleted );
   hb_setSetItem( HB_SET_DELETED, pItem );
   hb_itemRelease( pItem );
  #else
   hb_set.HB_SET_DELETED = bDeleted;
  #endif
#endif
}

static void hb_setSetDateFormat( char* szDateFormat )
{
#if ( defined( __XHARBOUR__ ) ) && ( ! defined(HB_VER_CVSID) || ( HB_VER_CVSID < 6575 ) )
   HB_SET_STRUCT *hb_set = hb_GetSetStructPtr();
   strcpy( hb_set->HB_SET_DATEFORMAT, szDateFormat );
#else
  #if defined( HARBOUR_VER_AFTER_101 )
   PHB_ITEM pItem = hb_itemNew( NULL );
   hb_itemPutC( pItem, szDateFormat );
   hb_setSetItem( HB_SET_DATEFORMAT, pItem );
   hb_itemRelease( pItem );
  #else
   strcpy( hb_set.HB_SET_DATEFORMAT, szDateFormat );
  #endif
#endif
}

static char * leto_recWithAlloc( DBFAREAP pArea, PAREASTRU pAStru, ULONG *ulLen )
{
   if( pAStru )
   {
      char* szData = (char*) malloc( pArea->uiRecordLen + PDBFAREA( uiFieldCount ) * 3 + 24 );
      if( szData )
      {
         *ulLen = leto_rec( pAStru, szData + 5 ) + 1;    // Length is incremented because of adding '+'
         if( *ulLen )
         {
            szData[0] = '\3';
            szData[1] = (BYTE) (*ulLen & 0xff);
            szData[2] = (BYTE) ((*ulLen >> 8) & 0xff);
            szData[3] = (BYTE) ((*ulLen >> 16) & 0xff);
            szData[4] = '+';
            *ulLen += 4;
         }
         else
         {
            free( szData );
            szData = NULL;
         }
         return szData;
      }
   }
   return NULL;
}

static void leto_SetUserEnv( PUSERSTRU pUStru )
{
#if defined( __XHARBOUR__ ) && ( ! defined(HB_VER_CVSID) || ( HB_VER_CVSID < 6575 ) )
   HB_SET_STRUCT *hb_set = hb_GetSetStructPtr();
   hb_set->hb_set_century = pUStru->bCentury;
#elif defined( __XHARBOUR__ ) || defined( HARBOUR_VER_AFTER_101 )
   hb_setSetCentury( pUStru->bCentury );
#elif defined( HARBOUR_VER_BEFORE_100 )
   hb_set.hb_set_century = pUStru->bCentury;
#endif
   if( pUStru->cdpage )
   {
#if !( defined( __XHARBOUR__ ) ) && ( defined( HARBOUR_VER_AFTER_101 ) )
      hb_vmSetCDP( pUStru->cdpage );
#else
      hb_cdpSelect( pUStru->cdpage );
#endif
   }
   if( pUStru->szDateFormat )
   {
      hb_setSetDateFormat( pUStru->szDateFormat );
   }
}

HB_FUNC( LETO_SETUSERENV )
{
   leto_SetUserEnv(s_users + hb_parni(1));
}

static void leto_SetFilter( PUSERSTRU pUStru, PAREASTRU pAStru, DBFAREAP pArea )
{
   if( pAStru->itmFltExpr )
   {
      leto_SetUserEnv( pUStru );
      PDBFAREA( dbfi.itmCobExpr ) = pAStru->itmFltExpr;
      PDBFAREA( dbfi.fFilter ) = TRUE;
   }
}

void leto_Seek( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   char * szData1 = NULL;
   const char * pData;
   const char * pp1, * pp2, * pp3;
   ULONG ulAreaID;
   ULONG ulLen = 4;
   char  szOrder[16];
   BOOL  bSoftSeek, bFindLast;
   int   iKeyLen;
   int nParam = leto_GetParam( szData, &pp1, &pp2, &pp3, NULL );

   if( nParam < 3 )
      pData = szErr2;
   else
   {
      sscanf( szData, "%lu;", &ulAreaID );
      memcpy( szOrder, pp1, pp2-pp1-1 );
      szOrder[pp2-pp1-1] = '\0';
      bSoftSeek = ( *pp2 & 0x10 )? 1 : 0;
      bFindLast = ( *pp2 & 0x20 )? 1 : 0;
      iKeyLen = (((int)*pp3++) & 0xFF);

      if( ulAreaID )
      {
         PAREASTRU pAStru = leto_FindArea( pUStru, ulAreaID );
         pArea = leto_SelectArea( ulAreaID, 0, NULL );
         if( pArea )
         {
            hb_xvmSeqBegin();
            if( leto_SetFocus( (AREAP) pArea, szOrder ) )
            {
               PHB_ITEM pKey = leto_KeyToItem( pArea, pp3, iKeyLen, NULL );

               if( pKey )
               {
                  LETOTAG * pTag;

                  hb_setSetDeleted( *pp2 & 1 );

                  pTag = leto_FindTag(pAStru, szOrder);
                  leto_SetScope( pArea, pTag, TRUE, TRUE );
                  leto_SetScope( pArea, pTag, FALSE, TRUE );

                  leto_SetFilter( pUStru, pAStru, pArea );
                  SELF_SEEK( (AREAP)pArea, bSoftSeek, pKey, bFindLast );
                  leto_ClearFilter( pArea );

                  leto_SetScope( pArea, pTag, TRUE, FALSE );
                  leto_SetScope( pArea, pTag, FALSE, FALSE );

                  hb_itemRelease( pKey );
                  if( bHrbError )
                     pData = szErr101;
                  else
                  {
                     szData1 = leto_recWithAlloc( pArea, pAStru, &ulLen );
                     if( szData1 )
                        pData = szData1;
                     else
                        pData = szErr2;
                  }
               }
               else
                  pData = szErr4;
            }
            else
               pData = szErr4;
            hb_xvmSeqEnd();
         }
         else
            pData = szErr3;
      }
      else
         pData = szErr2;
   }

   leto_SendAnswer( pUStru, pData, ulLen );
   uiAnswerSent = 1;
   if( szData1 )
      free( szData1 );

}

void leto_Scope( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   ULONG ulAreaID;
   int iCommand;
   const char * pData;
   const char * ptr, * ptrEnd;
   char  szOrder[16];
   int   iKeyLen;
   PAREASTRU pAStru;

   if( ( ( ptr = strchr( szData, ';' ) ) == NULL ) ||
       ( ( ptrEnd = strchr( ++ptr, ';' ) ) == NULL ) )
      pData = szErr2;
   else
   {
      sscanf( szData, "%lu;", &ulAreaID );
      sscanf( ptr, "%d;", &iCommand );
      ptr = ptrEnd + 1;
      if( ( ptrEnd = strchr( ptr, ';' ) ) == NULL )
         pData = szErr2;
      else
      {
         memcpy( szOrder, ptr, ptrEnd-ptr );
         szOrder[ptrEnd-ptr] = '\0';
         ptr = ptrEnd + 1;
         iKeyLen = (((int)*ptr++) & 0xFF);

         if( ulAreaID && szOrder[0] )
         {
            pArea = leto_SelectArea( ulAreaID, 0, NULL );
            pAStru = leto_FindArea( pUStru, ulAreaID );
            if( pArea && pAStru )
            {
               LETOTAG *pTag = leto_FindTag( pAStru, szOrder );

               if( pTag )
               {
                  PHB_ITEM pKey = leto_KeyToItem( pArea, ptr, iKeyLen, szOrder );

                  if( pKey )
                  {
                     if( iCommand == DBOI_SCOPETOP || iCommand == DBOI_SCOPESET )
                     {
                        if( !pTag->pTopScope )
                           pTag->pTopScope = hb_itemNew( NULL );
                        hb_itemCopy( pTag->pTopScope, pKey );
                     }
                     if( iCommand == DBOI_SCOPEBOTTOM || iCommand == DBOI_SCOPESET )
                     {
                        if( !pTag->pBottomScope )
                           pTag->pBottomScope = hb_itemNew( NULL );
                        hb_itemCopy( pTag->pBottomScope, pKey );
                     }
                     hb_itemRelease( pKey );
                  }
                  if( (iCommand == DBOI_SCOPETOPCLEAR || iCommand == DBOI_SCOPECLEAR) && pTag->pTopScope )
                  {
                     hb_itemClear(pTag->pTopScope);
                  }
                  if( (iCommand == DBOI_SCOPEBOTTOMCLEAR || iCommand == DBOI_SCOPECLEAR) && pTag->pBottomScope )
                  {
                     hb_itemClear(pTag->pBottomScope);
                  }
                  pData = szOk;
               }
               else
               {
                  pData = szErr3;
               }
            }
            else
               pData = szErr2;
         }
         else
            pData = szErr2;
      }
   }

   leto_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;

}

static void leto_ScopeCommand(DBFAREAP pArea, USHORT uiCommand, PHB_ITEM pKey)
{
   DBORDERINFO pInfo;

   memset( &pInfo, 0, sizeof( DBORDERINFO ) );
   pInfo.itmNewVal = pKey;
   pInfo.itmOrder = NULL;
   pInfo.atomBagName = NULL;
   pInfo.itmResult = hb_itemNew( NULL );
   SELF_ORDINFO( (AREAP)pArea, uiCommand, &pInfo );
   hb_itemRelease( pInfo.itmResult );
}

static void leto_SetScope(DBFAREAP pArea, LETOTAG *pTag, BOOL bTop, BOOL bSet)
{
   if( pTag )
   {
      PHB_ITEM pKey = (bTop ? pTag->pTopScope : pTag->pBottomScope);
      if( pKey )
      {
         USHORT uiCommand;

         if( bSet )
         {
            uiCommand = (bTop ? DBOI_SCOPETOP : DBOI_SCOPEBOTTOM);
         }
         else
         {
            uiCommand = (bTop ? DBOI_SCOPETOPCLEAR : DBOI_SCOPEBOTTOMCLEAR);
         }

         leto_ScopeCommand(pArea, uiCommand, (bSet ? pKey : NULL ) );
      }
   }
}

void leto_Skip( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   char * szData1 = NULL;
   const char * pData = NULL;
   const char * pp1, * pp2, * pp3, * pp4;
   char  szOrder[16];
   ULONG ulAreaID, ulRecNo;
   ULONG ulLen = 4, ulLenAll, ulLen2;
   LONG lSkip;
   PAREASTRU pAStru;
   int nParam = leto_GetParam( szData, &pp1, &pp2, &pp3, &pp4 );

   if( nParam < 5 )
      pData = szErr2;
   else
   {
      sscanf( szData, "%lu;", &ulAreaID );
      sscanf( pp1, "%ld;", &lSkip );
      sscanf( pp2, "%lu;", &ulRecNo );
      memcpy( szOrder, pp3, pp4-pp3-1 );
      szOrder[pp4-pp3-1] = '\0';

      if( ulAreaID )
      {
         BOOL bFlag;
         pArea = leto_SelectArea( ulAreaID, 0, NULL );
         pAStru = leto_FindArea( pUStru, ulAreaID );

         if( pArea && pAStru )
         {
            USHORT uiSkipBuf = pAStru->uiSkipBuf ? pAStru->uiSkipBuf : uiCacheRecords;
            LETOTAG * pTag = NULL;

            hb_xvmSeqBegin();
            if( ! bShareTables )
            {
               if ( ( lSkip == 1 ) || ( lSkip == -1 ) )
               {
                  ULONG ulRecCount;
                  SELF_RECCOUNT( (AREAP) pArea, &ulRecCount );
                  if( ( ULONG ) uiSkipBuf > ulRecCount )
                  {
                     uiSkipBuf = ( ulRecCount ? ulRecCount : 1 );
                  }
               }
               else
                  uiSkipBuf = 1;
            }

            leto_GotoIf( pArea, ulRecNo );
            leto_SetFocus( (AREAP) pArea, szOrder );
            hb_setSetDeleted( *pp4 & 1 );
            if( lSkip && *szOrder )
            {
               pTag = leto_FindTag(pAStru, szOrder);
               /* leto_SetScope( pArea, pTag, lSkip < 0, TRUE ); */
               leto_SetScope( pArea, pTag, TRUE, TRUE );
               leto_SetScope( pArea, pTag, FALSE, TRUE );
            }

            leto_SetFilter( pUStru, pAStru, pArea );

            SELF_SKIP( (AREAP) pArea, lSkip );

            if( !bHrbError )
            {
               szData1 = (char*) malloc( (pArea->uiRecordLen + PDBFAREA( uiFieldCount ) * 3 + 24) * uiSkipBuf );
               ulLenAll = leto_rec( pAStru, szData1+5 );
               if( ! ulLenAll )
               {
                  pData = szErr2;
               }
               else if( ( lSkip == 1 ) || ( lSkip == -1 ) )
               {
                  USHORT i = 1;

                  while( i < uiSkipBuf && !bHrbError )
                  {
                     if( lSkip == 1 )
                        SELF_EOF( (AREAP) pArea, &bFlag );
                     else
                        SELF_BOF( (AREAP) pArea, &bFlag );
                     if( bFlag )
                     {
                        break;
                     }
                     else
                     {
                        SELF_SKIP( (AREAP) pArea, lSkip );

                        if( !bHrbError )
                        {
                           if( lSkip == -1 )
                           {
                              SELF_BOF( (AREAP) pArea, &bFlag );
                              if( bFlag )
                                 break;
                           }
                           i ++;
                           ulLen2 = leto_rec( pAStru, szData1+5 + ulLenAll );
                           if( ! ulLen2 )
                           {
                              pData = szErr2;
                              break;
                           }
                           ulLenAll += ulLen2;
                        }
                        else
                        {
                           pData = szErr101;
                           break;
                        }
                     }
                  }
                  // free(szData2);
               }

               leto_ClearFilter( pArea );

               if( lSkip && *szOrder )
               {
                  /* leto_SetScope( pArea, pTag, lSkip < 0, FALSE ); */
                  leto_SetScope( pArea, pTag, TRUE, FALSE );
                  leto_SetScope( pArea, pTag, FALSE, FALSE );
               }

               if( bHrbError )
               {
                  pData = szErr101;
               }
               else if( pData == NULL )
               {
                  ulLenAll ++;   // Length is incremented because of adding '+'
                  szData1[0] = '\3';
                  szData1[1] = (BYTE) (ulLenAll & 0xff);
                  szData1[2] = (BYTE) ((ulLenAll >> 8) & 0xff);
                  szData1[3] = (BYTE) ((ulLenAll >> 16) & 0xff);
                  szData1[4] = '+';
                  pData = szData1;
                  ulLen = 4 + ulLenAll;
               }
            }
            else
               pData = szErr101;

            hb_xvmSeqEnd();
         }
         else
            pData = szErr2;
      }
      else
         pData = szErr2;
   }

   leto_SendAnswer( pUStru, pData, ulLen );
   uiAnswerSent = 1;
   if( szData1 )
      free( szData1 );

}

void leto_Goto( PUSERSTRU pUStru, const char * szData )
{
   DBFAREAP pArea;
   char * szData1 = NULL;
   const char * pData = NULL;
   const char * pp1, * pp2, * pp3;
   char  szOrder[16];
   ULONG ulAreaID;
   ULONG ulLen = 4;
   LONG lRecNo;
   PAREASTRU pAStru;
   int nParam = leto_GetParam( szData, &pp1, &pp2, &pp3, NULL );

   if( nParam < 4 )
      pData = szErr2;
   else
   {
      sscanf( szData, "%lu;", &ulAreaID );
      sscanf( pp1, "%ld;", &lRecNo );
      memcpy( szOrder, pp2, pp3-pp2-1 );
      szOrder[pp3-pp2-1] = '\0';
   }

   if( !pData && ulAreaID )
   {
      hb_xvmSeqBegin();
      pArea = leto_SelectArea( ulAreaID, 0, NULL );
      pAStru = leto_FindArea( pUStru, ulAreaID );

      if( lRecNo == -3 )
      {
         ULONG ulRecCount;
         SELF_RECCOUNT( (AREAP) pArea, &ulRecCount );
         lRecNo = ulRecCount + 10;
      }

      if( lRecNo >= 0 )
      {
         leto_GotoIf( pArea, lRecNo );
      }
      else if( (lRecNo == -1) || (lRecNo == -2) )
      {
         BOOL bTop = lRecNo==-1;
         LETOTAG * pTag = NULL;

         leto_SetFocus( (AREAP) pArea, szOrder );
         hb_setSetDeleted( *pp3 & 1 );

         if( *szOrder )
            pTag = leto_FindTag(pAStru, szOrder);

         if( pTag )
         {
            leto_SetScope( pArea, pTag, FALSE, TRUE);
            leto_SetScope( pArea, pTag, TRUE, TRUE);
         }

         leto_SetFilter( pUStru, pAStru, pArea );

         if( bTop )
         {
            SELF_GOTOP( (AREAP) pArea );
         }
         else
         {
            SELF_GOBOTTOM( (AREAP) pArea );
         }

         leto_ClearFilter( pArea );
         if( pTag )
         {
            leto_SetScope( pArea, pTag, 0, FALSE);
            leto_SetScope( pArea, pTag, 1, FALSE);
         }

      }
      if( bHrbError )
         pData = szErr101;
      else
      {
         szData1 = leto_recWithAlloc( pArea, pAStru, &ulLen );
         if( szData1 )
            pData = szData1;
         else
            pData = szErr2;
      }
      hb_xvmSeqEnd();
   }
   else
      pData = szErr2;

   leto_SendAnswer( pUStru, pData, ulLen );
   uiAnswerSent = 1;
   if( szData1 )
      free( szData1 );
}

int leto_Memo( PUSERSTRU pUStru, const char* szData, TRANSACTSTRU * pTA )
{
   DBFAREAP pArea;
   const char * ptr, * ptrEnd;
   const char * pData = NULL;
   BOOL bPut = FALSE;
   BOOL bAdd = FALSE;
   ULONG ulAreaID, ulNumrec;
   USHORT uiField;
   ULONG ulLen = 4;
   int iRes = 0;

   if( ( ( ptr = strchr( szData, ';' ) ) == NULL ) ||
           ( ( ptrEnd = strchr( ++ptr, ';' ) ) == NULL ) )
   {
      pData = szErr2;
      iRes = 2;
   }
   else
   {
      if( !strncmp( szData,"put",3 ) )
         bPut = TRUE;
      else if( !strncmp( szData,"add",3 ) )
      {
         bPut = TRUE;
         bAdd = TRUE;
      }
      else if( strncmp( szData,"get",3 ) )
      {
         pData = szErr2;
         iRes = 2;
      }
      if( !pData )
      {
         sscanf( ptr, "%lu;", &ulAreaID );
         ptr = ptrEnd + 1;
         if( ( ptrEnd = strchr( ptr, ';' ) ) == NULL )
         {
            pData = szErr2;
            iRes = 2;
         }
         else
         {
            sscanf( ptr, "%lu;", &ulNumrec );
            ptr = ptrEnd + 1;
            if( ( ptrEnd = strchr( ptr, ';' ) ) == NULL )
            {
               pData = szErr2;
               iRes = 2;
            }
            else
            {
               ULONG ulField;

               sscanf( ptr, "%ld;", &ulField );
               uiField = (USHORT)ulField;
               if( ulAreaID && ( ulNumrec || pTA ) && uiField )
               {
                  hb_rddSelectWorkAreaNumber( ulAreaID/512 );
                  pArea = (DBFAREAP) hb_rddGetCurrentWorkAreaPointer();
                  if( pArea )
                  {
                     PHB_ITEM pItem;
                     PAREASTRU pAStru = leto_FindArea( pUStru, ulAreaID );

                     if( !pTA )
                        hb_xvmSeqBegin();
                     if( ( !pTA || ulNumrec ) &&
                           bPut && pAStru->pTStru->bShared && !pAStru->bLocked &&
                           leto_IsRecLocked( pAStru, ulNumrec ) != 1 )
                     {
                        /*  The table is opened in shared mode, but the record isn't locked */
                        pData = szErr4;
                        iRes = 4;
                     }
                     else
                     {
                        if( !pTA )
                        {
                           pItem = hb_itemPutNL( NULL, 0 );
                           SELF_RECID( (AREAP)pArea, pItem );
                           if( (ULONG)hb_itemGetNL( pItem ) != ulNumrec )
                           {
                              hb_itemPutNL( pItem, ulNumrec );
                              SELF_GOTOID( (AREAP)pArea, pItem );
                           }
                           hb_itemClear( pItem );
                        }
                        else
                        {
                           pTA->bAppend = ( bAdd )? 1 : 0;
                           pItem = hb_itemNew( NULL );
                           pTA->ulRecNo = ulNumrec;
                        }
                        if( !bHrbError )
                        {
                           if( bPut )
                           {
                              USHORT uiLenLen;
                              ULONG ulMemoLen;

                              ptr = ptrEnd + 1;
                              if( ( uiLenLen = (((int)*ptr) & 0xFF) ) >= 10 )
                              {
                                 hb_itemRelease( pItem );
                                 pData = szErr2;
                                 iRes = 2;
                              }
                              else
                              {
                                 ulMemoLen = leto_b2n( ++ptr, uiLenLen );
                                 ptr += uiLenLen;
                                 hb_itemPutCL( pItem, (char*)ptr, ulMemoLen );

                                 if( pTA )
                                 {
                                    pTA->pArea = pArea;
                                    pTA->uiFlag = 4;
                                    pTA->uiItems  = 1;
                                    pTA->puiIndex = (USHORT*) malloc( sizeof(USHORT) );
                                    pTA->pItems   = (PHB_ITEM*) malloc( sizeof(PHB_ITEM) );
                                    pTA->puiIndex[0] = uiField;
                                    pTA->pItems[0]   = hb_itemNew( pItem );
                                    hb_itemClear( pItem );
                                 }
                                 else
                                 {
                                    SELF_PUTVALUE( (AREAP)pArea, uiField, pItem );
                                    hb_itemClear( pItem );
                                    if( bHrbError )
                                       pData = szErr101;
                                    else
                                       pData = szOk;
                                 }
                              }
                           }
                           else
                           {
                              ULONG ulBufLen;

                              SELF_GETVALUE( (AREAP)pArea, uiField, pItem );
                              if( bHrbError )
                              {
                                 pData = szErr101;
                                 hb_itemClear( pItem );
                              }
                              else
                              {
                                 ulLen = hb_itemGetCLen( pItem );
                                 ulBufLen = ulLen + 22;
                                 if( ulBufLen > ulBufCryptLen )
                                 {
                                    if( !ulBufCryptLen )
                                       pBufCrypt = (BYTE*) hb_xgrab( ulBufLen );
                                    else
                                       pBufCrypt = (BYTE*) hb_xrealloc( pBufCrypt, ulBufLen );
                                    ulBufCryptLen = ulBufLen;
                                 }
                                 if( bCryptTraf && ulLen )
                                 {
                                    leto_encrypt( hb_itemGetCPtr(pItem), ulLen, (char*) pBufCrypt+5, &ulLen, LETO_PASSWORD );
                                 }
                                 else
                                    memcpy( pBufCrypt+5, hb_itemGetCPtr(pItem), ulLen );
                                 ulLen ++;
                                 *pBufCrypt = '\3';
                                 *(pBufCrypt+1) = (BYTE) (ulLen & 0xff);
                                 *(pBufCrypt+2) = (BYTE) ((ulLen >> 8) & 0xff);
                                 *(pBufCrypt+3) = (BYTE) ((ulLen >> 16) & 0xff);
                                 *(pBufCrypt+4) = '+';
                                 ulLen += 4;
                                 pData = (char*) pBufCrypt;
                                 hb_itemClear( pItem );
                              }

                           }
                        }
                        else
                           pData = szErr101;

                        hb_itemRelease( pItem );
                     }
                     if( !pTA )
                        hb_xvmSeqEnd();
                  }
                  else
                  {
                     pData = szErr3;
                     iRes = 3;
                  }
               }
               else
               {
                  pData = szErr2;
                  iRes = 2;
               }
            }
         }
      }
   }
   if( !pTA )
   {
      leto_SendAnswer( pUStru, pData, ulLen );
      uiAnswerSent = 1;
   }
   return iRes;

}

void leto_Ordfunc( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   char szData1[32];
   const char * pData;
   char * szData2 = NULL;
   ULONG ulLen = 4;
   ULONG ulAreaID;
   char  szOrder[16];
   const char * pp1, * pp2, * pp3;
   int nParam = leto_GetParam( szData, &pp1, &pp2, &pp3, NULL );

   if( nParam < 2 || *szData != '0' )
      pData = szErr2;
   else
   {
      sscanf( pp1, "%lu;", &ulAreaID );
      if( ulAreaID )
      {
         pArea = leto_SelectArea( ulAreaID, 0, NULL );
         if( pArea )
         {
            hb_xvmSeqBegin();
            if( *(szData+1) == '3' )
            {
               /* Reindex */
               SELF_ORDLSTREBUILD( (AREAP) pArea );
               if( bHrbError )
                  pData = szErr101;
               else
                  pData = szOk;
            }
            else if( *(szData+1) == '4' )
            {
               /* ordListClear */
               PTABLESTRU pTStru;
               PINDEXSTRU pIStru;
               PAREASTRU pAStru = leto_FindArea( pUStru, ulAreaID );
               LETOTAG * pTag = pAStru->pTag, * pTag1, * pTagPrev;
               USHORT ui = 0;
               BOOL   bClear = 0;

               while( pTag )
               {
                  pIStru = pTag->pIStru;

                  // Just check, if it is another tag of an already checked bag
                  pTag1 = pAStru->pTag;
                  while( pTag1 != pTag && pTag1->pIStru != pIStru )
                     pTag1 = pTag1->pNext;

                  if( pTag1 != pTag )
                  {  // if a bag is checked already
                     if( ! *pTag1->szTagName )
                        *pTag->szTagName = '\0';
                  }
                  else if( !pIStru->bCompound )
                  {  // it is a new bag and it isn't compound index
                     *pTag->szTagName = '\0';
                     pIStru->uiAreas --;
                  }
                  pTag = pTag->pNext;
               }

               // Now release and remove all tags of noncompound index
               pTag = pAStru->pTag;
               pTagPrev = NULL;
               while( pTag )
               {
                  if( ! *pTag->szTagName )
                  {
                     pTag1 = pTag;
                     if( pTag == pAStru->pTag )
                        pTag = pAStru->pTag = pTag->pNext;
                     else
                     {
                        pTag = pTag->pNext;
                        if( pTagPrev )
                           pTagPrev->pNext = pTag;
                     }

                     if( pTag1->pTopScope )
                        hb_itemRelease( pTag1->pTopScope );
                     if( pTag1->pBottomScope )
                        hb_itemRelease( pTag1->pBottomScope );
                     hb_xfree( pTag1 );
                  }
                  else
                  {
                     pTagPrev = pTag;
                     pTag = pTagPrev->pNext;
                  }
               }

               // And, finally, scan the open indexes of a table
               pTStru = pAStru->pTStru;
               pIStru = pTStru->pIStru;
               while( ui < pTStru->uiIndexAlloc )
               {
                 if( pIStru->BagName && !pIStru->bCompound )
                 {
                    if( !pIStru->uiAreas )
                    {
                       bClear = 1;
                       leto_CloseIndex( pIStru );
                    }
                 }
                 pIStru ++;
                 ui ++;
               }
               if( bClear )
               {
                  SELF_ORDLSTCLEAR( (AREAP) pArea );
                  pIStru = pTStru->pIStru;
                  ui = 0;
                  while( ui < pTStru->uiIndexAlloc )
                  {
                    if( pIStru->BagName && !pIStru->bCompound )
                    {
                       DBORDERINFO pOrderInfo;
                       memset( &pOrderInfo, 0, sizeof( DBORDERINFO ) );
                       pOrderInfo.atomBagName = hb_itemPutC( NULL,pIStru->szFullName );
                       pOrderInfo.itmResult = hb_itemNew( NULL );
                       SELF_ORDLSTADD( (AREAP)pArea, &pOrderInfo );
                       hb_itemRelease( pOrderInfo.itmResult );
                       hb_itemRelease( pOrderInfo.atomBagName );
                    }
                    pIStru ++;
                    ui ++;
                  }
               }

               pData = szOk;
            }
            else
            {
               /* Check, if the order name transferred */
               if( nParam < 3 )
                  pData = szErr2;
               else
               {
                  LETOTAG * pTag = NULL;
                  PAREASTRU pAStru = leto_FindArea( pUStru, ulAreaID );

                  memcpy( szOrder, pp2, pp3-pp2-1 );
                  szOrder[pp3-pp2-1] = '\0';

                  if( leto_SetFocus( (AREAP) pArea, szOrder ) )
                  {
                     if( *szOrder )
                     {
                        pTag = leto_FindTag(pAStru, szOrder);
                        leto_SetScope( pArea, pTag, FALSE, TRUE );
                        leto_SetScope( pArea, pTag, TRUE, TRUE );
                     }
                     leto_SetFilter( pUStru, pAStru, pArea );

                     switch( *(szData+1) )
                     {
                        case '1':  /* ordKeyCount */
                        {
                           DBORDERINFO pInfo;
                           memset( &pInfo, 0, sizeof( DBORDERINFO ) );
                           pInfo.itmResult = hb_itemPutNL( NULL, 0 );
                           SELF_ORDINFO( (AREAP)pArea, DBOI_KEYCOUNT, &pInfo );
                           if( bHrbError )
                              pData = szErr101;
                           else
                           {
                              sprintf( szData1, "+%lu;", hb_itemGetNL(pInfo.itmResult) );
                              pData = szData1;
                              ulLen = strlen( pData );
                           }
                           hb_itemRelease( pInfo.itmResult );
                           break;
                        }
                        case '2':  /* ordKeyNo */
                        {
                           DBORDERINFO pInfo;
                           ULONG ulRecNo;

                           /* Check, if the current record number is transferred */
                           if( nParam < 4 )
                              pData = szErr2;
                           else
                           {
                              sscanf( pp3, "%lu;", &ulRecNo );
                              leto_GotoIf( pArea, ulRecNo );

                              memset( &pInfo, 0, sizeof( DBORDERINFO ) );
                              pInfo.itmResult = hb_itemPutNL( NULL, 0 );
                              SELF_ORDINFO( (AREAP)pArea, DBOI_POSITION, &pInfo );
                              if( bHrbError )
                                 pData = szErr101;
                              else
                              {
                                 sprintf( szData1, "+%lu;", hb_itemGetNL(pInfo.itmResult) );
                                 pData = szData1;
                                 ulLen = strlen( pData );
                              }
                              hb_itemRelease( pInfo.itmResult );
                           }
                           break;
                        }
                        case '5':  /* ordKeyGoto */
                        {
                           DBORDERINFO pInfo;
                           ULONG ulRecNo;

                           /* Check, if the current record number is transferred */
                           if( nParam < 4 )
                              pData = szErr2;
                           else
                           {
                              sscanf( pp3, "%lu;", &ulRecNo );
                              memset( &pInfo, 0, sizeof( DBORDERINFO ) );
                              pInfo.itmNewVal = hb_itemPutNL( NULL, ulRecNo );
                              pInfo.itmResult = hb_itemPutL( NULL, FALSE );
                              SELF_ORDINFO( (AREAP)pArea, DBOI_POSITION, &pInfo );
                              if( bHrbError )
                                 pData = szErr101;
                              else
                              {
                                 szData2 = leto_recWithAlloc( pArea, pAStru, &ulLen );
                                 if( szData2 )
                                    pData = szData2;
                                 else
                                    pData = szErr2;
                              }
                              hb_itemRelease( pInfo.itmResult );
                           }
                           break;
                        }
                        case '6':  /* ordSkipUnique */
                        {
                           DBORDERINFO pInfo;
                           ULONG ulDirection;

                           /* Check, if the direction is transferred */
                           if( nParam < 4 )
                              pData = szErr2;
                           else
                           {
                              sscanf( pp3, "%lu;", &ulDirection );
                              memset( &pInfo, 0, sizeof( DBORDERINFO ) );
                              pInfo.itmNewVal = hb_itemPutNL( NULL, ulDirection );
                              pInfo.itmResult = hb_itemPutL( NULL, FALSE );
                              SELF_ORDINFO( (AREAP)pArea, DBOI_SKIPUNIQUE, &pInfo );
                              if( bHrbError )
                                 pData = szErr101;
                              else
                              {
                                 szData2 = leto_recWithAlloc( pArea, pAStru, &ulLen );
                                 if( szData2 )
                                    pData = szData2;
                                 else
                                    pData = szErr2;
                              }
                              hb_itemRelease( pInfo.itmResult );
                           }
                           break;
                        }
                        default:
                           pData = szErr2;
                           break;
                     }

                     leto_ClearFilter( pArea );

                     if( *szOrder )
                     {
                        leto_SetScope( pArea, pTag, FALSE, FALSE );
                        leto_SetScope( pArea, pTag, TRUE, FALSE );
                     }

                  }
                  else
                     pData = szErr4;
               }
            }
            hb_xvmSeqEnd();
         }
         else
            pData = szErr4;
      }
      else
         pData = szErr3;
   }
   leto_SendAnswer( pUStru, pData, ulLen );
   uiAnswerSent = 1;
   if( szData2 )
      free( szData2 );
}

void leto_Mgmt( PUSERSTRU pUStru, const char* szData )
{
   char * ptr;
   char * pData;
   BOOL bShow;
   const char * pp1, * pp2;
   int nParam = leto_GetParam( szData, &pp1, &pp2, NULL, NULL );

   if( ( bPass4M && !( pUStru->szAccess[0] & 2 ) ) || !nParam || *szData != '0' )
   {
      leto_SendAnswer( pUStru, szErrAcc, 4 );
   }
   else
   {
      double dCurrent = hb_dateSeconds();
      switch( *(szData+1) )
      {
         case '0':
         {
            char s[90];
            USHORT ui;
            double dWait = 0;
            long ulWait = 0;

            for( ui=0; ui<6; ui++ )
            {
               dWait  += dSumWait[ui];
               ulWait += uiSumWait[ui];

            }
            sprintf( s,"+%d;%d;%d;%d;%lu;%lu;%lu;%lu;%u;%u;%s;%7.3f;%7.3f;%lu;%lu;%lu;%lu;",
               uiUsersCurr,uiUsersMax,uiTablesCurr,uiTablesMax,
               (leto_Date()-lStartDate)*86400+(long)(dCurrent-dStartsec),
               ulOperations,ulBytesSent,ulBytesRead,uiIndexCurr,uiIndexMax,
               (pDataPath ? pDataPath : ""),dMaxDayWait,(ulWait)? dWait/ulWait : 0,
               ulTransAll,ulTransOK, hb_xquery( 1001 ), hb_xquery( 1002 ) );
            leto_SendAnswer( pUStru, s, strlen(s) );
            break;
         }
         case '1':
         {
            PUSERSTRU pUStru1 = s_users;
            int iTable = -1;
            USHORT ui = 0;
            USHORT uiUsers = 0;

            if( nParam == 2 )
            {
               sscanf( pp1,"%d;", &iTable );
               if( iTable >= uiTablesMax )
                  iTable = -1;
            }
            pData = ( char * ) malloc( uiUsersCurr*56 );
            ptr = pData;
            *ptr++ = '+'; *ptr++ = '0'; *ptr++ = '0'; *ptr++ = '0'; *ptr++ = ';';
            while( ui < uiUsersMax )
            {
               while( ui < uiUsersMax && !pUStru1->pAStru )
               {
                  ui ++;
                  pUStru1 ++;
               }
               if( ui < uiUsersMax && uiUsers < 999 )
               {
                  if( iTable != -1 )
                  {
                     int i;
                     bShow = FALSE;
                     for( i=0; i<pUStru1->uiAreasAlloc; i++ )
                     {
                        if( pUStru1->pAStru && pUStru1->pAStru[i].pTStru &&
                             pUStru1->pAStru[i].pTStru->uiAreaNum &&
                             ( pUStru1->pAStru[i].pTStru->uiAreaNum == s_tables[iTable].uiAreaNum ) )
                        {
                           bShow = TRUE;
                           break;
                        }
                     }
                  }
                  else
                     bShow = TRUE;
                  if( bShow )
                  {
                     sprintf( ptr,"%d;", ui );
                     ptr += strlen(ptr);
                     if( pUStru1->szAddr )
                     {
                        memcpy( ptr,pUStru1->szAddr,strlen((char*)pUStru1->szAddr) );
                        ptr += strlen((char*)pUStru1->szAddr);
                     }
                     else
                     {
                        memcpy( ptr,szNull,strlen(szNull) );
                        ptr += strlen(szNull);
                     }
                     *ptr++ = ';';
                     if( pUStru1->szNetname )
                     {
                        memcpy( ptr,pUStru1->szNetname,strlen((char*)pUStru1->szNetname) );
                        ptr += strlen((char*)pUStru1->szNetname);
                     }
                     else
                     {
                        memcpy( ptr,szNull,strlen(szNull) );
                        ptr += strlen(szNull);
                     }
                     *ptr++ = ';';
                     if( pUStru1->szExename )
                     {
                        memcpy( ptr,pUStru1->szExename,strlen((char*)pUStru1->szExename) );
                        ptr += strlen((char*)pUStru1->szExename);
                     }
                     else
                     {
                        memcpy( ptr,szNull,strlen(szNull) );
                        ptr += strlen(szNull);
                     }
                     *ptr++ = ';';
                     sprintf( (char*)ptr,"%lu;", (long)(dCurrent-pUStru1->dLastAct) );
                     ptr += strlen((char*)ptr);
                     uiUsers ++;
                  }
                  ui ++;
                  pUStru1 ++;
               }
            }
            *(pData+3) = ( uiUsers % 10 ) + 0x30;
            *(pData+2) = ( ( uiUsers / 10 ) % 10 ) + 0x30;
            *(pData+1) = ( ( uiUsers / 100 ) % 10 ) + 0x30;
            *ptr = '\0';
            leto_SendAnswer( pUStru, pData, ptr - pData );
            free( pData );
            break;
         }
         case '2':
         {
            if( uiTablesCurr )
            {
               int iUser = -1;
               USHORT ui = 0;
               USHORT uiTables = 0;

               if( nParam == 2 )
               {
                  sscanf( (char*)pp1,"%d;", &iUser );
                  if( iUser >= uiUsersMax )
                     iUser = -1;
               }
               pData = (char*) malloc( uiTablesCurr*128+8 );
               ptr = pData;
               *ptr++ = '+'; *ptr++ = '0'; *ptr++ = '0'; *ptr++ = '0'; *ptr++ = ';';
               if( iUser != -1 )
               {
                  PAREASTRU pAStru = s_users[iUser].pAStru;
                  if( pAStru )
                     for( ui=0; ui<s_users[iUser].uiAreasAlloc; ui++,pAStru++ )
                     {
                        if( pAStru->pTStru && pAStru->pTStru->szTable )
                        {
                           sprintf( ptr,"%d;", ui );
                           ptr += strlen(ptr);
                           memcpy( ptr,pAStru->pTStru->szTable,strlen((char*)pAStru->pTStru->szTable) );
                           ptr += strlen((char*)pAStru->pTStru->szTable);
                           *ptr++ = ';';
                           uiTables ++;
                        }
                     }
               }
               else
               {
                  PTABLESTRU pTStru1 = s_tables;
                  while( ui < uiTablesMax && uiTables < 999 )
                  {
                     while( ui < uiTablesMax && !pTStru1->szTable )
                     {
                        ui ++;
                        pTStru1 ++;
                     }
                     if( ui < uiTablesMax )
                     {
                        if( pTStru1->szTable )
                        {
                           sprintf( ptr,"%d;", ui );
                           ptr += strlen(ptr);
                           memcpy( ptr,pTStru1->szTable,strlen((char*)pTStru1->szTable) );
                           ptr += strlen((char*)pTStru1->szTable);
                           *ptr++ = ';';
                           uiTables ++;
                        }
                        ui ++;
                        pTStru1 ++;
                     }
                  }
               }
               *(pData+3) = ( uiTables % 10 ) + 0x30;
               *(pData+2) = ( ( uiTables / 10 ) % 10 ) + 0x30;
               *(pData+1) = ( ( uiTables / 100 ) % 10 ) + 0x30;
               *ptr = '\0';
               leto_SendAnswer( pUStru, pData, ptr - pData );
               free( pData );
            }
            else
               leto_SendAnswer( pUStru, "+000;", 5 );
            break;
         }
         case '3':
         {
            char s[30];
            sprintf( s, "+%lu;%9.3f;", leto_Date(), dCurrent );
            leto_SendAnswer( pUStru, s, strlen(s) );
            break;
         }
         case '9':
         {
            BYTE szUser[16];
            int iUser;
            USHORT uiLen;

            if( nParam == 2 && (uiLen = (pp2 - pp1 - 1)) < 16 )
            {
               memcpy( szUser, pp1, uiLen );
               szUser[uiLen] = '\0';
               if( strchr( (char*)szUser, '.' ) )
               {
                  USHORT ui = 0;
                  PUSERSTRU pUStru1 = s_users;
                  while( ui < uiUsersMax )
                  {
                     if( !pUStru1->pAStru && !pUStru1->szAddr && !strcmp( (char*)pUStru1->szAddr,(char*)szUser ) )
                     {
                       leto_CloseUS( pUStru1 );
                     }
                     ui ++;
                     pUStru1 ++;
                  }
               }
               else
               {
                  sscanf( (char*)szUser,"%d;", &iUser );
                  if( iUser < uiUsersMax )
                  {
                    leto_CloseUS( s_users + iUser );
                  }
               }
            }
            leto_SendAnswer( pUStru, szOk, 4 );
            break;
         }
         default:
         {
            leto_SendAnswer( pUStru, szErr2, 4 );
         }
      }

   }
   uiAnswerSent = 1;
}

void leto_Intro( PUSERSTRU pUStru, char* szData )
{
   const char * pData = NULL;
   char * pp1, * pp2, * pp3, * ptr;
   char pBuf[88];
   int nParam = leto_GetParam( szData, (const char**)&pp1, (const char**)&pp2, (const char**)&pp3, (const char**)&ptr );

   if( nParam < 3 )
      pData = szErr2;
   else
   {
      if( pUStru->szVersion )
         hb_xfree( pUStru->szVersion );
      pUStru->szVersion = (BYTE*) hb_xgrab( pp1 - szData );
      memcpy( pUStru->szVersion, szData, pp1 - szData - 1 );
      pUStru->szVersion[pp1-szData-1] = '\0';

      if( pUStru->szNetname )
         hb_xfree( pUStru->szNetname );
      pUStru->szNetname = (BYTE*) hb_xgrab( pp2 - pp1 );
      memcpy( pUStru->szNetname, pp1, pp2-pp1-1 );
      pUStru->szNetname[pp2-pp1-1] = '\0';

      if( pUStru->szExename )
         hb_xfree( pUStru->szExename );
      pUStru->szExename = (BYTE*) hb_xgrab( pp3 - pp2 );
      memcpy( pUStru->szExename, pp2, pp3-pp2-1 );
      pUStru->szExename[pp3-pp2-1] = '\0';

      pp1 = strchr( ptr, ';' );
      pp1 ++;

      /* pp3 - UserName, ptr - password */
      // leto_writelog( szData,0 );
      if( bPass4L || bPass4M || bPass4D )
      {
         BOOL bPassOk = 0;

         if( (ptr - pp3) > 1 && (ptr - pp3) < 18 )
         {
            char szBuf[36], szUser[18], szPass[24], * ppw;
            ULONG ulLen = ptr-pp3-1;

            memcpy( szUser, pp3, ulLen );
            ppw = szUser + ulLen;
            *ppw = '\0';
            // leto_writelog( szUser,0 );

            if( (pp1 - ptr) > 4 && (pp1 - ptr) < 50 )
            {
               char szKey[LETO_MAX_KEYLENGTH+1];
               USHORT uiKeyLen = strlen(LETO_PASSWORD);

               if( uiKeyLen > LETO_MAX_KEYLENGTH )
                  uiKeyLen = LETO_MAX_KEYLENGTH;

               memcpy( szKey, LETO_PASSWORD, uiKeyLen );
               szKey[uiKeyLen] = '\0';
               szKey[0] = pUStru->szDopcode[0];
               szKey[1] = pUStru->szDopcode[1];
               leto_hexchar2byte( ptr, pp1-ptr-1, szBuf );
               ulLen = (pp1-ptr-1) / 2;
               leto_decrypt( szBuf, ulLen, szPass, &ulLen, szKey );
               szPass[ulLen] = '\0';
               // leto_writelog( szPass,0 );
            }
            else
               ulLen = 0;

            pp3 = leto_acc_find( szUser, szPass );
            // leto_writelog( szUser,0 );
            // leto_writelog( szPass,0 );
            // leto_writelog( (pp3)? "Yes":"No",0 );
            if( pp3 )
               bPassOk = 1;
         }
         if( bPassOk )
         {
            // leto_writelog("pass - Ok",0);
            pUStru->szAccess[0] = *pp3;
            pUStru->szAccess[1] = *(pp3+1);
         }
         else if( bPass4L )
         {
            leto_CloseUS( pUStru );
            uiAnswerSent = 1;
            return;
         }
         else
            pData = szErrAcc;
      }
      else
         pUStru->szAccess[0] = pUStru->szAccess[1] = 0xff;

      if( nParam > 5 )
      {
         if( ( ptr = strchr( pp1, ';' ) ) != NULL )
         {
            *ptr = '\0';
            pUStru->cdpage = hb_cdpFind( pp1 );
            pp1 = ptr + 1;
            if( ( ptr = strchr( pp1, ';' ) ) != NULL )
            {
               if( pUStru->szDateFormat )
                  hb_xfree( pUStru->szDateFormat );
               pUStru->szDateFormat = (char*) hb_xgrab( ptr - pp1 + 1 );
               memcpy( pUStru->szDateFormat, pp1, ptr - pp1 );
               pUStru->szDateFormat[ptr-pp1] = '\0';
               if( *(ptr + 1) == ';')
                  pUStru->bCentury = (*(ptr + 2) == 'T' ? TRUE : FALSE);
            }
         }
      }
      if( !pData )
      {
         char * szVersion = hb_verHarbour();
         sprintf(pBuf, "+%c%c%c;%s;", ( pUStru->szAccess[0] & 1 )? 'Y' : 'N',
               ( pUStru->szAccess[0] & 2 )? 'Y' : 'N',
               ( pUStru->szAccess[0] & 4 )? 'Y' : 'N',
               szVersion);
         hb_xfree( szVersion );
         pData = pBuf;
         /* pData = szOk; */
      }
   }
   leto_SendAnswer( pUStru, pData, strlen( pData ) );
   uiAnswerSent = 1;

}

void leto_CloseAll4Us( PUSERSTRU pUStru )
{

   if( pUStru->pAStru )
   {
      USHORT ui;
      PAREASTRU pAStru = pUStru->pAStru;
      for( ui=0; ui<pUStru->uiAreasAlloc; ui++,pAStru++ )
         if( pAStru->pTStru )
            leto_CloseArea( pUStru-s_users, ui );
   }
   leto_SendAnswer( pUStru, szOk, 4 );
   uiAnswerSent = 1;

}

void leto_CloseT( PUSERSTRU pUStru, const char* szData )
{
   ULONG ulAreaID;
   USHORT ui;
   const char * pData;
   PAREASTRU pAStru = pUStru->pAStru;

   sscanf( szData, "%lu;", &ulAreaID );

   if( ulAreaID )
   {
      for( ui=0; ui<pUStru->uiAreasAlloc; ui++,pAStru++ )
         if( pAStru->ulAreaNum == ulAreaID )
            break;

      if( ui < pUStru->uiAreasAlloc )
      {
         leto_CloseArea( pUStru-s_users, ui );
         pData = szOk;
      }
      else
         pData = szErr3;
   }
   else
      pData = szErr2;

   leto_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;
}

void leto_Pack( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   ULONG ulAreaID;
   const char * pData;

   sscanf( szData, "%lu;", &ulAreaID );

   if( ulAreaID )
   {
      pArea = leto_SelectArea( ulAreaID, 0, NULL );
      if( pArea )
      {
         if( PDBFAREA( valResult ) )
            hb_itemClear( PDBFAREA( valResult ) );
         else
            PDBFAREA( valResult ) = hb_itemNew( NULL );
         hb_xvmSeqBegin();
         SELF_PACK( (AREAP) pArea );
         hb_xvmSeqEnd();
         if( bHrbError )
            pData = szErr101;
         else
            pData = szOk;
      }
      else
         pData = szErr3;
   }
   else
      pData = szErr2;

   leto_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;

}

void leto_Zap( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   ULONG ulAreaID;
   const char * pData;

   sscanf( szData, "%lu;", &ulAreaID );

   if( ulAreaID )
   {
      pArea = leto_SelectArea( ulAreaID, 0, NULL );
      if( pArea )
      {
         hb_xvmSeqBegin();
         SELF_ZAP( (AREAP) pArea );
         hb_xvmSeqEnd();
         if( bHrbError )
            pData = szErr101;
         else
            pData = szOk;
      }
      else
         pData = szErr3;
   }
   else
      pData = szErr2;
   leto_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;

}

void leto_Flush( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   ULONG ulAreaID;
   const char * pData;

   sscanf( szData, "%lu;", &ulAreaID );

   if( ulAreaID )
   {
      pArea = leto_SelectArea( ulAreaID, 0, NULL );
      if( pArea )
      {
         hb_xvmSeqBegin();
         SELF_FLUSH( ( AREAP ) pArea );
         hb_xvmSeqEnd();
         if( bHrbError )
            pData = szErr101;
         else
            pData = szOk;
      }
      else
         pData = szErr3;
   }
   else
      pData = szErr2;
   leto_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;

}

void leto_Reccount( PUSERSTRU pUStru, const char* szData )
{
   DBFAREAP pArea;
   ULONG ulAreaID;
   ULONG ulRecCount = 0;
   const char * pData;
   char s[16];

   sscanf( szData, "%lu;", &ulAreaID );

   if( ulAreaID )
   {
      pArea = leto_SelectArea( ulAreaID, 0, NULL );
      if( pArea )
      {
         SELF_RECCOUNT( (AREAP) pArea, &ulRecCount );
         sprintf( s, "+%lu;", ulRecCount );
         pData = s;
      }
      else
         pData = szErr2;
   }
   else
      pData = szErr2;
   leto_SendAnswer( pUStru, pData, strlen(pData) );
   uiAnswerSent = 1;

}

void leto_Set( PUSERSTRU pUStru, const char* szData )
{

   if( *szData != '0' || *(szData+2) != ';' || ( !strchr( szData+3, ';' ) ) )
   {
      leto_SendAnswer( pUStru, szErr2, 4 );
   }
   else
   {
      switch( *(szData+1) )
      {
         case '1':
            break;
         case '2':
         {
            ULONG ulAreaID;
            int   iSkipBuf;
            PAREASTRU pAStru;

            sscanf( szData+3, "%lu;%d;", &ulAreaID, &iSkipBuf );
            if( ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) != NULL )
            {
               pAStru->uiSkipBuf = (USHORT) iSkipBuf;
            }
            break;
         }
      }
      leto_SendAnswer( pUStru, szOk, 4 );
   }
   uiAnswerSent = 1;
}

void leto_Transaction( PUSERSTRU pUStru, const char* szData, ULONG ulTaLen )
{
   DBFAREAP pArea;
   USHORT uiLenLen;
   int iRecNumber, i = 0, i1;
   ULONG ulLen, ulRecNo;
   const char * ptr = szData;
   const char * pData = NULL;
   TRANSACTSTRU * pTA;
   int iRes = 0;
   BOOL bUnlockAll;
   char szData1[16];

   if( ( bPass4D && !( pUStru->szAccess[0] & 4 ) ) )
   {
      leto_SendAnswer( pUStru, szErrAcc, 4 );
      uiAnswerSent = 1;
      return;
   }

   ulTransAll ++;
   iRecNumber = leto_b2n( ptr, 2 );
   ptr ++; ptr ++;
   bUnlockAll = ( *ptr & 1 );
   ptr ++;

   if( iRecNumber )
   {
      pTA = (TRANSACTSTRU *) malloc( sizeof(TRANSACTSTRU) * iRecNumber );
      memset( pTA,0,sizeof(TRANSACTSTRU)*iRecNumber );

      while( !iRes && i < iRecNumber && ((ULONG)( ptr - szData )) < ulTaLen )
      {
         if( ( uiLenLen = (((int)*ptr) & 0xFF) ) < 10 )
         {
            ulLen = leto_b2n( ptr+1, uiLenLen );
            ptr += uiLenLen + 1;

            // sprintf( s,"Transact-1A %c%c%c%c%c",*ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4) );
            // leto_writelog( s,0 );

            if( !strncmp( ptr, "upd;", 4 ) )
               iRes = UpdateRec( pUStru, ptr+4, FALSE, NULL, &pTA[i] );
            else if( !strncmp( ptr, "add;", 4 ) )
               iRes = UpdateRec( pUStru, ptr+4, TRUE, NULL, &pTA[i] );
            else if( !strncmp( ptr, "memo;", 5 ) )
               iRes = leto_Memo( pUStru, ptr+5, &pTA[i] );
            else
               iRes = 2;
            ptr += ulLen;
         }
         else
         {
            iRes  = 2;
            pData = szErr2;
            break;
         }
         i ++;
      }

      if( !iRes )
      {
         // sprintf( s,"Transact-2 %d",iRecNumber );
         // leto_writelog( s,0 );

         hb_xvmSeqBegin();
         for( i=0; i < iRecNumber && !bHrbError; i++ )
         {
            pArea = pTA[i].pArea;

            // sprintf( s,"Transact-3 %d %d %d %d",i,pTA[i].bAppend,pTA[i].uiFlag,pTA[i].uiItems );
            // leto_writelog( s,0 );
            if( pTA[i].bAppend )
            {
               PHB_ITEM pItem = hb_itemPutNL( NULL, 0 );
               hb_rddSetNetErr( FALSE );
               SELF_APPEND( (AREAP)pArea, TRUE );

               SELF_RECID( (AREAP)pArea, pItem );
               pTA[i].ulRecNo = (ULONG)hb_itemGetNL( pItem );
               hb_itemRelease( pItem );
            }
            else
            {
               ulRecNo = pTA[i].ulRecNo;
               if( !ulRecNo )
               {
                  for( i1=i-1; i1>=0; i1-- )
                     if( pArea == pTA[i1].pArea && pTA[i1].bAppend )
                     {
                        ulRecNo = pTA[i1].ulRecNo;
                        break;
                     }
               }
               leto_GotoIf( pArea, ulRecNo );
            }

            if( pTA[i].uiFlag & 1 )
               SELF_DELETE( (AREAP)pArea );
            else if( pTA[i].uiFlag & 2 )
               SELF_RECALL( (AREAP)pArea );

            for( i1=0; i1 < pTA[i].uiItems; i1++ )
               if( pTA[i].pItems[i1] )
                  SELF_PUTVALUE( (AREAP)pArea, pTA[i].puiIndex[i1], pTA[i].pItems[i1] );

         }

         hb_xvmSeqEnd();
         if( bHrbError )
            pData = szErr101;
         else
         {
            pData = szOk;
            ulTransOK ++;
         }
      }
      else if( iRes == 2 )
         pData = szErr2;
      else if( iRes == 3 )
         pData = szErr3;
      else if( iRes == 4 )
         pData = szErr4;
      else if( iRes > 100 )
      {
         sprintf( szData1, "-%d;", iRes );
         pData = szData1;
      }

      for( i=0; i < iRecNumber; i++ )
      {
         if( pTA[i].puiIndex )
            free( pTA[i].puiIndex );
         if( pTA[i].pItems )
         {
            for( i1=0; i1 < pTA[i].uiItems; i1++ )
               if( pTA[i].pItems[i1] )
                  hb_itemRelease( pTA[i].pItems[i1] );
            free( pTA[i].pItems );
         }
      }
      free( pTA );
   }
   else
      pData = szErr2;

   if( bUnlockAll )
   {
      PAREASTRU pAStru = pUStru->pAStru;
      for( i=0; i<pUStru->uiAreasAlloc; i++,pAStru++ )
         if( pAStru->pTStru )
            leto_UnlockAllRec( pAStru );
   }

   leto_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;
}

static PHB_ITEM leto_mkCodeBlock( const char * szExp, ULONG ulLen )
{
   PHB_ITEM pBlock = NULL;

   if( ulLen > 0 )
   {
      char * szMacro = ( char * ) hb_xgrab( ulLen + 5 );

      szMacro[ 0 ] = '{';
      szMacro[ 1 ] = '|';
      szMacro[ 2 ] = '|';
      memcpy( szMacro + 3, szExp, ulLen );
      szMacro[ 3 + ulLen ] = '}';
      szMacro[ 4 + ulLen ] = '\0';

      pBlock = hb_itemNew( NULL );
      hb_vmPushString( szMacro, ulLen+5 );
      hb_xfree( szMacro );
      hb_macroGetValue( hb_stackItemFromTop( -1 ), 0, 64 );
      hb_itemMove( pBlock, hb_stackItemFromTop( -1 ) );
      hb_stackPop();
   }

   return pBlock;
}

void leto_Filter( PUSERSTRU pUStru, const char* szData )
{
   const char * pFilter;
   int nParam = leto_GetParam( szData, &pFilter, NULL, NULL, NULL );

   if( nParam < 1 )
   {
      leto_SendAnswer( pUStru, szErr2, 4 );
   }
   else
   {
      ULONG ulAreaID;
      PAREASTRU pAStru;

      sscanf( szData, "%lu;", &ulAreaID );
      if( ulAreaID && ( pAStru = leto_FindArea( pUStru, ulAreaID ) ) != NULL )
      {
         char * ptr;
         BOOL bClear, bRes;

         leto_SelectArea( ulAreaID, 0, NULL );
         ptr = strchr(pFilter, ';');
         if( ptr && ptr - pFilter > 1 )
            bClear = FALSE;
         else
            bClear = TRUE;

         if( pAStru->itmFltExpr )
         {
            hb_itemClear( pAStru->itmFltExpr );
            hb_itemRelease( pAStru->itmFltExpr );
         }

         if( bClear )
         {
            pAStru->itmFltExpr = NULL;
            leto_SendAnswer( pUStru, "++", 2 );
         }
         else
         {
            PHB_ITEM pFilterBlock = NULL;

            if( ! leto_ParseFilter( pFilter, ptr - pFilter ) )
            {
               bRes = FALSE;
            }
            else
            {
               bRes = TRUE;
               hb_xvmSeqBegin();
               pFilterBlock = leto_mkCodeBlock( pFilter, ptr - pFilter );
               if( pFilterBlock == NULL )
                  bRes = FALSE;
               else
                  hb_vmEvalBlock( pFilterBlock );
               hb_xvmSeqEnd();
               if( bHrbError )
               {
                  bRes = FALSE;
                  if( pFilterBlock )
                     hb_itemRelease( pFilterBlock );
               }
            }
            if( ! bRes )
            {
               leto_SendAnswer( pUStru, "-012", 4 );
            }
            else
            {
               pAStru->itmFltExpr = pFilterBlock;
               leto_SendAnswer( pUStru, "++", 2 );
            }

         }

      }
      else
      {
         leto_SendAnswer( pUStru, szErr2, 4 );
      }
   }

   uiAnswerSent = 1;
}

void leto_Sum( PUSERSTRU pUStru, const char* szData, USHORT uiDataLen )
{
   const char * pOrder, * pFields, * pFilter, *pFlag;
   int nParam = leto_GetParam( szData, &pOrder, &pFields, &pFilter, &pFlag );

   if( nParam < 3 )
   {
      leto_SendAnswer( pUStru, szErr2, 4 );
   }
   else
   {
      ULONG ulAreaID;

      sscanf( szData, "%lu;", &ulAreaID );

      if( ulAreaID )
      {
         char  szOrder[16];
         typedef struct
         {
            SHORT Pos;
            USHORT Dec;
            union
            {
               double dSum;
/*
#ifndef __XHARBOUR__
               HB_LONGLONG lSum;
#else
               LONGLONG lSum;
#endif
*/
               LONG lSum;
            } value;
         } SUMSTRU;
         SUMSTRU * pSums;
         USHORT usCount = 0, usAllocated = 10;
         BOOL bEnd = FALSE;
         USHORT usIndex;
         ULONG ulLen;
         int iKeyLen;
         char szFieldName[12];
         PHB_ITEM pItem = hb_itemNew( NULL );
         PHB_ITEM pFilterBlock = NULL;
         PHB_ITEM pTopScope = NULL;
         PHB_ITEM pBottomScope = NULL;
         BOOL bEof;
         char *ptr, * pNext, *ptrend;
         DBFAREAP pArea = leto_SelectArea( ulAreaID, 0, NULL );
//         PAREASTRU pAStru = leto_FindArea( pUStru, ulAreaID );
         char * pData;

         if( pFields-pOrder-1 < 16 )
         {
            memcpy( szOrder, pOrder, pFields-pOrder-1 );
            szOrder[ pFields-pOrder-1 ] = '\0';
         }
         else
            szOrder[0] = '\0';

         pSums = (SUMSTRU *) hb_xgrab( sizeof( SUMSTRU ) * usAllocated );
         ptr = ( char * ) pFields;
         ptrend = strchr(ptr, ';');
         while( TRUE )
         {
            pNext = strchr(ptr, ',');
            if( ! pNext || pNext > ptrend )
            {
               pNext = strchr(ptr, ';');
               if( ! pNext ) break;
               bEnd = TRUE;
            }

            usCount ++;
            if( usCount > usAllocated )
            {
               usAllocated += 10;
               pSums = (SUMSTRU *) hb_xrealloc( pSums, sizeof( SUMSTRU ) * usAllocated );
            }

            if( pNext-ptr < 12)
            {
               memcpy( szFieldName, ptr, pNext-ptr );
               szFieldName[ pNext-ptr ] = '\0';
            }
            else
               szFieldName[0] = '\0';

            if( szFieldName[0] == '#' )
            {
               pSums[ usCount-1 ].Pos = -1;
               pSums[ usCount-1 ].Dec = 0;
            }
            else
            {
               pSums[ usCount-1 ].Pos = (SHORT) hb_rddFieldIndex( (AREAP) pArea, szFieldName );
               if( pSums[ usCount-1 ].Pos )
               {
                  SELF_FIELDINFO( (AREAP) pArea, pSums[ usCount-1 ].Pos, DBS_DEC, pItem );
                  pSums[ usCount-1 ].Dec = hb_itemGetNI( pItem );
               }
               else
               {
                  pSums[ usCount-1 ].Dec = 0;
               }
            }

            if( pSums[ usCount-1 ].Dec > 0 )
               pSums[ usCount-1 ].value.dSum = 0.0;
            else
               pSums[ usCount-1 ].value.lSum = 0;

            if( bEnd ) break;
            ptr = pNext + 1;
         }

         if( pFilter )
         {
            ulLen = ( pFlag ? pFlag - pFilter - 1 : strlen(pFilter) );
            if( ulLen )
            {
               pFilterBlock = leto_mkCodeBlock( pFilter, ulLen );
            }
         }

         if( pFlag )
         {
            hb_setSetDeleted( *pFlag == 0x41 );

            ptr = ( char * ) pFlag + 2;
            if( ptr < szData + uiDataLen )
            {
               iKeyLen = (((int)*ptr++) & 0xFF);

               if( iKeyLen )
               {
                  pTopScope = leto_KeyToItem( pArea, ptr, iKeyLen, szOrder );
                  ptr += iKeyLen;
                  iKeyLen = (((int)*ptr++) & 0xFF);
                  if( ptr < szData + uiDataLen )
                  {
                     pBottomScope = leto_KeyToItem( pArea, ptr, iKeyLen, szOrder );
                  }
               }
            }
         }

         hb_xvmSeqBegin();

         if( szOrder[0] != '\0' )
         {
            leto_SetFocus( (AREAP) pArea, szOrder );
         }

         if( pTopScope )
         {
            leto_ScopeCommand(pArea, DBOI_SCOPETOP, pTopScope );
            leto_ScopeCommand(pArea, DBOI_SCOPEBOTTOM, (pBottomScope ? pBottomScope : pTopScope ) );
         }

         if( pFilterBlock )
         {
            PDBFAREA( dbfi.itmCobExpr ) = pFilterBlock;
            PDBFAREA( dbfi.fFilter ) = TRUE;
         }

         SELF_GOTOP( (AREAP) pArea );
         while( TRUE )
         {
            SELF_EOF( (AREAP) pArea, &bEof );
            if( bEof ) break;
            for( usIndex = 0; usIndex < usCount; usIndex ++ )
            {
               if( pSums[ usIndex ].Pos < 0 )
               {
                  pSums[ usIndex ].value.lSum ++;
               }
               else if( pSums[ usIndex ].Pos > 0 )
               {
                  if( SELF_GETVALUE( (AREAP) pArea, pSums[ usIndex ].Pos, pItem ) == SUCCESS )
                  {
                     if( pSums[ usIndex ].Dec > 0 )
                     {
                        pSums[ usIndex ].value.dSum += hb_itemGetND( pItem );
                     }
                     else
                     {
                        pSums[ usIndex ].value.lSum += hb_itemGetNL( pItem );
                     }
                  }
               }
            }
            SELF_SKIP( (AREAP) pArea, 1);
         }

         if( pFilterBlock )
            leto_ClearFilter( pArea )

         if( pTopScope )
         {
            leto_ScopeCommand(pArea, DBOI_SCOPETOPCLEAR, NULL );
            leto_ScopeCommand(pArea, DBOI_SCOPEBOTTOMCLEAR, NULL );
         }

         hb_xvmSeqEnd();

         if( pFilterBlock )
         {
            hb_itemClear( pFilterBlock );
            hb_itemRelease( pFilterBlock );
         }
         if( pTopScope )
            hb_itemRelease( pTopScope );
         if( pBottomScope )
            hb_itemRelease( pBottomScope );

         if( bHrbError )
         {
            leto_SendAnswer( pUStru, szErr4, 4 );
         }
         else
         {

            pData = hb_xgrab( usCount*21 + 3 );
            pData[0] = '+';
            ptr = pData + 1;
            for( usIndex = 0; usIndex < usCount; usIndex ++ )
            {
               if( ptr > pData + 1)
                  *ptr++ = ',';
               if( pSums[ usIndex ].Dec > 0 )
               {
                  ULONG ulLen;
                  BOOL bFreeReq;
                  char * buffer, *bufstr;

                  hb_itemPutNDDec( pItem, pSums[ usIndex ].value.dSum, pSums[ usIndex ].Dec );
                  buffer = hb_itemString( pItem, &ulLen, &bFreeReq );
                  bufstr = buffer;
                  while( *bufstr && *bufstr == ' ' )
                     bufstr ++;
                  strcpy(ptr, bufstr);

                  if( bFreeReq )
                     hb_xfree( buffer );
               }
               else
               {
                  sprintf(ptr, "%ld", pSums[ usIndex ].value.lSum);
               }
               ptr += strlen(ptr);
            }
            *ptr++ = ';';
            *ptr = '\0';

            leto_SendAnswer( pUStru, pData, strlen( pData ) );

            hb_xfree( pData );
         }
         hb_xfree( (char *) pSums );
         hb_itemRelease( pItem );

      }
      else
      {
         leto_SendAnswer( pUStru, szErr2, 4 );
      }

   }
   uiAnswerSent = 1;

}


void leto_runFunc( PUSERSTRU pUStru, PHB_DYNS* ppSym, const char* szName, const char* pCommand, ULONG ulLen )
{

   if( !( *ppSym ) )
      *ppSym = hb_dynsymFindName( szName );

   if( *ppSym )
   {
      hb_vmPushSymbol( hb_dynsymSymbol( *ppSym ) );
      hb_vmPushNil();
      if( pUStru )
      {
         hb_vmPushLong( (LONG ) ( pUStru-s_users ) );
         hb_vmPushString( (char*)pCommand, ulLen );
         hb_vmDo( 2 );
         leto_SendAnswer( pUStru, hb_parc( -1 ), hb_parclen( -1 ) );
         uiAnswerSent = 1;
      }
      else
         hb_vmDo( 0 );
   }

}

void leto_User( PUSERSTRU pUStru, const char* pData )
{
   char *ptr;

   if( ( *(pData+2) != ';' ) || ( *pData != '0' ) ||
          ( ptr = strchr( pData+3, ';' ) ) == NULL )
   {
      leto_SendAnswer( pUStru, szErr2, 4 );
   }
   else
   {
      const char * pp1, * pp2;
      int nParam = leto_GetParam( pData+3, &pp1, &pp2, NULL, NULL );

      if( nParam >= 1 )
      {
         char szName[ HB_SYMBOL_NAME_LEN + 1 ];
         PHB_DYNS pSym;
         ULONG ulLen = ( pp2 ? pp2-pp1-1 : 0 );

         memcpy( szName, pData+3, pp1-(pData+3)-1 );
         szName[pp1-(pData+3)-1] = '\0';
         hb_strUpper( szName, strlen(szName) );
         pSym = hb_dynsymFindName( szName );
         if( pSym )
         {
            char * pData1;
            hb_vmPushSymbol( hb_dynsymSymbol( pSym ) );
            hb_vmPushNil();
            hb_vmPushLong( (LONG ) ( pUStru-s_users ) );
            hb_vmPushString( pp1, ulLen );
            hb_vmDo( 2 );
            ulLen = hb_parclen( -1 );
            pData1 = hb_xgrab( ulLen + 7 );
            pData1[0] = '+';
            
            HB_PUT_LE_UINT32( pData1+1, ulLen );
            memcpy( pData1 + 5, hb_parc( -1 ), ulLen );
            pData1[ ulLen + 5 ] = ';';
            leto_SendAnswer( pUStru, pData1, ulLen + 6 );
            hb_xfree( pData1 );
            uiAnswerSent = 1;
         }
      }

   }
}

void ParseCommand( PUSERSTRU pUStru )
{
   const char * ptr;
   ULONG ulLen;

   static PHB_DYNS pSym_Fdboi   = NULL;
   static PHB_DYNS pSym_Fdbri   = NULL;
   static PHB_DYNS pSym_Fopent  = NULL;
   static PHB_DYNS pSym_Fopeni  = NULL;
   static PHB_DYNS pSym_Fcreat  = NULL;
   static PHB_DYNS pSym_Fcreai  = NULL;
//   static PHB_DYNS pSym_Fsetrel = NULL;
//   static PHB_DYNS pSym_Fclrrel = NULL;

   bHrbError = 0;
   uiAnswerSent = 0;
   ptr = ( const char * ) pUStru->pBufRead;
   ulLen = pUStru->ulDataLen;
   // leto_writelog(ptr, 0);
   switch( *ptr )
   {
      case 's':
         // scop, seek, set, skip
         switch( *(ptr+1) )
         {
            case 'k':
               if( !strncmp( ptr,"skip;",5 ) )
                  leto_Skip( pUStru, ptr+5 );
               break;
            case 'e':
               if( !strncmp( ptr,"seek;",5 ) )
                  leto_Seek( pUStru, ptr+5 );
               else if( !strncmp( ptr,"setf;",5 ) )
                  leto_Filter( pUStru, ptr+5 );
               break;
            case 'c':
               if( !strncmp( ptr,"scop;",5 ) )
                  leto_Scope(pUStru, ptr+5);
               break;
            case 'u':
               if( !strncmp( ptr,"sum;",4 ) )
                  leto_Sum( pUStru, ptr+4, ulLen-4 );
               break;
         }
         break;
      case 'g':
         // goto
         if( !strncmp( ptr,"goto;",5 ) )
            leto_Goto( pUStru, ptr+5 );
         break;
      case 'u':
         // upd, unlock, usr
         if( !strncmp( ptr,"upd;",4 ) )
            leto_UpdateRec( pUStru, ptr+4, FALSE );
         else if( !strncmp( ptr,"unlock;",7 ) )
            leto_Unlock( pUStru, ptr+7 );
         else if( !strncmp( ptr,"usr;",4 ) )
            leto_User( pUStru, ptr+4 );
         break;
      case 'a':
         // add
         if( !strncmp( ptr,"add;",4 ) )
            leto_UpdateRec( pUStru, ptr+4, TRUE );
         break;
      case 'd':
         // dboi, dbri
         if( !strncmp( ptr,"dboi;",5 ) )
            leto_runFunc( pUStru, &pSym_Fdboi, "HS_ORDERINFO", ptr+5, ulLen-5 );
         else if( !strncmp( ptr,"dbri;",5 ) )
            leto_runFunc( pUStru, &pSym_Fdbri, "HS_RECORDINFO", ptr+5, ulLen-5 );
         break;
      case 'r':
         switch( *(ptr+1) )
         {
            case 'c':
            // rcou
               if( !strncmp( ptr,"rcou;",5 ) )
                  leto_Reccount( pUStru, ptr+5 );
               break;
/*
            case 'e':
            // rel
               if( !strncmp( ptr,"rel;01;",7 ) )
                  leto_runFunc( pUStru, &pSym_Fsetrel, "HS_SETREL", ptr+7, ulLen-7 );
               else if( !strncmp( ptr,"rel;02;",7 ) )
                  leto_runFunc( pUStru, &pSym_Fclrrel, "HS_CLRREL", ptr+7, ulLen-7 );
               break;
*/
         }
      case 'f':
         // flush, file
         if( !strncmp( ptr,"flush;",6 ) )
            leto_Flush( pUStru, ptr+6 );
         else if( !strncmp( ptr,"file;",5 ) )
            leto_filef( pUStru, ptr+5 );
         break;
      case 'l':
         // lock
         if( !strncmp( ptr,"lock;",5 ) )
            leto_Lock( pUStru, ptr+5 );
         break;
      case 'i':
         // islock
         if( !strncmp( ptr,"islock;",7 ) )
            leto_IsRecLockedUS( pUStru, ptr+7 );
         break;
      case 'm':
         // memo, mgmt
         if( !strncmp( ptr,"memo;",5 ) )
            leto_Memo( pUStru, ptr+5, NULL );
         break;
      case 'o':
         // open, ord
         if( !strncmp( ptr,"open;",5 ) )
         {
            if( !strncmp( ptr+5,"01;",3 ) )
               leto_runFunc( pUStru, &pSym_Fopent, "HS_OPENTABLE", ptr+8, ulLen-8 );
            else if( !strncmp( ptr+5,"02;",3 ) )
               leto_runFunc( pUStru, &pSym_Fopeni, "HS_OPENINDEX", ptr+8, ulLen-8 );
         }
         else if( !strncmp( ptr,"ord;",4 ) )
            leto_Ordfunc( pUStru, ptr+4 );
         break;
      case 'c':
         // creat, close
         if( !strncmp( ptr,"close;",6 ) )
         {
            if( !strncmp( ptr+6,"01;",3 ) )
               leto_CloseT( pUStru, ptr+9 );
            else if( !strncmp( ptr+6,"00;",3 ) )
               leto_CloseAll4Us( pUStru );
         }
         else if( !strncmp( ptr,"creat;",6 ) )
         {
            if( !strncmp( ptr+6,"01;",3 ) )
               leto_runFunc( pUStru, &pSym_Fcreat, "HS_CREATETABLE", ptr+9, ulLen-9 );
            else if( !strncmp( ptr+6,"02;",3 ) )
               leto_runFunc( pUStru, &pSym_Fcreai, "HS_CREATEINDEX", ptr+9, ulLen-9 );
         }
         break;
      case 'p':
         // pack, ping
         if( !strncmp( ptr,"pack;",5 ) )
            leto_Pack( pUStru, ptr+5 );
         else if( !strncmp( ptr,"ping;",5 ) )
            leto_SendAnswer( pUStru, ptr, ulLen );
         break;
      case 't':
         // transaction
         if( !strncmp( ptr,"ta;",3 ) )
            leto_Transaction( pUStru, ptr+3, ulLen-3 );
         break;
      case 'q':
         // quit
         if( !strncmp( ptr,"quit;",5 ) )
         {
            leto_CloseUS( pUStru );
            uiAnswerSent = 1;
         }
         break;
      case 'z':
         // zap
         if( !strncmp( ptr,"zap;",4 ) )
            leto_Zap( pUStru, ptr+4 );
         break;
   }
   if( !uiAnswerSent )
      leto_SendAnswer( pUStru, "-001", 4 );
}
