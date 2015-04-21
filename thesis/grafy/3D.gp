#/bin/bash

############### common

set xtics add 0.001
set ytics offset -3 add 0.003
set ztics add 40

set hidden3d
set view 41, 325
set zrange[100:280]


########################### 3D-srtm1-s plochami.pdf
set terminal pdf
set output "3D-srtm1-s plochami.pdf"
splot '3D-srtm1-s plochami.dat' w lines, '3D-trasa.cor' u 1:2:4 w l



########################### 3D-aster1.pdf
set terminal pdf
set output "3D-aster1.pdf"
splot '3D-aster1.dat' w lines, '3D-trasa.cor' u 1:2:4 w l


########################### 3D-srtm3.pdf
set terminal pdf
set output "3D-srtm3.pdf"
splot '3D-srtm3.dat' w lines, '3D-trasa.cor' u 1:2:4 w l

set terminal pdf
set output "3D-srtm3x1.pdf"
splot '3D-srtm3x1.dat' w lines, '3D-trasa.cor' u 1:2:4 w l

set terminal wxt








#okoř
splot  'porovnani-3d.dat' u 1:2:3 with lines, '' u 1:2:4 with lines, '' u 1:2:5 with lines


#hřensko
splot  '3D-trasa.cor' u 1:2:3 with lines, '' u 1:2:4 with lines, '3D-srtm.dat' with points


splot '3D.dat' with points lt 1 pt 2, \
 '3D.dat' with lines, \
 







set style line 1 linecolor rgbcolor "#00ff00" linewidth 6 
set style line 2 linecolor rgbcolor "#ff00ff" linewidth 6
set style line 3 linecolor rgbcolor "#ff0000" linewidth 6
set hidden3d
splot '3D-srtm.dat' w lines, '3D-trasa.cor' u 1:2:3 with lines ls 2, '' u 1:2:4 with lines ls 1



set table "griddata"
set dgrid3d 11,4
set style data lines
splot "3D-srtm.dat"




set terminal pdf
set output 'porovnani.pdf'

set angles degrees
set mapping cylindrical
set parametric

#reset
set title "3D version using cylindrical coordinate system"
set ticslevel 0.0
set view 30,56,0.98
set zrange[-60:0]
unset key

