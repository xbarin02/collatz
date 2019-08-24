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

#define LUT_SIZE 41
#define LUT_SIZE128 81

typedef union {
	unsigned long ul[2];
	uint128_t ull;
} uint128_u;

uint128_u g_lutu[LUT_SIZE128];

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

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lutu[a].ull = pow3x(a);
	}
}

/* check convergence */
static void check(uint128_u n)
{
	uint128_u n0 = n;
	int e;

	do {
#if 1
		if (n.ull <= UINT128_C(87)<<60)
			return;
#endif
		n.ull++;

		e = __builtin_ctzx(n.ull);

		n.ull >>= e;

		/* (n,e) pair */
#if 1
		/* switch to 64-bit arithmetic */
		if ( n.ull < UINT128_C(1)<<64 && e < LUT_SIZE ) {
			return;
		}
#endif
		assert( n.ull <= UINT128_MAX >> 2*e && e < LUT_SIZE128 && "overflow" );

		n.ull *= g_lutu[e].ull;

		n.ull--;

		n.ull >>= __builtin_ctzx(n.ull);

		/* now we have a single n */

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

	printf("TASK_SIZE %lu\n", (unsigned long)TASK_SIZE);
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
