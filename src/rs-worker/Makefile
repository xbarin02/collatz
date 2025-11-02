CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE=500 -fopenmp
LDFLAGS=-Wl,--as-needed -fopenmp
LDLIBS+=
BINS=rs-worker rs-worker-1thread rs-worker-sc

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
endif

ifeq ($(DEBUG), 1)
        CFLAGS+=-Og -g -DDEBUG
        LDLIBS+=-rdynamic
endif

ifeq ($(USE_SIEVE), 1)
	CFLAGS+=-DUSE_SIEVE
endif

ifeq ($(USE_LUT50), 1)
        CFLAGS+=-DUSE_LUT50
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
