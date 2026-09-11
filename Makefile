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

# -fPIC on every object (debug included) so the same object tree serves both the static
# archive and the shared object — no separate PIC/non-PIC object trees to keep in sync.
# Release carries no debug info at all (no -g), on top of an explicit strip pass on the
# archive/shared object themselves — belt and suspenders, so nothing slips in from a
# mismatched flag later.
# -MMD -MP generate a per-object .d file listing every header it includes, so a header-only
# change (e.g. include/internal/source.h) correctly triggers a rebuild of every object that
# includes it -- not just the .c files touched directly. Without this, a stale object built
# against an old header layout can silently link into the archive/.so alongside fresh ones,
# producing a corrupted artifact with no build error (found the hard way: a new anvl_source_i
# vtable field left non-recompiled objects reading every later field one slot off).
DEBUG_CFLAGS   = -Wall -Wextra -g -O0 -fPIC -std=$(STD) $(INCLUDE) $(DEFS) -MMD -MP
RELEASE_CFLAGS = -Wall -Wextra -O2 -DNDEBUG -fPIC -std=$(STD) $(INCLUDE) $(DEFS) -MMD -MP

DEBUG_OBJS   = $(patsubst src/%.c,$(DEBUG_OBJ)/%.o,$(LIB_SRCS))
RELEASE_OBJS = $(patsubst src/%.c,$(RELEASE_OBJ)/%.o,$(LIB_SRCS))

LIB_DEBUG   = $(DEBUG_DIR)/libanvil.a
LIB_RELEASE = $(RELEASE_DIR)/libanvil.a

SO_DEBUG   = $(DEBUG_DIR)/libanvil.so
SO_RELEASE = $(RELEASE_DIR)/libanvil.so

.PHONY: all lib lib-debug lib-release so so-debug so-release clean

all: lib so

lib: lib-debug lib-release

so: so-debug so-release

lib-debug: $(LIB_DEBUG)

lib-release: $(LIB_RELEASE)

so-debug: $(SO_DEBUG)

so-release: $(SO_RELEASE)

$(LIB_DEBUG): $(DEBUG_OBJS)
	@mkdir -p $(DEBUG_DIR)
	$(AR) rcs $@ $^

$(LIB_RELEASE): $(RELEASE_OBJS)
	@mkdir -p $(RELEASE_DIR)
	$(AR) rcs $@ $^
	$(STRIP) --strip-debug --strip-unneeded $@

$(SO_DEBUG): $(DEBUG_OBJS)
	@mkdir -p $(DEBUG_DIR)
	$(CC) -shared -Wl,-soname,libanvil.so -o $@ $^

$(SO_RELEASE): $(RELEASE_OBJS)
	@mkdir -p $(RELEASE_DIR)
	$(CC) -shared -Wl,-soname,libanvil.so -o $@ $^
	$(STRIP) --strip-debug --strip-unneeded $@

$(DEBUG_OBJ)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@

$(RELEASE_OBJ)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

-include $(DEBUG_OBJS:.o=.d)
-include $(RELEASE_OBJS:.o=.d)

clean:
	rm -rf build lib coverage.info coverage.filtered.info $(COVERAGE_DIR)

# Combines test/unit's coverage (Anvil's own usage of the Sigma subset, plus core/types/schema)
# with test/sigma's (the Sigma subset's own dedicated suites, which exercise types Anvil itself
# never touches -- parray/slotarray/stack) into one merged picture. Neither directory alone
# tells the true story: test/unit barely touches parray/slotarray/stack (Anvil doesn't use
# them), and test/sigma barely touches memory.c (its suites mostly use a fake arena, not the
# real allocator) -- each fills in what the other misses.
.PHONY: coverage
COVERAGE_DIR = coverage-html
coverage:
	$(MAKE) -C test/unit coverage
	$(MAKE) -C test/sigma coverage
	lcov --capture --directory test/unit/bin --directory test/sigma/bin \
		--output-file coverage.info --rc branch_coverage=0
	lcov --remove coverage.info '*/testbit/*' '*/test/unit/*' '*/test/sigma/*' \
		'*/test/utilities/*' -o coverage.filtered.info
	genhtml coverage.filtered.info --output-directory $(COVERAGE_DIR)
	@echo "Combined coverage report: $(COVERAGE_DIR)/index.html"
