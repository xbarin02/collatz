#!/bin/bash

du -sh *.map *.dat

for f in *.map *.dat; do
	echo fallocate -d $f
done

du -sh *.map *.dat
