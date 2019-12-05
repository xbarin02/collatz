#include <stdio.h>
#include <stdlib.h>
#ifdef _USE_GMP
#	include <gmp.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _USE_GMP
	mpz_t n0, n, max;
	unsigned step = 0;

	if (argc < 2) {
		printf("[ERROR] argument expected\n");
		return EXIT_FAILURE;
	}

	mpz_init(n0);

	gmp_sscanf (argv[1], "%Zd", n0);

	mpz_init_set(n, n0); /* n = n0 */
	mpz_init_set(max, n); /* max = n */

	gmp_printf("step=%u n=%Zd\n", step, n);

	while (mpz_cmp_ui(n, 1UL) != 0) {
		/* next step */
		if (mpz_congruent_ui_p(n, 1UL, 2UL)) {
			/* odd step */
			mpz_mul_ui(n, n, 3UL);
			mpz_add_ui(n, n, 1UL);
		} else {
			/* even step */
			mpz_fdiv_q_ui(n, n, 2UL);
		}
		step++;

		if (mpz_cmp(n, max) > 0) {
			mpz_set(max, n);
		}

		gmp_printf("step=%u n=%Zd\n", step, n);
	}

	gmp_printf("maximum=%Zd (%lu)\n", max, (unsigned long)mpz_sizeinbase(max, 2));

	mpz_clear(max);
	mpz_clear(n);
	mpz_clear(n0);
#endif

	return 0;
}
