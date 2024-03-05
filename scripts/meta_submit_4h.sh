#!/bin/bash
#PBS -q backfill@meta-pbs.metacentrum.cz
#PBS -N collatz4
#PBS -S /bin/bash
#PBS -M ibarina@fit.vutbr.cz
#PBS -m n
#
#PBS -l select=1:ncpus=4:mem=2gb:scratch_local=1gb,walltime=24:00:00
#PBS -R eo

HOME=/storage/brno2/home/ibarina/

cd ~

TMPDIR=$SCRATCHDIR

export LANG=C

export SERVER_NAME=pcbarina.fit.vutbr.cz

echo "hostname=$(hostname)"
echo "pwd=$(pwd)"
echo "HOME=$HOME"
echo "cpu model name=$(cat /proc/cpuinfo | grep "model name" | head -n1)"
echo "cpus=$(cat /proc/cpuinfo | grep processor | wc -l)"
echo "TMPDIR=$TMPDIR"
echo "PBS_JOBID=${PBS_JOBID}"
# this is the cwd where qsub was executed, not cwd of the script itself
echo "PBS_O_WORKDIR=${PBS_O_WORKDIR}"
echo "SCRATCHDIR=$SCRATCHDIR"

type 'clean_scratch' && trap 'clean_scratch' TERM EXIT || :

module load clang-9.0
module load gmp

set -u
set -e

# check the connection
#if ! ping -c1 -q "${SERVER_NAME}"; then
#	echo "No connection!"
#	exit
#fi

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
make -C worker clean all USE_LIBGMP=1 CC=$CC USE_SIEVE=1 SIEVE_LOGSIZE=34 USE_PRECALC=1 USE_SIEVE3=0 USE_SIEVE9=1 USE_LUT50=1
make -C mclient clean all

pushd $MAPDIR
./unpack.sh hesieve-34.lut50 "$TMP"/collatz/src/worker
popd
mv worker/hesieve-34.lut50.map worker/esieve-34.lut50.map

cd mclient

# 4 hours walltime: limit 2 hours for the mclient; 3 hours for the worker
# 24 hours walltime: 23 hours for the mclient
stdbuf -o0 -e0 ./mclient -a 10800 -b 82800 4

popd
rm -rf -- "$TMP"

type clean_scratch && clean_scratch || :
