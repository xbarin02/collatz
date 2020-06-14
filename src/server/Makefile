CFLAGS+=-std=c89 -pedantic -Wall -Wextra -fopenmp -march=native -O3 -D_XOPEN_SOURCE -D_GNU_SOURCE
LDFLAGS+=-fopenmp
LDLIBS+=-lm
BINS=server chk-tool verify-result path find-maxima

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
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
