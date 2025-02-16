#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// #define LOG2 18
#define LOG2 24
#define LOG3 6

typedef unsigned long ulong;

/* a*n + c */
struct pair {
	ulong a, b;
};

int even_p(struct pair *pair)
{
	assert(pair != NULL);

	return (pair->a & 1) == 0 && (pair->b & 1) == 0;
}

int odd_p(struct pair *pair)
{
	assert(pair != NULL);

	return (pair->a & 1) == 0 && (pair->b & 1) == 1;
}

int indeterminate_p(struct pair *pair)
{
	assert(pair != NULL);

	return (pair->a & 1) == 1;
}

void step(struct pair *pair)
{
	assert(!indeterminate_p(pair));

	assert(pair != NULL);

	if (even_p(pair)) {
		pair->a /= 2;
		pair->b /= 2;
	} else {
		pair->a = (3 * pair->a) / 2;
		pair->b = (3 * pair->b + 1) / 2;
	}
}

void print(struct pair *pair)
{
	assert(pair != NULL);

	printf("%lun + %lu", pair->a, pair->b);
}

/* for sufficiently large n */
int less(struct pair *l, struct pair *r)
{
	assert(l != NULL && r != NULL);

	return l->a < r->a;
}

ulong pow3(ulong k)
{
	ulong r = 1;

	for (ulong i = 0; i < k; ++i) {
		r *= 3;
	}

	return r;
}

/* [3,9,27,81][residue] */
int *tab[LOG3];

void init()
{
	for (ulong k = 1; k < LOG3+1; ++k) {
		tab[k-1] = malloc(sizeof(int) * pow3(k));
		if (tab[k-1] == NULL) {
			abort();
		}
		for (ulong b = 0; b < pow3(k); ++b) {
			tab[k-1][b] = 0;
		}
	}
}

void kill(struct pair *pair)
{
	assert(pair != NULL);

	for (ulong i = 0, a = 3; a < pow3(LOG3+1); a *= 3, ++i) {
		if (pair->a == a) {
// 			printf(" (dead)");
			tab[i][pair->b] = 1;
		}
	}
}

void loop(struct pair *pair)
{
	struct pair pair0 = *pair;

// 	print(pair);

	while (!indeterminate_p(pair)) {
// 		printf(" -> ");
		step(pair);
// 		print(pair);

		if (less(&pair0, pair)) {
// 			printf("(dead)");
			kill(pair);
		}
	}

// 	printf("\n");
}

void dump()
{
	/* a => a*3. i => i+1 */
	for (ulong i = 0, a = 3; a < pow3(LOG3); a *= 3, ++i) {
		for (ulong j = 0; j < 3; ++j) {
			for (ulong b = 0; b < a; ++b) {
				tab[i+1][a*j + b] |= tab[i][b];
			}
		}
	}

#if 0
	printf("Dead:\n");

	for (ulong i = 0, a = 3; a < pow3(LOG3+1); a *= 3, ++i) {
		printf("(mod %lu) ", a);
		for (ulong b = 0; b < a; ++b) {
			printf("%i ", tab[i][b]);
		}
		printf("\n");
	}
#endif

	printf("Code:\n");

	for (ulong i = 0, a = 3; a < pow3(LOG3+1); a *= 3, ++i) {
		ulong c = 0;
		printf("n %% %lu != { ", a);
		for (ulong b = 0; b < a; ++b) {
			if (tab[i][b]) {
				printf("%lu, ", b);
				++c;
			}
		}
		printf("} // %f%% [%lu %lu]\n", 100.f*c/a, c, a-c);
	}
}

int main()
{
	struct pair pair;

	init();

	for (ulong a = 2; a < (1UL << LOG2); a *= 2) {
		for (ulong b = 1; b < a; b += 2) {
			pair = (struct pair){ a, b };

			loop(&pair);
		}
	}

	dump();

	return 0;
}
