
reset
set terminal pdf
set output "ele-z.pdf"
set style data lines

plot 'gpx_zizkov/bbbike.gpx.dat' u 1:4 t 'BBBike', \
'gpx_zizkov/brouter.gpx.dat' u 1:4 t 'BRouter', \
'gpx_zizkov/mapy-cz.gpx.dat' u 1:4 t 'Mapy.cz', \
'gpx_zizkov/routino-hills-0.gpx.dat' u 1:4 t 'Routino hills-0.gpx', \
'gpx_zizkov/routino-hills-1.gpx.dat' u 1:4 t 'Routino hills-1.gpx', \
'gpx_zizkov/routino-hills-100.gpx.dat' u 1:4 t 'Routino hills-100.gpx', \
'gpx_zizkov/yours-z.gpx.dat' u 1:4 t 'YOURS'

set terminal pdf linewidth 3
set output "ll-z.pdf"

set multiplot

#As the background picture's size is 800x410,
#we choose xrange and yrange of these values
unset tics
unset border
set lmargin at screen 0.175
set rmargin at screen 0.9
set bmargin at screen 0.15
set tmargin at screen 0.9

plot "map-z.png" binary filetype=png w rgbimage

#The x and y range of the population data file
set xrange [:]
set yrange [:]
set border
set tics out nomirror scale 2
set mxtics 5
set key left
set style data lines
set nokey


plot 'gpx_zizkov/bbbike.gpx.dat' u 3:2 t 'BBBike', \
'gpx_zizkov/brouter.gpx.dat' u 3:2 t 'BRouter', \
'gpx_zizkov/mapy-cz.gpx.dat' u 3:2 t 'Mapy.cz', \
'gpx_zizkov/routino-hills-0.gpx.dat' u 3:2 t 'Routino hills-0.gpx', \
'gpx_zizkov/routino-hills-1.gpx.dat' u 3:2 t 'Routino hills-1.gpx', \
'gpx_zizkov/routino-hills-100.gpx.dat' u 3:2 t 'Routino hills-100.gpx', \
'gpx_zizkov/yours-z.gpx.dat' u 3:2 t 'YOURS'

unset multiplot
set terminal wxt

