#!/bin/bash

cd collatz-$(hostname -s)/src

make -C worker clean all
make -C mclient clean all

cd mclient

screen -d -m ./spawn.sh
