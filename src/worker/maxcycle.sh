#!/bin/bash

make

t=32

PLOTFILE=maxcycle-$t.txt

rm -f -- "$PLOTFILE"

e=0
n=1

while :; do
	R=$(./worker -t $t $n | grep 'MAXIMUM_CYCLE ' | sed 's/MAXIMUM_CYCLE //')
	echo -e "$e\t$n\t$R"
	echo -e "$e\t$n\t$R" >> "$PLOTFILE"
	n=$(($n * 2))
	e=$(($e + 1))
done
