
#include <stdio.h>
#include "letocl.h"

void main( void )
{
   LETOCONNECTION * pConnection;

   LetoInit();

   if( ( pConnection = LetoConnectionNew( "127.0.0.1", 2812, NULL, NULL, 0 ) ) != NULL )
   {
      char * ptr, szData[64];

      printf( "Connected!\r\n" );
      printf( "%s\r\n", LetoGetServerVer( pConnection ) );
      if( ( ptr = LetoMgGetInfo( pConnection ) ) != NULL && *(ptr-1) == '+' )
      {
         LetoGetCmdItem( &ptr, szData ); ptr ++;
         printf( "Users current:  %s\t\t", szData );
         LetoGetCmdItem( &ptr, szData ); ptr ++;
         printf( "max: %s\r\n", szData );

         LetoGetCmdItem( &ptr, szData ); ptr ++;
         printf( "Tables current: %s\t\t", szData );
         LetoGetCmdItem( &ptr, szData ); ptr ++;
         printf( "max: %s\r\n", szData );

      }

      LetoConnectionClose( pConnection );
   }
   else
      printf( "Connection failure\r\n" );

   LetoExit( 1 );
}
