set term pdf
set output 'streamlines-wd.pdf'
set key off
# plot lines for all equal lambda stream lines in width/slope space:

set title "cie chromaticity streamlines, following gauss gradients"
set xrange [0:1]
set yrange [0:1]
set size square
plot for [i=2:38] 'circle.dat' u 1:($4 == i ? $2 : 1/0) w l lw 4

set title "width/slope of u shapes (purple)"
set xrange [200:500]
set yrange [0.0:0.03]
plot for [i=2:38] 'circle.dat' u 6:($4 == i ? $7 : 1/0) w l lw 4

set title "width/slope of n shapes (spectral)"
set yrange [-0.001:-0.0003]
replot
