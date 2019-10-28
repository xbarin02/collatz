/*
 * https://oeis.org/A006877
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>

#include "compat.h"
#include "wideint.h"

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

uint64_t check(uint128_t n)
{
	uint64_t steps = 0;
	int alpha, beta;

	assert(n != UINT128_MAX);

	do {
		n++;

		alpha = min(__builtin_ctzu64(n), LUT_SIZE64 - 1);

		steps += alpha;

		n >>= alpha;

		assert(n <= UINT128_MAX >> 2*alpha);

		n *= g_lut64[alpha];

		n--;

		beta = __builtin_ctzu64(n);

		steps += beta;

		n >>= beta;
	} while (n != 1);

	return steps;
}

int main()
{
	uint128_t n = 2;
	uint64_t steps, max_steps = 0;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	init_lut();

	for (;; ++n) {
		steps = check(n);

		if (steps > max_steps) {
			printf("%" PRIu64 ", ", (uint64_t)n);
			max_steps = steps;
		}
	}

	return 0;
}
