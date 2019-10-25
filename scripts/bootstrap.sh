#!/bin/bash

export SERVER_NAME=pcbarina.fit.vutbr.cz

cd ~/collatz-$(hostname -s)/src

if type clang > /dev/null 2> /dev/null && clang --version | grep -q "version [89]"; then
	echo "INFO: clang available"
fi

make -C worker clean all
make -C gpuworker clean all || echo "unable to build gpuworker"
make -C mclient clean all

cd mclient

screen -d -m ./spawn.sh
