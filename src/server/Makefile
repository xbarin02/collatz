CFLAGS += -std=c89 -pedantic -Wall -Wextra -fopenmp -march=native -O3 $(EXTRA_CFLAGS)
LDFLAGS += -fopenmp $(EXTRA_LDFLAGS)
LDLIBS += $(EXTRA_LDLIBS)
BINS = server

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
