
reset
set style data lines


splot 'gpx_haunspaulka/bbbike-prag.gpx.dat' u 3:2:4 t 'BBBike',  \
'gpx_haunspaulka/brouter.gpx.dat' u 3:2:4 t 'BRouter', \
'gpx_haunspaulka/openrouteservice-preffered-way.gpx.dat' u 3:2:4 t 'OpenRouteService', \
'gpx_haunspaulka/routino-hills-0.gpx.dat' u 3:2:4 t 'Routino hills-0.gpx', \
'gpx_haunspaulka/routino-hills-100.gpx.dat' u 3:2:4 t 'Routino hills-100.gpx', \
'gpx_haunspaulka/yours.gpx.dat' u 3:2:4 t 'YOURS'







