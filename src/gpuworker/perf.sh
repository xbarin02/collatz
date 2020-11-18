#!/bin/bash

for t in test*-e*.sh; do
	echo $t
	\time ./$t > $t.txt 2>&1
done
