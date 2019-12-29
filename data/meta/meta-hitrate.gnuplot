set title plotname
set xlabel "Purge threshold"
set ylabel "Pull threshold"

set term eps font "Times-Roman,13"
set output sprintf("%s.eps", filename)

set yrange [5.5:-0.5]
set pm3d map
set palette defined (0 0.80 0.456 0.40, 0.5 0.90 0.486 0.45, 2 1 0.945 0.808, 3 0.357 0.725 0.549, 3.2 0.247 0.58 0.424)
unset colorbox
set size square
set key off
set datafile separator tab
plot filename matrix rowheaders columnheaders using 1:2:3 with image
