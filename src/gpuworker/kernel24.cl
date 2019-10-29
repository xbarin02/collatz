typedef unsigned __int128 uint128_t;

#define UINT128_MAX (~(uint128_t)0)

uint pow3(size_t n)
{
	uint r = 1;
	uint b = 3;

	while (n) {
		if (n & 1) {
			r *= b;
		}
		b *= b;
		n >>= 1;
	}

	return r;
}

#define LUT_SIZE24 16
#define M24 0xffffff

__kernel void worker(
	__global ulong *checksum_alpha,
	ulong task_id,
	ulong task_size /* in log2 */,
	ulong task_units /* in log2 */)
{
	ulong private_checksum_alpha = 0;
	size_t id = get_global_id(0);

	__local uint lut[LUT_SIZE24];

	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < LUT_SIZE24; ++i) {
			lut[i] = pow3(i);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	uint128_t n0    = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 0) << (task_size - task_units)) + 3;
	uint128_t n_sup = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 1) << (task_size - task_units)) + 3;

	for (; n0 < n_sup; n0 += 4) {
		uint128_t n = n0;

		do {
			n++;

			size_t alpha = min((size_t)ctz((uint)n), (size_t)LUT_SIZE24 - 1);

			private_checksum_alpha += alpha;
			n >>= alpha;

			if (n > UINT128_MAX >> 2*alpha) {
				private_checksum_alpha = 0;
				goto end;
			}

			n *= lut[alpha] & M24;

			n--;

			n >>= ctz((uint)n);
		} while (n >= n0);
	}

end:
	checksum_alpha[id] = private_checksum_alpha;
}
