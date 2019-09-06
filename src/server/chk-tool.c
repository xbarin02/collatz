#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

/* 2^32 assignments (tasks) */
#define ASSIGNMENTS_NO (UINT64_C(1) << 32)

#define CHECKSUMS_SIZE (ASSIGNMENTS_NO * 8)

const uint64_t *open_checksums()
{
	const char *path = "checksums.dat";
	int fd = open(path, O_RDONLY, 0600);
	const void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}
/*
	if (ftruncate(fd, (off_t)CHECKSUMS_SIZE) < 0) {
		perror("ftruncate");
		abort();
	}
*/
	ptr = mmap(NULL, (size_t)CHECKSUMS_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (const uint64_t *)ptr;
}

const uint64_t *g_checksums = 0;

#define MIN(a, b) ( ((a) < (b)) ? (a): (b) )
#define MAX(a, b) ( ((a) > (b)) ? (a): (b) )

int main()
{
	uint64_t n;
	uint64_t min = UINT64_MAX, max = 0;

	g_checksums = open_checksums();

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		uint64_t checksum = g_checksums[n];

		if (checksum != 0) {
			min = MIN(min, checksum);
			max = MAX(max, checksum);
		}
	}

	printf("min = %" PRIu64 " (0x%" PRIx64 "); min>>24 = %" PRIu64 " (0x%" PRIx64 ")\n", min, min, min>>24, min>>24);
	printf("max = %" PRIu64 " (0x%" PRIx64 "); max>>24 = %" PRIu64 " (0x%" PRIx64 ")\n", max, max, max>>24, max>>24);

	return 0;
}