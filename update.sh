#!/bin/bash

cd ./collatz-$(hostname -s)/

git pull

cd

killall client mclient

sleep 1

killall worker

sleep 1

./collatz-$(hostname -s)/bootstrap.sh
