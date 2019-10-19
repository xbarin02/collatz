CFLAGS+=-std=c89 -pedantic -Wall -Wextra -march=native -O3 -D_XOPEN_SOURCE
LDLIBS+=-lOpenCL
LDFLAGS+=

ifndef $(TASK_UNITS)
	TASK_UNITS=16
endif
CFLAGS+=-DTASK_UNITS=$(TASK_UNITS)

CFLAGS+=$(EXTRA_CFLAGS)
LDLIBS+=$(EXTRA_LDLIBS)
LDFLAGS+=$(EXTRA_LDFLAGS)

BINS=gpuworker

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	$(RM) -- $(BINS)

.PHONY: distclean
distclean: clean
	$(RM) -- *.gcda
