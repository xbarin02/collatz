CFLAGS+=-std=c2x -Wall -Wextra -Wno-strict-prototypes -Xclang -fopenmp -march=native -O3 -D_XOPEN_SOURCE -D_GNU_SOURCE
LDFLAGS+=-lomp
LDLIBS+=
BINS=mclient

ifeq ($(DEBUG), 1)
        CFLAGS+=-Og -g -DDEBUG
        LDLIBS+=-rdynamic
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
