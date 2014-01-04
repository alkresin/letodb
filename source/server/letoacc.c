/*  $Id: letoacc.c,v 1.8 2010/08/13 16:43:27 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Leto db server functions
 *
 * Copyright 2009 Alexander S. Kresin <alex / at / belacy.belgorod.su>
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
#include "hbapifs.h"
#ifdef __XHARBOUR__
   #include "hbfast.h"
#else
   #include "hbapicls.h"
#endif
#include "hbapierr.h"
#include "math.h"
#include "srvleto.h"

#if defined( HB_IO_WIN )
   typedef HB_PTRDIFF LETO_FHANDLE;
#else
   typedef int LETO_FHANDLE;
#endif

/* EOL:  DOS - \r\n  Unix - \n,  \r = 13, \n = 10 */

typedef struct
{
   USHORT      uiUslen;
   USHORT      uiPasslen;
   char        szUser[16];
   char        szPass[16];
   char        szAccess[2];
} ACCSTRU, *PACCSTRU;

static PACCSTRU s_acc = NULL;

static USHORT uiAccAlloc  = 0;         // Number of allocated account structures
static USHORT uiAccCurr   = 0;         // Current number of accounts
static BOOL   bAccUpdated = 0;

extern BOOL   bPass4L;
extern BOOL   bPass4M;
extern BOOL   bPass4D;
extern char * pAccPath;

extern USHORT uiAnswerSent;

extern const char * szOk;
extern const char * szErr2;
extern const char * szErr3;
extern const char * szErr4;
extern const char * szErr101;
extern const char * szErrAcc;

extern int leto_GetParam( const char *szData, const char **pp2, const char **pp3, const char **pp4, const char **pp5 );
extern void leto_SendAnswer( PUSERSTRU pUStru, const char* szData, ULONG ulLen );

#define ACC_REALLOC       20

#define HB_SKIPTABSPACES( sptr ) while( *sptr && ( *sptr == ' ' || *sptr == '\t' || \
         *sptr == '\r' || *sptr == '\n' ) ) ( sptr )++

#define HB_SKIPCHARS( sptr ) while( *sptr && *sptr != ' ' && *sptr != '\t' && \
         *sptr != '=' && *sptr != ',' && *sptr != '\r' && *sptr != '\n' ) ( sptr )++

#define HB_SKIPEOL( sptr ) while( *sptr && ( *sptr == '\r' || *sptr == '\n' ) ) ( sptr )++

#define HB_SKIPTILLEOL( sptr ) while( *sptr && *sptr != '\n' ) ( sptr )++

char * leto_memoread( const char * szFilename, ULONG *pulLen )
{
   LETO_FHANDLE fhnd;
   char * pBuffer = NULL;

   fhnd = hb_fsOpen( (char*) szFilename, FO_READ | FO_SHARED );
   if( fhnd != FS_ERROR )
   {
      *pulLen = hb_fsSeek( fhnd, 0, FS_END );
      if( *pulLen != 0 )
      {
         pBuffer = ( char * ) hb_xgrab( *pulLen + 1 );

         hb_fsSeek( fhnd, 0, FS_SET );
         hb_fsReadLarge( fhnd, pBuffer, *pulLen );
         pBuffer[*pulLen] = '\0';
      }
      hb_fsClose( fhnd );
   }
   else
      *pulLen = 0;
   return pBuffer;
}

void leto_acc_read( const char * szFilename )
{
   ULONG ulLen;
   char * pBuffer = leto_memoread( szFilename, &ulLen );

   if( pBuffer )
   {
      const char * ptr, * ptrEnd;
      int  iUsers = 0;
      char szBuf[32], szPass[32];

      ptr = pBuffer;

      while( *ptr )
      {
         if(  *ptr >= '?' )
            iUsers ++;
         ptr = strchr( ptr, '\n' );
         HB_SKIPEOL( ptr );
      }

      uiAccAlloc = iUsers + ACC_REALLOC;
      s_acc = (ACCSTRU*) hb_xgrab( sizeof(ACCSTRU) * uiAccAlloc );
      memset( s_acc, 0, sizeof(ACCSTRU) * uiAccAlloc );

      ptr = pBuffer;
      iUsers = 0;

      while( *ptr )
      {
         if( *ptr < '?' )
         {
            ptr = strchr( ptr, '\n' );
         }
         else
         {
            ptrEnd = strchr( ptr, ';' );
            if( *ptrEnd && (ptrEnd-ptr) )
            {
               s_acc[iUsers].uiUslen = (USHORT) (ptrEnd-ptr);
               memcpy( s_acc[iUsers].szUser, ptr, (s_acc[iUsers].uiUslen>LETO_MAX_USERNAME)? LETO_MAX_USERNAME : s_acc[iUsers].uiUslen );

               ptr = ptrEnd + 1;
               ptrEnd = strchr( ptr, ';' );
               if( *ptrEnd )
               {
                  ulLen = (ULONG) (ptrEnd-ptr);
                  if( ulLen && ulLen <= 48 )
                  {
                     leto_hexchar2byte( ptr, ulLen, szBuf );
                     ulLen /= 2;
                     leto_decrypt( szBuf, ulLen, szPass, &ulLen, LETO_PASSWORD );
                     if( ulLen <= LETO_MAX_USERNAME )
                     {
                        memcpy( s_acc[iUsers].szPass, szPass, ulLen );
                        s_acc[iUsers].uiPasslen = (USHORT) ulLen;
                     }
                     else
                        s_acc[iUsers].uiUslen = 0;
                  }
                  if( s_acc[iUsers].uiUslen )
                  {
                     ptr = ptrEnd + 1;
                     ptrEnd = strchr( ptr, ';' );
                     if( *ptrEnd )
                     {
                        int i;
                        s_acc[iUsers].szAccess[0] = s_acc[iUsers].szAccess[1] = '\0';
                        for( i=0; i<8 && *ptr!=';'; i++, ptr++ )
                        {
                           if( *ptr == 'y' || *ptr == 'Y' )
                              s_acc[iUsers].szAccess[0] |= ( 1 << i );
                        }
                        iUsers ++;
                     }
                  }
               }
            }

            ptr = strchr( ptr, '\n' );
         }
         HB_SKIPEOL( ptr );
      }
      uiAccCurr = iUsers;

      hb_xfree( pBuffer );
   }
}

char * leto_acc_find( const char * szUser, const char * szPass )
{
   int i;
   USHORT uiLen = (USHORT) strlen( szUser );
   PACCSTRU pacc;

   if( s_acc )
      for( i = 0, pacc = s_acc; i < uiAccCurr; i++, pacc++ )
         if( uiLen == pacc->uiUslen && !strncmp( szUser, pacc->szUser, uiLen ) )
         {
            if( (uiLen = (USHORT)strlen(szPass) ) == pacc->uiPasslen && !strncmp( szPass, pacc->szPass, uiLen ) )
            {
               return pacc->szAccess;
            }
            else
            {
               return NULL;
            }
         }
   return NULL;
}

BOOL leto_acc_add( const char * szUser, const char * szPass, const char * szAccess )
{
   int i;
   USHORT uiLen = (USHORT)strlen( szUser );
   PACCSTRU pacc;

   if( s_acc )
      for( i = 0, pacc = s_acc; i < uiAccCurr; i++, pacc++ )
         if( uiLen == pacc->uiUslen && !strncmp( szUser, pacc->szUser, uiLen ) )
            return 0;
   if( uiLen <= LETO_MAX_USERNAME && strlen( szPass ) <= LETO_MAX_USERNAME )
   {
      if( !s_acc )
      {
         uiAccAlloc = ACC_REALLOC;
         s_acc = (ACCSTRU*) hb_xgrab( sizeof(ACCSTRU) * uiAccAlloc );
         memset( s_acc, 0, sizeof(ACCSTRU) * uiAccAlloc );
      }
      if( uiAccCurr == uiAccAlloc )
      {
         s_acc = (ACCSTRU*) hb_xrealloc( s_acc, sizeof(ACCSTRU) * ( uiAccAlloc+ACC_REALLOC ) );
         memset( s_acc+uiAccAlloc, 0, sizeof(ACCSTRU) * ACC_REALLOC );
         uiAccAlloc += ACC_REALLOC;
      }
      pacc = s_acc + uiAccCurr;

      pacc->uiUslen = uiLen;
      memcpy( pacc->szUser, szUser, uiLen );

      uiLen = (USHORT)strlen( szPass );
      pacc->uiPasslen = uiLen;
      memcpy( pacc->szPass, szPass, uiLen );

      if( szAccess && *szAccess )
      {
         for( i=0; i<8 && *szAccess; i++, szAccess++ )
         {
            if( *szAccess == 'y' || *szAccess == 'Y' || *szAccess == 't' || *szAccess == 'T' )
               pacc->szAccess[0] |= ( 1 << i );
         }
      }

      uiAccCurr ++;
      bAccUpdated = 1;
      return 1;

   }
   return 0;
}

BOOL leto_acc_setpass( const char * szUser, const char * szPass )
{
   int i;
   USHORT uiLen = (USHORT)strlen( szUser );
   PACCSTRU pacc;

   if( s_acc )
      for( i = 0, pacc = s_acc; i < uiAccCurr; i++, pacc++ )
         if( uiLen == pacc->uiUslen && !strncmp( szUser, pacc->szUser, uiLen ) )
         {
            if( ( uiLen = (USHORT)strlen( szPass ) ) <= LETO_MAX_USERNAME )
            {
               pacc->uiPasslen = uiLen;
               memcpy( pacc->szPass, szPass, uiLen );
               bAccUpdated = 1;
               return 1;
            }
            break;
         }

   return 0;
}

BOOL leto_acc_setacc( const char * szUser, const char * szAccess )
{
   int i;
   USHORT uiLen = (USHORT)strlen( szUser );
   PACCSTRU pacc;

   if( s_acc )
      for( i = 0, pacc = s_acc; i < uiAccCurr; i++, pacc++ )
         if( uiLen == pacc->uiUslen && !strncmp( szUser, pacc->szUser, uiLen ) )
         {
            if( szAccess && *szAccess )
            {
               pacc->szAccess[0] = '\0';
               for( i=0; i<8 && *szAccess; i++, szAccess++ )
               {
                  if( *szAccess == 'y' || *szAccess == 'Y' || *szAccess == 't' || *szAccess == 'T' )
                     pacc->szAccess[0] |= ( 1 << i );
               }
            }
            bAccUpdated = 1;
            return 1;
         }

   return 0;
}

void leto_acc_flush( const char * szFilename )
{
   LETO_FHANDLE fhnd;
   PACCSTRU pacc;
   int i, j;
   char szBuf[96], *ptr;
   char szPass[54];
   ULONG ulLen;

   if( s_acc && bAccUpdated )
   {
      fhnd = hb_fsCreate( (char*)szFilename, 0 );
      if( fhnd != FS_ERROR )
      {
         for( i = 0, pacc = s_acc; i < uiAccCurr; i++, pacc++ )
         {
            if( pacc->uiPasslen )
            {
               leto_encrypt( pacc->szPass, pacc->uiPasslen, szBuf, &ulLen, LETO_PASSWORD );
               leto_byte2hexchar( szBuf, (int)ulLen, szPass );
               ulLen *= 2;
            }
            memcpy( szBuf, pacc->szUser, pacc->uiUslen );
            ptr = szBuf + pacc->uiUslen;
            *ptr++ = ';';
            if( pacc->uiPasslen )
            {
               memcpy( ptr, szPass, ulLen );
               ptr += ulLen;
            }
            *ptr++ = ';';
            for( j=0; j<8; j++ )
               *ptr++ = ( pacc->szAccess[0] & ( 1 << j ) )? 'Y' : 'N';
            *ptr++ = ';';
            *ptr++ = '\r';
            *ptr++ = '\n';

            hb_fsWrite( fhnd, szBuf, (USHORT) (ptr - szBuf) );
         }
         hb_fsClose( fhnd );
      }
   }
}

void leto_acc_release( void )
{
   if( s_acc )
      hb_xfree( s_acc );
   s_acc = NULL;
}

void leto_Admin( PUSERSTRU pUStru, char* szData )
{
   const char * pData;
   char * pp1, * pp2, * pp3, * ptr;
   int nParam = leto_GetParam( szData, (const char**) &pp1, (const char**) &pp2, (const char**) &pp3, (const char**) &ptr );
   ULONG ulLen;
   char szBuf[32];

   if( ( !bPass4L && !bPass4M && !bPass4D ) || ( pUStru->szAccess[0] & 1 ) )
   {
      if( !nParam )
         pData = szErr2;
      else
      {
         if( !strncmp( szData,"uadd;",5 ) || !strncmp( szData,"upsw;",5 ) )
         {
            if( nParam < 3 )
               pData = szErr4;
            else
            {
               char szPass[24];
               char szKey[LETO_MAX_KEYLENGTH+1];
               USHORT uiKeyLen = strlen(LETO_PASSWORD);

               if( uiKeyLen > LETO_MAX_KEYLENGTH )
                  uiKeyLen = LETO_MAX_KEYLENGTH;

               memcpy( szKey, LETO_PASSWORD, uiKeyLen );
               szKey[uiKeyLen] = '\0';
               if( pUStru->szDopcode[0] )
               {
                  szKey[0] = pUStru->szDopcode[0];
                  szKey[1] = pUStru->szDopcode[1];
               }
               if( ( ulLen = (pp3-pp2-1) ) <= 48 )
               {
                  leto_hexchar2byte( pp2, pp3-pp2-1, szBuf );
                  ulLen = (pp3-pp2-1) / 2;
                  leto_decrypt( szBuf, ulLen, szPass, &ulLen, szKey );
                  szPass[ulLen] = '\0';
                  *(pp2-1) = '\0';
                  if( *(szData+1) == 'a' )
                  {
                     *(ptr-1) = '\0';
                     if( leto_acc_add( pp1, szPass, pp3 ) )
                        pData = szOk;
                     else
                        pData = szErr4;
                  }
                  else
                  {
                     if( leto_acc_setpass( pp1, szPass ) )
                        pData = szOk;
                     else
                        pData = szErr4;
                  }             
               }
               else
                  pData = szErr4;
            }
         }
         else if( !strncmp( szData,"uacc;",5 ) )
         {
            if( nParam < 3 )
               pData = szErr4;
            else
            {
               *(pp2-1) = '\0';
               *(pp3-1) = '\0';
               if( leto_acc_setacc( pp1, pp2 ) )
                  pData = szOk;
               else
                  pData = szErr4;
            }
         }
         else if( !strncmp( szData,"flush;",6 ) )
         {
            leto_acc_flush( pAccPath );
            pData = szOk;
         }
         else
            pData = szErr3;
      }
   }
   else
      pData = szErrAcc;
   leto_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;
}
