CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE
LDLIBS+=

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
endif

ifeq ($(USE_SIEVE), 1)
	CFLAGS+=-DUSE_SIEVE
endif

CFLAGS+=$(EXTRA_CFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)

LDFLAGS=
BINS=worker

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
