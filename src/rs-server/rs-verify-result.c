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
#include <math.h>
#include "wideint.h"
#include "compat.h"

#include "rs-config.h"

#define ASSIGNMENTS_NO (UINT64_C(1) << (LOG2_NO_PROCS))

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
const uint64_t *g_usertimes = 0;
const uint64_t *g_overflows = 0;

int main(int argc, char *argv[])
{
	uint64_t task_id = (argc > 1) ? atou64(argv[1]) : 0;
	uint64_t checksum;
	uint64_t usertime;
	uint64_t overflow;

	printf("TASK_ID %" PRIu64 "\n", task_id);
	assert(task_id < ASSIGNMENTS_NO);

	g_checksums = open_records("checksums.dat");
	g_usertimes = open_records("usertimes.dat");
	g_overflows = open_records("overflows.dat");

	checksum = g_checksums[task_id];

	printf("CHECKSUM %" PRIu64 "\n", checksum);

	usertime = g_usertimes[task_id];

	printf("TIME %" PRIu64 " ms\n", usertime);

	printf("TIME %" PRIu64 " (%" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ")\n",
			(usertime + 500)/1000,
			(usertime + 500)/1000/60/60, (usertime + 500)/1000/60%60, (usertime + 500)/1000%60);

	overflow = g_overflows[task_id];

	printf("OVERFLOWS: %" PRIu64 "\n", overflow);

	return 0;
}
