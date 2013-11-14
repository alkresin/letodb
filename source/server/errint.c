/*
 * $Id: errint.c,v 1.6 2010/06/02 07:08:44 aokhotnikov Exp $
 */

/*
 * Harbour Project source code:
 * The Error API (internal error)
 *
 * Copyright 1999-2004 Viktor Szakats <viktor.szakats@syenar.hu>
 * www - http://www.harbour-project.org
 *
 * Copyright 2009 Alexander S. Kresin <alex / at / belacy.belgorod.su>
 * updated for Leto db server
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
#include "hbapierr.h"
#include "hbapilng.h"
#include "hbdate.h"
#include "hbset.h"
#if defined (__XHARBOUR__) || !defined(__HARBOUR__) || ( (__HARBOUR__ - 0) < 0x020000 )
#define LETO_ERRCODE ULONG
#else
#define LETO_ERRCODE HB_ERRCODE
#endif

static HB_BOOL   bErrHandlerRun = 0;
extern void leto_errInternal( HB_ULONG ulIntCode, const char * szText, const char * szPar1, const char * szPar2 );

void hb_errInternalRaw( LETO_ERRCODE ulIntCode, const char * szText, const char * szPar1, const char * szPar2 )
{

   if( szPar1 == NULL )
      szPar1 = "";

   if( szPar2 == NULL )
      szPar2 = "";

   if( !bErrHandlerRun )
   {
      bErrHandlerRun = 1;
      leto_errInternal( (HB_ULONG) ulIntCode, szText, szPar1, szPar2 );
   }
   else
   {
      FILE * hLog;
      hLog = hb_fopen( "letodb_crash.log", "a+" );

      if( hLog )
      {
         fprintf( hLog, "Unrecoverable error %lu: ", (HB_ULONG) ulIntCode );
         if( szText )
            fprintf( hLog, "%s %s %s\n", szText, szPar1, szPar2 );
         fclose( hLog );
      }
   }

}

void hb_errInternal( LETO_ERRCODE ulIntCode, const char * szText, const char * szPar1, const char * szPar2 )
{
   hb_errInternalRaw( ulIntCode, szText, szPar1, szPar2 );

   /* release console settings */
   hb_conRelease();

   if( hb_cmdargCheck( "ERRGPF" ) )
   {
       int *pGPF = NULL;
       *pGPF = 0;
       *(--pGPF) = 0;
   }

   exit( EXIT_FAILURE );
}
