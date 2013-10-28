#include "hbclass.ch"
#include "hwgui.ch"
#include "hxml.ch"
#include "rddleto.ch"

#ifndef __XHARBOUR__
ANNOUNCE HB_GTSYS
REQUEST HB_GT_GUI_DEFAULT
#endif

Function Main( cAddress, cUser )
Public oApp := HApp():New( cAddress, cUser )

   INIT WINDOW oApp:oMainWnd MAIN TITLE "Messenger" ;
     AT 200,0 SIZE 320,400 FONT HFont():Add( "Georgia",0,-15,,204 )

   CONTEXT MENU oApp:oMenu
      MENUITEM "Connect" ID 101 ACTION GoConnect()
   ENDMENU

   @ 0,0 PANEL oApp:oTool OF oApp:oMainWnd SIZE oApp:oMainWnd:nWidth,28

   @ 0,0 OWNERBUTTON OF oApp:oTool    ;
        ON CLICK {||Menu()}           ;
        SIZE 28,28 FLAT  TEXT "M"

   @ 32,4 SAY oApp:oSayConnect CAPTION "" SIZE 20,20 BACKCOLOR 8421504

   @ 0,28 BROWSE oApp:oBrw1 ARRAY SIZE 320,250 STYLE WS_BORDER;
        ON SIZE {|o,x,y|o:Move(,,x,y-28)}

   oApp:oBrw1:aArray := {}
   oApp:oBrw1:AddColumn( HColumn():New( "",{|v,o|o:aArray[o:nCurrent,1]},"C",16,0 ) )
   oApp:oBrw1:AddColumn( HColumn():New( "",{|v,o|o:aArray[o:nCurrent,2]},"C",2,0 ) )

   GoConnect()
           
   ACTIVATE WINDOW oApp:oMainWnd
Return Nil

Static Function Menu()

   oApp:oMenu:Show( oApp:oMainWnd, 2, 30, .T. )
Return Nil

Static Function LoginInfo()
Local oDlg
Local cIp, nPort, cUser, cPass

   cIp   := oApp:cIp
   nPort := oApp:nPort
   cUser := oApp:cUser
   cPass := oApp:cPass

   INIT DIALOG oDlg TITLE "" AT 20,20 SIZE 500,200 ;
        FONT oApp:oMainWnd:oFont CLIPPER ;
        ON INIT {||onInit1(oDlg)}

   @ 20,10 SAY "Server:" SIZE 100,22
   @ 120,10 GET cIp SIZE 360,26

   @ 20,36 SAY "Port:" SIZE 100,22
   @ 120,36 GET nPort SIZE 80,26

   @ 20,62 SAY "Login:" SIZE 100,22
   @ 120,62 GET cUser SIZE 360,26

   @ 20,88 SAY "Password:" SIZE 100,22
   @ 120,88 GET cPass SIZE 360,26 STYLE ES_PASSWORD

   @ 200,150 BUTTON "Ok" ID IDOK SIZE 100,32

   ACTIVATE DIALOG oDlg

   IF oDlg:lResult
      IF oApp:cIp != cIp .OR. oApp:nPort != nPort .OR. oApp:cUser != cUser
         oApp:lUpd := .T.
         oApp:cIp   := cIp
         oApp:nPort := nPort
         oApp:cUser := cUser
      ENDIF
      oApp:cPass := cPass
   ENDIF

Return oDlg:lResult

Static Function onInit1( oDlg )

   IF Empty( oApp:cIp )
      SetFocus(oDlg:aControls[2]:handle)
   ELSEIF Empty( oApp:nPort )
      SetFocus(oDlg:aControls[4]:handle)
   ELSEIF Empty( oApp:cUser )
      SetFocus(oDlg:aControls[6]:handle)
   ELSEIF Empty( oApp:cPass )
      SetFocus(oDlg:aControls[8]:handle)
   ENDIF
Return .T.

Static Function GoConnect()
Local cIp, nPort, nRes, cErrText

   IF oApp:oTimer == Nil
      IF Empty(oApp:cIp) .OR. Empty(oApp:nPort) .OR. Empty(oApp:cUser) .OR. Empty(oApp:cPass)
         LoginInfo()
      ENDIF
      IF !Empty(oApp:cIp) .AND. !Empty(oApp:nPort) .AND. !Empty(oApp:cUser)
         IF leto_Connect( "//"+oApp:cIp+":"+Ltrim(Str(oApp:nPort))+"/", ;
               oApp:cUser, oApp:cPass ) == -1
            nRes := leto_Connect_Err()
            IF nRes == LETO_ERR_LOGIN
               cErrText := "Login failed"
            ELSEIF nRes == LETO_ERR_RECV
               cErrText := "Recv Error"
            ELSEIF nRes == LETO_ERR_SEND
               cErrText := "Send Error"
            ELSE
               cErrText := "No connection"
            ENDIF
            MsgStop( cErrText )
            Return .F.
         ENDIF

         IF !leto_varSet( "msg_userlist","_"+oApp:cUser,.T.,LETO_VCREAT+LETO_VOWN )
            MsgStop( "Identificatiom error" )
            Return .F.
         ENDIF

         oApp:oSayConnect:SetColor( , 65280, .T. )
         EnableMenuItem( oApp:oMenu,101, .F., .T. )
         IF oApp:oTimer == Nil
            SET TIMER oApp:oTimer OF oApp:oMainWnd VALUE 1000 ACTION {||TimerFunc()}
         ENDIF
      ENDIF
   ENDIF

Return .T.

Static Function Disconnect()

   oApp:oBrw1:aArray := {}
   oApp:oBrw1:Refresh()
   oApp:oSayConnect:SetColor( , 8421504, .T. )
   EnableMenuItem( oApp:oMenu,101, .T., .T. )
   IF oApp:oTimer != Nil
      oApp:oTimer:End()
      oApp:oTimer := Nil
   ENDIF

Return Nil

Static Function TimerFunc()
Local aUsers, i, iUs := 0
Static nTM := 0

   nTM ++
   IF nTM >= oApp:nDelta
      nTM := 0
      IF ( aUsers := leto_varGetlist( "msg_userlist",4 ) ) != Nil
         FOR i := 1 TO Len( aUsers )
            IF Substr( aUsers[i,1],2 ) != oApp:cUser
               iUs ++
               IF iUs > Len( oApp:oBrw1:aArray )
                  Aadd( oApp:oBrw1:aArray, { "","" } )
               ENDIF
               oApp:oBrw1:aArray[iUs,1] := Substr( aUsers[i,1],2 )
               oApp:oBrw1:aArray[iUs,2] := ""
            ENDIF
         NEXT
      ENDIF
      IF iUs < Len( oApp:oBrw1:aArray )
         oApp:oBrw1:aArray := Asize( oApp:oBrw1:aArray, iUs )
      ENDIF
      oApp:oBrw1:Refresh()
   ENDIF
Return Nil

Static Function ReadOptions( oApp )
Local oOptions := HXMLDoc():Read( "messenger.xml" )
Local oNode, i1, cIp, cPort, cUser, cPass

   IF !Empty( oOptions:aItems ) .AND. oOptions:aItems[1]:title == "init"
      FOR i1 := 1 TO Len( oOptions:aItems[1]:aItems )
         oNode := oOptions:aItems[1]:aItems[i1]
         IF oNode:title == "server"
            cIp := oNode:GetAttribute("ip")
            cPort := oNode:GetAttribute("port")
            cUser := oNode:GetAttribute("user")
         ENDIF
      NEXT
   ENDIF
   IF cIp != Nil
      oApp:cIp := cIp
   ENDIF
   IF cPort != Nil
      oApp:nPort := Val( cPort )
   ENDIF
   IF cUser != Nil
      oApp:cUser := cUser
   ENDIF

Return Nil

Static Function SaveOptions()
Local i, oXmlDoc := HXMLDoc():New( "windows-1251" ), oXMLNode, aAttr

   IF oApp:lUpd
     oXMLNode := HXMLNode():New( "init" )
     oXmlDoc:Add( oXMLNode )

     aAttr := { { "ip",oApp:cIp }, { "port",Ltrim(Str(oApp:nPort)) } }
     IF !Empty( oApp:cUser )
        Aadd( aAttr, { "user",oApp:cUser } )
     ENDIF
     oXMLNode:Add( HXMLNode():New( "server", HBXML_TYPE_SINGLE, aAttr ) )

     oXmlDoc:Save( "messenger.xml" )
   ENDIF
Return Nil

CLASS HApp

   DATA oMainWnd                     // Главное окно
   DATA oMenu
   DATA oTool                        // Панель инструментов
   DATA oBrw1
   DATA oSayConnect

   DATA cIp
   DATA nPort      INIT 2812
   DATA cUser
   DATA cPass

   DATA oTimer
   DATA lUpd       INIT .F.
   DATA nDelta     INIT 2

   METHOD New( cAddress, cUser )
ENDCLASS

METHOD New( cAddress, cUser ) CLASS HApp
Local nPos

   ReadOptions( Self )

   IF cAddress != Nil
      IF ( nPos := At( ":",cAddress ) ) != 0
         ::nPort := Val( Substr(cAddress,nPos+1) )
         cAddress := Left( cAddress, nPos-1 )
      ENDIF
      IF Left( cAddress,2 ) == "//"
         cAddress := Substr( cAddress, 3 )
      ENDIF
      ::cIp := cAddress
      ::lUpd := .T.
   ENDIF
   IF cUser != Nil
      ::cUser := cUser
   ENDIF

Return Self

EXIT PROCEDURE EXIPROC

   leto_DisConnect()
   IF oApp:oTimer != Nil
      oApp:oTimer:End()
   ENDIF
   SaveOptions()
RETURN
