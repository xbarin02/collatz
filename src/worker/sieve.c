#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "wideint.h"

uint128_t pow3(size_t n)
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

uint128_t T(uint128_t n)
{
	switch (n % 2) {
		case 0: return n / 2;
		case 1:
			assert(n <= (UINT128_MAX - 1) / 3);
			return (3 * n + 1) / 2;
	}

	return 0;
}

uint128_t T_k(uint128_t n, size_t k, size_t *o)
{
	size_t l;

	assert(o != NULL);

	*o = 0; /* odd steps */

	for (l = 0; l < k; ++l) {
		switch (n % 2) {
			case 0: /* even step */
				break;
			case 1: /* odd step */
				(*o)++;
				break;
		}

		n = T(n);
	}

	return n;
}

int is_killed_at_k(uint128_t b, size_t k)
{
	size_t c;

	/* a 2^k + b --> a 3^c + d */
	uint128_t d = T_k(b, k, &c);

	assert(UINT128_C(1) <= (UINT128_MAX >> k));
	
	return pow3(c) < (UINT128_C(1) << k) && d <= b;
}

int is_killed_below_k(uint128_t b, size_t k)
{
	while (--k > 0) {
		assert(UINT128_C(1) <= (UINT128_MAX >> k));

		if (is_killed_at_k(b % (UINT128_C(1) << k), k)) {
			return 1;
		}
	}

	return 0;
}

void *open_map(const char *path, size_t map_size)
{
	int fd = open(path, O_RDWR | O_CREAT, 0600);
	void *ptr;

	if (map_size == 0) {
		map_size = 1;
	}

	if (fd < 0) {
		perror("open");
		abort();
	}

	if (ftruncate(fd, (off_t)map_size) < 0) {
		perror("ftruncate");
		abort();
	}

	ptr = mmap(NULL, (size_t)map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return ptr;
}

unsigned char *g_map_sieve;

#define SET_LIVE(n) ( g_map_sieve[(n)>>3] |= (1<<((n)&7)) )

int main()
{
	char path[4096];
	size_t k = 32;
	uint128_t b;
	size_t map_size = ((size_t)1 << k) / 8; /* 2^k bits */

	sprintf(path, "sieve-%lu.map", (unsigned long)k);

	g_map_sieve = open_map(path, map_size);

	/* a 2^k + b */
	for (b = 0; b < (UINT128_C(1) << k); ++b) {
		int killed = is_killed_below_k(b, k) || is_killed_at_k(b, k);

		if (!killed) {
			SET_LIVE(b);
		}

#if 0
		if (1) {
			size_t c;

			/* a 2^k + b --> a 3^c + d */
			uint128_t d = T_k(b, k, &c);

			printf("a 2^%lu + %lu => a 3^%lu + %lu %s\n", (unsigned long)k, (unsigned long)b, (unsigned long)c, (unsigned long)d, killed ? " --> KILLED" : "");
		}
#endif
	}

	msync(g_map_sieve, map_size, MS_SYNC);
	munmap(g_map_sieve, map_size);

	return 0;
}
