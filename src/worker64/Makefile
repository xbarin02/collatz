CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE
LDLIBS+=
LDFLAGS+=
BINS=worker

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
endif

ifeq ($(ENABLE_ALPHA), 1)
	CFLAGS+=-DENABLE_ALPHA=1
endif

ifeq ($(ENABLE_BETA), 1)
	CFLAGS+=-DENABLE_BETA=1
endif

CFLAGS+=$(EXTRA_CFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)
LDFLAGS+=$(EXTRA_LDFLAGS)

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
