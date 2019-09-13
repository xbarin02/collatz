#!/bin/bash

make

t=24

PLOTFILE=checksum-$t.txt

for s in $(seq 1 1000); do
	a=$(./worker -t $t $s | grep CHECKSUM | cut -d' ' -f2)
	b=$(./worker -t $t $s | grep CHECKSUM | cut -d' ' -f3)

	echo -e "$s\t$a\t$b"
	echo -e "$s\t$a\t$b" >> "$PLOTFILE"
done
