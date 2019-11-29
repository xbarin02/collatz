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

#if defined(USE_PRECALC) && !defined(USE_SIEVE)
#	error Unsupported configuration
#endif

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
void mpz_init_set_u128(mpz_t rop, uint128_t op)
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

uint64_t mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
{
#ifdef _USE_GMP
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t n0;
	mpz_t a;
	uint64_t cycles = 0;

	g_overflow_counter++;

	assert(alpha_ >= 0);
	alpha = (mp_bitcnt_t)alpha_;

	mpz_init(a);
	mpz_init_set_u128(n, n_);
	mpz_init_set_u128(n0, n0_);

	do {
		if (alpha > ULONG_MAX) {
			alpha = ULONG_MAX;
		}

		/* n *= lut[alpha] */
		mpz_pow3(a, (unsigned long)alpha);

		mpz_mul(n, n, a);

		/* n-- */
		mpz_sub_ui(n, n, 1UL);

		cycles++;

		/* if (n > max_n) */
		if (mpz_cmp(n, g_mpz_max_n) > 0) {
			mpz_set(g_mpz_max_n, n);
			g_mpz_max_n0 = n0_;
		}

		beta = mpz_ctz(n);

		g_checksum_beta += beta;

		/* n >>= ctz(n) */
		mpz_fdiv_q_2exp(n, n, beta);

		/* all betas were factored out */

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

	return cycles;
#else
	(void)n0_;
	(void)n_;
	(void)alpha_;

	printf("ABORTED_DUE_TO_OVERFLOW\n");

	abort();
#endif
}

uint128_t g_max_n = 0;
uint128_t g_max_n0;
uint64_t g_maxcycles = 0;
uint128_t g_maxcycles_n0;

uint64_t check(uint128_t n)
{
	const uint128_t n0 = n;
	int alpha, beta;
	uint64_t cycles = 0;

	assert(n != UINT128_MAX);

	do {
		n++;

		do {
			alpha = __builtin_ctzu64(n);

			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}

			g_checksum_alpha += alpha;

			n >>= alpha;

			if (n > UINT128_MAX >> 2*alpha) {
				return cycles + mpz_check2(n0, n, alpha);
			}

			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

		cycles++;

		if (n > g_max_n) {
			g_max_n = n;
			g_max_n0 = n0;
		}

		do {
			beta = __builtin_ctzu64(n);

			g_checksum_beta += beta;

			n >>= beta;
		} while (!(n & 1));

		/* all betas were factored out */

		if (n < n0) {
			return cycles;
		}
	} while (1);
}

static uint64_t check2(uint128_t n0, uint128_t n)
{
	int alpha, beta;
	uint64_t cycles = 0;

	assert(n != UINT128_MAX);

	if (!(n & 1)) {
		goto even;
	}

	do {
		n++;

		do {
			alpha = __builtin_ctzu64(n);

			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}

			g_checksum_alpha += alpha;

			n >>= alpha;

			if (n > UINT128_MAX >> 2*alpha) {
				return cycles + mpz_check2(n0, n, alpha);
			}

			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

		cycles++;

	even:
		if (n > g_max_n) {
			g_max_n = n;
			g_max_n0 = n0;
		}

		do {
			beta = __builtin_ctzu64(n);

			g_checksum_beta += beta;

			n >>= beta;
		} while (!(n & 1));

		if (n < n0) {
			return cycles;
		}
	} while (1);
}

static void calc(uint64_t L, int R, int Salpha, int Sbeta, uint64_t task_size, uint64_t L0, uint64_t task_id, uint64_t cycles)
{
	uint128_t h;

	g_checksum_alpha += Salpha << (task_size - R);
	g_checksum_beta  += Sbeta  << (task_size - R);

	assert(R == Salpha + Sbeta);

	for (h = 0; h < (1UL << (task_size - R)); ++h) {
		uint128_t H = ((uint128_t)task_id << task_size) + (h << R);
		uint128_t N;
		uint128_t N0 = H + L0;
		uint64_t cycles2;

		assert(Salpha < LUT_SIZE64);

		N = (H >> (Salpha + Sbeta)) * g_lut64[Salpha] + L;

		cycles2 = cycles + check2(N0, N);

		if (cycles2 > g_maxcycles) {
			g_maxcycles = cycles2;
			g_maxcycles_n0 = N0;
		}
	}
}

/**
 * @param R remaining bits in 'n'
 */
void precalc(uint64_t n, int R, uint64_t task_size, uint64_t task_id)
{
	int R0 = R; /* copy of R */
	uint64_t L = n; /* only R-LSbits in n */
	uint64_t L0 = L;
	int alpha, beta;
	int Salpha = 0, Sbeta = 0; /* sum of alpha, beta */
	uint64_t cycles = 0;

	do {
		L++;

		do {
			alpha = __builtin_ctzu64(L);

			if (alpha > R) {
				alpha = R;
			}

			R -= alpha;
			Salpha += alpha;

			L >>= alpha;

			assert(L <= UINT64_MAX >> 2*alpha);

			L *= g_lut64[alpha];

			/* at this point, the L can be even (not all alpha were pulled out yet) or odd */
			/* independently, the R can be zero */
			if (R == 0) {
				L--;

				/* at this point, the L can be odd or even */
				calc(L, R0, Salpha, Sbeta, task_size, L0, task_id, cycles);
				return;
			}
		} while (!(L & 1));

		/* all alphas were pulled out and the L is now odd */

		L--;

		/* the 3n/2 sequence is complete */
		cycles++;

		if (L == 0) {
			/* no beta has been pulled out yet, the L is even */
			/* WARNING: not all R bits have been exhausted */
			beta = R;
			R -= beta;
			Sbeta += beta;
			calc(L, R0, Salpha, Sbeta, task_size, L0, task_id, cycles);
			return;
		}

		do {
			beta = __builtin_ctzu64(L);

			if (beta > R) {
				beta = R;
			}

			R -= beta;
			Sbeta += beta;

			L >>= beta;

			if (R == 0) {
				/* at least some (maybe all) betas were pulled out, the L can be even or odd */
				calc(L, R0, Salpha, Sbeta, task_size, L0, task_id, cycles);
				return;
			}
		} while (!(L & 1));

		/* all betas were factored out, the n/2 sequence is now complete */
	} while (1);
}

#define CHECK_WITH_CYCLES(n) \
	do { \
		uint64_t cycles = check(n); \
		if (cycles > g_maxcycles) { \
			g_maxcycles = cycles; \
			g_maxcycles_n0 = (n); \
		} \
	} while (0)

#ifdef USE_SIEVE
#	define CHECK_IF_LIVE(n) \
		if (IS_LIVE((n) & SIEVE_MASK)) { \
			CHECK_WITH_CYCLES(n); \
		}
#endif

#ifdef USE_SIEVE
#	define CHECK(n) CHECK_IF_LIVE(n)
#else
#	define CHECK(n) CHECK_WITH_CYCLES(n)
#endif

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

void report_usertime()
{
#ifndef __WIN32__
	struct rusage usage;
#else
	FILETIME ftCreation, ftExit, ftUser, ftKernel;
	HANDLE hProcess = GetCurrentProcess();
#endif

#ifndef __WIN32__
	assert(sizeof(uint64_t) >= sizeof(time_t));
	assert(sizeof(uint64_t) >= sizeof(suseconds_t));
	assert(sizeof(uint64_t) >= sizeof(time_t));

	/* user + system time */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
	} else {
		/* may wrap around */
		uint64_t usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec
		               + usage.ru_stime.tv_sec * UINT64_C(1000000) + usage.ru_stime.tv_usec;
		uint64_t secs = (usecs + 500000) / 1000000;

		printf("TIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
	}
#else
	if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
		uint64_t t = ((uint64_t)ftUser.dwHighDateTime << 32) + ftUser.dwLowDateTime;
		uint64_t secs = (t + 10000000/2) / 10000000;
		uint64_t usecs = (t + 10/2) / 10;
		printf("USERTIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
	}
#endif
}

void report_maximum(uint64_t task_id, uint64_t task_size)
{
#ifdef _USE_GMP
	if (1) {
		mpz_t t_max_n, mpz_maximum;

		/* max_n0 = g_max_n0 */
		mpz_init_set_u128(t_max_n, g_max_n);
		mpz_init_set(mpz_maximum, t_max_n); /* maximum = g_max_n */

		if (mpz_cmp(g_mpz_max_n, t_max_n) > 0) {
			g_max_n0 = g_mpz_max_n0; /* max_n0 = g_mpz_max_n0 */
			mpz_set(mpz_maximum, g_mpz_max_n); /* maximum = g_mpz_max_n */
		}

		gmp_printf("MAXIMUM %Zi\n", mpz_maximum);

		mpz_clear(mpz_maximum);
		mpz_clear(t_max_n);
	}
#endif

	printf("MAXIMUM_OFFSET %" PRIu64 "\n", (uint64_t)(g_max_n0 - ((uint128_t)(task_id + 0) << task_size)));
}

void report_prologue(uint64_t task_id, uint64_t task_size)
{
#ifdef USE_SIEVE
	printf("SIEVE_LOGSIZE %lu\n", (unsigned long)SIEVE_LOGSIZE);
#endif

	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)    ),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)    )
	);
}

void report_epilogue(uint64_t task_id, uint64_t task_size)
{
	report_usertime();

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);

	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, g_checksum_beta);

	report_maximum(task_id, task_size);

	printf("MAXIMUM_CYCLE %" PRIu64 "\n", g_maxcycles);
	printf("MAXIMUM_CYCLE_OFFSET %" PRIu64 "\n", (uint64_t)(g_maxcycles_n0 - ((uint128_t)(task_id + 0) << task_size)));

	printf("HALTED\n");
}

void init()
{
#ifdef USE_SIEVE
	char path[4096];
	size_t k = SIEVE_LOGSIZE;
	size_t map_size = SIEVE_SIZE;

	sprintf(path, "sieve-%lu.map", (unsigned long)k);

	g_map_sieve = open_map(path, map_size);
#endif
#ifdef _USE_GMP
	mpz_init_set_ui(g_mpz_max_n, 0UL);
#endif

	init_lut();
}

int parse_args(int argc, char *argv[], uint64_t *p_task_id, uint64_t *p_task_size)
{
	int opt;

	while ((opt = getopt(argc, argv, "t:a:")) != -1) {
		switch (opt) {
			unsigned long seconds;
			case 't':
				assert(p_task_size != NULL);
				*p_task_size = atou64(optarg);
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

	assert(p_task_id != NULL);

	*p_task_id = (optind < argc) ? atou64(argv[optind]) : 0;

	return 0;
}

void solve_task(uint64_t task_id, uint64_t task_size)
{
#ifdef USE_PRECALC
	uint64_t n;
	int R = SIEVE_LOGSIZE;
#else
	uint128_t n, n_min, n_sup;
#endif

#ifdef USE_PRECALC
	assert(task_size >= SIEVE_LOGSIZE);

	/* iterate over lowest R-bits */
	for (n = 0 + 3; n < (1UL << R) + 3; n += 4) {
#	ifdef USE_SIEVE
		if (IS_LIVE((n) & SIEVE_MASK)) {
#	else
		if (1) {
#	endif
			precalc(n, R, task_size, task_id);
		}
	}
#else
	/* n of the form 4n+3 */
	n_min = ((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ((uint128_t)(task_id + 1) << task_size) + 3;

	for (n = n_min; n < n_sup; n += 4) {
		CHECK(n);
	}
#endif
}

int main(int argc, char *argv[])
{
	uint64_t task_id = 0;
	uint64_t task_size = TASK_SIZE;
	int err;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	err = parse_args(argc, argv, &task_id, &task_size);

	if (err) {
		return err;
	}

	assert((uint128_t)(task_id + 1) <= (UINT128_MAX >> task_size));

	report_prologue(task_id, task_size);

	init();

	solve_task(task_id, task_size);

	report_epilogue(task_id, task_size);

	return 0;
}
