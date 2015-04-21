/* 	
Displays maps and elevations
Bob Osola http://www.osola.org.uk
*/

var dataType = "SRTM"; // either SRTM or OS

var osMap;

$(document).ready(function(){
    
    var OSpoints = [];
    var SRTMpoints = [];     
    var elevation = 0;    // holds elevation data returned from PHP class
    var interpolate = 0;  
    var lineStyle = {strokeColor: "#FF0000", strokeOpacity: 1, strokeWidth: 2};
	
    
    
    // display map       
    osMap = new OpenLayers.Map({
          div: "map",
          theme: null,
					projection: new OpenLayers.Projection("EPSG:900913"), //v metrech pro mercatora
					displayProjection: new OpenLayers.Projection("EPSG:4326"), //latlon
          
          controls: [
              new OpenLayers.Control.TouchNavigation({
                  dragPanOptions: {
                      enableKinetic: true
                  }
              }),
              new OpenLayers.Control.Zoom(),
							new OpenLayers.Control.MousePosition()
          ],
          layers: [
              new OpenLayers.Layer.OSM("OpenStreetMap", null, {
                  transitionEffect: "resize",
                  attribution: "&copy; <a href='http://www.openstreetmap.org/copyright'>OpenStreetMap</a> contributors"
              }),
              new OpenLayers.Layer.Vector("Vek")
          ]
      });
		  
		osMap.setCenter( fromLL(new OpenLayers.LonLat(14.3, 50.1)), 			14);

                          
    // get position data from mouse position by registering an onMouseMove handler   
    osMap.events.register("mousemove", osMap, function(e) {
                
        // NB: this next contains map units which are easting & northings for an OS map      
		    var point = osMap.getLonLatFromViewPortPx(e.xy);
        
        
        if (dataType == 'SRTM') {
            // convert OS units to lat & lon
            var lonlat = toLL(point);
            
						// send lat & long to PHP class to get elevation data
            var ajaxparams = "lat=" + lonlat.lat + "&lon=" +  lonlat.lon   
            $.post("getSRTMElevationDemo.php", ajaxparams, 
                function(returnedData){
                    elevation = returnedData;
                }, 'text');
        }
         
        // display the returned elevation in the table cell
        $('#Elev').html(dataType + ": " + elevation);      
    });
    

    // get position data from mouse clicks by registering an onClick handler
    osMap.events.register("click", osMap, function(e) {
   
   
        if (SRTMpoints.length >= 2) {
            $('#E1').html('');
            $('#N1').html(''); 
            SRTMpoints.length = 0; 
          }
   
        var vectorLayer = osMap.layers[1];
        
        // save the 1st mouse click coordinates to an array
        var lonLat = osMap.getLonLatFromViewPortPx(e.xy);
        var point = new OpenLayers.Geometry.Point(lonLat.lon, lonLat.lat);
        
				
        // convert OS grid units to lat & lon
        var SRTMlonlat = toLL(lonLat);
        SRTMpoints.push(SRTMlonlat);
        
        
        
        // display the 1st click coordinates in the table cell
        if (SRTMpoints.length == 1) { 
            $('#E0').html(SRTMpoints[0].lon.toFixed(6));
            $('#N0').html(SRTMpoints[0].lat.toFixed(6)); 
        } 
        
        // display the 2nd click coordinates in the table cell                           
        if (SRTMpoints.length == 2) {
                $('#E1').html(SRTMpoints[1].lon.toFixed(6));
                $('#N1').html(SRTMpoints[1].lat.toFixed(6)); 
                    
            // draw a line between the two markers
            var lineString = new OpenLayers.Geometry.LineString(OSpoints);
            var lineFeature = new OpenLayers.Feature.Vector(lineString, null, lineStyle); 
            vectorLayer.addFeatures([lineFeature]);
            
            // set up the parameters to pass to the PHP class which draws the chart
            // - this acts as the 'src' parameter of an <image>
            
            var imageSrc = "getSRTMChartDemo.php"
                + "?lat0=" + SRTMpoints[0].lat + "&lon0=" +  SRTMpoints[0].lon 
                + "&lat1=" + SRTMpoints[1].lat + "&lon1=" +  SRTMpoints[1].lon; 
                                   
            $('#chart').attr('src', imageSrc);
            $('#chart').show();
        }                                
    });
});

function setDataType(){
	// gets value of data selection radio buttom
    dataType = $(this).val();
}
function toLL(obj){return obj.clone().transform(osMap.projection, osMap.displayProjection);}
function fromLL(obj){return obj.clone().transform(osMap.displayProjection, osMap.projection);}
