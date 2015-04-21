<?php
error_reporting(E_ALL & ~E_NOTICE);


//pøíprava dat pro závìreèné vizualizace


//souøadnice
//haunspaulka: lon1=14.365070;lat1=50.098554;lon2=14.391910;lat2=50.113642 
	//50.098554,14.365070 50.113642,14.391910
	
	//br rout ok
	 
//zizkov: lon1=14.32252;lat1=50.09234;lon2=14.44949;lat2=50.07727
 //50.09234,14.32252 50.07727,14.44949



echo "<form><select name=file>";
foreach (glob('gpx*') as $f) {
	echo $_GET['file'] == $f ? "<option selected>$f" : "<option>$f";
}
echo "</select>";
echo "<input type=submit></form>";

if(!$_GET['file']) die();

require "../SRTMGeoTIFFReader.php";
$g = new SRTMGeoTIFFReader("../GeoData");


foreach (glob($_GET['file'].'/*.gpx') as $file) {

$title = $file;
if(strpos($file, 'bbbike') !== false) $title = "BBBike";
if(strpos($file, 'brouter') !== false) $title = "BRouter";
if(strpos($file, 'openrout') !== false) $title = "OpenRouteService";
if(strpos($file, 'yours') !== false) $title = "YOURS";
if(strpos($file, 'mapy') !== false) $title = "Mapy.cz";
if(strpos($file, 'routino') !== false) $title = "Routino ".strstr($file, 'hills');



$gpx = new SimpleXMLElement(file_get_contents($file));
$trkseg = $gpx->trk->trkseg;

$out = "";
$prevPoint = false;
$dist = 0.0;
$asc=0;
$desc=0;
foreach ($trkseg->trkpt as $trkpt) {
	$ele = $g->getElevation((double)$trkpt['lat'], (double)$trkpt['lon'], TRUE);//interpolace

	if(!$prevPoint){
		$out .= "$dist\t$trkpt[lat]\t$trkpt[lon]\t$ele\n";

		$prevPoint = array((double)$trkpt['lat'], (double)$trkpt['lon']);
		continue;		
	}
	
	$dist += 1000*$g->getDistance((double)$trkpt['lat'], (double)$trkpt['lon'], $prevPoint[0], $prevPoint[1], false);
	$out .= sprintf("%0.1f", $dist);
	$out .= "\t$trkpt[lat]\t$trkpt[lon]\t$ele\n";

	$g->getMultipleElevations(array((double)$trkpt['lat'], (double)$trkpt['lon'], $prevPoint[0], $prevPoint[1]), false, TRUE);
	$x = $g->getAscentDescent();
	$asc += $x['ascent'];
	$desc += $x['descent'];



	$prevPoint = array((double)$trkpt['lat'], (double)$trkpt['lon']);
}


$dist = round($dist);
echo "$title	&	$dist  & $asc	&	$desc	 \\\\<br>";
$gnuplot .= "'$file.dat' u 1:4 t '$title', \\<br>";
file_put_contents($file.".dat", $out);

}

echo "<hr>$gnuplot";


