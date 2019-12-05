/*
 * http://www.ericr.nl/wondrous/
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>

#include "wideint.h"
#include "compat.h"

#define REACH_ONE 1
#define START_VALUE 1
#define INCREMENT 1

#define LUT_SIZE64 41

uint64_t g_lut64[LUT_SIZE64];

uint64_t pow3(uint64_t n)
{
	uint64_t r = 1;

	for (; n > 0; --n) {
		assert(r <= UINT64_MAX / 3);

		r *= 3;
	}

	return r;
}

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE64; ++a) {
		g_lut64[a] = pow3((uint64_t)a);
	}
}

static int min(int a, int b)
{
	return a < b ? a : b;
}

double check(uint128_t n)
{
#if (REACH_ONE == 0)
	uint128_t n0 = n;
#endif
	uint64_t odd_steps = 0, even_steps = 0;
	int alpha, beta;

	assert(n != UINT128_MAX);

#if (REACH_ONE == 0)
	while (n >= n0 && n != 1) {
#else
	while (n != 1) {
#endif
		n++;

		alpha = min(ctzu64(n), LUT_SIZE64 - 1);

		odd_steps += alpha;

		n >>= alpha;

		assert(n <= UINT128_MAX >> 2*alpha);

		n *= g_lut64[alpha];

		n--;

		beta = ctzu64(n);

		even_steps += beta;

		n >>= beta;
	}

	return odd_steps / (double)even_steps;
}

int main()
{
	uint128_t n = START_VALUE;
	double C, max_C = 0.;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	init_lut();

	for (;; n += INCREMENT) {
		C = check(n);

		if (C == C && (C > max_C || n == START_VALUE)) {
			max_C = C;
			printf("%" PRIu64 " (%f), ", (uint64_t)n, max_C);
		}
	}

	return 0;
}
