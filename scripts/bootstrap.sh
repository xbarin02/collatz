#!/bin/bash

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

if ! test -d "$SRCDIR"; then
	pushd "$HOME"
	git clone git@github.com:xbarin02/collatz.git
	popd
else
	pushd "$SRCDIR"
	git pull || echo "cannot sync repo"
	popd
fi

if ! test -d "$MAPDIR"; then
	pushd "$HOME"
	git clone git@github.com:xbarin02/collatz-sieve.git
	popd
else
	pushd "$MAPDIR"
	git pull || echo "cannot sync repo"
	popd
fi

# cd $HOME/collatz-$HOSTNAME/src

mkdir -p -- "$TMP"
pushd -- "$TMP"

cp -r "${SRCDIR}" .

cd collatz/src

HOSTNAME=$(hostname -s | tr '[:upper:]' '[:lower:]')

TASK_UNITS=16
if [[ "$HOSTNAME" =~ ^pco204-..$ ]]; then
	TASK_UNITS=20
fi

# build mclient & worker
make -C worker clean all USE_LIBGMP=1 CC=$CC USE_SIEVE=1 USE_MOD12=1
make -C gpuworker clean all USE_LIBGMP=1 TASK_UNITS=${TASK_UNITS} || echo "unable to build gpuworker"
make -C mclient clean all

pushd $MAPDIR
./unpack.sh sieve-32 $TMP/collatz/src/worker
popd

cd mclient

CLEANUP_DIR=$TMP screen -d -m ./spawn.sh $*

popd
# rm -rf -- "$TMP"
