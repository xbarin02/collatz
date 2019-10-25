typedef unsigned __int128 uint128_t;

#define UINT128_MAX (~(uint128_t)0)
#define UINT64_MAX (~(ulong)0)

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

#define LUT_SIZE32 21

size_t check(ulong n0_, ulong n_, size_t alpha, uint lut[])
{
	uint128_t n0 = n0_;
	uint128_t n = n_;
	size_t checksum_alpha = 0;

	while (1) {
		if (n > UINT128_MAX >> 2*alpha) {
			return -1;
		}

		n *= lut[alpha];

		n--;

		n >>= ctz((uint)n);

		if (n < n0) {
			return checksum_alpha;
		}

		n++;

		alpha = min((size_t)ctz((uint)n), (size_t)LUT_SIZE32 - 1);

		checksum_alpha += alpha;

		n >>= alpha;
	}
}

__kernel void worker(
	__global ulong *checksum_alpha,
	ulong task_id,
	ulong task_size /* in log2 */,
	ulong task_units /* in log2 */)
{
	ulong private_checksum_alpha = 0;
	size_t id = get_global_id(0);

	__local uint lut[LUT_SIZE32];

	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < LUT_SIZE32; ++i) {
			lut[i] = pow3(i);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	if ((task_id + 1) > UINT64_MAX >> task_size) {
		goto end;
	}

	ulong n0    = ((ulong)task_id << task_size) + ((ulong)(id + 0) << (task_size - task_units)) + 3;
	ulong n_sup = ((ulong)task_id << task_size) + ((ulong)(id + 1) << (task_size - task_units)) + 3;

	for (; n0 < n_sup; n0 += 4) {
		ulong n = n0;

		do {
			n++;

			size_t alpha = min((size_t)ctz((uint)n), (size_t)LUT_SIZE32 - 1);

			private_checksum_alpha += alpha;
			n >>= alpha;

			if (n > UINT64_MAX >> 2*alpha) {
				size_t t = check(n0, n, alpha, lut);
				if (t == (size_t)-1) {
					private_checksum_alpha = 0;
					goto end;
				}
				private_checksum_alpha += t;
				break; /* next n0 */
			}

			n *= lut[alpha];

			n--;

			n >>= ctz((uint)n);
		} while (n >= n0);
	}

end:
	checksum_alpha[id] = private_checksum_alpha;
}
