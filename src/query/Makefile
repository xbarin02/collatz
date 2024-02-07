CFLAGS+=-std=gnu99 -pedantic -Wall -Wextra -Xclang -fopenmp -march=native -O3 -D_XOPEN_SOURCE -D_POSIX_C_SOURCE=199309L
LDFLAGS+=-lomp
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
