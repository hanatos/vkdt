set term pdf
set output 'streamlines-wd.pdf'
set key off
# plot lines for all equal lambda stream lines in width/slope space:

set title "cie chromaticity streamlines, following gauss gradients"
set xrange [0:1]
set yrange [0:1]
set size square
plot for [i=2:38] 'circle.dat' u 1:($4 == i ? $2 : 1/0) w l lw 2

set title "width/slope of u shapes (purple)"
set xrange [200:500]
set yrange [0.0:0.06]
set arrow from 238,0 to 238,0.062 nohead lc rgb "black"
plot for [i=2:38] 'circle.dat' u 6:($4 == i ? $7 : 1/0) w l lw 2, \
     0.003+8./(x-210)**1.65 w l lw 4 lc rgb "red", \
     .8/(x-310) w l lw 4 lc rgb "red"

set title "width/slope of n shapes (spectral)"
set yrange [-0.003:-0.0003]
set xrange [50:500]
plot for [i=2:38] 'circle.dat' u 6:($4 == i ? $7 : 1/0) w l lw 2, \
     -0.15/(x-20) w l lw 4 lc rgb "red"
