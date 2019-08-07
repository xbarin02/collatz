CFLAGS=-std=c89 -pedantic -Wall -Wextra -march=native -O3
BINS=collatz collatz2 collatz3 collatz4 prescreen
LDLIBS=-lgmp

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)
