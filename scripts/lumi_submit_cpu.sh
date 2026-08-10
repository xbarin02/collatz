#!/bin/bash
#SBATCH --job-name=rs-coll
#SBATCH --account=project_465002647
#SBATCH --partition=standard
#xSBATCH --threads=128
#SBATCH --cpus-per-task=128
#SBATCH --array=1-100
#SBATCH --output=/dev/null
#SBATCH --error=/dev/null
#SBATCH --time=03:00:00
#SBATCH --nodes=1
#SBATCH --mem=4G

HOME=$HOME

TMPDIR=/tmp
mkdir -p -- "$TMPDIR"

export LANG=C

export SERVER_NAME=pcbarina.fit.vutbr.cz

set -u
set -e

umask 077

CC=cc

module load LUMI/25.09
module load GMP/6.3.0-cpeGNU-25.09

SRCDIR=$HOME/collatz/
TMP=$(mktemp -d collatz.XXXXXXXX --tmpdir)

mkdir -p -- "$TMP"
pushd -- "$TMP"

cp -r "${SRCDIR}" .

cd collatz/src

make -C rs-worker clean rs-worker-sc USE_LIBGMP=1 CC=$CC
make -C rs-client clean all CC=$CC

cd rs-client

# limit 2 hours for the mclient; 3 hours for worker
stdbuf -o0 -e0 ./rs-client -a 10800 -b 7200 -B 128

popd
rm -rf -- "$TMP"

echo "Done"
