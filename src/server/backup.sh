#!/bin/bash

rm -f -- assigned.map.001
rm -f -- complete.map.001

mv assigned.map.000 assigned.map.001
mv complete.map.000 complete.map.001

cp assigned.map assigned.map.000
cp complete.map complete.map.000
