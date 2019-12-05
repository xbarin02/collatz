#!/bin/bash

function backup()
{
	F=$1
	F000=${1%%.*}.000.${1#*.}
	F001=${1%%.*}.001.${1#*.}

	echo "Backing up file $F..."
	test -e $F000 && mv -f -- $F000 $F001
	test -e $F && cp -- $F $F000
}

MAPS=( assigned.map complete.map )

for F in ${MAPS[@]}; do backup $F; done

DATS=( clientids.dat cycleoffs.dat checksums.dat mxoffsets.dat overflows.dat usertimes.dat )

for F in ${DATS[@]}; do backup $F; done
