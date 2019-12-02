#!/bin/bash

read -p "Deflate (y/n)? " answer
case ${answer:0:1} in
	y|Y )
		:
	;;
	* )
		exit 1
	;;
esac

du -sh *.map *.dat

echo Deflating...
for f in *.map *.dat; do
	fallocate -d $f
done

du -sh *.map *.dat
