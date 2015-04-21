<?php 
 
/****************************************************************
 Returns a single elevation for a given point location
 (uses POSTed params to avoid values being cached by some browsers )
****************************************************************/
require_once '../SRTMGeoTIFFReader.php';

$lat = $_POST['lat'];
$lon = $_POST['lon'];


$time_pre = microtime(true);

$dataReader = new SRTMGeoTIFFReader("../GeoData"); // directory containing SRTM data files
echo $dataReader->getElevation($lat, $lon, $interpolate=false);

$time_post = microtime(true);
$exec_time = round(1000*($time_post - $time_pre),2);
echo " ($exec_time ms)"; 
