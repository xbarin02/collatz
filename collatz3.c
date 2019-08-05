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

#define LUT_SIZE 32

unsigned long g_lut[LUT_SIZE];

/* 3^n, in runtime */
unsigned long lut_rt(unsigned long n)
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
		g_lut[a] = lut_rt(a);
	}
}

/* 3^n */
static unsigned long lut(unsigned long n)
{
	assert( n < LUT_SIZE );

	return g_lut[n];
}

/* check convergence */
void check(unsigned long n)
{
	unsigned long n0 = n/2;

	do {
		unsigned long e;

		n >>= __builtin_ctzl(n);

		/* 6n+2 < n0 (all numbers below n0 have already been checked) */
		if (n < n0 && n%3 == 1)
			return;

		n++;

		e = __builtin_ctzl(n);
		n >>= e;
		assert( n <= ULONG_MAX >> 2*e );
		n *= lut(e);

		n--;
	} while (1);
}

int main(int argc, char *argv[])
{
	unsigned long n;
	unsigned long n_max = (argc > 1) ? (unsigned long)atol(argv[1]) : (1UL<<31);

	init_lut();

	/* trajectory of all numbers of the form 2l+0 passes (in finitely many steps) through some 2m+1 < 2l+0 */
	/* trajectory of all numbers of the form 2m+1 passes (in finitely many steps) through some 6n+2 > 2m+1 */
	/* therefore, check only the numbers the form 6n+2, in order from the smallest one to the largest one */
	for (n = 6+2; n < n_max; n += 6) {
		check(n);
	}

	return 0;
}
