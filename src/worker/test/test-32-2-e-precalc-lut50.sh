#!/bin/bash

USE_SIEVE=1
USE_ESIEVE=1
USE_LIBGMP=1
USE_SIEVE3=0
USE_SIEVE9=1
USE_PRECALC=1
USE_LUT50=1

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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11)"; then
        echo "INFO: clang available"
        CC=clang
fi

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999215 "150874634531 225865785455 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999216 "150874958154 585714939119 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999217 "150874514263 350224008639 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999218 "150875045722 90451186779 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999219 "150874111522 490679694527 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999220 "150873839735 705488991903 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999221 "150874672856 187093925735 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999222 "150875025007 11710777343 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999223 "150873697035 247028916391 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999224 "150874292255 463055201307 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 98999225 "150874139777 668817880303 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 97064899 "150875124300 198296862719 0"

build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_ESIEVE=${USE_ESIEVE} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3} USE_SIEVE9=${USE_SIEVE9} USE_LUT50=${USE_LUT50} && verify 100982316 "150874673766 169129507999 0"