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

#define BOUNDARY_N (1UL<<39)
#define BOUNDARY_E 34

#define LUT_SIZE 41

unsigned long g_lut[LUT_SIZE];

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

void init_lut()
{
	unsigned long a;

	for (a = 0; a < LUT_SIZE; ++a) {
		g_lut[a] = pow3(a);
	}
}

/* check convergence */
void check(unsigned long n)
{
	unsigned long n0 = n;

	do {
		unsigned long e;

		n++;

		e = __builtin_ctzl(n);

		n >>= e;

		/* (n,e) pair */

		/* all (n,e) below the following limits have already been checked for convergence */
		if (e < BOUNDARY_E && n < BOUNDARY_N)
			return;

		assert( n <= ULONG_MAX >> 2*e );

		assert( e < LUT_SIZE );

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
