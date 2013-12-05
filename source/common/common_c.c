/*  $Id: common_c.c,v 1.15 2010/06/02 07:08:44 aokhotnikov Exp $  */

/*
 * Harbour Project source code:
 * Leto db server functions
 *
 * Copyright 2008 Alexander S. Kresin <alex / at / belacy.belgorod.su>
 * www - http://www.harbour-project.org
 *
 * Copyright 2007 Przemyslaw Czerpak <druzus / at / priv.onet.pl>
 * www - http://www.harbour-project.org
 * hb_dateMilliSeconds() function
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
#include "hbdate.h"
#include "hbapierr.h"
#include "hbapifs.h"
#include "funcleto.h"

#include <time.h>

#if ( defined( HB_OS_BSD ) || defined( HB_OS_LINUX ) ) && !defined( __WATCOMC__ )
   #include <sys/time.h>
#elif !( defined( HB_WINCE ) && defined( _MSC_VER ) )
   #include <sys/timeb.h>
#endif
#if defined( OS_UNIX_COMPATIBLE )
   #include <sys/times.h>
   #include <unistd.h>
#endif
#if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
   #include <windows.h>
#elif defined(_MSC_VER)
   #define timeb _timeb
   #define ftime _ftime
#endif

static HB_ULONG hb_dateMilliSec( void )
{
#if defined(HB_OS_WIN_32) || defined( HB_OS_WIN )
   SYSTEMTIME st;

   HB_TRACE(HB_TR_DEBUG, ("hb_dateMilliSeconds()"));

   GetLocalTime( &st );

   return ( HB_ULONG ) hb_dateEncode( st.wYear, st.wMonth, st.wDay ) * 86400000L +
          ( ( st.wHour * 60 + st.wMinute ) * 60 + st.wSecond ) * 1000 +
          st.wMilliseconds;
#elif ( defined( HB_OS_LINUX ) || defined( HB_OS_BSD ) ) && !defined( __WATCOMC__ )
   struct timeval tv;

   HB_TRACE(HB_TR_DEBUG, ("hb_dateMilliSeconds()"));

   gettimeofday( &tv, NULL );

   return ( HB_ULONG ) tv.tv_sec * 1000 + tv.tv_usec / 1000;
#else
   struct timeb tb;

   HB_TRACE(HB_TR_DEBUG, ("hb_dateMilliSeconds()"));

   ftime( &tb );

   return ( HB_ULONG ) tb.time * 1000 + tb.millitm;
#endif
}

HB_FUNC( LETO_MILLISEC )
{
   hb_retnl( hb_dateMilliSec() );
}

long int leto_b2n( const char *s, int iLenLen )
{
   unsigned long int n = 0;
   int i = 0;

   do
   {
      n += (unsigned long int)((unsigned char)s[i] << (8*i));
      i ++;
   }
   while( i < iLenLen );
   return n;
}

int leto_n2b( char *s, unsigned long int n )
{
   int i = 0;

   do
   {
      s[i] = (char)(n & 0xff);
      n = n >> 8;
      i ++;
   }
   while( n );
   return i;
}

int leto_n2cb( char *s, unsigned long int n, int iLenLen )
{
   int i = 0;

   do
   {
      s[i] = (char)(n & 0xff);
      n = n >> 8;
      i ++;
      iLenLen --;
   }
   while( iLenLen );
   return i;
}

char * leto_AddLen( char * pData, ULONG * ulLen, BOOL lBefore )
{
   if( *ulLen < 256 )
   {
      if( lBefore )
      {
         *(--pData) = (char) *ulLen & 0xFF;
         *(--pData) = '\1';
      }
      else
      {
         *(pData++) = '\1';
         *(pData++) = (char) *ulLen & 0xFF;
      }
      *ulLen = *ulLen + 2;
   }
   else
   {
      USHORT uiLenLen;
      char sNumf[4];
      uiLenLen = leto_n2b( sNumf, *ulLen );
      if( lBefore )
      {
         pData -= uiLenLen;
         memcpy( pData, sNumf, uiLenLen );
         *(--pData) = (char) uiLenLen & 0xFF;
      }
      else
      {
         *(pData++) = (char) uiLenLen & 0xFF;
         memcpy( pData, sNumf, uiLenLen );
         pData += uiLenLen;
      }
      *ulLen = *ulLen + uiLenLen + 1;
   }
   return pData;

}

/*
 * Returns 0, if the index bagname and the table name are identical
 */
int leto_BagCheck( const char * szTable, const char * szBagName )
{
   const char * p1, * p2, * p3, * p4, * ptr;
   char c;

   p1 = ptr = szTable;
   p2 = p4 = NULL;
   while( ( c = *ptr) != 0 )
   {
      if( c == '/' || c == '\\' )
         p2 = ptr;
      else if( c == '.' )
         p2 = ptr;
      ptr ++;
   }
   if( !p2 || p2 < p1 )
      p2 = ptr;

   p3 = ptr = szBagName;
   while( ( c = *ptr) != 0 )
   {
      if( c == '/' || c == '\\' )
         p4 = ptr;
      else if( c == '.' )
         p4 = ptr;
      ptr ++;
   }
   if( !p4 || p4 < p3 )
      p4 = ptr;

   if( ( p4 - p3 ) != ( p2 - p1 ) || strncmp( p1, p3, p2-p1 ) )
      return 1;
   else
      return 0;

}

void leto_byte2hexchar( const char * ptri, int iLen, char * ptro )
{
   int i, n;

   for( i=0; i<iLen; i++,ptri++ )
   {
      n = ((unsigned char) *ptri) >> 4;
      *ptro++ = (char) ( n < 10 )? n+48 : n+55;
      n = ((unsigned char) *ptri) &0x0f;
      *ptro++ = (char) ( n < 10 )? n+48 : n+55;
   }
}

void leto_hexchar2byte( const char * ptri, int iLen, char * ptro )
{
   int i, n, nRes;

   iLen /= 2;
   for( i=0; i<iLen; i++ )
   {
      n = (unsigned char) *ptri++;
      if( n > 64 && n < 71 )
         nRes = ( n - 55 ) * 16;
      else if( n > 47 && n < 58 )
         nRes = ( n - 48 ) * 16;
      else
         nRes = 0;

      n = (unsigned char) *ptri++;
      if( n > 64 && n < 71 )
         nRes += n - 55;
      else if( n > 47 && n < 58 )
         nRes += n - 48;
      else
         nRes = 0;
      *ptro++ = (char)nRes;
   }
}
