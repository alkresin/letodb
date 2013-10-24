/*  $Id: letocdp.ch,v 1.2 2010/08/19 15:43:07 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Header file for external codepages
 *
 * Copyright 2010 Pavel Tsarenko <tpe2 / at / mail.ru>
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

#ifdef __XHARBOUR__
   REQUEST ;
      HB_CODEPAGE_ELWIN, HB_CODEPAGE_ESWIN, ;
      HB_CODEPAGE_HR437, HB_CODEPAGE_HR852, ;
      HB_CODEPAGE_HU852, HB_CODEPAGE_HUWIN, HB_CODEPAGE_HUISO, ;
      HB_CODEPAGE_IT437, HB_CODEPAGE_IT850, ;
      HB_CODEPAGE_PL852, HB_CODEPAGE_PLWIN, ;
      HB_CODEPAGE_PT850, HB_CODEPAGE_PTISO, ;
      HB_CODEPAGE_RU866, HB_CODEPAGE_RUKOI8, ;
      HB_CODEPAGE_SL852, HB_CODEPAGE_SLISO, HB_CODEPAGE_SLWIN, ;
      HB_CODEPAGE_SV850, HB_CODEPAGE_SVWIN, ;
      HB_CODEPAGE_UA866, HB_CODEPAGE_UAKOI8, HB_CODEPAGE_UA1251
//#if defined(__XHARBOUR__) && ( (__XHARBOUR__ - 0) < 6347 )
//   REQUEST HB_CODEPAGE_RU1251
//#else
   REQUEST HB_CODEPAGE_RUWIN
//#endif

#else

   REQUEST ;
      HB_CODEPAGE_ELWIN, HB_CODEPAGE_ESWIN, ;
      HB_CODEPAGE_HR852, ;
      HB_CODEPAGE_HU852, HB_CODEPAGE_HUWIN, HB_CODEPAGE_HUISO, ;
      HB_CODEPAGE_IT437, HB_CODEPAGE_IT850, ;
      HB_CODEPAGE_PL852, HB_CODEPAGE_PLWIN, ;
      HB_CODEPAGE_PT850, HB_CODEPAGE_PTISO, ;
      HB_CODEPAGE_RU866, HB_CODEPAGE_RUKOI8, HB_CODEPAGE_RU1251, ;
      HB_CODEPAGE_SL852, HB_CODEPAGE_SLISO, HB_CODEPAGE_SLWIN, ;
      HB_CODEPAGE_SV850, HB_CODEPAGE_SVWIN, ;
      HB_CODEPAGE_UA866, HB_CODEPAGE_UAKOI8, HB_CODEPAGE_UA1251

//      HB_CODEPAGE_HR437,

#endif
