#!/bin/bash

USE_SIEVE=1
USE_LIBGMP=1
USE_SIEVE3=1
USE_PRECALC=1

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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11|12|13)"; then
        echo "INFO: clang available"
        CC=clang
fi

function build_wrapper()
{
	build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3}
}

build_wrapper && verify 98999215 "181049337008 225865785455 0"

build_wrapper && verify 98999216 "181049796166 585714939119 0"

build_wrapper && verify 98999217 "181049495450 350224008639 0"

build_wrapper && verify 98999218 "181049930994 90451186779 0"

build_wrapper && verify 98999219 "181048871931 1000868809979 0"

build_wrapper && verify 98999220 "181048868995 705488991903 0"

build_wrapper && verify 98999221 "181049554276 187093925735 0"

build_wrapper && verify 98999222 "181049757501 11710777343 0"

build_wrapper && verify 98999223 "181048402546 247028916391 0"

build_wrapper && verify 98999224 "181049311457 463055201307 0"

build_wrapper && verify 98999225 "181048746726 438673686143 0"

build_wrapper && verify 97064899 "181049886691 198296862719 0"

build_wrapper && verify 100982316 "181049266833 169129507999 0"
