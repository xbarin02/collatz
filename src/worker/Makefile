CFLAGS+=-std=c2x -v -pedantic -Wall -Wextra -Wno-long-long -march=native -O3 -D_XOPEN_SOURCE=500
LDFLAGS=
LDLIBS+=
BINS=worker

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-I/opt/homebrew/include -D_USE_GMP
	LDLIBS+=-L/opt/homebrew/lib -lgmp
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

ifeq ($(USE_LUT50), 1)
        CFLAGS+=-DUSE_LUT50
endif

ifndef $(SIEVE_LOGSIZE)
        SIEVE_LOGSIZE=32
endif
CFLAGS+=-DSIEVE_LOGSIZE=$(SIEVE_LOGSIZE)

ifeq ($(DEBUG), 1)
        CFLAGS+=-Og -g -DDEBUG
        LDLIBS+=-rdynamic
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
