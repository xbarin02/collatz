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
const uint64_t *g_clientids = 0;
const uint64_t *g_mxoffsets = 0;
const uint64_t *g_cycleoffs = 0;

#define MIN(a, b) ( ((a) < (b)) ? (a): (b) )
#define MAX(a, b) ( ((a) > (b)) ? (a): (b) )

static uint64_t round_div_ul(uint64_t n, uint64_t d)
{
	if (d == 0) {
		return 0;
	}

	return (n + d / 2) / d;
}

static uint128_t round_div_ull(uint128_t n, uint128_t d)
{
	if (d == 0) {
		return 0;
	}

	return (n + d / 2) / d;
}

uint64_t avg_and_print_usertime(uint128_t total, uint64_t count)
{
	uint64_t average = (uint64_t)round_div_ull(total, count);

	printf("- time records: %" PRIu64 " (%" PRIu64 "M)\n", count, round_div_ul(count, 1000000));
	printf("- total time: %" PRIu64 " hours = %" PRIu64 " days = %" PRIu64 " years\n",
		(uint64_t)round_div_ull(total, 3600) /* hours (60*60) */,
		(uint64_t)round_div_ull(total, 86400), /* days (60*60*24) */
		(uint64_t)round_div_ull(total, 31536000) /* years (60*60*24*365) */
	);
	printf("- average time: %" PRIu64 ":%02" PRIu64 ":%02" PRIu64 " (h:m:s)\n", (uint64_t)(average/60/60), (uint64_t)(average/60%60), (uint64_t)(average%60));

	printf("\n");

	return average;
}

void print_checksum_stats(uint64_t mask)
{
	uint64_t n;
	uint64_t min = UINT64_MAX, max = 0;
	uint64_t count = 0;

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		uint64_t checksum = g_checksums[n];

		if ((checksum>>24) != mask) {
			continue; /* other type */
		}

		if (checksum != 0) {
			min = MIN(min, checksum);
			max = MAX(max, checksum);
			count++;
		}
	}

	printf("- count = %" PRIu64 " (%" PRIu64 "M)\n", count, round_div_ul(count, 1000000));

	printf(
		"- min = %" PRIu64 " (0x%" PRIx64 "); "
		"min>>24 = %" PRIu64 " (0x%" PRIx64 "); "
		"min>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
		min, min,
		min>>24, min>>24,
		min>>23, min>>23
	);

	printf(
		"- max = %" PRIu64 " (0x%" PRIx64 "); "
		"max>>24 = %" PRIu64 " (0x%" PRIx64 "); "
		"max>>23 = %" PRIu64 " (0x%" PRIx64 ")\n",
		max, max,
		max>>24, max>>24,
		max>>23, max>>23
	);

	printf("\n");
}

void init()
{
	g_checksums = open_records("checksums.dat");
	g_usertimes = open_records("usertimes.dat");
	g_overflows = open_records("overflows.dat");
	g_clientids = open_records("clientids.dat");
	g_mxoffsets = open_records("mxoffsets.dat");
	g_cycleoffs = open_records("cycleoffs.dat");
}

struct timerec {
	uint128_t total;
	uint64_t count;
	uint64_t avg;
};

struct timerec timerec_create()
{
	struct timerec timerec = { 0, 0, 0 };

	return timerec;
}

int main()
{
	int show_checksum_stats = 1;
	int show_missing_checksums = 1;
	int show_usertime_records = 1;
	int show_overflows = 1;
	int show_clientids = 1;
	int show_mxoffsets = 1;
	int show_cycleoffs = 1;

	init();

	/* checksums */
	if (show_checksum_stats) {
		printf("sieve-2^2 (cpu, gpu) checksums:\n");
		print_checksum_stats(0x17f0f);

		printf("sieve-2^16 (gpu) checksums:\n");
		print_checksum_stats(0xa0ed);

		printf("sieve-2^24 (gpu) checksums:\n");
		print_checksum_stats(0x6ed9);
		print_checksum_stats(0x6eda);

		printf("sieve-2^32 (cpu, gpu) checksums:\n");
		print_checksum_stats(0x4cfe);
	}

	/* missing checksums */
	if (show_missing_checksums) {
		uint64_t n;
		int c = 0;

		printf("missing checksums:\n");
		for (n = 91226112; n < ASSIGNMENTS_NO; ++n) {
			uint64_t checksum = g_checksums[n];

			if (checksum == 0) {
				printf("- missing checksum on the assignment %" PRIu64 " (below %" PRIu64 " x 2^60)\n", n, (n >> 20) + 1);

				if (++c == 4) {
					break;
				}
			}
		}
		printf("\n");
	}

	/* usertime records */
	if (show_usertime_records) {
#		define ADD_TIME(tr, time) do { (tr).total += (time); (tr).count++; } while (0)
		uint64_t n;

		struct timerec tr_all = timerec_create();
		struct timerec tr_mod_2_2_short = timerec_create();
		struct timerec tr_mod_2_2_long = timerec_create();
		struct timerec tr_mod_2_32 = timerec_create();
		struct timerec tr_mod_2_16 = timerec_create();
		struct timerec tr_mod_2_24 = timerec_create();

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t usertime = g_usertimes[n];
			uint64_t checksum = g_checksums[n];

			if (usertime != 0) {
				ADD_TIME(tr_all, usertime);
			}

			/* classical sieve-4 results for both CPU & GPU */
			if (usertime != 0 && (checksum>>24) == 0x17f0f && usertime < 30*60) {
				ADD_TIME(tr_mod_2_2_short, usertime);
			}

			if (usertime != 0 && (checksum>>24) == 0x17f0f && usertime >= 30*60) {
				ADD_TIME(tr_mod_2_2_long, usertime);
			}

			if (usertime != 0 && (checksum>>24) == 0xa0ed) {
				ADD_TIME(tr_mod_2_16, usertime);
			}

			if (usertime != 0 && (checksum>>24) == 0x6ed9) {
				ADD_TIME(tr_mod_2_24, usertime);
			}

			if (usertime != 0 && (checksum>>24) == 0x6eda) {
				ADD_TIME(tr_mod_2_24, usertime);
			}

			if (usertime != 0 && (checksum>>24) == 0x4cfe) {
				ADD_TIME(tr_mod_2_32, usertime);
			}
		}

		printf("all user time records:\n");
		avg_and_print_usertime(tr_all.total, tr_all.count);

		printf("sieve-2^2 short user time records:\n");
		tr_mod_2_2_short.avg = avg_and_print_usertime(tr_mod_2_2_short.total, tr_mod_2_2_short.count);

		printf("sieve-2^2 long user time records:\n");
		tr_mod_2_2_long.avg = avg_and_print_usertime(tr_mod_2_2_long.total, tr_mod_2_2_long.count);

		printf("sieve-2^16 user time records:\n");
		avg_and_print_usertime(tr_mod_2_16.total, tr_mod_2_16.count);

		printf("sieve-2^24 user time records:\n");
		avg_and_print_usertime(tr_mod_2_24.total, tr_mod_2_24.count);

		printf("sieve-2^32 user time records:\n");
		avg_and_print_usertime(tr_mod_2_32.total, tr_mod_2_32.count);

		printf("speedup (sieve-2^2 long/short) = %" PRIu64 "\n", round_div_ul(tr_mod_2_2_long.avg, tr_mod_2_2_short.avg));
		printf("\n");

#		undef ADD_TIME
	}

	/* overflows */
	if (show_overflows) {
		uint64_t n;
		int overflow_found = 0;
		uint64_t overflow_count = 0;
		uint64_t overflow_sum = 0;
		int c = 0;

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t overflow = g_overflows[n];

			if (overflow != 0) {
				overflow_found = 1;
				overflow_count++;
				overflow_sum += overflow;

				if (++c < 32) {
					printf("- #%2i: found %" PRIu64 " overflows on the assignment %" PRIu64 " (below %" PRIu64 " x 2^60)\n", c, overflow, n, (n >> 20) + 1);
				}
			}
		}
		printf("\n");

		printf("overflow found: %s (%" PRIu64 " assignments, %" PRIu64 " overflows)\n", overflow_found ? "yes" : "no", overflow_count, overflow_sum);
		printf("\n");
	}

	/* clientids */
	if (show_clientids) {
		uint64_t n;
		uint64_t clientid_count = 0;

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t clientid = g_clientids[n];

			if (clientid != 0) {
				clientid_count++;
			}
		}

		printf("found %" PRIu64 " active assignments (client IDs)\n", clientid_count);
		printf("\n");
	}

	/* mxoffsets */
	if (show_mxoffsets) {
		uint64_t n;
		uint64_t mxoffset_count = 0;

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t mxoffset = g_mxoffsets[n];

			if (mxoffset != 0) {
				mxoffset_count++;
			}
		}

		printf("found %" PRIu64 " (%" PRIu64 "M) maximum value offset records (mxoffsets)\n", mxoffset_count, round_div_ul(mxoffset_count, 1000000));
		printf("\n");
	}

	/* cycleoffs */
	if (show_cycleoffs) {
		uint64_t n;
		uint64_t cycleoff_count = 0;

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t cycleoff = g_cycleoffs[n];

			if (cycleoff != 0) {
				cycleoff_count++;
			}
		}

		printf("found %" PRIu64 " (%" PRIu64 "M) longest cycle offset records (cycleoffs)\n", cycleoff_count, round_div_ul(cycleoff_count, 1000000));
		printf("\n");
	}

	return 0;
}
