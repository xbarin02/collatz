#!/bin/bash

cd collatz-$(hostname -s)

cd src

make -C task
make -C client

cd client

screen -d -m ./spawn.sh
