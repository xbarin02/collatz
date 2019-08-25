CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 $(EXTRA_CFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)
BINS=collatz_task

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
