<?php require("letocl.php");
  //$GLOBALS['leto_debug1'] = true;
  $testAddress = "199.10.11.9";
?>
<html>
<head><title> Leto test </title>
</head>
<body>
 <?php
  echo "Connecting... ";
  $conn = letoConnect( $testAddress, 2812 );
  if( $conn ) {
    echo $conn["ver"]."<br>";
    $arr = letoMgInfo($conn);
    echo "<br>Users current:  ".$arr[0]."  Users max:  ".$arr[1]."<br />";
    echo "Tables current: ".$arr[2]."  Tables max: ".$arr[3]."<br />";
    echo "Operations: ".$arr[5]."<br />";
    echo "Kbytes sent: ".strval(round(intval($arr[6])/1024))."  Kbytes read: ".strval(round(intval($arr[7])/1024))."<br />";
    echo "<br>";

    letoVarSet($conn,"g_1","var1","2",101,false,true);

    $n1 = letoVarGet($conn,"g_1","var1");
    if( $n1 )
      echo "var1 = ".strval($n1)."<br>";
    else
      echo "var1 = Error<br>";

    letoVarIncr($conn,"g_1","var1",false);
    $n1 = letoVarGet($conn,"g_1","var1");
    echo "After increment ";
    if( $n1 )
      echo "var1 = ".strval($n1)."<br>";
    else
      echo "var1 = Error<br>";

    letoVarDel($conn,"g_1","var1");
  }

  if(isset($conn))
    socket_close($conn["sock"]);
 ?>

</body>
</html>