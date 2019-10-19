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
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>
#include <inttypes.h>

#include "compat.h"
#include "wideint.h"

#ifdef __GNUC__
#	define likely(x)   __builtin_expect((x), 1)
#	define unlikely(x) __builtin_expect((x), 0)
#endif

#define TASK_SIZE 40

#define LUT_SIZE64 41
#define LUT_SIZE128 81

uint64_t g_lut64[LUT_SIZE64];
uint128_t g_lut128[LUT_SIZE128];

uint64_t pow3(uint64_t n)
{
	uint64_t r = 1;

	for (; n > 0; --n) {
		assert(r <= UINT64_MAX / 3);

		r *= 3;
	}

	return r;
}

uint128_t pow3x(uint128_t n)
{
	uint128_t r = 1;

	for (; n > 0; --n) {
		assert(r <= UINT128_MAX / 3);

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

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lut128[a] = pow3x((uint128_t)a);
	}
}

#ifdef ENABLE_ALPHA
static uint64_t g_check_sum_alpha = 0;
#endif
#ifdef ENABLE_BETA
static uint64_t g_check_sum_beta = 0;
#endif

static int check(uint64_t n)
{
	const uint64_t n0 = n;
	int alpha;
#ifdef ENABLE_BETA
	int beta;
#endif

	if (n == UINT64_MAX) {
		return 1;
	}

	do {
		n++;

		alpha = __builtin_ctzu64(n);
#ifdef ENABLE_ALPHA
		g_check_sum_alpha += alpha;
#endif
		n >>= alpha;

		if (n > UINT64_MAX >> 2*alpha || alpha >= LUT_SIZE64) {
			return 1;
		}

		n *= g_lut64[alpha];

		n--;
#ifdef ENABLE_BETA
		beta = __builtin_ctzu64(n);

		g_check_sum_beta += beta;
#endif
		n >>= __builtin_ctzu64(n);

		if (n < n0) {
			return 0;
		}
	} while (1);
}

static int checkx(uint128_t n)
{
	const uint128_t n0 = n;
	int alpha;
#ifdef ENABLE_BETA
	int beta;
#endif

	if (n == UINT128_MAX) {
		return 1;
	}

	do {
		n++;

		alpha = __builtin_ctzu128(n);
#ifdef ENABLE_ALPHA
		g_check_sum_alpha += alpha;
#endif
		n >>= alpha;

		if (n > UINT128_MAX >> 2*alpha || alpha >= LUT_SIZE128) {
			return 1;
		}

		n *= g_lut128[alpha];

		n--;
#ifdef ENABLE_BETA
		beta = __builtin_ctzu128(n);

		g_check_sum_beta += beta;
#endif
		n >>= __builtin_ctzu128(n);

		if (n < n0) {
			return 0;
		}
	} while (1);
}

static uint64_t g_overflow64_counter = 0;

unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

int main(int argc, char *argv[])
{
	uint64_t n;
	uint64_t n_sup;
	uint64_t task_id = 0;
	uint64_t task_size = TASK_SIZE;
	int opt;
	struct rusage usage;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while ((opt = getopt(argc, argv, "t:a:")) != -1) {
		switch (opt) {
			unsigned long seconds;
			case 't':
				task_size = atou64(optarg);
				break;
			case 'a':
				alarm(seconds = atoul(optarg));
				printf("ALARM %lu\n", seconds);
				break;
			default:
				fprintf(stderr, "Usage: %s [-t task_size] task_id\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	task_id = (optind < argc) ? atou64(argv[optind]) : 0;

	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	/* n of the form 4n+3 */
	n     = ((uint64_t)(task_id) << task_size) + 3;
	n_sup = ((uint64_t)(task_id) << task_size) + 3 + (UINT64_C(1) << task_size);

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		UINT64_C(0), (uint64_t)n, UINT64_C(0), (uint64_t)n_sup);

	init_lut();

	for (; n < n_sup; n += 4) {
		if (unlikely(check(n))) {
			g_overflow64_counter++;
			/* FIXME: correct the alpha & beta checksums */
			if (unlikely(checkx((uint128_t)n))) {
				printf("worker error: cannot verify the convergence using 128-bit arithmetic :(\n");
				abort();
			}
		}
	}

	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		perror("getrusage");
	} else {
		if ((sizeof(uint64_t) >= sizeof(time_t)) &&
		    (sizeof(uint64_t) >= sizeof(suseconds_t)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_utime.tv_usec)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec;
			uint64_t usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec;
			if (secs < UINT64_MAX && (uint64_t)usage.ru_utime.tv_usec >= UINT64_C(1000000/2)) {
				secs++;
			}
			printf("USERTIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
		} else if (sizeof(uint64_t) >= sizeof(time_t)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec;
			printf("USERTIME %" PRIu64 "\n", secs);
		}
	}

	printf("OVERFLOW 64 %" PRIu64 "\n", g_overflow64_counter);

#if defined(ENABLE_ALPHA) && defined(ENABLE_BETA)
	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_check_sum_alpha, g_check_sum_beta);
#elif defined(ENABLE_ALPHA)
	printf("CHECKSUM %" PRIu64 "\n", g_check_sum_alpha);
#else
	printf("CHECKSUM\n");
#endif

	printf("HALTED\n");

	return 0;
}
