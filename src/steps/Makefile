CFLAGS+=-std=c2x -pedantic -Wall -Wextra -Wno-long-long -march=native -O3 -D_XOPEN_SOURCE
LDFLAGS=
LDLIBS+=-lm
BINS=steps max-steps max-value max-strength max-completeness max-residue show path

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-I/opt/homebrew/include -D_USE_GMP
	LDLIBS+=-L/opt/homebrew/lib -lgmp
endif

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
