#!/bin/bash
#$ -N collatz204
#$ -S /bin/bash
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -q short.q@@pco204
#$ -o /mnt/matylda1/ibarina/sge/collatz/logs/o204.out
#$ -e /mnt/matylda1/ibarina/sge/collatz/logs/o204.err
#$ -tc 20
#$ -t 1-100000
#$ -pe smp 2

HOME=/mnt/matylda1/ibarina/sge

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

# don't forget git clone git@github.com:xbarin02/collatz.git into $HOME
SRCDIR=$HOME/collatz/
TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

echo "SRCDIR=$SRCDIR"
echo "TMP=$TMP"

mkdir -p -- "$TMP"
pushd -- "$TMP"

cp -r "${SRCDIR}" .

cd collatz/src

# build mclient & gpuworker
make -C gpuworker clean all USE_LIBGMP=1 TASK_UNITS=20
make -C mclient clean all

cd mclient

# 20 minutes for mclient; 4 hours minus 100 secs for the gpuworker
stdbuf -o0 -e0 ./mclient -a 14300 -b 1200 -g 1

popd
rm -rf -- "$TMP"
