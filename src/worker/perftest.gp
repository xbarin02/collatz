reset

set terminal pdf enhanced dashed linewidth 3 size 10cm,6cm font 'Serif,14' dashlength 2

set output "perftest.pdf"

# set key bottom

set ylabel "seconds"
# set format y "%.0s%c"
set yrange [0:120]

set xlabel "starting magnitude"
set format x "2^{%.0f}"
set xrange [0:64]
set xtics ("2^{32}" 0,"2^{40}" 8,"2^{48}" 16,"2^{56}" 24,"2^{64}" 32,"2^{72}" 40,"2^{80}" 48,"2^{88}" 56,"2^{96}" 64)

plot "usertime-32.txt" using ($1):($3) with lines linewidth 2 title "time per block"
