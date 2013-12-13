
#include <stdio.h>
#include "letocl.h"

void main( void )
{
   LETOCONNECTION * pConnection;

   LetoInit();

   if( ( pConnection = LetoConnectionNew( "127.0.0.1", 2812, NULL, NULL, 0 ) ) != NULL )
   {
      printf( "Connected!\r\n" );
      printf( "%s\r\n", LetoGetServerVer( pConnection ) );
      LetoConnectionClose( pConnection );
   }
   else
      printf( "Connection failure\r\n" );

   LetoExit( 1 );
}
