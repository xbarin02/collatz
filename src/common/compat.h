#ifndef COMPAT_COMPAT_H_
#define COMPAT_COMPAT_H_

#ifdef __WIN32__
#	pragma GCC diagnostic ignored "-Wlong-long"
#	pragma GCC diagnostic ignored "-Wformat"
#endif

#if defined(__GNUC__)
#	define UNUSED __attribute__ ((unused))
#	define HOT __attribute__ ((hot))
#else
#	define UNUSED
#	define HOT
#endif

#include <stdint.h>
#include <assert.h>

UNUSED HOT
static int ctzu64(uint64_t n)
{
	if (n == 0) {
		return 64;
	}

	switch (sizeof(uint64_t)) {
		case sizeof(unsigned long): return __builtin_ctzl((unsigned long)n);
#ifdef __WIN32__
		case sizeof(unsigned long long): return __builtin_ctzll((unsigned long long)n);
#endif
		default: assert(!"matching type"); /* __builtin_trap() in gcc 4.7 */
	}
}

#include <stdlib.h>

UNUSED
static uint64_t atou64(const char *nptr)
{
	switch (sizeof(uint64_t)) {
		case sizeof(unsigned long): return (uint64_t)strtoul(nptr, NULL, 10);
#ifdef __WIN32__
		case sizeof(unsigned long long): return (uint64_t)strtoull(nptr, NULL, 10);
#endif
		default: assert(!"matching type"); /* __builtin_trap() in gcc 4.7 */
	}
}

UNUSED
static unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

#endif /* COMPAT_COMPAT_H_ */
