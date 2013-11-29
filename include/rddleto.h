/*  $Id: rddleto.h,v 1.36 2010/07/07 17:49:16 alkresin Exp $  */

/*
 * Harbour Project source code:
 * Header file for Leto RDD
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

#include "hbapirdd.h"
#include "funcleto.h"
#include "rddleto.ch"

HB_EXTERN_BEGIN

typedef struct _CDPSTRU
{
   char *      szClientCdp;
   char *      szServerCdp;
   struct _CDPSTRU * pNext;
} CDPSTRU, *PCDPSTRU;

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
   BOOL        bCrypt;
   BOOL        bCloseAll;
   PCDPSTRU    pCdpTable;

   BOOL        bTransActive;
   BYTE *      szTransBuffer;
   ULONG       ulTransBuffLen;
   ULONG       ulTransDataLen;
   ULONG       ulRecsInTrans;
   ULONG       ulTransBlockLen;

   BOOL        bRefreshCount;

   char *      pBufCrypt;
   ULONG       ulBufCryptLen;

/* uiBufRefreshTime defines the time interval in 0.01 sec. After this 
   time is up, the records buffer must be refreshed, 100 by default */
   USHORT      uiBufRefreshTime;

   USHORT      uiDriver;
   USHORT      uiMemoType;

} LETOCONNECTION;

typedef struct _LETOBUFFER_
{
   BYTE *      pBuffer;          /* Buffer for records */
   ULONG       ulBufLen;         /* allocated buffer length */
   ULONG       ulBufDataLen;     /* data length in buffer */
   BOOL        bSetDeleted;      /* current _SET_DELETED flag */
   ULONG       ulDeciSec;        /* buffer time in 1/100 seconds */
   USHORT      uiShoots;         /* using statistic */
} LETOBUFFER;

typedef struct _LETOTAGINFO
{
   char *      BagName;
   char *      TagName;
   char *      KeyExpr;
   char *      ForExpr;
   PHB_ITEM    pKeyItem;
   BOOL        UniqueKey;
   BYTE        KeyType;
   USHORT      KeySize;
   USHORT      uiTag;
   USHORT      nField;           /* Field number for simple (one field) key expersion */
   PHB_ITEM    pTopScope;
   PHB_ITEM    pBottomScope;
   char *      pTopScopeAsString;
   char *      pBottomScopeAsString;
   int         iTopScopeAsString;
   int         iBottomScopeAsString;
   BOOL        UsrAscend;        /* user settable ascending/descending order flag */
   BOOL        Custom;           /* user settable custom order flag */

   LETOBUFFER  Buffer;           /* seek buffer */
   USHORT      uiBufSize;        /* records count in buffer */
   USHORT      uiRecInBuf;       /* current records in buffer*/

   USHORT      uiFCount;         /* index fields count */
   USHORT *    puiFields;        /* index fields array */

   struct _LETOTAGINFO * pNext;
} LETOTAGINFO;


/*
 *  LETO WORKAREA
 *  --------
 *  The Workarea Structure of Harbour remote Database Server RDD
 *
 */

typedef struct _LETOAREA_
{
   AREA     area;

   /*
    *  LETO's additions to the workarea structure
    *
    *  Warning: The above section MUST match WORKAREA exactly!  Any
    *  additions to the structure MUST be added below, as in this
    *  example.
    */

   LPDBRELINFO lpdbPendingRel;   /* Pointer to parent rel struct */

   char *   szDataFileName;      /* Name of data file */
   USHORT   uiRecordLen;         /* Size of record */
   ULONG    ulRecNo;             /* Current record */
   BYTE *   pRecord;             /* Buffer of record data */

   BYTE *   pBuffer;             /* Buffer for records */
   BYTE *   ptrBuf;
   ULONG    ulBufLen;
   ULONG    ulBufDataLen;
   USHORT   uiRecInBuf;
   ULONG    ulDeciSec;
   signed char BufDirection;

   USHORT   uiUpdated;
   USHORT * pFieldUpd;           /* Pointer to updated fields array */

   USHORT * pFieldOffset;        /* Pointer to field offset array */

   BOOL     fDeleted;            /* Deleted record */
   BOOL     fRecLocked;          /* TRUE if record is locked */

   BOOL     fShared;             /* Shared file */
   BOOL     fReadonly;           /* Read only file */
   BOOL     fFLocked;            /* TRUE if file is locked */

   USHORT   uiConnection;        /* connection number */
   ULONG    hTable;              /* ID of a table, gotten from the server */
   USHORT   uiDriver;            /* 0 id DBFCDX, 1 if DBFNTX */
   LETOTAGINFO * pTagInfo;
   USHORT   iOrders;             /* number of orders */
   LETOTAGINFO * pTagCurrent;    /* current order */

   ULONG *     pLocksPos;              /* List of records locked */
   ULONG       ulLocksMax;             /* Number of records locked */
   ULONG       ulLocksAlloc;           /* Number of records locked (allocated) */

   char     szMemoExt[HB_MAX_FILE_EXT + 1];    /* MEMO file extension */
   BYTE     bMemoType;           /* MEMO type used in DBF memo fields */
   USHORT   uiMemoVersion;       /* MEMO file version */

   USHORT   uiSkipBuf;           /* skip buffer size */

   LONG     lLastUpdate;         /* from dbf header: last update */
   SHORT    iBufRefreshTime;

} LETOAREA;

typedef LETOAREA * LETOAREAP;

HB_EXTERN_END

#if defined( HARBOUR_VER_BEFORE_100 ) 
typedef USHORT ( * DBENTRYP_RVVL )( struct _RDDNODE * pRDD, PHB_ITEM p1, PHB_ITEM p2, ULONG p3 );
#endif

#define LETO_MAX_KEY        256     /* Max len of key */
#define LETO_MAX_EXP        256     /* Max len of KEY/FOR expression */
#define LETO_MAX_TAGNAME     10     /* Max len of tag name */

/* BUFF_REFRESH_TIME defines the time interval in 0.01 sec. After this 
   time is up, the records buffer must be refreshed */
#define BUFF_REFRESH_TIME   100

#if ! defined( HB_RDD_MAX_DRIVERNAME_LEN ) && defined( HARBOUR_MAX_RDD_DRIVERNAME_LENGTH )
   #define  HB_RDD_MAX_DRIVERNAME_LEN HARBOUR_MAX_RDD_DRIVERNAME_LENGTH
   #define  HB_RDD_MAX_ALIAS_LEN HARBOUR_MAX_RDD_ALIAS_LENGTH
#endif
