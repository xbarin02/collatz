#!/bin/bash

make

t=32

PLOTFILE=usertime-$t.txt

rm -f -- "$PLOTFILE"

e=0
n=1

while :; do
	R=$(bc <<< "scale=6; $(./worker -t $t | grep USERTIME | cut -d' ' -f3) / 1000000")
	echo -e "$e\t$n\t$R"
	echo -e "$e\t$n\t$R" >> "$PLOTFILE"
	n=$(($n * 2))
	e=$(($e + 1))
done
