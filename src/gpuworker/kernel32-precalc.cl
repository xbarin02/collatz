typedef unsigned __int128 uint128_t;

#define UINT128_MAX (~(uint128_t)0)

#ifdef USE_LUT50
__constant static const ulong dict[] = {
	0x0000000000000000,
	0x0000000000000080,
	0x0000000008000000,
	0x0000000008000080,
	0x0000000080000000,
	0x0000000088000000,
	0x0000008000000000,
	0x0000008000000080,
	0x0000008008000000,
	0x0000008008000080,
	0x0000008080000000,
	0x0000008088000000,
	0x0000800000000000,
	0x0000800000000080,
	0x0000800008000000,
	0x0000800008000080,
	0x0000800080000000,
	0x0000800088000000,
	0x0000808000000000,
	0x0000808000000080,
	0x0000808008000000,
	0x0000808008000080,
	0x0800000000000000,
	0x0800008000000000,
	0x0800800000000000,
	0x0800808000000000,
	0x8000000000000000,
	0x8000000000000080,
	0x8000000008000000,
	0x8000000008000080,
	0x8000000080000000,
	0x8000000088000000,
	0x8000008000000000,
	0x8000008000000080,
	0x8000008008000000,
	0x8000008008000080,
	0x8000008080000000,
	0x8000008088000000,
	0x8000800000000000,
	0x8000800000000080,
	0x8000800008000000,
	0x8000800008000080,
	0x8000808000000000,
	0x8000808000000080,
	0x8000808008000000,
	0x8000808008000080,
	0x8800000000000000,
	0x8800008000000000,
	0x8800800000000000,
	0x8800808000000000
};
#endif

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

/* in log2 */
#ifndef SIEVE_LOGSIZE
#	define SIEVE_LOGSIZE 32
#endif

#ifdef USE_LUT50
#	define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8 / 8)
#else
#	define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8)
#endif

#define SIEVE_MASK ((1UL << SIEVE_LOGSIZE) - 1)

#define SIEVE_NAME sieve

#ifdef USE_LUT50
#	define GET_INDEX(n) (SIEVE_NAME[((n) & SIEVE_MASK) >> (3 + 3)])
#	define IS_LIVE(n) ((l_dict[GET_INDEX(n)] >> ((n) & 63)) & 1)
#else
#	define IS_LIVE(n) ((SIEVE_NAME[((n) & SIEVE_MASK) >> 3] >> (((n) & SIEVE_MASK) & 7)) & 1)
#endif

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

#ifdef USE_SIEVE9
static int is_live_in_sieve9(uint128_t n)
{
	ulong r = 0;

	r += (uint)(n);
	r += (uint)(n >> 32) * (ulong)4;
	r += (uint)(n >> 64) * (ulong)7;
	r += (uint)(n >> 96);

	r = r % 9;

	/* n is not {2, 4, 5, 8} (mod 9) */
	return r != 2 && r != 4 && r != 5 && r != 8;
}
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
	__local uint128_t max_ns[LUT_SIZE32];
	__local ulong max_ns_ul[LUT_SIZE32];

#ifdef USE_LUT50
	__local ulong l_dict[50];
#endif

#ifdef USE_LUT50
	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < 50; ++i) {
			l_dict[i] = dict[i];
		}
	}
#endif

	if (get_local_id(0) == 0) {
		for (size_t alpha = 0; alpha < LUT_SIZE32; ++alpha) {
			lut[alpha] = pow3(alpha);
			max_ns[alpha] = UINT128_MAX >> 2*alpha;
			max_ns_ul[alpha] = ~0UL >> 2*alpha;
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	/* 2^task_units = number of threads */
	/* 2^32 = 2^R = 2^SIEVE_LOGSIZE */

	/*              40 bits in total
	 *  /--------------------------------------\
	 *  |8 bits             32 bits            |
	 *  /------\/------------------------------\
	 *  |------||    threads      each thread  |
	 *  |------|/--------------\/--------------\
	 *  |------||        sieve + precalc       |
	 */
	uint128_t n_minimum  = ((uint128_t)(id + 0) << (SIEVE_LOGSIZE - task_units));
	uint128_t n_supremum = ((uint128_t)(id + 1) << (SIEVE_LOGSIZE - task_units));

	uint128_t max_n = 0;
	uint128_t max_n0 = ((uint128_t)(task_id + 0) << task_size);

	/* iterate over lowest (32 - task_units) bits */
	for (uint128_t n0 = n_minimum + 3; n0 < n_supremum + 3; n0 += 4) {
		if (!IS_LIVE(n0)) {
			continue;
		}

		barrier(CLK_LOCAL_MEM_FENCE);

		/* precalc(n0) */

		int R = SIEVE_LOGSIZE; /* copy since we need to decrement it */
		ulong L = (ulong)n0; /* only 32-LSbits in n0 */
		ulong L0 = (ulong)n0; /* copy of L */
		size_t Salpha = 0, Sbeta = 0; /* sum of alpha, beta */

		do {
			L++;
			do {
				size_t alpha = (size_t)ctz((uint)L);
				alpha = min(alpha, (size_t)LUT_SIZE32 - 1);
				alpha = min(alpha, (size_t)R);
				R -= alpha;
				Salpha += alpha;
				L >>= alpha;
				if (L > max_ns_ul[alpha]) {
					private_checksum_alpha = 0;
					goto end;
				}
				L *= lut[alpha];
				if (R == 0) {
					L--;
					goto lcalc;
				}
			} while (!(L & 1));
			L--;
			if (L == 0) {
				Sbeta += R;
				R -= R;
				goto lcalc;
			}
			do {
				size_t beta = ctz((uint)L);
				beta = min(beta, (size_t)R);
				R -= beta;
				Sbeta += beta;
				L >>= beta;
				if (R == 0) {
					goto lcalc;
				}
			} while (!(L & 1));
		} while (1);

		/* tail */
lcalc:
		{
			R = Salpha + Sbeta; /* R-LSbits have been precalculated above */

#if !defined(USE_SIEVE3) && !defined(USE_SIEVE9)
			private_checksum_alpha += Salpha << (task_size - R);
#endif

			/* iterate over highest 40 - 32 = 8 bits */
			for (uint128_t h = 0; h < (1UL << (task_size - R)); ++h) {
				uint128_t H = ((uint128_t)task_id << task_size) + (h << R);
				uint128_t N = H >> (Salpha + Sbeta);
				uint128_t N0 = H + L0;

#ifdef USE_SIEVE3
				if (!is_live_in_sieve3(N0)) {
					continue;
				}

				private_checksum_alpha += Salpha;
#endif
#ifdef USE_SIEVE9
				if (!is_live_in_sieve9(N0)) {
					continue;
				}

				private_checksum_alpha += Salpha;
#endif

				/* WARNING: this zeroes the alpha which is needed for the subsequent iterations */
				size_t Salpha0 = Salpha;
				do {
					size_t alpha = min(Salpha0, (size_t)LUT_SIZE32 - 1);
					if (N > max_ns[alpha]) {
						private_checksum_alpha = 0;
						goto end;
					}
					N *= lut[alpha];
					Salpha0 -= alpha;
				} while (Salpha0 > 0);
				if (N > UINT128_MAX - L) {
					private_checksum_alpha = 0;
					goto end;
				}
				N += L;

				if (!(N & 1)) {
					goto even;
				}

				do {
					N++;
					do {
						size_t alpha = (size_t)ctz((uint)N);
						alpha = min(alpha, (size_t)LUT_SIZE32 - 1);
						private_checksum_alpha += alpha;
						N >>= alpha;
						if (N > max_ns[alpha]) {
							private_checksum_alpha = 0;
							goto end;
						}
						N *= lut[alpha];
					} while (!(N & 1));
					N--;
even:
					if (N > max_n) {
						max_n = N;
						max_n0 = N0;
					}

					do {
						size_t beta = (size_t)ctz((uint)N);
						N >>= beta;
					} while (!(N & 1));
					if (N < N0) {
						goto next;
					}
				} while (1);
next:
				;
			} /* end for over highest 8 bits */
		} /* end lcalc */
	} /* end for over lowest 32 bits */
end:
	checksum_alpha[id] = private_checksum_alpha;
	mxoffset[id] = (ulong)(max_n0 - ((uint128_t)(task_id + 0) << task_size));
}
