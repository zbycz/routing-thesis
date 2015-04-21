
function toLL(obj) {
	return obj.clone().transform(osMap.projection, osMap.displayProjection);
}
function fromLL(obj) {
	return obj.clone().transform(osMap.displayProjection, osMap.projection);
}

// display map
var markerL, markerL2;
var osMap = new OpenLayers.Map({
	div: "map",
	theme: null,
	projection: new OpenLayers.Projection("EPSG:900913"), //v metrech pro mercatora
	displayProjection: new OpenLayers.Projection("EPSG:4326"), //latlon

	controls: [
		//new OpenLayers.Control.TouchNavigation({dragPanOptions: {enableKinetic: true}}),
		new OpenLayers.Control.Navigation(),
		new OpenLayers.Control.Zoom(),
		new OpenLayers.Control.MousePosition(),
		new OpenLayers.Control.LayerSwitcher()
	],
	layers: [
	
	
		new OpenLayers.Layer.XYZ("mq", ['http://otile3.mqcdn.com/tiles/1.0.0/map/${z}/${x}/${y}.png'], {numZoomLevels: 20}),
		
		new OpenLayers.Layer("Blank",{
			isBaseLayer: true,
			maxExtent: new OpenLayers.Bounds(
			    -128 * 156543.0339,
			    -128 * 156543.0339,
			    128 * 156543.0339,
			    128 * 156543.0339
			),
			maxResolution: 156543.0339,
			numZoomLevels: 21,
			units: "m",
			projection: "EPSG:900913"
		}),		
		markerL = new OpenLayers.Layer.Markers("Vek"),
		markerL2 = new OpenLayers.Layer.Markers("Vek2"),
		new OpenLayers.Layer.OSM("OpenStreetMap", null, {
			transitionEffect: "resize",
			attribution: "&copy; <a href='http://www.openstreetmap.org/copyright'>OpenStreetMap</a> contributors",
			numZoomLevels: 20
		}),
		new OpenLayers.Layer.XYZ("vrst", 'http://opentrackmap.cz/contours/${z}/${x}/${y}.png', {numZoomLevels: 20})

	]
});
osMap.setCenter(fromLL(new OpenLayers.LonLat(14.3, 50.1)), 10);





for(i=15; i<30; i+=3){
markerL2.addMarker(new OpenLayers.Marker(fromLL(new OpenLayers.LonLat(14 + (14+i/60)/60, 50+(52+27/60)/60)) ));
markerL2.addMarker(new OpenLayers.Marker(fromLL(new OpenLayers.LonLat(14 + (14+i/60)/60, 50+(52+24/60)/60)) ));
}


function addGPX(GPXpath) {

	osMap.addLayer(new OpenLayers.Layer.Vector("GPX", {
		projection: osMap.displayProjection,
		strategies: [new OpenLayers.Strategy.Fixed()],
		eventListeners: {'loadend': function() {
				osMap.zoomToExtent(this.getDataExtent());
			}},
		protocol: new OpenLayers.Protocol.HTTP({
			url: GPXpath,
			format: new OpenLayers.Format.GPX()
		}),
		style: {strokeColor: "green", strokeWidth: 5, strokeOpacity: 0.5}
	}));

}


$(function() {


	var prev, hmark = $('#hmark'), body = $('body'), move = document.getElementById('move');

	$('tr,.srtm>div').mouseover(function() {
		if (this.tagName === 'DIV') {
			var ele = $(this);
			var row = $('#r' + this.id.substr(1));
			body.scrollTop(row.offset().top - 20);
		}
		else {
			var row = $(this);
			var ele = $('#e' + this.id.substr(1));
		}
		var r = row.attr('data').split(',');
		var lat = r[0], lon = r[1], srtm = r[3], gps = r[4];

		markerL.clearMarkers();
		markerL.addMarker(new OpenLayers.Marker(fromLL(new OpenLayers.LonLat(lon, lat))));
		hmark.html(srtm + (gps ? ' (' + gps + ')' : ''));

		if (prev)
			prev.removeClass('mark');
		prev = row.add(ele);
		prev.addClass('mark');

		if(move.checked){
			osMap.setCenter(fromLL(new OpenLayers.LonLat(lon, lat)), 15);
		}

	}).click(function() {
		if (this.tagName === 'DIV')
			var row = $('#r' + this.id.substr(1));
		else
			var row = $(this);

		var r = row.attr('data').split(',');
		var lat = r[0], lon = r[1], srtm = r[3], gps = r[4];
		osMap.setCenter(fromLL(new OpenLayers.LonLat(lon, lat)), 15);
	});
});
