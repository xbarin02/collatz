CFLAGS += -std=c89 -pedantic -Wall -Wextra -fopenmp -march=native -O3 $(EXTRA_CFLAGS)
LDFLAGS += -fopenmp $(EXTRA_LDFLAGS)
LDLIBS += $(EXTRA_LDLIBS)
BINS = server chk-tool verify-result

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
endif

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
