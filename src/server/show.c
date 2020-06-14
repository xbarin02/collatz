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

void report(mpz_t n)
{
	int i = 0;
	mpz_t a;

	mpz_init(a);

	printf("<table>\n");
	printf("<tr><th>#</th><th>$n$</th></tr>\n");

	do {
		mp_bitcnt_t alpha, beta;

		mpz_add_ui(n, n, 1UL);
		alpha = mpz_ctz(n);
		mpz_fdiv_q_2exp(n, n, alpha);
		assert(alpha <= ULONG_MAX);
		mpz_pow3(a, (unsigned long)alpha);
		mpz_mul(n, n, a);

		mpz_sub_ui(n, n, 1UL);
		beta = mpz_ctz(n);
		mpz_fdiv_q_2exp(n, n, beta);

		gmp_printf("<tr><td>%i</td><td>%Zi</td></tr>\n", ++i, n);
	} while (mpz_cmp_ui(n, 1UL) != 0);

	printf("</table>\n");

	mpz_clear(a);
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
