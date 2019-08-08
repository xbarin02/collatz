CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -fopenmp
LDLIBS+=-lgmp
BINS=collatz collatz2 collatz3 collatz4 prescreen

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: collatz4-pgo
collatz4-pgo:
	$(RM) -- collatz4 collatz4.o collatz4.gcda
	CFLAGS=-fprofile-generate LDFLAGS=-fprofile-generate $(MAKE) collatz4
	./collatz4 32
	$(RM) -- collatz4 collatz4.o
	CFLAGS="-fprofile-use -fprofile-correction" LDFLAGS=-fprofile-use $(MAKE) collatz4
