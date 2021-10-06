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
#include "wideint.h"
#include "compat.h"

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
const uint64_t *g_usertimes = 0;

#define LUT_SIZE64 41

uint64_t g_lut64[LUT_SIZE64];

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE64; ++a) {
		g_lut64[a] = pow3u64((uint64_t)a);
	}
}

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

uint128_t get_max(uint128_t n0)
{
	int alpha, beta;
	uint128_t max_n = 0;
	uint128_t n = n0;

	assert(n0 != UINT128_MAX);

	do {
		n++;

		do {
			alpha = ctzu64(n);
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
			beta = ctzu64(n);
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
	uint64_t checksum;
	uint64_t mxoffset;
	uint64_t usertime;
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
	g_usertimes = open_records("usertimes.dat");

	checksum = g_checksums[task_id];

	printf("CHECKSUM %" PRIu64 "\n", checksum);
	printf("PREFIX 0x%" PRIx64 "\n", checksum >> 24);

	mxoffset = g_mxoffsets[task_id];

	if (mxoffset != 0) {
#ifdef _USE_GMP
		mxoffset_n0 = mxoffset + ((uint128_t)(task_id + 0) << task_size);
		maximum = get_max(mxoffset_n0);
		mpz_init_set_u128(mpz_max_n0, mxoffset_n0);
		mpz_init_set_u128(mpz_maximum, maximum);

		gmp_printf("MAXIMUM_N0 %Zi\n", mpz_max_n0);
		gmp_printf("MAXIMUM %Zi\n", mpz_maximum);
#endif
	}

	assert((uint128_t)task_id <= (UINT128_MAX >> task_size));

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)    ),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)    )
	);

	usertime = g_usertimes[task_id];

	printf("TIME %" PRIu64 "\n", usertime);

	return 0;
}
