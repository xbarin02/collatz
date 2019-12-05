CFLAGS += -std=c89 -pedantic -Wall -Wextra -fopenmp -march=native -O3 -D_XOPEN_SOURCE -D_GNU_SOURCE
LDFLAGS += -fopenmp
LDLIBS +=
BINS = mclient

CFLAGS+=$(EXTRA_CFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)
LDFLAGS+=$(EXTRA_LDFLAGS)

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
