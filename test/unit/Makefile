CC      = gcc
STD     = c2x
ANVIL   = ../..
TESTBIT = ../../../testbit

BUILD_NUM := $(shell git rev-list --count HEAD)
CFLAGS  = -Wall -Wextra -g -std=$(STD) \
          -I$(ANVIL)/include \
          -I$(TESTBIT)/include \
          -D_POSIX_C_SOURCE=200809L \
          -DANVL_BUILD=$(BUILD_NUM)

ANVIL_SRCS = $(ANVIL)/src/core/anvil.c \
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
LIB_DEBUG     = $(ANVIL)/lib/debug/libanvil.a
LIB_RELEASE   = $(ANVIL)/lib/release/libanvil.a

BIN = bin

.PHONY: all clean valgrind test_scalar_quotes test_amp_collections test_release test_version

all: $(BIN)/test_version $(BIN)/test_scalar_quotes $(BIN)/test_amp_collections $(BIN)/test_context $(BIN)/test_interp_string $(BIN)/test_using $(BIN)/test_anon_block_attrs $(BIN)/test_resolver $(BIN)/test_schema

$(BIN)/test_version: test_version.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_version: $(BIN)/test_version
	@$(BIN)/test_version

$(BIN)/test_context: test_context.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_context: $(BIN)/test_version $(BIN)/test_context
	@$(BIN)/test_version
	@$(BIN)/test_context

$(BIN)/test_scalar_quotes: test_scalar_quotes.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_scalar_quotes: $(BIN)/test_version $(BIN)/test_scalar_quotes
	@$(BIN)/test_scalar_quotes
	@$(BIN)/test_version

$(BIN)/test_amp_collections: test_amp_collections.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@
	
test_amp_collections: $(BIN)/test_version $(BIN)/test_amp_collections
	@$(BIN)/test_amp_collections
	@$(BIN)/test_version

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

test_release: $(LIB_RELEASE)
	$(CC) $(CFLAGS) test_scalar_quotes.c    $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_scalar_quotes
	$(CC) $(CFLAGS) test_amp_collections.c  $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_amp_collections
	$(CC) $(CFLAGS) test_interp_string.c    $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_interp_string
	$(CC) $(CFLAGS) test_using.c            $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_using
	$(CC) $(CFLAGS) test_anon_block_attrs.c $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_anon_block_attrs
	$(CC) $(CFLAGS) test_resolver.c         $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_resolver
	$(CC) $(CFLAGS) test_schema.c           $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_schema
	@echo "-- release QC --"
	@$(BIN)/rel_test_scalar_quotes
	@$(BIN)/rel_test_amp_collections
	@$(BIN)/rel_test_interp_string
	@$(BIN)/rel_test_using
	@$(BIN)/rel_test_anon_block_attrs
	@$(BIN)/rel_test_resolver
	@$(BIN)/rel_test_schema

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
