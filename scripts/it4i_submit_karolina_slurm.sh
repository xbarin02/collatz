#!/bin/bash
#SBATCH --job-name=collatz
#SBATCH --account=OPEN-28-77
#SBATCH --partition=qcpu
#SBATCH --array=1-1000
#xSBATCH --output=array_%A_%a.out
#xSBATCH --error=array_%A_%a.err
#SBATCH --ntasks-per-node 128
#SBATCH --time=04:00:00
#SBATCH --nodes=1

HOME=$HOME

TMPDIR=/scratch/project/open-28-77
mkdir -p -- "$TMPDIR"

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

ml GMP

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

# build mclient & worker
make -C worker clean all USE_LIBGMP=1 CC=$CC USE_SIEVE=1 SIEVE_LOGSIZE=34 USE_PRECALC=1 USE_SIEVE3=1 USE_LUT50=1
make -C mclient clean all

pushd $MAPDIR
./unpack.sh esieve-34.lut50 "$TMP"/collatz/src/worker
popd

cd mclient

# limit 2 hours for the mclient; 3 hours for worker
stdbuf -o0 -e0 ./mclient -a 10800 -b 7200 -B 128

popd
rm -rf -- "$TMP"

echo "Done"
