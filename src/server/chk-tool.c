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

/* 2^32 assignments (tasks) */
#define ASSIGNMENTS_NO (UINT64_C(1) << 32)

#define CHECKSUMS_SIZE (ASSIGNMENTS_NO * 8)
#define USERTIMES_SIZE (ASSIGNMENTS_NO * 8)
#define OVERFLOWS_SIZE (ASSIGNMENTS_NO * 8)
#define CLIENTIDS_SIZE (ASSIGNMENTS_NO * 8)

const uint64_t *open_checksums()
{
	const char *path = "checksums.dat";
	int fd = open(path, O_RDONLY, 0600);
	const void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)CHECKSUMS_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (const uint64_t *)ptr;
}

const uint64_t *open_usertimes()
{
	const char *path = "usertimes.dat";
	int fd = open(path, O_RDONLY, 0600);
	const void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)USERTIMES_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (const uint64_t *)ptr;
}

const uint64_t *open_overflows()
{
	const char *path = "overflows.dat";
	int fd = open(path, O_RDONLY, 0600);
	const void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)OVERFLOWS_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (const uint64_t *)ptr;
}

const uint64_t *open_clientids()
{
	const char *path = "clientids.dat";
	int fd = open(path, O_RDONLY, 0600);
	const void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)CLIENTIDS_SIZE, PROT_READ, MAP_SHARED, fd, 0);

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
const uint64_t *g_clientids = 0;

#define MIN(a, b) ( ((a) < (b)) ? (a): (b) )
#define MAX(a, b) ( ((a) > (b)) ? (a): (b) )

int main()
{
	uint64_t n;
	uint128_t total_user_time = 0;
	uint128_t total_user_time_short = 0;
	uint128_t total_user_time_long = 0;
	uint64_t user_time_count = 0;
	uint64_t user_time_count_short = 0;
	uint64_t user_time_count_long = 0;
	int c = 0;
	int overflow_found = 0;
	uint64_t clientids_count = 0;
	uint64_t overflow_count = 0;
	uint64_t overflow_sum = 0;
	uint64_t avg_user_time_long = 0;
	uint64_t avg_user_time_short = 0;

	g_checksums = open_checksums();
	g_usertimes = open_usertimes();
	g_overflows = open_overflows();
	g_clientids = open_clientids();

#if 1
	{
		uint64_t min = UINT64_MAX, max = 0;
		uint64_t count = 0;

		printf("classical mod4 checksums:\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t checksum = g_checksums[n];

			if ((checksum>>23) != 196126) {
				continue; /* other type */
			}

			if (checksum != 0) {
				min = MIN(min, checksum);
				max = MAX(max, checksum);
				count++;
			}
		}

		printf("count = %" PRIu64 "\n", count);

		printf(
			"min = %" PRIu64 " (0x%" PRIx64 "); "
			"min>>24 = %" PRIu64 " (0x%" PRIx64 "); "
			"min>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
			min, min,
			min>>24, min>>24,
			min>>23, min>>23
		);
		printf(
			"max = %" PRIu64 " (0x%" PRIx64 "); "
			"max>>24 = %" PRIu64 " (0x%" PRIx64 "); "
			"max>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
			max, max,
			max>>24, max>>24,
			max>>23, max>>23
		);

		printf("\n");
	}
#endif
#if 1
	{
		uint64_t min = UINT64_MAX, max = 0;
		uint64_t count = 0;

		printf("buggy mod12 checksums:\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t checksum = g_checksums[n];

			if ((checksum>>28) != 0xff5) {
				continue; /* other type */
			}

			if (checksum != 0) {
				min = MIN(min, checksum);
				max = MAX(max, checksum);
				count++;
			}
		}

		printf("count = %" PRIu64 "\n", count);

		printf(
			"min = %" PRIu64 " (0x%" PRIx64 "); "
			"min>>24 = %" PRIu64 " (0x%" PRIx64 "); "
			"min>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
			min, min,
			min>>24, min>>24,
			min>>23, min>>23
		);
		printf(
			"max = %" PRIu64 " (0x%" PRIx64 "); "
			"max>>24 = %" PRIu64 " (0x%" PRIx64 "); "
			"max>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
			max, max,
			max>>24, max>>24,
			max>>23, max>>23
		);

		printf("\n");
	}
#endif
#if 1
	{
		uint64_t min = UINT64_MAX, max = 0;
		uint64_t count = 0;

		printf("sieve-32 mod12 checksums:\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t checksum = g_checksums[n];

			if ((checksum>>24) != 0x3354) {
				continue; /* other type */
			}

			if (checksum != 0) {
				min = MIN(min, checksum);
				max = MAX(max, checksum);
				count++;
			}
		}

		printf("count = %" PRIu64 "\n", count);

		printf(
			"min = %" PRIu64 " (0x%" PRIx64 "); "
			"min>>24 = %" PRIu64 " (0x%" PRIx64 "); "
			"min>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
			min, min,
			min>>24, min>>24,
			min>>23, min>>23
		);
		printf(
			"max = %" PRIu64 " (0x%" PRIx64 "); "
			"max>>24 = %" PRIu64 " (0x%" PRIx64 "); "
			"max>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
			max, max,
			max>>24, max>>24,
			max>>23, max>>23
		);

		printf("\n");
	}
#endif

	for (n = 91226112; n < ASSIGNMENTS_NO; ++n) {
		uint64_t checksum = g_checksums[n];

		if (checksum == 0) {
			printf("- missing checksum on the assignment %" PRIu64 " (below %" PRIu64 " x 2^60)\n", n, (n >> 20) + 1);

			if (++c == 4)
				break;
		}
	}

	printf("\n");

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		uint64_t user_time = g_usertimes[n];

		if (user_time != 0) {
			total_user_time += user_time;
			user_time_count++;

			if (user_time < 30*60) {
				total_user_time_short += user_time;
				user_time_count_short++;
			} else {
				total_user_time_long += user_time;
				user_time_count_long++;
			}
		}
	}

	if (user_time_count > 0) {
		uint64_t avg_user_time = (uint64_t) ((total_user_time+(user_time_count/2)) / user_time_count);

		printf("user time records: %" PRIu64 "\n", user_time_count);
		printf("total user time: %" PRIu64 " hours = %" PRIu64 " days = %" PRIu64 " years\n",
			(uint64_t)((total_user_time+1800)/3600) /* hours (60*60) */,
			(uint64_t)((total_user_time+43200)/86400), /* days (60*60*24) */
			(uint64_t)((total_user_time+15768000)/31536000) /* years (60*60*24*365) */
		);
		printf("average user time: %" PRIu64 ":%02" PRIu64 ":%02" PRIu64 " (h:m:s)\n", (uint64_t)(avg_user_time/60/60), (uint64_t)(avg_user_time/60%60), (uint64_t)(avg_user_time%60));
	}

	printf("\n");

	if (user_time_count_short > 0) {
		uint64_t avg_user_time = (uint64_t) ((total_user_time_short+(user_time_count_short/2)) / user_time_count_short);

		avg_user_time_short = avg_user_time;

		printf("short user time records: %" PRIu64 "\n", user_time_count_short);
		printf("total user time (short): %" PRIu64 " hours = %" PRIu64 " days = %" PRIu64 " years\n",
			(uint64_t)((total_user_time_short+1800)/3600) /* hours (60*60) */,
			(uint64_t)((total_user_time_short+43200)/86400), /* days (60*60*24) */
			(uint64_t)((total_user_time_short+15768000)/31536000) /* years (60*60*24*365) */
		);
		printf("average user time (short): %" PRIu64 ":%02" PRIu64 ":%02" PRIu64 " (h:m:s)\n", (uint64_t)(avg_user_time/60/60), (uint64_t)(avg_user_time/60%60), (uint64_t)(avg_user_time%60));
	}

	printf("\n");

	if (user_time_count_long > 0) {
		uint64_t avg_user_time = (uint64_t) ((total_user_time_long+(user_time_count_long/2)) / user_time_count_long);

		avg_user_time_long = avg_user_time;

		printf("long user time records: %" PRIu64 "\n", user_time_count_long);
		printf("total user time (long): %" PRIu64 " hours = %" PRIu64 " days = %" PRIu64 " years\n",
			(uint64_t)((total_user_time_long+1800)/3600) /* hours (60*60) */,
			(uint64_t)((total_user_time_long+43200)/86400), /* days (60*60*24) */
			(uint64_t)((total_user_time_long+15768000)/31536000) /* years (60*60*24*365) */
		);
		printf("average user time (long): %" PRIu64 ":%02" PRIu64 ":%02" PRIu64 " (h:m:s)\n", (uint64_t)(avg_user_time/60/60), (uint64_t)(avg_user_time/60%60), (uint64_t)(avg_user_time%60));
	}

	printf("\n");

	if (avg_user_time_short > 0) {
		uint64_t speedup = (avg_user_time_long + avg_user_time_short/2) / avg_user_time_short;

		printf("speedup (long/short) = %" PRIu64 "\n", speedup);
	}

	printf("\n");

	c = 0;

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		uint64_t overflow = g_overflows[n];

		if (overflow != 0) {
			overflow_found = 1;
			overflow_count++;
			overflow_sum += overflow;

			if (++c < 32) {
				printf("- found %" PRIu64 " overflows on the assignment %" PRIu64 " (below %" PRIu64 " x 2^60)\n", overflow, n, (n >> 20) + 1);
			}
		}
	}

	printf("\n");

	printf("overflow found: %s (%" PRIu64 " assignments, %" PRIu64 " overflows)\n", overflow_found ? "yes" : "no", overflow_count, overflow_sum);
	printf("\n");

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		uint64_t clid = g_clientids[n];

		if (clid != 0) {
			clientids_count++;
		}
	}

	printf("found %" PRIu64 " active assignments (client IDs)\n", clientids_count);

	return 0;
}
