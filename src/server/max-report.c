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

#include "wideint.h"
#include "compat.h"

#define TASK_SIZE 40

#define LUT_SIZE64 41

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

void report()
{
	mpz_t N;
	mpf_t x2;
	mpz_t Mx2; /* 2 * Mx */

	mpz_init_set_u128(N, g_n0);
	mpf_init(x2);
	mpz_init(Mx2);

	mpz_mul_ui(Mx2, Mx, 2UL);

	Expansion(x2, N, Mx2);

	gmp_printf("<tr><td>%Zi</td><td>%Zi</td>", N, Mx2);
	gmp_printf("<td>%Ff</td>", x2);
	gmp_printf("<td>%lu</td><td>%lu</td></tr>\n", (unsigned long)mpz_sizeinbase(N, 2), (unsigned long)mpz_sizeinbase(Mx2, 2));

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

void solve_task(uint64_t task_id, uint64_t task_size)
{
	uint128_t n, n_min, n_sup;

	/* n of the form 4n+3 */
	n_min = ((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ((uint128_t)(task_id + 1) << task_size) + 3;

	for (n = n_min; n < n_sup; n += 4) {
		check(n, n);
	}
}

int main()
{
	uint64_t n;
	FILE *stream;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	init_lut();

	stream = fopen("max-assignments.txt", "r");

	if (stream == NULL) {
		abort();
	}

	mpz_init_set_ui(Mx, 0UL);

	printf("<tr><th>#</th><th>N</th><th>Mx(N)</th><th>X<sub>2</sub>(N)</th><th>B(N)</th><th>B(Mx(N))</th></tr>\n");

	/* for each assignment in the text file */
	while (fscanf(stream, "%" SCNu64, &n) == 1) {
		printf("<!-- assignment %" PRIu64 " -->\n", n);
		/* solve task */
		solve_task(n, TASK_SIZE);
	}

	fclose(stream);

	return 0;
}
