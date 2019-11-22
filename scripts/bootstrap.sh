#!/bin/bash

export LANG=C

export SERVER_NAME=pcbarina.fit.vutbr.cz

HOSTNAME=$(hostname -s | tr '[:upper:]' '[:lower:]')

TASK_UNITS=16
if [[ "pco204-00" =~ ^pco204-..$ ]]; then
	TASK_UNITS=20
fi

cd ~/collatz-${HOSTNAME}/src

CC=gcc
if type clang > /dev/null 2> /dev/null && clang --version | grep -q "version [89]"; then
	echo "INFO: clang available"
	CC=clang
fi

make -C worker clean all USE_LIBGMP=1 CC=$CC
make -C gpuworker clean all USE_LIBGMP=1 TASK_UNITS=${TASK_UNITS} || echo "unable to build gpuworker"
make -C mclient clean all

cd mclient

screen -d -m ./spawn.sh $*
