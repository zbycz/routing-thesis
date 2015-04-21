set encoding utf8
set style data lines

set terminal pdf
set output 'porovnani.pdf'

set style line 1 linecolor rgbcolor "#00ff00" linewidth 6 
set style line 2 linecolor rgbcolor "#ff00ff" linewidth 6
set style line 3 linecolor rgbcolor "#ff0000" linewidth 6

set xdata time
set timefmt "%H%M%S"
set xlabel "Čas [min]"
set ylabel "Výška [m]"
set format x "%M"
	

set datafile separator ";"

plot "porovnani-interpolace.csv" u 1:2 title "gps" with lines linestyle 1, \
   "" u 1:4 title "NN" with l linestyle 2

plot "porovnani-interpolace.csv" u 1:2 title "gps" with lines linestyle 1, \
   "" u 1:3 title "interpolace" with lines linestyle 3

