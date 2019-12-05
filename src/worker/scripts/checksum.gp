reset

set terminal pdf enhanced dashed linewidth 3 size 10cm,6cm font 'Serif,14' dashlength 2

set output "checksum.pdf"

set key bottom

set yrange [0:*]

t=24

plot \
	"checksum-24.txt" using 1:($2 / 2**t) with lines title "alpha", \
	"checksum-24.txt" using 1:($3 / 2**t) with lines title "beta"
