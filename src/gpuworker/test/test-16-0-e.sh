#!/bin/bash

KERNEL=kernel32.cl
SIEVE_LOGSIZE=16
USE_SIEVE3=0

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

	if test "$R" = "$2"; then
		if test "$R" = ABORTED_DUE_TO_OVERFLOW; then
			echo -e "\e[1m$1\e[0m: \e[32mPASSED\e[0m (\e[31m$R\e[0m)"
		else
			echo -e "\e[1m$1\e[0m: \e[32mPASSED\e[0m"
		fi
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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11)"; then
        echo "INFO: clang available"
        CC=clang
fi

function build_wrapper()
{
	build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_SIEVE3=${USE_SIEVE3}
}

build_wrapper && verify 98999215 "565678241554 225865785455 0"

build_wrapper && verify 98999216 "565679053018 585714939119 0"

build_wrapper && verify 98999217 "565679117320 981603428123 0"

build_wrapper && verify 98999218 "565679298859 371145336991 0"

build_wrapper && verify 98999219 "565677118921 30069329055 0"

build_wrapper && verify 98999220 "565678295112 705488991903 0"

build_wrapper && verify 98999221 "565678826163 8275696879 0"

build_wrapper && verify 98999222 "565678824715 11710777343 0"

build_wrapper && verify 98999223 "565677682279 247028916391 0"

build_wrapper && verify 98999224 "565678250831 463055201307 0"

build_wrapper && verify 98999225 "565677546761 851511117927 0"

build_wrapper && verify 97064899 "ABORTED_DUE_TO_OVERFLOW"
