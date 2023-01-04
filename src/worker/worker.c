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
#include <sys/resource.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef USE_SIEVE
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <sys/mman.h>
#	include <unistd.h>
#endif

#include <time.h>

#include "wideint.h"
#include "compat.h"

#if defined(USE_PRECALC) && !defined(USE_SIEVE)
#	error Unsupported configuration
#endif

#ifdef USE_LUT50
static const uint64_t dict[] = {
	0x0000000000000000,
	0x0000000000000080,
	0x0000000008000000,
	0x0000000008000080,
	0x0000000080000000,
	0x0000000088000000,
	0x0000008000000000,
	0x0000008000000080,
	0x0000008008000000,
	0x0000008008000080,
	0x0000008080000000,
	0x0000008088000000,
	0x0000800000000000,
	0x0000800000000080,
	0x0000800008000000,
	0x0000800008000080,
	0x0000800080000000,
	0x0000800088000000,
	0x0000808000000000,
	0x0000808000000080,
	0x0000808008000000,
	0x0000808008000080,
	0x0800000000000000,
	0x0800008000000000,
	0x0800800000000000,
	0x0800808000000000,
	0x8000000000000000,
	0x8000000000000080,
	0x8000000008000000,
	0x8000000008000080,
	0x8000000080000000,
	0x8000000088000000,
	0x8000008000000000,
	0x8000008000000080,
	0x8000008008000000,
	0x8000008008000080,
	0x8000008080000000,
	0x8000008088000000,
	0x8000800000000000,
	0x8000800000000080,
	0x8000800008000000,
	0x8000800008000080,
	0x8000808000000000,
	0x8000808000000080,
	0x8000808008000000,
	0x8000808008000080,
	0x8800000000000000,
	0x8800008000000000,
	0x8800800000000000,
	0x8800808000000000
};
#endif

#ifdef USE_SIEVE
#	ifndef SIEVE_LOGSIZE
#		define SIEVE_LOGSIZE 32
#	endif
#	define SIEVE_MASK ((1UL << SIEVE_LOGSIZE) - 1)
#	ifdef USE_LUT50
#		define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8 / 8)
#		define GET_INDEX(n) (g_map_sieve[((n) & SIEVE_MASK) >> (3 + 3)])
#		define IS_LIVE(n) ((dict[GET_INDEX(n)] >> ((n) & 63)) & 1)
#	else
#		define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8)
#		define IS_LIVE(n) ((g_map_sieve[((n) & SIEVE_MASK) >> 3] >> (((n) & SIEVE_MASK) & 7)) & 1)
#	endif
#endif

#define TASK_SIZE 40

#define LUT_SIZE64 41

#ifdef USE_SIEVE
const unsigned char *g_map_sieve;
#endif

uint64_t g_lut64[LUT_SIZE64];

uint128_t g_max_ns[LUT_SIZE64];
unsigned long g_max_ns_ul[LUT_SIZE64];

static uint64_t g_checksum_alpha = 0;
static uint64_t g_overflow_counter = 0;

#ifdef _USE_GMP
/* 3^n */
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}
#endif

/* init lookup table */
void init_lut(void)
{
	int alpha;

	for (alpha = 0; alpha < LUT_SIZE64; ++alpha) {
		g_lut64[alpha] = pow3u64((uint64_t)alpha);

		g_max_ns[alpha] = UINT128_MAX >> 2 * alpha;

		if (2 * alpha < 64) {
			g_max_ns_ul[alpha] = UINT64_MAX >> 2 * alpha;
		} else {
			g_max_ns_ul[alpha] = 0;
		}
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
	uint64_t nh = (uint64_t)(op >> 64);
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

void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
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
		if (alpha > ULONG_MAX) {
			alpha = ULONG_MAX;
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

	return;
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

void check(uint128_t n, uint128_t n0)
{
	assert(n != UINT128_MAX);

	if (!(n & 1)) {
		goto even;
	}

	do {
		n++;

		do {
			int alpha = ctzu64(n);

			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}

			g_checksum_alpha += alpha;

			n >>= alpha;

			if (n > g_max_ns[alpha]) {
				mpz_check2(n0, n, alpha);
				return;
			}

			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

	even:
		if (n > g_max_n) {
			g_max_n = n;
			g_max_n0 = n0;
		}

		do {
			int beta = ctzu64(n);

			n >>= beta;
		} while (!(n & 1));

		if (n < n0) {
			return;
		}
	} while (1);
}

#ifdef USE_SIEVE3
HOT
static int is_live_in_sieve3(uint128_t n)
{
	uint64_t r = 0;

	r += (uint32_t)(n);
	r += (uint32_t)(n >> 32);
	r += (uint32_t)(n >> 64);
	r += (uint32_t)(n >> 96);

	/* n is not {2, 5, 8} (mod 9) */
	return r % 3 != 2;
}
#endif

#ifdef USE_SIEVE9
static int is_live_in_sieve9(uint128_t n)
{
	uint64_t r = 0;

	r += (uint64_t)(n)        & 0xfffffffffffffff;
	r += (uint64_t)(n >>  60) & 0xfffffffffffffff;
	r += (uint64_t)(n >> 120);

	r = r % 9;

	/* n is not {2, 4, 5, 8} (mod 9) */
	return r != 2 && r != 4 && r != 5 && r != 8;
}
#endif

static void calc(uint64_t task_id, uint64_t task_size, uint64_t L0, int R0, uint64_t L, int Salpha)
{
	uint128_t h;

#if !defined(USE_SIEVE3) && !defined(USE_SIEVE9)
	g_checksum_alpha += Salpha << (task_size - R0);
#endif

	for (h = 0; h < (1UL << (task_size - R0)); ++h) {
		uint128_t H = ((uint128_t)task_id << task_size) + (h << R0);
		uint128_t N;
		uint128_t N0 = H + L0;

#ifdef USE_SIEVE3
		if (!is_live_in_sieve3(N0)) {
			continue;
		}

		g_checksum_alpha += Salpha;
#endif
#ifdef USE_SIEVE9
		if (!is_live_in_sieve9(N0)) {
			continue;
		}

		g_checksum_alpha += Salpha;
#endif

		assert(Salpha < LUT_SIZE64);

		N = (H >> R0) * g_lut64[Salpha] + L;

		check(N, N0);
	}
}

/**
 * @param R remaining bits in 'n'
 */
void precalc(uint64_t task_id, uint64_t task_size, uint64_t L0, int R0)
{
	uint64_t L = L0; /* only R-LSbits in n */
	int Salpha = 0; /* sum of alphas */
	int R = R0; /* copy of R */

	do {
		L++;

		do {
			int alpha = ctzu64(L);

			if (alpha > R) {
				alpha = R;
			}

			/* The behavior is undefined if the right operand is [...] greater than or equal to the length in bits of the [...] left operand */
			if (alpha == 64) {
				alpha = 63;
			}

			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}

			R -= alpha;
			Salpha += alpha;

			L >>= alpha;

			assert(L <= g_max_ns_ul[alpha] || L <= UINT64_MAX / g_lut64[alpha]);

			L *= g_lut64[alpha];

			/* at this point, the L can be even (not all alpha were pulled out yet) or odd */
			/* independently, the R can be zero */
			if (R == 0) {
				L--;

				/* at this point, the L can be odd or even */
				calc(task_id, task_size, L0, R0, L, Salpha);
				return;
			}
		} while (!(L & 1));

		/* all alphas were pulled out and the L is now odd */

		L--;

		/* the 3n/2 sequence is complete */

		do {
			int beta = ctzu64(L);

			if (beta > R) {
				beta = R;
			}

			/* The behavior is undefined if the right operand is [...] greater than or equal to the length in bits of the [...] left operand */
			if (beta == 64) {
				beta = 63;
			}

			R -= beta;

			L >>= beta;

			if (R == 0) {
				/* at least some (maybe all) betas were pulled out, the L can be even or odd */
				calc(task_id, task_size, L0, R0, L, Salpha);
				return;
			}
		} while (!(L & 1));

		/* all betas were factored out, the n/2 sequence is now complete */
	} while (1);
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

	ptr = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return ptr;
}
#endif

uint64_t start_time;

void report_usertime(void)
{
	struct rusage usage;
	uint64_t usecs, secs;
	struct timespec ts;
	uint64_t stop_time;

	assert(sizeof(uint64_t) >= sizeof(time_t));
	assert(sizeof(uint64_t) >= sizeof(suseconds_t));
	assert(sizeof(uint64_t) >= sizeof(time_t));

	/* user + system time */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
		return;
	}

	/* may wrap around */
	usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec
	      + usage.ru_stime.tv_sec * UINT64_C(1000000) + usage.ru_stime.tv_usec;
	secs = (usecs + 500000) / 1000000;

	printf("TIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	stop_time = ts.tv_sec * UINT64_C(1000000000) + ts.tv_nsec;

	assert(stop_time > start_time);

	printf("REALTIME %" PRIu64 " %" PRIu64 "\n", (stop_time - start_time + 500000000) / 1000000000, (stop_time - start_time + 500) / 1000);
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
	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(((uint128_t)(task_id + 0) << task_size) >> 64),
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)      ),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size) >> 64),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)      )
	);
}

void report_epilogue(uint64_t task_id, uint64_t task_size)
{
	report_usertime();

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);

	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, UINT64_C(0));

	report_maximum(task_id, task_size);

	printf("MAXIMUM_CYCLE_OFFSET %" PRIu64 "\n", UINT64_C(0));

	printf("HALTED\n");
}

void init(void)
{
#ifdef USE_SIEVE
	char path[4096];
	size_t k = SIEVE_LOGSIZE;
	size_t map_size = SIEVE_SIZE;

#ifdef USE_LUT50
	sprintf(path, "esieve-%lu.lut50.map", (unsigned long)k);
#else
	sprintf(path, "esieve-%lu.map", (unsigned long)k);
#endif

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
			case 'a':
				alarm(seconds = atoul(optarg));
				printf("ALARM %lu\n", seconds);
				break;
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
	uint64_t n, n_min, n_sup;
	int R = SIEVE_LOGSIZE;

	assert(task_size >= SIEVE_LOGSIZE);

	/* n of the form 4n+3 */
	n_min = (0UL << R) + 3;
	n_sup = (1UL << R) + 3;

	/* iterate over lowest R-bits */
	for (n = n_min; n < n_sup; n += 4) {
		if (1
#	ifdef USE_SIEVE
		      && IS_LIVE(n)
#	endif
		) {
			precalc(task_id, task_size, n, R);
		}
	}
#else
	uint128_t n, n_min, n_sup;

	/* n of the form 4n+3 */
	n_min = ((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ((uint128_t)(task_id + 1) << task_size) + 3;

	for (n = n_min; n < n_sup; n += 4) {
		if (1
#	ifdef USE_SIEVE
		      && IS_LIVE(n)
#	endif
#	ifdef USE_SIEVE3
		      && is_live_in_sieve3(n)
#	endif
#	ifdef USE_SIEVE9
		      && is_live_in_sieve9(n)
#	endif
		) {
			check(n, n);
		}
	}
#endif
}

int main(int argc, char *argv[])
{
	uint64_t task_id = 0;
	uint64_t task_size = TASK_SIZE;
	int err;
	struct timespec ts;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	start_time = ts.tv_sec * UINT64_C(1000000000) + ts.tv_nsec;

	err = parse_args(argc, argv, &task_id, &task_size);

	if (err) {
		return err;
	}

	assert((uint128_t)(task_id + 1) <= (UINT128_MAX >> task_size));

	report_prologue(task_id, task_size);

	init();

	solve_task(task_id, task_size);

	report_epilogue(task_id, task_size);

#ifdef _USE_GMP
	mpz_clear(g_mpz_max_n);
#endif

	return 0;
}
