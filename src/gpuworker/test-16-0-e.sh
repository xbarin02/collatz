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

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999215 "565678241554 225865785455 774712573087"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999216 "565679053018 585714939119 179472374087"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999217 "565679117320 981603428123 625267373159"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999218 "565679298859 371145336991 152337526527"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999219 "565677118921 30069329055 759086898943"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999220 "565678295112 705488991903 72345484027"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999221 "565678826163 8275696879 589522022399"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999222 "565678824715 11710777343 319962915967"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999223 "565677682279 247028916391 195234121343"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999224 "565678250831 463055201307 950509349991"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 98999225 "565677546761 851511117927 18710076399"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=1 && verify 97064899 "0 0 0"