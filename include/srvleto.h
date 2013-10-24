/*  $Id: srvleto.h,v 1.9 2010/06/26 09:20:07 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Header file for Leto DB Server
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

#define HB_EXTERNAL_RDDDBF_USE
#include "hbrdddbf.h"
// #include "hbapirdd.h"
#include "funcleto.h"
#include "rddleto.ch"

#if defined( HB_OS_WIN_32 ) || defined( HB_OS_WIN )
   #include "windows.h"
   #include <process.h>

   #define  LETO_MUTEX   CRITICAL_SECTION

   typedef struct _LETO_COND_
   {
     HANDLE   hEvent;
     LONG     ulLocked;
   } LETO_COND;

   #define LETO_THREAD_FUNC  unsigned __stdcall

   extern BOOL leto_ThreadCreate( unsigned int (__stdcall *ThreadFunc)(void*) );
#else
   #define _MULTI_THREADED
   #include <pthread.h>

   #define  LETO_MUTEX   pthread_mutex_t

   typedef struct _LETO_COND_
   {
     pthread_cond_t  cond;
     pthread_mutex_t mutex;
     BOOL     bLocked;
   } LETO_COND;

   #define LETO_THREAD_FUNC  void *
   extern BOOL leto_ThreadCreate( void* (*ThreadFunc)(void*) );
#endif

typedef struct _VAR_LINK
{
   USHORT      uiGroup;
   USHORT      uiVar;
} VAR_LINK;

typedef struct _DATABASE
{
   char *      szPath;
   USHORT      uiDriver;
   struct _DATABASE * pNext;
} DATABASE;

typedef struct
{
   char *      BagName;
   char *      szFullName;
   USHORT      uiAreas;
   BOOL        bCompound;
} INDEXSTRU, *PINDEXSTRU;

typedef struct
{
   USHORT      uiAreaNum;
   USHORT      uiDriver;
   USHORT      uiAreas;
   BOOL        bShared;
   BOOL        bLocked;
   BYTE *      szTable;
   ULONG *     pLocksPos;              /* List of records locked */
   ULONG       ulLocksMax;             /* Number of records locked */
   ULONG       ulLocksAlloc;           /* Number of records locked (allocated) */
   PINDEXSTRU  pIStru;
   USHORT      uiIndexAlloc;
} TABLESTRU, *PTABLESTRU;

typedef struct _LETOTAG
{
   PINDEXSTRU  pIStru;
   char        szTagName[12];
   PHB_ITEM    pTopScope;
   PHB_ITEM    pBottomScope;
   struct _LETOTAG * pNext;
} LETOTAG;

typedef struct
{
   ULONG       ulAreaNum;
   PTABLESTRU  pTStru;
   BOOL        bLocked;
   ULONG *     pLocksPos;              /* List of records locked */
   ULONG       ulLocksMax;             /* Number of records locked */
   ULONG       ulLocksAlloc;           /* Number of records locked (allocated) */
   USHORT      uiSkipBuf;
   LETOTAG *   pTag;
   PHB_ITEM    itmFltExpr;
   char        szAlias[HB_RDD_MAX_ALIAS_LEN + 1];
} AREASTRU, *PAREASTRU;

typedef struct
{
   HB_SOCKET_T hSocket;
   BYTE *      pBuffer;
   ULONG       ulBufferLen;
   ULONG       ulStartPos;
   BYTE *      pBufRead;
   ULONG       ulDataLen;
   BYTE *      szVersion;
   BYTE *      szAddr;
   BYTE *      szNetname;
   BYTE *      szExename;
   PHB_CODEPAGE cdpage;
   char *      szDateFormat;
   BOOL        bCentury;
   char        szAccess[2];
   char        szDopcode[2];
   double      dLastAct;
   PAREASTRU   pAStru;
   USHORT      uiAreasAlloc;
   VAR_LINK *  pVarLink;
   USHORT      uiVarsOwnCurr;
} USERSTRU, *PUSERSTRU;

typedef struct
{
   DBFAREAP    pArea;
   ULONG       ulRecNo;
   BOOL        bAppend;
   USHORT      uiFlag;
   USHORT      uiItems;
   USHORT *    puiIndex;
   PHB_ITEM *  pItems;
} TRANSACTSTRU;

extern BOOL leto_ReadMemArea( char * szBuffer, int iAddr, int iLength );
extern BOOL leto_WriteMemArea( const char * szBuffer, int iAddr, int iLength );
extern BOOL leto_ThreadMutexInit( LETO_MUTEX * pMutex );
extern void leto_ThreadMutexDestroy( LETO_MUTEX * pMutex );
extern BOOL leto_ThreadMutexLock( LETO_MUTEX * pMutex );
extern BOOL leto_ThreadMutexUnlock( LETO_MUTEX * pMutex );
extern BOOL leto_ThreadCondInit( LETO_COND * phEvent  );
extern void leto_ThreadCondDestroy( LETO_COND * phEvent );
extern int leto_ThreadCondWait( LETO_COND * phEvent, int iMilliseconds );
extern BOOL leto_ThreadCondUnlock( LETO_COND * pCond );
