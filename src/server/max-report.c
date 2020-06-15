#include <stdio.h>
#ifdef _USE_GMP
#	include <gmp.h>
#else
#	error "GMP support missing"
#endif
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <inttypes.h>

#define USE_SIEVE 1
#define USE_ESIEVE 1
#define USE_LUT50 1
#define SIEVE_LOGSIZE 34

#ifdef USE_SIEVE
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <sys/mman.h>
#	include <unistd.h>
#endif

#include <math.h>

#include "wideint.h"
#include "compat.h"

#define TASK_SIZE 40

#define LUT_SIZE64 41

#ifdef USE_LUT50
static const uint64_t dict[] = {
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

#ifdef USE_SIEVE
#	ifndef SIEVE_LOGSIZE
#		define SIEVE_LOGSIZE 32
#	endif
#	define SIEVE_MASK ((1UL << SIEVE_LOGSIZE) - 1)
#	ifdef USE_LUT50
#		define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8 / 8)
#		define GET_INDEX(n) (g_map_sieve[((n) & SIEVE_MASK) >> (3+3)])
#		define IS_LIVE(n) ((dict[GET_INDEX(n)] >> ((n)&63)) & 1)
#	else
#		define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8)
#		define IS_LIVE(n) ((g_map_sieve[ ((n) & SIEVE_MASK)>>3 ] >> (((n) & SIEVE_MASK)&7)) & 1)
#	endif
#endif

#ifdef USE_SIEVE
const unsigned char *g_map_sieve;
#endif

uint64_t g_lut64[LUT_SIZE64];

void mpz_init_set_u128(mpz_t rop, uint128_t op)
{
	uint64_t nh = (uint64_t)(op>>64);
	uint64_t nl = (uint64_t)(op);

	assert(sizeof(unsigned long) == sizeof(uint64_t));

	mpz_init_set_ui(rop, (unsigned long)nh);
	mpz_mul_2exp(rop, rop, (mp_bitcnt_t)64);
	mpz_add_ui(rop, rop, (unsigned long)nl);
}

static mp_bitcnt_t mpz_ctz(const mpz_t n)
{
	return mpz_scan1(n, 0);
}

static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE64; ++a) {
		g_lut64[a] = pow3u64((uint64_t)a);
	}
}

mpz_t Mx;
uint128_t g_n0;
uint128_t g_prev_n0 = 0;
int g_no = 1;

void Expansion(mpf_t x2, mpz_t N, mpz_t Mx)
{
	mpz_t N_square;
	mpf_t f_N_square;
	mpf_t f_Mx;

	mpz_init(N_square);

	mpz_mul(N_square, N, N);

	mpf_init(f_N_square);
	mpf_init(f_Mx);

	mpf_set_z(f_N_square, N_square);
	mpf_set_z(f_Mx, Mx);

	mpf_div(x2, f_Mx, f_N_square);

	mpf_clear(f_N_square);
	mpf_clear(f_Mx);

	mpz_clear(N_square);
}

double mpz_log2(const mpz_t op)
{
	signed long int exp;
	double d = mpz_get_d_2exp(&exp, op);

	return log2((double)d) + (double)exp;
}

void report()
{
	mpz_t N;
	mpf_t x2;
	mpz_t Mx2; /* 2 * Mx */

	if (g_prev_n0 != g_n0) {
		g_no++;
		g_prev_n0 = g_n0;
	}

	mpz_init_set_u128(N, g_n0);
	mpf_init(x2);
	mpz_init(Mx2);

#if 0
	mpz_mul_ui(Mx2, Mx, 2UL);
#else
	mpz_mul_ui(Mx2, Mx, 1UL);
#endif

	Expansion(x2, N, Mx2);

	gmp_printf("<tr><td>%i</td><td>%Zi</td><td>%Zi</td>", g_no, N, Mx2);
#if 0
	gmp_printf("<td>%Ff</td>", x2);
#endif
	gmp_printf("<td>%lu</td><td>%lu</td><td>%f</td></tr>\n", (unsigned long)mpz_sizeinbase(N, 2), (unsigned long)mpz_sizeinbase(Mx2, 2), mpz_log2(Mx2) / mpz_log2(N));

	mpz_clear(N);
	mpf_clear(x2);
	mpz_clear(Mx2);
}

void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
{
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t n0;
	mpz_t a;

	assert(alpha_ >= 0);
	alpha = (mp_bitcnt_t)alpha_;

	mpz_init(a);
	mpz_init_set_u128(n, n_);
	mpz_init_set_u128(n0, n0_);

	do {
		if (alpha > ULONG_MAX) {
			alpha = ULONG_MAX;
		}

		/* n *= lut[alpha] */
		mpz_pow3(a, (unsigned long)alpha);

		mpz_mul(n, n, a);

		/* n-- */
		mpz_sub_ui(n, n, 1UL);

		/* if (n > max_n) */
		if (mpz_cmp(n, Mx) > 0) {
			mpz_set(Mx, n);
			g_n0 = n0_;
			report();
		}

		beta = mpz_ctz(n);

		/* n >>= ctz(n) */
		mpz_fdiv_q_2exp(n, n, beta);

		/* all betas were factored out */

		if (mpz_cmp(n, n0) < 0) {
			break;
		}

		/* n++ */
		mpz_add_ui(n, n, 1UL);

		alpha = mpz_ctz(n);

		/* n >>= alpha */
		mpz_fdiv_q_2exp(n, n, alpha);
	} while (1);

	mpz_clear(a);
	mpz_clear(n);
	mpz_clear(n0);

	return;
}

void check(uint128_t n, uint128_t n0)
{
	mpz_t mpz_n;

	assert(n != UINT128_MAX);

	if (!(n & 1)) {
		goto even;
	}

	do {
		n++;

		do {
			int alpha = ctzu64(n);

			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}

			n >>= alpha;

			if (n > UINT128_MAX >> 2*alpha) {
				mpz_check2(n0, n, alpha);
				return;
			}

			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

	even:
		mpz_init_set_u128(mpz_n, n);
		if (mpz_cmp(mpz_n, Mx) > 0) {
			mpz_set(Mx, mpz_n);
			g_n0 = n0;
			report();
		}
		mpz_clear(mpz_n);

		do {
			int beta = ctzu64(n);

			n >>= beta;
		} while (!(n & 1));

		if (n < n0) {
			return;
		}
	} while (1);
}

int is_live(uint128_t n)
{
#ifdef USE_SIEVE
	return IS_LIVE(n);
#else
	return 1;
#endif
}

void solve_task(uint64_t task_id, uint64_t task_size)
{
	uint128_t n, n_min, n_sup;

	/* n of the form 4n+3 */
	n_min = ((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ((uint128_t)(task_id + 1) << task_size) + 3;

	for (n = n_min; n < n_sup; n += 4) {
		if (task_id == 0 || is_live(n)) {
			check(n, n);
		}
	}
}

#ifdef USE_SIEVE
const void *open_map(const char *path, size_t map_size)
{
	int fd = open(path, O_RDONLY, 0600);
	void *ptr;

	if (map_size == 0) {
		map_size = 1;
	}

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return ptr;
}
#endif

void init()
{
#ifdef USE_SIEVE
	char path[4096];
	size_t k = SIEVE_LOGSIZE;
	size_t map_size = SIEVE_SIZE;

#ifdef USE_ESIEVE
#	ifdef USE_LUT50
	sprintf(path, "esieve-%lu.lut50.map", (unsigned long)k);
#	else
	sprintf(path, "esieve-%lu.map", (unsigned long)k);
#	endif
#else
#	error "Unsupported"
#endif

	g_map_sieve = open_map(path, map_size);
#endif

	mpz_init_set_ui(Mx, 0UL);

	init_lut();
}

int main()
{
	uint64_t n;
	FILE *stream;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	init();

	stream = fopen("max-assignments.txt", "r");

	if (stream == NULL) {
		abort();
	}

	printf("<tr><th>#</th><th>N</th><th>Mx(N)</th>");
#if 0
	printf("<th>X<sub>2</sub>(N)</th>");
#endif
	printf("<th>B(N)</th><th>B(Mx(N))</th><th>r</th></tr>\n");

	/* for each assignment in the text file */
	while (fscanf(stream, "%" SCNu64, &n) == 1) {
		printf("<!-- assignment %" PRIu64 " -->\n", n);
		/* solve task */
		solve_task(n, TASK_SIZE);
	}

	fclose(stream);

	return 0;
}
