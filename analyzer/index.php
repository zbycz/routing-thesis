<meta charset="utf-8">
<div style="position:fixed;right:0;top:0;width:600px;bottom:0;padding-bottom:200px;">
	<div id="map" style="height:100%;"></div>
</div>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.4.2/jquery.min.js"></script>
<script src="lib/OpenLayers.js"></script>
<script src="analyzer.js"></script>
<link rel="stylesheet" href="http://openlayers.org/api/theme/default/style.css" type="text/css">
<link rel="stylesheet" href="analyzer.css">

<?php
error_reporting(E_ALL & ~E_NOTICE);


echo "<form><select name=file>";
foreach (glob('gpx/*') as $f) {
	echo $_GET['file'] == $f ? "<option selected>$f" : "<option>$f";
}
echo "</select>";
echo "<br><label><input type=checkbox name=agg checked>agg minutes:</label><input name=aggm value=1 size=1>";
echo "<label><input type=checkbox name=ip checked>interpolate</label> ";
echo "<label><input type=checkbox name=pytha>non-planar distance</label> ";
echo "<br><label><input type=checkbox name=skip checked>skip waiting</label> ";
echo "<input type=submit></form>";

echo "<script>addGPX('$_GET[file]');</script>";
echo "<p><label><input type=checkbox id=move>move map on move (JS)</label></p> ";

if (!$_GET['file'])
	exit;

require "analyzer.php";
$analyzer = new TrackAnalyzer($_GET['file']);

if ($_GET['ip']) $analyzer->ip = TRUE;
if ($_GET['pytha']) $analyzer->pythaDistance = TRUE;
if ($_GET['skip']) $analyzer->skip = TRUE;

$analyzer->analyze();
$analyzer->aggSeconds(abs(intval($_GET['aggm']))*60 ?: 60);


//export
if($_GET['start']){
	$start = false;
	echo "<hr>lat lon gps srtmIP srtmNN<br>";
	foreach($analyzer->data as $r){
		if($r->time->format('H:i:s') == $_GET['start'])	$start = TRUE;
		if($_GET['stop'] && $r->time->format('H:i:s') == $_GET['stop'])	break;
		if(!$start) continue;
	
		printf("%0.6f %0.6f ", $r->lat, $r->lon);
		echo " $r->gpsEle  $r->srtmEle ";
		echo $analyzer->dataReader->getElevation($r->lat, $r->lon, FALSE);
		echo " <br>";
	}
}



if ($_GET['agg']) {
	$analyzer->aggHills();
	$analyzer->outputHillsPercents();
	$analyzer->outputSections();
} else {
	$analyzer->outputAllTrkpts();
}

$analyzer->outputHeightData($_GET['agg']);
