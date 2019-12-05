/*
 * https://oeis.org/A006877
 * https://oeis.org/A060412
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

#define REACH_ONE 0
#define SUM_ODD_STEPS 1
#define SUM_EVEN_STEPS 1

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

uint64_t check(uint128_t n)
{
#if (REACH_ONE == 0)
	uint128_t n0 = n;
#endif
	uint64_t steps = 0;
	int alpha, beta;

	assert(n != UINT128_MAX);

#if (REACH_ONE == 0)
	while (n >= n0 && n != 1) {
#else
	while (n != 1) {
#endif
		n++;

		alpha = min(ctzu64(n), LUT_SIZE64 - 1);

#if (SUM_ODD_STEPS == 1)
		steps += alpha;
#endif

		n >>= alpha;

		assert(n <= UINT128_MAX >> 2*alpha);

		n *= g_lut64[alpha];

		n--;

		beta = ctzu64(n);

#if (SUM_EVEN_STEPS == 1)
		steps += beta;
#endif

		n >>= beta;
	}

	return steps;
}

int main()
{
	uint128_t n = 1;
	uint64_t steps, max_steps = 0;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	init_lut();

	for (;; ++n) {
		steps = check(n);

		if (steps > max_steps || n == 1) {
			max_steps = steps;
			printf("%" PRIu64 ", ", (uint64_t)n);
		}
	}

	return 0;
}
