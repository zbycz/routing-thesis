////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Routino map properties /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

var mapprops={ // contains all properties for the map to be displayed.

 // Default configuration:
 // UK coordinate range
 // West -11.0, South 49.5, East 2.0, North 61.0
 // Zoom level 4 to 15

 // EDIT THIS below to change the visible map limits

    westedge:  -11.0,          // Minimum longitude (degrees)
    eastedge:    2.0,          // Maximum longitude (degrees)

    southedge:  49.5,          // Minimum latitude (degrees)
    northedge:  61.0,          // Maximum latitude (degrees)

    zoomout:       4,          // Minimum zoom
    zoomin:       15,          // Maximum zoom

 // EDIT THIS above to change the visible map limits


 // EDIT THIS below to change the map URL(s) and copyright notices

    mapdata: [
        {
            label:    "OpenStreetMap",
            tileurl:  ["http://a.tile.openstreetmap.org/${z}/${x}/${y}.png",
                       "http://b.tile.openstreetmap.org/${z}/${x}/${y}.png",
                       "http://c.tile.openstreetmap.org/${z}/${x}/${y}.png"],
            attribution: {
                          data_url:  "http://www.openstreetmap.org/copyright",
                          data_text: "© OpenStreetMap contributors",
                          tile_url:  "http://www.openstreetmap.org/copyright",
                          tile_text: "© OpenStreetMap"
                         }
        },
        {
            label:    "MapQuest",
            tileurl:  ["http://otile1.mqcdn.com/tiles/1.0.0/map/${z}/${x}/${y}.jpg",
                       "http://otile2.mqcdn.com/tiles/1.0.0/map/${z}/${x}/${y}.jpg",
                       "http://otile3.mqcdn.com/tiles/1.0.0/map/${z}/${x}/${y}.jpg",
                       "http://otile4.mqcdn.com/tiles/1.0.0/map/${z}/${x}/${y}.jpg"],
            attribution: {
                          data_url:  "http://www.openstreetmap.org/copyright",
                          data_text: "© OpenStreetMap contributors",
                          tile_url:  "http://www.mapquest.com/",
                          tile_text: "© MapQuest <img src=\"http://developer.mapquest.com/content/osm/mq_logo.png\">"
                         }
        }
    ],

 // EDIT THIS above to change the map URL(s) and copyright notices


 // EDIT THIS below to change the maximum number of markers

 // The number of waypoints to include in the HTML
    maxmarkers: 9

 // EDIT THIS above to change the maximum number of markers

}; // end of map properties
