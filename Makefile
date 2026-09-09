CC = gcc
STD = c2x
AR = ar
STRIP = strip

BUILD_NUM := $(shell git rev-list --count HEAD)

INCLUDE = -Iinclude
DEFS    = -D_POSIX_C_SOURCE=200809L -DANVL_BUILD=$(BUILD_NUM)

# Full ANVL bundle for this milestone: core parser + resolver (src/core, src/sigma),
# the opt-in type registry (src/anvil_types.c), and the schema add-on (src/schema/schema.c) —
# see notes/deferred-work.md's still-open "bundle vs. minimal libanvil" packaging question.
# libanvil.a is the full bundle for now, not the eventual minimal split.
CORE_SRCS = src/core/anvil.c \
            src/core/anvil_flat.c \
            src/core/anvil_vtable.c \
            src/core/document.c \
            src/core/errors.c \
            src/core/files.c \
            src/core/module.c \
            src/core/parser.c \
            src/core/resolver.c \
            src/core/source.c \
            src/core/source_registry.c \
            src/sigma/list.c \
            src/sigma/map.c \
            src/sigma/math.c \
            src/sigma/time.c \
            src/sigma/arrays.c \
            src/sigma/array_base.c \
            src/sigma/farray.c \
            src/sigma/memory.c \
            src/sigma/strings.c \
            src/sigma/collections.c \
            src/sigma/query.c

TYPES_SRCS  = src/anvil_types.c
SCHEMA_SRCS = src/schema/schema.c

LIB_SRCS = $(CORE_SRCS) $(TYPES_SRCS) $(SCHEMA_SRCS)

DEBUG_DIR    = lib/debug
RELEASE_DIR  = lib/release
DEBUG_OBJ    = build/debug
RELEASE_OBJ  = build/release

# Release carries no debug info at all (no -g), on top of an explicit strip pass on the
# archive itself — belt and suspenders, so nothing slips in from a mismatched flag later.
DEBUG_CFLAGS   = -Wall -Wextra -g -O0 -std=$(STD) $(INCLUDE) $(DEFS)
RELEASE_CFLAGS = -Wall -Wextra -O2 -DNDEBUG -std=$(STD) $(INCLUDE) $(DEFS)

DEBUG_OBJS   = $(patsubst src/%.c,$(DEBUG_OBJ)/%.o,$(LIB_SRCS))
RELEASE_OBJS = $(patsubst src/%.c,$(RELEASE_OBJ)/%.o,$(LIB_SRCS))

LIB_DEBUG   = $(DEBUG_DIR)/libanvil.a
LIB_RELEASE = $(RELEASE_DIR)/libanvil.a

.PHONY: all lib lib-debug lib-release clean

all: lib

lib: lib-debug lib-release

lib-debug: $(LIB_DEBUG)

lib-release: $(LIB_RELEASE)

$(LIB_DEBUG): $(DEBUG_OBJS)
	@mkdir -p $(DEBUG_DIR)
	$(AR) rcs $@ $^

$(LIB_RELEASE): $(RELEASE_OBJS)
	@mkdir -p $(RELEASE_DIR)
	$(AR) rcs $@ $^
	$(STRIP) --strip-debug --strip-unneeded $@

$(DEBUG_OBJ)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@

$(RELEASE_OBJ)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

clean:
	rm -rf build lib
