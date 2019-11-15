#!/bin/bash

cd ~/collatz-$(hostname -s | tr '[:upper:]' '[:lower:]')/

git pull

cd

killall mclient

sleep 1

killall worker gpuworker

sleep 1

~/collatz-$(hostname -s | tr '[:upper:]' '[:lower:]')/scripts/bootstrap.sh
