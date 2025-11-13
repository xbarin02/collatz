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

int main(/*int argc, char *argv[]*/)
{
	uint64_t task_id;
	uint64_t checksum = 0;
	uint64_t usertime = 0;
	int complete = 1;
	uint64_t num_complete = 0;
	uint64_t num_time = 0;

	printf("TARGET = %i\n", TARGET);
	printf("ASSIGNMENTS_NO = %" PRIu64 "\n", ASSIGNMENTS_NO);
	printf("LOG2_NO_PROCS = %i\n", LOG2_NO_PROCS);

	g_checksums = open_records("checksums.dat");
	g_usertimes = open_records("usertimes.dat");

	for (task_id = 0; task_id < ASSIGNMENTS_NO; ++task_id) {
		uint64_t checksum_ = g_checksums[task_id];
		uint64_t usertime_ = g_usertimes[task_id];

		checksum += checksum_;
		usertime += usertime_;

		if (!checksum_) {
			complete = 0;
		} else {
			num_complete++;
		}

		if (usertime_) {
			num_time++;
		}
	}

	printf("TOTAL CHECKSUM %" PRIu64 "\n", checksum);
	printf("TOTAL TIME %" PRIu64 " ms\n", usertime);
	printf("TOTAL TIME %" PRIu64 " (%" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ")\n",
		(usertime + 500)/1000,
		(usertime + 500)/1000/60/60, (usertime + 500)/1000/60%60, (usertime + 500)/1000%60);

	printf("avg. time: %f (%" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ")\n", usertime / (double)num_time / 1000,
		(usertime + 500)/1000/num_time/60/60, (usertime + 500)/1000/num_time/60%60, (usertime + 500)/1000/num_time%60
	);

	printf("all assignments are complete: %i\n", complete);
	printf("number of completed assignments: %" PRIu64 "\n", num_complete);

	return 0;
}
