CFLAGS+=-std=c2x -pedantic -Wall -Wextra -Xclang -fopenmp -march=native -O3 -D_XOPEN_SOURCE -D_GNU_SOURCE
LDFLAGS+=-lomp
LDLIBS+=-lm
BINS=server chk-tool verify-result find-maxima

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-I/opt/homebrew/include -D_USE_GMP
	LDLIBS+=-L/opt/homebrew/lib -lgmp
endif

ifeq ($(USE_LIBM), 1)
	LDLIBS+=-lm
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
