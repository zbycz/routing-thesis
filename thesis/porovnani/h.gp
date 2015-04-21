
reset
set terminal pdf
set output "ele-h.pdf"
set style data lines

plot 'gpx_2hau/bbbike.gpx.dat' u 1:4 t 'BBBike', \
'gpx_2hau/brouter.gpx.dat' u 1:4 t 'BRouter', \
'gpx_2hau/mapy-cz.gpx.dat' u 1:4 t 'Mapy.cz', \
'gpx_2hau/openrouteservice-preffered-way.gpx.dat' u 1:4 t 'OpenRouteService', \
'gpx_2hau/routino-hills0.gpx.dat' u 1:4 t 'Routino hills0.gpx', \
'gpx_2hau/routino-hills100.gpx.dat' u 1:4 t 'Routino hills100.gpx', \
'gpx_2hau/yours-h.gpx.dat' u 1:4 t 'YOURS'


set terminal pdf linewidth 3
set output "ll-h.pdf"

set multiplot

#As the background picture's size is 800x410,
#we choose xrange and yrange of these values
unset tics
unset border
set lmargin at screen 0.175
set rmargin at screen 0.9
set bmargin at screen 0.15
set tmargin at screen 0.9

plot "map-h.png" binary filetype=png w rgbimage

#The x and y range of the population data file
set xrange [:]
set yrange [:]
set border
set tics out nomirror scale 2
set mxtics 5
set key left
set style data lines
set nokey

plot 'gpx_2hau/bbbike.gpx.dat' u 3:2 t 'BBBike', \
'gpx_2hau/brouter.gpx.dat' u 3:2 t 'BRouter', \
'gpx_2hau/routino-hills0.gpx.dat' u 3:2 t 'Routino hills0.gpx', \
'gpx_2hau/routino-hills100.gpx.dat' u 3:2 t 'Routino hills100.gpx'

unset multiplot
set terminal wxt
