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

#define TASK_SIZE 32

#define LUT_SIZE64 41
#define LUT_SIZE128 81
#define LUT_SIZEMPZ 512

uint128_t g_lut[LUT_SIZE128];
mpz_t g_mpz_lut[LUT_SIZEMPZ];

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

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lut[a] = pow3x(a);
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

/* check convergence */
static int check(uint128_t n)
{
	const uint128_t n0 = n;
	int e;

	if (n == UINT128_MAX) {
		return 1;
	}

	do {
		n++;

		e = __builtin_ctzx(n);

		n >>= e;

		/* now we have an (n,e) pair */

		if (n > UINT128_MAX >> 2*e || e >= LUT_SIZE128) {
			return 1;
		}

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzx(n);

		/* now we have a single n */

		if (n < n0) {
			return 0;
		}
	} while (1);
}

/* check convergence */
static void mpz_check(unsigned long nh, unsigned long nl)
{
	mpz_t n;
	mpz_t n0;
	mp_bitcnt_t e;

	/* n = nh * 2^64 + nl */
	mpz_init_set_ui(n, nh);
	mpz_mul_2exp(n, n, (mp_bitcnt_t)64);
	mpz_add_ui(n, n, nl);

	/* n0 = n */
	mpz_init_set(n0, n);

	do {
		/* n++ */
		mpz_add_ui(n, n, 1UL);

		e = mpz_ctz(n);

		/* n >>= e */
		mpz_fdiv_q_2exp(n, n, e);

		/* now we have an (n,e) pair */

		assert( e < LUT_SIZEMPZ && "overflow" );

		/* n *= lut[e] */
		mpz_mul(n, n, g_mpz_lut[e]);

		/* n-- */
		mpz_sub_ui(n, n, 1UL);

		/* n >>= ctz(n) */
		mpz_fdiv_q_2exp(n, n, mpz_ctz(n));

		/* now we have a single n */

		if (mpz_cmp(n, n0) < 0) {
			break;
		}
	} while (1);

	mpz_clear(n);
	mpz_clear(n0);

	return;
}

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

int main(int argc, char *argv[])
{
	uint128_t n;
	uint128_t n_sup;
	unsigned long task_id = (argc > 1) ? (unsigned long)atol(argv[1]) : 0;

	printf("TASK_SIZE %lu\n", (unsigned long)TASK_SIZE);
	printf("TASK_ID %lu\n", task_id);

	assert( 2*sizeof(unsigned long) == sizeof(uint128_t) && "unsupported memory model" );

	/* n of the form 4n+3 */
	n     = ( UINT128_C(task_id) << TASK_SIZE ) + 3;
	n_sup = ( UINT128_C(task_id) << TASK_SIZE ) + 3 + (1UL << TASK_SIZE);

	printf("RANGE 0x%016lx:%016lx .. 0x%016lx:%016lx\n", (unsigned long)(n>>64), (unsigned long)n, (unsigned long)(n_sup>>64), (unsigned long)n_sup);

	init_lut();

	for (; n < n_sup; n += 4) {
		if (unlikely(check(n))) {
			/* the function cannot verify the convergence using 128-bit arithmetic, use libgmp */
			mpz_check((unsigned long)(n>>64), (unsigned long)n);
		}
	}

	printf("HALTED\n");

	return 0;
}
