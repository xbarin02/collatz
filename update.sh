#!/bin/bash

git pull

cd

killall client

sleep 1

killall worker

sleep 1

./collatz-$(hostname -s)/bootstrap.sh
