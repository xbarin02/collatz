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

#ifdef USE_SIEVE
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <sys/mman.h>
#	include <unistd.h>
#endif

#include "compat.h"
#include "wideint.h"

#ifdef USE_SIEVE
#	define SIEVE_LOGSIZE 32
#	define SIEVE_MASK ((1UL << SIEVE_LOGSIZE) - 1)
#	define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8) /* 2^k bits */
#	define IS_LIVE(n) ( ( g_map_sieve[ (n)>>3 ] >> ((n)&7) ) & 1 )
#endif

#define TASK_SIZE 40

#define LUT_SIZE64 41

#ifdef USE_SIEVE
const unsigned char *g_map_sieve;
#endif

uint64_t g_lut64[LUT_SIZE64];

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

#ifdef _USE_GMP
/* 3^n */
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}
#endif

/* init lookup table */
void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE64; ++a) {
		g_lut64[a] = pow3((uint64_t)a);
	}
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

#ifdef _USE_GMP
mpz_t g_mpz_max_n;
uint128_t g_mpz_max_n0;
#endif

static void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
{
#ifdef _USE_GMP
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t n0;
	mpz_t a;

	g_overflow_counter++;

	assert(alpha_ >= 0);
	alpha = (mp_bitcnt_t)alpha_;

	mpz_init(a);
	mpz_init_set_u128(n, n_);
	mpz_init_set_u128(n0, n0_);

	do {
		if (alpha >= LUT_SIZE64) {
			alpha = LUT_SIZE64 - 1;
		}

		/* n *= lut[alpha] */
		mpz_pow3(a, (unsigned long)alpha);

		mpz_mul(n, n, a);

		/* n-- */
		mpz_sub_ui(n, n, 1UL);

		/* if (n > max_n) */
		if (mpz_cmp(n, g_mpz_max_n) > 0) {
			mpz_set(g_mpz_max_n, n);
			g_mpz_max_n0 = n0_;
		}

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

	mpz_clear(a);
	mpz_clear(n);
	mpz_clear(n0);
#else
	(void)n0_;
	(void)n_;
	(void)alpha_;

	abort();
#endif
}

uint128_t g_max_n = 0;
uint128_t g_max_n0;

static void check(uint128_t n)
{
	const uint128_t n0 = n;
	int alpha, beta;

	assert(n != UINT128_MAX);

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

		if (n > g_max_n) {
			g_max_n = n;
			g_max_n0 = n0;
		}

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

#ifdef USE_SIEVE
const void *open_map(const char *path, size_t map_size)
{
	int fd = open(path, O_RDONLY, 0600);
	void *ptr;

	if (map_size == 0) {
		map_size = 1;
	}

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)map_size, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return ptr;
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
#ifdef USE_SIEVE
	char path[4096];
	size_t k = SIEVE_LOGSIZE;
	size_t map_size = SIEVE_SIZE;

	sprintf(path, "sieve-%lu.map", (unsigned long)k);

	g_map_sieve = open_map(path, map_size);
#endif

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

#ifdef USE_SIEVE
	printf("SIEVE_LOGSIZE %lu\n", (unsigned long)SIEVE_LOGSIZE);
#endif

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

	/* n of the form 4n+3 */
	n     = ((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ((uint128_t)(task_id + 1) << task_size) + 3;

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(n>>64), (uint64_t)n, (uint64_t)(n_sup>>64), (uint64_t)n_sup);

#ifdef _USE_GMP
	mpz_init_set_ui(g_mpz_max_n, 0UL);
#endif

	init_lut();

	for (; n < n_sup; n += 4) {
#ifdef USE_SIEVE
		if (IS_LIVE(n & SIEVE_MASK)) {
			check(n);
		}
#else
		check(n);
#endif
	}

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
		uint64_t secs = (t + 10000000/2) / 10000000;
		uint64_t usecs = (t + 10/2) / 10;
		printf("USERTIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
	}
#endif

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);

	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, g_checksum_beta);

#ifdef _USE_GMP
	if (1) {
		mpz_t t_max_n;

		mpz_init_set_u128(t_max_n, g_max_n);

		if (mpz_cmp(g_mpz_max_n, t_max_n) > 0) {
			g_max_n0 = g_mpz_max_n0;
		}

		mpz_clear(t_max_n);
	}
#endif

	printf("MAXIMUM_OFFSET %" PRIu64 "\n", (uint64_t)(g_max_n0 - ((uint128_t)(task_id + 0) << task_size)));

	printf("HALTED\n");

	return 0;
}
