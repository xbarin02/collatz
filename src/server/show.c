#include <stdio.h>
#include <stdint.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif
#include <assert.h>

mp_bitcnt_t mpz_ctz(const mpz_t n)
{
	return mpz_scan1(n, 0);
}

void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}

void mpz_get_maximum(mpz_t max, const mpz_t n0)
{
	mpz_t n;

	mpz_init_set(n, n0);
	mpz_set(max, n);

	while (mpz_cmp_ui(n, 1UL) != 0) {
		if (mpz_congruent_ui_p(n, 1UL, 2UL)) {
			/* odd step */
			mpz_mul_ui(n, n, 3UL);
			mpz_add_ui(n, n, 1UL);
		} else {
			/* even step */
			mpz_fdiv_q_ui(n, n, 2UL);
		}

		if (mpz_cmp(n, max) > 0) {
			mpz_set(max, n);
		}
	}

	mpz_clear(n);
}

void report(mpz_t n)
{
	int i = 0;
	mpz_t a;
	mp_bitcnt_t Salpha = 0, Sbeta = 0;
	mpz_t max;

	mpz_init(a);
	mpz_init(max);

	mpz_get_maximum(max, n);

	printf("<table>\n");
	printf("<tr><th>#</th><th>$n$</th><th>$(\\alpha, \\beta)$</th></tr>\n");

	do {
		mp_bitcnt_t alpha, beta;

		mpz_add_ui(n, n, 1UL);
		alpha = mpz_ctz(n);
		Salpha += alpha;
		mpz_fdiv_q_2exp(n, n, alpha);
		assert(alpha <= ULONG_MAX);
		mpz_pow3(a, (unsigned long)alpha);
		mpz_mul(n, n, a);

		mpz_sub_ui(n, n, 1UL);
		beta = mpz_ctz(n);
		Sbeta += beta;
		mpz_fdiv_q_2exp(n, n, beta);

		assert(beta <= ULONG_MAX);
		gmp_printf("<tr><td>%i</td><td>%Zi</td><td>$(%lu, %lu)$</td></tr>\n", ++i, n, (unsigned long)alpha, (unsigned long)beta);
	} while (mpz_cmp_ui(n, 1UL) != 0);

	printf("</table>\n");

	printf("<ul>\n");
	printf("<li>Cycles: %i\n", i);
	printf("<li>Steps: %lu\n", (unsigned long)(Salpha + Sbeta));
	printf("<li>Odd steps: %lu\n", (unsigned long)Salpha);
	printf("<li>Even steps: %lu\n", (unsigned long)Sbeta);
	gmp_printf("<li>Maximum: %Zi\n", max);
	printf("</ul>\n");

	mpz_clear(a);
	mpz_clear(max);
}

int main(int argc, char *argv[])
{
	const char *str_n = argc > 1 ? argv[1] : "1";
	mpz_t n;

	mpz_init(n);

	gmp_sscanf(str_n, "%Zi", n);

	report(n);

	mpz_clear(n);

	return 0;
}
