#!/bin/bash

KERNEL=kernel32.cl
SIEVE_LOGSIZE=24
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
if type clang > /dev/null 2> /dev/null && clang --version | grep -qE "version (8|9|10|11|12|13|14|15|16)"; then
        echo "INFO: clang available"
        CC=clang
fi

function build_wrapper()
{
	build SIEVE_LOGSIZE=${SIEVE_LOGSIZE} USE_SIEVE3=${USE_SIEVE3} USE_LUT50=${USE_LUT50}
}

build_wrapper && verify 98999215 "260226429995 225865785455 0"

build_wrapper && verify 98999216 "260226900702 585714939119 0"

build_wrapper && verify 98999217 "260226551776 350224008639 0"

build_wrapper && verify 98999218 "260227034163 90451186779 0"

build_wrapper && verify 98999219 "260225940718 1000868809979 0"

build_wrapper && verify 98999220 "260225912848 705488991903 0"

build_wrapper && verify 98999221 "260226694342 187093925735 0"

build_wrapper && verify 98999222 "260226923735 11710777343 0"

build_wrapper && verify 98999223 "260225426393 247028916391 0"

build_wrapper && verify 98999224 "260226400843 463055201307 0"

build_wrapper && verify 98999225 "260225876900 438673686143 0"

build_wrapper && verify 97064899 "260226992630 198296862719 0"

build_wrapper && verify 100982316 "260226323063 169129507999 0"
