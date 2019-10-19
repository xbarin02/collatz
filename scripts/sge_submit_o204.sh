#!/bin/bash
#
#$ -S /bin/bash
#$ -N collatzo204
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -o /mnt/matylda1/ibarina/sge/collatz/logs/o204.out
#$ -e /mnt/matylda1/ibarina/sge/collatz/logs/o204.err
#$ -q all.q@@pco204
#$ -tc 1000
#$ -pe smp 2

set -u
set -e

export LANG=C

export SERVER_NAME=pcbarina.fit.vutbr.cz

TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

mkdir -p -- "$TMP"
pushd -- "$TMP"

SRCDIR=/mnt/matylda1/ibarina/sge/collatz/

cp -r "${SRCDIR}" .

cd collatz/src

make -C worker clean all
make -C gpuworker clean all TASK_UNITS=20
make -C mclient clean all

cd mclient

# 20 minutes for mclient; 4 hours minus 100 secs for the (gpu)worker
stdbuf -o0 -e0 ./mclient -a 14300 -b 1200 -g 1

popd
rm -rf -- "$TMP"
