CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -fopenmp
LDLIBS+=-lgmp
BINS=collatz collatz4 prescreen

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)
