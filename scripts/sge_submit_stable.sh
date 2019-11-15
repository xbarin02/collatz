#!/bin/bash
#
#$ -S /bin/bash
#$ -N collatz
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -o /dev/null
#$ -e /dev/null
#$ -q all.q@@stable
#$ -tc 600
#$ -t 1-100000

set -u
set -e

export LANG=C

export SERVER_NAME=pcbarina.fit.vutbr.cz

CC=gcc
if type clang > /dev/null 2> /dev/null && clang --version | grep -q "version [89]"; then
        echo "INFO: clang available"
        CC=clang
fi

TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

mkdir -p -- "$TMP"
pushd -- "$TMP"

SRCDIR=/mnt/matylda1/ibarina/sge/collatz/

cp -r "${SRCDIR}" .

cd collatz/src

make -C worker clean all USE_LIBGMP=1 CC=$CC
make -C mclient clean all

cd mclient

stdbuf -o0 -e0 ./mclient -a 14300 -1 1

popd
rm -rf -- "$TMP"
