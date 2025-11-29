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
#include "wideint.h"

#include "rs-config.h"

#define ASSIGNMENTS_NO (UINT64_C(1) << (LOG2_NO_PROCS))

#define MAP_SIZE ((ASSIGNMENTS_NO + 7) >> 3)

#define IS_ASSIGNED(n) (( g_map_assigned[ (n)>>3 ] >> ((n)&7) ) & 1)
#define IS_COMPLETE(n) (( g_map_complete[ (n)>>3 ] >> ((n)&7) ) & 1)

const unsigned char *g_map_assigned;
const unsigned char *g_map_complete;

const void *open_map(const char *path)
{
	int fd = open(path, O_RDONLY, 0600);
	const void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (const uint64_t *)ptr;
}

int main(void)
{
	uint64_t n;
	uint64_t completed = 0;
	uint64_t active = 0;

	g_map_assigned = open_map("assigned.map");
	g_map_complete = open_map("complete.map");

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		if (IS_COMPLETE(n)) {
			completed++;
		}

		if (IS_ASSIGNED(n) && !IS_COMPLETE(n)) {
			active++;
		}
	}

	printf("active assignements = %" PRIu64 "\n", active);
	printf("complete assignements = %" PRIu64 " / %" PRIu64 " = %f%%\n", completed, ASSIGNMENTS_NO, completed / (double)ASSIGNMENTS_NO * 100);

/*
	munmap(g_map_assigned, MAP_SIZE);
	munmap(g_map_complete, MAP_SIZE);
*/

	return 0;
}
