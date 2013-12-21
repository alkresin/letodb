/* $Id: $ */

/*
 * Harbour Project source code:
 * Header file for Leto RDD
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

#include "hbdefs.h"

#if !defined (HB_SOCKET_T)
   #if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
      #define HB_SOCKET_T SOCKET
      #ifndef HB_SOCKET_H_
         #include <winsock2.h>
      #endif
   #else
      #define HB_SOCKET_T int
   #endif
#endif

#define HB_MAX_FILE_EXT       10

HB_EXTERN_BEGIN

typedef struct _CDPSTRU
{
   char *      szClientCdp;
   char *      szServerCdp;
   struct _CDPSTRU * pNext;
} CDPSTRU, *PCDPSTRU;

typedef struct _LETOTABLE
{
   unsigned long hTable;
   unsigned int  uiDriver;

   unsigned int  uiFieldExtent;
   char *        szFields;

   char     szMemoExt[HB_MAX_FILE_EXT + 1];    /* MEMO file extension */
   BYTE     bMemoType;           /* MEMO type used in DBF memo fields */
   USHORT   uiMemoVersion;       /* MEMO file version */

} LETOTABLE, *PLETOTABLE;

typedef struct _LETOCONNECTION_
{
   HB_SOCKET_T hSocket;
   char *      pAddr;
   int         iPort;
   int         iTimeOut;
   char *      szPath;
   char        szVersion[24];
   unsigned int uiMajorVer;
   unsigned int uiMinorVer;
   char        szVerHarbour[80];
   char        szAccess[8];
   char        cDopcode[2];
   HB_BOOL        bCrypt;
   HB_BOOL        bCloseAll;
   PCDPSTRU    pCdpTable;

   HB_BOOL        bTransActive;
   BYTE *      szTransBuffer;
   ULONG       ulTransBuffLen;
   ULONG       ulTransDataLen;
   ULONG       ulRecsInTrans;
   ULONG       ulTransBlockLen;

   HB_BOOL        bRefreshCount;

   char *      pBufCrypt;
   ULONG       ulBufCryptLen;

/* uiBufRefreshTime defines the time interval in 0.01 sec. After this 
   time is up, the records buffer must be refreshed, 100 by default */
   USHORT      uiBufRefreshTime;

   USHORT      uiDriver;
   USHORT      uiMemoType;

   USHORT      uiProto;
   USHORT      uiTBufOffset;

} LETOCONNECTION;

HB_EXTERN_END

char * leto_getRcvBuff( void );
char * leto_firstchar( void );
void leto_setCallback( void( *fn )( void ) );
void LetoInit( void );
void LetoExit( unsigned int uiFull );
void LetoSetDateFormat( const char * szDateFormat );
void LetoSetCentury( char cCentury );
void LetoSetCdp( const char * szCdp );
void LetoSetModName( char * szModule );
int LetoGetConnectRes( void );
int LetoGetCmdItem( char ** pptr, char * szDest );
LETOCONNECTION * LetoConnectionNew( const char * szAddr, int iPort, const char * szUser, const char * szPass, int iTimeOut );
int LetoCloseAll( LETOCONNECTION * pConnection );
void LetoConnectionClose( LETOCONNECTION * pConnection );
char * LetoGetServerVer( LETOCONNECTION * pConnection );
void LetoSetPath( LETOCONNECTION * pConnection, const char * szPath );

long int leto_RecvFirst( LETOCONNECTION * pConnection );
long int leto_Recv( LETOCONNECTION * pConnection );
long int leto_DataSendRecv( LETOCONNECTION * pConnection, const char * sData, ULONG ulLen );
LETOCONNECTION * leto_getConnectionPool( void );
LETOCONNECTION * leto_ConnectionFind( const char * szAddr, int iPort );
const char * leto_GetServerCdp( LETOCONNECTION * pConnection, const char *szCdp );
int LetoCheckServerVer( LETOCONNECTION * pConnection, USHORT uiVer );
const char * leto_RemoveIpFromPath(const char * szPath);
int leto_getIpFromPath( const char * sSource, char * szAddr, int * piPort, char * szPath, BOOL bFile );
void leto_getFileFromPath( const char * sSource, char * szFile );
char * leto_DecryptBuf( LETOCONNECTION * pConnection, const char * ptr, ULONG * pulLen );
char * leto_DecryptText( LETOCONNECTION * pConnection, ULONG * pulLen );

char * LetoMgGetInfo( LETOCONNECTION * pConnection );
char * LetoMgGetUsers( LETOCONNECTION * pConnection, const char * szTable );
char * LetoMgGetTables( LETOCONNECTION * pConnection, const char * szUser );
void LetoMgKillUser( LETOCONNECTION * pConnection, const char * szUserId );
char * LetoMgGetTime( LETOCONNECTION * pConnection );

int LetoGetFError( void );
void LetoSetFError( int iErr );
int LetoVarSet( LETOCONNECTION * pConnection, char * szGroup, char * szVar, char cType, char * szValue, unsigned int uiFlags, char ** pRetValue );
char * LetoVarGet( LETOCONNECTION * pConnection, char * szGroup, char * szVar );
long LetoVarIncr( LETOCONNECTION * pConnection, char * szGroup, char * szVar, unsigned int uiFlags );
long LetoVarDecr( LETOCONNECTION * pConnection, char * szGroup, char * szVar, unsigned int uiFlags );
int LetoVarDel( LETOCONNECTION * pConnection, char * szGroup, char * szVar );
char * LetoVarGetList( LETOCONNECTION * pConnection, const char * szGroup, unsigned int uiMaxLen );

int LetoIsFileExist( LETOCONNECTION * pConnection, char * szFile );
int LetoFileErase( LETOCONNECTION * pConnection, char * szFile );
int LetoFileRename( LETOCONNECTION * pConnection, char * szFile, char * szFileNew );
char * LetoMemoRead( LETOCONNECTION * pConnection, char * szFile );
int LetoMemoWrite( LETOCONNECTION * pConnection, char * szFile, char * szValue, unsigned long ulLen );
char * LetoFileRead( LETOCONNECTION * pConnection, char * szFile, unsigned long ulStart, unsigned long * ulLen );
int LetoFileWrite( LETOCONNECTION * pConnection, char * szFile, char * szValue, unsigned long ulStart, unsigned long ulLen );
long int LetoFileSize( LETOCONNECTION * pConnection, char * szFile );
char * LetoDirectory( LETOCONNECTION * pConnection, char * szDir, char * szAttr );
int LetoMakeDir( LETOCONNECTION * pConnection, char * szFile );
