#!/bin/bash
#
#$ -S /bin/bash
#$ -N collatz
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -o /dev/null
#$ -e /dev/null
#$ -q all.q@@stable,all.q@@gpu
#$ -tc 2000

set -u
set -e

export LANG=C

TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

mkdir -p -- "$TMP"
pushd -- "$TMP"

SRCDIR=${SGE_O_WORKDIR}/

cp -r "${SRCDIR}" .

cd collatz/src

make -C worker clean all
make -C client clean all

cd client

stdbuf -o0 -e0 ./client -1 1

popd
rm -rf -- "$TMP"
