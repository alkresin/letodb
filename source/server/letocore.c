/*  $Id: letocore.c,v 1.4 2010/07/09 10:42:27 aokhotnikov Exp $  */

/*
 * Harbour Project source code:
 * Leto db server core
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

#ifndef HB_OS_PATH_DELIM_CHR
   #define HB_OS_PATH_DELIM_CHR OS_PATH_DELIMITER
#endif

#if !defined (SUCCESS)
#define SUCCESS            0
#define FAILURE            1
#endif

static const char * szRelease = "Leto DB Server v.0.95";

static int iServerPort;
static HB_SOCKET_T hSocketMain;          // Initial server socket

static BOOL   bThread1 = 1;
static BOOL   bThread2 = 1;
LETO_COND     cond_thread2;
static BOOL   bUnlockRes = 1;
LETO_MUTEX    mutex_thread2_2;

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
extern void leto_Mgmt( PUSERSTRU pUStru, const char* szData );
extern void leto_CloseTable( USHORT nTableStru );
extern PUSERSTRU leto_InitUS( HB_SOCKET_T hSocket );
extern void leto_CloseUS( PUSERSTRU pUStru );
extern void leto_Set( PUSERSTRU pUStru, const char* szData );
extern void leto_Variables( PUSERSTRU pUStru, char* szData );
extern PAREASTRU leto_FindArea( PUSERSTRU pUStru, ULONG ulAreaID );
extern void leto_CloseMemArea( void );
extern void ParseCommand( PUSERSTRU pUStru );

void leto_metka( int iMode, char * s1, char * s2 )
{
#define CARR_LEN   400
   static char       carr1[CARR_LEN][30];
   static USHORT     uiarr1len = 0;
   static LETO_MUTEX mutex_carr;

   switch( iMode )
   {
      case 0:
         leto_ThreadMutexInit( &mutex_carr );
         break;
      case 1:
         leto_ThreadMutexLock( &mutex_carr );
         if( uiarr1len < CARR_LEN )
            sprintf( carr1[uiarr1len++],"%s %lu %s", s1, leto_MilliSec(),(s2)? s2 : "" );
         leto_ThreadMutexUnlock( &mutex_carr );
         break;
      case 2:
      {
         USHORT i;
         leto_ThreadMutexDestroy( &mutex_carr );
         for( i=0; i<uiarr1len; i++ )
            leto_writelog( NULL, carr1[i] );
         break;
      }
   }
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
      if( pUStru->ulBufferLen < ulLen+3 )
      {
         pUStru->ulBufferLen = ulLen + 3;
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

LETO_THREAD_FUNC thread2( void * Cargo )
{
   USHORT ui;
   int iRes;
   PUSERSTRU pUStru;
   static double dSec4GC = 0;
   double dSec;

   HB_SYMBOL_UNUSED( Cargo );
   // char s[10];
   // leto_writelog( "thread2 - Ok", 0 );

   while( bThread2 )
   {
      if( ( iRes = leto_ThreadCondWait( &cond_thread2, -1 )  ) == 1 )
      {
         // leto_metka( 1, "T1 ", NULL );
         // s[0] = '\0';         
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
               leto_ThreadMutexLock( &mutex_thread2_2 );
               // hb_ip_rfd_set( pUStru->hSocket );
               pUStru->pBufRead = NULL;
               leto_ThreadMutexUnlock( &mutex_thread2_2 );
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
         leto_writelog( NULL, "thread2-2 %d",iRes );
   }
   return 0;
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
         leto_ThreadMutexLock( &mutex_thread2_2 );
         bRes = ( pUStru->pBufRead == NULL );
         leto_ThreadMutexUnlock( &mutex_thread2_2 );
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
                  leto_ThreadMutexLock( &mutex_thread2_2 );
                  // hb_ip_rfd_clr( pUStru->hSocket );
                  pUStru->ulDataLen = ulLen;
                  pUStru->pBufRead = (BYTE*)ptr;
                  bUnlockRes = leto_ThreadCondUnlock( &cond_thread2 );
                  leto_ThreadMutexUnlock( &mutex_thread2_2 );
                  // leto_metka( 1, "U1 ", (bUnlockRes)? "1":"0" );
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

   leto_ThreadCondInit( &cond_thread2 );
   // leto_ThreadMutexInit( &mutex_thread2_1 );
   leto_ThreadMutexInit( &mutex_thread2_2 );
   leto_ThreadCreate( thread2 );

   while( bThread1 )
   {
      /* if( uiUsersCurr > 1 )
         leto_metka( 1, "00 ", NULL ); */
      if( hb_ip_rfd_select( 1 ) > 0 )
      {
         // leto_metka( 1, "SE ", NULL );
         if( hb_ip_rfd_isset( hSocketMain ) )
         {
            // leto_metka( 1, "NEW", NULL );
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
         bUnlockRes = leto_ThreadCondUnlock( &cond_thread2 );
         // leto_metka( 1, "U2 ", (bUnlockRes)? "1":"0" );
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

   leto_ThreadMutexDestroy( &mutex_thread2_2 );
   // leto_ThreadMutexDestroy( &mutex_thread2_1 );
   leto_ThreadCondDestroy( &cond_thread2 );
   // leto_metka( 2, NULL, NULL );
}
