/*  $Id: net.c,v 1.4.2.2 2012/03/01 16:04:58 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * leto_NetName() function
 *
 * Copyright 1999-2001 Viktor Szakats <viktor.szakats@syenar.hu>
 * www - http://www.harbour-project.org
 *
 * Copyright 2008 Alexander S. Kresin <alex / at / belacy.belgorod.su>
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

/*
 * The following parts are Copyright of the individual authors.
 * www - http://www.harbour-project.org
 *
 * Copyright 2001 Luiz Rafael Culik <culik@sl.conex.net>
 *    Support for DJGPP/GCC/OS2 for netname
 *
 * See doc/license.txt for licensing terms.
 *
 */

#define HB_OS_WIN_32_USED
#define HB_OS_WIN_USED

#include "hbapi.h"
#include "funcleto.h"

#if defined(HB_OS_OS2) && defined(__GNUC__)

   #include "hb_io.h"

   /* 25/03/2004 - <maurilio.longo@libero.it>
      not needed anymore as of GCC 3.2.2 */

   #include <pwd.h>
   #include <sys/types.h>

   #if defined(__EMX__) && __GNUC__ * 1000 + __GNUC_MINOR__ < 3002
      #include <emx/syscalls.h>
      #define gethostname __gethostname
   #endif

   #define MAXGETHOSTNAME 256      /* should be enough for a host name */

#elif defined(HB_OS_DOS)

   #if defined(__DJGPP__) || defined(__RSX32__) || defined(__GNUC__)
      #include "hb_io.h"
      #include <sys/param.h>
   #endif

#elif defined(HB_OS_UNIX)

   #if !defined(__WATCOMC__)
      #include <pwd.h>
      #include <sys/types.h>
   #endif
   #include <unistd.h>
   #define MAXGETHOSTNAME 256      /* should be enough for a host name */

#elif defined(HB_OS_WIN_32) || defined( HB_OS_WIN )

   #include <windows.h>

#endif

char * leto_NetName( void )
{
#if defined(HB_OS_UNIX) || ( defined(HB_OS_OS2) && defined(__GNUC__) )

#  if defined(__WATCOMC__)
   return hb_getenv( "HOSTNAME" );
#  else
   char szValue[MAXGETHOSTNAME + 1], *szRet;
   USHORT uiLen;

   szValue[ 0 ] = '\0';
   gethostname( szValue, MAXGETHOSTNAME );

   uiLen = strlen(szValue);
   szRet = (char*) hb_xgrab( uiLen+1 );
   memcpy( szRet, szValue, uiLen );
   szRet[uiLen] = '\0';
   return szRet;
#  endif

#elif defined(HB_OS_DOS)

#  if defined(__DJGPP__) || defined(__RSX32__) || defined(__GNUC__)
   char szValue[MAXGETHOSTNAME + 1], *szRet;
   USHORT uiLen;

   szValue[ 0 ] = '\0';
   gethostname( szValue, MAXGETHOSTNAME );

   uiLen = strlen(szValue);
   szRet = (char*) hb_xgrab( uiLen+1 );
   memcpy( szRet, szValue, uiLen );
   szRet[uiLen] = '\0';
   return szRet;
#  endif

#elif defined(HB_OS_WIN_32) || defined( HB_OS_WIN )

   DWORD ulLen = MAX_COMPUTERNAME_LENGTH+1;
   char szValue[MAX_COMPUTERNAME_LENGTH+1], *szRet;

   szValue[ 0 ] = '\0';
   GetComputerName( szValue, &ulLen );

   ulLen = strlen( szValue );
   szRet = (char*) hb_xgrab( ulLen+1 );
   memcpy( szRet, szValue, ulLen );
   szRet[ulLen] = '\0';
   return szRet;

#else

   return NULL;

#endif
}
