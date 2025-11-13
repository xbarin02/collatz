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

#include <assert.h>

#include "rs-config.h"

#define ASSIGNMENTS_NO (UINT64_C(1) << (LOG2_NO_PROCS))

#define RECORDS_SIZE (ASSIGNMENTS_NO * 8)

uint128_t g_pow3[64];

uint128_t pow3u128(uint128_t n)
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

void pow3_init()
{
	int i = 0;
	for (; i < 64; ++i) {
		g_pow3[i] = pow3u128(i);
	}
}

void print(uint128_t n)
{
	char buff[40];
	char r_buff[40];

	char *ptr = buff;

	int i = 0;

	while (n != 0) {
		*(ptr++) = '0' + n % 10;
		n /= 10;
	}

	*ptr = 0;

	ptr--;

	while (ptr >= buff) {
		r_buff[i++] = *(ptr--);
	}

	r_buff[i] = 0;

	puts(r_buff);
}

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

int main(/*int argc, char *argv[]*/)
{
	uint64_t task_id;
	uint64_t checksum = 0;
	uint64_t usertime = 0;
	uint64_t overflow = 0;
	int complete = 1;
	uint64_t num_complete = 0;
	uint64_t num_time = 0;

	pow3_init();

	printf("TARGET = %i\n", TARGET);
	printf("ASSIGNMENTS_NO = %" PRIu64 "\n", ASSIGNMENTS_NO);
	printf("LOG2_NO_PROCS = %i\n", LOG2_NO_PROCS);

	g_checksums = open_records("checksums.dat");
	g_usertimes = open_records("usertimes.dat");
	g_overflows = open_records("overflows.dat");

	for (task_id = 0; task_id < ASSIGNMENTS_NO; ++task_id) {
		uint64_t checksum_ = g_checksums[task_id];
		uint64_t usertime_ = g_usertimes[task_id];
		uint64_t overflow_ = g_overflows[task_id];

		checksum += checksum_;
		usertime += usertime_;
		overflow += overflow_;

		if (!checksum_) {
			complete = 0;
		} else {
			num_complete++;
		}

		if (usertime_) {
			num_time++;
		}
	}

	printf("OLD_LIMIT (all numbers below this must be already verified) ");
	print(4 * g_pow3[TARGET + 0] + 2);

	printf("NEW LIMIT (all numbers below this are now verified) ");
	print(4 * g_pow3[TARGET + 1] + 2);

	printf("TOTAL CHECKSUM %" PRIu64 "\n", checksum);
	printf("TOTAL TIME %" PRIu64 " ms\n", usertime);
	printf("TOTAL TIME %" PRIu64 " secs (%" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ")\n",
		(usertime + 500)/1000,
		(usertime + 500)/1000/60/60, (usertime + 500)/1000/60%60, (usertime + 500)/1000%60);

	printf("avg. time: %f secs (%" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ")\n", usertime / (double)num_time / 1000,
		(usertime + 500)/1000/num_time/60/60, (usertime + 500)/1000/num_time/60%60, (usertime + 500)/1000/num_time%60
	);

	printf("overflows: %" PRIu64 "\n", overflow);

	printf("all assignments are complete: %i\n", complete);
	printf("number of completed assignments: %" PRIu64 "\n", num_complete);

	return 0;
}
