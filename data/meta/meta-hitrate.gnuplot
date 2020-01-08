set term postscript eps enhanced font "Times-Roman,36"
# set term wxt enhanced font "Times-Roman,18"

set datafile separator tab

set xlabel " " offset 0, 0.1
set x2label "Purge threshold"
set ylabel "Pull threshold" 
set xtics rotate by 90 left offset 0,-2

set output output_file

set yrange [5.50:-0.50]
set pm3d map
set palette defined (0 0.80 0.456 0.40, 0.5 0.90 0.486 0.45, 2 1 0.945 0.808, 3 0.357 0.725 0.549, 3.2 0.247 0.58 0.424)

set format cb "%.2f%%"
set cbtics data_min,data_step,data_max font "Times-Roman,36"


set size square
set key off
plot input_file matrix rowheaders columnheaders using 1:2:3 with image
# pause 5