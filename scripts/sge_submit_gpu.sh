#!/bin/bash
#
#$ -S /bin/bash
#$ -N collatzgpu
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -o /dev/null
#$ -e /dev/null
#$ -q all.q@@gpu
#$ -tc 1000
#$ -t 1-100000
#$ -l gpu=1

echo "CUDA_VISIBLE_DEVICES=${CUDA_VISIBLE_DEVICES}"

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

make -C worker clean all USE_LIBGMP=1
make -C gpuworker clean all USE_LIBGMP=1
make -C mclient clean all

cd mclient

# 20 minutes for mclient; 4 hours minus 100 secs for the (gpu)worker
stdbuf -o0 -e0 ./mclient -a 14300 -b 1200 -g 1

popd
rm -rf -- "$TMP"
