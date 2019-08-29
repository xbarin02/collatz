#!/bin/bash

cd collatz-$(hostname -s)/src

make -C worker
make -C client

cd client

screen -d -m ./spawn.sh
