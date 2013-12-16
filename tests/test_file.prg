/* $Id: $ */
/*
 * This sample demonstrates how to use file functions with Leto db server
 */

#include "rddleto.ch"

Function Main
Local cPath := "//127.0.0.1:2812/"
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

   ? 'leto_file( "test1.txt" ) - '
   ?? Iif( leto_file( "//127.0.0.1:2812/test1.txt" ), "Ok", "No" )

   ? 'leto_memowrite( "test1.txt" ) - '
   ?? Iif( leto_memowrite( "//127.0.0.1:2812/test1.txt", "A test!" ), "Ok", "Failure" )

   ? 'leto_file( "test1.txt" ) - '
   ?? Iif( leto_file( "//127.0.0.1:2812/test1.txt" ), "Ok", "No" )

   ? 'leto_memoread( "test1.txt" ) - '
   ?? leto_memoread( "//127.0.0.1:2812/test1.txt" )

   ? 'leto_frename( "test1.txt","test2.txt" ) - '
   ?? Iif( leto_frename( "//127.0.0.1:2812/test1.txt","test2.txt" ) == 0, "Ok", "Failure" )

   ? 'leto_file( "test1.txt" ) - '
   ?? Iif( leto_file( "//127.0.0.1:2812/test1.txt" ), "Ok", "No" )

   ? 'leto_file( "test2.txt" ) - '
   ?? Iif( leto_file( "//127.0.0.1:2812/test2.txt" ), "Ok", "No" )

   ? 'leto_ferase( "test2.txt" ) - '
   ?? Iif( leto_fErase( "//127.0.0.1:2812/test2.txt" ) == 0, "Ok", "Failure" )
   ?

   ? "Press any key to finish..."
   Inkey(0)

Return Nil
