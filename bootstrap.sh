#!/bin/bash

cd collatz-$(hostname -s)/src

make -C worker clean all
make -C client clean all

cd client

screen -d -m ./spawn.sh
