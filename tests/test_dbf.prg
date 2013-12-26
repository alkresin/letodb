/* $Id: test_dbf.prg,v 1.1.2.1 2013/12/23 07:45:30 alkresin Exp $ */
/*
 * This sample tests working with dbf files
 * Just change the cPath value to that one you need.
 */

Function Main( cPath )
Local aNames := { "Petr", "Ivan", "Alexander", "Pavel", "Alexey", "Fedor", ;
   "Konstantin", "Vladimir", "Nikolay", "Andrey", "Dmitry", "Sergey" }
Local i, aStru
Field NAME, NUM, INFO, DINFO

   REQUEST LETO
   RDDSETDEFAULT( "LETO" )
   SET DATE FORMAT "dd/mm/yy"

   IF Empty( cPath )
      cPath := "//127.0.0.1:2812/temp/"
   ELSE
      cPath := "//" + cPath + Iif( Right(cPath,1) $ "/\", "", "/" )
   ENDIF

   dbCreate( cPath+"test1", { {"NAME","C",10,0}, {"NUM","N",4,0}, {"INFO","C",32,0}, {"DINFO","D",8,0} } )
   ? "File has been created"

   USE ( cPath+"test1" ) New
   ? "File has been opened"
   aStru := dbStruct()
   ? "Fields:", Len( aStru )
   FOR i := 1 TO Len( aStru )
      ? i, aStru[i,1], aStru[i,2], aStru[i,3], aStru[i,4]
   NEXT

   FOR i := 1 TO Len( aNames )
      APPEND BLANK
      REPLACE NAME WITH aNames[i], NUM WITH i+1000, ;
         INFO WITH "This is a record number "+Ltrim(Str(i)), ;
         DINFO WITH Date()+i-1
   NEXT
   ? "Records has been added"

   INDEX ON NAME TAG NAME
   INDEX ON Str(NUM,4) TAG NUM
   ? "File has been indexed"
   ?

   i := RecCount()
   ? "Reccount ", i, Iif( i==Len(aNames), "- Ok","- Failure" )

   GO TOP
   ? "go top", NUM, NAME, DINFO, Iif( NUM==1001, "- Ok","- Failure" )
   REPLACE INFO WITH "First"

   GO BOTTOM
   ? "go bottom", NUM, NAME, DINFO, Iif( NUM==1012, "- Ok","- Failure" )
   REPLACE INFO WITH "Last"

   ?
   ? 'ordSetFocus( "NAME" )'
   ordSetFocus( "NAME" )
   GO TOP
   ? "go top", NUM, NAME, DINFO, Iif( NUM==1003, "- Ok","- Failure" )

   SKIP
   ? "skip", NUM, NAME, DINFO, Iif( NUM==1005, "- Ok","- Failure" )

   GO BOTTOM
   ? "go bottom", NUM, NAME, DINFO, Iif( NUM==1008, "- Ok","- Failure" )

   SKIP -1
   ? "skip -1", NUM, NAME, DINFO, Iif( NUM==1012, "- Ok","- Failure" )

   SEEK "Petr"
   ? "seek", NUM, NAME, DINFO, Iif( NUM==1001, "- Ok","- Failure" )
   SEEK "Andre"
   ? "seek", NUM, NAME, DINFO, Iif( NUM==1010, "- Ok","- Failure" )

   SET FILTER TO NUM >= 1004 .AND. NUM <= 1010
   ?
   ? "SET FILTER TO NUM >= 1004 .AND. NUM <= 1010"
   GO TOP
   ? "go top", NUM, NAME, DINFO, Iif( NUM==1005, "- Ok","- Failure" )

   SKIP
   ? "skip", NUM, NAME, DINFO, Iif( NUM==1010, "- Ok","- Failure" )

   GO BOTTOM
   ? "go bottom", NUM, NAME, DINFO, Iif( NUM==1008, "- Ok","- Failure" )

   SKIP -1
   ? "skip -1", NUM, NAME, DINFO, Iif( NUM==1004, "- Ok","- Failure" )

   ? "Press any key to continue..."
   Inkey(0)

   ?
   ? "SET FILTER TO, SET ORDER TO 0"
   SET FILTER TO
   SET ORDER TO 0

   GO TOP
   ? "First record", Iif( INFO = "First", "- Ok","- Failure" )

   GO BOTTOM
   ? "Last record", Iif( INFO = "Last", "- Ok","- Failure" )

   ?
   ? 'ordSetFocus( "NUM" ), SET SCOPE TO "1009", "1011"'
   ordSetFocus( "NUM" )
   SET SCOPE TO "1009", "1011"

   GO TOP
   ? "go top", NUM, NAME, DINFO, Iif( NUM==1009, "- Ok","- Failure" )

   SKIP
   ? "skip", NUM, NAME, DINFO, Iif( NUM==1010, "- Ok","- Failure" )

   SKIP
   ? "skip", NUM, NAME, DINFO, Iif( NUM==1011, "- Ok","- Failure" )

   SKIP
   ? "skip", NUM, NAME, DINFO, Iif( Eof(), "- Ok","- Failure" )

   dbCloseAll()
   ?
   ? "Press any key to continue..."
   Inkey(0)

   USE ( cPath+"test1" ) New
   i := 0
   ? "Tags:"
   DO WHILE !Empty( Ordkey(++i) )
      ? i, ordKey(i)
   ENDDO

   ?
   ? "Press any key to finish..."
   Inkey(0)

Return Nil
