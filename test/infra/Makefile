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

.PHONY: all clean
.PHONY: build_test_version build_test_files build_test_utils
.PHONY: test_version test_files test_utils

all: $(BIN)/test_version \
     $(BIN)/test_files \
	 $(BIN)/test_utils

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

$(BIN)/test_utils: test_utils.c $(ANVIL_SRCS) $(SIGMA_SRCS) $(UTIL_SRCS) $(TESTBIT_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test_utils: $(BIN)/test_version $(BIN)/test_utils
	@$(BIN)/test_utils
	@$(BIN)/test_version

clean:
	rm -f $(BIN)/*
