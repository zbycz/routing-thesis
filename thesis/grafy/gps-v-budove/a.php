<?php


$gpx = new SimpleXMLElement(file_get_contents('gps-v-budove.gpx'));
$trkseg = $gpx->trk->trkseg;
foreach ($trkseg->trkpt as $trkpt) {
	echo "$trkpt[lat]\t$trkpt[lon]\t$trkpt->time\n";
}


require "../analyzer/SRTMGeoTIFFReader.php";


$g = new SRTMGeoTIFFReader("g");



$lon1=14.5165;
$lat1=49.86615;
$lon2=14.51664;
$lat2=49.86615;

echo $g->getDistance($lat1, $lon1, $lat2, $lon2, false);

