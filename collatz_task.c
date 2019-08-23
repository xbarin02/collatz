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
#define BOUNDARY_N_ATLEAST_E64 (1UL<<35)
#define BOUNDARY_N_ATLEAST_E128 (1UL<<33)
#define BOUNDARY_E 14

#define LUT_SIZE 41
#define LUT_SIZE128 81

typedef union {
	unsigned long ul[2];
	uint128_t ull;
} uint128_u;

uint128_u g_lutu[LUT_SIZE128];

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
		g_lutu[a].ul[1] = 0;
		g_lutu[a].ul[0] = pow3(a);
	}

	for (; a < LUT_SIZE128; ++a) {
		g_lutu[a].ull = pow3x(a);
	}
}

/* check convergence */
static void check(uint128_u n)
{
	uint128_u n0 = n;
	int e;

	if (n.ul[1] != 0)
		goto checkx;

	do {
		n.ul[0]++;

		e = __builtin_ctzl(n.ul[0]);

		n.ul[0] >>= e;

		/* (n,e) pair */

		/* all (n,e) below the following limits have already been checked for convergence */
#if 0
		if (/*e < BOUNDARY_E &&*/ n.ul[0] < BOUNDARY_N_ATLEAST_E64)
#else
		if (e < BOUNDARY_E && n.ul[0] < BOUNDARY_N)
#endif
			return;

		/* switch to 128-bit arithmetic */
		if (n.ul[0] > ULONG_MAX >> 2*e || e >= LUT_SIZE) {
			goto checkx_ex;
		}

	check_ex:

		n.ul[0] *= g_lutu[e].ul[0];

		n.ul[0]--;

		n.ul[0] >>= __builtin_ctzl(n.ul[0]);

		if (n.ul[0] < n0.ul[0] && n0.ul[1] == 0)
			return;
	} while (1);

checkx:

	do {
		n.ull++;

		e = __builtin_ctzx(n.ull);

		n.ull >>= e;

		/* (n,e) pair */

		/* all (n,e) below the following limits have already been checked for convergence */
#if 1
		if (/*e < BOUNDARY_E &&*/ n.ull < BOUNDARY_N_ATLEAST_E128)
#else
		if (e < BOUNDARY_E && n.ull < BOUNDARY_N)
#endif
			return;

		/* switch to 64-bit arithmetic */
		if ( n.ul[1] == 0 && e < LUT_SIZE ) {
			goto check_ex;
		}

	checkx_ex:

		assert( n.ull <= UINT128_MAX >> 2*e && e < LUT_SIZE128 && "overflow" );

		n.ull *= g_lutu[e].ull;

		n.ull--;

		n.ull >>= __builtin_ctzx(n.ull);

		if (n.ull < n0.ull)
			return;
	} while (1);
}

#define TASK_SIZE 38

int main(int argc, char *argv[])
{
	uint128_u n;
	uint128_t n_sup;
	unsigned long task_id = (argc > 1) ? (unsigned long)atol(argv[1]) : 0;

	printf("TAKS_SIZE %lu\n", (unsigned long)TASK_SIZE);
	printf("TASK_ID %lu\n", task_id);

	n.ull = UINT128_C(1)<<64;
	assert( n.ul[0] == 0 && n.ul[1] == 1 && "unsupported endianity" );
	assert( 2*sizeof(unsigned long) == sizeof(uint128_t) && "unsupported memory model" );

	/* start at */
	/* n of the form 4n+3 */
	n.ull = ( UINT128_C(task_id) << TASK_SIZE ) + 3;
	n_sup = ( UINT128_C(task_id) << TASK_SIZE ) + 3 + (1UL << TASK_SIZE);

	printf("RANGE 0x%016lx:%016lx .. 0x%016lx:%016lx\n", n.ul[1], n.ul[0], (unsigned long)(n_sup>>64), (unsigned long)n_sup);

	init_lut();

	/* #pragma omp parallel for */
	for (; n.ull < n_sup; n.ull += 4) {
		check(n);
	}

	printf("HALTED\n");

	return 0;
}
