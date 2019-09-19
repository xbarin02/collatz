/**
 * Support for 128-bit integers.
 */

#ifndef WIDEINT_WIDEINT_H_
#define WIDEINT_WIDEINT_H_

/* all GNU compilers */
#if defined(__GNUC__)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-pedantic"
typedef unsigned __int128 uint128_t;
#pragma GCC diagnostic pop

#define UINT128_C(n) ( (uint128_t)n )
#define UINT128_MAX ( ~UINT128_C(0) )

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-pedantic"
typedef __int128 int128_t;
#pragma GCC diagnostic pop

#define INT128_C(n) ( (int128_t)n )
#define INT128_MAX ( (int128_t)(UINT128_MAX >> 1) )
#define INT128_MIN ( -INT128_MAX - 1 )

#include <limits.h>

#include "compat.h"

/* count trailing zeros */
__attribute__ ((unused))
static int __builtin_ctzu128(uint128_t n)
{
	if ((uint64_t)n == 0)
		return 64 + __builtin_ctzu64((uint64_t)(n >> 64));
	else
		return __builtin_ctzu64((uint64_t)n);
}

#else
#	error "Unsupported compiler"
#endif /* defined(__GNUC__) */

#endif /* WIDEINT_WIDEINT_H_ */
