#!/bin/sh -x

# This script can download either from GeoFabrik or Cloudmade.


# EDIT THIS to set the names of the files to download from GeoFabrik.
files="europe/great_britain.osm.bz2 europe/ireland.osm.bz2 europe/isle_of_man.osm.bz2"
server="download.geofabrik.de/openstreetmap"

## EDIT THIS to set the names of the files to download from Cloudmade.
#files="europe/northern_europe/united_kingdom/united_kingdom.osm.bz2 europe/northern_europe/ireland/ireland.osm.bz2"
#server="downloads.cloudmade.com"


# Download the files

for file in $files; do
   wget -N http://$server/$file
done


# Process the data

../bin/planetsplitter --errorlog *.osm.bz2
