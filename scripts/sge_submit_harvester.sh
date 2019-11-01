#!/bin/bash
#
#$ -S /bin/bash
#$ -N harvester
#$ -M ibarina@fit.vutbr.cz
#$ -m bea
#$ -o /mnt/matylda1/ibarina/sge/collatz/logs/harvester.out
#$ -e /mnt/matylda1/ibarina/sge/collatz/logs/harvester.err
#$ -q long.q@@stable
#$ -pe smp 28

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

make -C worker clean all USE_LIBGMP=1 USE_MOD12=1
make -C mclient clean all

cd mclient

stdbuf -o0 -e0 ./mclient -l 56

popd
rm -rf -- "$TMP"
