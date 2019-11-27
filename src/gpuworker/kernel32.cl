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

#define LUT_SIZE32 21

/* in log2 */
#define SIEVE_LOGSIZE 32
#define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8)
#define SIEVE_MASK ((1UL << SIEVE_LOGSIZE) - 1)

#define IS_LIVE(n) ( ( sieve[ (n)>>3 ] >> ((n)&7) ) & 1 )

__kernel void worker(
	__global ulong *checksum_alpha,
	ulong task_id,
	ulong task_size,
	ulong task_units,
	__global uchar *sieve,
	__global ulong *mxoffset,
	__global ulong *cycleoff
)
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

	ulong max_cycles = 0;
	uint128_t max_cycles_n0 = ((uint128_t)(task_id + 0) << task_size);

	/* iterate over lowest (32 - task_units) bits */
	for (uint128_t n0 = n_minimum + 3; n0 < n_supremum + 3; n0 += 4) {
		if (!IS_LIVE(n0 & SIEVE_MASK)) {
			continue;
		}

		/* precalc(n0): */

		int R = SIEVE_LOGSIZE; /* copy since we need to decrement it */
		ulong L = (ulong)n0; /* only 32-LSbits in n0 */
		ulong L0 = (ulong)n0; /* copy of L */
		size_t Salpha = 0, Sbeta = 0; /* sum of alpha, beta */
		ulong cycles = 0;

		do {
			L++;
			do {
				size_t alpha = (size_t)ctz((uint)L);
				alpha = min(alpha, (size_t)LUT_SIZE32 - 1);
				alpha = min(alpha, (size_t)R);
				R -= alpha;
				Salpha += alpha;
				L >>= alpha;
				if (L > ~0UL >> 2*alpha) {
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
			cycles++;
			if (L == 0) {
				R -= R;
				Sbeta += R;
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

lcalc:
		/* up to this point, the L, L0, alpha, and beta are okay */
		{
			R = Salpha + Sbeta; /* R-LSbits have been precalculated above */

			private_checksum_alpha += Salpha << (task_size - R);

			/* iterate over highest 40 - 32 = 8 bits */
			for (uint128_t h = 0; h < (1UL << (task_size - R)); ++h) {
				uint128_t H = ((uint128_t)task_id << task_size) + (h << R);
				uint128_t N = H >> (Salpha + Sbeta);
				uint128_t N0 = H + L0;
				ulong tot_cycles = cycles;

				/* WARNING: this zeroes the alpha which is needed for the subsequent iterations */
				size_t Salpha0 = Salpha;
				do {
					size_t alpha = min(Salpha0, (size_t)LUT_SIZE32 - 1);
					if (N > UINT128_MAX >> 2*alpha) {
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

				/* tot_cycles += check2(N0, N); */
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
						if (N > UINT128_MAX >> 2*alpha) {
							private_checksum_alpha = 0;
							goto end;
						}
						N *= lut[alpha];
					} while (!(N & 1));
					N--;
					tot_cycles++;
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
						if (tot_cycles > max_cycles) {
							max_cycles = tot_cycles;
							max_cycles_n0 = N0;
						}
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
	cycleoff[id] = (ulong)(max_cycles_n0 - ((uint128_t)(task_id + 0) << task_size));
}
