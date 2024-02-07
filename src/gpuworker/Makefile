CFLAGS+=-std=gnu99 -pedantic -Wall -Wextra -Wno-long-long -march=native -O3 -D_XOPEN_SOURCE=500 -DCL_TARGET_OPENCL_VERSION=220 -DCL_USE_DEPRECATED_OPENCL_1_2_APIS
LDFLAGS+=
LDLIBS+=-lOpenCL
BINS=gpuworker

USE_ASYNC_CALL=1

ifeq ($(USE_LUMI), 1)
	CFLAGS+=-I/opt/rocm/include
	LDLIBS=-L/opt/rocm/lib -Wl,-rpath,/opt/rocm/lib -lOpenCL
endif

ifndef $(SIEVE_LOGSIZE)
	SIEVE_LOGSIZE=16
endif
CFLAGS+=-DSIEVE_LOGSIZE=$(SIEVE_LOGSIZE)

ifndef $(TASK_UNITS)
	TASK_UNITS=16
endif
CFLAGS+=-DTASK_UNITS=$(TASK_UNITS)

ifeq ($(USE_LIBGMP), 1)
	CFLAGS+=-I/opt/homebrew/include -D_USE_GMP
	LDLIBS+=-L/opt/homebrew/lib -lgmp
endif

ifeq ($(USE_SIEVE3), 1)
	CFLAGS+=-DUSE_SIEVE3
endif

ifeq ($(USE_SIEVE9), 1)
	CFLAGS+=-DUSE_SIEVE9
endif

ifeq ($(USE_LUT50), 1)
        CFLAGS+=-DUSE_LUT50
endif

ifeq ($(USE_ASYNC_CALL), 1)
	CFLAGS+=-DUSE_ASYNC_CALL
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
