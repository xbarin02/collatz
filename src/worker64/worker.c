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
static uint64_t g_checksum_alpha = 0;
#endif
#ifdef ENABLE_BETA
static uint64_t g_checksum_beta = 0;
#endif
static uint64_t g_overflow64_counter = 0;

static void checkx2(uint128_t n0, uint128_t n, int alpha)
{
#ifdef ENABLE_BETA
	int beta;
#endif
	g_overflow64_counter++;

	do {
		if (alpha >= LUT_SIZE128 || n > UINT128_MAX / g_lut128[alpha]) {
#ifdef _USE_GMP
			mpz_check2(n0, n, alpha);
#else
			abort();
#endif
			return;
		}

		n *= g_lut128[alpha];

		n--;
#ifdef ENABLE_BETA
		beta = __builtin_ctzu128(n);

		g_checksum_beta += beta;
#endif
		n >>= __builtin_ctzu128(n);

		if (n < n0) {
			return;
		}

		n++;

		alpha = __builtin_ctzu128(n);
#ifdef ENABLE_ALPHA
		g_checksum_alpha += alpha;
#endif
		n >>= alpha;
	} while (1);
}

static void check(uint64_t n)
{
	const uint64_t n0 = n;
	int alpha;
#ifdef ENABLE_BETA
	int beta;
#endif

	if (n == UINT64_MAX) {
#ifdef ENABLE_ALPHA
		g_checksum_alpha += 64;
#endif
		checkx2(n0, UINT128_C(1), 64);
		return;
	}

	do {
		n++;

		alpha = __builtin_ctzu64(n);
#ifdef ENABLE_ALPHA
		g_checksum_alpha += alpha;
#endif
		n >>= alpha;

		if (n > UINT64_MAX >> 2*alpha || alpha >= LUT_SIZE64) {
			checkx2((uint128_t)n0, (uint128_t)n, alpha);
			return;
		}

		n *= g_lut64[alpha];

		n--;
#ifdef ENABLE_BETA
		beta = __builtin_ctzu64(n);

		g_checksum_beta += beta;
#endif
		n >>= __builtin_ctzu64(n);

		if (n < n0) {
			return;
		}
	} while (1);
}

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
		check(n);
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
	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, g_checksum_beta);
#elif defined(ENABLE_ALPHA)
	printf("CHECKSUM %" PRIu64 "\n", g_checksum_alpha);
#else
	printf("CHECKSUM\n");
#endif

	printf("HALTED\n");

	return 0;
}
