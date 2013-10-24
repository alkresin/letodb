/*  $Id: leto_lnx.c,v 1.9 2009/07/02 10:40:58 alkresin Exp $  */

/*
 * Harbour Project source code:
 * Leto db server (linux) functions
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
#include "sys/shm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "srvleto.h"

typedef struct
{
   int hMem;
   char * lpView;
   BOOL bNewArea;
} MEMAREA;

static MEMAREA s_MemArea = { -1,0,0 };
                                 
HB_FUNC( LETO_CREATEMEMAREA )
{
   key_t key;

   key = ftok( "/var/run", hb_parni(1) % 256 );

   if( ( s_MemArea.hMem = shmget( key, 512, IPC_CREAT|IPC_EXCL|0660 ) ) == -1 )
   {
      if( ( s_MemArea.hMem = shmget(key, 512, 0) ) == -1 )
      {
         hb_retni( -1 );
         return;
      }
      s_MemArea.bNewArea = FALSE;
   }
   else
      s_MemArea.bNewArea = TRUE;

   if( ( s_MemArea.lpView = (char*) shmat( s_MemArea.hMem, 0, 0 ) ) == (char *) -1 )
   {
      shmctl( s_MemArea.hMem, IPC_RMID, 0 );
      hb_retni( -1 );
      return;
   }
   if( s_MemArea.bNewArea )
      ((char*)s_MemArea.lpView)[0] = 'L';

   hb_retni( s_MemArea.bNewArea );
}

void leto_CloseMemArea( void )
{
   if( s_MemArea.lpView )
   {
      shmdt( s_MemArea.lpView );
   }
   if( s_MemArea.hMem != -1 && s_MemArea.bNewArea )
   {
      shmctl( s_MemArea.hMem, IPC_RMID, 0 );
   }
}

HB_FUNC( LETO_CLOSEMEMAREA )
{
   leto_CloseMemArea();
}

HB_FUNC( LETO_SETAREANEW )
{
   s_MemArea.bNewArea = TRUE;
}

BOOL leto_ReadMemArea( char * szBuffer, int iAddr, int iLength )
{
   if( !s_MemArea.lpView )
      return FALSE;

   memcpy( szBuffer, ((char*)s_MemArea.lpView)+iAddr, iLength );
   szBuffer[iLength] = '\0';
   return TRUE;
}

BOOL leto_WriteMemArea( const char * szBuffer, int iAddr, int iLength )
{
   if( !s_MemArea.lpView )
      return FALSE;

   memcpy( ((char*)s_MemArea.lpView)+iAddr, szBuffer, iLength );
   return TRUE;
}

HB_FUNC( LETO_DAEMON )
{
   int	iPID, stdioFD, i, numFiles;

   iPID = fork();

   switch( iPID )
   {
      case 0:             /* we are the child process */
         break;

      case -1:            /* error - bail out (fork failing is very bad) */
         fprintf(stderr,"Error: initial fork failed: %s\n", strerror(errno));
         hb_retl( FALSE );
         return;
         break;

      default:            /* we are the parent, so exit */
         hb_vmRequestQuit();
         break;
   }

   if( setsid() < 0 )
   {
      hb_retl( FALSE );
      return;
   }

   numFiles = sysconf( _SC_OPEN_MAX ); /* how many file descriptors? */		
   for( i=numFiles-1; i>=0; --i )      /* close all open files except lock */
   {
      close(i);
   }
	
   /* stdin/out/err to /dev/null */
   umask( 0 );                             /* set this to whatever is appropriate for you */
   stdioFD = open( "/dev/null",O_RDWR );   /* fd 0 = stdin */
   dup( stdioFD );                         /* fd 1 = stdout */
   dup( stdioFD );                         /* fd 2 = stderr */

   setpgrp();
   hb_retl( TRUE );
   return;

}

BOOL leto_ThreadCreate( void* (*ThreadFunc)(void*) )
{
   pthread_t threadID;

   if( !pthread_create( &threadID, NULL, ThreadFunc, (void *) NULL ) )
   {
      return TRUE;
   }
   else
      return FALSE;
}

BOOL leto_ThreadMutexInit( LETO_MUTEX * pMutex  )
{
   if( ( pthread_mutex_init( pMutex, NULL) ) == 0 )
      return TRUE;
   else
      return FALSE;
}

void leto_ThreadMutexDestroy( LETO_MUTEX * pMutex )
{
   pthread_mutex_destroy( pMutex );
}

BOOL leto_ThreadMutexLock( LETO_MUTEX * pMutex )
{
   return ( pthread_mutex_lock( pMutex ) == 0 );
}

BOOL leto_ThreadMutexUnlock( LETO_MUTEX * pMutex )
{
   return ( pthread_mutex_unlock( pMutex ) == 0 );
}

BOOL leto_ThreadCondInit( LETO_COND * pCond  )
{
   if( pthread_mutex_init ( &(pCond->mutex), NULL ) ||
         pthread_cond_init ( &(pCond->cond), NULL ) )
      return FALSE;
   else
      return TRUE;
}

void leto_ThreadCondDestroy( LETO_COND * pCond )
{
   pthread_cond_destroy( &(pCond->cond) );
   pthread_mutex_destroy( &(pCond->mutex) );
}

int leto_ThreadCondWait( LETO_COND * pCond, int iMilliseconds )
{
   int rc;

   pthread_mutex_lock( &(pCond->mutex) );
   pCond->bLocked = 1;
   if( iMilliseconds<0 )
      rc = pthread_cond_wait( &(pCond->cond), &(pCond->mutex) );
   else
   {
      struct timespec ntime;

      ntime.tv_sec  = 0;
      ntime.tv_nsec = iMilliseconds * 1000000;
      rc = pthread_cond_timedwait( &(pCond->cond), &(pCond->mutex), &ntime );
   }
   pCond->bLocked = 0;
   pthread_mutex_unlock( &(pCond->mutex) );

   return (rc==0)? 1 : ( (rc==ETIMEDOUT)? 0 : -1 );
}

BOOL leto_ThreadCondUnlock( LETO_COND * pCond )
{
   BOOL bRes = FALSE;

   pthread_mutex_lock( &(pCond->mutex) );
   if( pCond->bLocked )
   {
      if( !pthread_cond_broadcast( &(pCond->cond) ) )
         bRes = TRUE;
      pCond->bLocked = 0;
   }
   pthread_mutex_unlock( &(pCond->mutex) );
   return bRes;
}
