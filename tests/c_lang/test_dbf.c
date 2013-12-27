
#include <stdio.h>
#include "letocl.h"

static void setAddress( int argc, char *argv[], char * szAddr, int * iPort )
{
   *iPort = 2812;
   if( argc < 2 )
      strcpy( szAddr, "127.0.0.1" );
   else
   {
      char * ptr = argv[1], * ptrPort;
      unsigned int uiLen;

      if( !strncmp( ptr, "//", 2 ) )
         ptr += 2;
      if( ( ptrPort = strchr( ptr, ':' ) ) != NULL )
      {
         uiLen = ptrPort - ptr;
         *iPort = atol( ptrPort+1 );
      }
      else
         uiLen = strlen( ptr );
      memcpy( szAddr, ptr, uiLen );
      ptr = szAddr + uiLen;
      if( *(ptr-1) == '/' || *(ptr-1) == '\\' )
        ptr --;
      *ptr = '\0';
   }
}

void main( int argc, char *argv[] )
{
   LETOCONNECTION * pConnection;
   int iPort;
   char szAddr[128];

   setAddress( argc, argv, szAddr, &iPort );

   LetoInit();

   printf( "Connect to %s port %d\r\n", szAddr, iPort );
   if( ( pConnection = LetoConnectionNew( szAddr, iPort, NULL, NULL, 0 ) ) != NULL )
   {
      char * ptr, szData[64];
      LETOTABLE * pTable;

      printf( "Connected!\r\n" );
      printf( "%s\r\n", LetoGetServerVer( pConnection ) );

      pTable = LetoDbCreateTable( pConnection, "/test1", "test1", 
         "NAME;C;10;0;NUM;N;4;0;INFO;C;32;0;DINFO;D;8;0;", 0 );
      if( pTable )
      {
         unsigned int ui, uiFields, uiRet;
         unsigned long ulRecCount;
         char szRet[16];

         printf( "test1.dbf has been created.\r\n" );

         if( !LetoDbRecCount( pTable, &ulRecCount ) )
            printf( "Records: %d\r\n", ulRecCount );
         else
            printf( "LetoDbRecCount error\r\n" );

         LetoDbFieldCount( pTable, &uiFields );
         printf( "Fields number: %d\r\n", uiFields );
         for( ui=1; ui <= uiFields; ui++ )
         {
            if( !LetoDbFieldName( pTable, ui, szRet ) )
               printf( "   %-12s", szRet );
            else
               printf( "LetoDbFieldName error\r\n" );
            if( !LetoDbFieldType( pTable, ui, &uiRet ) )
               printf( "%d", uiRet );
            else
               printf( "LetoDbFieldType error\r\n" );
            if( !LetoDbFieldLen( pTable, ui, &uiRet ) )
               printf( "\t%d", uiRet );
            else
               printf( "LetoDbFieldLen error\r\n" );
            if( !LetoDbFieldDec( pTable, ui, &uiRet ) )
               printf( "\t%d\r\n", uiRet );
            else
               printf( "LetoDbFieldDec error\r\n" );
         }

         printf( "Append blank record - " );
         if( !LetoDbAppend( pTable, 0 ) )
            printf( "Ok\r\n" );
         else
            printf( "error\r\n" );

         printf( "Set first field - " );
         if( !LetoDbPutField( pTable, 1, "Kirill", 6 ) )
            printf( "Ok\r\n" );
         else
            printf( "error\r\n" );

         printf( "Set second field - " );
         if( !LetoDbPutField( pTable, 2, "56", 2 ) )
            printf( "Ok\r\n" );
         else
            printf( "error\r\n" );

         printf( "Set third field - " );
         if( !LetoDbPutField( pTable, 3, "A first record", 14 ) )
            printf( "Ok\r\n" );
         else
            printf( "error\r\n" );

         printf( "Set forth field - " );
         if( !LetoDbPutField( pTable, 4, "20131228", 8 ) )
            printf( "Ok\r\n" );
         else
            printf( "error\r\n" );

         printf( "Put record - " );
         if( !LetoDbPutRecord( pTable, 0 ) )
            printf( "Ok\r\n" );
         else
            printf( "error\r\n" );

         printf( "Close table - " );
         if( !LetoDbCloseTable( pTable ) )
            printf( "Ok\r\n" );
         else
            printf( "error\r\n" );
      }
      else
         printf( "Can not create the test1.dbf\r\n" );

      LetoConnectionClose( pConnection );
   }
   else
      printf( "Connection failure\r\n" );

   LetoExit( 1 );
}
