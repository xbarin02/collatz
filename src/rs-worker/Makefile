CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE=500
LDFLAGS=-Wl,--as-needed
LDLIBS+=
BINS=rs-worker

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
endif

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
