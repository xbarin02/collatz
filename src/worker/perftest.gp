reset

set terminal pdf enhanced dashed linewidth 3 size 10cm,6cm font 'Serif,14' dashlength 2

set output "perftest.pdf"

# set key bottom

set ylabel "seconds"
# set format y "%.0s%c"
set yrange [0:120]

set xlabel "starting magnitude × 2^{32}"
set format x "2^{%.0f}"
set xrange [0:64]
set xtic (0,8,16,24,32,40,48,56,64)

plot "usertime-32-dabler.txt" using ($1):($3) with lines linewidth 2 title "time per block"
