CFLAGS+=-std=c89 -pedantic -Wall -Wextra -fopenmp -march=native -O3 -D_XOPEN_SOURCE
LDFLAGS+=
LDLIBS+=
BINS=query

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
