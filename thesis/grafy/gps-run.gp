#!/usr/bin/gnuplot
reset
set encoding utf8


set label 1 "" at 14.516989,49.866331 point lt 7 lw 7 front


set yrange[49.8661:49.8666]
set xrange[14.5164:14.5176]

set style line 1 linecolor rgb "black" linewidth 5 
set arrow 1 from 14.5165,49.86615 to 14.51664,49.86615 nohead ls 1
set label 2 "10 m" at 14.5165,49.86617 nopoint 


set terminal pdf
set output "gps.pdf"

plot 'gps-v-budove.dat' u 2:1 w points t 'GPS záznam', 'gps-budova.dat' u 2:1 w lines t 'Budova'

set terminal wxt
