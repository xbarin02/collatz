#include "wideint.h"
#include "compat.h"
#include <assert.h>
#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif

#define ARR_LEN 43

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

void arr_init(int *arr)
{
	int i = 0;

	for (; i < ARR_LEN; ++i) {
		arr[i] = 0;
	}
}

uint128_t b_evaluate(int *arr)
{
	int exp = 0;
	uint128_t b = 0;

	for (; exp < ARR_LEN; ++exp) {
		assert(4 * (uint128_t)arr[exp] <= (UINT128_MAX - b) / pow3u128(exp));

		b += 4 * pow3u128(exp) * arr[exp];
	}

	return b;
}

int arr_increment(int *arr)
{
	int exp = 0;
	int c = 1; /* carry in */

	for (; exp < ARR_LEN; ++exp) {
		if (c) {
			if (arr[exp] == 1) {
				arr[exp] = 0;
				c = 1;
			} else {
				arr[exp] = 1;
				c = 0;
			}
		} else {
			break;
		}
	}

	/* carry out */
	if (c) {
		return 1;
	}

	return 0;
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

void mpz_check2(uint128_t n_, int alpha_)
{
#ifdef _USE_GMP
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t a;

	g_overflow_counter++;

	assert(alpha_ >= 0);
	alpha = (mp_bitcnt_t)alpha_;

	mpz_init(a);
	mpz_init_set_u128(n, n_);

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

		if (mpz_cmp_ui(n, 1UL) == 0) {
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

	return;
#else
	(void)n_;
	(void)alpha_;

	printf("ABORTED_DUE_TO_OVERFLOW\n");

	abort();
#endif
}

void check(uint128_t n)
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
				mpz_check2(n, alpha);
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

		if (n == 1) {
			return;
		}
	} while (1);
}

int main()
{
	uint128_t n;

	int arr[ARR_LEN];

	int i = 0;

	struct timespec ts;
	uint64_t start_time, stop_time;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	start_time = ts.tv_sec * 1000000000 + ts.tv_nsec;

	init_lut();

	arr_init(arr);

	printf("TARGET %i\n", ARR_LEN);

	while (1) {
		assert(pow3u128(ARR_LEN + 1) <= (UINT128_MAX - b_evaluate(arr) - 3) / 4);

		n = 4 * pow3u128(ARR_LEN + 1) + 3 + b_evaluate(arr);

		if (i++ == 0) {
			printf("smallest number: ");
			print(n);
		}

		check(n);

		if (arr_increment(arr) > 0) {
			printf("largest number : ");
			print(n);
			break;
		}
	}

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		printf("[ERROR] clock_gettime\n");
		abort();
	}

	stop_time = ts.tv_sec * 1000000000 + ts.tv_nsec;

	printf("REALTIME %" PRIu64 " %" PRIu64 "\n", (stop_time - start_time + 500000000) / 1000000000, (stop_time - start_time + 500) / 1000);
	printf("NUMBER_OF_TESTS %i\n", i);
	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);
	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, UINT64_C(0));

	return 0;
}
