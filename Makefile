CC = gcc
STD = c2x
AR = ar
STRIP = strip

# The build number reported as `+BUILD`: an explicit override (make ANVL_BUILD=176, or BUILD_NUM=176, or
# the environment) wins, so a release build can pass the tag's count and not depend on the checkout; else
# the commit count of HEAD; else 0, which the library reports as "unknown" (no `+BUILD`), e.g. in an
# extracted tarball with no .git. (BR-2610-anvl-version-001.)
BUILD_NUM := $(or $(ANVL_BUILD),$(BUILD_NUM),$(shell git rev-list --count HEAD 2>/dev/null),0)

# sigma.core: a real system-installed dependency, not vendored into this repo and not a
# sibling-relative path either (FR-2609-sigmem-001) -- see notes/sigma-system-alloc-dependency.md
# for the full rationale. A sibling-relative path (../sigma.core) broke the moment anvil is
# checked out *nested* inside something else, which is exactly how every binding vendors it
# (anvil.net/vendor/anvil, etc.) -- confirmed directly, not assumed. /usr/local/include and
# /usr/local/packages are absolute and machine-wide, so they resolve identically regardless of
# where this repo is checked out, matching the sigma.* ecosystem's own existing package
# convention (sigma.memory's config.sh already references /usr/local/packages/sigma.core.o).
INCLUDE = -Iinclude -I/usr/local/include

# AnvilScript (ASL) opt-in: off by default, matching notes/anvilscript-design.md Decision 11 —
# object-model-only consumers shouldn't pay for scripting they never use. Always defined (0 or
# 1), never left undefined, so `#if WITH_ASL` in source can't silently take "off" by omission.
# `anvil_is_script_enabled()` is the runtime-queryable mirror of this same build-time constant.
WITH_ASL ?= 0

DEFS = -D_POSIX_C_SOURCE=200809L -DANVL_BUILD=$(BUILD_NUM) -DWITH_ASL=$(WITH_ASL)

# Full ANVL bundle for this milestone: core parser + resolver (src/core, src/sigma),
# the opt-in type registry (src/anvil_types.c), and the schema add-on (src/schema/schema.c) —
# see notes/deferred-work.md's still-open "bundle vs. minimal libanvil" packaging question.
# libanvil.a is the full bundle for now, not the eventual minimal split.
CORE_SRCS = src/core/anvil.c \
            src/core/anvil_flat.c \
            src/core/anvil_vtable.c \
            src/core/asl_parser.c \
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
            src/sigma/strings.c \
            src/sigma/collections.c \
            src/sigma/query.c

TYPES_SRCS  = src/anvil_types.c src/anvil_types_vtable.c
SCHEMA_SRCS = src/schema/schema.c src/schema/schema_vtable.c

# sigma.system.alloc (FR-2609-sigmem-001): the Allocator/bump-arena provider, formerly
# src/sigma/memory.c's own vendored-and-modified copy. A real prebuilt package, installed once
# to /usr/local/packages/ -- linked as-is, the same way any other real system library is (you
# don't get to pick a "debug" build of libm either). Not copied into this repo at all.
SIGMA_SYSTEM_ALLOC_PKG = /usr/local/packages/sigma.system.alloc.o

# The streaming writer and document builder (include/anvil_writer*.h, include/anvil_builder*.h,
# src/writer/) are a standalone add-on: it links no
# reader code and the reader links none of it. It ships in the bundle by default; build a
# reader-only library with `make WITH_WRITER=0` (see `check-reader-only`, which proves nothing of
# the writer leaks into that build). FR/FR-2609-anvl-writer-001.md.
WITH_WRITER ?= 1
WRITER_SRCS = src/writer/grammar.c \
              src/writer/writer.c \
              src/writer/writer_vtable.c \
              src/writer/builder.c \
              src/writer/builder_vtable.c

# The real ScriptEngine (body/AST parser, eventually the compiler/VM/registry/built-ins) --
# unlike src/core/asl_parser.c (the signature parser, always in CORE_SRCS), this is excluded
# at the file level when WITH_ASL=0, not just inert behind an internal #if: nothing in a
# WITH_ASL=0 build ever calls into it, so there's no call site forcing it to always compile.
ASL_SRCS = src/asl/asl_body_parser.c

LIB_SRCS = $(CORE_SRCS) $(TYPES_SRCS) $(SCHEMA_SRCS)
ifeq ($(WITH_WRITER),1)
LIB_SRCS += $(WRITER_SRCS)
endif
ifeq ($(WITH_ASL),1)
LIB_SRCS += $(ASL_SRCS)
endif

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
# -fvisibility=hidden: internal implementation symbols (src/sigma/*'s Math/List/Map/... vtable
# globals, mod_*/doc_*/collection_*/array_*/etc.) have no business in a consumer's dynamic
# symbol table at all -- a native addon process hosting anvil_node.node alongside unrelated
# other native code found this out the hard way (a same-named global in another addon silently
# won symbol resolution over one of these, segfaulting on a bogus pointer). The public ABI
# (anvil_flat.h, anvil_vtable.h, anvil_writer*.h, anvil_builder*.h) stays exported exactly as
# before via `#pragma GCC visibility push(default)/pop` wrapping each of those headers' own
# declarations -- see notes/symbol-visibility.md.
DEBUG_CFLAGS   = -Wall -Wextra -g -O0 -fPIC -fvisibility=hidden -std=$(STD) $(INCLUDE) $(DEFS) -MMD -MP
RELEASE_CFLAGS = -Wall -Wextra -O2 -DNDEBUG -fPIC -fvisibility=hidden -std=$(STD) $(INCLUDE) $(DEFS) -MMD -MP

DEBUG_OBJS   = $(patsubst src/%.c,$(DEBUG_OBJ)/%.o,$(LIB_SRCS)) $(SIGMA_SYSTEM_ALLOC_PKG)
RELEASE_OBJS = $(patsubst src/%.c,$(RELEASE_OBJ)/%.o,$(LIB_SRCS)) $(SIGMA_SYSTEM_ALLOC_PKG)

LIB_DEBUG   = $(DEBUG_DIR)/libanvil.a
LIB_RELEASE = $(RELEASE_DIR)/libanvil.a

SO_DEBUG   = $(DEBUG_DIR)/libanvil.so
SO_RELEASE = $(RELEASE_DIR)/libanvil.so

.PHONY: anvil-dump fuzz-memory-test fuzz-prove check-drift-tables check-dump-valgrind check-conformance update-conformance check-numeric-fastpath all lib lib-debug lib-release so so-debug so-release clean check-reader-only check-exports check-32bit check-tsan check-test-release-fresh check-version

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

$(SO_DEBUG): $(DEBUG_OBJS) libanvil.exports
	@mkdir -p $(DEBUG_DIR)
	$(CC) -shared -Wl,-soname,libanvil.so -Wl,--version-script=libanvil.exports -o $@ $(filter-out libanvil.exports,$^)

$(SO_RELEASE): $(RELEASE_OBJS) libanvil.exports
	@mkdir -p $(RELEASE_DIR)
	$(CC) -shared -Wl,-soname,libanvil.so -Wl,--version-script=libanvil.exports -o $@ $(filter-out libanvil.exports,$^)
	$(STRIP) --strip-debug --strip-unneeded $@

$(DEBUG_OBJ)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@

$(RELEASE_OBJ)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

-include $(DEBUG_OBJS:.o=.d)
-include $(RELEASE_OBJS:.o=.d)

# -D flags are not tracked by -MMD, so anvil.o (the only object that reads ANVL_BUILD) would keep a stale
# `+BUILD` after the number changes (a new commit, or an explicit ANVL_BUILD=). A stamp file holding the
# number, rewritten only when it changes, makes the object depend on it (BR-2610-anvl-version-001).
BUILD_STAMP = build/.anvl_build_number
$(shell mkdir -p build; printf '%s\n' '$(BUILD_NUM)' | cmp -s - $(BUILD_STAMP) 2>/dev/null || printf '%s\n' '$(BUILD_NUM)' > $(BUILD_STAMP))
$(DEBUG_OBJ)/core/anvil.o $(RELEASE_OBJ)/core/anvil.o: $(BUILD_STAMP)

# Builds the reader-only bundle (WITH_WRITER=0) into its own directories, then checks it: no
# anvil_writer_*/anvil_builder_* function or `Writer`/`Builder` vtable symbol, and a program using only the reader API links
# against it. The full bundle must still carry both.
READER_ONLY_DIR = build/reader-only
# Every function declared in a public header must be exported from the release shared object, and
# no internal symbol may be (scripts/check-exports.sh).
check-exports: so-release
	sh scripts/check-exports.sh $(SO_RELEASE) include

# Compile-only check of the code that must be correct where size_t is 32 bits (wasm32, 32-bit ARM):
# -Werror turns any silent 64->32 constant truncation into a failure. Needs the 32-bit libc headers
# (gcc-multilib); skips with a message when they are absent. Emscripten reproduces it too.
check-32bit:
	@if echo '#include <stdint.h>' | $(CC) -m32 -E -x c - >/dev/null 2>&1; then \
		$(CC) -m32 -std=$(STD) -Wall -Wextra -Werror -Woverflow $(INCLUDE) $(DEFS) -c src/writer/grammar.c -o /dev/null && \
		echo "check-32bit OK: src/writer/grammar.c compiles clean with -m32"; \
	else echo "check-32bit SKIPPED: no -m32 headers here (install gcc-multilib, or build with emcc)"; fi

# Concurrent load/read/dispose under ThreadSanitizer (test/infra/tsan_load_stress.c): any data race in
# anvil, or in a global it calls, is reported and fails the target. TSAN_ALLOC=source also rebuilds
# sigma.system.alloc from $(SIGMA_CORE)/src/system_alloc.c with the sanitizer, since races inside the
# prebuilt (uninstrumented) allocator object are otherwise invisible. BR-2610-anvl-registry-001.
SIGMA_CORE ?= ../sigma.core
TSAN_DIR    = build/tsan
TSAN_MODES ?= different identical diamond
ifeq ($(WITH_ASL),1)
TSAN_MODES += asl
endif
TSAN_ALLOC ?= prebuilt
check-tsan:
	@mkdir -p $(TSAN_DIR)
	@if [ "$(TSAN_ALLOC)" = "source" ]; then \
		$(CC) -fsanitize=thread -g -O1 -std=$(STD) -I/usr/local/include $(DEFS) -c $(SIGMA_CORE)/src/system_alloc.c -o $(TSAN_DIR)/alloc.o; \
		cp $(TSAN_DIR)/alloc.o $(TSAN_DIR)/alloc.use.o; \
	else cp $(SIGMA_SYSTEM_ALLOC_PKG) $(TSAN_DIR)/alloc.use.o; fi
	$(CC) -fsanitize=thread -g -O1 -std=$(STD) $(INCLUDE) $(DEFS) $(LIB_SRCS) test/infra/tsan_load_stress.c $(TSAN_DIR)/alloc.use.o -lpthread -o $(TSAN_DIR)/stress
	@rc=0; for m in $(TSAN_MODES); do \
		TSAN_OPTIONS="halt_on_error=0 exitcode=66 report_signal_unsafe=0" $(TSAN_DIR)/stress $$m > $(TSAN_DIR)/$$m.log 2>&1; r=$$?; \
		echo "check-tsan [$$m, alloc=$(TSAN_ALLOC)]: exit $$r, $$(grep -c 'WARNING: ThreadSanitizer' $(TSAN_DIR)/$$m.log) race report(s), $$(grep -c '^FAIL' $(TSAN_DIR)/$$m.log) load failure(s)"; \
		[ $$r -eq 0 ] || rc=1; \
	done; exit $$rc

# test_release must never link a stale archive (SR-2610-anvl-build-001); scripts/check-test-release-fresh.sh.
check-test-release-fresh:
	sh scripts/check-test-release-fresh.sh

# The reported version is semver and honors an explicit build number (scripts/check-version.sh).
check-version:
	sh scripts/check-version.sh

check-reader-only:
	$(MAKE) WITH_WRITER=0 RELEASE_DIR=$(READER_ONLY_DIR)/lib RELEASE_OBJ=$(READER_ONLY_DIR)/obj lib-release
	$(MAKE) WITH_WRITER=1 RELEASE_DIR=$(READER_ONLY_DIR)/full-lib RELEASE_OBJ=$(READER_ONLY_DIR)/full-obj lib-release
	@if nm $(READER_ONLY_DIR)/lib/libanvil.a | grep -E ' [TDR] (anvil_writer_|anvil_builder_|Writer$$|Builder$$)'; then \
		echo "FAIL: writer symbols present in the reader-only library"; exit 1; fi
	@nm $(READER_ONLY_DIR)/full-lib/libanvil.a | grep -q ' T anvil_writer_new' || \
		{ echo "FAIL: full bundle is missing the writer"; exit 1; }
	@nm $(READER_ONLY_DIR)/full-lib/libanvil.a | grep -q ' T anvil_builder_new' || \
		{ echo "FAIL: full bundle is missing the builder"; exit 1; }
	@printf '#include "anvil_flat.h"\nint main(void){anvil_document d=anvil_load_buffer("#!aml\\nx := 1;\\n",16);return d?0:1;}\n' > $(READER_ONLY_DIR)/reader_only.c
	$(CC) -std=$(STD) $(INCLUDE) $(DEFS) $(READER_ONLY_DIR)/reader_only.c $(READER_ONLY_DIR)/lib/libanvil.a -o $(READER_ONLY_DIR)/reader_only
	$(READER_ONLY_DIR)/reader_only
	@echo "reader-only OK: no writer symbols, reader links and runs"

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

# Fast decimal-to-double path vs strtod: edge cases, fuzz, and the x87 / rounding-mode gates.
check-numeric-fastpath:
	sh scripts/check-numeric-fastpath.sh

# anvil-dump (tools/anvil-dump.c): prints the canonical description of what native parsed, the contract
# in conformance/FORMAT.md. Built against the debug objects (valgrind-readable), static, with the
# internal headers it needs for source spans. conformance/ holds the cases and native's expected dumps.
DUMP_BIN = build/tools/anvil-dump
anvil-dump: $(DUMP_BIN)

$(DUMP_BIN): tools/anvil-dump.c $(DEBUG_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -MMD -MP,$(DEBUG_CFLAGS)) tools/anvil-dump.c $(DEBUG_OBJS) -o $@

# Regenerates every expected dump and diffs it against the committed one; any difference fails.
# ANVIL_DUMP=<path> checks another build of the dump tool (for example a deliberately altered one).
check-conformance: $(DUMP_BIN) check-drift-tables
	sh scripts/check-conformance.sh check $(or $(ANVIL_DUMP),$(DUMP_BIN))

# The drift tables (conformance/drift-tables.txt) are generated from include/*.h; stale ones fail.
check-drift-tables:
	sh scripts/check-drift-tables.sh

# Rewrites the expected dumps. Only alongside a deliberate native change, committed with it.
update-conformance: $(DUMP_BIN)
	sh scripts/check-drift-tables.sh write
	sh scripts/check-conformance.sh update $(or $(ANVIL_DUMP),$(DUMP_BIN))

# anvil-dump over the whole corpus under valgrind: no leak, no memory error (slow: one valgrind run per input).
check-dump-valgrind: $(DUMP_BIN)
	sh scripts/check-dump-valgrind.sh $(DUMP_BIN)

# Proves the conformance tooling can fail: an altered scratch build must turn check-conformance RED and be found
# by the differential fuzzer, while native against itself shows 0 differences (conformance/fuzz/prove.sh).
fuzz-prove: $(DUMP_BIN)
	sh conformance/fuzz/prove.sh

# 200k fuzz cases under a 1 GiB memory cap, foreground; reports the harness's peak RSS.
fuzz-memory-test: $(DUMP_BIN)
	sh conformance/fuzz/test-memory.sh $(DUMP_BIN)
