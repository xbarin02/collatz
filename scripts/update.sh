#!/bin/bash

cd ~/collatz-$(hostname -s | tr '[:upper:]' '[:lower:]')/

git pull

cd

killall mclient

killall worker gpuworker

sleep 1

~/collatz-$(hostname -s | tr '[:upper:]' '[:lower:]')/scripts/bootstrap.sh
