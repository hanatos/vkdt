#!/bin/bash

# cat munsell.dat | awk '{print $1}' | sort | uniq
hue="10B 10BG 10G 10GY 10P 10PB 10R 10RP 10Y 10YR \
2.5B 2.5BG 2.5G 2.5GY 2.5P 2.5PB 2.5R 2.5RP 2.5Y 2.5YR \
5B 5BG 5G 5GY 5P 5PB 5R 5RP 5Y 5YR \
7.5B 7.5BG 7.5G 7.5GY 7.5P 7.5PB 7.5R 7.5RP 7.5Y 7.5YR"
chroma="2 4 6 8 10 12"


cat > munsell.gp << EOF
set term pdf
set output 'munsell.pdf'
set size square
plot \\
EOF
# input
for h in $hue
do
  cat >> munsell.gp << EOF
"<(awk '\$1 == \"$h\" { print \$0 }' ../data/munsell.dat)" u 4:5 w lp pt 7 ps 0.1 lw 1 notitle, \\
EOF
done
# chroma ~ellipses
cat >> munsell.gp << EOF

plot \\
EOF
# input
for c in $chroma
do
  cat >> munsell.gp << EOF
"<(awk '\$3 == \"$c\" { print \$0 }' ../data/munsell.dat)" u 4:5 w lp pt 7 ps 0.1 lw 1 notitle, \\
EOF
done

# # wavelengths
# V=5
# cat >> munsell.gp << EOF
# 
# set xrange [400:700]
# plot \\
# EOF
# for h in $hue
# do
#   cat >> munsell.gp << EOF
# "<(awk '{ if( \$1 == \"$h\" && \$2 == \"$V\") print \$0 }' ../data/munsell.dat)" u 6:3 w lp pt 7 ps .1 lw 1 notitle, \\
# EOF
# done
gnuplot munsell.gp

