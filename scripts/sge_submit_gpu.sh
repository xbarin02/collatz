#!/bin/bash
#$ -N collatzgpu
#$ -S /bin/bash
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -q short.q@@gpu
#$ -o /dev/null
#$ -e /dev/null
#$ -tc 20
#$ -t 1-100000
#$ -l gpu=1

HOME=$HOME

export LANG=C

export SERVER_NAME=pcbarina.fit.vutbr.cz

echo "hostname=$(hostname)"
echo "pwd=$(pwd)"
echo "HOME=$HOME"
echo "cpu model name=$(cat /proc/cpuinfo | grep "model name" | head -n1)"
echo "cpus=$(cat /proc/cpuinfo | grep processor | wc -l)"
echo "TMPDIR=$TMPDIR"

nvidia-smi -L

echo "CUDA_VISIBLE_DEVICES=${CUDA_VISIBLE_DEVICES}"

set -u
set -e

# check the connection
if ! ping -c1 -q "${SERVER_NAME}"; then
	echo "No connection!"
	exit
fi

umask 077

CC=gcc
if type clang > /dev/null 2> /dev/null && clang --version | grep -q "version [89]"; then
        echo "INFO: clang available"
        CC=clang
fi

# don't forget git clone git@github.com:xbarin02/collatz.git into $HOME
SRCDIR=$HOME/collatz/
MAPDIR=$HOME/collatz-sieve/
TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

echo "SRCDIR=$SRCDIR"
echo "TMP=$TMP"

mkdir -p -- "$TMP"
pushd -- "$TMP"

cp -r "${SRCDIR}" .

cd collatz/src

# build mclient & gpuworker
make -C gpuworker clean all CC=$CC
make -C mclient clean all

pushd $MAPDIR
./unpack.sh sieve-16 $TMP/collatz/src/gpuworker || :
./unpack.sh sieve-24 $TMP/collatz/src/gpuworker
./unpack.sh sieve-32 $TMP/collatz/src/gpuworker
popd

cd mclient

# 20 minutes for mclient; 29 minutes for the gpuworker
stdbuf -o0 -e0 ./mclient -a 1740 -b 1200 -g 1

popd
rm -rf -- "$TMP"
