# =============================================================================
# Anvil Makefile — Root of the Forge — FINAL VERSION — PINNED
# =============================================================================

.SILENT:

# -----------------------------------------------------------------------------
# Tools & flags
# -----------------------------------------------------------------------------
CC      = cc
CFLAGS  = -std=c2x -Wall -Wextra -O3 -march=native -fPIC -g
WRAP_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc
AR      = ar
ARFLAGS = rcs

# -----------------------------------------------------------------------------
# Directories
# -----------------------------------------------------------------------------
BUILD       = build
TEST_BUILD  = $(BUILD)/test
BIN         = bin

SRC    		= src
INC         = -Iinclude -I/usr/include/sigcore
SRC_TESTS   = test
TEST_INC    = -Iinclude/test $(INC) -I/usr/include/sigtest

# -----------------------------------------------------------------------------
# Core source objects
# -----------------------------------------------------------------------------
CORE_OBJS = $(BUILD)/anvil.o     \
            $(BUILD)/context.o   \
            $(BUILD)/parser.o    \
            $(BUILD)/types.o     \
            $(BUILD)/resolver.o  \
            $(BUILD)/writer.o    \
            $(BUILD)/operators.o \
            $(BUILD)/symbols.o   \
            $(BUILD)/errors.o    \
            $(BUILD)/utils.o

# -----------------------------------------------------------------------------
# Ensure build directories exist
# -----------------------------------------------------------------------------
$(BUILD) $(BIN) $(TEST_BUILD) $(BIN)/obj:
	@mkdir -p $@

dirs: $(BUILD) $(BIN) $(TEST_BUILD) $(BIN)/obj

# -----------------------------------------------------------------------------
# Core library (static)
# -----------------------------------------------------------------------------
$(BIN)/libanvil.a: $(CORE_OBJS) | $(BIN)
	$(AR) $(ARFLAGS) $@ $^

# -----------------------------------------------------------------------------
# Core library (shared)
# -----------------------------------------------------------------------------
$(BIN)/libanvil.so: $(CORE_OBJS) | $(BIN)
	$(CC) -shared -o $@ $^

$(BUILD)/%.o: $(SRC)/core/%.c | dirs
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(BUILD)/utils.o: $(SRC)/utilities/utils.c | dirs
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# -----------------------------------------------------------------------------
# CLI executable
# -----------------------------------------------------------------------------
$(BIN)/anvil: $(SRC)/cli/main.c $(BIN)/libanvil.a | $(BIN)
	$(CC) $(CFLAGS) $(INC) $< -L$(BIN) -lanvil -lsigcore -o $@

# -----------------------------------------------------------------------------
# Test utilities (test-only)
# -----------------------------------------------------------------------------
$(TEST_BUILD)/utils.o: $(SRC_TESTS)/utilities/helpers.c | $(TEST_BUILD)
	$(CC) $(CFLAGS) $(TEST_INC) -c $< -o $@

# -----------------------------------------------------------------------------
# Individual test: make test_foo
# -----------------------------------------------------------------------------
test_%: $(BIN)/test_%
	@$<

$(BIN)/test_%: $(TEST_BUILD)/test_%.o $(CORE_OBJS) $(TEST_BUILD)/utils.o $(BIN)/obj/fat.o | $(BIN)
	$(CC) $(CFLAGS) $(TEST_INC) $(WRAP_LDFLAGS) $^ -o $@

$(BIN)/obj/fat.o: $(BIN)/obj/sigtest.o $(BIN)/obj/sigcore.o
	ld -r $^ -o $@

$(TEST_BUILD)/test_%.o: $(SRC_TESTS)/test_%.c | $(TEST_BUILD)
	$(CC) $(CFLAGS) $(TEST_INC) -c $< -o $@

# -----------------------------------------------------------------------------
# Welcome target
# -----------------------------------------------------------------------------
welcome: $(BIN)/anvil
	@echo "Anvil CLI ready → ./bin/anvil"

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------
clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.a $(BUILD)/*.so
	rm -f $(TEST_BUILD)/*.o
	rm -f $(BIN)/lib* $(BIN)/anvil

# -----------------------------------------------------------------------------
# Phony targets
# -----------------------------------------------------------------------------
.PHONY: all dirs welcome test clean