#!/bin/bash

function check()
{
	T=$(./gpuworker $1)

	CHECKSUM=$(echo "$T" | sed -unE '/CHECKSUM/s/.* (.*) .*/\1/p')
	MAXIMUM_OFFSET=$(echo "$T" | sed -unE '/MAXIMUM_OFFSET/s/.* (.*)/\1/p')
	MAXIMUM_CYCLE_OFFSET=$(echo "$T" | sed -unE '/MAXIMUM_CYCLE_OFFSET/s/.* (.*)/\1/p')
	# '

	echo "$CHECKSUM $MAXIMUM_OFFSET $MAXIMUM_CYCLE_OFFSET"
}

function verify()
{
	echo "$1: checking..."

	if test "$(check $1)" = "$2"; then
		echo "$1: PASSED"
	else
		echo "$1: FAILED"
	fi
}

function build()
{
	echo make --quiet clean all $* CC=$CC
	make --quiet clean all $* CC=$CC
}

CC=gcc
if type clang > /dev/null 2> /dev/null && clang --version | grep -q "version [89]"; then
        echo "INFO: clang available"
        CC=clang
fi

build SIEVE_LOGSIZE=32 && verify 98999215 "330684153073 626941880799 774712573087"

build SIEVE_LOGSIZE=32 && verify 98999216 "330685251240 585714939119 179472374087"

build SIEVE_LOGSIZE=32 && verify 98999217 "330685267527 981603428123 625267373159"

build SIEVE_LOGSIZE=32 && verify 98999218 "330685137688 371145336991 152337526527"

build SIEVE_LOGSIZE=32 && verify 98999219 "330682821619 30069329055 551992735199"

build SIEVE_LOGSIZE=32 && verify 98999220 "330684443074 705488991903 72345484027"

build SIEVE_LOGSIZE=32 && verify 98999221 "330684669313 8275696879 589522022399"

build SIEVE_LOGSIZE=32 && verify 98999222 "330683899994 11710777343 42471675263"

build SIEVE_LOGSIZE=32 && verify 98999223 "330683130485 247028916391 195234121343"

build SIEVE_LOGSIZE=32 && verify 98999224 "330684344694 463055201307 950509349991"

build SIEVE_LOGSIZE=32 && verify 98999225 "330683090534 851511117927 18710076399"

build SIEVE_LOGSIZE=32 && verify 97064899 "330683601700 411750901999 411750901999"
