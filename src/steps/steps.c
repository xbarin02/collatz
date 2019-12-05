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

#define LUT_SIZE64 41

uint64_t g_lut64[LUT_SIZE64];

static uint64_t g_checksum_alpha = 0;
static uint64_t g_checksum_beta = 0;

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

static void check(uint128_t n)
{
#if (REACH_ONE == 0)
	uint128_t n0 = n;
#endif
	int alpha, beta;

	assert(n != UINT128_MAX);

#if (REACH_ONE == 0)
	while (n >= n0 && n != 1) {
#else
	while (n != 1) {
#endif
		n++;

		alpha = min(ctzu64(n), LUT_SIZE64 - 1);

		g_checksum_alpha += alpha;

		n >>= alpha;

		assert(n <= UINT128_MAX >> 2*alpha);

		n *= g_lut64[alpha];

		n--;

		beta = ctzu64(n);

		g_checksum_beta += beta;

		n >>= beta;
	}
}

int main(int argc, char *argv[])
{
	uint128_t n;
	uint64_t task_id = 0;

	task_id = (optind < argc) ? atou64(argv[optind]) : 0;

	printf("NUMBER %" PRIu64 "\n", task_id);

	n = (uint128_t)task_id;

	init_lut();

	check(n);

	printf("STEPS ODD=%" PRIu64 " EVEN=%" PRIu64 " TOTAL=%" PRIu64 "\n", g_checksum_alpha, g_checksum_beta, g_checksum_alpha + g_checksum_beta);

	return 0;
}
