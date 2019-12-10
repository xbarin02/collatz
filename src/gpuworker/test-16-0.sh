#!/bin/bash

KERNEL=kernel32.cl
SIEVE_LOGSIZE=16

function check()
{
	T=$(./gpuworker -k "$KERNEL" $1)

	CHECKSUM=$(echo "$T" | sed -unE '/CHECKSUM/s/.* (.*) .*/\1/p')
	MAXIMUM_OFFSET=$(echo "$T" | sed -unE '/MAXIMUM_OFFSET/s/.* (.*)/\1/p')
	MAXIMUM_CYCLE_OFFSET=$(echo "$T" | sed -unE '/MAXIMUM_CYCLE_OFFSET/s/.* (.*)/\1/p')
	# '

	if [[ "$T" =~ ABORTED_DUE_TO_OVERFLOW ]]; then
		echo "ABORTED_DUE_TO_OVERFLOW"
		return
	fi

	echo "$CHECKSUM $MAXIMUM_OFFSET $MAXIMUM_CYCLE_OFFSET"
}

function verify()
{
	echo -e "\e[1m$1\e[0m: checking..."

	R="$(check $1)"

	if test "$R" = ABORTED_DUE_TO_OVERFLOW; then
		echo -e "\e[1m$1\e[0m: \e[31m$R\e[0m"
	elif test "$R" = "$2"; then
		echo -e "\e[1m$1\e[0m: \e[32mPASSED\e[0m"
	else
		echo -e "\e[1m$1\e[0m: \e[31mFAILED ($R)\e[0m"
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

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999215 "691174614257 626941880799 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999216 "691175712424 585714939119 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999217 "691175728711 981603428123 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999218 "691175598872 371145336991 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999219 "691173282803 30069329055 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999220 "691174904258 705488991903 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999221 "691175130497 8275696879 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999222 "691174361178 11710777343 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999223 "691173591669 247028916391 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999224 "691174805878 463055201307 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 98999225 "691173551718 851511117927 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} && verify 97064899 "0 0 0"
