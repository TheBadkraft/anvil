CC      = gcc
STD     = c2x
ANVIL   = ../..
TESTBIT = ../../../testbit

CFLAGS  = -Wall -Wextra -g -std=$(STD) \
          -I$(ANVIL)/include \
          -I$(TESTBIT)/include \
          -D_POSIX_C_SOURCE=200809L

ANVIL_SRCS = $(ANVIL)/src/core/anvil_impl.c \
             $(ANVIL)/src/core/context.c \
             $(ANVIL)/src/core/errors.c \
             $(ANVIL)/src/core/memory.c \
             $(ANVIL)/src/core/parser.c \
             $(ANVIL)/src/core/strings.c \
             $(ANVIL)/src/utilities/utils.c

RESOLVER_SRCS = $(ANVIL)/src/resolver/resolver.c
VARS_SRCS     = $(ANVIL)/src/vars/vars.c

DEFAULT_LIB = $(ANVIL_SRCS) $(RESOLVER_SRCS) $(VARS_SRCS)

TESTBIT_SRC = $(TESTBIT)/src/testbit.c

SCHEMA_SRCS   = $(ANVIL)/src/schema/schema.c
SCHEMA_LIB    = $(DEFAULT_LIB) $(SCHEMA_SRCS)

BIN = bin

.PHONY: all clean valgrind test_scalar_quotes

all: $(BIN)/test_scalar_quotes $(BIN)/test_amp_collections $(BIN)/test_interp_string $(BIN)/test_using $(BIN)/test_anon_block_attrs $(BIN)/test_resolver $(BIN)/test_schema

$(BIN)/test_scalar_quotes: test_scalar_quotes.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_scalar_quotes: $(BIN)/test_scalar_quotes
	@$(BIN)/test_scalar_quotes

$(BIN)/test_amp_collections: test_amp_collections.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/test_interp_string: test_interp_string.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/test_using: test_using.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/test_anon_block_attrs: test_anon_block_attrs.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/test_resolver: test_resolver.c $(DEFAULT_LIB) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/test_schema: test_schema.c $(SCHEMA_LIB) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

valgrind: all
	valgrind --leak-check=full --track-origins=yes $(BIN)/test_scalar_quotes
	valgrind --leak-check=full --track-origins=yes $(BIN)/test_amp_collections
	valgrind --leak-check=full --track-origins=yes $(BIN)/test_interp_string
	valgrind --leak-check=full --track-origins=yes $(BIN)/test_using
	valgrind --leak-check=full --track-origins=yes $(BIN)/test_anon_block_attrs
	valgrind --leak-check=full --track-origins=yes $(BIN)/test_resolver
	valgrind --leak-check=full --track-origins=yes $(BIN)/test_schema

clean:
	rm -rf $(BIN)
