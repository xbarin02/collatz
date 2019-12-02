#!/bin/bash

for f in *.dat *.map; do
	echo fallocate -d $f
done
