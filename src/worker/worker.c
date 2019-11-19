/**
 * @file
 * @brief Simple program that check convergence of the Collatz problem.
 *
 * @author David Barina <ibarina@fit.vutbr.cz>
 */
#include <stdio.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#ifndef __WIN32__
#	include <sys/resource.h>
#else
#	include <windows.h>
#endif
#include <stdint.h>
#include <inttypes.h>

#include "compat.h"
#include "wideint.h"

#define TASK_SIZE 40

#define LUT_SIZE64 41
#ifdef _USE_GMP
#	define LUT_SIZEMPZ 512
#endif

uint64_t g_lut64[LUT_SIZE64];
#ifdef _USE_GMP
mpz_t g_mpz_lut[LUT_SIZEMPZ];
#endif

static uint64_t g_checksum_alpha = 0;
static uint64_t g_checksum_beta = 0;
static uint64_t g_overflow_counter = 0;

/* 3^n */
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

#ifdef _USE_GMP
/* 3^n */
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}
#endif

/* init lookup tables */
void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE64; ++a) {
		g_lut64[a] = pow3((uint64_t)a);
	}

#ifdef _USE_GMP
	for (a = 0; a < LUT_SIZEMPZ; ++a) {
		mpz_init(g_mpz_lut[a]);
		mpz_pow3(g_mpz_lut[a], (unsigned long)a);
	}
#endif
}

#ifdef _USE_GMP
/* count trailing zeros */
static mp_bitcnt_t mpz_ctz(const mpz_t n)
{
	return mpz_scan1(n, 0);
}
#endif

#ifdef _USE_GMP
static void mpz_init_set_u128(mpz_t rop, uint128_t op)
{
	uint64_t nh = (uint64_t)(op>>64);
	uint64_t nl = (uint64_t)(op);

	assert(sizeof(unsigned long) == sizeof(uint64_t));

	mpz_init_set_ui(rop, (unsigned long)nh);
	mpz_mul_2exp(rop, rop, (mp_bitcnt_t)64);
	mpz_add_ui(rop, rop, (unsigned long)nl);
}
#endif

static void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
{
#ifdef _USE_GMP
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t n0;

	assert(alpha_ >= 0);
	alpha = (mp_bitcnt_t)alpha_;

	mpz_init_set_u128(n, n_);
	mpz_init_set_u128(n0, n0_);

	do {
		if (alpha >= LUT_SIZEMPZ) {
			abort();
		}

		/* n *= lut[alpha] */
		mpz_mul(n, n, g_mpz_lut[alpha]);

		/* n-- */
		mpz_sub_ui(n, n, 1UL);

		beta = mpz_ctz(n);

		g_checksum_beta += beta;

		/* n >>= ctz(n) */
		mpz_fdiv_q_2exp(n, n, beta);

		if (mpz_cmp(n, n0) < 0) {
			break;
		}

		/* n++ */
		mpz_add_ui(n, n, 1UL);

		alpha = mpz_ctz(n);

		g_checksum_alpha += alpha;

		/* n >>= alpha */
		mpz_fdiv_q_2exp(n, n, alpha);
	} while (1);

	mpz_clear(n);
	mpz_clear(n0);
#else
	(void)n0_;
	(void)n_;
	(void)alpha_;

	abort();
#endif
}

static void check(uint128_t n)
{
	const uint128_t n0 = n;
	int alpha, beta;

	if (n == UINT128_MAX) {
		g_checksum_alpha += 128;
		mpz_check2(n0, UINT128_C(1), 128);
		return;
	}

	do {
		n++;

		alpha = __builtin_ctzu64(n);

		if (alpha >= LUT_SIZE64) {
			alpha = LUT_SIZE64 - 1;
		}

		g_checksum_alpha += alpha;

		n >>= alpha;

		if (n > UINT128_MAX >> 2*alpha) {
			mpz_check2(n0, n, alpha);
			return;
		}

		n *= g_lut64[alpha];

		n--;

		beta = __builtin_ctzu64(n);

		g_checksum_beta += beta;

		n >>= beta;

		if (n < n0) {
			return;
		}
	} while (1);
}

unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

#ifdef USE_MOD12
static uint128_t ceil_mod12(uint128_t n)
{
	return (n + 11) / 12 * 12;
}
#endif

int main(int argc, char *argv[])
{
	uint128_t n;
	uint128_t n_sup;
	uint64_t task_id = 0;
	uint64_t task_size = TASK_SIZE;
	int opt;
#ifndef __WIN32__
	struct rusage usage;
#else
	FILETIME ftCreation, ftExit, ftUser, ftKernel;
	HANDLE hProcess = GetCurrentProcess();
#endif
#ifdef USE_MOD12
	int k;
#endif

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while ((opt = getopt(argc, argv, "t:a:")) != -1) {
		switch (opt) {
			unsigned long seconds;
			case 't':
				task_size = atou64(optarg);
				break;
#ifndef __WIN32__
			case 'a':
				alarm(seconds = atoul(optarg));
				printf("ALARM %lu\n", seconds);
				break;
#endif
			default:
				fprintf(stderr, "Usage: %s [-t task_size] task_id\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	task_id = (optind < argc) ? atou64(argv[optind]) : 0;

	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	assert((uint128_t)(task_id + 1) <= (UINT128_MAX >> task_size));

#ifdef USE_MOD12
	/* n of the form 12n+3 */
	n     = ceil_mod12((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ceil_mod12((uint128_t)(task_id + 1) << task_size) + 3;
#else
	/* n of the form 4n+3 */
	n     = ((uint128_t)(task_id) << task_size) + 3;
	n_sup = ((uint128_t)(task_id) << task_size) + 3 + (UINT128_C(1) << task_size);
#endif

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(n>>64), (uint64_t)n, (uint64_t)(n_sup>>64), (uint64_t)n_sup);

	init_lut();

#ifdef USE_MOD12
	for (; n < n_sup; n += 12) {
		for (k = 0; k < 2; ++k) {
			check(n + 4*k);
		}
	}
#else
	for (; n < n_sup; n += 4) {
		check(n);
	}
#endif

#ifndef __WIN32__
	/* the total amount of time spent executing in user mode, expressed in a timeval structure (seconds plus microseconds) */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
	} else {
		if ((sizeof(uint64_t) >= sizeof(time_t)) &&
		    (sizeof(uint64_t) >= sizeof(suseconds_t)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_utime.tv_usec)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec;
			uint64_t usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec;
			if (secs < UINT64_MAX && (uint64_t)usage.ru_utime.tv_usec >= UINT64_C(1000000/2)) {
				/* round up */
				secs++;
			}
			printf("USERTIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
		} else if (sizeof(uint64_t) >= sizeof(time_t)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec;
			printf("USERTIME %" PRIu64 "\n", secs);
		}
	}

	/* user + system time */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
	} else {
		if ((sizeof(uint64_t) >= sizeof(time_t)) &&
		    (sizeof(uint64_t) >= sizeof(suseconds_t)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    (usage.ru_stime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    ((uint64_t)usage.ru_utime.tv_sec <= UINT64_MAX - usage.ru_stime.tv_sec) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_utime.tv_usec) &&
		    (usage.ru_stime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_stime.tv_usec) &&
		    ((usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec) <= UINT64_MAX - (usage.ru_stime.tv_sec * UINT64_C(1000000) + usage.ru_stime.tv_usec))) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec + usage.ru_stime.tv_sec;
			uint64_t usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec
			               + usage.ru_stime.tv_sec * UINT64_C(1000000) + usage.ru_stime.tv_usec;
			if (secs < UINT64_MAX && (uint64_t)usage.ru_utime.tv_usec + usage.ru_stime.tv_usec >= UINT64_C(1000000/2)) {
				/* round up */
				secs++;
			}
			printf("TIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
		} else if (sizeof(uint64_t) >= sizeof(time_t)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec + usage.ru_stime.tv_sec; /* may overflow */
			printf("TIME %" PRIu64 "\n", secs);
		}
	}
#else
	if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
		uint64_t t = ((uint64_t)ftUser.dwHighDateTime << 32) + ftUser.dwLowDateTime;
		/* Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC). */
		uint64_t secs = (t + 10000000/2) / 10000000;
		uint64_t usecs = (t + 10/2) / 10;
		printf("USERTIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
	}
#endif

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);

	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, g_checksum_beta);

	printf("HALTED\n");

	return 0;
}
