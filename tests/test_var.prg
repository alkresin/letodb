/* $Id: test_var.prg,v 1.2 2010/06/01 09:07:37 alkresin Exp $ */
/*
 * This sample demonstrates how to use set/get variables functions with Leto db server
 */

#include "rddleto.ch"

Function Main
Local cPath := "//127.0.0.1:2812/temp/"
Local lRes

   REQUEST LETO
   RDDSETDEFAULT( "LETO" )

   ? "Connect to " + cPath + " - "
   IF ( leto_Connect( cPath ) ) == -1
      nRes := leto_Connect_Err()
      IF nRes == LETO_ERR_LOGIN
         ?? "Login failed"
      ELSEIF nRes == LETO_ERR_RECV
         ?? "Recv Error"
      ELSEIF nRes == LETO_ERR_SEND
         ?? "Send Error"
      ELSE
         ?? "No connection"
      ENDIF
      Return Nil
   ELSE
      ?? "Ok"
   ENDIF
   ?

   ?  "Adding 'var_int' = 100 to [main] - "
   lRes := leto_varSet( "main","var_int",100 )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ?  "Adding 'var_int' = 100 to [main] - "
   lRes := leto_varSet( "main","var_int",100,LETO_VCREAT )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ?  "Adding 'var_log' = .T. to [main] - "
   lRes := leto_varSet( "main","var_log",.T.,LETO_VCREAT )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ?  "Adding 'var_char' = 'Just a small test;' to [main] - "
   lRes := leto_varSet( "main","var_char","Just a small test;",LETO_VCREAT )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ShowVars()

   ? "var_int =",leto_varGet( "main","var_int" )
   ? "var_char =",leto_varGet( "main","var_char" )
   ? "var_log = ",leto_varGet( "main","var_log" )

   ? "Press any key to continue..."
   Inkey(0)

   ? "Increment var_int, current value is ",leto_varIncr( "main","var_int" )
   ? "Increment var_int, current value is ",leto_varIncr( "main","var_int" )
   ? "Decrement var_int, current value is ",leto_varDecr( "main","var_int" )

   ? "Delete var_log - "
   lRes := leto_varDel( "main","var_log" )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ShowVars()

   ? "Delete var_char - "
   lRes := leto_varDel( "main","var_char" )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ShowVars()

   ? "var_int =",leto_varGet( "main","var_int" )

   ? "Delete var_int - "
   lRes := leto_varDel( "main","var_int" )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ?

   ? "Press any key to finish..."
   Inkey(0)

Return Nil

Static Function ShowVars()
Local i, j, arr, arr1

   ? "Vars list:"
   IF ( arr := leto_varGetlist() ) != Nil
      FOR i := 1 TO Len( arr )
         ? arr[i] + " ("
         arr1 := leto_varGetlist( arr[i] )
         FOR j := 1 TO Len( arr1 )
            ?? arr1[j] + Iif( j == Len( arr1 ), ")-->", "," )
         NEXT
         arr1 := leto_varGetlist( arr[i],8 )
         FOR j := 1 TO Len( arr1 )
            ? "   " + arr1[j,1] + ":",arr1[j,2]
         NEXT
      NEXT
   ELSE
      ? "Error reading list:",leto_ferror()
   ENDIF
   ? "----------"

Return Nil
