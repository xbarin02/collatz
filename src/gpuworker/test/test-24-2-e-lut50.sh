#!/bin/bash

KERNEL=kernel32.cl
SIEVE_LOGSIZE=24
USE_ESIEVE=1
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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11)"; then
        echo "INFO: clang available"
        CC=clang
fi

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999215 "216855436618 225865785455 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999216 "216855432245 585714939119 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999217 "216855284080 350224008639 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999218 "216855648464 90451186779 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999219 "216854870767 1000868809979 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999220 "216855061043 705488991903 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999221 "216855475322 187093925735 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999222 "216855998255 835529048767 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999223 "216854201005 392324915199 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999224 "216855433947 33996997311 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999225 "216854643682 438673686143 0"

build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_ESIEVE=${USE_ESIEVE} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 97064899 "216855663515 198296862719 0"
