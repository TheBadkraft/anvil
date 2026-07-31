CC      = gcc
STD     = c2x
ANVL   = ../..
TESTBIT = ../../../testbit

BUILD_NUM := $(shell git rev-list --count HEAD)
CFLAGS  = -Wall -Wextra -g -std=$(STD) \
          -I$(ANVL)/include \
          -I$(TESTBIT)/include \
          -D_POSIX_C_SOURCE=200809L \
          -DANVL_BUILD=$(BUILD_NUM)

ANVIL_SRCS = $(ANVL)/src/core/anvil.c \
			 $(ANVL)/src/core/errors.c \
			 $(ANVL)/src/core/files.c \
			 $(ANVL)/src/core/module.c \
			 $(ANVL)/src/core/document.c \
			 $(ANVL)/src/core/source.c \
			 $(ANVL)/src/sigma/list.c \
			 $(ANVL)/src/sigma/arrays.c \
             $(ANVL)/src/sigma/memory.c \
             $(ANVL)/src/sigma/strings.c \
			 $(ANVL)/src/sigma/collections.c

TEST_HELPER  = $(ANVL)/test/utilities/helpers.c

RESOLVER_SRCS = $(ANVL)/src/resolver/resolver.c
VARS_SRCS     = $(ANVL)/src/vars/vars.c

DEFAULT_LIB = $(ANVIL_SRCS) $(RESOLVER_SRCS) $(VARS_SRCS)

TESTBIT_SRC = $(TESTBIT)/src/testbit.c

SCHEMA_SRCS   = $(ANVL)/src/schema/schema.c
SCHEMA_LIB    = $(DEFAULT_LIB) $(SCHEMA_SRCS)
LIB_DEBUG     = $(ANVL)/lib/debug/libanvil.a
LIB_RELEASE   = $(ANVL)/lib/release/libanvil.a

BIN = bin
VALGRIND_LOG = $(BIN)/valgrind_test.log
UNIT_CASES = $(patsubst test_%.c,%,$(wildcard test_*.c))

define run_test_bin
	@if [ -n "$(filter build,$(MAKECMDGOALS))" ]; then \
		echo "-- build only: $(1) --"; \
	elif [ -n "$(filter val,$(MAKECMDGOALS))" ]; then \
		mkdir -p $(BIN); \
		echo "-- valgrind: $(1) --"; \
		valgrind --leak-check=full --track-origins=yes \
			--error-exitcode=42 \
			--log-file=$(VALGRIND_LOG) \
			$(BIN)/$(1); \
		echo "Valgrind log: $(VALGRIND_LOG)"; \
	else \
		$(BIN)/$(1); \
	fi
endef

.PHONY: all clean val build test test_release
.PHONY: build_test_version build_test_core build_test_fixtures
.PHONY: test_version test_core $(UNIT_CASES)

all: $(BIN)/test_version \
	$(BIN)/test_core \
	$(BIN)/test_fixtures

$(BIN)/test_version: test_version.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_version: $(BIN)/test_version

test_version: $(BIN)/test_version
	$(call run_test_bin,test_version)

$(BIN)/test_core: test_core.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_core: $(BIN)/test_core

test_core: $(BIN)/test_version $(BIN)/test_core
	$(call run_test_bin,test_core)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_fixtures: test_fixtures.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_fixtures: $(BIN)/test_fixtures

test_fixtures: $(BIN)/test_version $(BIN)/test_fixtures
	$(call run_test_bin,test_fixtures)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

# Shorthand aliases: make core [val|build] -> make test_core [val|build]
$(UNIT_CASES): %: test_%
	@:

# Mode-switch goal for: make test_core val
val:
	@:

# Mode-switch goal for: make test_core build
build:
	@:

# Mode-switch goal for: make fixtures test
test:
	@:

# Inactive test targets (kept as placeholders while test files are absent in test/unit/):
# $(BIN)/test_amp_collections: test_amp_collections.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER)
# 	@mkdir -p $(BIN)
# 	$(CC) $(CFLAGS) $^ -o $@
#
# test_amp_collections: $(BIN)/test_version $(BIN)/test_amp_collections
# 	@$(BIN)/test_amp_collections
# 	@$(BIN)/test_version
#
# $(BIN)/test_context: test_context.c $(ANVIL_SRCS) $(TESTBIT_SRC)
# 	@mkdir -p $(BIN)
# 	$(CC) $(CFLAGS) $^ -o $@
#
# test_context: $(BIN)/test_version $(BIN)/test_context
# 	@$(BIN)/test_context
# 	@$(BIN)/test_version
#
# $(BIN)/test_parser_core: test_parser_core.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER)
# 	@mkdir -p $(BIN)
# 	$(CC) $(CFLAGS) $^ -o $@
#
# test_parser_core: $(BIN)/test_version $(BIN)/test_parser_core
# 	@$(BIN)/test_parser_core
# 	@$(BIN)/test_version

test_release: $(LIB_RELEASE)
	$(CC) $(CFLAGS) test_core.c $(TEST_HELPER) $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_core
	$(CC) $(CFLAGS) test_version.c          $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_version
	@echo "-- release QC --"
	@$(BIN)/rel_test_core
	@$(BIN)/rel_test_version

clean:
	rm -f $(BIN)/*
