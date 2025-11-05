#include "wideint.h"
#include "compat.h"
#include <assert.h>
#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif
#ifdef _OPENMP
#	include <omp.h>
#endif

#ifdef USE_SIEVE
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <sys/mman.h>
#	include <unistd.h>
#endif

#define DEFAULT_TARGET 28

int g_target = DEFAULT_TARGET;
int g_no_procs = 64;
int g_tid = 0;

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
#		define SIEVE_LOGSIZE 24
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

#ifdef USE_SIEVE
const unsigned char *g_map_sieve;
#endif

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

	printf("SIEVE %lu\n", (unsigned long)k);
#else
	printf("SIEVE 0\n");
#endif
}

uint128_t pow3u128(uint128_t n)
{
	uint128_t r = 1;
	uint128_t b = 3;

	while (n) {
		if (n & 1) {
			assert(r <= UINT128_MAX / b);
			r *= b;
		}

		assert(b <= UINT128_MAX / b);
		b *= b;

		n >>= 1;
	}

	return r;
}

uint128_t g_pow3[64];

void pow3_init()
{
	int i = 0;
	for (; i < 64; ++i) {
		g_pow3[i] = pow3u128(i);
	}
}

void print(uint128_t n)
{
	char buff[40];
	char r_buff[40];

	char *ptr = buff;

	int i = 0;

	while (n != 0) {
		*(ptr++) = '0' + n % 10;
		n /= 10;
	}

	*ptr = 0;

	ptr--;

	while (ptr >= buff) {
		r_buff[i++] = *(ptr--);
	}

	r_buff[i] = 0;

	puts(r_buff);
}

uint128_t b_evaluate(uint64_t arr)
{
	int exp = 0;
	uint128_t b = 0;

	for (; exp < g_target; ++exp) {
		assert((arr & 1) <= (UINT128_MAX - b) / g_pow3[exp]);

		b += g_pow3[exp] * (arr & 1);

		arr >>= 1;
	}

	assert(b <= UINT128_MAX / 4);

	b *= 4;

	return b;
}

#ifdef _USE_GMP
/* 3^n */
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}
#endif

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

#define LUT_SIZE64 41

uint64_t g_lut64[LUT_SIZE64];
uint128_t g_max_ns[LUT_SIZE64];

static uint64_t g_checksum_alpha = 0;
static uint64_t g_overflow_counter = 0;

/* init lookup table */
void init_lut(void)
{
	int alpha;

	for (alpha = 0; alpha < LUT_SIZE64; ++alpha) {
		g_lut64[alpha] = pow3u64((uint64_t)alpha);

		g_max_ns[alpha] = UINT128_MAX >> 2 * alpha;
	}
}

void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
{
#ifdef _USE_GMP
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t n0;
	mpz_t a;

	#pragma omp critical
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

void check(uint128_t n, uint128_t n0)
{
	int Salpha = 0;

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

			Salpha += alpha;

			n >>= alpha;

			if (n > g_max_ns[alpha]) {
				g_checksum_alpha += Salpha;
				mpz_check2(n0, n, alpha);
				return;
			}

			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

	even:
		do {
			int beta = ctzu64(n);

			n >>= beta;
		} while (!(n & 1));

		if (n < n0) {
			g_checksum_alpha += Salpha;
			return;
		}
	} while (1);
}

size_t floor_log2(int n)
{
	size_t r = 0;

	assert(n != 0);

	while (n >>= 1) {
		r++;
	}

	return r;
}

int parse_args(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "t:n:N:i:a:")) != -1) {
		switch (opt) {
			unsigned long seconds;
			case 't':
				g_target = atoi(optarg);
				assert(g_target > 0);
				break;
			case 'n':
				g_no_procs = atoi(optarg);
				assert(g_no_procs > 0);
				break;
			case 'N':
				g_no_procs = 1UL << atoi(optarg);
				assert(g_no_procs > 0);
				break;
			case 'i':
				g_tid = atoi(optarg);
				assert(g_tid >= 0);
				break;
			case 'a':
				alarm(seconds = atoul(optarg));
				printf("ALARM %lu\n", seconds);
				break;
			default:
				fprintf(stderr, "Usage: %s [-t target] [-n no_procs] [-i task_id]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Unexpected arguments\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct timespec ts;
	uint64_t start_time, stop_time;

	uint64_t arr;
	uint64_t total_i = 0;
	int low_bits, high_bits;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	parse_args(argc, argv);

	assert((g_no_procs & (g_no_procs - 1)) == 0);

	high_bits = floor_log2(g_no_procs);
	low_bits = g_target - high_bits;

	assert(low_bits >= 0);

	pow3_init();

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	start_time = ts.tv_sec * 1000000000 + ts.tv_nsec;

	init_lut();

	init(); /* sieve */

	arr = 0;

	printf("TARGET %i\n", g_target);

	printf("OLD_LIMIT ");
	print(4 * g_pow3[g_target] + 2);

	printf("NO_PROCS %i\n", g_no_procs);

	assert(g_target <= 64);
	assert(high_bits + low_bits <= 64);

	{
		uint64_t i = 0;
		uint128_t n;

		/* arr[tid] = [ 0..0 | low_bits | tid ] */

		uint64_t arr_min = (uint64_t)g_tid << low_bits;

		assert(arr_min < (UINT64_C(1) << g_target));

		arr = arr_min;

		while (1) {
			assert(g_pow3[g_target] <= (UINT128_MAX - b_evaluate(arr) - 3) / 4);

			n = 4 * g_pow3[g_target] + 3 + b_evaluate(arr);

			if (i++ == 0 && g_tid == 0) {
				printf("DEBUG smallest number: ");
				print(n);
			}

			if (1
#	ifdef USE_SIEVE
				&& IS_LIVE(n)
#	endif
			) {
				check(n, n);
			}

			arr++;

			if ((arr & ((UINT64_C(1) << low_bits) - 1)) == 0) {
				if (g_tid == g_no_procs - 1) {
					printf("DEBUG largest number : ");
					print(n);
				}

				break;
			}
		}

		total_i += i;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	stop_time = ts.tv_sec * 1000000000 + ts.tv_nsec;

	printf("REALTIME %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", (stop_time - start_time + 500000000) / 1000000000, (stop_time - start_time + 500) / 1000, (stop_time - start_time + 500000) / 1000000);
	printf("NUMBER_OF_TESTS %" PRIu64 "\n", total_i);

	printf("SPEED %f nsecs/number\n", (stop_time - start_time) /* nsec */ / (double)(UINT64_C(1) << g_target));

	printf("SPEED %f numbers/nsec\n", (double)(UINT64_C(1) << g_target) / (stop_time - start_time) /* nsec */);
	printf("SPEED %g numbers/sec\n", (double)(UINT64_C(1) << g_target) / (stop_time - start_time) /* nsec */ * 1000 * 1000 * 1000);

	assert((total_i & (total_i - 1)) == 0);
	assert((int)floor_log2(total_i) == g_target - high_bits);

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);
	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, UINT64_C(0));
	printf("NEW_LIMIT ");
	print(4 * g_pow3[g_target + 1] + 2);
	printf("SUCCESS\n");

	return 0;
}
