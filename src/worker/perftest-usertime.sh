#!/bin/bash

make

t=32

PLOTFILE=usertime-$t.txt

rm -f -- "$PLOTFILE"

e=0
n=1

while :; do
	R=$(\time ./worker -t $t $n 2>&1 | grep "user" | sed 's/user.*//')
	echo -e "$e\t$n\t$R"
	echo -e "$e\t$n\t$R" >> "$PLOTFILE"
	n=$(($n * 2))
	e=$(($e + 1))
done
