CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 $(EXTRA_CFLAGS) -D_XOPEN_SOURCE -D_USE_GMP
LDLIBS+=-lgmp $(EXTRA_LDLIBS)
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
