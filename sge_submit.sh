#!/bin/bash
#
#$ -S /bin/bash
#$ -N collatz
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -o /mnt/matylda1/ibarina/sge/collatz/out
#$ -e /mnt/matylda1/ibarina/sge/collatz/err
#$ -q all.q@@stable,all.q@@gpu
#$ -tc 2000

set -u
set -e

export LANG=C

echo "hostname=$(hostname)"
echo "pwd=$(pwd)"
echo "TMPDIR=$TMPDIR"
TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)
echo "TMP=${TMP}"

mkdir -p -- "$TMP"
pushd -- "$TMP"

SRCDIR=${SGE_O_WORKDIR}/
echo "SRCDIR=$SRCDIR"

cp -r "${SRCDIR}" .

cd collatz/src

make -C worker
make -C client

cd client

stdbuf -o0 -e0 ./client -1 1

popd
rm -rf -- "$TMP"

echo "DONE"
