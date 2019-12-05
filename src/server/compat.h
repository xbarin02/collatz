#ifndef COMPAT_COMPAT_H_
#define COMPAT_COMPAT_H_

#ifdef __WIN32__
	#pragma GCC diagnostic ignored "-Wlong-long"
	#pragma GCC diagnostic ignored "-Wformat"
#endif

#include <limits.h>

/* count trailing zeros */
__attribute__ ((unused))
static int __builtin_ctzx(uint128_t n)
{
	if ((unsigned long)n == 0)
		return (sizeof(unsigned long) * CHAR_BIT) + __builtin_ctzl((unsigned long)(n >> (sizeof(unsigned long) * CHAR_BIT)));
	else
		return __builtin_ctzl((unsigned long)n);
}

#include <assert.h>

__attribute__ ((unused))
static int __builtin_ctzu64(uint64_t n)
{
	switch (sizeof(uint64_t)) {
		case sizeof(unsigned long): return __builtin_ctzl((unsigned long)n);
#ifdef __WIN32__
		case sizeof(unsigned long long): return __builtin_ctzll((unsigned long long)n);
#endif
		default: assert(!"matching type");
	}
}

#include <stdlib.h>

__attribute__ ((unused))
static uint64_t atou64(const char *nptr)
{
	switch (sizeof(uint64_t)) {
		case sizeof(unsigned long): return (uint64_t)strtoul(nptr, NULL, 10);
#ifdef __WIN32__
		case sizeof(unsigned long long): return (uint64_t)strtoull(nptr, NULL, 10);
#endif
		default: assert(!"matching type");
	}
}

#endif /* COMPAT_COMPAT_H_ */
