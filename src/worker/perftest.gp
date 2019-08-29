reset

set terminal pdf enhanced dashed linewidth 3 size 10cm,6cm font 'Serif,14' dashlength 2

set output "perftest.pdf"

# set key bottom

set ylabel "seconds"
# set format y "%.0s%c"
set yrange [0:120]

set xlabel "starting magnitude × 2^{32}"
set format x "2^{%.0f}"
set xrange [0:63]

plot "usertime-32-dabler.txt" using ($1):($3) with lines linewidth 2 title "time per 32-bit block"
