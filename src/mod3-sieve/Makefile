CFLAGS+=-std=c99 -pedantic -Wall -Wextra -g -D_XOPEN_SOURCE -D_GNU_SOURCE -march=native -O3
LDFLAGS+=-rdynamic
LDLIBS+=-lm

BIN=main

ifeq ($(BUILD),debug)
	CFLAGS+=-Og -g
	LDFLAGS+=-rdynamic
endif

ifeq ($(BUILD),release)
	CFLAGS+=-march=native -O3 -DNDEBUG
endif

ifeq ($(BUILD),profile-generate)
	CFLAGS+=-march=native -O3 -DNDEBUG -fprofile-generate
	LDFLAGS+=-fprofile-generate
endif

ifeq ($(BUILD),profile-use)
	CFLAGS+=-march=native -O3 -DNDEBUG -fprofile-use
endif

ifeq ($(BUILD),profile)
	CFLAGS+=-Og -g -pg
	LDFLAGS+=-rdynamic -pg
endif

.PHONY: all
all: $(BIN)

.PHONY: clean
clean:
	-$(RM) -- *.o $(BIN)

.PHONY: distclean
distclean: clean
	-$(RM) -- *.gcda gmon.out cachegrind.out.* callgrind.out.*

process: process.o libfactor.o
