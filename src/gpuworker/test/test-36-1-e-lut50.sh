#!/bin/bash

KERNEL=kernel32.cl
SIEVE_LOGSIZE=36
USE_SIEVE3=1
USE_LUT50=1

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
	build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_SIEVE3=${USE_SIEVE3} USE_LUT50=${USE_LUT50}
}

build_wrapper && verify 98999215 "155804083085 225865785455 0"

build_wrapper && verify 98999216 "155804547591 585714939119 0"

build_wrapper && verify 98999217 "155804243969 350224008639 0"

build_wrapper && verify 98999218 "155804661768 90451186779 0"

build_wrapper && verify 98999219 "155803589457 1000868809979 0"

build_wrapper && verify 98999220 "155803565110 705488991903 0"

build_wrapper && verify 98999221 "155804292030 187093925735 0"

build_wrapper && verify 98999222 "155804456373 11710777343 0"

build_wrapper && verify 98999223 "155803128782 247028916391 0"

build_wrapper && verify 98999224 "155804054789 463055201307 0"

build_wrapper && verify 98999225 "155803461778 438673686143 0"

build_wrapper && verify 97064899 "155804590372 198296862719 0"

build_wrapper && verify 100982316 "155803980942 169129507999 0"
