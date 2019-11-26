#!/bin/bash

killall mclient

killall worker gpuworker

sleep 1

~/collatz/scripts/bootstrap.sh
