# plot width vs slope:
set xtics ("200" 200,"500" 500)
set yrange [0.001:100]
set xrange [190:500]
set cbrange [360:830]
set xlabel 'width'
set ylabel 'log slope'
set logscale y
plot 'dat' u 2:3:1 w p ls 7 ps 0.5 palette z
