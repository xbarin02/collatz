#!/bin/bash

KERNEL=kernel32.cl
SIEVE_LOGSIZE=24
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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11|12|13|14|15)"; then
        echo "INFO: clang available"
        CC=clang
fi

function build_wrapper()
{
	build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_SIEVE3=${USE_SIEVE3}
}

build_wrapper && verify 98999215 "390339659554 225865785455 0"

build_wrapper && verify 98999216 "390340546613 585714939119 0"

build_wrapper && verify 98999217 "390340530896 981603428123 0"

build_wrapper && verify 98999218 "390340864165 371145336991 0"

build_wrapper && verify 98999219 "390338509130 30069329055 0"

build_wrapper && verify 98999220 "390339732880 705488991903 0"

build_wrapper && verify 98999221 "390340305013 8275696879 0"

build_wrapper && verify 98999222 "390340375358 11710777343 0"

build_wrapper && verify 98999223 "390339126386 247028916391 0"

build_wrapper && verify 98999224 "390339733038 463055201307 0"

build_wrapper && verify 98999225 "390339002599 851511117927 0"

build_wrapper && verify 97064899 "ABORTED_DUE_TO_OVERFLOW"

build_wrapper && verify 100982316 "390340316241 169129507999 0"
