/*  $Id: $  */

/*
 * Harbour Project source code:
 * Leto db server core function, Harbour mt model
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

#define _HB_API_INTERNAL_
#include "hbthread.h"
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

#ifndef HB_OS_PATH_DELIM_CHR
   #define HB_OS_PATH_DELIM_CHR OS_PATH_DELIMITER
#endif

#if !defined (SUCCESS)
#define SUCCESS            0
#define FAILURE            1
#endif

typedef struct
{
   HB_THREAD_ID id;
   HB_THREAD_HANDLE hThread;
   HB_COND_T * cond;
   HB_CRITICAL_T * mutex;
   HB_BOOL bLock;
   char * szData;
} LETO_THREAD;

static const char * szRelease = "Leto DB Server v.0.95";

static int iServerPort;
static HB_SOCKET_T hSocketMain;          // Initial server socket

static BOOL   bThread1 = 1;
static BOOL   bThread2 = 1;

static BOOL   bUnlockRes = 1;

static HB_CRITICAL_NEW( mutex_thread2_1 );

static HB_BOOL bLock_thread2;
static HB_COND_NEW( cond_thread2 );
static HB_CRITICAL_NEW( mutex_thread2_2 );
static HB_THREAD_HANDLE hThread2;
static HB_THREAD_ID th_id2;
static const char * szThread2Data = "two";

static LETO_THREAD * s_thread;

extern USHORT uiAnswerSent;
extern ULONG ulBytesSent;
extern ULONG ulOperations;
extern ULONG ulBytesRead;

extern PUSERSTRU s_users;
extern PTABLESTRU s_tables;

extern USHORT uiUsersMax;
extern USHORT uiTablesAlloc;

extern BOOL bHrbError;

extern double dMaxWait;
extern double dMaxDayWait;
extern long   ulToday;
extern double dSumWait[];
extern unsigned int uiSumWait[];

extern BOOL   bPass4L;
extern BOOL   bPass4M;
extern BOOL   bPass4D;
extern BOOL   bCryptTraf;

extern USHORT uiUsersCurr;
extern PUSERSTRU pUStru_Curr;
extern ULONG  ulAreaID_Curr;

extern ULONG leto_MilliSec( void );
extern long leto_Date( void );
extern void leto_Admin( PUSERSTRU pUStru, char* szData );
extern void leto_Intro( PUSERSTRU pUStru, char* szData );
extern void leto_filef( PUSERSTRU pUStru, const char* szData );
extern void leto_IsRecLockedUS( PUSERSTRU pUStru, const char* szData );
extern void leto_Goto( PUSERSTRU pUStru, const char * szData );
extern void leto_Lock( PUSERSTRU pUStru, const char* szData );
extern void leto_Unlock( PUSERSTRU pUStru, const char* szData );
extern void leto_Seek( PUSERSTRU pUStru, const char* szData );
extern void leto_Scope( PUSERSTRU pUStru, const char* szData );
extern void leto_Skip( PUSERSTRU pUStru, const char* szData );
extern void leto_Ordfunc( PUSERSTRU pUStru, const char* szData );
extern int leto_Memo( PUSERSTRU pUStru, const char* szData, TRANSACTSTRU * pTA );
extern void leto_Mgmt( PUSERSTRU pUStru, const char* szData );
extern void leto_CloseAll4Us( PUSERSTRU pUStru );
extern void leto_CloseT( PUSERSTRU pUStru, const char* szData );
extern void leto_CloseTable( USHORT nTableStru );
extern PUSERSTRU leto_InitUS( HB_SOCKET_T hSocket );
extern void leto_CloseUS( PUSERSTRU pUStru );
extern void leto_Pack( PUSERSTRU pUStru, const char* szData );
extern void leto_Zap( PUSERSTRU pUStru, const char* szData );
extern void leto_Flush( PUSERSTRU pUStru, const char* szData );
extern void leto_Reccount( PUSERSTRU pUStru, const char* szData );
extern void leto_UpdateRec( PUSERSTRU pUStru, const char* szData, BOOL bAppend );
extern void leto_Set( PUSERSTRU pUStru, const char* szData );
extern void leto_Variables( PUSERSTRU pUStru, char* szData );
extern void leto_Transaction( PUSERSTRU pUStru, const char* szData, ULONG ulTaLen );
extern PAREASTRU leto_FindArea( PUSERSTRU pUStru, ULONG ulAreaID );
extern void leto_CloseMemArea( void );

void leto_ThreadIdLog( void )
{
   char s[24];
   PHB_THREADSTATE pThread = ( PHB_THREADSTATE ) hb_vmThreadState();

   if( pThread )
      sprintf( s,"Id: %d", pThread->th_no );
   else
      sprintf( s,"null" );
   leto_writelog( s, 0 );
}

int leto2_ThreadCondWait( HB_COND_T * cond, HB_CRITICAL_T * mutex, HB_BOOL * bLock )
{
   HB_BOOL rc;

   hb_threadEnterCriticalSection( mutex );
   *bLock = 1;

   rc = hb_threadCondWait( cond, mutex );

   *bLock = 0;
   hb_threadLeaveCriticalSection( mutex );

   return (rc)? 1 : -1;
}

BOOL leto2_ThreadCondUnlock( HB_COND_T * cond, HB_CRITICAL_T * mutex, HB_BOOL * bLock )
{
   HB_BOOL bRes = FALSE;

   hb_threadEnterCriticalSection( mutex );
   if( *bLock )
   {
      bRes = hb_threadCondBroadcast( cond );
      *bLock = 0;
   }
   hb_threadLeaveCriticalSection( mutex );
   return bRes;
}

void leto_errInternal( ULONG ulIntCode, const char * szText, const char * szPar1, const char * szPar2 )
{
   FILE * hLog;
   PAREASTRU pAStru = NULL;
   PTABLESTRU pTStru;
   USHORT ui;
   int i;

   if( pUStru_Curr && ulAreaID_Curr )
      pAStru = leto_FindArea( pUStru_Curr, ulAreaID_Curr );

   hLog = hb_fopen( "letodb_crash.log", "a+" );

   if( hLog )
   {
      char szTime[ 9 ];
      int iYear, iMonth, iDay;
      
      hb_dateToday( &iYear, &iMonth, &iDay );
      hb_dateTimeStr( szTime );
         
      fprintf( hLog, HB_I_("Breakdown at: %04d.%02d.%02d %s\n"), iYear, iMonth, iDay, szTime );
      fprintf( hLog, "Unrecoverable error %lu: ", ulIntCode );
      if( szText )
         fprintf( hLog, "%s %s %s\n", szText, szPar1, szPar2 );
      fprintf( hLog, "------------------------------------------------------------------------\n");
      if( pUStru_Curr )
      {
         fprintf( hLog, "User: %s %s %s\n", pUStru_Curr->szAddr, pUStru_Curr->szNetname, pUStru_Curr->szExename );
         fprintf( hLog, "Command: %s\n", pUStru_Curr->pBufRead );
      }
      if( pAStru )
         fprintf( hLog, "Table: %s\n", pAStru->pTStru->szTable );

      fclose( hLog );
   }

   pTStru = s_tables;
   ui = 0;
   i = -1;
   if( pTStru )
   {
      while( ui < uiTablesAlloc )
      {
         if( !pAStru || pAStru->pTStru != pTStru )
         {
            if( pTStru->szTable )
               leto_CloseTable( ui );
         }
         else if( pAStru )
            i = (int) ui;
         ui ++;
         pTStru ++;
      }
      if( i >= 0 )
      {
         if( pAStru->pTStru->szTable )
            leto_CloseTable( (USHORT)i );
      }      
      hb_xfree( s_tables );
      s_tables = NULL;
   }
   bThread1 = bThread2 = 0;
   leto_CloseMemArea();

}

static void leto_timewait( double dLastAct )
{  
   static int iCurIndex = 0;
   static long ulCurSec = 0;
   long ulDay = leto_Date();

   if( ulDay != ulToday )
   {
      ulToday = ulDay;
      dMaxDayWait = 0;
   }

   if( dLastAct )
   {
      double dCurrSec = hb_dateSeconds(), dSec;
      long ulSec = ( (long)dCurrSec ) / 10;

      if( ( dSec = ( dCurrSec - dLastAct ) ) > dMaxWait )
         dMaxWait = dSec;

      if( dSec > dMaxDayWait )
         dMaxDayWait = dSec;

      if( ulSec != ulCurSec )
      {
         ulCurSec = ulSec;
         if( ++iCurIndex >= 6 )
            iCurIndex = 0;
         dSumWait[iCurIndex]  = 0;
         uiSumWait[iCurIndex] = 0;
      }
      dSumWait[iCurIndex] += dSec;
      uiSumWait[iCurIndex] ++;
   }
}

void leto_SendAnswer( PUSERSTRU pUStru, const char* szData, ULONG ulLen )
{

   if( (((int)*szData) & 0xFF) >= 10 && *(szData+ulLen-1) != '\n' )
   {
      if( pUStru->ulBufferLen < ulLen+2 )
      {
         pUStru->ulBufferLen = ulLen + 2;
         pUStru->pBuffer = (BYTE*) hb_xrealloc( pUStru->pBuffer,pUStru->ulBufferLen );
      }
      memcpy( pUStru->pBuffer, szData, ulLen );
      *(pUStru->pBuffer+ulLen) = '\r';
      *(pUStru->pBuffer+ulLen+1) = '\n';
      *(pUStru->pBuffer+ulLen+2) = '\0';
      ulLen += 2;
      hb_ipSend( pUStru->hSocket, (char*)pUStru->pBuffer, ulLen, -1 );
   }
   else
      hb_ipSend( pUStru->hSocket, szData, ulLen, -1 );
   // leto_writelog(pUStru->pBuffer, 0);
   ulBytesSent += ulLen;

}

static void leto_runFunc( PUSERSTRU pUStru, PHB_DYNS* ppSym, const char* szName, const char* pCommand, ULONG ulLen )
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

static void ParseCommand( PUSERSTRU pUStru )
{
   const char * ptr;
   ULONG ulLen;

   static PHB_DYNS pSym_Fsetflt = NULL;
   static PHB_DYNS pSym_Fdboi   = NULL;
   static PHB_DYNS pSym_Fdbri   = NULL;
   static PHB_DYNS pSym_Fopent  = NULL;
   static PHB_DYNS pSym_Fopeni  = NULL;
   static PHB_DYNS pSym_Fcreat  = NULL;
   static PHB_DYNS pSym_Fcreai  = NULL;
   static PHB_DYNS pSym_Fsum    = NULL;
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
                  leto_runFunc( pUStru, &pSym_Fsetflt, "HS_SETFILTER", ptr+5, ulLen-5 );
               break;
            case 'c':
               if( !strncmp( ptr,"scop;",5 ) )
                  leto_Scope(pUStru, ptr+5);
               break;
            case 'u':
               if( !strncmp( ptr,"sum;",4 ) )
                  leto_runFunc( pUStru, &pSym_Fsum, "HS_SUM", ptr+4, ulLen-4 );
               break;
         }
         break;
      case 'g':
         // goto
         if( !strncmp( ptr,"goto;",5 ) )
            leto_Goto( pUStru, ptr+5 );
         break;
      case 'u':
         // upd, unlock
         if( !strncmp( ptr,"upd;",4 ) )
            leto_UpdateRec( pUStru, ptr+4, FALSE );
         else if( !strncmp( ptr,"unlock;",7 ) )
            leto_Unlock( pUStru, ptr+7 );
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

static HB_THREAD_STARTFUNC ( thread2 )
{
   USHORT ui;
   int iRes;
   PUSERSTRU pUStru;
   static double dSec4GC = 0;
   double dSec;
   static PHB_DYNS pSym_ISet = NULL;

   HB_SYMBOL_UNUSED( Cargo );
   // char s[10];
   hb_vmThreadInit( NULL );
   leto_runFunc( NULL, &pSym_ISet, "HS_INITSET", NULL, 0 );
   leto_writelog( "thread2 - Start", 0 );
   leto_ThreadIdLog();

   while( bThread2 )
   {
      // leto_writelog( "thread2 - 1", 0 );
      if( ( iRes = leto2_ThreadCondWait( &cond_thread2, &mutex_thread2_2, &bLock_thread2 )  ) == 1 )
      {
         // leto_writelog( "thread2 - 1A", 0 );
         for( ui=0,pUStru=s_users; ui<uiUsersMax; ui++,pUStru++ )
         {
            if( pUStru->pAStru && pUStru->pBufRead )
            {
               // memcpy( s,pUStru->pBufRead,5 );
               // s[5] = '\0';
               // leto_ThreadMutexLock( &mutex_thread2_1 );
               pUStru_Curr = pUStru;
               leto_timewait( pUStru->dLastAct );
               ParseCommand( pUStru );
               hb_threadEnterCriticalSection( &mutex_thread2_2 );
               pUStru->pBufRead = NULL;
               hb_threadLeaveCriticalSection( &mutex_thread2_2 );

               // leto_ThreadMutexUnlock( &mutex_thread2_1 );
            }
         }
         if( ( dSec = hb_dateSeconds() ) - dSec4GC > 300 )
         {
            dSec4GC = dSec;
            #if ( defined( __XHARBOUR__ ) ) || ( defined( HARBOUR_VER_AFTER_101 ) )
            hb_gcCollectAll( FALSE );
            #else
            hb_gcCollectAll();
            #endif
         }
         // leto_metka( 1, "T2 ", s );
      }
      else
      {
         char s[40];
         sprintf(s,"thread2-2 %d",iRes );
         leto_writelog( s, 0 );
      }
   }
   hb_vmThreadQuit();
   HB_THREAD_END

}


void leto_ScanUS( void )
{
   PUSERSTRU pUStru;
   USHORT ui, uiLenLen;
   ULONG ulLen;
   int iLen;
   BOOL bRes;

   for( ui=0,pUStru=s_users; ui<uiUsersMax; ui++,pUStru++ )
   {
      if( pUStru->pAStru && hb_ip_rfd_isset( pUStru->hSocket ) )
      {
         hb_threadEnterCriticalSection( &mutex_thread2_2 );
         bRes = ( pUStru->pBufRead == NULL );
         hb_threadLeaveCriticalSection( &mutex_thread2_2 );

         if( !bRes )
         {
            // leto_metka( 1, "SL1", NULL );
            // leto_ThreadMutexLock( &mutex_thread2_1 );
            // leto_ThreadMutexUnlock( &mutex_thread2_1 );
            #if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
            Sleep(0);
            #else
            sleep(0);
            #endif
            // leto_metka( 1, "SL2", NULL );
            continue;
         }
         if( pUStru->ulBufferLen - pUStru->ulStartPos < HB_SENDRECV_BUFFER_SIZE )
         {
            pUStru->ulBufferLen += HB_SENDRECV_BUFFER_SIZE;
            pUStru->pBuffer = (BYTE*) hb_xrealloc( pUStru->pBuffer,pUStru->ulBufferLen );
         }
         ulLen = 0;
         iLen = hb_ipRecv( pUStru->hSocket, (char*)pUStru->pBuffer + pUStru->ulStartPos,
                     HB_SENDRECV_BUFFER_SIZE );
         if( iLen > 0 )
         {
            /* {
               char s[16];
               sprintf( s,"%d %c%c%c%c%c%c%c",iLen,*pUStru->pBuffer,*(pUStru->pBuffer+1),*(pUStru->pBuffer+2),*(pUStru->pBuffer+3),*(pUStru->pBuffer+4),*(pUStru->pBuffer+5),*(pUStru->pBuffer+6));
               leto_metka( 1, "REA", s );
            } */
            pUStru->ulStartPos += iLen;
            ulBytesRead += iLen;

            if( ( uiLenLen = (((int)*pUStru->pBuffer) & 0xFF) ) < 10 &&
                       pUStru->ulStartPos > (ULONG)uiLenLen )
               ulLen = leto_b2n( (char *)(pUStru->pBuffer+1), uiLenLen );
            if( ( ulLen > 0 && ulLen+uiLenLen+1 <= pUStru->ulStartPos ) ||
                  ( uiLenLen >= 10 &&
                  *(pUStru->pBuffer+pUStru->ulStartPos-1) == '\n' &&
                  *(pUStru->pBuffer+pUStru->ulStartPos-2) == '\r' ) )
            {
               char * ptr;

               ulOperations ++;
               if( ( uiLenLen = (((int)*pUStru->pBuffer) & 0xFF) ) < 10 )
               {
                  ptr = (char*) pUStru->pBuffer + uiLenLen + 1;
                  ulLen = pUStru->ulStartPos - uiLenLen - 1;
               }
               else
               {
                  ptr = (char*) pUStru->pBuffer;
                  ulLen = pUStru->ulStartPos - 2;
               }
               *(ptr+ulLen) = '\0';
               pUStru->dLastAct = hb_dateSeconds();
               if( *ptr == 'i' && !strncmp( ptr,"intro;",6 ) )
                  leto_Intro( pUStru, ptr+6 );
               else if( *ptr == 'm' && !strncmp( ptr,"mgmt;",5 ) )
                  leto_Mgmt( pUStru, ptr+5 );
               else if( *ptr == 'v' && !strncmp( ptr,"var;",4 ) )
                  leto_Variables( pUStru, ptr+4 );
               else if( *ptr == 's' && !strncmp( ptr,"set;",4 ) )
                  leto_Set( pUStru, ptr+4 );
               else if( *ptr == 'a' && !strncmp( ptr,"admin;",6 ) )
                  leto_Admin( pUStru, ptr+6 );
               else
               {
                  hb_threadEnterCriticalSection( &mutex_thread2_2 );
                  pUStru->ulDataLen = ulLen;
                  pUStru->pBufRead = (BYTE*)ptr;
                  bUnlockRes = leto2_ThreadCondUnlock( &cond_thread2, &mutex_thread2_2, &bLock_thread2 );
                  hb_threadLeaveCriticalSection( &mutex_thread2_2 );
               }
               pUStru->ulStartPos = 0;
            }
         }
         else if( hb_iperrorcode() )
         {
            // Socket error while reading
            leto_CloseUS( pUStru );
         }
      }
   }
}

HB_FUNC( LETO_SERVER )
{
   char szBuffer[32];
   HB_SOCKET_T incoming;
   long int lTemp;

   static double dSec4MA = 0;
   double dSec;

   hb_ip_rfd_zero();

   // leto_metka( 0, NULL, NULL );
   iServerPort = hb_parni(1);
   if( ( hSocketMain = hb_ipServer( iServerPort, NULL, 10 ) ) == ( HB_SOCKET_T ) -1 )
      return;
   hb_ip_rfd_set( hSocketMain );

   s_thread = (LETO_THREAD*) malloc( sizeof(LETO_THREAD)*3 );
   memset( s_thread, 0, sizeof(LETO_THREAD)*3 );
   leto_ThreadIdLog();

   s_thread[0].hThread = hb_threadCreate( &(s_thread[0].id), thread2, (void*) (s_thread[0].szData) );

   while( bThread1 )
   {
      if( hb_ip_rfd_select( 1 ) > 0 )
      {
         if( hb_ip_rfd_isset( hSocketMain ) )
         {
            incoming = hb_ipAccept( hSocketMain, -1, szBuffer, &lTemp );
            ulOperations ++;
            if( incoming != -1 && !hb_iperrorcode() )
            {
               PUSERSTRU pUStru = leto_InitUS( incoming );

               hb_ip_rfd_set( incoming );

               leto_timewait( 0 );
               lTemp = strlen( szBuffer );
               pUStru->szAddr = (BYTE*) hb_xgrab( lTemp + 1 );
               memcpy( pUStru->szAddr, szBuffer, lTemp );
               pUStru->szAddr[lTemp] = '\0';
               lTemp = strlen(szRelease);
               memcpy( szBuffer, szRelease, lTemp );
               szBuffer[lTemp++] = ';';
               szBuffer[lTemp++] = (bCryptTraf)? 'Y':'N';
               if( bPass4L || bPass4M || bPass4D )
               {                
                  sprintf( szBuffer+lTemp,"%d;", (int) (ulOperations%100) );
                  pUStru->szDopcode[0] = *( szBuffer+lTemp );
                  pUStru->szDopcode[1] = *( szBuffer+lTemp+1 );
               }
               else
               {
                  szBuffer[lTemp++] = ';';
                  szBuffer[lTemp] = '\0';
               }
               leto_SendAnswer( pUStru, szBuffer, strlen(szBuffer) );
            }
         }
         leto_ScanUS();
      }
      if( !bUnlockRes )
      {
         bUnlockRes = leto2_ThreadCondUnlock( &cond_thread2, &mutex_thread2_2, &bLock_thread2 );
      }

#ifdef __CONSOLE__
      if( hb_inkey( FALSE,0,hb_setGetEventMask() ) == 27 )
         break;
#endif
      leto_ReadMemArea( szBuffer, 0, 1 );
      if( *szBuffer == '0' )
         break;
      if( ( dSec = hb_numInt( hb_dateSeconds() ) ) - dSec4MA > 1 )
      {
         dSec4MA = dSec;
         sprintf( szBuffer, "%06lu%03d", (ULONG)dSec, uiUsersCurr );
         leto_WriteMemArea( szBuffer, 1, 9 );
      }
   }
   bThread2 = 0;
#if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
   Sleep(0);
#else
   sleep(0);
#endif
   leto2_ThreadCondUnlock( &cond_thread2, &mutex_thread2_2, &bLock_thread2 );
   free( s_thread );
}
