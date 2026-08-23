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

SIGMA_SRCS = $(ANVL)/src/sigma/strings.c \
             $(ANVL)/src/sigma/memory.c \
			 	 $(ANVL)/src/sigma/arrays.c \
			 	 $(ANVL)/src/sigma/array_base.c \
			 	 $(ANVL)/src/sigma/farray.c \
             $(ANVL)/src/sigma/collections.c \
			 	 $(ANVL)/src/sigma/list.c \
			 	 $(ANVL)/src/sigma/map.c \
			 	 $(ANVL)/src/sigma/math.c \
			 	 $(ANVL)/src/sigma/time.c

ANVIL_SRCS = $(ANVL)/src/core/anvil.c \
			 	 $(ANVL)/src/core/errors.c \
				 $(ANVL)/src/core/files.c \
				 $(ANVL)/src/core/module.c \
				 $(ANVL)/src/core/document.c \
				 $(ANVL)/src/core/parser.c \
				 $(ANVL)/src/core/source.c \
				 $(ANVL)/src/core/source_registry.c

UTIL_SRCS  = $(ANVL)/src/utilities/utils.c

HELPERS_SRC = ../utilities/helpers.c

VERSION_SRC = ../unit/test_version.c

TESTBIT_SRC = $(TESTBIT)/src/testbit.c

BIN = bin
VALGRIND_LOG = $(BIN)/valgrind_test_list.log

.PHONY: all clean
.PHONY: build_test_version build_test_files build_test_strings build_test_utils build_test_list build_test_map build_test_math
.PHONY: test_version test_files test_strings test_utils test_map test_math
.PHONY: valgrind_test_list

all: $(BIN)/test_version \
     $(BIN)/test_files \
     $(BIN)/test_strings \
	 $(BIN)/test_utils \
	 $(BIN)/test_map \
	 $(BIN)/test_math

$(BIN)/test_version: $(VERSION_SRC) $(SIGMA_SRCS) $(ANVIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_version: $(BIN)/test_version
	@$(BIN)/test_version

$(BIN)/test_files: test_files.c $(ANVIL_SRCS) $(SIGMA_SRCS) $(TESTBIT_SRC) $(HELPERS_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_files: $(BIN)/test_version $(BIN)/test_files
	@$(BIN)/test_files
	@$(BIN)/test_version

$(BIN)/test_strings: test_strings.c $(ANVIL_SRCS) $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_strings: $(BIN)/test_version $(BIN)/test_strings
	@$(BIN)/test_strings
	@$(BIN)/test_version

$(BIN)/test_utils: test_utils.c $(ANVIL_SRCS) $(SIGMA_SRCS) $(UTIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_utils: $(BIN)/test_version $(BIN)/test_utils
	@$(BIN)/test_utils
	@$(BIN)/test_version

$(BIN)/test_list: test_list.c $(ANVIL_SRCS) $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/test_map: test_map.c $(ANVIL_SRCS) $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/test_math: test_math.c $(ANVIL_SRCS) $(SIGMA_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: test_list
test_list: $(BIN)/test_version $(BIN)/test_list
	@$(BIN)/test_list
	@$(BIN)/test_version

build_test_list: $(BIN)/test_list

.PHONY: test_map
test_map: $(BIN)/test_version $(BIN)/test_map
	@$(BIN)/test_map
	@$(BIN)/test_version

build_test_map: $(BIN)/test_map

.PHONY: test_math
test_math: $(BIN)/test_version $(BIN)/test_math
	@$(BIN)/test_math
	@$(BIN)/test_version

build_test_math: $(BIN)/test_math

valgrind_test_list: $(BIN)/test_version $(BIN)/test_list
	@mkdir -p $(BIN)
	@valgrind --leak-check=full --track-origins=yes \
		--error-exitcode=42 \
		--log-file=$(VALGRIND_LOG) \
		$(BIN)/test_list
	@echo "Valgrind log: $(VALGRIND_LOG)"

clean:
	rm -f $(BIN)/*