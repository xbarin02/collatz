#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "compat.h"
#include "wideint.h"

#ifdef __GNUC__
#	define likely(x)   __builtin_expect((x), 1)
#	define unlikely(x) __builtin_expect((x), 0)
#endif

uint128_t pow3x(uint128_t n)
{
	uint128_t r = 1;

	for (; n > 0; --n) {
		assert( r <= UINT128_MAX / 3 );

		r *= 3;
	}

	return r;
}

#define LUT_SIZE128 81

uint128_t g_lut[LUT_SIZE128];

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lut[a] = pow3x((uint128_t)a);
	}
}

static int check(uint128_t n)
{
	const uint128_t n0 = n;
	int alpha;

	if (n == UINT128_MAX) {
		return 1;
	}

	do {
		n++;

		alpha = __builtin_ctzx(n);

		n >>= alpha;

		if (n > UINT128_MAX >> 2*alpha || alpha >= LUT_SIZE128) {
			return 1;
		}

		n *= g_lut[alpha];

		n--;

		n >>= __builtin_ctzx(n);

		if (n < n0) {
			return 0;
		}
	} while (1);
}

#define TASK_SIZE 40

int main(int argc, char *argv[])
{
	uint64_t task_id = (argc > 1) ? atou64(argv[1]) : 0;
	uint64_t task_size = TASK_SIZE;
	uint128_t n;
	uint128_t n_sup;

	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	/* n of the form 4n+3 */
	n     = (UINT128_C(task_id) << task_size) + 3;
	n_sup = (UINT128_C(task_id) << task_size) + 3 + (UINT64_C(1) << task_size);

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(n>>64), (uint64_t)n, (uint64_t)(n_sup>>64), (uint64_t)n_sup);

	init_lut();

	for (; n < n_sup; n += 4) {
		if (unlikely(check(n))) {
			/* the function cannot verify the convergence using 128-bit arithmetic */
			printf("0x%016" PRIx64 ":%016" PRIx64 "\n", (uint64_t)(n>>64), (uint64_t)n);
		}
	}

	return 0;
}
