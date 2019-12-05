CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE=500
LDLIBS+=-lOpenCL
LDFLAGS+=
BINS=gpuworker

ifndef $(SIEVE_LOGSIZE)
	SIEVE_LOGSIZE=16
endif
CFLAGS+=-DSIEVE_LOGSIZE=$(SIEVE_LOGSIZE)

ifndef $(TASK_UNITS)
	TASK_UNITS=16
endif
CFLAGS+=-DTASK_UNITS=$(TASK_UNITS)

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-D_USE_GMP
	LDLIBS+=-lgmp
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
