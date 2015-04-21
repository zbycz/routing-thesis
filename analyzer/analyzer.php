<?php

require "SRTMGeoTIFFReader.php";

class TrackAnalyzer
{

	public $gpx, $dataReader, $srtmTimer;

	public function __construct($file)
	{
		if (!file_exists($file)) {
			throw new Exception("File not found");
		}

		$this->srtmTimer = microtime(true);
		$this->gpx = new SimpleXMLElement(file_get_contents($file));
		$this->dataReader = new SRTMGeoTIFFReader("GeoData"); // directory containing SRTM data files
	}

	public $ip = false; //interpolate
	public $pythaDistance = false; //non-planar distance
	public $skip = false; //skip v < 2 km/h

	public $data;
	public $totalDist = 0;

	/** parses the GPX file and files $data array, adds srtmEle */
	public function analyze()
	{
		echo "trkseg: " . count($this->gpx->trk->trkseg) . "<br>";
		$trkseg = $this->gpx->trk->trkseg;

		$this->data = array();
		$prev = false;
		$k = 0;
		foreach ($trkseg->trkpt as $trkpt) {

			$this->data[$k++] = $r = new stdClass;
			$r->k = $k;
			$r->lat = (double) $trkpt['lat'];
			$r->lon = (double) $trkpt['lon'];
			if (!$prev) {
				$prev = $r; //first row
			}

			//elevation
			$r->srtmEle = $this->dataReader->getElevation($r->lat, $r->lon, $this->ip);
			$r->gpsEle = $trkpt->ele;

			//distance diff
			$planarDist = $this->dataReader->getDistance($r->lat, $r->lon, $prev->lat, $prev->lon, false);
			$eledif = ($prev->srtmEle - $r->srtmEle)/1000;
			if($this->pythaDistance)
				$r->dist = sqrt(pow($planarDist,2) +  pow($eledif,2));
			else
				$r->dist = $planarDist;
			$this->totalDist += $r->dist;

			//time diff
			$r->time = new DateTime($trkpt->time);
			$i = $r->time->diff($prev->time);
			$r->sec = $i->s + $i->i * 60 + $i->h * 60 * 60;

			//velocity
			$r->v = @ round($r->dist / ($r->sec / 60 / 60));
			$r->skip = $this->skip && ($r->v < 2);

			$prev = $r;
		}

		echo "timer-gpx+srtm: " . round(microtime(true) - $this->srtmTimer, 3) . "s<br>";
	}

	public $agg;

	/** aggregates sections cca equaling $sectionTime */
	public function aggSeconds($sectionTime = 60)
	{
		$buff = new stdClass;
		$buff->latlons = array();
		$buff->len = 0;

		$sections = 0;
		$addFinalLatlon = FALSE;

		$this->agg = array();
		foreach ($this->data as $k => $r) {
			if ($addFinalLatlon) {
				array_push($first->latlons, $r->lat);
				array_push($first->latlons, $r->lon);
				$addFinalLatlon = FALSE;
			}

			if(!$r->skip){
				$buff->sec += $r->sec;
				$buff->dist += $r->dist;
				array_push($buff->latlons, $r->lat);
				array_push($buff->latlons, $r->lon);
				$buff->len++;
			}

			if (($buff->sec >= $sectionTime) OR ($r->skip && $buff->len) ) { //dopočítáme první buffer a uděláme nový
				$first = $this->agg[] = $this->data[$k - $buff->len + 1 - $r->skip];
				$first->rows = $buff->len;
				$first->agSec = $buff->sec;
				$first->agDist = $buff->dist;
				$first->agV = round($buff->dist / ($buff->sec / 60 / 60));
				$first->latlons = $buff->latlons;

				$buff = new stdClass;
				$buff->latlons = array();
				$buff->len = 0;
				$addFinalLatlon = TRUE;
				$sections++;
			}

			if($r->skip){
				$skipped = $this->agg[] = $r;
			}
		}

		echo "sections by time {$sectionTime}sec: $sections ×";
	}

	public $hills;
	public $totalUp, $totalDown;

	public function aggHills()
	{
		$buff = new stdClass;
		$buff->len = 0;

		foreach ($this->agg as $key => $r) {

			if(!$r->skip){
				$buff->sec += $r->agSec;
				$buff->dist += $r->agDist;
				$buff->len++;

				$direction = sgn($this->agg[$key + 1]->srtmEle - $r->srtmEle); //+ stoupame -klesame
			}
			else $direction = 0;


			if ($buff->direction != $direction OR ($r->skip && $buff->len )) { //dopočítáme první buffer a uděláme nový
				$first = (object)$this->agg[$key - $buff->len - $r->skip]; //-$r->skip kompenzuje $buff->len
				$first->hilRows = $buff->len;
				$first->hilSec = $buff->sec;
				$first->hilDist = $buff->dist;
				$first->hilDistm = round(1000 * $buff->dist);
				$first->hilV = @round($buff->dist / ($buff->sec / 60 / 60));
				$first->hilEledif = $r->srtmEle - $first->srtmEle;
				$first->hilEndLL = "$r->lat,$r->lon";
				$first->hilPerc = @round($first->hilEledif / $first->hilDistm  *100);

				$buff = new stdClass;
				$buff->len = 0;
			}
			$buff->direction = $direction;


		}
	}

	public function outputHillsPercents()
	{
		$perc = array();
		foreach ($this->agg as $key => $r) {
			if ($r->hilRows AND !$r->skip) {
				if($r->hilDist > 1) //km
					$perc[] = $r;
		}}

		usort($perc, function($a,$b){ return $a->hilPerc<$b->hilPerc; });

		echo "<table border=1 borderColor=lightgray cellpadding=5>
			<tr><th>kopec<th>↗↘<th>[s]<th>[v]";
		foreach($perc as $r){
			echo "<tr id=r$r->k class=row data='$r->lat,$r->lon,$r->k,$r->srtmEle,$r->gpsEle'>";
			$dist = round($r->hilDist, 1);
			echo "<td> $r->hilPerc %";
			echo "<td>". eledif($r->hilEledif);
			echo "<td>$dist <small>km</small> ";
			echo "<td>$r->hilV <small>km/h</small> ";
		}
		echo "</table><br><br>";
	}

	public function outputAllTrkpts()
	{
		echo "<table border=1 borderColor=lightgray cellpadding=5>
			<tr><th>ts<th>t<th>s<th>v<th>gps<th>srtm<th>[t]<th>[s]<th>[v]<th>↗↘";
		foreach ($this->data as $k => $r) {
			$class = $r->skip ? " skip" : "";
			echo "
			<tr id=r$k class='row$class' data='$r->lat,$r->lon,$k,$r->srtmEle,$r->gpsEle'>
			<td>" . $r->time->format('H:i:s') . "
			<td>$r->sec <small>s</small>";
			echo $r->v ? "<td>" . round(1000 * $r->dist) . " <small>m</small><td>$r->v <small>km/h</small>" : "<td><td>";
			/* echo "<td" . (abs($r->gpsEle - $r->srtmEle) < 15 ? ' class=small' : '') . ">$r->gpsEle"; */

			//eledif
			$a = round($r->gpsEle);
			$b = round($this->data[$k - 1]->gpsEle);
			echo "<td>" . eledif($a && $b ? $a - $b : 0) . " <small>$r->gpsEle</small>";

			//eledif
			echo "<td>" . eledif($r->srtmEle - $this->data[$k - 1]->srtmEle) . " <small>$r->srtmEle</small>";

			if ($r->rows) {
				echo "<td rowspan=$r->rows>$r->agSec <small>s</small>";
				echo "<td rowspan=$r->rows>" . round(1000 * $r->agDist) . " <small>m</small>";
				echo "<td rowspan=$r->rows>$r->agV <small>km/h</small>";


				$this->dataReader->getMultipleElevations($r->latlons, false, $this->ip);
				echo "<td rowspan=$r->rows>" . eledif($this->dataReader->getAscentDescent());
			}
		}
		echo "</table>";
	}

	public function outputSections()
	{
		echo "<table border=1 borderColor=lightgray cellpadding=5>
			<tr><th>ts<th>[t]<th>[s]<th>[v]<th title=srtm4>[ele]<th>Δ <th>↗↘ <th># <th>add/del<th>#<th colspan=2>hill sections";

		$prev = new stdClass();
		foreach ($this->agg as $key => $r) {
			$class = $r->skip ? " class=skip" : "";
			echo "
			<tr id=r$r->k data='$r->lat,$r->lon,$r->k,$r->srtmEle,$r->gpsEle'$class>
			<td>" . $r->time->format('H:i:s') . "
			<td>$r->agSec <small>s</small>";

			echo $r->agV ? "<td>" . round(1000 * $r->agDist) . " <small>m</small><td>$r->agV <small>km/h</small>" : "<td><td>";

			echo "<td>$r->srtmEle <small></small>";
			//eledif
			echo "<td>" . eledif($this->agg[$key + 1]->srtmEle - $r->srtmEle);

			if($r->skip)
				echo "<td><td><td><td>";
			else {
				//getMultipleElevations
				$points = $this->dataReader->getMultipleElevations($r->latlons, false, $this->ip);
				$ascdes = $this->dataReader->getAscentDescent();
				echo "<td>" . eledif($ascdes); // title='" . implode(",", $points) . "'
				echo "<td><small>" . count($points) . "</small>";


				//getMultipleElevations - add intermediate points
				$points = $this->dataReader->getMultipleElevations($r->latlons, true, $this->ip); //addIntermediatelatLons
				$ascdes2 = $this->dataReader->getAscentDescent();
				echo "<td>" . ($ascdes != $ascdes2 ? eledif($ascdes2) : "");
				echo "<td><small>" . count($points) . "</small>";

				$this->totalUp += $ascdes2['ascent'];
				$this->totalDown += $ascdes2['descent'];
			}

			//rowspan
			if ($r->hilRows AND !$r->skip) {
				$dist = round($r->hilDist, 1);
				echo "<td rowspan=$r->hilRows>". eledif($r->hilEledif);
				echo "<td rowspan=$r->hilRows title='$dist km'>$r->hilV <small>km/h</small> ";
				//echo $r->hilEledif ." ~ " .($r->hilDist*1000);
				echo "<td rowspan=$r->hilRows> $r->hilPerc %";
			}
			$prev = $r;
		}
		echo "</table>";

	}

	public function outputHeightData($agg = false)
	{
		$data = $agg ? $this->agg : $this->data;

		$min = 11000;
		$max = 0;
		foreach ($data as $r) {
			$min = min($min, $r->srtmEle);
			$max = max($max, $r->srtmEle);
		}
		$min -= 30;

		echo "<div class='srtm back'>";
		foreach ($data as $k => $r) {
			$h = round(($r->srtmEle - $min) / ($max - $min) * 200); //200px height
			echo "<div id=e$r->k><div style='height:{$h}px;'></div></div>";
		}
		echo "</div>";

		echo "<div class='srtm over'>";
		foreach ($data as $k => $r) {
			$h = round(($r->gpsEle - $min) / ($max - $min) * 200); //200px height
			$hmark = "$r->srtmEle / gps: $r->gpsEle";
			echo "<div id=x$r->k><div style='height:{$h}px;'></div></div>";
		}
		echo "</div>";



		echo "<div style='position:fixed;right:0;bottom:0;'>
				Total dist: ".round($this->totalDist,1)." km<br>
				↗ $this->totalUp; ↘ $this->totalDown<br>
				<b id='hmark'></b>
				Min/max: $min -> $max</div>
				";
	}

}

function eledif($eledif)
{
	if (is_array($eledif))
		return eledif($eledif['ascent']) . eledif(-$eledif['descent']);
	if ($eledif)
		return ($eledif > 0 ? "↗$eledif" : ("<i>↘" . abs($eledif) . "</i>"));
}

function sgn($num)
{
	if ($num > 0)
		return 1;
	if ($num < 0)
		return -1;
	return 0;
}
