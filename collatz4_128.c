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
#define BOUNDARY_E 16

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

/* declare */
void checkx_ex(uint128_t n, int e);

void check_ex(unsigned long n, int e)
{
	do {
		/* switch back to 128-bit implementation */
		if (n > ULONG_MAX >> 2*e || e >= LUT_SIZE) {
			checkx_ex((uint128_t)n, e);
			return;
		}

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzl(n);

		n++;

		e = __builtin_ctzl(n);

		n >>= e;

		/* (n,e) pair */

		if (e < BOUNDARY_E && n < BOUNDARY_N)
			return;
	} while (1);
}

void checkx_ex(uint128_t n, int e)
{
	do {
		assert( n <= UINT128_MAX >> 2*e );

		assert( e < LUT_SIZE128 );

		n *= g_lutx[e];

		n--;

		n >>= __builtin_ctzx(n);

		n++;

		e = __builtin_ctzx(n);

		n >>= e;

		/* (n,e) pair */

		if (e < BOUNDARY_E && n < BOUNDARY_N)
			return;

		/* at this point, if hi_part(n) == 0 and e < LUT_SIZE, we could switch back to 64-bit implementation */
		if ( (unsigned long)(n>>64) == 0 && e < LUT_SIZE ) {
			check_ex((unsigned long)n, e);
			return;
		}
	} while (1);
}

/* check convergence */
void check(unsigned long n)
{
	do {
		int e;

		n >>= __builtin_ctzl(n);

		n++;

		e = __builtin_ctzl(n);

		n >>= e;

		/* (n,e) pair */

		/* all (n,e) below the following limits have already been checked for convergence */
		if (e < BOUNDARY_E && n < BOUNDARY_N)
			return;

		if (n > ULONG_MAX >> 2*e || e >= LUT_SIZE) {
			checkx_ex((uint128_t)n, e);
			return;
		}

		n *= g_lut[e];

		n--;
	} while (1);
}

int main(int argc, char *argv[])
{
	unsigned long n;
	unsigned long n_sup = 1UL << (
		(argc > 1) ? (unsigned long)atol(argv[1])
		: 32 );

	init_lut();

	/* trajectory of all numbers of the form 2l+0 passes (in finitely many steps) through some 2m+1 < 2l+0 */
	/* trajectory of all numbers of the form 2m+1 passes (in finitely many steps) through some 6n+2 > 2m+1 */
	/* therefore, check only the numbers the form 6n+2, in order from the smallest one to the largest one */
	#pragma omp parallel for
	for (n = 6+2; n < n_sup; n += 6) {
		check(n);
	}

	return 0;
}
