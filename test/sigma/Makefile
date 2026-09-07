CC      = gcc
STD     = c2x
ANVL    = ../..
TESTBIT = ../../../testbit

BUILD_NUM := $(shell git rev-list --count HEAD)
CFLAGS  = -Wall -Wextra -g -std=$(STD) \
          -I$(ANVL)/include \
          -I$(TESTBIT)/include \
          -D_POSIX_C_SOURCE=200809L \
          -DANVL_BUILD=$(BUILD_NUM)

SIGMA_SRCS = $(ANVL)/src/sigma/strings.c \
             $(ANVL)/src/sigma/memory.c \
             $(ANVL)/src/sigma/arrays.c \
             $(ANVL)/src/sigma/array_base.c \
             $(ANVL)/src/sigma/query.c \
             $(ANVL)/src/sigma/farray.c \
             $(ANVL)/src/sigma/parray.c \
             $(ANVL)/src/sigma/slotarray.c \
             $(ANVL)/src/sigma/collections.c \
             $(ANVL)/src/sigma/list.c \
             $(ANVL)/src/sigma/stack.c \
             $(ANVL)/src/sigma/map.c \
             $(ANVL)/src/sigma/math.c \
             $(ANVL)/src/sigma/time.c

TESTBIT_SRC = $(TESTBIT)/src/testbit.c

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

.PHONY: all clean val build test
.PHONY: build_test_list build_test_map build_test_math build_test_strings
.PHONY: build_test_query build_test_farray build_test_parray build_test_collections build_test_slotarray
.PHONY: build_test_stack
.PHONY: test_list test_map test_math test_strings $(UNIT_CASES)
.PHONY: test_query test_farray test_parray test_collections test_slotarray test_stack

all: $(BIN)/test_list \
     $(BIN)/test_map \
     $(BIN)/test_math \
     $(BIN)/test_strings \
     $(BIN)/test_query \
     $(BIN)/test_farray \
     $(BIN)/test_parray \
     $(BIN)/test_collections \
     $(BIN)/test_slotarray \
     $(BIN)/test_stack

$(BIN)/test_list: test_list.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_list: $(BIN)/test_list

test_list: $(BIN)/test_list
	$(call run_test_bin,test_list)

$(BIN)/test_map: test_map.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_map: $(BIN)/test_map

test_map: $(BIN)/test_map
	$(call run_test_bin,test_map)

$(BIN)/test_math: test_math.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_math: $(BIN)/test_math

test_math: $(BIN)/test_math
	$(call run_test_bin,test_math)

$(BIN)/test_strings: test_strings.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_strings: $(BIN)/test_strings

test_strings: $(BIN)/test_strings
	$(call run_test_bin,test_strings)

$(BIN)/test_query: test_query.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_query: $(BIN)/test_query

test_query: $(BIN)/test_query
	$(call run_test_bin,test_query)

$(BIN)/test_farray: test_farray.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_farray: $(BIN)/test_farray

test_farray: $(BIN)/test_farray
	$(call run_test_bin,test_farray)

$(BIN)/test_parray: test_parray.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_parray: $(BIN)/test_parray

test_parray: $(BIN)/test_parray
	$(call run_test_bin,test_parray)

$(BIN)/test_collections: test_collections.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_collections: $(BIN)/test_collections

test_collections: $(BIN)/test_collections
	$(call run_test_bin,test_collections)

$(BIN)/test_slotarray: test_slotarray.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_slotarray: $(BIN)/test_slotarray

test_slotarray: $(BIN)/test_slotarray
	$(call run_test_bin,test_slotarray)

$(BIN)/test_stack: test_stack.c $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

build_test_stack: $(BIN)/test_stack

test_stack: $(BIN)/test_stack
	$(call run_test_bin,test_stack)

# Shorthand aliases: make list [val|build] -> make test_list [val|build]
$(UNIT_CASES): %: test_%
	@:

# Mode-switch goal for: make test_list val
val:
	@:

# Mode-switch goal for: make test_list build
build:
	@:

test:
	@:

clean:
	rm -f $(BIN)/*
