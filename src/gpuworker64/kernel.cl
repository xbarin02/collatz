size_t ctzl(unsigned long n)
{
	return 63 - clz(n & -n);
}

typedef unsigned long uint64_t;

#define UINT64_MAX (~(uint64_t)0)

uint64_t pow3(size_t n)
{
	uint64_t r = 1;
	uint64_t b = 3;

	while (n) {
		if (n & 1) {
			r *= b;
		}
		b *= b;
		n >>= 1;
	}

	return r;
}

#define LUT_SIZE64 41

__kernel void worker(
	__global unsigned long *overflow_counter,
	__global unsigned long *checksum_alpha,
	unsigned long task_id,
	unsigned long task_size /* in log2 */,
	unsigned long task_units /* in log2 */)
{
	unsigned long private_overflow_counter = 0;
	unsigned long private_checksum_alpha = 0;
	size_t id = get_global_id(0);

#if 1
	__local uint64_t lut[LUT_SIZE64];

	unsigned long i;
	if (get_local_id(0) == 0) {
		for (i = 0; i < LUT_SIZE64; ++i) {
			lut[i] = pow3(i);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);
#else
	uint64_t lut[LUT_SIZE64];

	unsigned long i;
	for (i = 0; i < LUT_SIZE64; ++i) {
		lut[i] = pow3(i);
	}
#endif

	uint64_t n0    = ((uint64_t)task_id << task_size) + ((uint64_t)(id + 0) << (task_size - task_units)) + 3;
	uint64_t n_sup = ((uint64_t)task_id << task_size) + ((uint64_t)(id + 1) << (task_size - task_units)) + 3;

	for (; n0 < n_sup; n0 += 4) {
		uint64_t n = n0;
		do {
			n++;
			size_t alpha = ctzl(n);
			private_checksum_alpha += alpha;
			n >>= alpha;

			if (n > UINT64_MAX >> 2*alpha || alpha >= LUT_SIZE64) {
				private_overflow_counter++;
				break;
			}

			n *= lut[alpha];

			n--;
			n >>= ctzl(n);
		} while (n >= n0);
	}

	overflow_counter[id] = private_overflow_counter;
	checksum_alpha[id] = private_checksum_alpha;
}
