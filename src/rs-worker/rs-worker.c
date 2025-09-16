#include "wideint.h"
#include "compat.h"
#include <assert.h>
#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif
#ifdef _OPENMP
	#include <omp.h>
#endif

#ifndef TARGET
#	define TARGET 44
#endif

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

void arr_init(uint64_t *arr)
{
	*arr = 0;
}

uint128_t b_evaluate(uint64_t arr)
{
	int exp = 0;
	uint128_t b = 0;

	for (; exp < TARGET; ++exp) {
		assert(4 * (arr & 1) <= (UINT128_MAX - b) / g_pow3[exp]);

		b += 4 * g_pow3[exp] * (arr & 1);

		arr >>= 1;
	}

	return b;
}

void arr_increment(uint64_t *arr)
{
	/* increment */
	(*arr)++;
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

static uint64_t g_checksum_alpha[1024];
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

void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_, uint64_t *l_checksum_alpha)
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

		*l_checksum_alpha += alpha;

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
	(void)l_checksum_alpha;

	printf("ABORTED_DUE_TO_OVERFLOW\n");

	abort();
#endif
}

void check(uint128_t n, uint128_t n0, uint64_t *l_checksum_alpha)
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
				*l_checksum_alpha += Salpha;
				mpz_check2(n0, n, alpha, l_checksum_alpha);
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
			*l_checksum_alpha += Salpha;
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

int main()
{
	struct timespec ts;
	uint64_t start_time, stop_time;

	int threads;
	uint64_t *arr;
	int t;
	uint64_t checksum = 0;
	uint64_t total_i = 0;
	int low_bits, high_bits;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	#pragma omp parallel
	{
		#pragma omp master
		threads = omp_get_num_threads();
	}

	assert((threads & (threads - 1)) == 0);

	high_bits = floor_log2(threads);
	low_bits = TARGET - high_bits;

	assert(low_bits >= 0);
	printf("low_bits = %i\n", low_bits);

	arr = malloc(sizeof(uint64_t) * threads);

	assert(arr != NULL);

	pow3_init();

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	start_time = ts.tv_sec * 1000000000 + ts.tv_nsec;

	init_lut();

	for (t = 0; t < threads; ++t) {
		arr_init(arr + t);
	}

	for (t = 0; t < 1024; ++t) {
		g_checksum_alpha[t] = 0;
	}

	printf("TARGET %i\n", TARGET);

	printf("LIMIT (all numbers below this must be already verified) ");
	print(4 * g_pow3[TARGET] + 2);

	printf("threads = %i\n", threads);

	assert(TARGET <= 64);
	assert(high_bits + low_bits <= 64);

	#pragma omp parallel num_threads(threads) reduction(+:checksum) reduction(+:total_i)
	{
		uint64_t i = 0;
		int tid = omp_get_thread_num();
		uint128_t n;

		/* arr[tid] = [ 0..0 | low_bits | tid ] */

		uint64_t arr_min = (uint64_t)tid;

		arr[tid] = arr_min;

		while (1) {
			assert(g_pow3[TARGET] <= (UINT128_MAX - b_evaluate(arr[tid]) - 3) / 4);

			n = 4 * g_pow3[TARGET] + 3 + b_evaluate(arr[tid]);

			if (i++ == 0 && tid == 0) {
				printf("smallest number: ");
				print(n);
			}

			check(n, n, g_checksum_alpha + tid);

			arr[tid] += UINT64_C(1) << high_bits;

			if ((arr[tid] & (((UINT64_C(1) << low_bits) - 1) << high_bits)) == 0) {
				if (tid == threads - 1) {
					printf("largest number : ");
					print(n);
				}

				break;
			}
		}

		checksum += *(g_checksum_alpha + tid);
		total_i += i;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	stop_time = ts.tv_sec * 1000000000 + ts.tv_nsec;

	printf("REALTIME %" PRIu64 " %" PRIu64 "\n", (stop_time - start_time + 500000000) / 1000000000, (stop_time - start_time + 500) / 1000);
	printf("NUMBER_OF_TESTS %" PRIu64 "\n", total_i);
	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);
	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", checksum, UINT64_C(0));
	printf("NEW_LIMIT (all numbers below this are now verified) ");
	print(4 * g_pow3[TARGET + 1] + 2);
	printf("SUCCESS\n");

	free(arr);

	return 0;
}
