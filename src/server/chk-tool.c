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

uint64_t print_checksum_stats_ex(uint64_t mask, size_t shift)
{
	uint64_t n;
	uint64_t min = UINT64_MAX, max = 0;
	uint64_t count = 0;

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		uint64_t checksum = g_checksums[n];

		if ((checksum>>shift) != mask) {
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

	return count;
}

uint64_t print_checksum_stats(uint64_t mask)
{
	return print_checksum_stats_ex(mask, 24);
}

void init()
{
	g_checksums = open_records("checksums.dat");
	g_usertimes = open_records("usertimes.dat");
	g_overflows = open_records("overflows.dat");
	g_clientids = open_records("clientids.dat");
	g_mxoffsets = open_records("mxoffsets.dat");
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

int main(int argc, char *argv[])
{
	int show_checksums = 0;
	int show_missing_checksums = 0;
	int show_usertimes = 0;
	int show_overflows = 0;
	int show_clientids = 0;
	int show_mxoffsets = 0;
	int opt;

	while ((opt = getopt(argc, argv, "sxtoimc")) != -1) {
		switch (opt) {
			case 's':
				show_checksums = 1;
				break;
			case 'x':
				show_missing_checksums = 1;
				break;
			case 't':
				show_usertimes = 1;
				break;
			case 'o':
				show_overflows = 1;
				break;
			case 'i':
				show_clientids = 1;
				break;
			case 'm':
				show_mxoffsets = 1;
				break;
			default:
				printf("[ERROR] Usage: %s [options]\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	init();

#if 0
	/* find holes in mxoffset[] */
	if (1) {
		int state = 0; /* zero/hole */
		uint64_t n;
		uint64_t n0 = 0;

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t mxoffset = g_mxoffsets[n];

			if (state == 0) {
				if (mxoffset == 0) {
					/* ok, still hole :/ */
				} else {
					/* transition hole -> nonzero mxoffset :) */
					state = 1;
					if (n0 != 0) {
						printf("hole: [%lu .. %lu) of size %lu\n", n0, n, n - n0);
					}
					n0 = n;
				}
			} else {
				if (mxoffset == 0) {
					/* transition nonzero mxoffset -> hole :( */
					state = 0;
#					if 0
					printf("fill: [%lu .. %lu) of size %lu\n", n0, n, n - n0);
#					endif
					n0 = n;
				} else {
					/* ok, still sequence of nonzero mxoffsets :) */
				}
			}
		}

		printf("\n");
	}
#endif

	/* checksums */
	if (show_checksums) {
		uint64_t cpu_count = 0, gpu_count = 0;

		printf("analyzing checksums...\n");

		printf("sieve-2^2 checksums:\n");
		(void)print_checksum_stats(0x17f0f);

		printf("sieve-2^16 checksums:\n");
		gpu_count += print_checksum_stats(0xa0ed);

		printf("esieve-2^16 checksums:\n");
		gpu_count += print_checksum_stats_ex(0x83b, 28);

		printf("esieve-2^24 checksums:\n");
		gpu_count += print_checksum_stats_ex(0x5ae, 28);

		printf("esieve-2^24 sieve-3^1 checksums:\n");
		gpu_count += print_checksum_stats(0x3c96);

		printf("sieve-2^32 checksums:\n");
		cpu_count += print_checksum_stats(0x4cfe);

		printf("sieve-2^32 sieve-3^1 checksums:\n");
		cpu_count += print_checksum_stats(0x3354);

		printf("esieve-2^32 sieve-3^1 checksums:\n");
		cpu_count += print_checksum_stats(0x2a27);

		printf("esieve-2^34 sieve-3^1 checksums:\n");
		cpu_count += print_checksum_stats(0x27d8);

		printf("esieve-2^34 sieve-3^2 checksums:\n");
		cpu_count += print_checksum_stats(0x2134);

		printf("h-esieve-2^34 sieve-3^2 checksums:\n");
		cpu_count += print_checksum_stats_ex(0x1ac, 28);

		printf("Results: %" PRIu64 " work units on CPU, %" PRIu64 " work units on GPU, %" PRIu64 " work units in total\n", cpu_count, gpu_count, cpu_count + gpu_count);
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
	if (show_usertimes) {
#		define ADD_TIME(tr, time) do { (tr).total += (time); (tr).count++; } while (0)
		uint64_t n;

		struct timerec tr_all = timerec_create();
		struct timerec tr_mod_2_2 = timerec_create();
		struct timerec tr_mod_2_16 = timerec_create(); /* gpu */
		struct timerec tr_mod_2_16_e = timerec_create(); /* gpu */
		struct timerec tr_mod_2_24_e = timerec_create(); /* gpu */
		struct timerec tr_mod_2_24_3_1_e = timerec_create(); /* gpu */
		struct timerec tr_mod_2_32 = timerec_create(); /* cpu */
		struct timerec tr_mod_2_32_3_1 = timerec_create(); /* cpu */
		struct timerec tr_mod_2_32_3_1_e = timerec_create(); /* cpu */
		struct timerec tr_mod_2_34_3_1_e = timerec_create(); /* cpu */
		struct timerec tr_mod_2_34_3_2_e = timerec_create(); /* cpu */
		struct timerec tr_mod_2_34_3_2_h_e = timerec_create(); /* cpu */
		struct timerec tr_cpu = timerec_create();
		struct timerec tr_gpu = timerec_create();

		printf("analyzing time records...\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t usertime = g_usertimes[n];
			uint64_t checksum = g_checksums[n];

			if (usertime > 2 * 60 * 60) {
#if 1
				printf("%" PRIu64 ": %" PRIu64 " (0x%" PRIx64 ")\n", n, usertime, checksum >> 24);
#endif
				continue;
			}

			if (usertime != 0) {
				ADD_TIME(tr_all, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x17f0f) {
				ADD_TIME(tr_mod_2_2, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0xa0ed) {
				ADD_TIME(tr_mod_2_16, usertime);
				ADD_TIME(tr_gpu, usertime);
			}

			if (usertime != 0 && (checksum >> 28) == 0x83b) {
				ADD_TIME(tr_mod_2_16_e, usertime);
				ADD_TIME(tr_gpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x4cfe) {
				ADD_TIME(tr_mod_2_32, usertime);
				ADD_TIME(tr_cpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x3354) {
				ADD_TIME(tr_mod_2_32_3_1, usertime);
				ADD_TIME(tr_cpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x2a27) {
				ADD_TIME(tr_mod_2_32_3_1_e, usertime);
				ADD_TIME(tr_cpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x27d8) {
				ADD_TIME(tr_mod_2_34_3_1_e, usertime);
				ADD_TIME(tr_cpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x5ae2) {
				ADD_TIME(tr_mod_2_24_e, usertime);
				ADD_TIME(tr_gpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x5ae1) {
				ADD_TIME(tr_mod_2_24_e, usertime);
				ADD_TIME(tr_gpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x3c96) {
				ADD_TIME(tr_mod_2_24_3_1_e, usertime);
				ADD_TIME(tr_gpu, usertime);
			}

			if (usertime != 0 && (checksum >> 24) == 0x2134) {
				ADD_TIME(tr_mod_2_34_3_2_e, usertime);
				ADD_TIME(tr_cpu, usertime);
			}

			if (usertime != 0 && (checksum >> 28) == 0x1ac) {
				ADD_TIME(tr_mod_2_34_3_2_h_e, usertime);
				ADD_TIME(tr_cpu, usertime);
			}
		}

		printf("all user time records:\n");
		avg_and_print_usertime(tr_all.total, tr_all.count);

		printf("GPU user time records:\n");
		avg_and_print_usertime(tr_gpu.total, tr_gpu.count);

		printf("CPU user time records:\n");
		avg_and_print_usertime(tr_cpu.total, tr_cpu.count);

		printf("sieve-2^2 user time records:\n");
		tr_mod_2_2.avg = avg_and_print_usertime(tr_mod_2_2.total, tr_mod_2_2.count);

		printf("sieve-2^16 user time records:\n");
		avg_and_print_usertime(tr_mod_2_16.total, tr_mod_2_16.count);

		printf("esieve-2^16 user time records:\n");
		avg_and_print_usertime(tr_mod_2_16_e.total, tr_mod_2_16_e.count);

		printf("esieve-2^24 user time records:\n");
		avg_and_print_usertime(tr_mod_2_24_e.total, tr_mod_2_24_e.count);

		printf("esieve-2^24 sieve-3^1 user time records:\n");
		avg_and_print_usertime(tr_mod_2_24_3_1_e.total, tr_mod_2_24_3_1_e.count);

		printf("sieve-2^32 user time records:\n");
		avg_and_print_usertime(tr_mod_2_32.total, tr_mod_2_32.count);

		printf("sieve-2^32 sieve-3^1 user time records:\n");
		avg_and_print_usertime(tr_mod_2_32_3_1.total, tr_mod_2_32_3_1.count);

		printf("esieve-2^32 sieve-3^1 user time records:\n");
		avg_and_print_usertime(tr_mod_2_32_3_1_e.total, tr_mod_2_32_3_1_e.count);

		printf("esieve-2^34 sieve-3^1 user time records:\n");
		avg_and_print_usertime(tr_mod_2_34_3_1_e.total, tr_mod_2_34_3_1_e.count);

		printf("esieve-2^34 sieve-3^2 user time records:\n");
		avg_and_print_usertime(tr_mod_2_34_3_2_e.total, tr_mod_2_34_3_2_e.count);

		printf("h-esieve-2^34 sieve-3^2 user time records:\n");
		avg_and_print_usertime(tr_mod_2_34_3_2_h_e.total, tr_mod_2_34_3_2_h_e.count);

#		undef ADD_TIME
	}

	/* overflows */
	if (show_overflows) {
		uint64_t n;
		uint64_t overflow_count = 0;
		uint64_t overflow_sum = 0;
#if 0
		int c = 0;
#endif

		printf("analyzing overflows...\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t overflow = g_overflows[n];

			if (overflow != 0) {
				overflow_count++;
				overflow_sum += overflow;

#if 0
				if (++c < 48) {
					printf("- #%2i: found %" PRIu64 " overflows on the assignment %9" PRIu64 " (below %3" PRIu64 " x 2^60)\n", c, overflow, n, (n >> 20) + 1);
				}
#endif
			}
		}
		printf("\n");

		printf("overflows found: %" PRIu64 " assignments, %" PRIu64 " overflows\n", overflow_count, overflow_sum);
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
		int c = 0;
		uint64_t mxoffset_count = 0;

		/* find records that have incomplete mxoffset */
		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t checksum = g_checksums[n];
			uint64_t mxoffset = g_mxoffsets[n];

			if (checksum) {
				if (!mxoffset) {
					c++;
				}
			}
		}

		printf("*** found %i incomplete records ***\n", c);
		printf("\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t mxoffset = g_mxoffsets[n];

			if (mxoffset != 0) {
				mxoffset_count++;
			}
		}

		printf("found %" PRIu64 " (%" PRIu64 "M) maximum value offset records (mxoffsets)\n", mxoffset_count, round_div_ul(mxoffset_count, 1000000));
		printf("\n");
	}

	return 0;
}
