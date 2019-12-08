CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE
LDFLAGS=
LDLIBS+=
BINS=worker

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
endif

ifeq ($(USE_SIEVE), 1)
	CFLAGS+=-DUSE_SIEVE
endif

ifeq ($(USE_PRECALC), 1)
	CFLAGS+=-DUSE_PRECALC
endif

ifeq ($(USE_SIEVE3), 1)
	CFLAGS+=-DUSE_SIEVE3
endif

ifeq ($(USE_SIEVE9), 1)
	CFLAGS+=-DUSE_SIEVE9
endif

ifeq ($(USE_ESIEVE), 1)
        CFLAGS+=-DUSE_ESIEVE
endif

CFLAGS+=$(EXTRA_CFLAGS)
LDFLAGS+=$(EXTRA_LDFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
