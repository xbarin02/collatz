#!/bin/bash

USE_SIEVE=1
USE_LIBGMP=1
USE_SIEVE3=0
USE_SIEVE9=1
USE_PRECALC=1
USE_LUT50=1
SIEVE_LOGSIZE=34

function check()
{
	T=$(./worker $1)

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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11|12|13|14)"; then
        echo "INFO: clang available"
        CC=clang
fi

function build_wrapper()
{
	build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} SIEVE_LOGSIZE=${SIEVE_LOGSIZE}
}

build_wrapper && verify 98999215 "142609242936 225865785455 0"

build_wrapper && verify 98999216 "142609210196 585714939119 0"

build_wrapper && verify 98999217 "142609102442 350224008639 0"

build_wrapper && verify 98999218 "142609421513 90451186779 0"

build_wrapper && verify 98999219 "142608692070 1000868809979 0"

build_wrapper && verify 98999220 "142608833648 705488991903 0"

build_wrapper && verify 98999221 "142609239605 187093925735 0"

build_wrapper && verify 98999222 "142609730139 835529048767 0"

build_wrapper && verify 98999223 "142608037957 392324915199 0"

build_wrapper && verify 98999224 "142609232103 33996997311 0"

build_wrapper && verify 98999225 "142608398083 438673686143 0"

build_wrapper && verify 97064899 "142609438953 198296862719 0"

build_wrapper && verify 100982316 "142609340122 169129507999 0"
