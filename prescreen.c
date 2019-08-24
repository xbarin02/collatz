/**
 * @file
 * @brief Simple program that check convergence of the Collatz problem.
 *
 * @author David Barina <ibarina@fit.vutbr.cz>
 */
#include <stdio.h>
#include <gmp.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include "wideint.h"

#define BOUNDARY_N (1UL<<44)
#define BOUNDARY_E 16

#define LUT_SIZE 41
#define LUT_SIZE128 81
#define LUT_SIZEMPZ 512

unsigned long g_lut[LUT_SIZE];

uint128_t g_lutx[LUT_SIZE128];

mpz_t g_mpz_lut[LUT_SIZEMPZ];

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

/* 3^n */
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}

/* init lookup tables */
void init_lut()
{
	unsigned long a;

	for (a = 0; a < LUT_SIZE; ++a) {
		g_lut[a] = pow3(a);
	}

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lutx[a] = pow3x(a);
	}

	for (a = 0; a < LUT_SIZEMPZ; ++a) {
		mpz_init(g_mpz_lut[a]);
		mpz_pow3(g_mpz_lut[a], a);
	}
}

/* count trailing zeros */
static mp_bitcnt_t mpz_ctz(const mpz_t n)
{
	return mpz_scan1(n, 0);
}

mpz_t bound_n;

void mpz_prescreen(unsigned long n0, unsigned long n_sup, int e0)
{
	mpz_t n;
	mp_bitcnt_t e;

	mpz_init_set_ui(n, n0);

	assert( sizeof(mp_bitcnt_t) >= sizeof(int) );
	e = (mp_bitcnt_t)e0;

	do {
		assert( e < LUT_SIZEMPZ );

		mpz_mul(n, n, g_mpz_lut[e]);

		mpz_sub_ui(n, n, 1UL);

		mpz_fdiv_q_2exp(n, n, mpz_ctz(n));

		/* now we have a single n */
		if (mpz_cmp(n, bound_n) < 0)
			return;

		mpz_add_ui(n, n, 1UL);

		e = mpz_ctz(n);
		mpz_fdiv_q_2exp(n, n, e);

		/* now we have (n,e) pair */

		/* all (n,e) pairs below the following boundary have been checked in previous runs of the pre-screening */
		if ( mpz_cmp_ui(n, BOUNDARY_N) < 0 && e < BOUNDARY_E ) {
			mpz_clear(n);
			return;
		}

		/* all (n,e) with n < n_sup and e < e0 have already been checked */
		if ( (mpz_cmp_ui(n, n_sup) < 0 && e < (mp_bitcnt_t)e0) || (mpz_cmp_ui(n, n0) < 0 && e == (mp_bitcnt_t)e0) ) {
			mpz_clear(n);
			return;
		}
	} while (1);
}

/* declaration */
void prescreenx_ex(uint128_t n0, uint128_t n_sup, int e0, uint128_t n, int e);

void prescreen_ex(unsigned long n0, unsigned long n_sup, int e0, unsigned long n, int e)
{
	do {
		/* we are unable to compute the next step in 64-bit arithmetic, swithc back to 128-bit */
		if ( (n > ULONG_MAX >> 2*e) || (e >= LUT_SIZE) ) {
			prescreenx_ex(n0, n_sup, e0, n, e);
			return;
		}

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzl(n);

		/* now we have a single n */
		if (1 /* every unsigned long < 87<<60 */)
			return;

		n++;

		e = __builtin_ctzl(n);
		n >>= e;

		/* now we have (n,e) pair */

		/* all (n,e) pairs below the following boundary have been checked in previous runs of the pre-screening */
		if ( n < BOUNDARY_N && e < BOUNDARY_E ) /* these constants should be promoted to uint128_t */
			return;

		/* all (n,e) with n < n_sup and e < e0 have already been checked */
		if ( (n < n_sup && e < e0) || (n < n0 && e == e0) )
			return;
	} while (1);
}

void prescreenx_ex(uint128_t n0, uint128_t n_sup, int e0, uint128_t n, int e)
{
	do {
		/* we are unable to compute the next step in 128-bit arithmetic */
		if ( (n > UINT128_MAX >> 2*e) || (e >= LUT_SIZE128) ) {
			/* NOTE: importantly, all these (initial) arguments fit into unsigned long type */
			mpz_prescreen((unsigned long)n0, (unsigned long)n_sup, e0);
			return;
		}

		n *= g_lutx[e];

		n--;

		n >>= __builtin_ctzx(n);

		/* now we have a single n */
		if (n <= UINT128_C(87)<<60)
			return;

		n++;

		e = __builtin_ctzx(n);
		n >>= e;

		/* now we have (n,e) pair */

		/* all (n,e) pairs below the following boundary have been checked in previous runs of the pre-screening */
		if ( n < BOUNDARY_N && e < BOUNDARY_E ) /* these constants should be promoted to uint128_t */
			return;

		/* all (n,e) with n < n_sup and e < e0 have already been checked */
		if ( (n < n_sup && e < e0) || (n < n0 && e == e0) )
			return;
#if 0
		/* at this point, if hi_part(n) == 0 and e < LUT_SIZE, we could switch back to 64-bit implementation */
		if ( (unsigned long)(n>>64) == 0 && e < LUT_SIZE ) {
			/* NOTE: importantly, all these (initial) arguments fit into unsigned long type */
			prescreen_ex((unsigned long)n0, (unsigned long)n_sup, e0, (unsigned long)n, e);
			return;
		}
#endif
	} while (1);
}

void prescreen(unsigned long n, unsigned long n_sup, int e)
{
	unsigned long n0 = n;
	int e0 = e;

	do {
		/* we are unable to compute the next step in 64-bit arithmetic */
		if ( (n > ULONG_MAX >> 2*e) || (e >= LUT_SIZE) ) {
			prescreenx_ex(n0, n_sup, e0, n, e);
			return;
		}

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzl(n);

		/* now we have a single n */

		n++;

		e = __builtin_ctzl(n);
		n >>= e;

		/* now we have (n,e) pair */

		/* all (n,e) pairs below the following boundary have been checked in previous runs of the pre-screening */
		if ( n < BOUNDARY_N && e < BOUNDARY_E )
			return;

		/* all (n,e) with n < n_sup and e < e0 have already been checked */
		if ( (n < n_sup && e < e0) || (n < n0 && e == e0) )
			return;
	} while (1);
}

int main(int argc, char *argv[])
{
	int e;
	unsigned long n;
	unsigned long n_sup = 1UL << (
		(argc > 1) ? (unsigned long)atol(argv[1])
		: 32 );

	assert( sizeof(unsigned long)*CHAR_BIT == 64 );
	assert( sizeof(uint128_t)*CHAR_BIT == 128 );

	init_lut();

	mpz_init_set_ui(bound_n, 87UL);
	mpz_mul_2exp(bound_n, bound_n, (mp_bitcnt_t)60);

	/* for each e */
	for (e = 1; ; ++e) {
		printf("e = %i...\n", e);
		if (e < BOUNDARY_E && n_sup <= BOUNDARY_N)
		    continue;
		/* for each odd n */
		#pragma omp parallel for
		for (n = 1; n < n_sup; n += 2) {
			if (n < BOUNDARY_N && e < BOUNDARY_E)
				continue;
			/* check */
			prescreen(n, n_sup, e);
		}
	}

	mpz_clear(bound_n);

	return 0;
}
