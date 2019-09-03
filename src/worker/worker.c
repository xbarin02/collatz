/**
 * @file
 * @brief Simple program that check convergence of the Collatz problem.
 *
 * @author David Barina <ibarina@fit.vutbr.cz>
 */
#define _XOPEN_SOURCE
#include <stdio.h>
#include <gmp.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>
#include <inttypes.h>

#include "wideint.h"

#define TASK_SIZE 40

#define LUT_SIZE64 41
#define LUT_SIZE128 81
#define LUT_SIZEMPZ 512

uint128_t g_lut[LUT_SIZE128];
mpz_t g_mpz_lut[LUT_SIZEMPZ];

/* 3^n */
uint128_t pow3x(uint128_t n)
{
	uint128_t r = 1;

	for (; n > 0; --n) {
		assert( r <= UINT128_MAX / 3 );

		r *= 3;
	}

	return r;
}

/* 3^n */
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}

/* init lookup tables */
void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lut[a] = pow3x((uint128_t)a);
	}

	for (a = 0; a < LUT_SIZEMPZ; ++a) {
		mpz_init(g_mpz_lut[a]);
		mpz_pow3(g_mpz_lut[a], (unsigned long)a);
	}
}

/* count trailing zeros */
static mp_bitcnt_t mpz_ctz(const mpz_t n)
{
	return mpz_scan1(n, 0);
}

static uint64_t g_check_sum = 0;

/* check convergence */
static int check(uint128_t n)
{
	const uint128_t n0 = n;
	int e;

	if (n == UINT128_MAX) {
		return 1;
	}

	do {
		n++;

		e = __builtin_ctzx(n);

		g_check_sum += e;

		n >>= e;

		/* now we have an (n,e) pair */

		if (n > UINT128_MAX >> 2*e || e >= LUT_SIZE128) {
			return 1;
		}

		n *= g_lut[e];

		n--;

		n >>= __builtin_ctzx(n);

		/* now we have a single n */

		if (n < n0) {
			return 0;
		}
	} while (1);
}

static uint64_t g_overflow_counter = 0;

static void mpz_init_set_u128(mpz_t rop, uint128_t op)
{
	uint64_t nh = (uint64_t)(op>>64);
	uint64_t nl = (uint64_t)(op);

	assert( sizeof(unsigned long) == sizeof(uint64_t) );

	mpz_init_set_ui(rop, (unsigned long)nh);
	mpz_mul_2exp(rop, rop, (mp_bitcnt_t)64);
	mpz_add_ui(rop, rop, (unsigned long)nl);
}

/* check convergence */
static void mpz_check(uint128_t n_)
{
	mpz_t n;
	mpz_t n0;
	mp_bitcnt_t e;

	g_overflow_counter++;

	mpz_init_set_u128(n, n_);

	/* n0 = n */
	mpz_init_set(n0, n);

	do {
		/* n++ */
		mpz_add_ui(n, n, 1UL);

		e = mpz_ctz(n);

		g_check_sum += e;

		/* n >>= e */
		mpz_fdiv_q_2exp(n, n, e);

		/* now we have an (n,e) pair */

		assert( e < LUT_SIZEMPZ && "overflow" );

		/* n *= lut[e] */
		mpz_mul(n, n, g_mpz_lut[e]);

		/* n-- */
		mpz_sub_ui(n, n, 1UL);

		/* n >>= ctz(n) */
		mpz_fdiv_q_2exp(n, n, mpz_ctz(n));

		/* now we have a single n */

		if (mpz_cmp(n, n0) < 0) {
			break;
		}
	} while (1);

	mpz_clear(n);
	mpz_clear(n0);

	return;
}

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

/* DEPRECATED */
static unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

static uint64_t atou64(const char *nptr)
{
	assert( sizeof(uint64_t) == sizeof(unsigned long) );

	return (uint64_t)atoul(nptr);
}

int main(int argc, char *argv[])
{
	uint128_t n;
	uint128_t n_sup;
	uint64_t task_id = 0;
	uint64_t task_size = TASK_SIZE;
	int opt;
	struct rusage usage;
	uint64_t usecs = 0;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while ((opt = getopt(argc, argv, "t:")) != -1) {
		switch (opt) {
			case 't':
				task_size = atou64(optarg);
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
	n     = ( UINT128_C(task_id) << task_size ) + 3;
	n_sup = ( UINT128_C(task_id) << task_size ) + 3 + (UINT64_C(1) << task_size);

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " .. 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(n>>64), (uint64_t)n, (uint64_t)(n_sup>>64), (uint64_t)n_sup);

	init_lut();

	for (; n < n_sup; n += 4) {
		if (unlikely(check(n))) {
			/* the function cannot verify the convergence using 128-bit arithmetic, use libgmp */
			mpz_check(n);
		}
	}

	/* the total amount of time spent executing in user mode, expressed in a timeval structure (seconds plus microseconds) */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
	} else {
		if ((sizeof(uint64_t) >= sizeof(time_t)) &&
		    (sizeof(uint64_t) >= sizeof(suseconds_t)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_utime.tv_usec)) {
			usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec;
			printf("USERTIME %" PRIu64 " %" PRIu64 "\n", (uint64_t)usage.ru_utime.tv_sec, usecs);
		} else if (sizeof(uint64_t) >= sizeof(time_t)) {
			printf("USERTIME %" PRIu64 "\n", (uint64_t)usage.ru_utime.tv_sec);
		}
	}

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);

	/* zero checksum is always invalid, so the server can implement simple valididty check */
	if (g_check_sum == 0)
		g_check_sum = ~0;

	printf("CHECKSUM %" PRIu64 "\n", g_check_sum);

	printf("HALTED\n");

	return 0;
}
