#!/bin/bash
#PBS -N collatz
#PBS -l select=1:ncpus=24
#PBS -l walltime=4:00:00
#PBS -m a
#PBS -M ibarina@fit.vutbr.cz

HOME=/storage/brno11-elixir/home/ibarina

echo "hostname=$(hostname)"
echo "pwd=$(pwd)"
echo "HOME=$HOME"
echo "cpu model name=$(cat /proc/cpuinfo | grep "model name" | head -n1)"
echo "cpus=$(cat /proc/cpuinfo | grep "model name" | wc -l)"

# check the connection
if ! ping -c1 -q pcbarina2.fit.vutbr.cz; then
	echo "No connection!"
	exit
fi

module load gmp

SRCDIR=$HOME/collatz
TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

mkdir -p -- "$TMP"
pushd -- "$TMP"

cp -r "${SRCDIR}" .

cd collatz/src

# build mclient & worker
make -C worker clean all
make -C mclient clean all

cd mclient

stdbuf -o0 -e0 ./mclient -1 24

popd
rm -rf -- "$TMP"
