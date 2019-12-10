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

#ifdef USE_SIEVE3
static int is_live_in_sieve3(uint128_t n)
{
	ulong r = 0;

	r += (uint)(n);
	r += (uint)(n >> 32);
	r += (uint)(n >> 64);
	r += (uint)(n >> 96);

	return r % 3 != 2;
}
#endif

#define LUT_SIZE32 21

#ifndef USE_LOCAL_SIEVE
#	define USE_LOCAL_SIEVE 1
#endif

/* in log2 */
#ifndef SIEVE_LOGSIZE
#	define SIEVE_LOGSIZE 16
#endif

#define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8)
#define SIEVE_MASK ((1UL << SIEVE_LOGSIZE) - 1)

#if (USE_LOCAL_SIEVE == 1)
#	define IS_LIVE(n) ( ( local_sieve[ ((n) & SIEVE_MASK)>>3 ] >> (((n) & SIEVE_MASK)&7) ) & 1 )
#else
#	define IS_LIVE(n) ( ( sieve[ ((n) & SIEVE_MASK)>>3 ] >> (((n) & SIEVE_MASK)&7) ) & 1 )
#endif

__kernel void worker(
	__global ulong *checksum_alpha,
	ulong task_id,
	ulong task_size,
	ulong task_units,
	__global uchar *sieve,
	__global ulong *mxoffset
)
{
	ulong private_checksum_alpha = 0;
	size_t id = get_global_id(0);

	__local uint lut[LUT_SIZE32];

#if (USE_LOCAL_SIEVE == 1)
	__local uchar local_sieve[SIEVE_SIZE];
#endif

	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < LUT_SIZE32; ++i) {
			lut[i] = pow3(i);
		}

#if (USE_LOCAL_SIEVE == 1)
		for (size_t i = 0; i < SIEVE_SIZE; ++i) {
			local_sieve[i] = sieve[i];
		}
#endif
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	uint128_t n_minimum  = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 0) << (task_size - task_units)) + 3;
	uint128_t n_supremum = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 1) << (task_size - task_units)) + 3;

	uint128_t max_n = 0;
	uint128_t max_n0;

	for (uint128_t n0 = n_minimum; n0 < n_supremum; n0 += 4) {
		while (!IS_LIVE(n0)
#ifdef USE_SIEVE3
		        || !is_live_in_sieve3(n0)
#endif
		) {
			n0 += 4;
		}
		if (n0 >= n_supremum) {
			break;
		}

		uint128_t n = n0;

		do {
			n++;

			size_t alpha = min((size_t)ctz((uint)n), (size_t)LUT_SIZE32 - 1);

			private_checksum_alpha += alpha;
			n >>= alpha;

			if (n > UINT128_MAX >> 2*alpha) {
				private_checksum_alpha = 0;
				goto end;
			}

			n *= lut[alpha];

			n--;

			if (n > max_n) {
				max_n = n;
				max_n0 = n0;
			}

			n >>= ctz((uint)n);
		} while (n >= n0);
	}

end:
	checksum_alpha[id] = private_checksum_alpha;
	mxoffset[id] = (ulong)(max_n0 - ((uint128_t)(task_id + 0) << task_size));
}
