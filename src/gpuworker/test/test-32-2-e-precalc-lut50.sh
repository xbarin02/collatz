#!/bin/bash

KERNEL=kernel32-precalc.cl
SIEVE_LOGSIZE=32
USE_SIEVE3=0
USE_SIEVE9=1
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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11|12|13)"; then
        echo "INFO: clang available"
        CC=clang
fi

function build_wrapper()
{
	build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50}
}

build_wrapper && verify 98999215 "150874523748 225865785455 0"

build_wrapper && verify 98999216 "150874503309 585714939119 0"

build_wrapper && verify 98999217 "150874417297 350224008639 0"

build_wrapper && verify 98999218 "150874708528 90451186779 0"

build_wrapper && verify 98999219 "150873988372 1000868809979 0"

build_wrapper && verify 98999220 "150874180317 705488991903 0"

build_wrapper && verify 98999221 "150874515726 187093925735 0"

build_wrapper && verify 98999222 "150875049584 835529048767 0"

build_wrapper && verify 98999223 "150873330330 392324915199 0"

build_wrapper && verify 98999224 "150874532211 33996997311 0"

build_wrapper && verify 98999225 "150873682211 438673686143 0"

build_wrapper && verify 97064899 "150874731246 198296862719 0"

build_wrapper && verify 100982316 "150874673766 169129507999 0"
