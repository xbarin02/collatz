CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -fopenmp $(EXTRA_CFLAGS)
LDLIBS+=-lgmp $(EXTRA_LDLIBS)
BINS=collatz collatz_128 collatz_128_2 collatz_enum collatz_task collatz_task2 collatz4 collatz4_128 simple2 prescreen

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
