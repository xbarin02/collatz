/*
 * http://www.ericr.nl/wondrous/strengths.html
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

#define LUT_SIZE64 41

uint64_t g_lut64[LUT_SIZE64];

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE64; ++a) {
		g_lut64[a] = pow3u64((uint64_t)a);
	}
}

int64_t check(uint128_t n)
{
#if (REACH_ONE == 0)
	uint128_t n0 = n;
#endif
	uint64_t steps_alpha = 0, steps_beta = 0;
	int alpha, beta;

	assert(n != UINT128_MAX);

#if (REACH_ONE == 0)
	while (n >= n0 && n != 1) {
#else
	while (n != 1) {
#endif
		n++;

		alpha = ctzu64(n);

		if (alpha >= LUT_SIZE64) {
			alpha = LUT_SIZE64 - 1;
		}

		steps_alpha += alpha;

		n >>= alpha;

		assert(n <= UINT128_MAX >> 2*alpha);

		n *= g_lut64[alpha];

		n--;

		beta = ctzu64(n);

		steps_beta += beta;

		n >>= beta;
	}

	return 2 * steps_alpha - 3 * steps_beta;
}

int main()
{
	uint128_t n = 1;
	int64_t strength, max_strength = 0;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	init_lut();

	for (;; ++n) {
		strength = check(n);

		if (strength > max_strength || n == 1) {
			max_strength = strength;
			printf("%" PRIu64 ", ", (uint64_t)n);
		}
	}

	return 0;
}
