#!/bin/bash
#PBS -A OPEN-16-1
#PBS -q qprod
#PBS -N collatz
#PBS -l select=1:ncpus=24:ompthreads=24,walltime=04:00:00

set -u
set -e

export LANG=C

# module load libgmp
ml GMP

# tunel from login1 to pcbarina2 must already exist
ssh -TN -f -L 5006:localhost:5006 login1

# tunel from localhost to login1 has been established
export SERVER_NAME=localhost

# don't forget git clone git@github.com:xbarin02/collatz.git into $HOME
SRCDIR=$HOME/collatz
TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

echo "PBS_ARRAY_INDEX=${PBS_ARRAY_INDEX}"
echo "PBS_JOBID=${PBS_JOBID}"
# this is the cwd where qsub was executed, not cwd of the script itself
echo "PBS_O_WORKDIR=${PBS_O_WORKDIR}"
echo "TMPDIR=$TMPDIR"
echo "SRCDIR=$SRCDIR"
echo "hostname=$(hostname)"
echo "cpus=$(cat /proc/cpuinfo | grep processor | wc -l)"
echo "TMP=$TMP"

mkdir -p -- "$TMP"
pushd -- "$TMP"

cp -r "${SRCDIR}" .

cd collatz/src

# build client & worker
make clean all

cd client

stdbuf -o0 -e0 ./client -1 24

popd
rm -rf -- "$TMP"
