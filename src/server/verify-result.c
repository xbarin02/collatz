/**
 * the question whether the gpuworker works correctly arises
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "compat.h"
#include "wideint.h"

#define TASK_SIZE 40

#define ASSIGNMENTS_NO (UINT64_C(1) << 32)

#define RECORDS_SIZE (ASSIGNMENTS_NO * 8)

const uint64_t *open_records(const char *path)
{
	int fd = open(path, O_RDONLY, 0600);
	const void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)RECORDS_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (const uint64_t *)ptr;
}

const uint64_t *g_checksums = 0;
const uint64_t *g_mxoffsets = 0;

#define LUT_SIZE128 81
#define LUT_SIZE64 41
#ifdef _USE_GMP
#	define LUT_SIZEMPZ 512
#endif

uint128_t g_lut[LUT_SIZE128];
uint64_t g_lut64[LUT_SIZE64];
#ifdef _USE_GMP
mpz_t g_mpz_lut[LUT_SIZEMPZ];
#endif

static uint64_t g_checksum_alpha = 0;
static uint128_t g_max = 0;
static uint128_t g_max_n0 = 0;

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
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}
#endif

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lut[a] = pow3x((uint128_t)a);
	}

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
mpz_t g_mpz_max;
uint128_t g_mpz_max_n0;
#endif

#ifdef _USE_GMP
static void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
{
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t n0;

	printf("OVERFLOW MPZ @ n0=0x%016" PRIx64 ":%016" PRIx64 " (not maximum)\n", (uint64_t)(n0_>>64), (uint64_t)n0_);

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

		/* handle maximum */
		if (mpz_cmp(n, g_mpz_max) > 0) {
			mpz_set(g_mpz_max, n);
			g_mpz_max_n0 = n0_;
		}

		beta = mpz_ctz(n);

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
}
#endif

static void check2(uint128_t n0, uint128_t n, int alpha)
{
	printf("OVERFLOW 128 @ n0=0x%016" PRIx64 ":%016" PRIx64 " (not maximum)\n", (uint64_t)(n0>>64), (uint64_t)n0);

	do {
		if (alpha >= LUT_SIZE128 || n > UINT128_MAX / g_lut[alpha]) {
#ifdef _USE_GMP
			mpz_check2(n0, n, alpha);
#else
			abort();
#endif
			return;
		}

		n *= g_lut[alpha];

		n--;

		if (n > g_max) {
			g_max = n;
			g_max_n0 = n0;
		}

		n >>= __builtin_ctzx(n);

		if (n < n0) {
			return;
		}

		n++;

		alpha = __builtin_ctzx(n);

		g_checksum_alpha += alpha;

		n >>= alpha;
	} while (1);
}

static void check(uint128_t n)
{
	const uint128_t n0 = n;
	int alpha;

	if (n == UINT128_MAX) {
		g_checksum_alpha += 128;
		check2(n0, UINT128_C(1), 128);
		return;
	}

	do {
		n++;

		alpha = __builtin_ctzx(n);

		g_checksum_alpha += alpha;

		n >>= alpha;

		if (n > UINT128_MAX >> 2*alpha || alpha >= LUT_SIZE128) {
			check2(n0, n, alpha);
			return;
		}

		n *= g_lut[alpha];

		n--;

		if (n > g_max) {
			g_max = n;
			g_max_n0 = n0;
		}

		n >>= __builtin_ctzx(n);

		if (n < n0) {
			return;
		}
	} while (1);
}

uint128_t get_max(uint128_t n0)
{
	int alpha, beta;
	uint128_t max_n = 0;
	uint128_t n = n0;

	assert(n0 != UINT128_MAX);

	do {
		n++;

		do {
			alpha = __builtin_ctzu64(n);
			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}
			n >>= alpha;
			if (n > UINT128_MAX >> 2*alpha) {
				return 0;
			}
			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

		if (n > max_n) {
			max_n = n;
		}

		do {
			beta = __builtin_ctzu64(n);
			n >>= beta;
		} while (!(n & 1));

		if (n < n0) {
			return max_n;
		}
	} while (1);
}

int main(int argc, char *argv[])
{
	uint64_t task_id = (argc > 1) ? atou64(argv[1]) : 0;
	uint64_t task_size = TASK_SIZE;
	uint128_t n;
	uint128_t n_min;
	uint128_t n_sup;
	uint64_t checksum;
	uint64_t mxoffset;
#ifdef _USE_GMP
	uint128_t mxoffset_n0;
	uint128_t maximum;
	mpz_t mpz_max_n0;
	mpz_t mpz_maximum;
#endif

	init_lut();

	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	g_checksums = open_records("checksums.dat");
	g_mxoffsets = open_records("mxoffsets.dat");

	checksum = g_checksums[task_id];

	printf("CHECKSUM %" PRIu64 " (recorded)\n", checksum);

	mxoffset = g_mxoffsets[task_id];

	if (mxoffset != 0) {
#ifdef _USE_GMP
		mxoffset_n0 = mxoffset + ((uint128_t)(task_id + 0) << task_size);
		maximum = get_max(mxoffset_n0);
		mpz_init_set_u128(mpz_max_n0, mxoffset_n0);
		mpz_init_set_u128(mpz_maximum, maximum);

		gmp_printf("GMP: maximum n0 = %Zi\n", mpz_max_n0);
		gmp_printf("GMP: maximum n  = %Zi\n", mpz_maximum);
#endif
	}

	assert((uint128_t)task_id <= (UINT128_MAX >> task_size));

	/* n of the form 4n+3 */
	n_min = ((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ((uint128_t)(task_id + 1) << task_size) + 3;

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)    ),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)    )
	);

#ifdef _USE_GMP
	mpz_init_set_ui(g_mpz_max, 0UL);
	g_mpz_max_n0 = 0;
#endif

	printf("[DEBUG] computing checksum...\n");

	for (n = n_min; n < n_sup; n += 4) {
		check(n);
	}

	printf("CHECKSUM %" PRIu64 " (computed)\n", g_checksum_alpha);

	if (checksum) {
		if (checksum != g_checksum_alpha) {
			printf("[ERROR] checksum does not match!\n");
		}
	}

#ifdef _USE_GMP
	if (1) {
		mpz_t max, max_n0;

		mpz_init_set_u128(max, g_max);

		if (g_mpz_max_n0 && mpz_cmp(max, g_mpz_max) < 0) {
			mpz_set(max, g_mpz_max);
			mpz_init_set_u128(max_n0, g_mpz_max_n0);
		} else {
			mpz_init_set_u128(max_n0, g_max_n0);
		}

		gmp_printf("MAXIMUM %Zd n0=%Zd (may differ from the original Collatz function)\n", max, max_n0);

		mpz_clear(max_n0);
		mpz_clear(max);
	}
#endif

	return 0;
}
