#include "wideint.h"
#include <assert.h>
#include <stdio.h>

uint128_t pow3u128(uint128_t n)
{
	uint128_t r = 1;
	uint128_t b = 3;

	while (n) {
		if (n & 1) {
			assert(r <= UINT128_MAX / b);
			r *= b;
		}

		assert(b <= UINT128_MAX / b);
		b *= b;

		n >>= 1;
	}

	return r;
}

void print(uint128_t n)
{
	char buff[40];
	char r_buff[40];

	char *ptr = buff;

	int i = 0;

	while (n != 0) {
		*(ptr++) = '0' + n % 10;
		n /= 10;
	}

	*ptr = 0;

	ptr--;

	while (ptr >= buff) {
		r_buff[i++] = *(ptr--);
	}

	r_buff[i] = 0;

	puts(r_buff);
}

#define ARR_LEN 43

void arr_init(int *arr)
{
	int i = 0;

	for (; i < ARR_LEN; ++i) {
		arr[i] = 0;
	}
}

uint128_t b_evaluate(int *arr)
{
	int exp = 0;
	uint128_t b = 0;

	for (; exp < ARR_LEN; ++exp) {
		assert(4 * (uint128_t)arr[exp] <= (UINT128_MAX - b) / pow3u128(exp));

		b += 4 * pow3u128(exp) * arr[exp];
	}

	return b;
}

int arr_increment(int *arr)
{
	int exp = 0;
	int c = 1; /* carry in */

	for (; exp < ARR_LEN; ++exp) {
		if (c) {
			if (arr[exp] == 1) {
				arr[exp] = 0;
				c = 1;
			} else {
				arr[exp] = 1;
				c = 0;
			}
		} else {
			break;
		}
	}

	/* carry out */
	if (c) {
		return 1;
	}

	return 0;
}

int main()
{
	uint128_t n;

	int arr[ARR_LEN];
	arr_init(arr);

	while (1) {
		assert(pow3u128(44) <= (UINT128_MAX - b_evaluate(arr) - 3) / 4);

		n = 4 * pow3u128(44) + 3 + b_evaluate(arr);

		printf("check the trajectory of ");
		print(n);

		if (arr_increment(arr) > 0) {
			break;
		}
	}

	return 0;
}
