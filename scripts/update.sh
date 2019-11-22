#!/bin/bash

cd ~/collatz/

git pull

cd

killall mclient

killall worker gpuworker

sleep 1

~/collatz/scripts/bootstrap.sh
