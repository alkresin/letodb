/* $Id: letocl.h,v 1.1.2.16 2014/01/09 18:36:26 ptsarenko Exp $ */

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
#define LETO_MAX_TAGNAME      10     /* Max len of tag name */
#define LETO_MAX_EXP         256     /* Max len of KEY/FOR expression */
#define LETO_MAX_KEY         256     /* Max len of key */

/* Field types from hbapirdd.h */

#define HB_FT_NONE            0
#define HB_FT_STRING          1     /* "C" */
#define HB_FT_LOGICAL         2     /* "L" */
#define HB_FT_DATE            3     /* "D" */
#define HB_FT_LONG            4     /* "N" */
#define HB_FT_FLOAT           5     /* "F" */
#define HB_FT_INTEGER         6     /* "I" */
#define HB_FT_DOUBLE          7     /* "B" */
#define HB_FT_TIME            8     /* "T" */
#if !( defined( HB_FT_DATETIME ) )
#define HB_FT_DATETIME        8
#endif
#define HB_FT_TIMESTAMP       9     /* "@" */
#if !( defined( HB_FT_DAYTIME ) )
#define HB_FT_DAYTIME         9
#endif
#if !( defined( HB_FT_MODTIME ) )
#define HB_FT_MODTIME         10    /* "=" */
#endif
#define HB_FT_ROWVER          11    /* "^" */
#define HB_FT_AUTOINC         12    /* "+" */
#define HB_FT_CURRENCY        13    /* "Y" */
#define HB_FT_CURDOUBLE       14    /* "Z" */
#define HB_FT_VARLENGTH       15    /* "Q" */
#define HB_FT_MEMO            16    /* "M" */
#define HB_FT_ANY             17    /* "V" */
#define HB_FT_IMAGE           18    /* "P" */
#if !( defined( HB_FT_PICTURE ) )
   #define HB_FT_PICTURE         18
#endif
#define HB_FT_BLOB            19    /* "W" */
#define HB_FT_OLE             20    /* "G" */

#define LETO_INDEX_UNIQ       1
#define LETO_INDEX_ALL        2
#define LETO_INDEX_REST       4
#define LETO_INDEX_DESC       8
#define LETO_INDEX_CUST      16
#define LETO_INDEX_ADD       32
#define LETO_INDEX_TEMP      64
#define LETO_INDEX_FILT     128

HB_EXTERN_BEGIN

typedef struct _CDPSTRU
{
   char *      szClientCdp;
   char *      szServerCdp;
   struct _CDPSTRU * pNext;
} CDPSTRU, *PCDPSTRU;

typedef struct _LETOBUFFER_
{
   unsigned char * pBuffer;          /* Buffer for records */
   unsigned long ulBufLen;         /* allocated buffer length */
   unsigned long ulBufDataLen;     /* data length in buffer */
   unsigned int  bSetDeleted;      /* current _SET_DELETED flag */
   unsigned long ulDeciSec;        /* buffer time in 1/100 seconds */
   unsigned int  uiShoots;         /* using statistic */
} LETOBUFFER;

typedef struct _LETOFIELD
{
   char       szName[12];
   unsigned int  uiType;
   unsigned int  uiLen;
   unsigned int  uiDec;
} LETOFIELD;

typedef struct _LETOTAGINFO
{
   char *      BagName;
   char *      TagName;
   char *      KeyExpr;
   char *      ForExpr;
   unsigned int UniqueKey;
   unsigned char KeyType;
   unsigned int KeySize;
   unsigned int uiTag;
   unsigned int nField;           /* Field number for simple (one field) key expersion */
   char *      pTopScopeAsString;
   char *      pBottomScopeAsString;
   int         iTopScopeAsString;
   int         iBottomScopeAsString;
   unsigned int UsrAscend;        /* user settable ascending/descending order flag */
   unsigned int Custom;           /* user settable custom order flag */

   LETOBUFFER  Buffer;           /* seek buffer */
   unsigned int uiBufSize;        /* records count in buffer */
   unsigned int uiRecInBuf;       /* current records in buffer*/

   void *      pExtra;

   struct _LETOTAGINFO * pNext;
} LETOTAGINFO;

typedef struct _LETOTABLE
{
   unsigned long hTable;
   unsigned int  uiDriver;
   unsigned int  uiConnection;

   unsigned int  uiFieldExtent;
   LETOFIELD *   pFields;
   unsigned int  uiUpdated;
   unsigned int * pFieldUpd;           /* Pointer to updated fields array */
   unsigned int * pFieldOffset;        /* Pointer to field offset array */

   unsigned int  uiOrders;
   char *        szTags;

   char     szMemoExt[HB_MAX_FILE_EXT + 1];    /* MEMO file extension */
   unsigned char bMemoType;           /* MEMO type used in DBF memo fields */
   unsigned int  uiMemoVersion;       /* MEMO file version */

   unsigned int  fShared;             /* Shared file */
   unsigned int  fReadonly;           /* Read only file */
   unsigned int  fBof;                /* HB_TRUE if "bof" */
   unsigned int  fEof;                /* HB_TRUE if "eof" */
   unsigned int  fFound;              /* HB_TRUE if "found" */
   unsigned int  fDeleted;            /* Deleted record */
   unsigned int  fRecLocked;          /* TRUE if record is locked */

   unsigned int  uiRecordLen;         /* Size of record */
   unsigned long ulRecNo;
   unsigned long ulRecCount;          /* Count of records */
   unsigned char * pRecord;           /* Buffer of record data */

   LETOBUFFER  Buffer;                /* skip buffer */
   unsigned char *  ptrBuf;
   unsigned int  uiRecInBuf;
   signed char BufDirection;

   unsigned int  uiSkipBuf;           /* skip buffer size */
   long          lLastUpdate;         /* from dbf header: last update */
   int           iBufRefreshTime;

   LETOTAGINFO * pTagInfo;

   unsigned long * pLocksPos;         /* List of records locked */
   unsigned long   ulLocksMax;        /* Number of records locked */
   unsigned long   ulLocksAlloc;      /* Number of records locked (allocated) */
   unsigned int    fFLocked;          /* TRUE if file is locked */

} LETOTABLE;

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
   HB_BOOL     bCrypt;
   HB_BOOL     bCloseAll;
   PCDPSTRU    pCdpTable;

   HB_BOOL     bTransActive;
   BYTE *      szTransBuffer;
   ULONG       ulTransBuffLen;
   ULONG       ulTransDataLen;
   ULONG       ulRecsInTrans;
   ULONG       ulTransBlockLen;

   HB_BOOL     bRefreshCount;

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
void LetoSetDeleted( unsigned int uiDeleted );
void LetoSetAutOpen( unsigned int uiAutOpen );
void LetoSetModName( char * szModule );
int LetoGetConnectRes( void );
int LetoGetCmdItem( char ** pptr, char * szDest );
LETOCONNECTION * LetoConnectionNew( const char * szAddr, int iPort, const char * szUser, const char * szPass, int iTimeOut );
int LetoCloseAll( LETOCONNECTION * pConnection );
void LetoConnectionClose( LETOCONNECTION * pConnection );
char * LetoGetServerVer( LETOCONNECTION * pConnection );
void LetoSetPath( LETOCONNECTION * pConnection, const char * szPath );
unsigned int LetoSetFastAppend( unsigned int uiFApp );

void LetoDbFreeTag( LETOTAGINFO * pTagInfo );
unsigned int LetoDbCloseTable( LETOTABLE * pTable );
LETOTABLE * LetoDbCreateTable( LETOCONNECTION * pConnection, char * szFile, char * szAlias, char * szFields, unsigned int uiArea );
LETOTABLE * LetoDbOpenTable( LETOCONNECTION * pConnection, char * szFile, char * szAlias, int iShared, int iReadOnly, char * szCdp, unsigned int uiArea );
unsigned int LetoDbBof( LETOTABLE * pTable );
unsigned int LetoDbEof( LETOTABLE * pTable );
unsigned int LetoDbGetField( LETOTABLE * pTable, unsigned int uiIndex, char * szRet, unsigned int * uiLen );
unsigned int LetoDbRecCount( LETOTABLE * pTable, unsigned long * ulCount );
unsigned int LetoDbRecNo( LETOTABLE * pTable, unsigned long * ulRecNo );
unsigned int LetoDbFieldCount( LETOTABLE * pTable, unsigned int * uiCount );
unsigned int LetoDbFieldName( LETOTABLE * pTable, unsigned int uiIndex, char * szName );
unsigned int LetoDbFieldType( LETOTABLE * pTable, unsigned int uiIndex, unsigned int * uiType );
unsigned int LetoDbFieldLen( LETOTABLE * pTable, unsigned int uiIndex, unsigned int * uiLen );
unsigned int LetoDbFieldDec( LETOTABLE * pTable, unsigned int uiIndex, unsigned int * uiDec );
void LetoDbGotoEof( LETOTABLE * pTable );
unsigned int LetoDbGoTo( LETOTABLE * pTable, unsigned long ulRecNo, char * szTag );
unsigned int LetoDbGoTop( LETOTABLE * pTable, char * szTag );
unsigned int LetoDbGoBottom( LETOTABLE * pTable, char * szTag );
unsigned int LetoDbSkip( LETOTABLE * pTable, long lToSkip, char * szTag );
unsigned int LetoDbPutRecord( LETOTABLE * pTable, unsigned int bCommit );
unsigned int LetoDbPutField( LETOTABLE * pTable, unsigned int uiIndex, char * szValue, unsigned int uiLen );
unsigned int LetoDbAppend( LETOTABLE * pTable, unsigned int fUnLockAll );
unsigned int LetoDbOrderCreate( LETOTABLE * pTable, char * szBagName, char * szTag, char * szKey, unsigned char bType, unsigned int uiFlags, char * szFor, char * szWhile, unsigned long ulNext );
unsigned int LetoDbSeek( LETOTABLE * pTable, char * szTag, char * szKey, unsigned int uiKeyLen, unsigned int bSoftSeek, unsigned int bFindLast );
unsigned int LetoDbClearFilter( LETOTABLE * pTable );
unsigned int LetoDbSetFilter( LETOTABLE * pTable, char * szFilter );
unsigned int LetoDbCommit( LETOTABLE * pTable );
unsigned int LetoDbIsRecLocked( LETOTABLE * pTable, unsigned long ulRecNo, unsigned int * uiRes );
unsigned int LetoDbRecLock( LETOTABLE * pTable, unsigned long ulRecNo );
unsigned int LetoDbRecUnLock( LETOTABLE * pTable, unsigned long ulRecNo );
unsigned int LetoDbFileLock( LETOTABLE * pTable );
unsigned int LetoDbFileUnLock( LETOTABLE * pTable );
unsigned int LetoDbPack( LETOTABLE * pTable );
unsigned int LetoDbZap( LETOTABLE * pTable );
unsigned int LetoDbReindex( LETOTABLE * pTable );

long int leto_RecvFirst( LETOCONNECTION * pConnection );
long int leto_Recv( LETOCONNECTION * pConnection );
long int leto_DataSendRecv( LETOCONNECTION * pConnection, const char * sData, unsigned long ulLen );
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

int LetoGetError( void );
void LetoSetError( int iErr );
int LetoVarSet( LETOCONNECTION * pConnection, char * szGroup, char * szVar, char cType, char * szValue, unsigned int uiFlags, char ** pRetValue );
char * LetoVarGet( LETOCONNECTION * pConnection, char * szGroup, char * szVar );
long LetoVarIncr( LETOCONNECTION * pConnection, char * szGroup, char * szVar, unsigned int uiFlags );
long LetoVarDecr( LETOCONNECTION * pConnection, char * szGroup, char * szVar, unsigned int uiFlags );
int LetoVarDel( LETOCONNECTION * pConnection, char * szGroup, char * szVar );
char * LetoVarGetList( LETOCONNECTION * pConnection, const char * szGroup, unsigned int uiMaxLen );

int LetoFileExist( LETOCONNECTION * pConnection, char * szFile );
int LetoFileErase( LETOCONNECTION * pConnection, char * szFile );
int LetoFileRename( LETOCONNECTION * pConnection, char * szFile, char * szFileNew );
char * LetoMemoRead( LETOCONNECTION * pConnection, char * szFile, unsigned long * ulMemoLen );
int LetoMemoWrite( LETOCONNECTION * pConnection, char * szFile, char * szValue, unsigned long ulLen );
char * LetoFileRead( LETOCONNECTION * pConnection, char * szFile, unsigned long ulStart, unsigned long * ulLen );
int LetoFileWrite( LETOCONNECTION * pConnection, char * szFile, char * szValue, unsigned long ulStart, unsigned long ulLen );
long int LetoFileSize( LETOCONNECTION * pConnection, char * szFile );
char * LetoDirectory( LETOCONNECTION * pConnection, char * szDir, char * szAttr );
int LetoDirMake( LETOCONNECTION * pConnection, char * szFile );

unsigned int leto_IsBinaryField( unsigned int uiType, unsigned int uiLen );
void leto_SetBlankRecord( LETOTABLE * pTable, unsigned int uiAppend );
int leto_ParseRecord( LETOTABLE * pTable, const char * szData, unsigned int uiCrypt );
void leto_AllocBuf( LETOBUFFER *pLetoBuf, unsigned long ulDataLen, ULONG ulAdd );
unsigned long leto_BufStore( LETOTABLE * pTable, char * pBuffer, const char * ptr, unsigned long ulDataLen );
unsigned int leto_HotBuffer( LETOTABLE * pTable, LETOBUFFER * pLetoBuf, unsigned int uiBufRefreshTime );
unsigned int leto_OutBuffer( LETOBUFFER * pLetoBuf, char * ptr );
unsigned long leto_BufRecNo( char * ptrBuf );
void leto_setSkipBuf( LETOTABLE * pTable, const char * ptr, unsigned long ulDataLen, unsigned int uiRecInBuf );
void leto_AddTransBuffer( LETOCONNECTION * pConnection, char * pData, ULONG ulLen );
void leto_SetUpdated( LETOTABLE * pTable, USHORT uiUpdated );
char * leto_ParseTagInfo( LETOTABLE * pTable, char * pBuffer );
char * leto_AddKeyToBuf( char * szData, char * szKey, unsigned int uiKeyLen, unsigned long *pulLen );
void leto_ClearSeekBuf( LETOTAGINFO * pTagInfo );
void leto_ClearBuffers( LETOTABLE * pTable );
unsigned int leto_IsRecLocked( LETOTABLE * pTable, unsigned long ulRecNo );
void leto_AddRecLock( LETOTABLE * pTable, unsigned long ulRecNo );
