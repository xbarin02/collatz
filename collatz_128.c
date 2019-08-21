/**
 * @file
 * @brief Simple program that check convergence of the Collatz problem.
 *
 * @author David Barina <ibarina@fit.vutbr.cz>
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include "wideint.h"

#define BOUNDARY_N (1UL<<44)
#define BOUNDARY_E 14

#define LUT_SIZE 41
#define LUT_SIZE128 81

unsigned long g_lut[LUT_SIZE];
uint128_t g_lutx[LUT_SIZE128];

/* 3^n */
unsigned long pow3(unsigned long n)
{
	unsigned long r = 1;

	for (; n > 0; --n) {
		assert( r <= ULONG_MAX / 3 );

		r *= 3;
	}

	return r;
}

/* 3^n */
uint128_t pow3x(uint128_t n)
{
	uint128_t r = 1;

	for (; n > 0; --n) {
		assert( r <= UINT128_MAX / 3 );

		r *= 3;
	}

	return r;
}

void init_lut()
{
	unsigned long a;

	for (a = 0; a < LUT_SIZE; ++a) {
		g_lut[a] = pow3(a);
	}

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lutx[a] = pow3x(a);
	}
}

/* count trailing zeros */
static int __builtin_ctzx(uint128_t n)
{
	if ((unsigned long)n == 0)
		return (sizeof(unsigned long) * CHAR_BIT) + __builtin_ctzl((unsigned long)(n >> (sizeof(unsigned long) * CHAR_BIT)));
	else
		return __builtin_ctzl((unsigned long)n);
}

/* declare */
void checkx_ex(uint128_t n, int e, unsigned long n0);

void check_ex(unsigned long n, int e, unsigned long n0)
{
	do {
		/* switch back to 128-bit implementation */
		if (n > ULONG_MAX >> 2*e || e >= LUT_SIZE) {
			checkx_ex((uint128_t)n, e, n0);
			return;
		}

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzl(n);
#if 1
		if (n < n0)
			return;
#endif
		n++;

		e = __builtin_ctzl(n);

		n >>= e;

		/* (n,e) pair */

		if (e < BOUNDARY_E && n < BOUNDARY_N)
			return;
	} while (1);
}

void checkx_ex(uint128_t n, int e, unsigned long n0)
{
	do {
		assert( n <= UINT128_MAX >> 2*e );

		assert( e < LUT_SIZE128 );

		n *= g_lutx[e];

		n--;

		n >>= __builtin_ctzx(n);

		if (n < n0)
			return;

		n++;

		e = __builtin_ctzx(n);

		n >>= e;

		/* (n,e) pair */

		if (e < BOUNDARY_E && n < BOUNDARY_N)
			return;

		/* at this point, if hi_part(n) == 0 and e < LUT_SIZE, we could switch back to 64-bit implementation */
		if ( (unsigned long)(n>>64) == 0 && e < LUT_SIZE ) {
			check_ex((unsigned long)n, e, n0);
			return;
		}
	} while (1);
}

/* check convergence */
void check(unsigned long n)
{
	unsigned long n0 = n;

	do {
		int e;

		n++;

		e = __builtin_ctzl(n);

		n >>= e;

		/* (n,e) pair */

		/* all (n,e) below the following limits have already been checked for convergence */
		if (e < BOUNDARY_E && n < BOUNDARY_N)
			return;

		if (n > ULONG_MAX >> 2*e || e >= LUT_SIZE) {
			checkx_ex((uint128_t)n, e, n0);
			return;
		}

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzl(n);
	} while (n >= n0);
}

int main(int argc, char *argv[])
{
	unsigned long n;
	unsigned long n_sup = 1UL << (
		(argc > 1) ? (unsigned long)atol(argv[1])
		: 32 );

	init_lut();

	/* n of the form 4n+3 */
	#pragma omp parallel for
	for (n = 3; n < n_sup; n += 4) {
		check(n);
	}

	return 0;
}
