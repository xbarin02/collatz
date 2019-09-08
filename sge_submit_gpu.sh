#!/bin/bash
#
#$ -S /bin/bash
#$ -N collatz
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -o /dev/null
#$ -e /dev/null
#$ -q all.q@@gpu
#$ -tc 10

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
make -C mclient clean all

cd mclient

stdbuf -o0 -e0 ./mclient -1 1

popd
rm -rf -- "$TMP"
