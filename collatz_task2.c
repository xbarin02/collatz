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

uint128_t g_lut[LUT_SIZE128];

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
		g_lut[a] = pow3x(a);
	}
}

/* check convergence */
static void check(uint128_t n)
{
	uint128_t n0 = n;
	int e;

	do {
#if 0
		/* Christian Hercher, Uber die Lange nicht-trivialer Collatz-Zyklen, Artikel in der Zeitschrift "Die Wurzel" Hefte 6 und 7/2018 */
		if (n <= UINT128_C(87)<<60)
			return;
#endif
		n++;

		e = __builtin_ctzx(n);

		n >>= e;

		/* now we have an (n,e) pair */
#if 1
		/* switch to 64-bit arithmetic */
		if ( n < UINT128_C(1)<<64 && e < LUT_SIZE ) {
			/* when the result of n*3^e and thus also odd_part(n*3^e-1) fits the 64-bit unsigned long, it is surely less than 87*2^60 */
			return;
		}
#endif
		assert( n <= UINT128_MAX >> 2*e && e < LUT_SIZE128 && "overflow" );

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzx(n);

		/* now we have a single n */

		if (n < n0)
			return;
	} while (1);
}

#define TASK_SIZE 38

int main(int argc, char *argv[])
{
	uint128_t n;
	uint128_t n_sup;
	unsigned long task_id = (argc > 1) ? (unsigned long)atol(argv[1]) : 0;

	printf("TASK_SIZE %lu\n", (unsigned long)TASK_SIZE);
	printf("TASK_ID %lu\n", task_id);

	assert( 2*sizeof(unsigned long) == sizeof(uint128_t) && "unsupported memory model" );

	/* start at */
	/* n of the form 4n+3 */
	n     = ( UINT128_C(task_id) << TASK_SIZE ) + 3;
	n_sup = ( UINT128_C(task_id) << TASK_SIZE ) + 3 + (1UL << TASK_SIZE);

	printf("RANGE 0x%016lx:%016lx .. 0x%016lx:%016lx\n", (unsigned long)(n>>64), (unsigned long)n, (unsigned long)(n_sup>>64), (unsigned long)n_sup);

	init_lut();

	/* #pragma omp parallel for */
	for (; n < n_sup; n += 4) {
		check(n);
	}

	printf("HALTED\n");

	return 0;
}
