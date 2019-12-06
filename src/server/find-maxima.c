#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif
#include "wideint.h"

#define TASK_SIZE 40

/* 2^32 assignments (tasks) */
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

const uint64_t *g_mxoffsets = 0;

void init()
{
	g_mxoffsets = open_records("mxoffsets.dat");
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

#ifdef _USE_GMP
void mpz_get_maximum(mpz_t max, uint128_t n0_)
{
	mpz_t n0;
	mpz_t n;

	mpz_init_set_u128(n0, n0_);
	mpz_init_set(n, n0);

	while (mpz_cmp(n, n0) >= 0) {
		if (mpz_congruent_ui_p(n, 1UL, 2UL)) {
			/* odd step */
			mpz_mul_ui(n, n, 3UL);
			mpz_add_ui(n, n, 1UL);
		} else {
			/* even step */
			mpz_fdiv_q_ui(n, n, 2UL);
		}

		if (mpz_cmp(n, max) > 0) {
			mpz_set(max, n);
		}
	}
}
#endif

mpz_t g_max;
uint128_t g_max_n0;

int main()
{
	uint64_t n;
#ifdef _USE_GMP
	mpz_t max;
#endif

	init();

#ifdef _USE_GMP
	mpz_init_set_ui(g_max, 0);
	mpz_init(max);
#endif

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		uint64_t mxoffset = g_mxoffsets[n];

		if (mxoffset) {
			uint128_t n0 = mxoffset + ((uint128_t)n << TASK_SIZE);

#ifdef _USE_GMP
			mpz_get_maximum(max, n0);

			if (mpz_cmp(max, g_max) > 0) {
				mpz_set(g_max, max);
				g_max_n0 = n0;

				gmp_printf("new maximum: n0 = ??? max = %Zi (bitsize %lu)\n", g_max, (unsigned long)mpz_sizeinbase(g_max, 2));
			}
#endif
		}
	}

	return 0;
}
