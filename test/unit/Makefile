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
			 $(ANVL)/src/core/anvil_flat.c \
			 $(ANVL)/src/core/anvil_vtable.c \
			 $(ANVL)/src/core/document.c \
			 $(ANVL)/src/core/errors.c \
			 $(ANVL)/src/core/files.c \
			 $(ANVL)/src/core/module.c \
			 $(ANVL)/src/core/parser.c \
			 $(ANVL)/src/core/resolver.c \
			 $(ANVL)/src/core/source.c \
			 $(ANVL)/src/core/source_registry.c \
			 $(ANVL)/src/sigma/list.c \
			 $(ANVL)/src/sigma/map.c \
			 $(ANVL)/src/sigma/math.c \
			 $(ANVL)/src/sigma/time.c \
			 $(ANVL)/src/sigma/arrays.c \
			 $(ANVL)/src/sigma/array_base.c \
			 $(ANVL)/src/sigma/farray.c \
          $(ANVL)/src/sigma/memory.c \
          $(ANVL)/src/sigma/strings.c \
			 $(ANVL)/src/sigma/collections.c \
			 $(ANVL)/src/sigma/query.c

TEST_HELPER  = $(ANVL)/test/utilities/helpers.c
TEST_DEBUG   = $(ANVL)/test/utilities/debug.c

TESTBIT_SRC = $(TESTBIT)/src/testbit.c

# Opt-in modules not part of ANVIL_SRCS by default, each built entirely on the public flat API
# (anvil_flat.h), no core parser/resolver changes — see notes/native-schema.md.
#
# anvil_types.c is part of ANVL proper, just opt-in — usable without schema.c, a sibling of
# core/ and sigma/ under src/, not nested under src/schema/. schema.c (not yet built) is the
# actual add-on layered on top of it — src/schema/ is reserved for that, and will depend on
# TYPES_SRCS, never the reverse.
TYPES_SRCS  = $(ANVL)/src/anvil_types.c
TYPES_LIB   = $(ANVIL_SRCS) $(TYPES_SRCS)

# schema.c is the real add-on layered on top of TYPES_LIB — lives under src/schema/, reserved
# for it. Depends on types, never the reverse.
SCHEMA_SRCS = $(ANVL)/src/schema/schema.c
SCHEMA_LIB  = $(TYPES_LIB) $(SCHEMA_SRCS)

LIB_DEBUG     = $(ANVL)/lib/debug/libanvil.a
LIB_RELEASE   = $(ANVL)/lib/release/libanvil.a

# Built by the root Makefile (core + types + schema bundled — see notes/deferred-work.md),
# not compiled here — test_release links against the real artifact, not source files.
$(LIB_DEBUG):
	$(MAKE) -C $(ANVL) lib-debug

$(LIB_RELEASE):
	$(MAKE) -C $(ANVL) lib-release

BIN = bin
VALGRIND_LOG = $(BIN)/valgrind_test.log
UNIT_CASES = $(patsubst test_%.c,%,$(wildcard test_*.c))

# gcc namespaces each source's .gcno/.gcda by the *binary's* name (e.g.
# bin/test_source-document.gcno) when compiling multiple sources into one -o target with
# --coverage, so all 15 suites' coverage data can safely live side by side in bin/ without
# colliding, and running the same suite twice accumulates rather than overwrites.
COVERAGE_DIR = coverage-html
COVERAGE_INFO = $(BIN)/coverage.info
COVERAGE_FILTERED = $(BIN)/coverage.filtered.info

.PHONY: coverage
coverage:
	@$(MAKE) clean
	@$(MAKE) all CFLAGS="$(CFLAGS) --coverage"
	@echo "-- running every suite to collect coverage data --"
	@for t in $(BIN)/test_*; do "$$t" >/dev/null 2>&1 || true; done
	lcov --capture --directory $(BIN) --output-file $(COVERAGE_INFO) --rc branch_coverage=0
	lcov --remove $(COVERAGE_INFO) '*/testbit/*' '*/test/unit/*' '*/test/utilities/*' \
		-o $(COVERAGE_FILTERED)
	genhtml $(COVERAGE_FILTERED) --output-directory $(BIN)/$(COVERAGE_DIR)
	@echo "Coverage report: $(BIN)/$(COVERAGE_DIR)/index.html"

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
.PHONY: build_test_version build_test_module build_test_registry build_test_header build_test_source
.PHONY: build_test_body_amp build_test_body_aml build_test_types build_test_schema
.PHONY: test_version test_module test_registry test_header test_source test_body_amp test_body_aml test_resolver test_anvil_native test_anvil_vtable test_attribute_grammar test_value_fragment test_types test_schema $(UNIT_CASES)

all: $(BIN)/test_version \
	$(BIN)/test_module \
	$(BIN)/test_document \
	$(BIN)/test_registry \
	$(BIN)/test_header \
	$(BIN)/test_source \
	$(BIN)/test_body_amp \
	$(BIN)/test_body_aml \
	$(BIN)/test_resolver \
	$(BIN)/test_anvil_native \
	$(BIN)/test_anvil_vtable \
	$(BIN)/test_attribute_grammar \
	$(BIN)/test_value_fragment \
	$(BIN)/test_types \
	$(BIN)/test_schema
# test_types links src/anvil_types.c (TYPES_LIB) instead of plain ANVIL_SRCS
# — the one binary in this suite that needs the opt-in type registry.
$(BIN)/test_types: test_types.c $(TYPES_LIB) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_types: $(BIN)/test_types

test_types: $(BIN)/test_version $(BIN)/test_types
	$(call run_test_bin,test_types)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

# test_schema links src/schema/schema.c (SCHEMA_LIB, which already includes TYPES_LIB) —
# the one binary in this suite that needs the schema add-on.
$(BIN)/test_schema: test_schema.c $(SCHEMA_LIB) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_schema: $(BIN)/test_schema

test_schema: $(BIN)/test_version $(BIN)/test_schema
	$(call run_test_bin,test_schema)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

# test_fixtures is disabled: it depends on the deprecated root/doc APIs that
# were replaced by the source-hash identity model. Revive after the parser
# is reintegrated with anvl_source.

$(BIN)/test_version: test_version.c $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_version: $(BIN)/test_version

test_version: $(BIN)/test_version
	$(call run_test_bin,test_version)

$(BIN)/test_module: test_module.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_module: $(BIN)/test_module

test_module: $(BIN)/test_version $(BIN)/test_module
	$(call run_test_bin,test_module)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_document: test_document.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_document: $(BIN)/test_document

test_document: $(BIN)/test_version $(BIN)/test_document
	$(call run_test_bin,test_document)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_registry: test_registry.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_registry: $(BIN)/test_registry

test_registry: $(BIN)/test_version $(BIN)/test_registry
	$(call run_test_bin,test_registry)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_header: test_header.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_header: $(BIN)/test_header

test_header: $(BIN)/test_version $(BIN)/test_header
	$(call run_test_bin,test_header)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_source: test_source.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_source: $(BIN)/test_source

test_source: $(BIN)/test_version $(BIN)/test_source
	$(call run_test_bin,test_source)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_body_amp: test_body_amp.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_body_amp: $(BIN)/test_body_amp

test_body_amp: $(BIN)/test_version $(BIN)/test_body_amp
	$(call run_test_bin,test_body_amp)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_body_aml: test_body_aml.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_body_aml: $(BIN)/test_body_aml

test_body_aml: $(BIN)/test_version $(BIN)/test_body_aml
	$(call run_test_bin,test_body_aml)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_resolver: test_resolver.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_resolver: $(BIN)/test_resolver

test_resolver: $(BIN)/test_version $(BIN)/test_resolver
	$(call run_test_bin,test_resolver)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_anvil_native: test_anvil_native.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_anvil_native: $(BIN)/test_anvil_native

test_anvil_native: $(BIN)/test_version $(BIN)/test_anvil_native
	$(call run_test_bin,test_anvil_native)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_anvil_vtable: test_anvil_vtable.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_anvil_vtable: $(BIN)/test_anvil_vtable

test_anvil_vtable: $(BIN)/test_version $(BIN)/test_anvil_vtable
	$(call run_test_bin,test_anvil_vtable)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_attribute_grammar: test_attribute_grammar.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_attribute_grammar: $(BIN)/test_attribute_grammar

test_attribute_grammar: $(BIN)/test_version $(BIN)/test_attribute_grammar
	$(call run_test_bin,test_attribute_grammar)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

$(BIN)/test_value_fragment: test_value_fragment.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER) $(TEST_DEBUG)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_value_fragment: $(BIN)/test_value_fragment

test_value_fragment: $(BIN)/test_version $(BIN)/test_value_fragment
	$(call run_test_bin,test_value_fragment)
	@if [ -z "$(filter build val,$(MAKECMDGOALS))" ]; then \
		$(BIN)/test_version; \
	fi

# test_fixtures disabled — see `all` target comment.
# $(BIN)/test_fixtures: test_fixtures.c $(ANVIL_SRCS) $(TESTBIT_SRC) $(TEST_HELPER)
# 	@mkdir -p $(BIN)
# 	$(CC) $(CFLAGS) $^ -o $@

# Shorthand aliases: make core [val|build] -> make test_module [val|build]
$(UNIT_CASES): %: test_%
	@:

# Mode-switch goal for: make test_module val
val:
	@:

# Mode-switch goal for: make test_module build
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
# $(BIN)/test_module: test_module.c $(ANVIL_SRCS) $(TESTBIT_SRC)
# 	@mkdir -p $(BIN)
# 	$(CC) $(CFLAGS) $^ -o $@
#
# test_module: $(BIN)/test_version $(BIN)/test_module
# 	@$(BIN)/test_module
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
	$(CC) $(CFLAGS) test_module.c $(TEST_HELPER) $(TEST_DEBUG) $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_module
	$(CC) $(CFLAGS) test_version.c          $(TESTBIT_SRC) $(LIB_RELEASE) -o $(BIN)/rel_test_version
	@echo "-- release QC --"
	@$(BIN)/rel_test_module
	@$(BIN)/rel_test_version

clean:
	rm -rf $(BIN)/*
