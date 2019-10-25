size_t ctzl(unsigned long n)
{
	return 63 - clz(n & -n);
}

typedef unsigned __int128 uint128_t;

#define UINT128_MAX (~(uint128_t)0)

size_t ctzu128(uint128_t n)
{
	size_t a = ctzl(n);

	if (a == ~0UL) {
		return 64 + ctzl(n>>64);
	}

	return a;
}

uint128_t pow3(size_t n)
{
	uint128_t r = 1;
	uint128_t b = 3;

	while (n) {
		if (n & 1) {
			r *= b;
		}
		b *= b;
		n >>= 1;
	}

	return r;
}

#define LUT_SIZE128 81

__kernel void worker(
	__global unsigned long *checksum_alpha,
	unsigned long task_id,
	unsigned long task_size /* in log2 */,
	unsigned long task_units /* in log2 */)
{
	unsigned long private_checksum_alpha = 0;
	size_t id = get_global_id(0);

#if 1
	__local uint128_t lut[LUT_SIZE128];

	unsigned long i;
	if (get_local_id(0) == 0) {
		for (i = 0; i < LUT_SIZE128; ++i) {
			lut[i] = pow3(i);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);
#else
	uint128_t lut[LUT_SIZE128];

	unsigned long i;
	for (i = 0; i < LUT_SIZE128; ++i) {
		lut[i] = pow3(i);
	}
#endif

	uint128_t n0    = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 0) << (task_size - task_units)) + 3;
	uint128_t n_sup = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 1) << (task_size - task_units)) + 3;

	for (; n0 < n_sup; n0 += 4) {
		uint128_t n = n0;
		do {
			n++;
			size_t alpha = ctzu128(n);
			private_checksum_alpha += alpha;
			n >>= alpha;

			if (n > UINT128_MAX >> 2*alpha || alpha >= LUT_SIZE128) {
				private_checksum_alpha = 0;
				goto end;
			}

			n *= lut[alpha];

			n--;
			n >>= ctzu128(n);
		} while (n >= n0);
	}

end:
	checksum_alpha[id] = private_checksum_alpha;
}
