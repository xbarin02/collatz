#!/bin/bash
#PBS -N collatzgpu
#PBS -S /bin/bash
#PBS -M ibarina@fit.vutbr.cz
#PBS -m a
#PBS -q qnvidia
#PBS -l walltime=04:00:00
#PBS -R eo
#PBS -A OPEN-18-7

HOME=$HOME

TMPDIR=/scratch/project/open-18-7
mkdir -p -- "$TMPDIR"

export POCL_CACHE_DIR=${TMPDIR}/kcache

mkdir -p "${POCL_CACHE_DIR}"

export LANG=C

# tunel from login1 to pcbarina must already exist
ssh -TN -f -L 5006:localhost:5006 login1

# tunel from localhost to login1 has been established
export SERVER_NAME=localhost

echo "hostname=$(hostname)"
echo "pwd=$(pwd)"
echo "HOME=$HOME"
echo "cpu model name=$(cat /proc/cpuinfo | grep "model name" | head -n1)"
echo "cpus=$(cat /proc/cpuinfo | grep processor | wc -l)"
echo "TMPDIR=$TMPDIR"
echo "PBS_JOBID=${PBS_JOBID}"
# this is the cwd where qsub was executed, not cwd of the script itself
echo "PBS_O_WORKDIR=${PBS_O_WORKDIR}"

ml GMP CUDA

nvidia-smi -L

echo "CUDA_VISIBLE_DEVICES=${CUDA_VISIBLE_DEVICES}"

set -u
set -e

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
make -C gpuworker clean all CC=gcc SIEVE_LOGSIZE=24 USE_SIEVE3=1 LDFLAGS='-fuse-ld=bfd'
make -C mclient clean all LDFLAGS='-fuse-ld=bfd'

pushd $MAPDIR
./unpack.sh esieve-24 $TMP/collatz/src/gpuworker
popd

cd mclient

# 120 minutes for mclient; 4 hours minus 100 secs for the gpuworker
stdbuf -o0 -e0 ./mclient -a 14300 -b 7200 -g -d 4

popd
rm -rf -- "$TMP"
rm -rf -- "${POCL_CACHE_DIR}"
