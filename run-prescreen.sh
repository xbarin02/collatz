#!/bin/bash

THREADS=${1:-1}
BITS=${2:-32}

echo "THREADS: $THREADS"
echo "BITS: $BITS"

make prescreen && OMP_NUM_THREADS=$THREADS OMP_SCHEDULE=static stdbuf -o0 -e0 ./prescreen | tee prescreen-2^$BITS.txt
