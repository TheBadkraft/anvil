CC      = gcc
STD     = c2x
ANVL    = ../..
TESTBIT = ../../../testbit

CFLAGS  = -Wall -Wextra -g -std=$(STD) \
          -I$(ANVL)/include \
          -I$(TESTBIT)/include \
          -D_POSIX_C_SOURCE=200809L

BIN = bin

LIB_DEBUG   = $(ANVL)/lib/debug/libanvil.a
LIB_RELEASE = $(ANVL)/lib/release/libanvil.a

TESTBIT_SRC = $(TESTBIT)/src/testbit.c

.PHONY: all debug release clean lib-debug lib-release

# These deliberately do NOT compile any src/*.c directly — only test_e2e.c and testbit's own
# harness are compiled here; everything else comes from the already-built static library, to
# prove the shipped artifact itself works, not just the sources that go into it.

all: release debug

lib-debug:
	$(MAKE) -C $(ANVL) lib-debug

lib-release:
	$(MAKE) -C $(ANVL) lib-release

$(LIB_DEBUG): lib-debug

$(LIB_RELEASE): lib-release

$(BIN)/test_e2e_debug: test_e2e.c $(TESTBIT_SRC) $(LIB_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) test_e2e.c $(TESTBIT_SRC) $(LIB_DEBUG) -o $@

$(BIN)/test_e2e_release: test_e2e.c $(TESTBIT_SRC) $(LIB_RELEASE)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) test_e2e.c $(TESTBIT_SRC) $(LIB_RELEASE) -o $@

debug: $(BIN)/test_e2e_debug
	$(BIN)/test_e2e_debug

release: $(BIN)/test_e2e_release
	$(BIN)/test_e2e_release

clean:
	rm -f $(BIN)/*
