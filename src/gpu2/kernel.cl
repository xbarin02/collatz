size_t ctzl(unsigned long n)
{
        return 63 - clz(n & -n);
}

typedef unsigned __int128 uint128_t;

#define UINT128_MAX (~(uint128_t)0)

size_t ctzu128(uint128_t n)
{
        size_t a = ctzl(n);

        if (a == ~0UL)
                return 64 + ctzl(n>>64);

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
#if 0
	/* n (global) */
        uint128_t n      = ((uint128_t)task_id << task_size) + 3;
	/* n_sup (global) */
        uint128_t n_sup  = ((uint128_t)task_id << task_size) + 3 + ((unsigned long)1 << task_size);
#endif
	/* n (local) */
	uint128_t n_     = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 0) << (task_size - task_units)) + 3;
	/* n_sup (local) */
        uint128_t n_sup_ = ((uint128_t)task_id << task_size) + ((uint128_t)(id + 1) << (task_size - task_units)) + 3;
#if 0
	printf("worker %lu: RANGE 0x%016lu:%016lu 0x%016lu:%016lu [LOCAL RANGE 0x%016lu:%016lu 0x%016lu:%016lu]\n",
		id,
		(unsigned long)(n>>64), (unsigned long)n,
		(unsigned long)(n_sup>>64), (unsigned long)n_sup,
		(unsigned long)(n_>>64), (unsigned long)n_,
		(unsigned long)(n_sup_>>64), (unsigned long)n_sup_
	);
#endif

	for (; n_ < n_sup_; n_ += 4) {
		uint128_t n = n_, n0 = n_;
#if 1
		do {
			n++;
			size_t alpha = ctzu128(n);
			private_checksum_alpha += alpha;
			n >>= alpha;
			if(n > UINT128_MAX >> 2*alpha || alpha >= 81) {
				private_overflow_counter++;
				break;
			}
			n *= pow3(alpha);
			n--;
			n >>= ctzu128(n);
			if (n < n0) {
				break;
			}
		} while (1);
#endif
	}

	overflow_counter[id] = private_overflow_counter;
	checksum_alpha[id] = private_checksum_alpha;
}
