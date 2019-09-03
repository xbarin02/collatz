#!/bin/bash

make

PLOTFILE=usertime.txt

rm -f -- "$PLOTFILE"

e=0
n=1

while :; do
	USERTIME=$(\time ./worker $n 2>&1 | grep "user" | sed 's/user.*//')
	echo -e "$e\t$n\t$USERTIME"
	echo -e "$e\t$n\t$USERTIME" >> "$PLOTFILE"
	n=$(($n * 2))
	e=$(($e + 1))
done
