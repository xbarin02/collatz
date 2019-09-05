#!/bin/bash

set -e
set -u

export LANG=C

MASTER_BASHPID=${BASHPID}

function die() {
	echo -e "*\n* ERROR: $@\n*"
	[[ ${BASHPID} != ${MASTER_BASHPID} ]] && kill -s SIGTERM ${MASTER_BASHPID}
	exit 1
}

LOCKFILE=/tmp/collatz.lock
LOGFILE=/tmp/collatz.log

function release_lock() {
	rm -f -- "${LOCKFILE}"
}

function remove_logs() {
	rm -f -- "${LOGFILE}".{err,out}
}

function cleanup() {
	release_lock
	remove_logs
}

function bye() {
	cleanup
	die "interrupted"
}

trap bye INT
trap bye TERM

if ! (set -o noclobber ; echo > "${LOCKFILE}"); then
	die "client already running (or orphaned '${LOCKFILE}')"
fi

THREADS=$(grep processor /proc/cpuinfo | wc -l)
CLIENT=./mclient

if test -x "${CLIENT}"; then
	echo "executing '${CLIENT}' with ${THREADS} worker threads..."
	if nice -n 19 "${CLIENT}" "${THREADS}" 2> "${LOGFILE}".err > "${LOGFILE}".out; then
		echo "shutting down ..."
	else
		cleanup
		die "'${CLIENT}' failure"
	fi
else
	cleanup
	die "cannot execute '${CLIENT}'"
fi

cleanup
echo "successfully finished"
