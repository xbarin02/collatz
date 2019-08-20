CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -fopenmp
LDLIBS+=-lgmp
BINS=collatz collatz_128 collatz_enum collatz4 collatz4_128 simple2 prescreen

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)
