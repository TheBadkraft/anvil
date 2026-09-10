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
SO_DEBUG    = $(ANVL)/lib/debug/libanvil.so
SO_RELEASE  = $(ANVL)/lib/release/libanvil.so

TESTBIT_SRC = $(TESTBIT)/src/testbit.c

.PHONY: all debug release so-debug so-release clean lib-debug lib-release build-so-debug build-so-release

# These deliberately do NOT compile any src/*.c directly — only test_e2e.c and testbit's own
# harness are compiled here; everything else comes from the already-built library (static or
# shared), to prove the shipped artifact itself works, not just the sources that go into it.

all: release debug so-release so-debug

lib-debug:
	$(MAKE) -C $(ANVL) lib-debug

lib-release:
	$(MAKE) -C $(ANVL) lib-release

build-so-debug:
	$(MAKE) -C $(ANVL) so-debug

build-so-release:
	$(MAKE) -C $(ANVL) so-release

$(LIB_DEBUG): lib-debug

$(LIB_RELEASE): lib-release

$(SO_DEBUG): build-so-debug

$(SO_RELEASE): build-so-release

$(BIN)/test_e2e_debug: test_e2e.c $(TESTBIT_SRC) $(LIB_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) test_e2e.c $(TESTBIT_SRC) $(LIB_DEBUG) -o $@

$(BIN)/test_e2e_release: test_e2e.c $(TESTBIT_SRC) $(LIB_RELEASE)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) test_e2e.c $(TESTBIT_SRC) $(LIB_RELEASE) -o $@

$(BIN)/test_e2e_so_debug: test_e2e.c $(TESTBIT_SRC) $(SO_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) test_e2e.c $(TESTBIT_SRC) -L$(ANVL)/lib/debug -lanvil \
		-Wl,-rpath,$(abspath $(ANVL)/lib/debug) -o $@

$(BIN)/test_e2e_so_release: test_e2e.c $(TESTBIT_SRC) $(SO_RELEASE)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) test_e2e.c $(TESTBIT_SRC) -L$(ANVL)/lib/release -lanvil \
		-Wl,-rpath,$(abspath $(ANVL)/lib/release) -o $@

debug: $(BIN)/test_e2e_debug
	$(BIN)/test_e2e_debug

release: $(BIN)/test_e2e_release
	$(BIN)/test_e2e_release

so-debug: $(BIN)/test_e2e_so_debug
	$(BIN)/test_e2e_so_debug

so-release: $(BIN)/test_e2e_so_release
	$(BIN)/test_e2e_so_release

clean:
	rm -f $(BIN)/*
