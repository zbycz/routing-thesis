#!/usr/bin/gnuplot

############### common
reset
set xtics add 0.001
set ytics offset -3 add 0.003
set ztics add 40
set style data lines


set encoding utf8
set zlabel "Výška [m]" rotate offset 1
set xlabel "Y"
set ylabel "X"

set hidden3d
set view 41, 325
set key at screen 0.88,screen 0.9


set zrange[100:280]

#původní: set xrange [50.872:50.876]
#set xrange [50.871:50.877] #smrštění longitude
#set xrange [:] reverse
#set view 52, 31
#původní: set yrange [14.235:14.253]
#set yrange [14.235:14.250]


set style line 4 pointtype 1 pointsize 2 linecolor '#000000' lw 3
set label 1 "" at 50.873816,14.238229,168 point ls 4 front


########################### 3D-srtm1-s plochami.pdf
#set terminal pdf
#set output "3D-srtm1-s plochami.pdf"
#splot '3D-srtm1-s plochami.dat', \
#      '3D-trasa.cor' u 1:2:4



trasa = "trasa + SRTM výšky

########################### 3D-aster1.pdf
set terminal pdf
set output "3D-aster1.pdf"
splot '3D-aster1.dat' t "ASTER GDEM 1x1 arcsec", \
      '3D-trasa.cor' u 1:2:4 t trasa, '' u 1:2:3 t "trasa + GPS výšky"


########################### 3D-srtm3.pdf
set terminal pdf
set output "3D-srtm3.pdf"
splot '3D-srtm3.dat' t "SRTM3 3x3 arcsec", \
      '3D-trasa.cor' u 1:2:4 t '', '' u 1:2:3 t ""

set terminal pdf
set output "3D-srtm3x1.pdf"
splot '3D-srtm3x1.dat' t "SRTM3 3x1 arcsec", \
      '3D-trasa.cor' u 1:2:4 t '', '' u 1:2:3 t ""

set terminal wxt


