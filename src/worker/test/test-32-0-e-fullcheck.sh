#!/bin/bash

USE_SIEVE=1
USE_LIBGMP=1
USE_SIEVE3=0
USE_PRECALC=0

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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11|12|13|14|15|16)"; then
        echo "INFO: clang available"
        CC=clang
fi

function build_wrapper()
{
	build USE_SIEVE=${USE_SIEVE} USE_PRECALC=${USE_PRECALC} USE_LIBGMP=${USE_LIBGMP} USE_SIEVE3=${USE_SIEVE3}
}

build_wrapper && verify 98999215 "271574034955 225865785455 0"

build_wrapper && verify 98999216 "271574881239 585714939119 0"

build_wrapper && verify 98999217 "271574909472 981603428123 0"

build_wrapper && verify 98999218 "271575191090 371145336991 0"

build_wrapper && verify 98999219 "271572889206 30069329055 0"

build_wrapper && verify 98999220 "271574145250 705488991903 0"

build_wrapper && verify 98999221 "271574648408 8275696879 0"

build_wrapper && verify 98999222 "271574644731 11710777343 0"

build_wrapper && verify 98999223 "271573571427 247028916391 0"

build_wrapper && verify 98999224 "271574107872 463055201307 0"

build_wrapper && verify 98999225 "271573333913 851511117927 0"

build_wrapper && verify 97064899 "271573656743 411750901999 0"

build_wrapper && verify 100982316 "271574679628 169129507999 0"
