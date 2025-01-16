#!/bin/bash
#PBS -N collatzgpu
#PBS -S /bin/bash
#PBS -M ibarina@fit.vutbr.cz
#PBS -m n
#
#PBS -q gpu
#PBS -l select=1:ncpus=1:ngpus=1:mem=2gb:scratch_local=2gb,walltime=4:00:00
#PBS -R eo

HOME=/storage/brno11-elixir/home/ibarina

TMPDIR=$SCRATCHDIR

export POCL_CACHE_DIR=${SCRATCHDIR}/kcache

mkdir -p "${POCL_CACHE_DIR}"

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
module load cuda-10.1

nvidia-smi -L

echo "CUDA_VISIBLE_DEVICES=${CUDA_VISIBLE_DEVICES}"

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

# build mclient & gpuworker
make -C gpuworker clean all CC=gcc SIEVE_LOGSIZE=24 USE_SIEVE3=1
make -C mclient clean all

pushd $MAPDIR
./unpack.sh h2esieve-24 $TMP/collatz/src/gpuworker
popd
mv gpuworker/h2esieve-24.map gpuworker/esieve-24.map

cd mclient

# 120 minutes for mclient; 4 hours minus 100 secs for the gpuworker
stdbuf -o0 -e0 ./mclient -a 14300 -b 7200 -g 1

popd
rm -rf -- "$TMP"
rm -rf -- "${POCL_CACHE_DIR}"

type clean_scratch && clean_scratch || :
