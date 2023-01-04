#!/bin/bash
#$ -N collatz
#$ -S /bin/bash
#$ -M ibarina@fit.vutbr.cz
#$ -m a
#$ -q short.q@@stable
#$ -o /dev/null
#$ -e /dev/null
#$ -tc 600
#$ -t 1-100000
#$ -p -1023
#

HOME=$HOME

export LANG=C

export SERVER_NAME=pcbarina.fit.vutbr.cz

echo "hostname=$(hostname)"
echo "pwd=$(pwd)"
echo "HOME=$HOME"
echo "cpu model name=$(cat /proc/cpuinfo | grep "model name" | head -n1)"
echo "cpus=$(cat /proc/cpuinfo | grep processor | wc -l)"
echo "TMPDIR=$TMPDIR"

set -u
set -e

# check the connection
if ! ping -c1 -q "${SERVER_NAME}"; then
	echo "No connection!"
	exit
fi

umask 077

CC=gcc
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11|12|13|14|15)"; then
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

# build mclient & worker
make -C worker clean all USE_LIBGMP=1 CC=$CC USE_SIEVE=1 SIEVE_LOGSIZE=34 USE_PRECALC=1 USE_SIEVE3=1 USE_LUT50=1
make -C mclient clean all

pushd $MAPDIR
./unpack.sh esieve-34.lut50 "$TMP"/collatz/src/worker
popd

cd mclient

# no limit for mclient; 29 minutes for the worker
stdbuf -o0 -e0 ./mclient -a 1740 -1 1

popd
rm -rf -- "$TMP"
