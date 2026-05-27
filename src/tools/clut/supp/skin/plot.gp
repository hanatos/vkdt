# plot for [i=0:3] 'test.dat' u 1:2+3*i w l lw 4, for [i=0:3] '' u 1:3+3*i w l lw 1, for[i=0:3] '' u 1:4+3*i w lp lw 2, 'plot-skin-avg.dat' u 1:($2*4) w l lw 1
set term pdf
set output "eval.pdf"

unset key
a=3 # offset for model0
# a=4 # offset for model1

plot for [i=0:4] 'test.dat' u 1:2+3*i w l lw 1 lc i t 'ref', \
     for [i=0:4] '' u 1:3+3*i w l lw 3 lc i t 'model0'

# set yrange [0:4]
plot for [i=0:9] 'test.dat' u 1:2+3*i w l lw 1 lc i t 'ref', \
     for [i=0:9] '' u 1:a+3*i w l lw 3 lc i t 'model1'
# set yrange [-2.0:2.0]
plot for [i=0:9] 'test.dat' u 1:(column(2+3*i)-column(a+3*i)) w l lw 2 lc i t 'diff'

# set yrange [0:4]
plot for [i=10:19] 'test.dat' u 1:2+3*i w l lw 1 lc i t 'ref', \
     for [i=10:19] '' u 1:a+3*i w l lw 3 lc i t 'model1'
# set yrange [-2.0:2.0]
plot for [i=10:19] 'test.dat' u 1:(column(2+3*i)-column(a+3*i)) w l lw 2 lc i t 'diff'

# set yrange [0:4]
plot for [i=20:29] 'test.dat' u 1:2+3*i w l lw 1 lc i t 'ref', \
     for [i=20:29] '' u 1:a+3*i w l lw 3 lc i t 'model1'
# set yrange [-2.0:2.0]
plot for [i=20:29] 'test.dat' u 1:(column(2+3*i)-column(a+3*i)) w l lw 2 lc i t 'diff'
