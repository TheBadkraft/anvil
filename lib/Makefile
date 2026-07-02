# ------------------------------------------------------------------
# Anvil C Parser — lib/Makefile
# Outputs: lib/debug/libanvil.{a,so}
#          lib/release/libanvil.{a,so}
# ------------------------------------------------------------------

CC      := gcc
STD     := c2x
AR      := ar
ARFLAGS := rcs

ANVIL   := ..

SRCS := \
    $(ANVIL)/src/core/anvil.c \
    $(ANVIL)/src/core/context.c \
    $(ANVIL)/src/core/errors.c \
    $(ANVIL)/src/core/memory.c \
    $(ANVIL)/src/core/operators.c \
    $(ANVIL)/src/core/parser.c \
    $(ANVIL)/src/core/strings.c \
    $(ANVIL)/src/core/symbols.c \
    $(ANVIL)/src/core/types.c \
    $(ANVIL)/src/import/import.c \
    $(ANVIL)/src/resolver/resolver.c \
    $(ANVIL)/src/schema/schema.c \
    $(ANVIL)/src/utilities/utils.c \
    $(ANVIL)/src/vars/vars.c

INCLUDES := -I$(ANVIL)/include -I$(ANVIL)/src/core
BUILD_NUM := $(shell git rev-list --count HEAD)
COMMON    := -std=$(STD) -D_POSIX_C_SOURCE=200809L -DANVL_BUILD=$(BUILD_NUM) $(INCLUDES)

DEBUG_FLAGS   := -Wall -Wextra -g -O0
RELEASE_FLAGS := -Wall -Wextra -O2 -DNDEBUG

DEBUG_OBJS   := $(patsubst $(ANVIL)/%.c, debug/%.o,   $(SRCS))
RELEASE_OBJS := $(patsubst $(ANVIL)/%.c, release/%.o, $(SRCS))

DEBUG_LIB_A   := debug/libanvil.a
DEBUG_LIB_SO  := debug/libanvil.so
RELEASE_LIB_A := release/libanvil.a
RELEASE_LIB_SO:= release/libanvil.so

.PHONY: all debug release bindings clean

all: debug bindings

debug: $(DEBUG_LIB_A) $(DEBUG_LIB_SO)

release: $(RELEASE_LIB_A) $(RELEASE_LIB_SO)

bindings:
	$(MAKE) -C ../bindings generate

$(DEBUG_LIB_A): $(DEBUG_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(DEBUG_LIB_SO): $(DEBUG_OBJS)
	$(CC) -shared -o $@ $^

$(RELEASE_LIB_A): $(RELEASE_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(RELEASE_LIB_SO): $(RELEASE_OBJS)
	$(CC) -shared -o $@ $^

debug/%.o: $(ANVIL)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON) $(DEBUG_FLAGS) -fPIC -c $< -o $@

release/%.o: $(ANVIL)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON) $(RELEASE_FLAGS) -fPIC -c $< -o $@

clean:
	rm -rf debug release