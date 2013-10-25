/*  $Id: letotray.prg,v 1.1 2010/06/14 17:16:17 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Leto tray - sample utility for start/stop letodb server under windows
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


#include "windows.ch"
#include "guilib.ch"

Static cPath
Static cIp := '127.0.0.1'
Static nPort := 2812

Function Main
Local oMainWindow, oTrayMenu, oIcon := HIcon():AddResource("ICON_1")

   cPath := DiskName() + ":\" + CurDir() + "\"

   IF ! File( cPath + "letodb.exe" )
      MsgInfo("letodb.exe not found")
      Return nil
   ENDIF

   INIT WINDOW oMainWindow MAIN TITLE "LetoDB server"

   CONTEXT MENU oTrayMenu
      MENUITEM "Start letodb" ACTION StartServer()
      MENUITEM "Stop letodb" ACTION StopServer()
      SEPARATOR
      MENUITEM "Exit"  ACTION EndWindow()
   ENDMENU

   oMainWindow:InitTray( oIcon,, oTrayMenu, "LetoDB server" )

   ACTIVATE WINDOW oMainWindow NOSHOW
   oTrayMenu:End()

Return Nil

Function StartServer
//if Connect()
//  Leto_Disconnect()  
//  MsgInfo("LetoDB server alreary running")
//else
  ShellExecute( cPath + "letodb.exe",,, cPath )
//endif

Return nil


Function StopServer
//if Connect()
//  Leto_Disconnect()  
  ShellExecute( cPath + "letodb.exe",, "stop", cPath )
//else
//  MsgInfo("LetoDB server is not running")
//endif
Return nil

Static func Connect
Return Leto_Connect("//" + cIp + ":" + LTrim(Str(nPort)) + "/") != -1
