/* $Id: test_var.prg,v 1.2.2.1 2013/12/19 10:56:09 alkresin Exp $ */
/*
 * This sample demonstrates how to use set/get variables functions with Leto db server
 */

#include "rddleto.ch"

Function Main( cPath )
Local lRes

   REQUEST LETO
   RDDSETDEFAULT( "LETO" )

   IF Empty( cPath )
      cPath := "//127.0.0.1:2812/"
   ELSE
      cPath := "//" + cPath + Iif( Right(cPath,1) $ "/\", "", "/" )
   ENDIF

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

   ?  "Adding 'var_int' = 100 to [main] (Err (3)) "
   lRes := leto_varSet( "main","var_int",100 )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ?  "Adding 'var_int' = 100 to [main] (Ok) "
   lRes := leto_varSet( "main","var_int",100,LETO_VCREAT )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ?  "Adding 'var_log' = .T. to [main] (Ok) "
   lRes := leto_varSet( "main","var_log",.T.,LETO_VCREAT )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ?  "Adding 'var_char' = 'Just a test;' to [main] (Ok) "
   lRes := leto_varSet( "main","var_char","Just a test;",LETO_VCREAT )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ShowVars()

   ? "var_int = (100)",leto_varGet( "main","var_int" )
   ? "var_char = (Just a test;)",leto_varGet( "main","var_char" )
   ? "var_log = (.T.)",leto_varGet( "main","var_log" )

   ? "Press any key to continue..."
   Inkey(0)
   ?

   ? "Increment var_int, current value is (100)",leto_varIncr( "main","var_int" )
   ? "Increment var_int, current value is (101)",leto_varIncr( "main","var_int" )
   ? "Decrement var_int, current value is (102)",leto_varDecr( "main","var_int" )

   ? "Delete var_log (Ok) "
   lRes := leto_varDel( "main","var_log" )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ShowVars()

   ? "Delete var_char (Ok) "
   lRes := leto_varDel( "main","var_char" )
   IF lRes
      ?? "Ok"
   ELSE
      ?? "Err (" + Ltrim(Str(leto_ferror())) + ")"
   ENDIF

   ShowVars()

   ? "Delete var_int (Ok) "
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

   ? "--- Vars list ---"
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
