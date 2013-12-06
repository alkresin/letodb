/*  $Id: funcleto.h,v 1.28.2.19 2013/12/06 09:42:17 alkresin Exp $  */

/*
 * Harbour Project source code:
 * Header file for Leto RDD and Server
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

#if defined( __XHARBOUR__ )
   #include "hbverbld.h"
   #if defined(HB_VER_CVSID) && ( HB_VER_CVSID >= 6575 )
      #define HARBOUR_VER_AFTER_101
   #endif
#elif defined( __HARBOUR__ )
   #if __HARBOUR__ - 0 >= 0x010100
      #define HARBOUR_VER_AFTER_101
   #elif __HARBOUR__ - 0 < 0x000100
      #define HARBOUR_VER_BEFORE_100
   #endif
   #if __HARBOUR__ - 0 <= 0x020000
      typedef HB_U32 HB_FATTR;
   #endif
#endif

#if ( defined( HB_OS_WIN_32 )  || defined( HB_OS_WIN ) ) && !defined( WIN32 )
   #define WIN32
#endif
#if !defined( unix ) && ( defined( __LINUX__ ) || defined( HB_OS_LINUX ) )
   #define unix
#endif

#if defined( __WATCOMC__ ) || defined( __LCC__ )
   #define _declspec( dllexport ) __declspec( dllexport )
#endif

#define HB_SENDRECV_BUFFER_SIZE         16384
#if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
   #define HB_SOCKET_T SOCKET
   #ifndef HB_SOCKET_H_
      #include <winsock2.h>
   #endif
#else
   #define HB_SOCKET_T int
#endif

#define HB_LETO_VERSION_STRING   "2.12"

#define LETO_MAX_USERNAME      16
#define LETO_PASSWORD "hE8Q,jy5+R"
#define LETO_MAX_KEYLENGTH     15

#if defined( __XHARBOUR__ )
   #if ! defined(HB_VER_CVSID) || ( HB_VER_CVSID < 6606 )
      #define __OLDRDD__
   #endif
#elif ( __HARBOUR__ - 0 < 0x020000 )
   #define __OLDRDD__
#endif

#define LETO_MSGSIZE_LEN      4

#if !( defined( HB_FT_STRING ) )
#define HB_FT_STRING          HB_IT_STRING
#define HB_FT_LOGICAL         HB_IT_LOGICAL
#define HB_FT_DATE            HB_IT_DATE
#define HB_FT_LONG            HB_IT_LONG
#define HB_FT_INTEGER         HB_IT_INTEGER
#define HB_FT_DOUBLE          HB_IT_DOUBLE
#define HB_FT_MEMO            HB_IT_MEMO
#define HB_FT_PICTURE         18    /* "P" */
#define HB_FT_BLOB            19    /* "W" */
#define HB_FT_OLE             20    /* "G" */
#define HB_FT_ANY             HB_IT_ANY
#define HB_FT_FLOAT           5     /* "F" */
#define HB_FT_CURRENCY        13    /* "Y" */
#endif
#if !( defined( HB_FT_DATETIME ) )
#define HB_FT_DATETIME        8     /* "T" */
#endif
#if !( defined( HB_FT_DAYTIME ) )
#define HB_FT_DAYTIME         9     /* "@" */
#define HB_FT_MODTIME         10    /* "=" */
#endif
#if !( defined( HB_FT_PICTURE ) )
#define HB_FT_PICTURE         18    /* "P" */
#endif

#if !defined( __XHARBOUR__ )
   #if !defined( HARBOUR_VER_BEFORE_100 ) 
      #if (__HARBOUR__ >= 0x010100)
         #define hb_cdp_page hb_vmCDP()
         #if ( __HARBOUR__ - 0 > 0x020000 )
            typedef HB_ERRCODE ERRCODE;
         #endif
      #endif
   #endif
#else
   #define hb_cdp_page hb_cdppage()
   #if ! defined(HB_VER_CVSID) || ( HB_VER_CVSID >= 6380 )
      typedef HB_ERRCODE ERRCODE;
   #endif
#endif
#if !defined( HB_PFS )
   #define HB_PFS "l"
#endif

#if defined( HB_IO_WIN )
   typedef HB_PTRDIFF LETO_FHANDLE;
#else
   typedef int LETO_FHANDLE;
#endif

#if ! defined( _POSIX_PATH_MAX )
  #define _POSIX_PATH_MAX HB_PATH_MAX
#endif

#if defined( __XHARBOUR__ ) || ( __HARBOUR__ - 0 < 0x020000 )
   #if !defined( __XHARBOUR__ )
      #define HB_LONG         LONG
      #define HB_ULONG        ULONG
   #endif

   typedef unsigned char   HB_BYTE;
   typedef int             HB_BOOL;
   typedef unsigned short  HB_USHORT;
   typedef ULONG           HB_SIZE;
#elif !defined( FALSE )
   #define FALSE           HB_FALSE
   #define TRUE            HB_TRUE
   #define BOOL            HB_BOOL
   #define BYTE            HB_BYTE
   #define SHORT           HB_SHORT
   #define USHORT          HB_USHORT
   #define LONG            HB_LONG
   #define ULONG           HB_ULONG
   #define UINT            HB_UINT
   #define UINT32          HB_U32
   #define UINT64          HB_U64
#endif

#if !defined( HB_FALSE )
   #define HB_FALSE      0
#endif
#if !defined( HB_TRUE )
   #define HB_TRUE       (!0)
#endif
#if !defined( HB_ISNIL )
   #define HB_ISNIL( n )         ISNIL( n )
   #define HB_ISCHAR( n )        ISCHAR( n )
   #define HB_ISNUM( n )         ISNUM( n )
   #define HB_ISLOG( n )         ISLOG( n )
   #define HB_ISDATE( n )        ISDATE( n )
   #define HB_ISMEMO( n )        ISMEMO( n )
   #define HB_ISBYREF( n )       ISBYREF( n )
   #define HB_ISARRAY( n )       ISARRAY( n )
   #define HB_ISOBJECT( n )      ISOBJECT( n )
   #define HB_ISBLOCK( n )       ISBLOCK( n )
   #define HB_ISPOINTER( n )     ISPOINTER( n )
   #define HB_ISHASH( n )        ISHASH( n )
   #define HB_ISSYMBOL( n )      ISSYMBOL( n )
#endif

#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
   #define HB_MAXINT    HB_LONG
#endif

#if !defined( HB_ISFIRSTIDCHAR )
   #define HB_ISDIGIT( c )         ( ( c ) >= '0' && ( c ) <= '9' )
   #define HB_ISFIRSTIDCHAR( c )   ( c >= 'A' && c <= 'Z' ) || c == '_' || ( c >= 'a' && c <= 'z' )
   #define HB_ISNEXTIDCHAR( c )    ( HB_ISFIRSTIDCHAR(c) || HB_ISDIGIT( c ) )
#endif

#define LETO_FLG_BOF          1
#define LETO_FLG_EOF          2
#define LETO_FLG_LOCKED       4
#define LETO_FLG_DEL          8
#define LETO_FLG_FOUND       16

extern int hb_ipDataReady( HB_SOCKET_T hSocket, int timeout );
extern int hb_ipRecv( HB_SOCKET_T hSocket, char *Buffer, int iMaxLen );
extern int hb_ipSend( HB_SOCKET_T hSocket, const char *Buffer, int iSend, int timeout );
extern HB_SOCKET_T hb_ipConnect( const char * szHost, int iPort, int timeout );
extern HB_SOCKET_T hb_ipServer( int iPort, const char * szAddress, int iListen );
extern HB_SOCKET_T hb_ipAccept( HB_SOCKET_T hSocket, int timeout, char * szAddr, long int * lPort );
extern void hb_ipInit( void );
extern void hb_ipCleanup( void );
extern int hb_iperrorcode( void );
extern int hb_ipclose( HB_SOCKET_T hSocket );
extern void hb_ip_rfd_zero( void);
extern void hb_ip_rfd_clr( HB_SOCKET_T hSocket );
extern void hb_ip_rfd_set( HB_SOCKET_T hSocket );
extern BOOL hb_ip_rfd_isset( HB_SOCKET_T hSocket );
extern int hb_ip_rfd_select( int iTimeOut );
extern void hb_getLocalIP( HB_SOCKET_T hSocket, char * szIP );
extern int leto_n2b( char *s, unsigned long int n );
extern int leto_n2cb( char *s, unsigned long int n, int iLenLen );
extern long int leto_b2n( const char *s, int iLenLen );
extern char * leto_AddLen( char * pData, ULONG * uiLen, BOOL lBefore );
extern int leto_BagCheck( const char * szTable, const char * szBagName );
extern void leto_writelog( const char * sFile, int n, const char * s, ... );
extern void leto_encrypt( const char *ptri, unsigned long int ulLen, char *ptro, unsigned long int *pLen, const char *key );
extern void leto_decrypt( const char *ptri, unsigned long int ulLen, char *ptro, unsigned long int *pLen, const char *key );
extern void leto_byte2hexchar( const char * ptri, int iLen, char * ptro );
extern void leto_hexchar2byte( const char * ptri, int iLen, char * ptro );
