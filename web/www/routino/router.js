//
// Routino router web page Javascript
//
// Part of the Routino routing software.
//
// This file Copyright 2008-2013 Andrew M. Bishop
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//


var vismarkers, markers, markersmoved, paramschanged;
var homelat=null, homelon=null;


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Initialisation /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Make a deep copy of the routino profile.

var routino_default={};
for(var l1 in routino)
   if(typeof(routino[l1])!='object')
      routino_default[l1]=routino[l1];
   else
     {
      routino_default[l1]={};
      for(var l2 in routino[l1])
         if(typeof(routino[l1][l2])!='object')
            routino_default[l1][l2]=Number(routino[l1][l2]);
         else
           {
            routino_default[l1][l2]={};
            for(var l3 in routino[l1][l2])
               routino_default[l1][l2][l3]=Number(routino[l1][l2][l3]);
           }
     }

// Store the latitude and longitude in the routino variable

routino.point=[];
for(var marker=1;marker<=mapprops.maxmarkers;marker++)
  {
   routino.point[marker]={};

   routino.point[marker].lon="";
   routino.point[marker].lat="";
   routino.point[marker].search="";
   routino.point[marker].active=false;
   routino.point[marker].used=false;
   routino.point[marker].home=false;
  }

// Process the URL query string and extract the arguments

var legal={"^lon"             : "^[-0-9.]+$",
           "^lat"             : "^[-0-9.]+$",
           "^zoom"            : "^[0-9]+$",

           "^lon[1-9]"        : "^[-0-9.]+$",
           "^lat[1-9]"        : "^[-0-9.]+$",
           "^search[1-9]"     : "^.+$",
           "^transport"       : "^[a-z]+$",
           "^highway-[a-z]+"  : "^[0-9.]+$",
           "^speed-[a-z]+"    : "^[0-9.]+$",
           "^property-[a-z]+" : "^[0-9.]+$",
           "^oneway"          : "^(1|0|true|false|on|off)$",
           "^turns"           : "^(1|0|true|false|on|off)$",
           "^weight"          : "^[0-9.]+$",
           "^height"          : "^[0-9.]+$",
           "^width"           : "^[0-9.]+$",
           "^length"          : "^[0-9.]+$",

           "^language"        : "^[-a-zA-Z]+$"};

var args={};

if(location.search.length>1)
  {
   var query,queries;

   query=location.search.replace(/^\?/,"");
   query=query.replace(/;/g,'&');
   queries=query.split('&');

   for(var i=0;i<queries.length;i++)
     {
      queries[i].match(/^([^=]+)(=(.*))?$/);

      k=RegExp.$1;
      v=unescape(RegExp.$3);

      for(var l in legal)
        {
         if(k.match(RegExp(l)) && v.match(RegExp(legal[l])))
            args[k]=v;
        }
     }
  }


//
// Fill in the HTML - add the missing waypoints
//

function html_init()            // called from router.html
{
 var waypoints=document.getElementById("waypoints");

 var waypoint_html=waypoints.rows[0].innerHTML;
 waypoints.deleteRow(0);

 var searchresults_html=waypoints.rows[0].innerHTML;
 waypoints.deleteRow(0);

 for(var marker=mapprops.maxmarkers;marker>=1;marker--)
   {
    var searchresults=waypoints.insertRow(0);

    searchresults.style.display="none";
    searchresults.id="searchresults" + marker;
    searchresults.innerHTML=searchresults_html.split('XXX').join(marker);

    var waypoint=waypoints.insertRow(0);

    waypoint.style.display="none";
    waypoint.id="waypoint" + marker;
    waypoint.innerHTML=waypoint_html.split('XXX').join(marker);
   }
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Form handling /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//
// Form initialisation - fill in the uninitialised parts
//

function form_init()            // called from router.html
{
 // Fill in the waypoints

 vismarkers=0;

 for(var marker=mapprops.maxmarkers;marker>=1;marker--)
   {
    var lon=args["lon" + marker];
    var lat=args["lat" + marker];
    var search=args["search" + marker];

    if(lon != undefined && lat != undefined && search != undefined && lon != "" && lat != "" && search != "")
      {
       markerAddForm(marker);

       formSetSearch(marker,search);
       formSetCoords(marker,lon,lat);

       markerAddMap(marker);

       markerSearch(marker);

       vismarkers++;
      }
    else if(lon != undefined && lat != undefined && lon != "" && lat != "")
      {
       markerAddForm(marker);

       formSetCoords(marker,lon,lat);

       markerAddMap(marker);

       markerCoords(marker);

       vismarkers++;
      }
    else if(search != undefined && search != "")
      {
       markerAddForm(marker);

       formSetSearch(marker,search);

       markerSearch(marker);

       DoSearch(marker);

       vismarkers++;
      }
    else if(vismarkers || marker<=2)
      {
       markerAddForm(marker);

       vismarkers++;
      }

    var searchfield=document.forms["form"].elements["search" + marker];

    if(searchfield.addEventListener)
       searchfield.addEventListener('keyup', searchOnReturnKey, false);
    else if(searchfield.attachEvent)
       searchfield.attachEvent('keyup', searchOnReturnKey); // Internet Explorer
   }

 // Update the transport type with the URL settings which updates all HTML forms to defaults.

 var transport=routino.transport;

 if(args["transport"] != undefined)
    transport=args["transport"];

 formSetTransport(transport);

 // Update the HTML with the URL settings

 if(args["language"] != undefined)
    formSetLanguage(args["language"]);

 for(var key in routino.profile_highway)
    if(args["highway-" + key] != undefined)
       formSetHighway(key,args["highway-" + key]);

 for(var key in routino.profile_speed)
    if(args["speed-" + key] != undefined)
       formSetSpeed(key,args["speed-" + key]);

 for(var key in routino.profile_property)
    if(args["property-" + key] != undefined)
       formSetProperty(key,args["property-" + key]);

 for(var key in routino.restrictions)
   {
    if(key=="oneway" || key=="turns")
      {
       if(args[key] != undefined)
          formSetRestriction(key,args[key]);
      }
    else
      {
       if(args["restrict-" + key] != undefined)
          formSetRestriction(key,args["restrict-" + key]);
      }
   }

 // Get the home location cookie and compare to each waypoint

 var cookies=document.cookie.split('; ');

 for(var cookie=0;cookie<cookies.length;cookie++)
    if(cookies[cookie].substr(0,"Routino-home".length)=="Routino-home")
      {
       var data=cookies[cookie].split(/[=:;]/);

       if(data[1]=="lon") homelon=Number(data[2]);
       if(data[3]=="lat") homelat=Number(data[4]);
      }

 if(homelon!=null && homelat!=null)
   {
    for(var m=1;m<=vismarkers;m++)
       markerCheckHome(m);

    // If the first location is empty and the cookie is set then fill it.

    if(!routino.point[1].used)
       markerMoveHome(1);
   }
}


//
// Function to perform the search if the return key is pressed.
// (using 'onchange' only triggers once and is confusing when clicking outside the field).
//

function searchOnReturnKey(ev)
{
 if(ev.keyCode==13)
    if(this.name.match(/^search([0-9]+)$/))
       formSetSearch(RegExp.$1);

 return(true);
}


//
// Change of language in the form
//

function formSetLanguage(value) // called from router.html (with no arguments)
{
 if(value == undefined)
   {
    for(var lang=0;lang<document.forms["form"].elements["language"].length;lang++)
       if(document.forms["form"].elements["language"][lang].checked)
          routino.language=document.forms["form"].elements["language"][lang].value;
   }
 else
   {
    for(var lang=0;lang<document.forms["form"].elements["language"].length;lang++)
       if(document.forms["form"].elements["language"][lang].value==value)
          document.forms["form"].elements["language"][lang].checked=true;
       else
          document.forms["form"].elements["language"][lang].checked=false;

    routino.language=value;
   }
}


//
// Change of transport in the form
//

function formSetTransport(value) // called from router.html
{
 routino.transport=value;

 for(var key in routino.transports)
    document.forms["form"].elements["transport"][routino.transports[key]-1].checked=(key==routino.transport);

 for(var key in routino.profile_highway)
    document.forms["form"].elements["highway-" + key].value=routino.profile_highway[key][routino.transport];

 for(var key in routino.profile_speed)
    document.forms["form"].elements["speed-" + key].value=routino.profile_speed[key][routino.transport];

 for(var key in routino.profile_property)
    document.forms["form"].elements["property-" + key].value=routino.profile_property[key][routino.transport];

 for(var key in routino.restrictions)
   {
    if(key=="oneway" || key=="turns")
       document.forms["form"].elements["restrict-" + key].checked=routino.profile_restrictions[key][routino.transport];
    else
       document.forms["form"].elements["restrict-" + key].value=routino.profile_restrictions[key][routino.transport];
   }

 paramschanged=true;
}


//
// Change of highway in the form
//

function formSetHighway(type,value) // called from router.html (with one argument)
{
 if(value == undefined)
    routino.profile_highway[type][routino.transport]=document.forms["form"].elements["highway-" + type].value;
 else
   {
    document.forms["form"].elements["highway-" + type].value=value;
    routino.profile_highway[type][routino.transport]=value;
   }

 paramschanged=true;
}


//
// Change of Speed in the form
//

function formSetSpeed(type,value) // called from router.html (with one argument)
{
 if(value == undefined)
    routino.profile_speed[type][routino.transport]=document.forms["form"].elements["speed-" + type].value;
 else
   {
    document.forms["form"].elements["speed-" + type].value=value;
    routino.profile_speed[type][routino.transport]=value;
   }

 paramschanged=true;
}


//
// Change of Property in the form
//

function formSetProperty(type,value) // called from router.html (with one argument)
{
 if(value == undefined)
    routino.profile_property[type][routino.transport]=document.forms["form"].elements["property-" + type].value;
 else
   {
    document.forms["form"].elements["property-" + type].value=value;
    routino.profile_property[type][routino.transport]=value;
   }

 paramschanged=true;
}


//
// Change of Restriction rule in the form
//

function formSetRestriction(type,value) // called from router.html (with one argument)
{
 if(value == undefined)
   {
    if(type=="oneway" || type=="turns")
       routino.profile_restrictions[type][routino.transport]=document.forms["form"].elements["restrict-" + type].checked;
    else
       routino.profile_restrictions[type][routino.transport]=document.forms["form"].elements["restrict-" + type].value;
   }
 else
   {
    if(type=="oneway" || type=="turns")
       document.forms["form"].elements["restrict-" + type].checked=value;
    else
       document.forms["form"].elements["restrict-" + type].value=value;

    routino.profile_restrictions[type][routino.transport]=value;
   }

 paramschanged=true;
}


//
// Set the feature coordinates from the form when the form changes.
//

function formSetCoords(marker,lon,lat) // called from router.html (with one argument)
{
 clearSearchResult(marker);

 if(lon == undefined && lat == undefined)
   {
    lon=document.forms["form"].elements["lon" + marker].value;
    lat=document.forms["form"].elements["lat" + marker].value;
   }

 if(lon == "" && lat == "")
   {
    document.forms["form"].elements["lon" + marker].value="";
    document.forms["form"].elements["lat" + marker].value="";

    routino.point[marker].lon="";
    routino.point[marker].lat=""
   }
 else
   {
    if(lon=="")
      {
       var lonlat=map.getCenter().clone();
       lonlat.transform(epsg900913,epsg4326);

       lon=lonlat.lon;
      }

    if(lon<-180) lon=-180;
    if(lon>+180) lon=+180;

    if(lat=="")
      {
       var lonlat=map.getCenter().clone();
       lonlat.transform(epsg900913,epsg4326);

       lat=lonlat.lat;
      }

    if(lat<-90 ) lat=-90 ;
    if(lat>+90 ) lat=+90 ;

    var lonlat = new OpenLayers.LonLat(lon,lat);
    lonlat.transform(epsg4326,epsg900913);

    markers[marker].move(lonlat);

    markersmoved=true;

    document.forms["form"].elements["lon" + marker].value=format5f(lon);
    document.forms["form"].elements["lat" + marker].value=format5f(lat);

    routino.point[marker].lon=lon;
    routino.point[marker].lat=lat;
    routino.point[marker].used=true;

    markerCheckHome(marker);
   }
}


//
// Set the search field from the form when the form changes.
//

function formSetSearch(marker,search) // called from event handler linked to router.html (with one argument)
{
 clearSearchResult(marker);

 if(search == undefined)
   {
    routino.point[marker].search=document.forms["form"].elements["search" + marker].value;

    DoSearch(marker);
   }
 else
   {
    document.forms["form"].elements["search" + marker].value=search;

    routino.point[marker].search=search;
   }
}


//
// Format a number in printf("%.5f") format.
//

function format5f(number)
{
 var newnumber=Math.floor(number*100000+0.5);
 var delta=0;

 if(newnumber>=0 && newnumber<100000) delta= 100000;
 if(newnumber<0 && newnumber>-100000) delta=-100000;

 var string=String(newnumber+delta);

 var intpart =string.substring(0,string.length-5);
 var fracpart=string.substring(string.length-5,string.length);

 if(delta>0) intpart="0";
 if(delta<0) intpart="-0";

 return(intpart + "." + fracpart);
}


//
// Build a set of URL arguments
//

function buildURLArguments(lang)
{
 var url= "transport=" + routino.transport;

 for(var marker=1;marker<=vismarkers;marker++)
    if(routino.point[marker].active)
      {
       url=url + ";lon" + marker + "=" + routino.point[marker].lon;
       url=url + ";lat" + marker + "=" + routino.point[marker].lat;
       if(routino.point[marker].search != "")
          url=url + ";search" + marker + "=" + encodeURIComponent(routino.point[marker].search);
      }

 for(var key in routino.profile_highway)
    if(routino.profile_highway[key][routino.transport]!=routino_default.profile_highway[key][routino.transport])
       url=url + ";highway-" + key + "=" + routino.profile_highway[key][routino.transport];

 for(var key in routino.profile_speed)
    if(routino.profile_speed[key][routino.transport]!=routino_default.profile_speed[key][routino.transport])
       url=url + ";speed-" + key + "=" + routino.profile_speed[key][routino.transport];

 for(var key in routino.profile_property)
    if(routino.profile_property[key][routino.transport]!=routino_default.profile_property[key][routino.transport])
       url=url + ";property-" + key + "=" + routino.profile_property[key][routino.transport];

 for(var key in routino.restrictions)
    if(routino.profile_restrictions[key][routino.transport]!=routino_default.profile_restrictions[key][routino.transport])
       url=url + ";" + key + "=" + routino.profile_restrictions[key][routino.transport];

 if(lang && routino.language)
    url=url + ";language=" + routino.language;

 return(url);
}


//
// Build a set of URL arguments for the map location
//

function buildMapArguments()
{
 var lonlat = map.getCenter().clone();
 lonlat.transform(epsg900913,epsg4326);

 var zoom = map.getZoom() + map.minZoomLevel;

 return "lat=" + format5f(lonlat.lat) + ";lon=" + format5f(lonlat.lon) + ";zoom=" + zoom;
}


//
// Update a URL
//

function updateURL(element)     // called from router.html
{
 if(element.id == "permalink_url")
    element.href=location.pathname + "?" + buildURLArguments(true) + ";" + buildMapArguments();

 if(element.id == "visualiser_url")
    element.href="visualiser.html" + "?" + buildMapArguments();

 if(element.id == "edit_url")
    element.href="http://www.openstreetmap.org/edit" + "?" + buildMapArguments();

 if(element.id.match(/^lang_([a-zA-Z-]+)_url$/))
    element.href="router.html" + "." + RegExp.$1 + "?" + buildURLArguments(false) + ";" + buildMapArguments();
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Map handling /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

var map;
var layerMap=[], layerVectors, layerGPX;
var epsg4326, epsg900913;

//
// Initialise the 'map' object
//

function map_init()             // called from router.html
{
 lon =args["lon"];
 lat =args["lat"];
 zoom=args["zoom"];

 // Map URLs and limits are in mapprops.js.

 //
 // Create the map
 //

 epsg4326=new OpenLayers.Projection("EPSG:4326");
 epsg900913=new OpenLayers.Projection("EPSG:900913");

 map = new OpenLayers.Map ("map",
                           {
                            controls:[
                                      new OpenLayers.Control.Navigation(),
                                      new OpenLayers.Control.PanZoomBar(),
                                      new OpenLayers.Control.ScaleLine(),
                                      new OpenLayers.Control.LayerSwitcher()
                                      ],

                            projection: epsg900913,
                            displayProjection: epsg4326,

                            minZoomLevel: mapprops.zoomout,
                            numZoomLevels: mapprops.zoomin-mapprops.zoomout+1,
                            maxResolution: 156543.03390625 / Math.pow(2,mapprops.zoomout),

                            // These two lines are not needed with OpenLayers 2.12
                            units: "m",
                            maxExtent:        new OpenLayers.Bounds(-20037508.34, -20037508.34, 20037508.34, 20037508.34),

                            restrictedExtent: new OpenLayers.Bounds(mapprops.westedge,mapprops.southedge,mapprops.eastedge,mapprops.northedge).transform(epsg4326,epsg900913)
                           });

 // Add map tile layers

 for(var l=0;l < mapprops.mapdata.length;l++)
   {
    layerMap[l] = new OpenLayers.Layer.TMS(mapprops.mapdata[l].label,
                                           mapprops.mapdata[l].tileurl,
                                           {
                                            getURL: limitedUrl,
                                            displayOutsideMaxExtent: true,
                                            buffer: 1
                                           });
    map.addLayer(layerMap[l]);
   }

 // Update the attribution if the layer changes

 map.events.register("changelayer",layerMap,change_attribution_event);

 function change_attribution_event(event)
 {
  for(var l=0;l < mapprops.mapdata.length;l++)
     if(this[l] == event.layer)
        change_attribution(l);
 }

 function change_attribution(l)
 {
  var data_url =mapprops.mapdata[l].attribution.data_url;
  var data_text=mapprops.mapdata[l].attribution.data_text;
  var tile_url =mapprops.mapdata[l].attribution.tile_url;
  var tile_text=mapprops.mapdata[l].attribution.tile_text;

  document.getElementById("attribution_data").innerHTML="<a href=\"" + data_url + "\" target=\"data_attribution\">" + data_text + "</a>";
  document.getElementById("attribution_tile").innerHTML="<a href=\"" + tile_url + "\" target=\"tile_attribution\">" + tile_text + "</a>";
 }

 change_attribution(0);

 // Get a URL for the tile (mostly copied from OpenLayers/Layer/XYZ.js).

 function limitedUrl(bounds)
 {
  var res = map.getResolution();

  var x = Math.round((bounds.left - this.maxExtent.left) / (res * this.tileSize.w));
  var y = Math.round((this.maxExtent.top - bounds.top) / (res * this.tileSize.h));
  var z = map.getZoom() + map.minZoomLevel;

  var limit = Math.pow(2, z);
  x = ((x % limit) + limit) % limit;

  var xyz = {'x': x, 'y': y, 'z': z};
  var url = this.url;

  if (OpenLayers.Util.isArray(url))
    {
     var s = '' + xyz.x + xyz.y + xyz.z;
     url = this.selectUrl(s, url);
    }
        
  return OpenLayers.String.format(url, xyz);
 }

 // Define a GPX layer but don't add it yet

 layerGPX={shortest: null, quickest: null};

 gpx_style={shortest: new OpenLayers.Style({},{strokeWidth: 3, strokeColor: "#00FF00"}),
            quickest: new OpenLayers.Style({},{strokeWidth: 3, strokeColor: "#0000FF"})};

 // Add a vectors layer

 layerVectors = new OpenLayers.Layer.Vector("Markers");
 map.addLayer(layerVectors);

 // A set of markers

 markers={};
 markersmoved=false;
 paramschanged=false;

 for(var marker=1;marker<=mapprops.maxmarkers;marker++)
   {
    markers[marker] = new OpenLayers.Feature.Vector(new OpenLayers.Geometry.Point(0,0),{},
                                                    new OpenLayers.Style({},{externalGraphic: 'icons/marker-' + marker + '-red.png',
                                                                             fillColor: "white",
                                                                             graphicYOffset: -25,
                                                                             graphicWidth: 21,
                                                                             graphicHeight: 25,
                                                                             display: "none"}));

    layerVectors.addFeatures([markers[marker]]);
   }

 // A function to drag the markers

 var drag = new OpenLayers.Control.DragFeature(layerVectors,
                                               {onDrag:     dragMove,
                                                onComplete: dragComplete });
 map.addControl(drag);
 drag.activate();

 // Markers to highlight a selected point

 for(var highlight in highlights)
   {
    highlights[highlight] = new OpenLayers.Feature.Vector(new OpenLayers.Geometry.Point(0,0),{},
                                                          new OpenLayers.Style({},{strokeColor: route_dark_colours[highlight],
                                                                                   fillColor: "white",
                                                                                   pointRadius: 10,
                                                                                   strokeWidth: 4,
                                                                                   fillOpacity: 0,
                                                                                   display: "none"}));

    layerVectors.addFeatures([highlights[highlight]]);
   }

 // A popup for routing results

 for(var popup in popups)
    popups[popup] = createPopup(popup);

 // Set the map centre to the limited range specified

 map.setCenter(map.restrictedExtent.getCenterLonLat(), map.getZoomForExtent(map.restrictedExtent,true));
 map.maxResolution = map.getResolution();

 // Move the map

 if(lon != undefined && lat != undefined && zoom != undefined)
   {
    if(lon<mapprops.westedge) lon=mapprops.westedge;
    if(lon>mapprops.eastedge) lon=mapprops.eastedge;

    if(lat<mapprops.southedge) lat=mapprops.southedge;
    if(lat>mapprops.northedge) lat=mapprops.northedge;

    if(zoom<mapprops.zoomout) zoom=mapprops.zoomout;
    if(zoom>mapprops.zoomin)  zoom=mapprops.zoomin;

    var lonlat = new OpenLayers.LonLat(lon,lat);
    lonlat.transform(epsg4326,epsg900913);

    map.moveTo(lonlat,zoom-map.minZoomLevel);
   }
}


//
// OpenLayers.Control.DragFeature callback for a drag occuring.
//

function dragMove(feature,pixel)
{
 for(var marker in markers)
    if(feature==markers[marker])
       dragSetForm(marker);
}


//
// OpenLayers.Control.DragFeature callback for completing a drag.
//

function dragComplete(feature,pixel)
{
 for(var marker in markers)
    if(feature==markers[marker])
       dragSetForm(marker);
}


//
// Set the feature coordinates in the form after dragging.
//

function dragSetForm(marker)
{
 var lonlat = new OpenLayers.LonLat(markers[marker].geometry.x, markers[marker].geometry.y);
 lonlat.transform(epsg900913,epsg4326);

 var lon=format5f(lonlat.lon);
 var lat=format5f(lonlat.lat);

 formSetCoords(marker,lon,lat);
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Marker handling ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


//
// Toggle a marker on the map.
//

function markerToggleMap(marker) // called from router.html
{
 if(!routino.point[marker].used)
   {
    routino.point[marker].used=true;
    markerCentre(marker);
    markerCoords(marker);
   }

 markerAddRemoveMap(marker,!routino.point[marker].active);
}


//
// Show or hide a marker on the map.
//

function markerAddRemoveMap(marker,active)
{
 if(active)
    markerAddMap(marker);
 else
    markerRemoveMap(marker);
}


//
// Show a marker on the map.
//

function markerAddMap(marker)
{
 clearSearchResult(marker);

 markers[marker].style.display = "";
 routino.point[marker].active=true;
 routino.point[marker].used=true;

 updateIcon(marker);

 markersmoved=true;
}


//
// Remove a marker from the map.
//

function markerRemoveMap(marker)
{
 clearSearchResult(marker);

 markers[marker].style.display = "none";
 routino.point[marker].active=false;

 updateIcon(marker);

 markersmoved=true;
}


//
// Display search string for the marker
//

function markerSearch(marker)   // called from router.html
{
 clearSearchResult(marker);

 document.getElementById("coords" + marker).style.display="none";
 document.getElementById("search" + marker).style.display="";
}


//
// Display coordinates for the marker
//

function markerCoords(marker)   // called from router.html
{
 clearSearchResult(marker);

 document.getElementById("search" + marker).style.display="none";
 document.getElementById("coords" + marker).style.display="";
}


//
// Centre the marker on the map
//

function markerCentre(marker)   // called from router.html
{
 if(!routino.point[marker].used)
    return;

 clearSearchResult(marker);

 var lonlat=map.getCenter().clone();
 lonlat.transform(epsg900913,epsg4326);

 formSetCoords(marker,lonlat.lon,lonlat.lat);
}


//
// Centre the map on the marker
//

function markerRecentre(marker) // called from router.html
{
 if(!routino.point[marker].used)
    return;

 clearSearchResult(marker);

 lon=routino.point[marker].lon;
 lat=routino.point[marker].lat;

 var lonlat = new OpenLayers.LonLat(lon,lat);
 lonlat.transform(epsg4326,epsg900913);

 map.panTo(lonlat);
}


//
// Clear the current marker.
//

function markerRemove(marker)   // called from router.html
{
 clearSearchResult(marker);

 for(var m=marker;m<vismarkers;m++)
    markerCopy(m,m+1);

 markerRemoveForm(vismarkers--);

 if(vismarkers==1)
    markerAddAfter(1);
}


//
// Add a marker before the current one.
//

function markerAddBefore(marker)
{
 clearSearchResult(marker);

 if(vismarkers==mapprops.maxmarkers || marker==1)
    return false;

 markerAddForm(++vismarkers);

 for(var m=vismarkers;m>marker;m--)
    markerCopy(m,m-1);

 markerClearForm(marker-1);
}


//
// Add a marker after the current one.
//

function markerAddAfter(marker) // called from router.html
{
 clearSearchResult(marker);

 if(vismarkers==mapprops.maxmarkers)
    return false;

 markerAddForm(++vismarkers);

 for(var m=vismarkers;m>(marker+1);m--)
    markerCopy(m,m-1);

 markerClearForm(marker+1);
}


//
// Set this marker as the home location.
//

function markerHome(marker)     // called from router.html
{
 if(!routino.point[marker].used)
   {
    markerMoveHome(marker);
   }
 else
   {
    clearSearchResult(marker);

    markerSetClearHome(marker,!routino.point[marker].home);
   }
}


//
// Set this marker as the current location.
//

function markerLocate(marker)   // called from router.html
{
 clearSearchResult(marker);

 if(navigator.geolocation)
    navigator.geolocation.getCurrentPosition(
                                             function(position) {
                                              formSetCoords(marker,position.coords.longitude,position.coords.latitude);
                                              markerAddMap(marker);
                                             });
}


//
// Update an icon to set colours and home or normal marker.
//

function updateIcon(marker)
{
 if(routino.point[marker].home)
   {
    if(routino.point[marker].active)
       document.images["waypoint" + marker].src="icons/marker-home-red.png";
    else
       document.images["waypoint" + marker].src="icons/marker-home-grey.png";

    markers[marker].style.externalGraphic="icons/marker-home-red.png";
   }
 else
   {
    if(routino.point[marker].active)
       document.images["waypoint" + marker].src="icons/marker-" + marker + "-red.png";
    else
       document.images["waypoint" + marker].src="icons/marker-" + marker + "-grey.png";

    markers[marker].style.externalGraphic="icons/marker-" + marker + "-red.png";
   }

 layerVectors.drawFeature(markers[marker]);
}


//
// Move the marker to the home location
//

function markerMoveHome(marker)
{
 if(homelon==null || homelat==null)
    return;

 routino.point[marker].home=true;
 routino.point[marker].used=true;

 formSetCoords(marker,homelon,homelat);
 markerAddMap(marker);
}


//
// Set or clear the home marker icon
//

function markerSetClearHome(marker,home)
{
 var cookie;
 var date = new Date();

 if(home)
   {
    homelat=routino.point[marker].lat;
    homelon=routino.point[marker].lon;

    cookie="Routino-home=lon:" + homelon + ":lat:" + homelat;

    date.setUTCFullYear(date.getUTCFullYear()+5);

    routino.point[marker].home=true;
   }
 else
   {
    homelat=null;
    homelon=null;

    cookie="Routino-home=unset";

    date.setUTCFullYear(date.getUTCFullYear()-1);

    routino.point[marker].home=false;
   }

 document.cookie=cookie + ";expires=" + date.toGMTString();

 updateIcon(marker);

 for(m=1;m<=mapprops.maxmarkers;m++)
    markerCheckHome(m);
}


//
// Check if a marker is the home marker
//

function markerCheckHome(marker)
{
 var home=routino.point[marker].home;

 if(routino.point[marker].lon==homelon && routino.point[marker].lat==homelat)
    routino.point[marker].home=true;
 else
    routino.point[marker].home=false;

 if(home!=routino.point[marker].home)
    updateIcon(marker);
}


//
// Move this marker up.
//

function markerMoveUp(marker)   // called from router.html
{
 if(marker==1)
   {
    for(var m=1;m<vismarkers;m++)
       markerSwap(m,m+1);
   }
 else
    markerSwap(marker,marker-1);
}


//
// Move this marker down.
//

function markerMoveDown(marker) // called from router.html
{
 if(marker==vismarkers)
   {
    for(var m=vismarkers;m>1;m--)
       markerSwap(m,m-1);
   }
 else
    markerSwap(marker,marker+1);
}


//
// Copy a marker from one place to another.
//

function markerCopy(marker1,marker2)
{
 for(var element in routino.point[marker2])
    routino.point[marker1][element]=routino.point[marker2][element];

 document.getElementById("search" + marker1).style.display=document.getElementById("search" + marker2).style.display;

 document.getElementById("coords" + marker1).style.display=document.getElementById("coords" + marker2).style.display;

 document.forms["form"].elements["search" + marker1].value=document.forms["form"].elements["search" + marker2].value;

 formSetCoords(marker1,routino.point[marker1].lon,routino.point[marker1].lat);

 markerAddRemoveMap(marker1,routino.point[marker1].active);
}


//
// Swap a pair of markers.
//

function markerSwap(marker1,marker2)
{
 for(var element in routino.point[marker2])
   {
    var temp=routino.point[marker1][element];
    routino.point[marker1][element]=routino.point[marker2][element];
    routino.point[marker2][element]=temp;
   }

 var search_display=document.getElementById("search" + marker1).style.display;
 document.getElementById("search" + marker1).style.display=document.getElementById("search" + marker2).style.display;
 document.getElementById("search" + marker2).style.display=search_display;

 var coords_display=document.getElementById("coords" + marker1).style.display;
 document.getElementById("coords" + marker1).style.display=document.getElementById("coords" + marker2).style.display;
 document.getElementById("coords" + marker2).style.display=coords_display;

 var search_value=document.forms["form"].elements["search" + marker1].value;
 document.forms["form"].elements["search" + marker1].value=document.forms["form"].elements["search" + marker2].value;
 document.forms["form"].elements["search" + marker2].value=search_value;

 formSetCoords(marker1,routino.point[marker1].lon,routino.point[marker1].lat);
 formSetCoords(marker2,routino.point[marker2].lon,routino.point[marker2].lat);

 markerAddRemoveMap(marker1,routino.point[marker1].active);
 markerAddRemoveMap(marker2,routino.point[marker2].active);
}


//
// Reverse the markers.
//

function markersReverse()       // called from router.html
{
 for(var marker=1;marker<=vismarkers/2;marker++)
    markerSwap(marker,vismarkers+1-marker);
}


//
// Close the loop.
//

function markersLoop()          // called from router.html
{
 if(vismarkers==mapprops.maxmarkers)
    return false;

 if(routino.point[vismarkers].lon==routino.point[1].lon && routino.point[vismarkers].lat==routino.point[1].lat)
    return false;

 if(routino.point[vismarkers].used)
    markerAddForm(++vismarkers);

 markerCopy(vismarkers,1);
}


//
// Display the form for a marker
//

function markerAddForm(marker)
{
 document.getElementById("waypoint" + marker).style.display="";
}


//
// Hide the form for a marker
//

function markerRemoveForm(marker)
{
 document.getElementById("waypoint" + marker).style.display="none";

 markerClearForm(marker);
}


//
// Clear the form for a marker
//

function markerClearForm(marker)
{
 markerRemoveMap(marker);
 markerCoords(marker);

 formSetCoords(marker,"","");
 formSetSearch(marker,"");

 updateIcon(marker);

 routino.point[marker].used=false;
 routino.point[marker].home=false;
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Route results handling ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

var route_light_colours={shortest: "#60C060", quickest: "#6060C0"};
var route_dark_colours ={shortest: "#408040", quickest: "#404080"};

var highlights={shortest: null, quickest: null};
var popups={shortest: null, quickest: null};
var routepoints={shortest: {}, quickest: {}};
var gpx_style={shortest: null, quickest: null};

//
// Zoom to a specific item in the route
//

function zoomTo(type,line)
{
 var lonlat = new OpenLayers.LonLat(routepoints[type][line].lon,routepoints[type][line].lat);
 lonlat.transform(epsg4326,epsg900913);

 map.moveTo(lonlat,map.numZoomLevels-2);
}


//
// Highlight a specific item in the route
//

function highlight(type,line)
{
 if(line==-1)
   {
    highlights[type].style.display = "none";

    drawPopup(popups[type],null);
   }
 else
   {
    // Marker

    var lonlat = new OpenLayers.LonLat(routepoints[type][line].lon,routepoints[type][line].lat);
    lonlat.transform(epsg4326,epsg900913);

    highlights[type].move(lonlat);

    if(highlights[type].style.display = "none")
       highlights[type].style.display = "";

    // Popup

    drawPopup(popups[type],"<table>" + routepoints[type][line].html + "</table>");
   }

 layerVectors.drawFeature(highlights[type]);
}


//
// Create a popup - not using OpenLayers because want it fixed on screen not fixed on map.
//

function createPopup(type)
{
 var popup=document.createElement('div');

 popup.className = "popup";

 popup.innerHTML = "<span></span>";

 popup.style.display = "none";

 popup.style.position = "fixed";
 popup.style.top = "-4000px";
 popup.style.left = "-4000px";
 popup.style.zIndex = "100";

 popup.style.padding = "5px";

 popup.style.opacity=0.85;
 popup.style.backgroundColor=route_light_colours[type];
 popup.style.border="4px solid " + route_dark_colours[type];

 document.body.appendChild(popup);

 return(popup);
}


//
// Draw a popup - not using OpenLayers because want it fixed on screen not fixed on map.
//

function drawPopup(popup,html)
{
 if(html==null)
   {
    popup.style.display="none";
    return;
   }

 if(popup.style.display=="none")
   {
    var map_div=document.getElementById("map");

    popup.style.left  =map_div.offsetParent.offsetLeft+map_div.offsetLeft+60 + "px";
    popup.style.top   =                                map_div.offsetTop +30 + "px";
    popup.style.width =map_div.clientWidth-100 + "px";

    popup.style.display="";
   }

 popup.innerHTML=html;
}


//
// Remove a GPX trace
//

function removeGPXTrace(type)
{
 map.removeLayer(layerGPX[type]);
 layerGPX[type].destroy();
 layerGPX[type]=null;

 displayStatus(type,"no_info");

 document.getElementById(type + "_links").style.display = "none";

 document.getElementById(type + "_route").innerHTML = "";

 hideshow_hide(type);
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Server handling ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//
// Display data statistics
//

function displayStatistics() // called from router.html
{
 // Use AJAX to get the statistics

 OpenLayers.Request.GET({url: "statistics.cgi", success: runStatisticsSuccess});
}


//
// Success in running data statistics generation.
//

function runStatisticsSuccess(response)
{
 document.getElementById("statistics_data").innerHTML="<pre>" + response.responseText + "</pre>";
 document.getElementById("statistics_link").style.display="none";
}


//
// Submit form - perform the routing
//

function findRoute(type) // called from router.html
{
 tab_select("results");

 hideshow_hide('help_options');
 hideshow_hide('shortest');
 hideshow_hide('quickest');

 displayStatus("result","running");

 var url="router.cgi" + "?" + buildURLArguments(true) + ";type=" + type;

 // Destroy the existing layer(s)

 if(markersmoved || paramschanged)
   {
    if(layerGPX.shortest!=null)
       removeGPXTrace("shortest");
    if(layerGPX.quickest!=null)
       removeGPXTrace("quickest");
    markersmoved=false;
    paramschanged=false;
   }
 else if(layerGPX[type]!=null)
    removeGPXTrace(type);

 // Use AJAX to run the router

 routing_type=type;

 OpenLayers.Request.GET({url: url, success: runRouterSuccess, failure: runRouterFailure});
}


//
// Success in running router.
//

function runRouterSuccess(response)
{
 var lines=response.responseText.split('\n');

 var uuid=lines[0];
 var success=lines[1];

 var link;

 // Update the status message

 if(success=="ERROR")
   {
    displayStatus("result","error");
    hideshow_show('help_route');

    link=document.getElementById("router_log_error");
    link.href="results.cgi?uuid=" + uuid + ";type=router;format=log";

    return;
   }
 else
   {
    displayStatus("result","complete");
    hideshow_hide('help_route');

    link=document.getElementById("router_log_complete");
    link.href="results.cgi?uuid=" + uuid + ";type=router;format=log";
   }

 // Update the routing result message

 link=document.getElementById(routing_type + "_html");
 link.href="results.cgi?uuid=" + uuid + ";type=" + routing_type + ";format=html";

 link=document.getElementById(routing_type + "_gpx_track");
 link.href="results.cgi?uuid=" + uuid + ";type=" + routing_type + ";format=gpx-track";

 link=document.getElementById(routing_type + "_gpx_route");
 link.href="results.cgi?uuid=" + uuid + ";type=" + routing_type + ";format=gpx-route";

 link=document.getElementById(routing_type + "_text_all");
 link.href="results.cgi?uuid=" + uuid + ";type=" + routing_type + ";format=text-all";

 link=document.getElementById(routing_type + "_text");
 link.href="results.cgi?uuid=" + uuid + ";type=" + routing_type + ";format=text";

 links=document.getElementById(routing_type + "_links").style.display = "";

 // Add a GPX layer

 var url="results.cgi?uuid=" + uuid + ";type=" + routing_type + ";format=gpx-track";

 layerGPX[routing_type] = new OpenLayers.Layer.Vector("GPX (" + routing_type + ")",
                                                      {
                                                       protocol:   new OpenLayers.Protocol.HTTP({url: url, format: new OpenLayers.Format.GPX()}),
                                                       strategies: [new OpenLayers.Strategy.Fixed()],
                                                       style:      gpx_style[routing_type],
                                                       projection: map.displayProjection
                                                      });

 map.addLayer(layerGPX[routing_type]);

 hideshow_show(routing_type);

 displayResult(routing_type,uuid);
}


//
// Failure in running router.
//

function runRouterFailure(response)
{
 displayStatus("result","failed");
}


//
// Display the status
//

function displayStatus(type,subtype,content)
{
 var child=document.getElementById(type + "_status").firstChild;

 do
   {
    if(child.id != undefined)
       child.style.display="none";

    child=child.nextSibling;
   }
 while(child != undefined);

 var chosen_status=document.getElementById(type + "_status_" + subtype);

 chosen_status.style.display="";

 if(content != null)
    chosen_status.innerHTML=content;
}


//
// Display the route
//

function displayResult(type,uuid)
{
 routing_type = type;

 // Add the route

 var url="results.cgi?uuid=" + uuid + ";type=" + routing_type + ";format=html";

 // Use AJAX to get the route

 OpenLayers.Request.GET({url: url, success: getRouteSuccess, failure: getRouteFailure});
}


//
// Success in getting route.
//

function getRouteSuccess(response)
{
 var lines=response.responseText.split('\n');

 routepoints[routing_type]=[];

 var points=routepoints[routing_type];

 var table=0;
 var point=0;
 var total_table,total_word;

 for(var line=0;line<lines.length;line++)
   {
    var thisline=lines[line];

    if(table==0)
      {
       if(thisline.match('<table>'))
          table=1;
       else
          continue;
      }

    if(thisline.match('</table>'))
       break;

    if(thisline.match('<tr class=\'([a-z])\'>'))
      {
       var rowtype=RegExp.$1;

       if(rowtype=='c')
         {
          thisline.match('<td class=\'r\'> *([-0-9.]+) *([-0-9.]+)');
          points[point]={lat: Number(RegExp.$1), lon: Number(RegExp.$2), html: "", highway: "", distance: "", total: ""};

          point++;
         }
       else if(rowtype=='n')
         {
          points[point-1].html += thisline;
         }
       else if(rowtype=='s')
         {
          thisline.match('<span class=\'h\'>([^<]+)</span>');
          points[point-1].highway = RegExp.$1;

          thisline.match('<span class=\'d\'>([^<]+)</span>');
          points[point-1].distance = RegExp.$1;

          thisline.match('(<span class=\'j\'>[^<]+</span>)');
          points[point-1].total = RegExp.$1;

          thisline.match('^(.*).<span class=\'j\'>');

          points[point-1].html += RegExp.$1;
         }
       else if(rowtype=='t')
         {
          points[point-1].html += thisline;

          thisline.match('^(.*<td class=\'r\'>)');
          total_table = RegExp.$1;

          thisline.match('<td class=\'l\'>([^<]+)<');
          total_word = RegExp.$1;

          thisline.match('<span class=\'j\'>([^<]+)</span>');
          points[point-1].total = RegExp.$1;
         }
      }
   }

 displayStatus(routing_type,"info",points[point-1].total.bold());

 var result="<table onmouseout='highlight(\"" + routing_type + "\",-1)'>";

 for(var p=0;p<point-1;p++)
   {
    points[p].html += total_table + points[p].total;

    result=result + "<tr onclick='zoomTo(\"" + routing_type + "\"," + p + ")'" +
                    " onmouseover='highlight(\"" + routing_type + "\"," + p + ")'>" +
                    "<td class='distance' title='" + points[p].distance + "'>#" + (p+1) +
                    "<td class='highway'>" + points[p].highway;
   }

 result=result + "<tr onclick='zoomTo(\"" + routing_type + "\"," + p + ")'" +
                 " onmouseover='highlight(\"" + routing_type + "\"," + p + ")'>" +
                 "<td colspan='2'>" + total_word + " " + points[p].total;

 result=result + "</table>";

 document.getElementById(routing_type + "_route").innerHTML=result;
}


//
// Failure in getting route.
//

function getRouteFailure(response)
{
 document.getElementById(routing_type + "_route").innerHTML = "";
}


//
// Perform a search
//

function DoSearch(marker)
{
 // Use AJAX to get the search result

 var search=routino.point[marker].search;

 var bounds=map.getExtent().clone();
 bounds.transform(epsg900913,epsg4326);

 var url="search.cgi?marker=" + marker +
         ";left=" + format5f(bounds.left) +
         ";top="  + format5f(bounds.top) +
         ";right="  + format5f(bounds.right) +
         ";bottom=" + format5f(bounds.bottom) +
         ";search=" + encodeURIComponent(search);

 OpenLayers.Request.GET({url: url, success: runSearchSuccess});
}


var searchresults=[];

//
// Success in running search.
//

function runSearchSuccess(response)
{
 var lines=response.responseText.split('\n');

 var marker=lines[0];
 var cpuinfo=lines[1];  // not used
 var message=lines[2];  // not used

 if(message != "")
   {
    alert(message);
    return;
   }

 searchresults[marker]=[];

 for(var line=3;line<lines.length;line++)
   {
    var thisline=lines[line];

    if(thisline=="")
       break;

    thisline.match('([-.0-9]+) ([-.0-9]+) (.*)');

    searchresults[marker][searchresults[marker].length]={lat: Number(RegExp.$1), lon: Number(RegExp.$2), name: RegExp.$3};
   }

 if(searchresults[marker].length==1)
   {
    formSetSearch(marker,searchresults[marker][0].name);
    formSetCoords(marker,searchresults[marker][0].lon,searchresults[marker][0].lat);
    markerAddMap(marker);
   }
 else
   {
    var results=document.getElementById("searchresults" + marker);

    var innerHTML="<td colspan=\"3\">";

    for(var n=0;n<searchresults[marker].length;n++)
      {
       if(n>0)
          innerHTML+="<br>";

       innerHTML+="<a href=\"#\" onclick=\"choseSearchResult(" + marker + "," + n + ")\">" +
                  searchresults[marker][n].name +
                  "</a>";
      }

    results.innerHTML=innerHTML;

    results.style.display="";
   }
}


//
// Display search results.
//

function choseSearchResult(marker,n)
{
 if(n>=0)
   {
    formSetSearch(marker,searchresults[marker][n].name);
    formSetCoords(marker,searchresults[marker][n].lon,searchresults[marker][n].lat);
    markerAddMap(marker);
   }
}


//
// Clear search results.
//

function clearSearchResult(marker)
{
 document.getElementById("searchresults" + marker).style.display="none";
}
